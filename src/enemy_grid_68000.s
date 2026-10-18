/* Same fixed-point admission as the C reference. Word arithmetic intentionally
 * preserves its final s16 coordinate wrapping. No gameplay timing changes. */
    .text
    .even
    .globl RT_enemyActivateGrid
RT_enemyActivateGrid:
    movem.l %d2-%d4,-(%sp)
    move.l 16(%sp),%a0
    move.l %a0,%d0
    beq.w .LgridBad
    move.l 20(%sp),%a1
    move.l %a1,%d0
    beq.w .LgridBad
    move.b 9(%a0),%d0
    andi.b #252,%d0
    cmpi.b #128,%d0
    bne.w .LgridBad
    move.b 8(%a0),%d0
    cmpi.b #4,%d0
    beq.s .LgridKind
    cmpi.b #8,%d0
    beq.s .LgridKind
    cmpi.b #12,%d0
    bne.w .LgridBad
    cmpi.b #6,10(%a0)
    bne.w .LgridBad
.LgridKind:
    moveq #0,%d1
    move.b 10(%a0),%d1
    move.w %d1,%d2
    andi.w #252,%d2
    beq.s .LgridCoordinates
    cmpi.w #4,%d2
    beq.s .LgridCoordinates
    cmpi.w #8,%d2
    beq.s .LgridCoordinates
    cmpi.w #60,%d2
    bne.w .LgridBad
.LgridCoordinates:
    move.w 2(%a1),%d4
    lsl.w #4,%d4
    sub.w 6(%a0),%d4
    addi.w #5120,%d4
    cmpi.w #5120,%d4
    bcc.w .LgridDone
    move.w (%a1),%d4
    lsl.w #4,%d4
    move.w 2(%a0),%d0
    sub.w %d4,%d0
    moveq #0,%d3
    btst #1,%d1
    bne.s .LgridLeft
    cmpi.w #-768,%d0
    blt.s .LgridOutside
    bra.s .LgridEntering
.LgridLeft:
    cmpi.w #5376,%d0
    bge.s .LgridOutside
.LgridEntering:
    moveq #1,%d3
    bra.s .LgridService
.LgridOutside:
    move.w %d1,%d4
    andi.w #3,%d4
    bne.s .LgridService
    move.b 9(%a0),%d4
    andi.b #3,%d4
    add.b %d4,%d1
.LgridService:
    cmpi.w #4,%d2
    bne.s .LgridPause
    moveq #0,%d4
    move.b 29(%a0),%d4
    andi.w #48,%d4
    lsr.w #2,%d4
    addi.w #20,%d4
    move.b #20,35(%a0)
    btst #1,%d1
    beq.s .LgridSpeed
    neg.w %d4
    move.b #30,35(%a0)
.LgridSpeed:
    move.w %d4,24(%a0)
    clr.w 26(%a0)
    bra.s .LgridPublish
.LgridPause:
    tst.b %d3
    bne.s .LgridPublish
    tst.w %d2
    beq.s .LgridPublish
    move.b 29(%a0),%d4
    andi.b #3,%d4
    bne.s .LgridNoDelay
    move.b #2,35(%a0)
    ori.b #4,32(%a0)
    bra.s .LgridPublish
.LgridNoDelay:
    clr.b 35(%a0)
.LgridPublish:
    clr.b 11(%a0)
    tst.b %d3
    beq.s .LgridRetain
    move.b %d1,9(%a0)
    lea rtEnemyEntries,%a1
    move.w (%a1,%d2.w),12(%a0)
    lsr.w #2,%d2
    addi.w #12,%d2
    move.w %d2,18(%a0)
    bra.s .LgridDone
.LgridRetain:
    move.b %d1,10(%a0)
.LgridDone:
    moveq #1,%d0
    bra.s .LgridReturn
.LgridBad:
    moveq #0,%d0
.LgridReturn:
    movem.l (%sp)+,%d2-%d4
    rts
