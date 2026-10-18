/* Lossless PCM presence-mask prototype. Each group starts with one mask byte,
 * MSB first, followed by the samples whose mask bits are 1. Zero bits produce
 * the original unsigned-PCM silence value, 128. No quantization or resampling.
 * C ABI as decode.s; raw_bytes must be a positive multiple of eight.
 */
    .text
    .globl decode
decode:
    move.l 4(%sp),%a0
    move.l 8(%sp),%a1
    move.l 12(%sp),%d0
    movem.l %d2/%a2,-(%sp)
    lea (%a1,%d0.l),%a2
    move.l #0x80808080,%d2
.Lgroup:
    move.b (%a0)+,%d1
    bne.s .Lbits
    move.l %d2,(%a1)+
    move.l %d2,(%a1)+
    bra.s .Lnext
.Lbits:
    /* Fixed eight samples remove a per-sample loop/counter from playback. */
    .rept 8
    add.b %d1,%d1
    bcc.s 1f
    move.b (%a0)+,(%a1)+
    bra.s 2f
1:
    move.b %d2,(%a1)+
2:
    .endr
.Lnext:
    cmpa.l %a2,%a1
    bcs.s .Lgroup
    movem.l (%sp)+,%d2/%a2
    rts
