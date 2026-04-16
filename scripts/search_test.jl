using Intsect
using PProf: PProf
using Profile: Profile

board = handle_newgame_command(MLPGame)

board = from_game_string(
    # raw"Base;InProgress;White[13]",
    raw"Base;InProgress;White[13];wA1;bA1 wA1/;wB1 wA1\;bA2 bA1/;wG1 /wA1;bQ bA1-;wQ -wG1;bA2 bQ\\",
)

time_limit_s = 0.1

best_action = get_best_move(board; time_limit_s=time_limit_s, debug=true)

# best_action = get_best_move(board; time_limit_s=time_limit_s, debug=true)

# Profile.clear()
# Profile.@profile get_best_move(board; time_limit_s=time_limit_s, debug=false)
# PProf.pprof()

# Profile.Allocs.clear()
# Profile.Allocs.@profile sample_rate = 0.01 get_best_move(
#     board; time_limit_s=time_limit_s, debug=false
# )
# PProf.Allocs.pprof()

show(board)
println("^^ board before move ^^")