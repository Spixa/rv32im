.section .text
.globl _start
_start:
    mv s0, sp # save original sp

    
    # push two values (LIFO order)
    li a0, 1
    li t0, 0xAAAAAAAA
    addi sp, sp, -4
    sw t0, 0(sp)

    li a0, 2
    li t1, 0xBBBBBBBB
    addi sp, sp, -4
    sw t1, 0(sp)

    # pop top, verify it's t1
    li a0, 3
    lw t2, 0(sp)
    bne t2, t1, fail
    addi sp, sp, 4

    # pop next, verify it's t0
    li a0, 4
    lw t2, 0(sp)
    bne t2, t0, fail
    addi sp, sp, 4

    # sp must be back where we started
    li a0, 5
    bne sp, s0, fail

    # nested call: callee saves ra/s0/s1, uses its own frame
    li a0, 6
    jal ra, inner
    li t3, 123
    bne a0, t3, fail

    li a0, 0 # success
    j done
fail:
done:
    li a7, 93
    ecall

inner:
    addi sp, sp, -16 # make a frame
    sw ra, 12(sp)
    sw s0, 8(sp)
    sw s1, 4(sp)

    li s1, 123
    mv a0, s1

    lw s1, 4(sp)
    lw s0, 8(sp)
    lw ra, 12(sp)
    addi sp, sp, 16
    ret
