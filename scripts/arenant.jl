using Intsect
using PProf: PProf
using Profile: Profile

time_limit = 0.02
debug = false
full_debug = false

Arenant.run_arena(;
    debug=debug,
    time_limit_s=time_limit,
    full_debug=full_debug,
    results_path="./arenant_results.txt",
)

# Profile.clear()
# Profile.@profile Arenant.run_arena(; debug=debug, time_limit_s=time_limit, full_debug=full_debug)
# PProf.pprof()
# src1 = Arenant.EngineSpec("source", ``, true, "")
# src2 = Arenant.EngineSpec("source", ``, true, "")

# Arenant.play_one_match(
#     src1,
#     src2,
#     # "engines\\intsect-first-release.bat",
#     time_limit;
#     starting_position=raw"wL;bL wL\;wA1 \wL;bM bL\;wQ /wA1;bA1 /bL;wA1 bM-;bQ bA1\\",
#     debug=debug,
# )