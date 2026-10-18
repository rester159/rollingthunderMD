/* Experimental lossless LZ32 decoder, baseline 68000 only.
 * C ABI: decode(const void *packed, void *output, unsigned raw_bytes).
 * Raw size must be a positive multiple of four. Trusted build-time streams:
 * literal token 0..32767 = token+1 longs, followed by those longs;
 * match token 1llllldddddddddd = l+3 longs, d+1 longs back.
 * Output is the entire history; no extra dictionary/scratch RAM is needed.
 * Overlapping matches intentionally copy one long at a time.
 * This is a diagnostic prototype, not a production cartridge feature.
 */
    .text
    .globl decode
decode:
    move.l 4(%sp),%a0
    move.l 8(%sp),%a1
    move.l 12(%sp),%d0
    movem.l %d2/%a2-%a3,-(%sp)
    lea (%a1,%d0.l),%a3
.Ltoken:
    move.w (%a0)+,%d0
    bmi.s .Lmatch
.Lliteral:
    move.l (%a0)+,(%a1)+
    dbf %d0,.Lliteral
    bra.s .Lnext
.Lmatch:
    move.w %d0,%d1
    andi.w #1023,%d1
    addq.w #1,%d1
    lsl.w #2,%d1
    move.l %a1,%a2
    suba.w %d1,%a2
    lsr.w #8,%d0
    lsr.w #2,%d0
    andi.w #31,%d0
    addq.w #2,%d0
.Lcopy:
    move.l (%a2)+,(%a1)+
    dbf %d0,.Lcopy
.Lnext:
    cmpa.l %a3,%a1
    bcs.s .Ltoken
    movem.l (%sp)+,%d2/%a2-%a3
    rts
