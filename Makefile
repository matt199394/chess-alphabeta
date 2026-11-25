# CHESS 2026 - Commodore 64 (cc65)
#
#   make        -> scacchi64.prg
#   make clean

CC65     = cl65
C64FLAGS = -t c64 -Oirs -DUSE_ASM_MOVEGEN -Wl -D,__STACKSIZE__=0x1000

all: scacchi64.prg

scacchi64.prg: src/chess.c src/main_c64.c src/gen6502.s src/chess.h src/font_writer.h
	mkdir -p build
	$(CC65) $(C64FLAGS) -m build/c64.map -o $@ src/chess.c src/gen6502.s src/main_c64.c

clean:
	rm -f scacchi64.prg
	rm -rf build

.PHONY: all clean
