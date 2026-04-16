
struct EngineSpec
    name::String
    cmd::Union{Cmd,Nothing}
    is_source::Bool
    path_hint::String
end

function spawn_engine(cmd::Cmd)
    # Merge stderr into stdout so startup failures are visible to the parent.
    return open(pipeline(cmd; stderr=stdout), "r+")
end

"""
    spawn_and_greet(spec::EngineSpec; debug=false)

Spawns an engine process and reads the initial greeting.
Returns the process handle, or nothing if the engine is a source engine.
"""
function spawn_and_greet(spec::EngineSpec; debug=false)
    if spec.is_source
        return nothing
    end
    proc = spawn_engine(spec.cmd)
    debug && println("[DEBUG] Reading greeting from $(spec.name)")
    start_read = time()
    read_until_ok(proc, spec.name; timeout_s=50.0, debug=debug, context="greeting")
    return proc
end

function read_until_ok(proc, engine_name; timeout_s=5.0, debug=false, context="")
    start_time = time()
    last_line = ""
    while (time() - start_time) < timeout_s
        if Base.process_exited(proc)
            break
        end
        line = strip(readline(proc))
        debug && println("[DEBUG] $engine_name $context: $line")
        last_line = line
        if startswith(line, "ok")
            finish_read = time()
            debug && println("[DEBUG] time $context = $(finish_read - start_time)")
            return true
        end
    end
    @warn "Timeout waiting for ok" engine_name context last_line
    return false
end

function read_bestmove(proc, engine_name; timeout_s=5.0, debug=false)
    start_time = time()
    best_move = ""
    last_line = ""
    while (time() - start_time) < timeout_s
        if Base.process_exited(proc)
            break
        end
        line = strip(readline(proc))
        debug && println("[DEBUG] $engine_name response: $line")
        last_line = line
        if startswith(line, "ok")
            finish_read = time()
            debug && println("[DEBUG] time reading best move = $(finish_read - start_time)")
            return best_move
        elseif !isempty(line)
            best_move = line
        end
    end
    @warn "Timeout waiting for bestmove" engine_name last_line
    return best_move
end

"""
    shutdown_engine(proc, engine_name; timeout_s=2.0, debug=false)

Attempts a graceful shutdown of an engine process, then forces termination if needed.
"""
function shutdown_engine(proc, engine_name; timeout_s=2.0, debug=false)
    if proc === nothing
        return nothing
    end
    try
        write(proc, "exit\n")
        flush(proc)
    catch
        # Ignore write errors if the process already exited.
    end
    try
        close(proc.in)
    catch
        # Ignore close errors if stdin is already closed.
    end

    wait_task = @async wait(proc)
    start_time = time()
    while !istaskdone(wait_task) && (time() - start_time) < timeout_s
        sleep(0.05)
    end
    if !istaskdone(wait_task)
        debug && println("[DEBUG] Forcing engine shutdown: $engine_name")
        try
            kill(proc)
        catch
            # Ignore kill errors if the process already exited.
        end
        try
            wait(proc)
        catch
            # Ignore wait errors after kill.
        end
    end
end

