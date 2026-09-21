# toolchain
CROSS   := riscv64-elf-
CC      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy

# flags
ARCH     := rv32im
ABI      := ilp32
CFLAGS   := -march=$(ARCH) -mabi=$(ABI) -ffreestanding -nostdlib -nostartfiles -O2 -Wall -Wextra
LDFLAGS  := -Wl,-Ttext=0x0 -nostdlib -nostartfiles -lgcc

# dirs
ASM_DIR := src/asm
RT_DIR  := src/rt
ELF_DIR := elf
BIN_DIR := bin
OBJ_DIR := build

# src and targets
ASM_SRCS := $(wildcard $(ASM_DIR)/*.s)
ELFS     := $(patsubst $(ASM_DIR)/%.s,$(ELF_DIR)/%.elf,$(ASM_SRCS))
BINS     := $(patsubst $(ASM_DIR)/%.s,$(BIN_DIR)/%.bin,$(ASM_SRCS))

RT_SRCS  := $(wildcard $(RT_DIR)/*.c)
RT_OBJS  := $(patsubst $(RT_DIR)/%.c,$(OBJ_DIR)/%.o,$(RT_SRCS))

# phony fuck
.PHONY: all clean run

all: $(ELFS) $(BINS)

# compile runtime C files
$(OBJ_DIR)/%.o: $(RT_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# assemble and link each asm file against the runtime
$(ELF_DIR)/%.elf: $(ASM_DIR)/%.s $(RT_OBJS) | $(ELF_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# extract raw binary
$(BIN_DIR)/%.bin: $(ELF_DIR)/%.elf | $(BIN_DIR)
	$(OBJCOPY) -O binary $< $@

$(ELF_DIR) $(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

run: all
	cargo run -- $(BIN_DIR)/$(FILE).bin

clean:
	rm -rf $(ELF_DIR) $(BIN_DIR) $(OBJ_DIR)