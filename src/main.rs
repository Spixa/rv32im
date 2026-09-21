use std::{env, fs, process};

use crate::{
    cpu::{Cpu, DRAM_BASE, Trap},
    memory::Bus,
};

mod cpu;
mod memory;

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        eprintln!("usage: {} <raw-binary>", args[0]);
        process::exit(1);
    }

    let bytes = fs::read(&args[1]).expect("failed to read binary");

    let mut bus = Bus::new(); // allocates 128MiB immediately
    bus.load(DRAM_BASE, &bytes); // write all of the binary in memory at base addr

    let mut cpu = Cpu::new(bus);

    loop {
        match cpu.step() {
            Ok(()) => {}
            Err(Trap::Exit(code)) => {
                println!("\n[emu] exited with code {}", code);
                process::exit(code);
            }
            Err(Trap::IllegalInstr(inst)) => {
                eprintln!(
                    "[emu] illegal instruction 0x{:08x} at pc 0x{:08x}",
                    inst, cpu.pc
                );
                process::exit(1);
            }
        }
    }
}
