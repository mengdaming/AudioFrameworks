/*
 * filter_arm.S
 *
 * Description: Assembler optimised version of TTA1 hybrid filter
 * Copyright (c) 2010 Yoshihisa Uchida. All rights reserved.
 * Distributed under the GNU Lesser General Public License (LGPL).
 * The complete text of the license can be found in the COPYING
 * file included in the distribution.
 *
 */

/////////////////// hybrid_filter_dec(fltst *fs, int *in) ///////////////////
/////////////////////////////////////////////////////////////////////////////

#ifdef USE_IRAM
    .section .icode, "ax", %progbits
#else
    .text
#endif
    .align
    .global  hybrid_filter_dec
    .type    hybrid_filter_dec, %function

hybrid_filter_dec:
    @ input: r0 = fs, r1 = in
    stmdb    sp!, {r4 - r12, lr}

    @ get fs members
    @ r2 pA  := fs->dl + fs->index
    @ r3 pM  := fs->dx + fs->index
    @ r4 pB  := fs->qm
    @ r5 fs->index
    @ r6 fs->error
    @ lr sum := fs->round

    add      r2,  r0, #144                       @ r2 = fs->dl
    add      r3,  r0, #48                        @ r3 = fs->dx
    add      r4,  r0, #16                        @ r4 = fs->qm
    ldmia    r0,  {r5, r6, lr}                   @ r5 = fs->index
                                                 @ r6 = fs->error
                                                 @ lr = fs->round
    mov      r5,  r5, asl #2
    add      r2,  r2, r5                         @ r2 = fs->dl + fs->index
    add      r3,  r3, r5                         @ r3 = fs->dx + fs->index

    cmp      r6,  #0
    bne      .hf_positive_dec

    @ case fs->error == 0

    add      r3,  r3, #32
    ldmia    r4!, {r5, r6,  r7,  r8 }
    ldmia    r2!, {r9, r10, r11, r12}
    mla      lr,  r5, r9,  lr
    mla      lr,  r6, r10, lr
    mla      lr,  r7, r11, lr
    mla      lr,  r8, r12, lr
    ldmia    r4!, {r5, r6,  r7,  r8 }
    b        .hf2_dec

.hf_positive_dec:
    blt      .hf_negative_dec

    @ case fs->error > 0

    ldmia    r4,  {r5, r6,  r7,  r8 }
    ldmia    r3!, {r9, r10, r11, r12}
    add      r5,  r5, r9
    add      r6,  r6, r10
    add      r7,  r7, r11
    add      r8,  r8, r12
    stmia    r4!, {r5, r6,  r7,  r8 }            @ update fs->qm[0], ..., fs->qm[3]
    ldmia    r2!, {r9, r10, r11, r12}
    mla      lr,  r5, r9,  lr
    mla      lr,  r6, r10, lr
    mla      lr,  r7, r11, lr
    mla      lr,  r8, r12, lr
    ldmia    r4,  {r5, r6,  r7,  r8 }
    ldmia    r3!, {r9, r10, r11, r12}
    add      r5,  r5, r9
    add      r6,  r6, r10
    add      r7,  r7, r11
    add      r8,  r8, r12
    stmia    r4!, {r5, r6,  r7,  r8 }            @ update fs->qm[4], ..., fs->qm[7]
    b .hf2_dec

.hf_negative_dec:
    @ case fs->error < 0

    ldmia    r4,  {r5, r6,  r7,  r8 }
    ldmia    r3!, {r9, r10, r11, r12}
    sub      r5,  r5, r9
    sub      r6,  r6, r10
    sub      r7,  r7, r11
    sub      r8,  r8, r12
    stmia    r4!, {r5, r6,  r7,  r8 }            @ update fs->qm[0], ..., fs->qm[3]
    ldmia    r2!, {r9, r10, r11, r12}
    mla      lr,  r5, r9,  lr
    mla      lr,  r6, r10, lr
    mla      lr,  r7, r11, lr
    mla      lr,  r8, r12, lr
    ldmia    r4,  {r5, r6,  r7,  r8 }
    ldmia    r3!, {r9, r10, r11, r12}
    sub      r5,  r5, r9
    sub      r6,  r6, r10
    sub      r7,  r7, r11
    sub      r8,  r8, r12
    stmia    r4!, {r5, r6,  r7,  r8 }            @ update fs->qm[4], ..., fs->qm[7]

