/* UI C64 - scacchiera 3x3 con pezzi disegnati (charset custom in banca VIC 2).
   Schermo a $8400, charset a $8800; il pannello dei testi sta a sinistra.
   In banca 2 il VIC vede RAM solo in $8000-$8FFF ($9000-$9FFF = char ROM,
   $A000-$BFFF = BASIC ROM): schermo+charset devono stare lì e il BSS deve
   restare sotto $8400 (controllare build/c64.map dopo ogni modifica). */
#include <conio.h>      /* cgetc, kbhit */
#include <c64.h>
#include <peekpoke.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "chess.h"
#include "font_writer.h"

static u8 flip;
static u8 level;
static u8 h1, h2;                 /* ultima mossa (0xFF = nessuna) */
static u8 depth;
static u32 nodeLimit;
static u8 joyCursor;              /* casa 0x88 del cursore joystick */
static u8 joyFrom;                /* casa selezionata (0xFF = nessuna) */
static char aiMove[8];            /* ultima mossa del computer (e2-e4) */
static u8 hasAiMove;              /* 1 = c'e' una mossa del computer da mostrare */
static s16 aiScore;
static u32 aiTicks;
static u8  emptyBoard;           /* 1 = disegna la scacchiera senza pezzi (menu) */

/* layout: schermo in banca VIC 2, scacchiera 3x3 caratteri per casa */
#define SCR_BASE   0x8400         /* memoria schermo (il BSS deve restare sotto) */
#define COL_BASE   0xD800         /* Color RAM (fissa) */
#define CHR_BASE   0x8800         /* charset custom */

#define BOARD_X    14             /* colonna prima casa */
#define BOARD_Y    1              /* riga prima casa */
#define FILE_ROW   0              /* etichette colonne in alto */
#define RANK_COL_L 13             /* etichetta traversa sinistra */
#define RANK_COL_R 38             /* etichetta traversa destra */

#define MOVE_ROW   2              /* pannello: mossa del computer */
#define NODI_ROW   4
#define VAL_ROW    5
#define DEPTH_ROW  6
#define CHECK_ROW  8
#define PROMPT_ROW 20
#define MSG_ROW    21
#define OVER_ROW   23
#define OVER_KEY_ROW 24
#define PANEL_W    13             /* larghezza del pannello sinistro (colonne 0..12) */

/* ------------------------------------------------------------------ */
/* glyph dei pezzi (3x3, 5 celle "a croce"; gli angoli usano lo spazio) */
/* ordine celle: top, left, center, right, bottom (8 byte ciascuna)    */
/* ------------------------------------------------------------------ */

