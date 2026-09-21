.section .data
msg:
    .asciz "Hello from my emulator!\n"

.section .text
.globl _start
_start:
    # printing messages
    li a7, 64
    li a0, 1
    la a1, msg
    li a2, 24
    ecall

    # exitting
    li a7, 93
    li a0, 0
    ecall