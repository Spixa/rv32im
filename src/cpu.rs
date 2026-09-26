use crate::memory::{Bus, DRAM_BASE, HEAP_BASE, MMAP_BASE, STACK_TOP};

#[derive(Debug)]
pub enum Trap {
    Exit(i32),
    IllegalInstr(u32),
}

pub struct Cpu {
    pub regs: [u32; 32],
    pub pc: u32,
    pub bus: Bus,
    pub program_break: u32,
    pub _mmap_ptr: u32,
}

impl Cpu {
    pub fn new(bus: Bus) -> Self {
        let mut cpu = Self {
            regs: [0; 32],
            pc: DRAM_BASE,
            bus,
            program_break: HEAP_BASE,
            _mmap_ptr: MMAP_BASE,
        };
        cpu.regs[2] = STACK_TOP; // set stack pointer (sp) to somewhere at the top of the memory; 
        cpu
    }

    /// Write to registers, ignore writes to x0
    #[inline]
    fn write_reg(&mut self, reg: usize, value: u32) {
        if reg != 0 {
            self.regs[reg] = value;
        }
    }

    /// Fetch, decode execute ONE instruction
    pub fn step(&mut self) -> Result<(), Trap> {
        let inst = self.bus.read_word(self.pc);
        if std::env::var("TRACE").is_ok() {
            eprintln!(
                "pc=0x{:08x} sp=0x{:08x} ra=0x{:08x} s0=0x{:08x} inst=0x{:08x}",
                self.pc, self.regs[2], self.regs[1], self.regs[8], inst
            );
        }
        self.execute(inst)
    }

