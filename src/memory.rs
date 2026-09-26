pub const MEMORY_SIZE: usize = 128 * 1024 * 1024; // 128 MiB
pub const DRAM_SIZE: u32 = MEMORY_SIZE as u32;
pub const DRAM_BASE: u32 = 0x0000_0000;
pub const HEAP_BASE: u32 = 0x0100_0000;
pub const MMAP_BASE: u32 = 0x0400_0000;
pub const STACK_TOP: u32 = DRAM_SIZE;

pub struct Bus {
    dram: Vec<u8>,
}

impl Bus {
    pub fn new() -> Self {
        Self {
            dram: vec![0; MEMORY_SIZE], // holy shit
        }
    }

    pub fn load(&mut self, addr: u32, data: &[u8]) {
        let addr = addr as usize;
        self.dram[addr..addr + data.len()].copy_from_slice(data);
    }

    pub fn read_byte(&self, addr: u32) -> u8 {
        self.dram[addr as usize]
    }

    pub fn read_half(&self, addr: u32) -> u16 {
        let addr = addr as usize;
        u16::from_le_bytes([self.dram[addr], self.dram[addr + 1]])
    }

    pub fn read_word(&self, addr: u32) -> u32 {
        let addr = addr as usize;
        u32::from_le_bytes([
            self.dram[addr],
            self.dram[addr + 1],
            self.dram[addr + 2],
            self.dram[addr + 3],
        ])
    }

    pub fn write_byte(&mut self, addr: u32, value: u8) {
        self.dram[addr as usize] = value;
    }

    pub fn write_half(&mut self, addr: u32, value: u16) {
        let addr = addr as usize;
        self.dram[addr..addr + 2].copy_from_slice(&value.to_le_bytes());
    }

    pub fn write_word(&mut self, addr: u32, value: u32) {
        let addr = addr as usize;

        if addr + 4 > self.dram.len() {
            panic!("write_word OOB: addr=0x{:08x} value=0x{:08x}", addr, value);
        }
        self.dram[addr..addr + 4].copy_from_slice(&value.to_le_bytes());
    }
}
