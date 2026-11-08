/* Diagnostic palette-normalized fast byte-LZ decoder for baseline 68000.
 * Match opcode: low 7 bits + 3 bytes. Distance byte 1..255; zero escapes
 * to a two-byte big-endian distance. Literal opcode: low 7 bits + 1 bytes.
 * C ABI: decode(packed, output, size_and_colors). Low 16 bits are raw bytes;
 * high 16 bits optionally give the used color count (zero means sixteen).
 * packed starts with the eight-byte
 * nibble permutation, then an independent LZ stream. Palette expansion uses
 * 272 bytes on the stack. Matches copy already restored pixels, so only
 * literals need palette translation. No retained dictionary or static RAM.
 */
    .text
    .globl decode
decode:
    movem.l %d2-%d7/%a2-%a5,-(%sp)
    move.l 44(%sp),%a0
    move.l 48(%sp),%a1
    move.l 52(%sp),%d0
    lea 8(%a0),%a5
    bra.s .Lbegin
    .globl RT_decodeCanonicalPose
RT_decodeCanonicalPose:
    /* Runtime ABI: stream, palette, output, (colors << 16) | raw_bytes. */
    movem.l %d2-%d7/%a2-%a5,-(%sp)
    move.l 44(%sp),%a5
    move.l 48(%sp),%a0
    move.l 52(%sp),%a1
    move.l 56(%sp),%d0
.Lbegin:
    /* High word optionally bounds the palette rows to colors actually used. */
    move.l %d0,%d6
    swap %d6
    andi.l #65535,%d0
    tst.w %d6
    bne.s .Lcolors
    moveq #16,%d6
.Lcolors:
    subq.w #1,%d6
    lea (%a1,%d0.l),%a3
    lea -272(%sp),%sp
    move.l %sp,%a2
    moveq #7,%d7
.Lpalette:
    move.b (%a0)+,%d0
    move.b %d0,%d1
    lsr.b #4,%d0
    andi.b #15,%d1
    move.b %d0,(%a2)+
    move.b %d1,(%a2)+
    dbf %d7,.Lpalette
    move.l %a2,%a4
    movem.l (%sp),%d2-%d5
    move.l %sp,%a2
    move.w %d6,%d7
.Lrow:
    moveq #0,%d0
    move.b (%a2)+,%d0
    lsl.w #4,%d0
    move.w %d0,%d1
    lsl.w #8,%d0
    or.w %d1,%d0
    move.w %d0,%d1
    swap %d0
    move.w %d1,%d0
    move.l %d2,%d1
    or.l %d0,%d1
    move.l %d1,(%a4)+
    move.l %d3,%d1
    or.l %d0,%d1
    move.l %d1,(%a4)+
    move.l %d4,%d1
    or.l %d0,%d1
    move.l %d1,(%a4)+
    move.l %d5,%d1
    or.l %d0,%d1
    move.l %d1,(%a4)+
    dbf %d7,.Lrow
    lea 16(%sp),%a4
    move.l %a5,%a0
/* Word tokens retain even output and history boundaries throughout. */
.Ltoken:
    moveq #0,%d0
    move.b (%a0)+,%d0
    bmi.w .Lmatch
    add.w %d0,%d0
    addq.w #1,%d0
    moveq #0,%d1
    cmpi.w #7,%d0
    bcs.s .Lliteral
    move.w %d0,%d2
    not.w %d2
    andi.w #7,%d2
    add.w %d2,%d2
    move.w %d2,%d3
    add.w %d2,%d2
    add.w %d3,%d2
    lsr.w #3,%d0
    jmp .Lliteral8(%pc,%d2.w)
.Lliteral8:
    move.b (%a0)+,%d1
    move.b (%a4,%d1.w),(%a1)+
    move.b (%a0)+,%d1
    move.b (%a4,%d1.w),(%a1)+
    move.b (%a0)+,%d1
    move.b (%a4,%d1.w),(%a1)+
    move.b (%a0)+,%d1
    move.b (%a4,%d1.w),(%a1)+
    move.b (%a0)+,%d1
    move.b (%a4,%d1.w),(%a1)+
    move.b (%a0)+,%d1
    move.b (%a4,%d1.w),(%a1)+
    move.b (%a0)+,%d1
    move.b (%a4,%d1.w),(%a1)+
    move.b (%a0)+,%d1
    move.b (%a4,%d1.w),(%a1)+
    dbf %d0,.Lliteral8
    bra.s .Lnext
.Lliteral:
    move.b (%a0)+,%d1
    move.b (%a4,%d1.w),(%a1)+
    dbf %d0,.Lliteral
    bra.s .Lnext
.Lmatch:
    andi.w #127,%d0
    addq.w #2,%d0
    moveq #0,%d1
    move.b (%a0)+,%d1
    bne.s .Ldistance
    move.b (%a0)+,%d1
    lsl.w #8,%d1
    move.b (%a0)+,%d1
.Ldistance:
    add.w %d1,%d1
    move.l %a1,%a2
    suba.w %d1,%a2
    move.w %d0,%d2
    lsr.w #1,%d0
    subq.w #1,%d0
    cmpi.w #2,%d1
    beq.s .Lrepeat
.Llong:
    move.l (%a2)+,(%a1)+
    dbf %d0,.Llong
    btst #0,%d2
    beq.s .Lnext
    move.w (%a2)+,(%a1)+
    bra.s .Lnext
.Lrepeat:
    move.w (%a2),%d3
    swap %d3
    move.w (%a2),%d3
.Lfill:
    move.l %d3,(%a1)+
    dbf %d0,.Lfill
    btst #0,%d2
    beq.s .Lnext
    move.w %d3,(%a1)+
.Lnext:
    cmpa.l %a3,%a1
    bcs.w .Ltoken
    lea 272(%sp),%sp
    movem.l (%sp)+,%d2-%d7/%a2-%a5
    rts
