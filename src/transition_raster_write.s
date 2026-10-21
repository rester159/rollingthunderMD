/* H40 has two FIFO consumption slots in horizontal blank (352 and 820
 * master clocks). Enqueue only two words, with separate CRAM addresses so
 * unchanged palette entries need no write. Synchronize near the end of the
 * preceding line; instruction latency puts both words before those slots.
 * The caller parks ROM streaming in RAM playback before entering this code. */
    .text
    .align 2
    .global RT_transitionRasterWriteColors
RT_transitionRasterWriteColors:
    movem.l %d2-%d3/%d6-%d7/%a2-%a4,-(%sp)
    move.l 32(%sp),%a0
    move.l 36(%sp),%a1
    move.l 40(%sp),%d6
    subq.w #1,%d6
    lea 0xc00000,%a2
    lea 9(%a2),%a4
    lea 4(%a2),%a3
    moveq #-96,%d7               /* 0xa0, preceding line's last pixels */
0:
    moveq #0,%d0
    move.b (%a0)+,%d0
    add.w %d0,%d0
    ori.w #0xc000,%d0
    swap %d0
    move.w (%a1)+,%d1
    moveq #0,%d2
    move.b (%a0)+,%d2
    add.w %d2,%d2
    ori.w #0xc000,%d2
    swap %d2
    move.w (%a1)+,%d3
1:
    cmp.b (%a4),%d7
    bls.s 1b
2:
    cmp.b (%a4),%d7
    bhi.s 2b
    move.l %d0,(%a3)
    move.w %d1,(%a2)
    move.l %d2,(%a3)
    move.w %d3,(%a2)
    dbra %d6,0b
    movem.l (%sp)+,%d2-%d3/%d6-%d7/%a2-%a4
    rts
