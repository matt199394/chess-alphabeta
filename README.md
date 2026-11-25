# CHESS 2026 (Commodore 64)

A small chess game for the Commodore 64, written in portable C89 with an
optional 6502 assembly move generator. The engine (search, evaluation, move
generation) is shared between targets; this repository contains only the C64
build.

## Features

- 3×3-character board (24×24) with custom-drawn pieces in a relocated charset.
- Human vs. computer, 4 difficulty levels (time-limited).
- Input by keyboard (`e2e4`) or joystick (port 2, fallback port 1).
- SID sound effects: valid move, invalid move, check.
- English UI, title `CHESS 2026`.
- Incremental board redraw (only the affected squares after a move).

## Build

Requires the [cc65](https://cc65.github.io/) toolchain (`cl65`).

```
make            # -> scacchi64.prg
make clean
```

Run in VICE:

```
c1541 -format "chess,01" d64 chess.d64 -write scacchi64.prg chess
x64sc -autostart chess.d64
```

The build uses `-DUSE_ASM_MOVEGEN` (the `gen6502.s` move generator). The C
move generator remains available as a reference; the two paths must produce the
same legal moves.

## Memory organization

The game runs with the VIC-II in **bank 2** (`$8000-$BFFF`), selected by
`POKE($DD00, ... | $01)`. The layout is:

| Region | Address | Size | Notes |
|---|---|---|---|
| Program (code + RODATA + DATA) | `$0801` – `~$5ED9` | ~22 KB | BASIC stub, code, const tables (glyphs, font) |
| BSS (static variables) | `~$5ED9` – `~$83C9` | ~9.4 KB | engine arrays + UI state |
| Screen (video matrix) | `$8400` – `$87E7` | 1000 B | 40×25, `$D018` = `$13` |
| Charset (custom) | `$8800` – `$8FFF` | 2048 B | text font + piece glyphs |
| Color RAM | `$D800` – `$DBE7` | 1000 B | fixed hardware location |
| Software stack | `$C000` – `$CFFF` | 4096 B | `__HIMEM__` = `$D000`, `__STACKSIZE__` = `$1000` |

**The critical constraint:** in VIC bank 2 the VIC sees RAM only in
`$8000-$8FFF`. `$9000-$9FFF` is the character ROM and `$A000-$BFFF` is the
BASIC ROM, so a charset placed at `$9000` is *ignored* (the VIC reads the ROM,
not your RAM). Therefore:

- screen and charset **must** live in `$8000-$8FFF`;
- with screen at `$8400` and charset at `$8800`, the BSS **must stay below
  `$8400`**, or the static variables overwrite the screen.

The BSS is placed right after the code, so **every added function/variable
pushes the BSS up**. After any change, check `build/c64.map`: the `BSS` segment
must end below `$8400`.

The charset stores the piece glyphs **inverted** (0 = piece pixel, 1 = square
background). Pieces occupy characters 64–123: solid black pieces from 64,
white outline pieces from 94. The text font (characters 0–63) comes from the
"1-writer" font in `src/font_writer.h`.

## Alpha-beta pruning

The engine chooses a move with a **negamax alpha-beta** search. It alternates
between maximizing (side to move) and minimizing (opponent), carrying two
bounds down the tree:

- **alpha** — best score the maximizing side is already guaranteed;
- **beta** — best score the minimizing side is already guaranteed.

A *beta cutoff* (`score >= beta`) prunes the remaining moves at a node, because
the opponent would never allow this position. The search returns exactly the
same move as plain minimax, but visits far fewer nodes.

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
            break              # beta cutoff

    return alpha
```

Alpha-beta is most effective when good moves are tried first. The engine orders
moves with:

- **MVV-LVA** — most valuable victim, least valuable attacker, for captures;
- **killer moves** — quiet moves that caused a cutoff at the same depth;
- **iterative deepening** — the best move of the previous depth is tried first.

At the leaves it runs a **quiescence search** (captures/promotions only) so the
static evaluation is not taken while the position is still "noisy".

- Pruning never changes the chosen move, only the node count.
- Best case visits about `2 * b^(d/2)` nodes instead of `b^d`.
- On the 6502, `alpha`/`beta` are 16-bit; mate scores are offset by the ply so
  a faster mate is preferred over a slower one.
