/* STM32F407 Linker Script */
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
  SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

SECTIONS
{
  .text :
  {
    KEEP(*(.isr_vector))
    *(.text*)
    *(.rodata*)
    _etext = .;
  } > FLASH

  .data :
  {
    _sdata = .;
    *(.data*)
    _edata = .;
  } > SRAM AT > FLASH

  _sidata = LOADADDR(.data);

  .bss :
  {
    _sbss = .;
    *(.bss*)
    *(COMMON)
    _ebss = .;
  } > SRAM

  _estack = ORIGIN(SRAM) + LENGTH(SRAM);
}