.hf2_dec:
    ldmia    r2!, {r9, r10, r11, r12}
    mla      lr,  r5, r9,  lr
    mla      lr,  r6, r10, lr
    mla      lr,  r7, r11, lr
    mla      lr,  r8, r12, lr

    @ fs->error = *in;
    @ *in += (sum >> fs->shift)
    @ *pA = *in

    ldr      r5,  [r1]                           @ r5 = *in
    ldr      r6,  [r0, #12]                      @ r6 = fs->shift
    add      lr,  r5, lr, asr r6
    str      lr,  [r1]                           @ *in += (sum >> fs->shift)

    @ update fs->index

    ldr      r1,  [r0]                           @ r1 = fs->index
    add      r1,  r1,  #1
    ands     r1,  r1,  #15                       @ set Z flag (after this, CPSR must keep !!)
    stmia    r0,  {r1, r5}                       @ fs->index = (++fs->index & 15)
                                                 @ fs->error = (original) *in
    @ change *pM, *(pM-1), *(pM-2), *(pM-3)
    @ r9  = *(pA-4), r5 = *(pM-3)
    @ r10 = *(pA-3), r6 = *(pM-2)
    @ r11 = *(pA-2), r7 = *(pM-1)
    @ r12 = *(pA-1), r8 = *(pM-0)
    @ lr  = *(pA-0)

    mov      r4,  #1
    orr      r5,  r4, r9,  asr #30
    orr      r6,  r4, r10, asr #30
    orr      r7,  r4, r11, asr #30
    orr      r8,  r4, r12, asr #30
    mov      r6,  r6, lsl #1
    mov      r7,  r7, lsl #1
    mov      r8,  r8, lsl #2

    @ change *(pA-1), *(pA-2), *(pA-3)
    sub      r12, lr,  r12
    sub      r11, r12, r11
    sub      r10, r11, r10

    @ check fs->index is zero
    beq      .hf_memshl_dec

    @ set to the memory: *pA, *(pA-1), *(pA-2), *(pA-3), *pM, *(pM-1), *(pM-2), *(pM-3)
    stmda    r2,  {r10, r11, r12, lr}
    stmda    r3,  {r5,  r6,  r7,  r8}
    ldmfd    sp!, {r4-r12, pc}                   @ end (when fs->index != 0)

.hf_memshl_dec:
    @ memshl (fs->dl)
    @ r9  = fs->dl[16 + 3]
    @ r10 = fs->dl[16 + 4]
    @ r11 = fs->dl[16 + 5]
    @ r12 = fs->dl[16 + 6]
    @ lr  = fs->dl[16 + 7]

    add      r2,  r0,  #208                      @ r2 = fs->dl + 16
    ldmia    r2,  {r1, r3, r4}
    sub      r2,  r2,  #64                       @ r2 = fs->dl
    stmia    r2,  {r1, r3, r4, r9 - r12, lr}

    @ memshl (fs->dx)
    @ r5 = fs->dx[16 + 4]
    @ r6 = fs->dx[16 + 5]
    @ r7 = fs->dx[16 + 6]
    @ r8 = fs->dx[16 + 7]

    add      r9,  r0,  #112                      @ r9 = fs->dx + 16
    ldmia    r9,  {r1, r2, r3, r4}
    sub      r9,  r9,  #64                       @ r9 = fs->dx
    stmia    r9,  {r1 - r8}
    ldmfd    sp!, {r4 - r12, pc}                 @ end (when fs->index == 0)

hybrid_filter_dec_end:
    .size    hybrid_filter_dec, hybrid_filter_dec_end - hybrid_filter_dec

/////////////////// hybrid_filter_enc(fltst *fs, int *in) ///////////////////
/////////////////////////////////////////////////////////////////////////////

#ifdef USE_IRAM
    .section .icode, "ax", %progbits
#else
    .text
#endif
    .align
    .global  hybrid_filter_enc
    .type    hybrid_filter_enc, %function

hybrid_filter_enc:
    @ input: r0 = fs, r1 = in
    stmdb    sp!, {r4 - r12, lr}

    @ get fs members
    @ r2 pA  := fs->dl + fs->index
    @ r3 pM  := fs->dx + fs->index
    @ r4 pB  := fs->qm
    @ r5 fs->index
    @ r6 fs->error
    @ lr sum := fs->round

    add      r2,  r0, #144                       @ r2 = fs->dl
    add      r3,  r0, #48                        @ r3 = fs->dx
    add      r4,  r0, #16                        @ r4 = fs->qm
    ldmia    r0,  {r5, r6, lr}                   @ r5 = fs->index
                                                 @ r6 = fs->error
                                                 @ lr = fs->round
    mov      r5,  r5, asl #2
    add      r2,  r2, r5                         @ r2 = fs->dl + fs->index
    add      r3,  r3, r5                         @ r3 = fs->dx + fs->index

    cmp      r6,  #0
    bne      .hf_positive_enc

    @ case fs->error == 0

    add      r3,  r3, #32
    ldmia    r4!, {r5, r6,  r7,  r8 }
    ldmia    r2!, {r9, r10, r11, r12}
    mla      lr,  r5, r9,  lr
    mla      lr,  r6, r10, lr
    mla      lr,  r7, r11, lr
    mla      lr,  r8, r12, lr
    ldmia    r4!, {r5, r6,  r7,  r8 }
    b        .hf2_enc

.hf_positive_enc:
    blt      .hf_negative_enc

    @ case fs->error > 0

    ldmia    r4,  {r5, r6,  r7,  r8 }
    ldmia    r3!, {r9, r10, r11, r12}
    add      r5,  r5, r9
    add      r6,  r6, r10
    add      r7,  r7, r11
    add      r8,  r8, r12
    stmia    r4!, {r5, r6,  r7,  r8 }            @ update fs->qm[0], ..., fs->qm[3]
    ldmia    r2!, {r9, r10, r11, r12}
    mla      lr,  r5, r9,  lr
    mla      lr,  r6, r10, lr
    mla      lr,  r7, r11, lr
    mla      lr,  r8, r12, lr
    ldmia    r4,  {r5, r6,  r7,  r8 }
    ldmia    r3!, {r9, r10, r11, r12}
    add      r5,  r5, r9
    add      r6,  r6, r10
    add      r7,  r7, r11
    add      r8,  r8, r12
    stmia    r4!, {r5, r6,  r7,  r8 }            @ update fs->qm[4], ..., fs->qm[7]
    b .hf2_enc

.hf_negative_enc:
    @ case fs->error < 0

    ldmia    r4,  {r5, r6,  r7,  r8 }
    ldmia    r3!, {r9, r10, r11, r12}
    sub      r5,  r5, r9
    sub      r6,  r6, r10
    sub      r7,  r7, r11
    sub      r8,  r8, r12
    stmia    r4!, {r5, r6,  r7,  r8 }            @ update fs->qm[0], ..., fs->qm[3]
    ldmia    r2!, {r9, r10, r11, r12}
    mla      lr,  r5, r9,  lr
    mla      lr,  r6, r10, lr
    mla      lr,  r7, r11, lr
    mla      lr,  r8, r12, lr
    ldmia    r4,  {r5, r6,  r7,  r8 }
    ldmia    r3!, {r9, r10, r11, r12}
    sub      r5,  r5, r9
    sub      r6,  r6, r10
    sub      r7,  r7, r11
    sub      r8,  r8, r12
    stmia    r4!, {r5, r6,  r7,  r8 }            @ update fs->qm[4], ..., fs->qm[7]

.hf2_enc:
    ldmia    r2!, {r9, r10, r11, r12}
    mla      lr,  r5, r9,  lr
    mla      lr,  r6, r10, lr
    mla      lr,  r7, r11, lr
    mla      lr,  r8, r12, lr

    @*pA = *in;
    @*in -= (sum >> fs->shift);
    @fs->error = *in;
    
    ldr      r5,  [r1]                           @ r5 = *in
    ldr      r6,  [r0, #12]                      @ r6 = fs->shift
    sub      lr,  r5, lr, asr r6
    str      lr,  [r1]                           @ *in -= (sum >> fs->shift)

    @ update fs->index

    ldr      r1,  [r0]                           @ r1 = fs->index
    add      r1,  r1,  #1
    ands     r1,  r1,  #15                       @ set Z flag (after this, CPSR must keep !!)
    stmia    r0,  {r1, lr}                       @ fs->index = (++fs->index & 15)
    mov      lr,  r5                             @ fs->error = (new) *in
    
    @ change *pM, *(pM-1), *(pM-2), *(pM-3)
    @ r9  = *(pA-4), r5 = *(pM-3)
    @ r10 = *(pA-3), r6 = *(pM-2)
    @ r11 = *(pA-2), r7 = *(pM-1)
    @ r12 = *(pA-1), r8 = *(pM-0)
    @ lr  = *(pA-0)

    mov      r4,  #1
    orr      r5,  r4, r9,  asr #30
    orr      r6,  r4, r10, asr #30
    orr      r7,  r4, r11, asr #30
    orr      r8,  r4, r12, asr #30
    mov      r6,  r6, lsl #1
    mov      r7,  r7, lsl #1
    mov      r8,  r8, lsl #2

    @ change *(pA-1), *(pA-2), *(pA-3)
    sub      r12, lr,  r12
    sub      r11, r12, r11
    sub      r10, r11, r10

    @ check fs->index is zero
    beq      .hf_memshl_enc

    @ set to the memory: *pA, *(pA-1), *(pA-2), *(pA-3), *pM, *(pM-1), *(pM-2), *(pM-3)
    stmda    r2,  {r10, r11, r12, lr}
    stmda    r3,  {r5,  r6,  r7,  r8}
    ldmfd    sp!, {r4-r12, pc}                   @ end (when fs->index != 0)

.hf_memshl_enc:
    @ memshl (fs->dl)
    @ r9  = fs->dl[16 + 3]
    @ r10 = fs->dl[16 + 4]
    @ r11 = fs->dl[16 + 5]
    @ r12 = fs->dl[16 + 6]
    @ lr  = fs->dl[16 + 7]

    add      r2,  r0,  #208                      @ r2 = fs->dl + 16
    ldmia    r2,  {r1, r3, r4}
    sub      r2,  r2,  #64                       @ r2 = fs->dl
    stmia    r2,  {r1, r3, r4, r9 - r12, lr}

    @ memshl (fs->dx)
    @ r5 = fs->dx[16 + 4]
    @ r6 = fs->dx[16 + 5]
    @ r7 = fs->dx[16 + 6]
    @ r8 = fs->dx[16 + 7]

    add      r9,  r0,  #112                      @ r9 = fs->dx + 16
    ldmia    r9,  {r1, r2, r3, r4}
    sub      r9,  r9,  #64                       @ r9 = fs->dx
    stmia    r9,  {r1 - r8}
    ldmfd    sp!, {r4 - r12, pc}                 @ end (when fs->index == 0)

hybrid_filter_enc_end:
    .size    hybrid_filter_enc, hybrid_filter_enc_end - hybrid_filter_enc

/* eof */