/* pezzi ridisegnati da superscacchi.png (16x24 -> cross 24x24, ordine celle top,left,center,right,bottom) */
static const u8 glyph_solid[6][5][8] = {
    /* pawn */ {
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x1C, 0x3E, 0x67 }, /* top */
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* left */
        { 0x5F, 0x5F, 0x7F, 0x3E, 0x1C, 0x1C, 0x1C, 0x1C }, /* center */
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* right */
        { 0x1C, 0x3E, 0x3E, 0x7F, 0x55, 0x7F, 0x00, 0x00 }, /* bottom */
    },
    /* knight */ {
        { 0x00, 0x00, 0x00, 0x18, 0x1C, 0x7A, 0xFD, 0xBD }, /* top */
        { 0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* left */
        { 0xFE, 0xFE, 0x3F, 0x7F, 0xF7, 0xF7, 0x76, 0x26 }, /* center */
        { 0x80, 0x80, 0x40, 0x40, 0x80, 0x80, 0x00, 0x00 }, /* right */
        { 0x1E, 0x1C, 0x3C, 0x7E, 0xDB, 0xFF, 0x00, 0x00 }, /* bottom */
    },
    /* bishop */ {
        { 0x00, 0x00, 0x00, 0x00, 0x3C, 0x5E, 0xBF, 0xBF }, /* top */
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* left */
        { 0xFF, 0x7E, 0x3C, 0x3C, 0x7E, 0xFF, 0x7E, 0x3C }, /* center */
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* right */
        { 0x3C, 0x3C, 0x7E, 0xFF, 0xA5, 0xFF, 0x00, 0x00 }, /* bottom */
    },
    /* rook */ {
        { 0x00, 0x00, 0x00, 0x00, 0xB6, 0xB6, 0x7F, 0x7F }, /* top */
        { 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* left */
        { 0x7F, 0xAA, 0xBF, 0x5E, 0x5E, 0x5E, 0x5E, 0x5E }, /* center */
        { 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* right */
        { 0x5E, 0x5E, 0xDF, 0xAA, 0x7F, 0xFF, 0x00, 0x00 }, /* bottom */
    },
    /* queen */ {
        { 0x00, 0x00, 0x00, 0x00, 0x55, 0x55, 0xFF, 0xAB }, /* top */
        { 0x03, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00 }, /* left */
        { 0xBF, 0xDF, 0x5E, 0x5E, 0xDF, 0xBF, 0xDF, 0x5E }, /* center */
        { 0xC0, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00 }, /* right */
        { 0x5E, 0xDF, 0xBF, 0xBF, 0x81, 0xFF, 0x00, 0x00 }, /* bottom */
    },
    /* king */ {
        { 0x00, 0x00, 0x00, 0x18, 0x18, 0x7E, 0x7E, 0x18 }, /* top */
        { 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00 }, /* left */
        { 0x18, 0x3C, 0xFF, 0xDB, 0xE7, 0x7E, 0x3C, 0xFF }, /* center */
        { 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00 }, /* right */
        { 0x3C, 0x7E, 0x7E, 0xFF, 0xC3, 0xFF, 0x00, 0x00 }, /* bottom */
    },
};

static const u8 glyph_outline[6][5][8] = {
    /* pawn */ {
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x1C, 0x22, 0x49 }, /* top */
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* left */
        { 0x51, 0x41, 0x41, 0x22, 0x14, 0x14, 0x14, 0x14 }, /* center */
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* right */
        { 0x22, 0x22, 0x41, 0x55, 0x41, 0x7F, 0x00, 0x00 }, /* bottom */
    },
    /* knight */ {
        { 0x00, 0x00, 0x00, 0x18, 0x14, 0x62, 0x83, 0xA0 }, /* top */
        { 0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* left */
        { 0x00, 0xC8, 0x28, 0x48, 0x88, 0x99, 0x42, 0x32 }, /* center */
        { 0x80, 0x80, 0x40, 0x40, 0x80, 0x80, 0x00, 0x00 }, /* right */
        { 0x12, 0x12, 0x32, 0x42, 0x81, 0xFF, 0x00, 0x00 }, /* bottom */
    },
    /* bishop */ {
        { 0x00, 0x00, 0x00, 0x00, 0x3C, 0x52, 0xA1, 0xA1 }, /* top */
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* left */
        { 0xC1, 0x42, 0x24, 0x24, 0x42, 0x81, 0x42, 0x24 }, /* center */
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* right */
        { 0x24, 0x24, 0x42, 0xDB, 0x81, 0xFF, 0x00, 0x00 }, /* bottom */
    },
    /* rook */ {
        { 0x00, 0x00, 0x00, 0x00, 0xB6, 0xB6, 0x49, 0x40 }, /* top */
        { 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* left */
        { 0x55, 0xAA, 0xB5, 0x52, 0x52, 0x52, 0x52, 0x52 }, /* center */
        { 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* right */
        { 0x52, 0x52, 0xD5, 0xAA, 0x55, 0xFF, 0x00, 0x00 }, /* bottom */
    },
    /* queen */ {
        { 0x00, 0x00, 0x00, 0x00, 0x55, 0xAA, 0x81, 0x81 }, /* top */
        { 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00 }, /* left */
        { 0x40, 0xA1, 0x52, 0x52, 0x91, 0x20, 0x91, 0x52 }, /* center */
        { 0x80, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00 }, /* right */
        { 0x52, 0x91, 0x91, 0xA1, 0x81, 0xFF, 0x00, 0x00 }, /* bottom */
    },
    /* king */ {
        { 0x00, 0x00, 0x00, 0x18, 0x18, 0x66, 0x66, 0x18 }, /* top */
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* left */
        { 0x18, 0x24, 0x42, 0x81, 0x42, 0x24, 0x24, 0xC3 }, /* center */
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* right */
        { 0x24, 0x42, 0x42, 0x81, 0x81, 0xFF, 0x00, 0x00 }, /* bottom */
    },
};
/* ------------------------------------------------------------------ */
/* output testi (POKE diretto: conio scrive sempre a $0400)            */
/* ------------------------------------------------------------------ */

static u8 to_screen(char c)
{
    if (c >= 'A' && c <= 'Z') return (u8)(c - 'A' + 1);
    if (c >= 'a' && c <= 'z') return (u8)(c - 'a' + 1);
    if (c >= '0' && c <= '9') return (u8)(c - '0' + 0x30);
    switch (c) {
        case ' ': return 0x20;
        case ':': return 0x3A;
        case '-': return 0x2D;
        case '.': return 0x2E;
        case ',': return 0x2C;
        case '/': return 0x2F;
        case '!': return 0x21;
        case '(': return 0x28;
        case ')': return 0x29;
        case '?': return 0x3F;
        default:  return 0x20;
    }
}

static void scr_put(u8 x, u8 y, u8 code, u8 color)
{
    u16 o = (u16)(y * 40 + x);
    POKE((u16)(SCR_BASE + o), code);
    POKE((u16)(COL_BASE + o), color);
}

static void scr_puts(u8 x, u8 y, const char *s, u8 color)
{
    while (*s) {
        scr_put(x, y, to_screen(*s), color);
        x++;
        s++;
    }
}

static void scr_unum(u8 x, u8 y, unsigned int v, u8 color)
{
    char b[8];
    utoa(v, b, 10);
    scr_puts(x, y, b, color);
}

static void scr_snum(u8 x, u8 y, s16 v, u8 color)
{
    char b[8];
    itoa(v, b, 10);
    scr_puts(x, y, b, color);
}

static void scr_blank(u8 x, u8 y, u8 n, u8 color)
{
    while (n--) scr_put(x++, y, 0x20, color);
}

/* effetto sonoro SID: f = frequency word, w = forma d'onda (senza gate) */
static void sfx(u8 f, u8 w)
{
    POKE(0xD418, 0x0F);            /* volume massimo */
    POKE(0xD405, 0x00);            /* attacco immediato (evita attacco lento) */
    POKE(0xD406, 0xF0);            /* sustain 15, release 0 */
    POKE(0xD402, 0x00);            /* pulse width 50% (onda quadra squillante) */
    POKE(0xD403, 0x08);
    POKE(0xD400, f);
    POKE(0xD401, 0x00);
    POKE(0xD404, (u8)(w | 0x01));  /* gate on */
    {
        u32 t = (u32)clock() + 12;
        while ((u32)clock() < t) {}
    }
    POKE(0xD404, w);               /* gate off */
}

static void scr_clear(void)
{
    u16 i;
    for (i = 0; i < 1000; i++) {
        POKE((u16)(SCR_BASE + i), 0x20);
        POKE((u16)(COL_BASE + i), COLOR_WHITE);
    }
}

/* ------------------------------------------------------------------ */
/* charset custom                                                     */
/* ------------------------------------------------------------------ */

static void install_charset(void)
{
    u8 t, cell, i;
    u16 j;

    /* azzera tutto il charset */
    for (j = 0; j < 2048; j++) {
        POKE((u16)(CHR_BASE + j), 0x00);
    }

    /* font "1-writer": solo i glyph 0x00-0x3F (testo: A-Z, cifre, punteggiatura) */
    for (j = 0; j < 512; j++) {
        POKE((u16)(CHR_BASE + j), font_writer[j]);
    }

    /* blocco solido (reverse-space) al glyph 0xA0, usato per le case vuote */
    for (j = 0; j < 8; j++) {
        POKE((u16)(CHR_BASE + 0xA0 * 8 + j), 0xFF);
    }

    /* pezzi: solid (neri) da glyph 64, outline (bianchi) da glyph 94.
       I glyph sono memorizzati INVERTITI (0 = pezzo, 1 = sfondo casa):
       cosi' il pezzo appare nero (sfondo globale) sulla casa colorata. */
    for (t = 0; t < 6; t++) {
        for (cell = 0; cell < 5; cell++) {
            u16 a = (u16)(CHR_BASE + (64 + t * 5 + cell) * 8);
            u16 b = (u16)(CHR_BASE + (94 + t * 5 + cell) * 8);
            for (i = 0; i < 8; i++) {
                POKE((u16)(a + i), (u8)(0xFF ^ glyph_solid[t][cell][i]));
                POKE((u16)(b + i), (u8)(0xFF ^ glyph_outline[t][cell][i]));
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* scacchiera                                                         */
/* ------------------------------------------------------------------ */

/* codice schermo della cella (cr,cc) della casa 3x3; 0xA0 = blocco solido */
static u8 piece_code(u8 p, u8 cr, u8 cc)
{
    u8 t, base, cell;

    if (!p) return 0xA0;                     /* casa vuota: tutto solido */

    t = (u8)(PT(p) - 1);
    base = (PC(p) == BLACK) ? 64 : 94;

    if (cr == 0 && cc == 1) cell = 0;        /* top */
    else if (cr == 1 && cc == 0) cell = 1;   /* left */
    else if (cr == 1 && cc == 1) cell = 2;   /* center */
    else if (cr == 1 && cc == 2) cell = 3;   /* right */
    else if (cr == 2 && cc == 1) cell = 4;   /* bottom */
    else return 0xA0;                        /* angolo: blocco solido */

    return (u8)(base + t * 5 + cell);
}

/* colore della casa: chiaro/scuro a scacchiera; giallo = selezione o
   destinazione dell'ultima mossa, cyan = cursore joystick */
static u8 square_color(u8 sq)
{
    u8 rank = (u8)(sq >> 4);
    u8 file = (u8)(sq & 7);

    if (sq == h2 || sq == joyFrom) return COLOR_YELLOW;
    if (sq == joyCursor) return COLOR_CYAN;
    /* a1 scura: (traversa+colonna) pari -> scura, dispari -> chiara */
    return ((rank + file) & 1) ? COLOR_GRAY3 : COLOR_BLUE;
}

/* disegna una casa 3x3: sfondo colorato (reverse) e pezzo "scavato" */
static void draw_square(u8 sq)
{
    u8 r, f, x, y, cr, cc, p, col, code;
    u16 sbase;

    p = board[sq];
    if (emptyBoard) p = 0;
    col = square_color(sq);
    r = flip ? (u8)(sq >> 4) : (u8)(7 - (sq >> 4));
    f = flip ? (u8)(7 - (sq & 7)) : (u8)(sq & 7);
    x = (u8)(BOARD_X + 3 * f);
    y = (u8)(BOARD_Y + 3 * r);
    sbase = (u16)(SCR_BASE + y * 40 + x);

    for (cr = 0; cr < 3; cr++) {
        for (cc = 0; cc < 3; cc++) {
            code = piece_code(p, cr, cc);
            POKE((u16)(sbase + cr * 40 + cc), code);
            POKE((u16)(sbase - SCR_BASE + COL_BASE + cr * 40 + cc), col);
        }
    }
}

static void draw_board(void)
{
    u8 r, f, rank, file, sq;

    scr_clear();
    scr_puts(0, 0, "CHESS 2026", COLOR_WHITE);

    /* etichette colonne in alto */
    for (f = 0; f < 8; f++) {
        scr_put((u8)(BOARD_X + 3 * f + 1), FILE_ROW,
                to_screen((char)('a' + (flip ? (7 - f) : f))), COLOR_WHITE);
    }

    for (r = 0; r < 8; r++) {
        rank = flip ? r : (u8)(7 - r);
        scr_put(RANK_COL_R, (u8)(BOARD_Y + 3 * r + 1),
                to_screen((char)('1' + rank)), COLOR_WHITE);
        for (f = 0; f < 8; f++) {
            file = flip ? (u8)(7 - f) : f;
            sq = (u8)((rank << 4) | file);
            draw_square(sq);
        }
    }
}

/* ridisegna la casa sq con l'evidenziazione corrente */
static void paint_sq(u8 sq)
{
    if (sq == 0xFF) return;
    draw_square(sq);
}

/* applica una mossa e ridisegna solo le case toccate (niente draw_board).
   Arrocco ed en passant muovono piu' di un pezzo: lì si ridisegna tutto. */
static void apply_move(u16 m)
{
    u8 from = M_FROM(m);
    u8 to = M_TO(m);
    u8 t = PT(board[from]);
    u8 oh2 = h2;
    u8 complex = (t == KING && ((u8)(to-from) == 2 || (u8)(from-to) == 2)) ||
                 (t == PAWN && !board[to] && (u8)(to & 7) != (u8)(from & 7));

    commit_move(m);
    h1 = from;
    h2 = to;
    joyFrom = 0xFF;
    joyCursor = 0xFF;

    if (complex) {
        draw_board();
        return;
    }
    if (oh2 != 0xFF) draw_square(oh2);
    draw_square(from);
    draw_square(to);
}

static void show_status(void)
{
    scr_puts(0, MOVE_ROW, "AI:", COLOR_WHITE);
    scr_puts(3, MOVE_ROW, aiMove, COLOR_YELLOW);

    scr_puts(0, NODI_ROW, "nodes", COLOR_WHITE);
    scr_unum(6, NODI_ROW, (unsigned int)(nodes / 1000UL), COLOR_WHITE);
    scr_puts(9, NODI_ROW, "k", COLOR_WHITE);

    scr_puts(0, VAL_ROW, "val", COLOR_WHITE);
    scr_snum(4, VAL_ROW, aiScore, COLOR_WHITE);

    scr_puts(0, DEPTH_ROW, "d", COLOR_WHITE);
    scr_unum(1, DEPTH_ROW, (unsigned int)thinkDepth, COLOR_WHITE);
    scr_puts(3, DEPTH_ROW, "t", COLOR_WHITE);
    scr_unum(4, DEPTH_ROW, (unsigned int)(aiTicks / 60UL), COLOR_WHITE);
    scr_puts(7, DEPTH_ROW, "s", COLOR_WHITE);
}

/* ------------------------------------------------------------------ */
/* joystick                                                            */
/* ------------------------------------------------------------------ */

#define JB_UP    0x01
#define JB_DOWN  0x02
#define JB_LEFT  0x04
#define JB_RIGHT 0x08
#define JB_FIRE  0x10

static u8 joy_read_raw(void)
{
    u8 p2 = (u8)((PEEK(0xDC00) ^ 0x1F) & 0x1F);
    u8 p1 = (u8)((PEEK(0xDC01) ^ 0x1F) & 0x1F);
    return p2 ? p2 : p1;   /* 1 = premuto */
}

static u8 joy_poll(void)
{
    static u8 lastRaw = 0xFF;
    static u8 stable = 0;
    static u8 prev = 0;
    static u8 cnt = 0;
    static u32 lastTick = 0;
    u32 now = (u32)clock();
    u8 raw;

    if (now == lastTick) return 0;
    lastTick = now;

    raw = joy_read_raw();
    if (raw == lastRaw) {
        if (cnt < 2) {
            cnt++;
            if (cnt == 2) stable = raw;
        }
    } else {
        lastRaw = raw;
        cnt = 0;
    }

    raw = (u8)(stable & (stable ^ prev));
    prev = stable;
    return raw;
}

static u16 move_from_squares(u8 from, u8 to)
{
    char s[6];

    s[0] = (char)('a' + (from & 7));
    s[1] = (char)('1' + (from >> 4));
    s[2] = (char)('a' + (to & 7));
    s[3] = (char)('1' + (to >> 4));
    s[4] = 'q';
    s[5] = 0;
    return parse_move(s);
}

/* input della mossa umana: tastiera (e2e4) oppure joystick (cursore).
   Ritorna la mossa legale scelta, oppure MOVE_NONE per uscire ('.'/STOP). */
static u16 get_human_move(void)
{
    char kb[16];
    u8 klen = 0;
    u8 r, f, sq, e;

    scr_blank(0, PROMPT_ROW, PANEL_W, COLOR_BLACK);
    scr_blank(0, MSG_ROW, PANEL_W, COLOR_BLACK);
    scr_puts(0, PROMPT_ROW, "move:", COLOR_WHITE);

    joyCursor = kingSq[side];
    joyFrom = 0xFF;
    paint_sq(joyCursor);

    for (;;) {
        if (kbhit()) {
            int c = cgetc();
            if (c == 13 || c == 10) {
                kb[klen] = 0;
                if (klen == 0) continue;
                {
                    u16 mv = parse_move(kb);
                    if (mv != MOVE_NONE) return mv;
                }
                scr_puts(0, MSG_ROW, "bad move", COLOR_LIGHTRED);
                sfx(228, 0x20);
                klen = 0;
                continue;
            }
            if (c == 20 || c == 8) {
                if (klen) {
                    klen--;
                    scr_put((u8)(6 + klen), PROMPT_ROW, 0x20, COLOR_WHITE);
                }
                continue;
            }
            if (c == 3 || c == '.') return MOVE_NONE;
            if (klen < 5) {
                scr_put((u8)(6 + klen), PROMPT_ROW, to_screen((char)c), COLOR_WHITE);
                kb[klen++] = (char)c;
            }
        } else {
            e = joy_poll();
            if (!e) continue;

            r = (u8)(joyCursor >> 4);
            f = (u8)(joyCursor & 7);
            if ((e & JB_UP)    && (flip ? r > 0 : r < 7)) r = (u8)(r + (flip ? -1 : 1));
            if ((e & JB_DOWN)  && (flip ? r < 7 : r > 0)) r = (u8)(r + (flip ? 1 : -1));
            if ((e & JB_LEFT)  && (flip ? f < 7 : f > 0)) f = (u8)(f + (flip ? 1 : -1));
            if ((e & JB_RIGHT) && (flip ? f > 0 : f < 7)) f = (u8)(f + (flip ? -1 : 1));
            sq = (u8)((r << 4) | f);

            if (sq != joyCursor) {
                u8 old = joyCursor;
                joyCursor = sq;
                paint_sq(old);
                paint_sq(joyCursor);
            }

            if (e & JB_FIRE) {
                if (joyFrom == 0xFF) {
                    if (board[joyCursor] && PC(board[joyCursor]) == side) {
                        joyFrom = joyCursor;
                        paint_sq(joyFrom);
                    } else {
                        scr_puts(0, MSG_ROW, "not yours", COLOR_LIGHTRED);
                        sfx(228, 0x20);
                    }
                } else {
                    u16 mv = move_from_squares(joyFrom, joyCursor);
                    if (mv != MOVE_NONE) return mv;
                    scr_puts(0, MSG_ROW, "bad move", COLOR_LIGHTRED);
                    sfx(228, 0x20);
                    {
                        u8 oldFrom = joyFrom;
                        joyFrom = 0xFF;
                        if (board[joyCursor] && PC(board[joyCursor]) == side)
                            joyFrom = joyCursor;
                        if (oldFrom != joyCursor) paint_sq(oldFrom);
                        paint_sq(joyCursor);
                    }
                }
            }
        }
    }
}

static u8 ask_level(void)
{
    int c;

    scr_puts(0, 2, "LEVEL", COLOR_WHITE);
    scr_puts(0, 4, "1 EASY", COLOR_WHITE);
    scr_puts(0, 5, "2 MEDIUM", COLOR_WHITE);
    scr_puts(0, 6, "3 HARD", COLOR_WHITE);
    scr_puts(0, 7, "4 STRONG", COLOR_WHITE);
    scr_puts(0, 9, "CHOICE:", COLOR_WHITE);
    do { c = cgetc(); } while (c < '1' || c > '4');
    scr_put(7, 9, to_screen((char)c), COLOR_WHITE);
    return (u8)(c - '1');
}

static u8 ask_color(void)
{
    int c;

    scr_puts(0, 11, "COLOR:", COLOR_WHITE);
    scr_puts(0, 12, "1 WHITE", COLOR_WHITE);
    scr_puts(0, 13, "2 BLACK", COLOR_WHITE);
    do { c = cgetc(); } while (c < '1' || c > '2');
    scr_put(7, 11, to_screen((char)c), COLOR_WHITE);
    return (u8)(c - '1');
}

static void game_over(const char *msg)
{
    u8 i;

    for (i = 0; i < PANEL_W; i++) {
        scr_put(i, OVER_ROW, 0x20, COLOR_BLACK);
        scr_put(i, OVER_KEY_ROW, 0x20, COLOR_BLACK);
    }
    scr_puts(0, OVER_ROW, msg, COLOR_YELLOW);
    scr_puts(0, OVER_KEY_ROW, "press a key", COLOR_WHITE);
    cgetc();
}

static void restart_game(void)
{
    new_game();
    h1 = h2 = 0xFF;
    hasAiMove = 0;
    draw_board();
}

int main(void)
{
    u8 human;
    u16 m;
    s16 score;
    u32 t0, t1;
    u8 running = 1;

    h1 = h2 = 0xFF;
    joyCursor = 0xFF;
    joyFrom = 0xFF;
    hasAiMove = 0;

    POKE(0xD020, COLOR_BLACK);               /* bordo */
    POKE(0xD021, COLOR_BLACK);               /* sfondo */

    install_charset();
    POKE(0xDD00, (u8)((PEEK(0xDD00) & 0xFC) | 0x01));  /* banca VIC 2 */
    POKE(0xD018, 0x13);                      /* schermo $8400, charset $8800 */

    init_engine();
    new_game();

    emptyBoard = 1;
    draw_board();
    emptyBoard = 0;

    level = ask_level();
    human = ask_color();
    human = (human == 0) ? WHITE : BLACK;
    flip = (human == BLACK) ? 1 : 0;

    switch (level) {
        case 0:  depth = 1; nodeLimit = 0; quiesceMax = 0; thinkSeconds = 0; break;
        case 1:  depth = 3; nodeLimit = 0; quiesceMax = 2; thinkSeconds = 5; break;
        case 2:  depth = 4; nodeLimit = 0; quiesceMax = 4; thinkSeconds = 20; break;
        default: depth = 5; nodeLimit = 0; quiesceMax = 4; thinkSeconds = 60; break;
    }

    draw_board();

    while (running) {
        joyCursor = 0xFF;
        joyFrom = 0xFF;

        if (!has_legal_moves()) {
            if (in_check())
                game_over(side == WHITE ? "BLACK WINS" : "WHITE WINS");
            else
                game_over("STALEMATE");
            restart_game();
            continue;
        }
        if (fifty_moves()) { game_over("50 MOVE DRAW"); restart_game(); continue; }
        if (repetition_count() >= 2) { game_over("REPETITION"); restart_game(); continue; }
        if (insufficient_material()) { game_over("MATERIAL"); restart_game(); continue; }

        if (in_check()) {
            scr_puts(0, CHECK_ROW, "CHECK!", COLOR_YELLOW);
            sfx(47, 0x40);
        } else
            scr_blank(0, CHECK_ROW, 8, COLOR_BLACK);

        if (side == human) {
            if (hasAiMove) show_status();
            m = get_human_move();
            if (m == MOVE_NONE) break;
            apply_move(m);
            sfx(93, 0x40);
        } else {
            scr_puts(0, PROMPT_ROW, "thinking...", COLOR_WHITE);
            t0 = (u32)clock();
            m = think(depth, nodeLimit, &score);
            t1 = (u32)clock();
            if (m == MOVE_NONE) continue;
            apply_move(m);
            sfx(93, 0x40);
            aiMove[0] = (char)('a' + (h1 & 7));
            aiMove[1] = (char)('1' + (h1 >> 4));
            aiMove[2] = '-';
            aiMove[3] = (char)('a' + (h2 & 7));
            aiMove[4] = (char)('1' + (h2 >> 4));
            aiMove[5] = 0;
            aiScore = score;
            aiTicks = (u32)(t1 - t0);
            hasAiMove = 1;
        }
    }

    return 0;
}