"""
    play_one_match(engine1, engine2, time_limit; starting_position="", debug=false)

Plays two UHP engines against each other from a given starting position until one wins.
- engine1: EngineSpec for first engine
- engine2: EngineSpec for second engine
- time_limit: Thinking time per move in seconds
- starting_position: Starting moves separated by semicolons (default: empty for new game)
- debug: Print debug statements (default: false)

Returns 1 if engine1 wins, 2 if engine2 wins, 0 for draw.
"""
function play_one_match(
    engine1::EngineSpec,
    engine2::EngineSpec,
    time_limit_s::Real;
    starting_position="",
    debug=false,
    engine1_proc=nothing,
    engine2_proc=nothing,
)
    start = time()
    engine1_name = engine1.name
    engine2_name = engine2.name
    engine_names = [engine1_name, engine2_name]
    println("White: $engine1_name vs Black: $engine2_name")
    is_source1 = engine1.is_source
    is_source2 = engine2.is_source
    is_source = [is_source1, is_source2]
    engine_paths = [engine1.path_hint, engine2.path_hint]

    # Use pre-spawned procs or spawn new ones
    owns_engine1 = false
    owns_engine2 = false
    if !is_source1
        if engine1_proc !== nothing
            engine1 = engine1_proc
        else
            engine1 = spawn_and_greet(engine1; debug=debug)
            owns_engine1 = true
        end
    end
    if !is_source2
        if engine2_proc !== nothing
            engine2 = engine2_proc
        else
            engine2 = spawn_and_greet(engine2; debug=debug)
            owns_engine2 = true
        end
    end

    # Start a new game - Base+MLP is standard Hive with all expansions
    if isempty(starting_position)
        game_state = "Base+MLP;InProgress;white[1]"
    else
        game_state = "Base+MLP;InProgress;white[1];" * starting_position
    end
    debug && println("[DEBUG] Sending newgame commands...")
    boards = Board[from_game_string(game_state), from_game_string(game_state)]
    for (i, engine) in enumerate([engine1, engine2])
        debug && println("[DEBUG] Sending newgame to engine $i: $game_state")
        if is_source[i]
            # see boards.
            debug && println("[DEBUG] Engine $i response: ok")
        else
            write(engine, "newgame $game_state\n")
            flush(engine)
            # Wait for ok
            read_until_ok(engine, engine_names[i]; timeout_s=10.0, debug=debug, context="newgame")
        end
    end
    debug && println("[DEBUG] Newgame commands complete")

    move_number = 0
    current_engine = engine1
    current_engine_i = 1
    current_color = "White"
    other_engine = engine2
    move_history = String[]
    source_board_1 = boards[1]
    source_board_2 = boards[2]

    max_moves = 500
    # Play until game over
    while move_number < max_moves
        move_number += 1
        debug && println("[DEBUG] Move $move_number starting, $current_color to play")

        # Ask current engine for best move with time limit
        debug && println(
            "[DEBUG] Requesting bestmove from $current_color $(engine_names[current_engine_i])"
        )
        move_start_time = time()
        if !is_source[current_engine_i]
            # Check if current engine is nokamute, either windows or linux version
            current_engine_path = engine_paths[current_engine_i]
            if contains(lowercase(current_engine_path), "nokamute")
                write(current_engine, "bestmove seconds $time_limit_s\n")
            else
                write(current_engine, "bestmove time 00:00:0$time_limit_s\n")
            end
            flush(current_engine)
            debug && println("[DEBUG] Bestmove request sent, waiting for response...")

            # Read the best move
            response_timeout_s = max(5.0, time_limit_s + 2.0)
            best_move = read_bestmove(
                current_engine,
                engine_names[current_engine_i];
                timeout_s=response_timeout_s,
                debug=debug,
            )
        else
            board = current_engine_i == 1 ? source_board_1 : source_board_2
            debug && println("[DEBUG] Bestmove request sent, waiting for response...")
            action = get_best_move(board; time_limit_s=time_limit_s, debug=false)
            best_move = move_string_from_action(board, action)
            debug && println("[DEBUG] $current_color response: $best_move")
            debug && println("[DEBUG] $current_color response: ok")
        end
        move_elapsed_s = time() - move_start_time
        debug && println("[DEBUG] Bestmove received: $best_move")
        debug && println(
            "[DEBUG] $current_color $(engine_names[current_engine_i]) thought for $(round(move_elapsed_s; digits=3))s",
        )

        if isempty(best_move)
            break
        end

        push!(move_history, best_move)

        # Update source boards with the move
        do_action(source_board_1, best_move)
        do_action(source_board_2, best_move)

        # Check for endgame on the source board
        if source_board_1.gameover
            finish = time()
            println("=== Game Over ===")
            show(GameString(source_board_1))
            if source_board_1.victor == WHITE
                println("Winner: White ($engine1_name)\n")
            elseif source_board_1.victor == BLACK
                println("Winner: Black ($engine2_name)\n")
            else
                println("Draw between $engine1_name and $engine2_name\n")
            end
            if owns_engine1
                shutdown_engine(engine1, engine1_name; debug=debug)
            end
            if owns_engine2
                shutdown_engine(engine2, engine2_name; debug=debug)
            end
            return source_board_1.victor
        end

        # Update game state
        game_state = game_state * ";" * best_move

        # Send the move to both engines
        debug && println("[DEBUG] Sending play command to both engines: $best_move")
        for engine in [current_engine, other_engine]
            engine_i = engine == engine1 ? 1 : 2
            if !is_source[engine_i]
                write(engine, "play $best_move\n")
                debug && println("[DEBUG] Sending play to engine $engine_i")
                flush(engine)
                # Wait for ok
                read_until_ok(
                    engine, engine_names[engine_i]; timeout_s=10.0, debug=debug, context="play"
                )
            else
                debug && println("[DEBUG] Engine $engine_i play response: ok")
            end
        end
        debug && println("[DEBUG] Play commands complete")

        # Switch players
        if current_engine == engine1
            current_engine = engine2
            current_engine_i = 2
            other_engine = engine1
            current_color = "Black"
        else
            current_engine = engine1
            current_engine_i = 1
            other_engine = engine2
            current_color = "White"
        end
    end

    if move_number >= max_moves
        println("=== Game aborted after $max_moves moves (forced draw) ===")
    end

    if owns_engine1
        shutdown_engine(engine1, engine1_name; debug=debug)
    end
    if owns_engine2
        shutdown_engine(engine2, engine2_name; debug=debug)
    end
    return Intsect.DRAW
