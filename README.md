# Bare-Metal Audio Driver — STM32F407 + QEMU

A bare-metal audio driver written in C, targeting the STM32F407 Cortex-M4 microcontroller, simulated in QEMU. Built from scratch without any HAL, OS, or standard library — every register write is explicit.

---

## Architecture

```
PDM microphone (simulated)
        │
        │  1-bit PDM stream @ 3.072 MHz
        ▼
┌───────────────────────────┐
│   Stage 1: CIC Filter     │  3 integrators + 3 combs
│   Decimation x64          │  Add/subtract only — no multiply
│   Runs at PDM bit rate    │
└───────────┬───────────────┘
            │  one sample per 64 bits @ 48 kHz
            ▼
┌───────────────────────────┐
│   Stage 2: FIR Filter     │  16-tap, Q15 fixed-point
│   Low-pass ~19 kHz        │  Ring buffer delay line
│   Runs at PCM rate        │
└───────────┬───────────────┘
            │  clean 16-bit PCM @ 48 kHz
            ▼
┌───────────────────────────┐
│   Ping-Pong Buffer        │  Buffer A plays while B fills
│   Interrupt-safe handoff  │  No mutex needed — pointer swap
└───────────┬───────────────┘
            │
            ▼
┌───────────────────────────┐
│   I2S Peripheral Driver   │  Register-level setup
│   DMA circular mode       │  CPU free during transfer
│   ISR on buffer complete  │  ~microseconds of CPU time
└───────────┬───────────────┘
            │
            ▼
        Speaker / DAC
```

---

## Key Technical Decisions

**Why DMA instead of interrupts for sample transfer?**
Interrupt-driven I2S would fire 48,000 times per second — one interrupt per sample. DMA transfers an entire buffer (512 samples) and fires one interrupt at the end. CPU overhead drops by ~512x.

**Why ping-pong buffers?**
DMA needs a stable buffer to read from while the CPU fills the next one. Two buffers — one active (DMA reading), one inactive (CPU writing) — means neither side ever waits for the other. No lock needed; a single pointer swap is atomic.

**Why CIC for decimation, not a direct FIR?**
A CIC filter uses only addition and subtraction — no multiplications. Running at 3 MHz PDM rate on a Cortex-M4, a multiply-heavy FIR would consume the entire CPU. CIC costs almost nothing. The FIR runs at the decimated 48 kHz rate where multiplications are affordable.

**Why Q15 fixed-point instead of float?**
The STM32F407 has an FPU, but the PDM filter is designed to also run on Cortex-M0/M3 cores which do not. Q15 integer arithmetic is portable, deterministic in timing, and fast on any ARM core.

**Why no libc / no OS?**
This is a driver, not an application. Every dependency adds unpredictable latency. Bare-metal code gives deterministic timing — critical for real-time audio where a 1 ms glitch is audible.

---

## Project Structure

```
src/
  main.c          Vector table, .data/.bss init, main loop
  i2s.c / .h      I2S peripheral driver — register writes, DMA control
  audio_buf.c     Ping-pong buffer — safe DMA/CPU handoff
  sine_gen.c      440 Hz sine generator — Q15 LUT, no runtime float
  pdm_filter.c    PDM-to-PCM decimation filter — CIC + FIR pipeline
  pdm_filter.h    Public API and PDMFilter struct

tests/
  test_audio_buf.c    9 tests: init, swap, pointer aliasing, isolation
  test_sine_gen.c     9 tests: frequency, phase, amplitude, continuity
  test_pdm_filter.c   9 tests: CIC/FIR correctness, sine roundtrip, dual-channel
  unity/              Unity test framework (ThrowTheSwitch)

linker/
  stm32f407.ld    Memory map: 1 MB Flash @ 0x08000000, 128 KB SRAM
```

---

## Build & Run

**Prerequisites**
```bash
brew install arm-none-eabi-gcc qemu
```

**Unit tests — run natively on Mac (no hardware needed)**
```bash
make test
```
Expected: 27 tests, 0 failures across 3 suites.

**Build firmware for ARM**
```bash
make
```

**Simulate in QEMU**
```bash
make qemu
# Press Ctrl-A then X to quit
```

---

## Test Coverage

| Suite | Tests | What is verified |
|---|---|---|
| `test_audio_buf` | 9 | Buffer init, pointer swap, read/write never alias, isolation |
| `test_sine_gen` | 9 | Frequency, phase continuity, amplitude, stereo channels match |
| `test_pdm_filter` | 9 | CIC/FIR pipeline, silence near zero, sine roundtrip, dual-channel independence, int16 clamping |
| **Total** | **27** | |

---

## Concepts Demonstrated

| Concept | Location |
|---|---|
| Memory-mapped I/O | `src/i2s.h` register macros |
| Volatile registers | `src/i2s.c` `wait_flag_clear()` |
| DMA circular mode | `src/i2s.c` `i2s_start()` |
| Ping-pong double buffering | `src/audio_buf.c` |
| Vector table in C | `src/main.c` `.isr_vector` section |
| .data / .bss startup | `src/main.c` `Reset_Handler()` |
| CIC decimation filter | `src/pdm_filter.c` `cic_process_bit()` |
| Q15 FIR filter | `src/pdm_filter.c` `fir_process()` |
| Sigma-delta modulation | `tests/test_pdm_filter.c` `pcm_to_pdm_byte()` |
| Cross-compilation | `Makefile` — `arm-none-eabi-gcc` |
| Linker script | `linker/stm32f407.ld` |

---

## Toolchain

- `arm-none-eabi-gcc` — cross-compiler for ARM Cortex-M4
- `QEMU` — `netduinoplus2` machine, STM32F407 simulation
- `gcc` (host) — native Mac build for unit tests
- `Unity` — lightweight C test framework
