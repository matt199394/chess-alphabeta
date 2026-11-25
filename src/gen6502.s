;
; gen6502.s - generazione delle mosse in assembly 6502 per cc65
;
;   u8 __fastcall__ gen_moves6502 (u16 *list)      ; capsOnly in asm_caps
;
; Genera le mosse di pedoni, cavalli, alfieri, torri, donne e re; l'arrocco
; resta nel C (li' serve attacked()). La lista e' quella globale di chess.c:
; mosse da 16 bit (from | to<<7 | promo<<14).
;
; Convenzioni cc65:
;   - unico argomento (list) in A = low, X = high
;   - capsOnly arriva dalla variabile globale asm_caps (evita di dover
;     leggere lo stack software)
;   - ptr1 e' uno dei puntatori in zero page forniti dal runtime
;
; Tutto sta in un unico .proc cosi' le etichette locali (@) sono condivise.
;
        .include "zeropage.inc"

        .export _gen_moves6502
        .import _board, _side, _epSq, _asm_caps
        .importzp ptr1, tmp1

MAXMOVES = 120                  ; deve coincidere con chess.h (__CC65__)

        .segment "BSS"
m_n:     .res 1                 ; numero di mosse generate
m_sq:    .res 1                 ; casa in esame (0x88)
m_to:    .res 1                 ; casa d'arrivo
m_promo: .res 1                 ; 0 = no, 1 = B, 2 = R, 3 = Q
m_caps:  .res 1                 ; solo catture
m_side8: .res 1                 ; side << 3
m_off:   .res 1                 ; offset di direzione (con segno)
m_i:     .res 1                 ; indice dei cicli cavallo/re (X viene usato
                                ; dalle subroutine, quindi non e' salvabile)

        .segment "CODE"

.proc _gen_moves6502: near

        sta     ptr1            ; list low
        stx     ptr1+1          ; list high
        lda     _asm_caps
        sta     m_caps

        lda     _side
        asl     a
        asl     a
        asl     a
        sta     m_side8

        lda     #0
        sta     m_n
        sta     m_sq

@scan:
        ldx     m_sq
        lda     _board,x        ; le case 0x88 non valide sono sempre vuote
        beq     @next
        and     #8
        cmp     m_side8
        bne     @next
        ldx     m_sq
        lda     _board,x
        and     #7
        cmp     #1
        beq     @do_pawn
        cmp     #2
        beq     @do_knight
        cmp     #3
        beq     @do_bishop
        cmp     #4
        beq     @do_rook
        cmp     #5
        beq     @do_queen
        jsr     @gen_king        ; type 6
        jmp     @next
@do_pawn:
        jsr     @gen_pawn
        jmp     @next
@do_knight:
        jsr     @gen_knight
        jmp     @next
@do_bishop:
        jsr     @gen_bishop
        jmp     @next
@do_rook:
        jsr     @gen_rook
        jmp     @next
@do_queen:
        jsr     @gen_queen
@next:
        inc     m_sq
        lda     m_sq
        cmp     #120
        bcc     @scan

        lda     m_n
        rts

; ------------------------------------------------------------------
; aggiunge la mossa (m_sq -> m_to) con il tipo di promozione in m_promo
; ------------------------------------------------------------------
@add_mv:
        lda     m_n
        cmp     #MAXMOVES
        bcs     @add_full
        asl     a
        tay
        lda     m_to
        lsr     a               ; byte alto di to<<7
        sta     tmp1
        lda     m_to
        and     #1
        beq     @nobit
        lda     #$80
@nobit:
        ora     m_sq            ; from | (to&1)<<7
        sta     (ptr1),y
        iny
        lda     m_promo
        beq     @nopromo
        asl     a
        asl     a
        asl     a
        asl     a
        asl     a
        asl     a               ; promo<<14, byte alto
@nopromo:
        ora     tmp1
        sta     (ptr1),y
        inc     m_n
@add_full:
        rts

@add_quiet:
        lda     #0
        sta     m_promo
        jmp     @add_mv

@add_promos4:
        lda     #3
        sta     m_promo
        jsr     @add_mv
        lda     #2
        sta     m_promo
        jsr     @add_mv
        lda     #1
        sta     m_promo
        jsr     @add_mv
        lda     #0
        sta     m_promo
        jmp     @add_mv

; ------------------------------------------------------------------
; casa d'arrivo generica: vuota (se non solo catture) o nemica
; ------------------------------------------------------------------
@try_target:
        ldx     m_to
        lda     _board,x
        bne     @occupied
        lda     m_caps
        bne     @try_ret
        jmp     @add_quiet
@occupied:
        and     #8
        cmp     m_side8
        beq     @try_ret
        jmp     @add_quiet
@try_ret:
        rts

; ------------------------------------------------------------------
; raggio (alfiere/torre/donna) con offset in m_off
; ------------------------------------------------------------------
@do_ray:
        lda     m_sq
        clc
        adc     m_off
        sta     m_to
@ray_loop:
        lda     m_to
        and     #$88
        bne     @ray_end
        ldx     m_to
        lda     _board,x
        bne     @ray_hit
        lda     m_caps
        bne     @ray_next
        jsr     @add_quiet
@ray_next:
        lda     m_to
        clc
        adc     m_off
        sta     m_to
        jmp     @ray_loop
@ray_hit:
        and     #8
        cmp     m_side8
        beq     @ray_end
        jsr     @add_quiet
@ray_end:
        rts

; ------------------------------------------------------------------
; pezzi
; ------------------------------------------------------------------
@gen_knight:
        lda     #0
        sta     m_i
@kn_loop:
        ldx     m_i
        lda     m_sq
        clc
        adc     @koff,x
        sta     m_to
        and     #$88
        bne     @kn_skip
        jsr     @try_target
@kn_skip:
        inc     m_i
        lda     m_i
        cmp     #8
        bcc     @kn_loop
        rts
@koff:   .byte   33, 31, 18, 14, 223, 225, 238, 242   ; -33,-31,-18,-14

@gen_king:
        lda     #0
        sta     m_i
@kg_loop:
        ldx     m_i
        lda     m_sq
        clc
        adc     @goff,x
        sta     m_to
        and     #$88
        bne     @kg_skip
        jsr     @try_target
@kg_skip:
        inc     m_i
        lda     m_i
        cmp     #8
        bcc     @kg_loop
        rts
@goff:   .byte   17, 16, 15, 1, 255, 241, 240, 239      ; -1,-15,-16,-17

@gen_bishop:
        lda     #17
        jsr     @ray
        lda     #15
        jsr     @ray
        lda     #<-15
        jsr     @ray
        lda     #<-17
        jmp     @ray
@gen_rook:
        lda     #16
        jsr     @ray
        lda     #<-16
        jsr     @ray
        lda     #1
        jsr     @ray
        lda     #<-1
        jmp     @ray
@gen_queen:
        lda     #17
        jsr     @ray
        lda     #16
        jsr     @ray
        lda     #15
        jsr     @ray
        lda     #1
        jsr     @ray
        lda     #<-1
        jsr     @ray
        lda     #<-15
        jsr     @ray
        lda     #<-16
        jsr     @ray
        lda     #<-17
@ray:
        sta     m_off
        jmp     @do_ray

; ------------------------------------------------------------------
; pedoni
; ------------------------------------------------------------------
@gen_pawn:
        lda     _side
        beq     @white_pawn
        jmp     @black
@white_pawn:

; ---- bianco: spinta +16 ----
        lda     m_sq
        clc
        adc     #16
        sta     m_to
        and     #$88
        bne     @w_caps
        ldx     m_to
        lda     _board,x
        bne     @w_caps
        lda     m_sq
        and     #$F0
        cmp     #$60
        beq     @w_promo_push
        jsr     @add_quiet
        lda     m_sq
        cmp     #16               ; doppia spinta dalla traversa 2
        bcc     @w_caps
        cmp     #24
        bcs     @w_caps
        lda     m_sq
        clc
        adc     #32
        sta     m_to
        ldx     m_to
        lda     _board,x
        bne     @w_caps
        jsr     @add_quiet
@w_caps:
        lda     m_sq
        clc
        adc     #15
        sta     m_to
        and     #$88
        bne     @w_cap2
        jsr     @w_capture
@w_cap2:
        lda     m_sq
        clc
        adc     #17
        sta     m_to
        and     #$88
        bne     @w_done
        jsr     @w_capture
@w_done:
        rts
@w_promo_push:
        jsr     @add_promos4
        jmp     @w_caps
@w_capture:
        ldx     m_to
        lda     _board,x
        beq     @w_ep
        and     #8
        beq     @w_cap_ret
        lda     m_sq
        and     #$F0
        cmp     #$60
        beq     @w_promo_cap
        jmp     @add_quiet
@w_cap_ret:
        rts
@w_ep:
        lda     _epSq
        cmp     m_to
        bne     @w_cap_ret
        jmp     @add_quiet
@w_promo_cap:
        jmp     @add_promos4

; ---- nero: spinta -16 ----
@black:
        lda     m_sq
        sec
        sbc     #16
        sta     m_to
        and     #$88
        bne     @b_caps
        ldx     m_to
        lda     _board,x
        bne     @b_caps
        lda     m_sq
        and     #$F0
        cmp     #$10
        beq     @b_promo_push
        jsr     @add_quiet
        lda     m_sq
        cmp     #96               ; doppia spinta dalla traversa 7
        bcc     @b_caps
        cmp     #104
        bcs     @b_caps
        lda     m_sq
        sec
        sbc     #32
        sta     m_to
        ldx     m_to
        lda     _board,x
        bne     @b_caps
        jsr     @add_quiet
@b_caps:
        lda     m_sq
        sec
        sbc     #15
        sta     m_to
        and     #$88
        bne     @b_cap2
        jsr     @b_capture
@b_cap2:
        lda     m_sq
        sec
        sbc     #17
        sta     m_to
        and     #$88
        bne     @b_done
        jsr     @b_capture
@b_done:
        rts
@b_promo_push:
        jsr     @add_promos4
        jmp     @b_caps
@b_capture:
        ldx     m_to
        lda     _board,x
        beq     @b_ep
        and     #8
        bne     @b_cap_ret
        lda     m_sq
        and     #$F0
        cmp     #$10
        beq     @b_promo_cap
        jmp     @add_quiet
@b_cap_ret:
        rts
@b_ep:
        lda     _epSq
        cmp     m_to
        bne     @b_cap_ret
        jmp     @add_quiet
@b_promo_cap:
        jmp     @add_promos4

.endproc
