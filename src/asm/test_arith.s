.section .text
.globl _start
_start:
    li t0, 5
    li t1, 3

    li a0, 1
    add t2, t0, t1
    li t3, 8
    bne t2, t3, fail

    li a0, 2
    sub t2, t0, t1
    li t3, 2
    bne t2, t3, fail

    li a0, 3
    li t0, 0xF0
    li t1, 0x0F
    xor t2, t0, t1
    li t3, 0xFF
    bne t2, t3, fail

    li a0, 4
    or t2, t0, t1
    li t3, 0xFF
    bne t2, t3, fail

    li a0, 5
    and t2, t0, t1
    li t3, 0
    bne t2, t3, fail

    # SLL
    li a0, 6
    li t0, 1
    li t1, 4
    sll t2, t0, t1
    li t3, 16
    bne t2, t3, fail

    # SRL: logical shift, top bits become 0
    li a0, 7
    li t0, 0x80000000
    li t1, 4
    srl t2, t0, t1
    li t3, 0x08000000
    bne t2, t3, fail

    # SRA: arithmetic shift, top bits copy sign
    li a0, 8
    sra t2, t0, t1
    li t3, 0xF8000000
    bne t2, t3, fail

    # SLT signed: -1 < 1 is true
    li a0, 9
    li t0, -1
    li t1, 1
    slt t2, t0, t1
    li t3, 1
    bne t2, t3, fail

    # SLTU unsigned: 0xFFFFFFFF < 1 is false
    li a0, 10
    sltu t2, t0, t1
    li t3, 0
    bne t2, t3, fail

    li a0, 0
    j done
fail:
done:
    li a7, 93
    ecall