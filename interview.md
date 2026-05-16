# Interview Q&A — Bare-Metal Audio Driver

Use this file to prepare for technical screens at Qualcomm, ARM, Apple, and similar companies.
Read the question, cover the answer, try to answer from memory, then check.

---

## DMA & Interrupts

**Q: Why did you use DMA instead of a CPU-driven loop to transfer audio samples?**

A: An interrupt-driven approach would fire 48,000 times per second — once per sample at 48 kHz. That is enormous ISR overhead. DMA transfers an entire buffer of 512 samples autonomously and fires a single interrupt when done. The CPU is free for the entire duration of the transfer. Overhead drops by roughly 512x.

---

**Q: What is a DMA controller — does it have its own CPU?**

A: No. A DMA controller is a dedicated hardware block on the chip with a fixed function: copy N bytes from address A to address B. It has source/destination address registers and a counter, but no instruction decoder and no program counter. You configure it by writing to its registers, then it operates independently on the bus using hardwired silicon logic. The CPU and DMA share the same memory bus and can both access RAM, but they operate independently.

---

**Q: What happens if your ISR takes too long?**

A: The DMA exhausts the current buffer and starts reading the next one before the CPU has finished filling it. You get a buffer underrun — the DAC plays stale or zeroed data, which sounds like a click or dropout. In a real product you would track underrun events with a counter and surface them as a health metric.

---

**Q: What is the difference between DMA circular mode and normal mode?**

A: In normal mode, DMA transfers N bytes then stops — you must re-arm it manually in the ISR. In circular mode, it automatically wraps back to the start of the buffer when it reaches the end and continues without any CPU intervention. Circular mode is standard for continuous audio streaming because it removes the re-arm window where an underrun could happen.

---

## Ping-Pong Buffers

**Q: Why do you need two buffers? Why not one?**

A: DMA reads from the buffer continuously. If you had one buffer, you would need to pause DMA while the CPU refills it — that pause would cause a glitch. With two buffers (ping and pong), DMA reads from buffer A while the CPU writes to buffer B. When DMA finishes A, you swap: DMA starts on B (already filled), CPU starts refilling A. No pause, no glitch.

---

**Q: How do you prevent the CPU and DMA from accessing the same buffer at the same time?**

A: The pointer swap in the ISR is a single atomic operation — the CPU updates one pointer variable. On a Cortex-M4, a 32-bit aligned store is atomic by hardware guarantee. So the moment the ISR swaps, DMA is pointed at the new buffer and the CPU works on the old one. There is no window where both access the same buffer.

---

**Q: What is a buffer underrun and how would you detect it?**

A: An underrun happens when the audio consumer (DMA / DAC) runs out of data before the producer (CPU) has refilled the buffer. Detection: in the ISR, check whether the inactive buffer has been marked as "ready" by the CPU. If not, the CPU fell behind — increment an underrun counter. You can expose this counter over a debug UART or via a register that a host tool reads.

---

## PDM Filter

**Q: What is PDM and where does it come from?**

A: PDM stands for Pulse Density Modulation. MEMS microphones output a single 1-bit data line clocked at a few MHz. The density of 1s in the stream encodes the audio amplitude — lots of 1s means positive pressure, lots of 0s means negative pressure. It is the raw output format of nearly every digital MEMS microphone (Knowles, ST, TDK).

---

**Q: What is a CIC filter and why is it used for PDM decimation?**

A: CIC stands for Cascaded Integrator-Comb. It is a decimation filter made entirely of addition and subtraction — no multiplications. This is critical because PDM arrives at 3 MHz. Running a multiply-heavy FIR at that rate would saturate the CPU. CIC does the heavy downsampling cheaply. The output rate is PDM rate / decimation factor — in this project 3.072 MHz / 64 = 48 kHz.

---

**Q: What are the two stages in your PDM filter pipeline?**

A: Stage 1 is a CIC filter — 3 integrator stages running at PDM bit rate, followed by 3 comb (differencer) stages running at the decimated rate. It downsamples by 64x using only add/subtract. Stage 2 is a 16-tap FIR low-pass filter running at 48 kHz. It cleans up the rough frequency response left by the CIC using Q15 fixed-point multiply-accumulate. The FIR uses a ring buffer delay line and Hamming-windowed sinc coefficients.

---

**Q: Why use Q15 fixed-point instead of float for the FIR?**

A: Two reasons. First, the STM32F407 has an FPU, but Cortex-M0 and M3 cores do not — Q15 makes the code portable across the entire ARM Cortex-M family. Second, fixed-point arithmetic has deterministic timing — no floating-point exceptions, no denormal slowdowns. In real-time audio, predictable timing is more important than floating-point convenience.

---

**Q: How does a Q15 ring buffer work?**

A: The delay line is a fixed-size array. A write index (fir_idx) points to the current position. Each new sample overwrites the oldest sample at fir_idx. To read sample K steps back, you compute index = (fir_idx + TAPS - K) % TAPS. This wraps around the array without any data movement. After writing, fir_idx advances by 1 modulo TAPS.

