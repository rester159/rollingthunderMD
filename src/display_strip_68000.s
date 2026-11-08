/* Shared-block map column, already clipped and split at the 32-cell ring seam.
 * C ABI: map, block base (including column within block), destination,
 * byte stride, nonzero count. Full-width block indices preserve large maps. */
    .text
    .globl RT_copyPackedColumn
RT_copyPackedColumn:
    movem.l %d2/%a2,-(%sp)
    move.l 12(%sp),%a0
    move.l 16(%sp),%a1
    move.l 20(%sp),%a2
    move.l 24(%sp),%d1
    move.l 28(%sp),%d2
    subq.w #1,%d2
.Lcolumn:
    moveq #0,%d0
    move.w (%a0),%d0
    lsl.l #5,%d0
    move.w (%a1,%d0.l),(%a2)+
    adda.l %d1,%a0
    dbf %d2,.Lcolumn
    movem.l (%sp)+,%d2/%a2
    rts
