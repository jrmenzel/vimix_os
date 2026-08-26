/* SPDX-License-Identifier: MIT */
/* set architecture when calling the linker with -m ARCH */
ENTRY( _start )

SECTIONS
{
 . = USER_TEXT_START;
 
  .text : {
    *(.text .text.*)
  }

  .rodata : ALIGN(CONSTANT(MAXPAGESIZE)) {
    . = ALIGN(16);
    *(.srodata .srodata.*) /* do not need to distinguish this from .rodata */
    . = ALIGN(16);
    *(.rodata .rodata.*)
  }

  .data : ALIGN(CONSTANT(MAXPAGESIZE)) {
    *(.got .got.*)
    *(.data.rel.ro .data.rel.ro.*)
    *(.sdata .sdata.*) /* do not need to distinguish this from .data */
    . = ALIGN(16);
    *(.data .data.*)
  }

  .bss : {
    . = ALIGN(16);
    *(.sbss .sbss.*) /* do not need to distinguish this from .bss */
    . = ALIGN(16);
    *(.bss .bss.*)
  }

  PROVIDE(end = .);
}
