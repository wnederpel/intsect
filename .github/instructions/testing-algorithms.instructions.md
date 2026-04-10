---
description: "Use when making algorithmic improvements to search, evaluation, or move generation. Covers how to validate correctness, measure performance, and test playing strength."
applyTo: "src/ai/**"
---
# Testing Algorithmic Improvements

## Running tests and scripts

- Run unit tests via `julia --project=. test/runtests.jl` in a terminal.
- Run scripts (profiling, benchmarks, arenant) via the `mcp_julia-repl_exec_repl` MCP tool, NOT via `julia` in a terminal.

## Available validation methods

Choose the appropriate level(s) based on context:

### 1. Unit tests (`test/`)
Run the test suite to verify correctness after any change that effects move generation, placement, game state, and perft correctness:
```
julia --project=. test/runtests.jl
```
Unit tests do not cover search.

### 2. Perft correctness (`scripts/perft_test.jl`)
Verifies move generation produces the correct number of nodes at each depth. Use after changes to move generation or game state logic. Run via MCP REPL.

### 3. Perft benchmarks (`scripts/perft_benchmark.jl`)
Measures KN/s and memory per node. Use to verify performance improvements or catch regressions. See `profiling.instructions.md` for details on agent-readable output. Run via MCP REPL.

### 4. Arenant matches (`scripts/arenant.jl`)
Runs matches against other engines to test playing strength. Use after changes to search or evaluation:

Do this via powershell
```powershell
julia --project=. --startup-file=no -e 'using Intsect; Intsect.Arenant.run_arena(debug=true, time_limit_s=0.02, full_debug=false, results_path="./arenant_results.txt")'
```
you can edit the parameters.
You can edit what engines fight each other in the arenant in engines.yaml

The results will be stored in ./arenant_results.txt and you can read this file to see the score. the 'source' engine is the current state of the code. This command can take ~5 minutes. Consider adding a timeout or canceling the command if you see no responses.

**Important:** The `source` engine in arenant is always the current state of the code. The opponent engine(s) are configured in `engines/engines.yaml`.

**Monitoring arenant progress:** Run arenant in async mode. Arenant prints RESULTS checkpoints every 10 positions (20 games). Use `get_terminal_output` to check progress. Look for lines like 
`engine name:\n Wins: XX / YY (ZZ.Z%)`. XX are the wins by the engines, YY the total number of games, ZZ the win percentage.


## What to validate when
this is vibes based, you decide!