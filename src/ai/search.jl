
const MAX_KILLER_PLY = PV_STORE_SIZE
const KILLERS_PER_PLY = 2

mutable struct KillerTable
    moves::Matrix{Int32}  # MAX_KILLER_PLY × KILLERS_PER_PLY
end

function KillerTable()
    return KillerTable(fill(Int32(-1), MAX_KILLER_PLY, KILLERS_PER_PLY))
end

function clear!(kt::KillerTable)
    return kt.moves .= Int32(-1)
end

function store_killer!(kt::KillerTable, ply::Int, move::Int32)
    if move == Int32(-1) || move == pass_index()
        return nothing
    end
    # Don't store duplicates
    if kt.moves[ply, 1] == move
        return nothing
    end
    # Shift slot 1 → slot 2, new move → slot 1
    kt.moves[ply, 2] = kt.moves[ply, 1]
    return kt.moves[ply, 1] = move
end

function is_killer(kt::KillerTable, ply::Int, move::Int32)::Bool
    return kt.moves[ply, 1] == move || kt.moves[ply, 2] == move
end

function get_best_move(board::Board; depth=5000, time_limit_s=10.0, debug=true)::Action
    timed_out = Ref(false)
    if time_limit_s <= 0
        time_limit_s = 9999
    end
    debug && show(GameString(board))
    timer = Timer(time_limit_s) do _
        debug && println("stopping search bc of time")
        timed_out[] = true
    end

    nodes_processed = Ref(0)

    best_move, best_score = iterative_deepening(
        board, board.ply, timed_out, depth, nodes_processed, debug;
    )

    close(timer)

    if debug
        println("done")
        println("Nodes processed: $(format_with_dots(nodes_processed[]))")
        println("Best score: $best_score")
        show(ALL_ACTIONS[best_move], board)
    end

    return ALL_ACTIONS[best_move]
end

function iterative_deepening(
    board::Board,
    initial_ply::UInt16,
    timed_out::Ref{Bool},
    iterative_deepening_depth::Int,
    nodes_processed::Ref{Int},
    debug::Bool,
)
    # For testing, always test the move "wA3 \bM" at the root
    best_move, second_best, best_score = Int32(-1), Int32(-1), -Inf32

    # Clear the pv store
    for i in 1:PV_STORE_SIZE
        all(board.pv_store[i] .== -1) && break
        board.pv_store[i] .= -1
    end

    sa_array = Vector{SuggestedActions}(undef, 200)
    for i in eachindex(sa_array)
        sa_array[i] = SuggestedActions(Int32(-1) * ones(Int32, 20), board)
    end

    killer_table = SuggestedActions[
        SuggestedActions(Int32(-1) * ones(Int32, 5), board),
        SuggestedActions(Int32(-1) * ones(Int32, 5), board),
    ]

    @no_escape begin
        for depth in 1:iterative_deepening_depth
            debug && println("iterative deepening at depth $depth")
            debug && println("best, second best = $best_move, $second_best")
            extension_budget = depth ÷ 2

            buffer_idx = 1

            sa = sa_array[1]
            add!(sa, second_best)

            best_score = minimax(
                board,
                initial_ply,
                timed_out,
                depth,
                depth,
                extension_budget,
                buffer_idx,
                nodes_processed,
                debug,
                board.pv_store[1][1],
                killer_table;
                suggested_moves_array=sa_array,
            )
            new_best_move = board.pv_store[1][1]
            if new_best_move != best_move
                # The idea behind this to test a move that was previously thought to be good at the root lvl, not at the iterative deepening lvl
                second_best = best_move
                best_move = new_best_move
            end

            if timed_out[] || best_score == Inf32 || best_score == -Inf32
                break
            end
        end
    end
    if best_move == Int32(-1)
        best_move = action_index(rand(validactions(board), 1)[begin])
        debug && println("No valid moves found, returning a random move")
        debug && show(ALL_ACTIONS[best_move], board)
        # No valid moves, just return a pass
    end

    return best_move, best_score
end

