.section .text
.globl _start
_start:
    # alloc 8 bytes 
    li a0, 1
    li a0, 8
    jal __alloc
    beqz a0, fail
    mv s0, a0

    # 8-byte alignment
    andi t0, s0, 7
    bnez t0, fail

    # write/read through it
    li a0, 2
    li t1, 0xDEADBEEF
    sw t1, 0(s0)
    lw t2, 0(s0)
    bne t1, t2, fail

    # second alloc: must be after the first 
    li a0, 3
    li a0, 8
    jal __alloc
    beqz a0, fail
    mv s1, a0
    sub t0, s1, s0
    # 8 (user) + 16 (header) = 24, rounded to 8 = 24
    li t1, 24
    bne t0, t1, fail

    # realloc: should preserve contents
    li a0, 4
    mv a0, s0
    li a1, 64
    jal __realloc
    beqz a0, fail
    lw t2, 0(a0)
    li t1, 0xDEADBEEF
    bne t1, t2, fail

    # calloc: test allocation + ensure it is zeroed 
    li a0, 5
    li a0, 4
    li a1, 16
    jal __calloc
    beqz a0, fail
    # lets pick off some random words in the segment for this and make sure they are 0
    lw t0, 0(a0)
    bnez t0, fail 
    lw t0, 60(a0) 
    bnez t0, fail
    # good enough. calloc works!

    li a0, 0
    j done
fail:
    li a0, 99
done:
    li a7, 93
    ecall