---

**Q: How did you test the PDM filter without real hardware?**

A: I wrote a sigma-delta modulator in the test file — a first-order feedback loop that converts a 16-bit PCM sample into 8 bits of PDM. This is the same algorithm a real MEMS mic uses internally. I fed a 1 kHz sine wave through the modulator, piped the PDM bytes into the filter, and verified the output was non-zero, oscillating between positive and negative values. The test does not check exact values because group delay shifts the timing — it checks that the signal is alive and has the right sign pattern.

---

## Memory & Bare-Metal

**Q: What happens between power-on and main() in your firmware?**

A: The CPU fetches the reset vector from address 0x08000000 (Flash). That points to Reset_Handler. Reset_Handler copies the .data section from Flash to SRAM (initialized globals), zeroes the .bss section (uninitialized globals), then calls main(). Without this, global variables would have garbage values. This is normally done by the C runtime (crt0), but in bare-metal you write it yourself.

---

**Q: What is a linker script and what does yours do?**

A: A linker script tells the linker where to place each section of code and data in the target's memory map. The STM32F407 has 1 MB of Flash starting at 0x08000000 and 128 KB of SRAM starting at 0x20000000. The linker script places .text (code) and .rodata (constants) in Flash, and .data and .bss in SRAM. It also defines the symbols that Reset_Handler uses to know where to copy .data from and where .bss ends.

---

**Q: Why is the volatile keyword important in a driver?**

A: Without volatile, the compiler assumes a variable's value can only change through the code it can see. It may cache a register value in a CPU register and never re-read from memory. A hardware peripheral register can change at any time — DMA sets a flag, an interrupt fires, the hardware signals completion. volatile tells the compiler: do not cache this, always read from the actual memory address.

---

## System Design

**Q: What would you change to support a second audio channel (stereo input)?**

A: Declare a second PDMFilter instance. Each instance holds its own integrator, comb, and FIR delay line state — there is no shared state. Call pdm_filter_process_byte() independently for each channel's byte stream. The ping-pong buffer already stores interleaved stereo (L, R, L, R...) so no buffer changes are needed. The I2S driver would need to be configured for stereo (I2S_CHLEN full-word mode).

---

**Q: How would you add an equalizer to this pipeline?**

A: After the FIR output (which is clean 16-bit PCM at 48 kHz), insert a chain of biquad IIR filters — one biquad section per frequency band. Each biquad is: y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]. In Q15/Q23 fixed-point this is five multiply-accumulate operations per sample per band. At 48 kHz with 5 bands, that is 240,000 MACs per second — well within Cortex-M4 budget.

---

**Q: What is the difference between this project and a production driver like the STM32 HAL?**

A: The HAL wraps every peripheral in a handle struct (SAI_HandleTypeDef etc.), uses weak callback functions so application code can override ISR behaviour without touching the driver, and supports multiple transfer modes (polling, interrupt, DMA) through a unified API. It also handles error states, timeouts, and re-entrancy. My driver is a direct register-write implementation — it does one specific thing efficiently and is easier to reason about, but less flexible than a HAL.

---

**Q: What would you add to make this production-ready?**

A: Several things:
1. Error handling — timeout in wait loops, return error codes, not silent hang
2. Clock configuration — currently assumes the PLL is already set up; a real driver initialises RCC
3. RTOS integration — move the refill loop into a FreeRTOS task, use a semaphore signalled from the ISR instead of a polling flag
4. Power management — gate the I2S clock when idle, use DMA low-power mode
5. Underrun/overrun counters — surfaced over debug interface for production diagnostics

---

## Behavioural

**Q: Walk me through the most interesting bug you fixed in this project.**

A: The first version of pdm_filter_process_byte had incorrect readiness detection. It tried to detect when a new CIC sample was ready by checking if bit_count had just wrapped to zero AND the current bit was zero — a coincidence check that fired at the wrong time. The fix was to refactor cic_process_bit into a function that returns 1 when output is ready, stores the result in f->cic_out, and returns 0 otherwise. The outer loop then calls fir_process only when cic_process_bit returns 1. Clean state machine, no coincidence logic.

---

**Q: Why did you build this project?**

A: I wanted to understand what actually happens between a microphone and a speaker at the hardware level — below the OS, below any library. Audio is a demanding real-time domain: you cannot tolerate jitter, you cannot block, you have to move data at a fixed rate forever. Building a driver that does that on bare metal forces you to understand DMA, interrupts, fixed-point arithmetic, and memory layout in a way that reading about them does not.

---

**Q: What would you build next to extend this project?**

A: Two things. First, swap the sine generator for a real PDM microphone stream — either from a real Nucleo board or a captured PDM trace file — so the filter processes real audio. Second, add a biquad EQ stage between the PDM filter output and the I2S output, which would demonstrate a complete capture-process-playback pipeline in bare-metal C. Longer term, port the driver into a FreeRTOS task structure to show the same concepts in a production-representative environment.
