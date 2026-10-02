.section .data
panic_msg:
    .asciz "it is time to panic"

.section .text
.globl _start
_start:
    # three 32-byte allocations
    li s2, 1
    li a0, 32
    jal __alloc
    beqz a0, fail
    mv s0, a0   # A

    li s2, 2
    li a0, 32
    jal __alloc
    beqz a0, fail
    mv s1, a0   # B

    li s2, 3
    li a0, 32
    jal __alloc
    beqz a0, fail
    mv s3, a0   # C

    # free B, then alloc 32: must reuse B
    li s2, 4
    mv a0, s1
    jal __free

    li s2, 5
    li a0, 32
    jal __alloc
    bne a0, s1, fail

    # free the reused B and A. now A+B is a coalesced free region.
    li s2, 6
    mv a0, s1
    jal __free

    li s2, 7
    mv a0, s0
    jal __free

    # allocate 80 bytes: 80 + 20 = 100, rounded to 104 (8-byte aligned)
    # A (48) + B (48) = 96 coalesced, so a fit requires splitting
    # 96 < 104 → must bump brk. so this does NOT reuse A.
    # allocate 64 instead: 64 + 20 = 84 -[8 byte align]-> 88: fits in 96

    li s2, 8
    li a0, 64
    jal __alloc
    bne a0, s0, fail    # must start at A

    # double free must not corrupt
    li s2, 9
    mv a0, s1
    jal __free
    mv a0, s1
    jal __free

    # la s1, panic_msg
    # mv a0, s1
    # jal __panic

    li a0, 0
    j done
fail:
    mv a0, s2
done:
    li a7, 93
    ecall