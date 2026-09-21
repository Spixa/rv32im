## rv32im emulator
making an rv32im emulator catered for another project (compiled functional language). devlog is [here](https://t.me/maybe_uninit)!


### what we've got so far
all instructions for the base RV32I are here with the M extention too. available syscalls so far include `sys_write`, `sys_exit`, and `sys_brk` for heap allocation. small C runtime takes care of heap allocation (no freeing or garbage collection foundation yet for the language). very much so a WIP. 