end

"""
    read_positions(filepath::String)

Reads starting positions from a file where each line contains moves separated by semicolons.
Returns a vector of position strings.
"""
function read_positions(filepath::String)
    positions = String[]
    if !isfile(filepath)
        @warn "Position file not found: $filepath"
        return positions
    end

    open(filepath, "r") do io
        for line in eachline(io)
            line = strip(line)
            if !isempty(line)
                push!(positions, line)
            end
        end
    end

    return positions
end

"""
    format_results_block(results, positions, engine1_name, engine2_name)

Formats the RESULTS block as a string.
"""
function format_results_block(results, positions, engine1_name, engine2_name)
    io = IOBuffer()

    println(io, "\n" * "="^70)
    println(io, "RESULTS $engine1_name vs $engine2_name")
    println(io, "="^70)
    println(io, "Total games played: $(results["total_games"])")
    println(io, "Total positions: $(length(positions))")
    println(io, "Errors encountered: $(results["errors"])")
    println(io)

    engine1_total = results["engine1"]
    engine2_total = results["engine2"]
    draws = results["draws"]
    total = results["total_games"]

    if total > 0
        println(io, "$engine1_name:")
        println(
            io, "  Wins: $engine1_total / $total ($(round(engine1_total/total*100, digits=1))%)"
        )
        println(io)
        println(io, "$engine2_name:")
        println(
            io, "  Wins: $engine2_total / $total ($(round(engine2_total/total*100, digits=1))%)"
        )
        println(io)
        println(io, "Draws: $draws / $total ($(round(draws/total*100, digits=1))%)")
    else
        println(io, "No games completed successfully")
    end
    println(io, "="^70)

    return String(take!(io))
end

"""
    faceoff(engine1, engine2; time_limit=1.0, positions_file="starting_positions.txt", debug=false)

Plays two engines against each other through all starting positions, with each engine playing both colors.
- engine1: EngineSpec for first engine
- engine2: EngineSpec for second engine
- time_limit: Thinking time per move in seconds (default: 1.0)
- positions_file: File containing starting positions (default: "starting_positions.txt")
- debug: Print debug statements (default: false)

Returns a summary of results.
"""
function faceoff(
    engine1::EngineSpec,
    engine2::EngineSpec;
    time_limit_s=1,
    positions_file="./starting_positions.txt",
    debug=false,
    results_path=nothing,
    full_debug=false,
)
    engine1_name = engine1.name
    engine2_name = engine2.name
    println("\n" * "="^70)
    println("Faceoff between:")
    println("Engine 1: $engine1_name")
    println("Engine 2: $engine2_name")
    println("="^70 * "\n")
    # Read starting positions
    positions = read_positions(positions_file)

    # Spawn engines once and reuse across all games
    proc1 = !engine1.is_source ? spawn_and_greet(engine1; debug=full_debug) : nothing
    proc2 = !engine2.is_source ? spawn_and_greet(engine2; debug=full_debug) : nothing

    # Track results
    results = Dict("engine1" => 0, "engine2" => 0, "draws" => 0, "total_games" => 0, "errors" => 0)

    errors_log = String[]

    # Play each position with both color assignments
    for (pos_idx, position) in enumerate(positions)
        println("Position $pos_idx")
        println("Starting from $position")

        # Game 1: Engine1 as White, Engine2 as Black
        try
            result1 = play_one_match(
                engine1,
                engine2,
                time_limit_s;
                starting_position=position,
                debug=full_debug,
                engine1_proc=proc1,
                engine2_proc=proc2,
            )

            if result1 == WHITE
                results["engine1"] += 1
            elseif result1 == BLACK
                results["engine2"] += 1  # Engine2 won as Black
            else
                results["draws"] += 1
            end
            results["total_games"] += 1
        catch e
            if e isa InterruptException
                println("InterruptException received, logging results so far first.")
                results_block = format_results_block(results, positions, engine1_name, engine2_name)
                print(results_block)
                rethrow()
            else
                debug && rethrow()
                results["errors"] += 1
                error_msg = "Position $pos_idx, Game 1 (Engine1 White): $(sprint(showerror, e))"
                push!(errors_log, error_msg)
                @warn error_msg
            end
        end

        # Game 2: Engine2 as White, Engine1 as Black
        try
            result2 = play_one_match(
                engine2,
                engine1,
                time_limit_s;
                starting_position=position,
                debug=full_debug,
                engine1_proc=proc2,
                engine2_proc=proc1,
            )

            if result2 == WHITE
                results["engine2"] += 1
            elseif result2 == BLACK
                results["engine1"] += 1  # Engine1 won as Black
            else
                results["draws"] += 1
            end
            results["total_games"] += 1
        catch e
            if e isa InterruptException
                println("InterruptException received, logging results so far first.")
                results_block = format_results_block(results, positions, engine1_name, engine2_name)
                print(results_block)
                rethrow()
            else
                debug && rethrow()
                results["errors"] += 1
                error_msg = "Position $pos_idx, Game 2 (Engine2 White): $(sprint(showerror, e))"
                push!(errors_log, error_msg)
                @warn error_msg
            end
        end

        if pos_idx > 5 && pos_idx % 10 == 0
            results_block = format_results_block(results, positions, engine1_name, engine2_name)
            print(results_block)
        end
    end

    results_block = format_results_block(results, positions, engine1_name, engine2_name)
    print(results_block)

    if results_path !== nothing
        open(results_path, "a") do io
            write(io, results_block)
        end
    end

    # Shutdown engines that we spawned
    if proc1 !== nothing
        shutdown_engine(proc1, engine1_name; debug=full_debug)
    end
    if proc2 !== nothing
        shutdown_engine(proc2, engine2_name; debug=full_debug)
    end

    if !isempty(errors_log)
        println("\n" * "="^70)
        println("ERRORS LOG")
        println("="^70)
        for error_msg in errors_log
            println(error_msg)
        end
    end
    println("="^70)

    return results
