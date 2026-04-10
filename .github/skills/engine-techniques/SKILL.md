---
name: engine-techniques
description: 'Game engine programming techniques for Hive. Use when implementing or improving search, evaluation, move ordering, transposition tables, time management, or debugging search behavior. Covers alpha-beta enhancements, pruning, move ordering heuristics, eval term design, TT replacement schemes, and Hive-specific adaptations of chess engine theory.'
---

# Game Engine Techniques for Hive

Reference skill for implementing and improving the Intsect Hive engine. Covers techniques from chess programming adapted for Hive's unique properties.

## Hive vs Chess — Key Differences

All chess engine literature assumes captures, material imbalance, and a fixed set of pieces. Hive breaks these assumptions:

| Chess assumption | Hive reality | Impact |
|-----------------|-------------|--------|
| Captures remove pieces | No captures — all pieces stay | No MVV-LVA, no SEE, no quiescence on captures |
| Material imbalance drives eval | All pieces always exist | Eval is purely positional (mobility, queen safety, pinning) |
| ~30 legal moves typical | 50-200+ legal moves common | Move ordering and pruning are critical for depth |
| Pieces start on the board | Pieces enter via placement | Opening theory = placement strategy |
| King is stationary target | Queen can be relocated (before pinned) | Queen safety is dynamic |

**When reading CPW articles, always ask:** does this technique rely on captures or material? If so, find the Hive equivalent.

## Search

### Current State
The engine uses negamax with alpha-beta pruning, iterative deepening, PV tracking, TT cutoffs, and killer moves. See `src/ai/search.jl`.

### Alpha-Beta Enhancements

**Principal Variation Search (PVS)**
Search the first move (expected PV move) with a full window. Search remaining moves with a null/scout window (alpha, alpha+1). If the scout search fails high, re-search with the full window.
- Ref: https://www.chessprogramming.org/Principal_Variation_Search
- Benefit: reduces nodes searched when move ordering is good
- Risk: if move ordering is bad, re-searches add overhead

**Aspiration Windows**
At each iterative deepening iteration, search with a narrow window around the previous iteration's score instead of (-∞, +∞). Widen and re-search on fail-high or fail-low.
- Ref: https://www.chessprogramming.org/Aspiration_Windows
- Typical initial window: ±0.5 to ±1.0 (in your Float32 score units)
- Widen by 4× on failure, fall back to full window after 2 failures

