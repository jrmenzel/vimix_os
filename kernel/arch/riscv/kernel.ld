OUTPUT_ARCH( "riscv" )
ENTRY( _entry )

SECTIONS
{
  /*
   * ensure that entry.S / _entry is at the expected location
   * (depends on the platform to build for)
   */
  .text.start KERNBASE : {
    PROVIDE(start_of_kernel = .);
    *entry.o( .text )
  }

  .text : {
    *(.text .text.*)
    . = ALIGN(0x1000);
    _trampoline = .;
    *(trampsec)
    . = ALIGN(0x1000);
    ASSERT(. - _trampoline == 0x1000, "error: trampoline larger than one page");
    PROVIDE(end_of_text = .);
  }

    /* keep m mode data out of BSS */
  M_MODE : {
    *(.M_MODE)
  }

  .rodata : ALIGN(CONSTANT(MAXPAGESIZE)) {
    . = ALIGN(16);
    *(.srodata .srodata.*) /* do not need to distinguish this from .rodata */
    . = ALIGN(16);
    *(.rodata .rodata.*)
  }

  .data : ALIGN(CONSTANT(MAXPAGESIZE)) {
    . = ALIGN(16);
    *(.sdata .sdata.*) /* do not need to distinguish this from .data */
    . = ALIGN(16);
    *(.data .data.*)

    /* avoid writing to HTIF locations during bss clear */
    . = ALIGN(16);
    tohost = .;
    fromhost = .;
  }

  .bss : ALIGN(CONSTANT(MAXPAGESIZE)) {
    . = ALIGN(16);
    PROVIDE(bss_start = .);
    *(.sbss .sbss.*) /* do not need to distinguish this from .bss */
    . = ALIGN(16);
    *(.bss .bss.*)
    PROVIDE(bss_end = .);
  }

  /* keep the kernel stack out of .bss to allow clearing the .bss
     early from C instead of from entry.S */
  STACK : {
    *(.STACK)
  }

  PROVIDE(end_of_kernel = .);

  PROVIDE(size_of_text = SIZEOF(.text));
  PROVIDE(size_of_rodata = SIZEOF(.rodata));
  PROVIDE(size_of_data = SIZEOF(.data));
  PROVIDE(size_of_bss = SIZEOF(.bss));
}