function minimax(
    board::Board,
    initial_ply::UInt16,
    timed_out::Ref{Bool},
    depth::Int,
    initial_depth::Int,
    extension_budget::Int,
    buffer_idx::Int,
    nodes_processed::Ref{Int},
    debug::Bool,
    pv_move::Int32,
    killer_table::Vector{SuggestedActions};
    suggested_moves_array::Vector{SuggestedActions},
    alpha::Float32=-Inf32,
    beta::Float32=Inf32,
    pv_node::Bool=true,
)
    final_lvl = depth <= 1
    if board.gameover || depth <= 0
        score = evaluate_board(board; debug=false)
        return score
    end

    buffer =
        buffer_idx <= length(PERFT_BUFFER) ? PERFT_BUFFER[buffer_idx] : default_buffer(AllocBuffer)
    suggested_moves =
        buffer_idx <= length(suggested_moves_array) ? suggested_moves_array[buffer_idx] :
        DUMMY_SUGGESTED_ACTIONS

    current_hash = get_hash_value(board)
    search_entry = board.search_store[(current_hash & SEARCH_STORE_MASK) + 1]

    stored_suggested_move = Int32(-1)

    if search_entry.full_hash == current_hash
        stored_score = search_entry.score
        stored_suggested_move = search_entry.action_chosen
        if search_entry.depth >= depth && !pv_node
            if search_entry.type == :exact
                return stored_score
            elseif search_entry.type == :lowerbound && stored_score >= beta
                return stored_score
            end
        end
        # This should be changed to a single hash move value
        add!(suggested_moves, stored_suggested_move)
    end

    # maximizing = board.current_color == WHITE
    score_at_depth = -Inf32
    action_chosen_at_depth = pass_index()

    steps_below_initial_ply = board.ply - initial_ply

    type = :exact
    if steps_below_initial_ply <= initial_depth - 3
        # Yield to allow timer to trigger
        yield()
    end

    ply = Int(steps_below_initial_ply) + 1  # 1-indexed for Julia

    @no_escape buffer begin
        move_buffer = @alloc(eltype(Int32), VALID_BUFFER_SIZE)
        ordered_move_buffer = @alloc(eltype(Int32), VALID_BUFFER_SIZE)
        validactions!(board, move_buffer)

        good_moves_buffer = @alloc(eltype(Int32), VALID_BUFFER_SIZE)
        normal_moves_buffer = @alloc(eltype(Int32), VALID_BUFFER_SIZE)
        bad_moves_buffer = @alloc(eltype(Int32), VALID_BUFFER_SIZE)
        suggested_moves_buffer = @alloc(eltype(Int32), length(suggested_moves.actions))
        killer_moves_buffer = @alloc(eltype(Int32), VALID_BUFFER_SIZE)

        idx = order_moves!(
            ordered_move_buffer,
            board,
            move_buffer,
            pv_move,
            suggested_moves,
            killer_table[board.current_color],
            good_moves_buffer,
            normal_moves_buffer,
            bad_moves_buffer,
            suggested_moves_buffer,
            killer_moves_buffer,
        )

        for i in 1:idx
            action_as_index = ordered_move_buffer[i]
            action = Intsect.ALL_ACTIONS[action_as_index]

            do_action(board, action_as_index)

            nodes_processed[] += 1

            new_depth = depth - 1
            # do_late_move_reduction = false
            # Late move reduction, remove depth further if late in the move list and not interesting
            do_late_move_reduction =
                depth > 2 &&
                i > 2 &&
                (
                    board.queen_pos_white < 0 ||
                    board.queen_pos_black < 0 ||
                    !Intsect.are_neighs(
                        action.goal_loc,
                        board.current_color == WHITE ? board.queen_pos_white :
                        board.queen_pos_black,
                    )
                )
            score = -Inf32
            if do_late_move_reduction
                # also try to do null move pruning here, assume move is bad and do more aggressive pruning, reset if necessary
                further_reduced_depth = new_depth - Int(round(0.99 + log(depth) + log(i) / 3.14))
                score =
                    -1 * minimax(
                        board,
                        initial_ply,
                        timed_out,
                        further_reduced_depth,
                        initial_depth,
                        extension_budget,
                        buffer_idx + 1,
                        nodes_processed,
                        debug,
                        board.pv_store[1][steps_below_initial_ply + 2], # PV move to try first at next depth
                        killer_table;
                        alpha=-beta,
                        beta=-alpha,
                        suggested_moves_array=suggested_moves_array, # These are good moves the opp might be able to make
                        pv_node=pv_node && (i == 1),
                    )
            end
            if !do_late_move_reduction || score > alpha
                score =
                    -1 * minimax(
                        board,
                        initial_ply,
                        timed_out,
                        new_depth,
                        initial_depth,
                        extension_budget,
                        buffer_idx + 1,
                        nodes_processed,
                        debug,
                        board.pv_store[1][steps_below_initial_ply + 2], # PV move to try first at next depth
                        killer_table;
                        alpha=-beta,
                        beta=-alpha,
                        suggested_moves_array=suggested_moves_array, # These are good moves the opp might be able to make
                        pv_node=pv_node && (i == 1),
                    )
            end

            undo(board)

            if score > score_at_depth || score_at_depth == -Inf32
                score_at_depth = score
                action_chosen_at_depth = action_as_index

                if score_at_depth >= alpha
                    # This is a pv move
                    alpha = score_at_depth

                    board.pv_store[steps_below_initial_ply + 1][steps_below_initial_ply + 1] =
                        action_as_index
                    if final_lvl
                        # Terminate PV - no children to copy from
                        if steps_below_initial_ply + 2 <= PV_STORE_SIZE
                            board.pv_store[steps_below_initial_ply + 1][steps_below_initial_ply + 2] = Int32(
                                -1
                            )
                        end
                    else
                        board.pv_store[steps_below_initial_ply + 1][(steps_below_initial_ply + 2):end] = board.pv_store[steps_below_initial_ply + 2][(steps_below_initial_ply + 2):end]
                    end
                    if debug
                        search_debug_print(
                            board, initial_ply, score_at_depth, action_chosen_at_depth
                        )
                    end
                end
            end

            if beta <= score_at_depth
                # beta cut off
                type = :lowerbound
                add!(killer_table[board.current_color], action_as_index)
                break
            end

            if timed_out[]
                # If we are timed out we stop after one iteration 
                break
            end
        end
    end

    if (type == :exact || search_entry.depth <= depth)
        # Much to improve with transpositions tables.
        # https://deepwiki.com/search/does-stock-fish-have-a-tt-and_9a5e715f-a810-42f7-8ffb-901171686393
        # https://www.chessprogramming.org/Triangular_PV-table
        entry = SearchStoreEntry(
            current_hash, score_at_depth, Int32(depth), action_chosen_at_depth, type, Int32(-1)
        )
        board.search_store[(current_hash & SEARCH_STORE_MASK) + 1] = entry
    end

    return score_at_depth
