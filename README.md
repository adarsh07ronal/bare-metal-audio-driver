# Minimal Audio Device Driver
### Embedded development learning project — STM32F407 + QEMU

---

## What this is

A bare-metal audio driver in C that runs on a simulated STM32F407
microcontroller via QEMU. It produces a 440 Hz sine tone using:

- **I2S peripheral driver** — register-level setup, DMA control
- **Ping-pong audio buffers** — interrupt-safe double-buffering
- **Sine wave generator** — LUT-based, no floating-point at runtime
- **Unit tests** — run on your Mac with no hardware needed

---

## Prerequisites

```bash
brew install arm-none-eabi-gcc qemu
```

---

## Build & run

```bash
# Run unit tests (on your Mac, instant feedback)
make test

# Build firmware for ARM
make

# Launch in QEMU  (Ctrl-A then X to quit)
make qemu
```

---

## Project structure

```
src/
  main.c        Entry point, vector table, main loop
  i2s.c / .h    I2S peripheral driver (register writes, DMA)
  audio_buf.c   Ping-pong buffer — safe handoff between DMA and CPU
  sine_gen.c    440 Hz test tone generator

tests/
  test_audio_buf.c   9 tests: init, swap, pointer aliasing, isolation
  test_sine_gen.c    9 tests: frequency, phase, amplitude, continuity
  unity/unity.h      Minimal test framework (single header)

linker/
  stm32f407.ld       Memory map: 1 MB FLASH @ 0x08000000, 128 KB SRAM
```

---

## Key concepts practised

| Concept | Where |
|---|---|
| Memory-mapped I/O | `i2s.h` register macros |
| Volatile registers | `i2s.c` `wait_flag_clear()` |
| Linker scripts | `linker/stm32f407.ld` |
| Vector table | `main.c` `.isr_vector` section |
| .data / .bss startup | `Reset_Handler()` |
| Ping-pong DMA buffers | `audio_buf.c` |
| Fixed-point LUT | `sine_gen.c` |
| Cross-compilation | `Makefile` |

---

## Next steps

1. Add a real DMA ISR that calls `i2s_dma_callback()`
2. Replace the sine generator with WAV file playback
3. Add volume control via I2C to a WM8960 codec
4. Port to a real Nucleo-F407 board