**Null Move Pruning**
Skip your turn (make a "null move") and search at reduced depth. If the score is still >= beta, prune this branch. The idea: if doing nothing is already good enough, actually moving will be even better.
- Ref: https://www.chessprogramming.org/Null_Move_Pruning
- **Hive adaptation**: Hive has a pass action, so null move is literally passing. Be careful: passing in Hive is a real (sometimes forced) move, so null-move pruning may need restrictions (e.g., don't apply when the queen is nearly surrounded).
- Typical reduction: R=2 or R=3

**Late Move Reductions (LMR)**
Moves ordered late (not PV, not killer, not "good") are unlikely to be best. Search them at reduced depth first; re-search at full depth only if they beat alpha.
- Ref: https://www.chessprogramming.org/Late_Move_Reductions
- **Hive adaptation**: since there are no captures to exempt, exempt queen-threatening moves and moves to/from queen-adjacent hexes instead
- Don't reduce the first N moves (typically N=3-4)
- Don't reduce at low depths (depth < 3)

**Futility Pruning**
Near leaf nodes, if the static eval + a margin is still below alpha, skip searching quiet moves.
- Ref: https://www.chessprogramming.org/Futility_Pruning
- **Hive adaptation**: all moves are "quiet" (no captures), so this needs careful margins. Consider pruning only moves classified as "bad" in your move ordering.

**Search Extensions**
Extend search depth for critical moves (e.g., moves that surround the queen, moves that change queen liberty count from 2→1).
- Ref: https://www.chessprogramming.org/Extensions
- Budget extensions per path to avoid explosion (already in the codebase: `extension_budget`)

### References
- Iterative Deepening: https://www.chessprogramming.org/Iterative_Deepening
- Alpha-Beta: https://www.chessprogramming.org/Alpha-Beta
- Negamax: https://www.chessprogramming.org/Negamax

## Move Ordering

Good move ordering is the single biggest factor in alpha-beta efficiency. With Hive's high branching factor, the difference between good and bad ordering can be 10-100× in nodes searched.

### Priority Levels (high to low)

1. **TT/PV move** — The best move from a previous search of this position. Almost always the best move again.
2. **Suggested moves** — Moves from the `SuggestedActions` system (heuristic pre-screening).
3. **Killer moves** — Quiet moves that caused beta cutoffs at the same ply in sibling nodes. Already implemented with 2 slots per ply + same-side killer (ply-2).
4. **Good moves** — Moves of strong pieces (Ants, Mosquitoes). Currently the "good" category.
5. **Normal moves** — Everything else.
6. **Bad moves** — Moves of weak/slow pieces (Grasshoppers, Spiders in the current ordering).

### Potential Improvements

**History Heuristic**
Track how often each move causes a beta cutoff across the entire search. Use this count to score non-killer quiet moves. Reset at each iterative deepening iteration.
- Ref: https://www.chessprogramming.org/History_Heuristic
- Implementation: `history[color][action_index] += depth * depth` on cutoff
- Sort normal/bad moves by history score

**Countermove Heuristic**
For each move the opponent just made, track which response move caused a cutoff. When the opponent repeats that move, try the countermove early.
- Ref: https://www.chessprogramming.org/Countermove_Heuristic
- User note: is this not the same as the killer heuristic?

**Queen-Proximity Ordering**
Hive-specific: moves to hexes adjacent to the enemy queen are likely tactical. Prioritize them.

### References
- Move Ordering overview: https://www.chessprogramming.org/Move_Ordering
- Killer Heuristic: https://www.chessprogramming.org/Killer_Heuristic
- History Heuristic: https://www.chessprogramming.org/History_Heuristic

## Transposition Table

### Current State
The engine uses a single-entry hash table (`search_store`) indexed by `hash & SEARCH_STORE_MASK`. Entries store: full hash, score, depth, best move, bound type. TT cutoffs are skipped on PV nodes.

### Replacement Schemes

**Always-Replace** (current): New entries overwrite old ones regardless of depth. Simple but loses deep entries.

**Depth-Preferred**: Only replace if the new entry has >= depth. Preserves deep searches but can fill with stale entries.

**Two-Tier (Stockfish-style)**: Two slots per bucket — one depth-preferred, one always-replace. Best of both worlds.
- Ref: https://www.chessprogramming.org/Transposition_Table#Replacement_Strategies

### Entry Aging
Add a generation counter to entries. Increment each search. Prefer replacing old-generation entries. Prevents stale entries from blocking fresh results.

### Common TT Bugs
- **Type-1 errors (hash collisions)**: Two positions share the same hash. Mitigated by storing and checking `full_hash`. Verify `full_hash` covers enough bits.
- **Type-2 errors (index collisions)**: Two positions map to the same table slot. Solved by better replacement schemes.
- **TT and PV interaction**: Never use TT cutoffs on PV nodes (already implemented). Ensure TT best move is still validated for legality in the current position.
- **Score adjustment for game-over**: If TT stores a mate/win score, it must be adjusted for ply distance when retrieved at a different depth. Currently using `Inf32`/`-Inf32` — consider adding ply-distance.

### References
- Transposition Table: https://www.chessprogramming.org/Transposition_Table
- Zobrist Hashing: https://www.chessprogramming.org/Zobrist_Hashing

## Evaluation

### Current State
The evaluation in `src/ai/evaluate.jl` covers: queen safety (surround count), top-of-hive bonus, piece freedom (mobility), and pieces-in-hand penalty. Eval cache keyed by Zobrist hash.

### Tapered Evaluation
Weight eval terms differently based on game phase. Opening: prioritize development. Midgame: prioritize mobility and position. Endgame: prioritize queen surround count.
- Ref: https://www.chessprogramming.org/Tapered_Eval
- Use `board.ply` or piece-count as the phase indicator

### Tuning Weights

**Texel Tuning**
Collect positions from self-play with known outcomes (1.0 = white win, 0.0 = black win, 0.5 = draw). Optimize eval weights to minimize the mean squared error between sigmoid(eval) and the game outcome.
- Ref: https://www.chessprogramming.org/Texel%27s_Tuning_Method
- Requires ~100k+ labeled positions

**Manual Tuning via Arenant**
Change a weight, run arenant matches, measure win rate change. Slow but simple. Use statistical significance: ~100+ games minimum for small Elo differences.

### References
- Evaluation overview: https://www.chessprogramming.org/Evaluation
- Tapered Eval: https://www.chessprogramming.org/Tapered_Eval
- Texel Tuning: https://www.chessprogramming.org/Texel%27s_Tuning_Method

## Time Management

Control how long the engine thinks per move. Critical for timed games.

**Fixed allocation**: `total_time / expected_remaining_moves`. Simple but wasteful.

**Dynamic allocation**: Think longer on complex positions (many legal moves, score instability across iterations) and shorter on obvious ones (only 1-2 legal moves, score stable).

**Score stability**: If the best move and score don't change across 2-3 iterative deepening iterations, stop early (soft timeout).

**Hard vs soft timeout**: Soft timeout = stop after completing the current iteration. Hard timeout = stop mid-search (already implemented via `timed_out` flag).

- Ref: https://www.chessprogramming.org/Time_Management

## Debugging Search

### Common Symptoms and Causes

| Symptom | Likely cause |
|---------|-------------|
| Engine plays obviously bad moves | Move ordering bug, eval sign error, TT returning stale data |
| Score oscillates wildly between depths | Eval is not smooth, or search instability from TT interaction |
| Engine doesn't find shallow tactics | PV not propagated correctly, or TT cutoff on PV node |
| Engine slows down at depth N | Type instability, allocation hotspot, or TT thrashing |
| Different results with/without TT | TT replacement bug, hash collision, or bound type handling error |

### Debugging Techniques

**Dump the PV**: Print the full principal variation at each depth. Check it makes sense as a sequence of moves.

**Compare with/without TT**: Disable TT cutoffs and compare search results. If they differ, the TT logic has a bug.

**Perft before and after**: Any change to move generation or game state must pass perft. Run `scripts/perft_test.jl`.

**Single-position analysis**: Set up a position where you know the right move. Search at increasing depths and verify the engine finds it.

### References
- Search Instability: https://www.chessprogramming.org/Search_Instability
- Perft: https://www.chessprogramming.org/Perft

## Julia Performance Considerations

Relevant when implementing any of the above techniques:

- **Type stability**: Every function in the search/eval hot path must be type-stable. Use `@code_warntype` to verify. A single `Any` return kills performance.
- **Avoid allocations**: The search allocates zero per node (uses pre-allocated buffers and `@no_escape`). New code must maintain this. Use `@btime` to check.
- **`@inbounds`**: Use on array accesses in hot loops where bounds are guaranteed by construction.
- **StaticArrays**: `MVector` and `SVector` are stack-allocated. Don't accidentally convert to `Vector`.
- **Avoid closures capturing mutable state**: These allocate. Pass state as arguments.
- **Branch prediction**: Put the common case first in if/else chains.

See `profiling.instructions.md` for profiling workflows.
