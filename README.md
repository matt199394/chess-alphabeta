# Alpha-Beta Pruning

A brief description of the alpha-beta search algorithm used by the
**CHESS 2026** chess engine (C89, ported to the Commodore 64 and PC).

## Why it exists

A chess engine picks a move by searching the game tree with **minimax**:
at each level it alternates between maximizing (the side to move) and
minimizing (the opponent). Plain minimax visits every leaf, which is far
too slow on a 1 MHz 6502.

**Alpha-beta pruning** is an optimization of minimax that skips branches which
can never affect the final decision. It returns **exactly the same move** as
plain minimax, but visits far fewer nodes (often an order of magnitude less).

## The two bounds

Two values are carried down the tree:

- **alpha** — the best score the maximizing side is already guaranteed.
- **beta** — the best score the minimizing side is already guaranteed.

At a maximizing node, if a move scores `>= beta`, the opponent would never
allow this position, so the remaining moves are pruned (a *beta cutoff*). At a
minimizing node the symmetric cutoff happens when a score is `<= alpha`.

## Negamax form

Negamax expresses both cases with one recursive call by flipping the sign of
the score:

```
function search(depth, alpha, beta):
    if depth == 0:
        return evaluate()

    best = -infinity
    for each move:
        make(move)
        score = -search(depth - 1, -beta, -alpha)
        unmake(move)

        if score > best:
            best = score
        if best > alpha:
            alpha = best
        if alpha >= beta:
            break              # beta cutoff: prune the rest

    return alpha
```

`alpha` is the value of the best line found so far; `beta` is the ceiling
imposed by the opponent's best alternative elsewhere in the tree.

## Why move ordering matters

Alpha-beta is most effective when good moves are tried first. The CHESS 2026
engine orders moves with:

- **MVV-LVA** — "most valuable victim, least valuable attacker" for captures
  (try winning the queen before winning a pawn).
- **Killer moves** — quiet moves that caused a cutoff at the same depth.
- **Iterative deepening** — search depth 1, 2, 3, ... and try the best move of
  the previous iteration first.

At the leaves it runs a **quiescence search** (captures/promotions only) so the
static evaluation is not taken while the position is still "noisy".

## Key facts

- Pruning never changes the chosen move, only the number of nodes visited.
- Best case visits about `2 * b^(d/2)` nodes instead of `b^d`; the worst case
  degenerates to plain minimax.
- On the 6502 the engine keeps `alpha`/`beta` in zero page/registers and uses
  16-bit scores; mate scores are offset by the ply so a faster mate is
  preferred over a slower one.
