OUTPUT_ARCH( "riscv" )
ENTRY( _entry )

SECTIONS
{
  . = PAGE_OFFSET + TEXT_OFFSET;
  /*
   * ensure that entry.S / _entry is at the expected location
   * (depends on the platform to build for)
   */
  .text.start : AT(ADDR(.text.start) - PAGE_OFFSET + PHYS_OFFSET) {
    PROVIDE(__start_kernel = .);
    *entry.o( .text )
  }

  .text : AT(ADDR(.text) - PAGE_OFFSET + PHYS_OFFSET) {
    *(.text .text.*)
    . = ALIGN(CONSTANT(MAXPAGESIZE));
    _trampoline = .;
    *(trampsec)
    . = ALIGN(CONSTANT(MAXPAGESIZE));
    ASSERT(. - _trampoline == 0x1000, "error: trampoline larger than one page");
    PROVIDE(__end_text = .);
  }

  .rodata : AT(ADDR(.rodata) - PAGE_OFFSET + PHYS_OFFSET) {
    . = ALIGN(CONSTANT(MAXPAGESIZE));
    PROVIDE(__start_rodata = .);
    *(.srodata .srodata.*) /* do not need to distinguish this from .rodata */
    . = ALIGN(16);
    *(.rodata .rodata.*)
  }

  .data : AT(ADDR(.data) - PAGE_OFFSET + PHYS_OFFSET) {
    . = ALIGN(CONSTANT(MAXPAGESIZE));
    PROVIDE(__start_data = .);
    *(.sdata .sdata.*) /* do not need to distinguish this from .data */
    . = ALIGN(16);
    *(.data .data.*)

    /* avoid writing to HTIF locations during bss clear */
    . = ALIGN(16);
    tohost = .;
    fromhost = .;

    PROVIDE(__end_data = .);
  }

  .bss : AT(ADDR(.bss) - PAGE_OFFSET + PHYS_OFFSET) {
    . = ALIGN(CONSTANT(MAXPAGESIZE));
    PROVIDE(__start_bss = .);
    *(.sbss .sbss.*) /* do not need to distinguish this from .bss */
    . = ALIGN(16);
    *(.bss .bss.*)
    . = ALIGN(8); /* keep __size_of_bss a multiple of 64 bit sizeof(size_t) */
    PROVIDE(__end_bss = .);
  }

  PROVIDE(__end_of_kernel = .);

  PROVIDE(__size_of_text = SIZEOF(.text));
  PROVIDE(__size_of_rodata = SIZEOF(.rodata));
  PROVIDE(__size_of_data = SIZEOF(.data));
  PROVIDE(__size_of_bss = SIZEOF(.bss));
}