end

function run_arena(;
    debug=false,
    time_limit_s=0.05,
    engines_file="./engines/engines.yaml",
    full_debug=false,
    results_path=nothing,
)
    if results_path !== nothing
        open(results_path, "w") do _
        end
    end

    engines = YAML.load_file(engines_file)
    intsect_engines = engines["intsect"]
    existing_engines = engines["existing_engines"]
    if existing_engines === nothing
        existing_engines = String[]
    end

    engines_dir = "engines"

    intsect_specs = parse_engine_list(intsect_engines; engines_dir=engines_dir)
    existing_specs = parse_engine_list(existing_engines; engines_dir=engines_dir)
    if isempty(intsect_specs)
        @warn "No valid intsect engines found; check engines.yaml entries."
        return nothing
    end

    # Play each intsect engine against the next one
    for i in 1:(length(intsect_specs) - 1)
        older_intsect = intsect_specs[i]
        newer_intsect = intsect_specs[i + 1]
        faceoff(
            older_intsect,
            newer_intsect;
            time_limit_s=time_limit_s,
            debug=debug,
            full_debug=full_debug,
            results_path=results_path,
        )
    end
    # Make all existing engines fight the latest intsect
    latest_intsect = intsect_specs[end]
    for existing in existing_specs
        faceoff(
            latest_intsect,
            existing;
            time_limit_s=time_limit_s,
            debug=debug,
            full_debug=full_debug,
            results_path=results_path,
        )
    end
    # And check if the new engine is better against the existing engines than the previous build
    if length(intsect_specs) > 1
        runner_up_intsect = intsect_specs[end - 1]
        for existing in existing_specs
            faceoff(
                runner_up_intsect,
                existing;
                time_limit_s=time_limit_s,
                debug=debug,
                full_debug=full_debug,
                results_path=results_path,
            )
        end
    end

    return nothing
end

function parse_engine_entry(entry::String; engines_dir="engines")
    entry_str = strip(entry)
    if isempty(entry_str)
        return nothing
    end
    if entry_str == "source"
        return EngineSpec("source", nothing, true, "source")
    end

    parts = split(entry_str)
    if length(parts) >= 2 && (parts[1] == "intsect")
        folder = strip(join(parts[2:end], " "))
        if isempty(folder)
            @warn "intsect entry missing folder: $entry"
            return nothing
        end
        folder_path = isabspath(folder) ? folder : joinpath(".", engines_dir, folder)
        folder_path = abspath(folder_path)
        exe_path = joinpath(folder_path, "bin", "intsect.exe")
        if !isdir(folder_path) || !isfile(exe_path)
            @warn "Skipping intsect entry; folder or exe missing: $folder_path"
            return nothing
        end
        bat_path = joinpath(".", engines_dir, "intsect.bat")
        bat_cmd = `$(bat_path) $(folder_path)`
        exe_cmd = `$exe_path`
        println("running $exe_path")
        engine_name = "intsect-" * basename(folder_path)
        return EngineSpec(engine_name, bat_cmd, false, exe_path)
    end

    path = isabspath(entry_str) ? entry_str : joinpath(".", engines_dir, entry_str)
    path = normpath(path)
    cmd = `$(path)`
    name = split(basename(path), ".")[1]
    return EngineSpec(name, cmd, false, path)
end

function parse_engine_list(entries; engines_dir="engines")
    specs = EngineSpec[]
    for entry in entries
        spec = parse_engine_entry(string(entry); engines_dir=engines_dir)
        if spec !== nothing
            push!(specs, spec)
        end
    end
    return specs
end