module Intsect

using StaticArrays
using EnumX
using DataStructures
using Bumper
using InteractiveUtils
using Printf
using UnsafeArrays

const SUPPRESS_ACTION_ERROR_OUTPUT = Ref(false)

# structs
export Board
export Move
export Placement
export Climb
export Pass
export GameString
export Action
export HexSet
export SuggestedActions
export MoveOrderingStats

# constants
export GRID_SIZE
export ROW_SIZE
export GAMETYPE
export BOARD
export EMPTY_TILE
export NOT_PLACED
export INVALID_LOC
export MID
export WHITE
export BLACK
export MAX_NUMS
export PERFT_BUFFER
# Constants for move generation, constructed in game
export ALL_ACTIONS
export ALL_ALL_NEIGHS

# enums
export Bug
export Gametype
export Direction

# methods
export set!
export remove!
export toggle!
export add!
export example
export start
export perft
export direction_from_string
export show
export get_hash_value
export fill_placement_locs_bb
export show_valid_actions
export show_pinned
export get_tile_from_string
export get_tile_info
export get_tile_name
export get_tile_height
export get_tile_bug
export get_tile_bug_num
export get_tile_color
export allneighs
export get_best_move
export start_match_player
export allneighs_view
export apply_direction
export action_from_move_string
export do_action
export validactions
export validactions!
export validactions_indices
export add_placements
export verify_perft
export isvalid_shifted_tile
export push_slidelocs!
export antmoves
export grasshoppermoves
export spidermoves
export beetlemoves
export ladybugmoves
export queenmoves
export pillbugmoves_normal
export pillbugmoves_throw
export mosquitomoves
export handle_newgame_command
export get_tile_on_board
export set_tile_on_board
export set_loc
export get_loc
export move_string_from_action
export gamestring
export undo
export evaluate_board
export from_game_string
export get_pinned_tiles
export format_with_dots
export extract_valid_actions

export BaseGame
export LGame
export PGame
export MGame
export MPGame
export MLGame
export LPGame
export MLPGame

# Game core files
include("game/enums.jl")
include("game/constants.jl")
include("game/structs.jl")
include("game/game.jl")
include("game/hex_set.jl")
include("main.jl")
include("game/show_methods.jl")
include("game/move_generation.jl")
include("game/perft.jl")
include("game/hash_values.jl")

# AI files
include("ai/suggested_actions.jl")
include("ai/search.jl")
include("ai/evaluate.jl")

# Match player
include("match_player/player.jl")

function @main(ARGS)
    try
        start()
        return 0
    catch e
        Base.invokelatest(Base.display_error, e, catch_backtrace())
        return 1
    end
end
# Precompilation for juliac builds
if Base.generating_output()
    using Intsect
    # Precompile frequently used functions with representative workload
    try
        # Load a complex mid-game position
        game_string = raw"Base+MLP;InProgress;white[5];wL;bL wL\;wM \wL;bM bL\;wA1 /wM;bA1 /bL;wQ wM/;bQ bM-;wA2 wQ\;bA2 bA1\;wA2 /bA2;bA1 /wA1;wB1 wQ\;bP bA2-;wM wB1\;bB1 \bA2;wA3 -wQ;bS1 /bA1;wA3 -bS1"
        board = from_game_string(game_string)

        # Precompile perft (move generation)
        perft(4, board)

        # Precompile search/evaluation
        get_best_move(board; time_limit_s=0.1, debug=false)

    catch e
        println("Warning: Precompilation failed - ", e)
    end
end

module Arenant
    using Intsect
    using YAML

    export run_arena
    export inspect_game
    export play_one_match

    include("arenant/arenant.jl")
    include("arenant/inspect_game.jl")
end

export Arenant

end # module Intsect
