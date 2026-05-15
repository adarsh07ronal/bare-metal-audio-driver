# -----------------------------------------------------------------------
# Audio Driver Makefile
# Targets:
#   make          — build firmware (.elf) for QEMU
#   make test     — build and run unit tests natively on your Mac
#   make qemu     — launch firmware in QEMU (Ctrl-A X to quit)
#   make clean    — remove build artefacts
# -----------------------------------------------------------------------

# ── Toolchain ──────────────────────────────────────────────────────────
CC_ARM   = arm-none-eabi-gcc
CC_HOST  = gcc

# ── Firmware flags (ARM Cortex-M4) ─────────────────────────────────────
ARCH_FLAGS = -mcpu=cortex-m4 -mthumb -mfloat-abi=soft

FW_CFLAGS  = $(ARCH_FLAGS)          \
             -O2 -Wall -Wextra      \
             -ffreestanding         \
             -nostdlib              \
             -Isrc

FW_LDFLAGS = $(ARCH_FLAGS)                          \
             -T linker/stm32f407.ld                 \
             -nostdlib                              \
             -Wl,--gc-sections                      \
             -lgcc

# ── Host (test) flags ──────────────────────────────────────────────────
HOST_CFLAGS = -O0 -g -Wall -Wextra -Isrc -Itests/unity -lm

# ── Sources ─────────────────────────────────────────────────────────────
FW_SRCS = src/main.c       \
          src/i2s.c        \
          src/audio_buf.c  \
          src/sine_gen.c

# ── Output directory ────────────────────────────────────────────────────
BUILD = build

# ═══════════════════════════════════════════════════════════════════════
# Default target: firmware ELF
# ═══════════════════════════════════════════════════════════════════════
.PHONY: all
all: $(BUILD)/audio.elf

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/audio.elf: $(FW_SRCS) | $(BUILD)
	$(CC_ARM) $(FW_CFLAGS) $(FW_SRCS) $(FW_LDFLAGS) -o $@
	@echo ""
	@echo "  Built: $@"
	@arm-none-eabi-size $@

# ═══════════════════════════════════════════════════════════════════════
# Unit tests — compiled for your Mac, run natively
# ═══════════════════════════════════════════════════════════════════════
.PHONY: test
test: test_audio_buf test_sine_gen test_pdm_filter
	@echo ""
	@echo "All test suites complete."

test_audio_buf: $(BUILD)
	$(CC_HOST) $(HOST_CFLAGS) \
	    tests/test_audio_buf.c src/audio_buf.c tests/unity/unity.c \
	    -o $(BUILD)/test_audio_buf
	@echo ""
	./$(BUILD)/test_audio_buf

test_sine_gen: $(BUILD)
	$(CC_HOST) -O0 -g -Wall -Wextra -Isrc -Itests/unity \
	    tests/test_sine_gen.c src/sine_gen.c tests/unity/unity.c \
	    -o $(BUILD)/test_sine_gen -lm
	@echo ""
	./$(BUILD)/test_sine_gen

test_pdm_filter: $(BUILD)
	$(CC_HOST) -O0 -g -Wall -Wextra -Isrc -Itests/unity \
	    tests/test_pdm_filter.c src/pdm_filter.c tests/unity/unity.c \
	    -o $(BUILD)/test_pdm_filter -lm
	@echo ""
	./$(BUILD)/test_pdm_filter

# ═══════════════════════════════════════════════════════════════════════
# Run in QEMU
# ═══════════════════════════════════════════════════════════════════════
.PHONY: qemu
qemu: $(BUILD)/audio.elf
	@echo "Starting QEMU... press Ctrl-A then X to quit."
	qemu-system-arm              \
	    -machine netduinoplus2   \
	    -cpu cortex-m4           \
	    -nographic               \
	    -kernel $(BUILD)/audio.elf

# ═══════════════════════════════════════════════════════════════════════
# Clean
# ═══════════════════════════════════════════════════════════════════════
.PHONY: clean
clean:
	rm -rf $(BUILD)