    fn execute(&mut self, inst: u32) -> Result<(), Trap> {
        let opcode = inst & 0x7f; // first 7 bits
        let rd = ((inst >> 7) & 0x1f) as usize; // next 5 bits for register number (x0 .. x31)
        let funct3 = (inst >> 12) & 0x7; // next 3 bits for function 3 specs
        let rs1 = ((inst >> 15) & 0x1f) as usize; // next 5 bits for register number (x0 .. x31)
        let rs2 = ((inst >> 20) & 0x1f) as usize; // next 5 bits for register number (x0 .. x31)
        let funct7 = (inst >> 25) & 0x7f; // next 7 bits for function 7 specs

        let mut next_pc = self.pc.wrapping_add(4); // advance by a word

        const LUI: u32 = 0x37;
        const AUIPC: u32 = 0x17;
        const JAL: u32 = 0x6f;
        const JALR: u32 = 0x67;
        const BRANCH: u32 = 0x63;
        const LOAD: u32 = 0x03;
        const STORE: u32 = 0x23;
        const IMMOP: u32 = 0x13;
        const OP: u32 = 0x33;
        const _FENCE: u32 = 0x0f; // TODO
        const SYSTEM: u32 = 0x73;

        match opcode {
            LUI => {
                let imm = inst & 0xffff_f000;
                self.write_reg(rd, imm);
            }
            AUIPC => {
                let imm = inst & 0xffff_f000;
                self.write_reg(rd, self.pc.wrapping_add(imm));
            }
            JAL => {
                // from here, calculating imm gets complicated, so we're using the helper functions at the bottom
                let imm = decode_j_imm(inst);
                self.write_reg(rd, self.pc.wrapping_add(4));
                next_pc = self.pc.wrapping_add(imm);
            }
            JALR => {
                let imm = decode_i_imm(inst);
                let target = self.regs[rs1].wrapping_add(imm) & !1;
                self.write_reg(rd, self.pc.wrapping_add(4));
                next_pc = target;
            }
            BRANCH => {
                let imm = decode_b_imm(inst);
                let a = self.regs[rs1];
                let b = self.regs[rs2];
                let take = match funct3 {
                    0x0 => a == b,                   // BEQ
                    0x1 => a != b,                   // BNE
                    0x4 => (a as i32) < (b as i32),  // BLT sandwich
                    0x5 => (a as i32) >= (b as i32), // BGE
                    0x6 => a < b,                    // BLTU
                    0x7 => a >= b,                   // BGEU
                    _ => return Err(Trap::IllegalInstr(inst)),
                };

                if take {
                    next_pc = self.pc.wrapping_add(imm); // if condition met, advance program counter by imemdiate offset
                }
            }
            LOAD => {
                let imm = decode_i_imm(inst);
                let addr = self.regs[rs1].wrapping_add(imm);
                let value = match funct3 {
                    0x0 => self.bus.read_byte(addr) as i8 as i32 as u32, // LB
                    0x1 => self.bus.read_half(addr) as i16 as i32 as u32, // LH
                    0x2 => self.bus.read_word(addr),                     // LW (normal),
                    0x4 => self.bus.read_byte(addr) as u32,              // LBU
                    0x5 => self.bus.read_half(addr) as u32,              // LHU
                    _ => return Err(Trap::IllegalInstr(inst)),
                };

                self.write_reg(rd, value);
            }
            STORE => {
                let imm = decode_s_imm(inst);
                let addr = self.regs[rs1].wrapping_add(imm);
                let value = self.regs[rs2];

                match funct3 {
                    0x0 => self.bus.write_byte(addr, value as u8),
                    0x1 => self.bus.write_half(addr, value as u16),
                    0x2 => self.bus.write_word(addr, value),
                    _ => return Err(Trap::IllegalInstr(inst)),
                }
            }
            IMMOP => {
                let imm = decode_i_imm(inst);
                let a = self.regs[rs1];

                let value = match funct3 {
                    0x0 => a.wrapping_add(imm),                // ADDI
                    0x2 => ((a as i32) < (imm as i32)) as u32, // SLTI
                    0x3 => (a < imm) as u32,                   // SLTIU
                    0x4 => a ^ imm,                            // XORI
                    0x6 => a | imm,                            // ORI
                    0x7 => a & imm,                            // ANDI
                    0x1 => a << (imm & 0x1f),                  // SLLI
                    0x5 => match funct7 {
                        0x00 => a >> (imm & 0x1f),                   // SRLI
                        0x20 => ((a as i32) >> (imm & 0x1f)) as u32, // SRAI
                        _ => return Err(Trap::IllegalInstr(inst)),
                    },
                    _ => return Err(Trap::IllegalInstr(inst)),
                };

                self.write_reg(rd, value);
            }
            OP => {
                let a = self.regs[rs1];
                let b = self.regs[rs2];
                let value = match (funct7, funct3) {
                    (0x00, 0x0) => a.wrapping_add(b),                 // ADD
                    (0x20, 0x0) => a.wrapping_sub(b),                 // SUB
                    (0x00, 0x1) => a << (b & 0x1f),                   // SLL
                    (0x00, 0x2) => ((a as i32) < (b as i32)) as u32,  // SLT
                    (0x00, 0x3) => (a < b) as u32,                    // SLTU
                    (0x00, 0x4) => a ^ b,                             // XOR
                    (0x00, 0x5) => a >> (b & 0x1f),                   // SRL
                    (0x20, 0x5) => ((a as i32) >> (b & 0x1f)) as u32, // SRA
                    (0x00, 0x6) => a | b,                             // OR
                    (0x00, 0x7) => a & b,                             // AND
                    /* start of M extention !! */
                    (0x01, 0x0) => a.wrapping_mul(b), // MUL
                    // MULH: signed * signed: take high 32 bits (64-bit op)
                    (0x01, 0x1) => (((a as i32 as i64).wrapping_mul(b as i32 as i64)) >> 32) as u32,
                    // MULHSU: signed * unsigned, take high 32 bits (64-bit op)
                    (0x01, 0x2) => (((a as i32 as i64).wrapping_mul(b as i64)) >> 32) as u32,
                    // MULHU: unsigned * unsigned, take high 32 bits (64-bit op)
                    (0x01, 0x3) => (((a as u64).wrapping_mul(b as u64)) >> 32) as u32,
                    (0x01, 0x4) => {
                        // DIV
                        if b == 0 {
                            u32::MAX // -1
                        } else {
                            (a as i32).wrapping_div(b as i32) as u32
                        }
                    }
                    // DIVU
                    (0x01, 0x5) => {
                        if b == 0 {
                            u32::MAX
                        } else {
                            a / b
                        }
                    }
                    // REM
                    (0x01, 0x6) => {
                        if b == 0 {
                            a // return dividend
                        } else {
                            (a as i32).wrapping_rem(b as i32) as u32
                        }
                    }
                    // REMU
                    (0x01, 0x7) => {
                        if b == 0 {
                            a
                        } else {
                            a % b
                        }
                    }

                    /* END of M extention !! */
                    _ => return Err(Trap::IllegalInstr(inst)),
                };
                self.write_reg(rd, value);
            }
            _FENCE => {} // single threaded: no-op
            SYSTEM => match funct3 {
                0x0 => match inst {
                    0x0000_0073 => self.handle_ecall()?,
                    0x0010_0073 => return Err(Trap::Exit(0)),
                    _ => return Err(Trap::IllegalInstr(inst)),
                },
                _ => return Err(Trap::IllegalInstr(inst)),
            },
            _ => return Err(Trap::IllegalInstr(inst)),
        }

        self.pc = next_pc;
        Ok(())
    }