end

function search_debug_print(board, initial_ply, score_at_depth, action_chosen_at_depth)
    # Printing things
    if board.ply == initial_ply
        show(ALL_ACTIONS[action_chosen_at_depth], board)
        println("Best path so far (score: $score_at_depth):")
        # Print the principal variation from pv_store
        done_actions = 0
        for action_idx in board.pv_store[begin]
            if action_idx == -1
                break
            end
            done_actions += 1
            print("  PV Move $(done_actions): ")
            show(ALL_ACTIONS[action_idx], board)

            do_action(board, action_idx)
        end
        for _ in 1:done_actions
            undo(board)
        end
        println()
    end
end

function count_queen_spots(board)
    if board.queen_pos_white < 0 || board.queen_pos_black < 0
        return -1, -1
    end
    open_white = 0
    for n in allneighs(board.queen_pos_white)
        if get_tile_on_board(board, n) == EMPTY_TILE
            open_white += 1
        end
    end
    open_black = 0
    for n in allneighs(board.queen_pos_black)
        if get_tile_on_board(board, n) == EMPTY_TILE
            open_black += 1
        end
    end
    return open_white, open_black
end

function order_moves!(
    ordered_move_buffer,
    board,
    move_buffer,
    last_best::Int32,
    suggested_moves::SuggestedActions,
    killer_table::SuggestedActions,
    good_moves_buffer,
    normal_moves_buffer,
    bad_moves_buffer,
    suggested_moves_buffer,
    killer_moves_buffer,
)
    # Always try the last best move first if it's possible 
    # Validate that suggested moves are actually valid for the current position.
    valid_best_move = Int32(-1)

    suggested_moves_index = 0
    killer_moves_index = 0
    good_moves_index = 0
    normal_moves_index = 0
    bad_moves_index = 0

    for move in 1:(board.action_index - 1)
        action_as_index = move_buffer[move]
        if action_as_index == last_best
            valid_best_move = action_as_index

        elseif contains(action_as_index, suggested_moves)
            suggested_moves_buffer[suggested_moves_index += 1] = action_as_index

        elseif contains(action_as_index, killer_table)
            killer_moves_buffer[killer_moves_index += 1] = action_as_index

        elseif action_type(action_as_index) == Move
            move = ALL_MOVEMENTS[action_as_index - MAX_PLACEMENT_INDEX]
            tile = get_tile_on_board(board, move.moving_loc)
            bug = UInt8(get_tile_bug(tile))
            color = get_tile_color(tile)
            if (
                bug == UInt8(Bug.ANT) ||
                bug == UInt8(Bug.MOSQUITO) ||
                color != board.current_color ||
                bug == UInt8(Bug.PILLBUG)
            )
                good_moves_buffer[good_moves_index += 1] = action_as_index
            elseif (bug == UInt8(Bug.GRASSHOPPER) || bug == UInt8(Bug.SPIDER))
                bad_moves_buffer[bad_moves_index += 1] = action_as_index
            else
                normal_moves_buffer[normal_moves_index += 1] = action_as_index
            end
        elseif action_type(action_as_index) == Placement
            placement = ALL_PLACEMENTS[action_as_index]
            bug = get_tile_bug(placement.tile)
            if bug == UInt8(Bug.ANT) || bug == UInt8(Bug.MOSQUITO)
                normal_moves_buffer[normal_moves_index += 1] = action_as_index
            else
                bad_moves_buffer[bad_moves_index += 1] = action_as_index
            end
        else
            normal_moves_buffer[normal_moves_index += 1] = action_as_index
        end
    end
    idx = 0
    if valid_best_move != Int32(-1)
        ordered_move_buffer[idx += 1] = valid_best_move
    end
    for move_i in 1:killer_moves_index
        ordered_move_buffer[idx += 1] = killer_moves_buffer[move_i]
    end
    for move_i in 1:suggested_moves_index
        ordered_move_buffer[idx += 1] = suggested_moves_buffer[move_i]
    end
    for move_i in 1:good_moves_index
        ordered_move_buffer[idx += 1] = good_moves_buffer[move_i]
    end
    for move_i in 1:normal_moves_index
        ordered_move_buffer[idx += 1] = normal_moves_buffer[move_i]
    end
    for move_i in 1:bad_moves_index
        ordered_move_buffer[idx += 1] = bad_moves_buffer[move_i]
    end

    return idx
end