    /// RISC-V Linux syscall ABI calling convention
    /// a7 = syscall number
    /// a0..a5 = arguments
    /// a0 = return value
    fn handle_ecall(&mut self) -> Result<(), Trap> {
        let syscall = self.regs[17]; // a7
        match syscall {
            64 => self.sys_write(),
            93 => {
                let code = self.regs[10] as i32; // a0
                return Err(Trap::Exit(code));
            }
            214 => self.sys_brk(),
            222 => self.sys_mmap(),
            _ => {
                eprintln!(
                    "[emu] unimplemented syscall {} (reading from a7 when `ecall` was called)",
                    syscall
                );
                self.regs[10] = (-38i32) as u32; // -ENOSYS
            }
        }

        Ok(())
    }

    /// args: a0=fd, a1=ptr, a2=count
    /// ret: a0=count or error
    fn sys_write(&mut self) {
        let fd = self.regs[10]; // a0
        let ptr = self.regs[11]; // a1
        let count = self.regs[12]; // a2

        // copy bytes from memory
        let mut bytes = Vec::with_capacity(count as usize);
        for i in 0..count {
            bytes.push(self.bus.read_byte(ptr.wrapping_add(i)));
        }

        // stdout | stderr
        if fd == 1 || fd == 2 {
            use std::io::Write;
            let stream: &mut dyn Write = if fd == 1 {
                &mut std::io::stdout()
            } else {
                &mut std::io::stderr()
            };

            // always discard
            let _ = stream.write_all(&bytes);
            let _ = stream.flush(); // ensure everything reaches dest
            self.regs[10] = count; // normal ret 
        } else {
            self.regs[10] = (-9i32) as u32; // -EBADF
        }
    }

    /// args: a0=new break (0 = query)
    /// ret: a0 = 0 -> success, others failure
    fn sys_brk(&mut self) {
        let new_break = self.regs[10];

        if new_break == 0 {
            self.regs[10] = self.program_break; // query
            return;
        }

        if !(HEAP_BASE..MMAP_BASE).contains(&new_break) || (new_break & 0x3) != 0 {
            self.regs[10] = self.program_break; // failure + query return
            return;
        }

        self.program_break = new_break;
        self.regs[10] = 0; // success
    }

    /// args: a0 = addr, a1 = length, a2 = prot, a3 = flags, a4 = fd, a5 = offset
    ///
    fn sys_mmap(&mut self) {
        unimplemented!()
    }
}

#[inline]
fn sign_extend(value: u32, bits: u32) -> u32 {
    let shift = 32 - bits;
    ((value << shift) as i32 >> shift) as u32
}

#[inline]
fn decode_i_imm(inst: u32) -> u32 {
    sign_extend(inst >> 20, 12)
}

#[inline]
fn decode_s_imm(inst: u32) -> u32 {
    let imm = ((inst >> 25) << 5) | ((inst >> 7) & 0x1f);
    sign_extend(imm, 12)
}

#[inline]
fn decode_b_imm(inst: u32) -> u32 {
    let imm = (((inst >> 31) & 0x1) << 12)
        | (((inst >> 7) & 0x1) << 11)
        | (((inst >> 25) & 0x3f) << 5)
        | (((inst >> 8) & 0xf) << 1);
    sign_extend(imm, 13)
}

#[inline]
fn decode_j_imm(inst: u32) -> u32 {
    let imm = (((inst >> 31) & 0x1) << 20)
        | (((inst >> 12) & 0xff) << 12)
        | (((inst >> 20) & 0x1) << 11)
        | (((inst >> 21) & 0x3ff) << 1);
    sign_extend(imm, 21)
}
