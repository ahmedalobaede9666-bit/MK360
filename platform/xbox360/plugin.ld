/* System DLL: fixed address in system image space, separate from title memory.
 * This experimental packer does not yet emit base relocations.
 */
OUTPUT_FORMAT("elf32-powerpc")
OUTPUT_ARCH(powerpc:common)
ENTRY(DllMain)
SECTIONS {
    . = 0x81F00000 + 0x1000;
    .rdata : { *(.rodata .rodata.* .sdata2 .sdata2.*) }
    . = ALIGN(0x10000);
    .text : {
        *(.text.xsdk_thunk_*)
        *(.text.mk360_tramp)
        *(.text .text.*)
    }
    . = ALIGN(0x10000);
    .data : { *(.data .data.* .sdata .sdata.*) }
    .bss : { *(.sbss .sbss.* .bss .bss.* COMMON) }
    ASSERT(. < 0x82000000, "MK360 overlaps title memory")
    .xsdk.imports 0 : { KEEP(*(.xsdk.imports)) }
    /DISCARD/ : { *(.comment .note.* .gnu.attributes .eh_frame .eh_frame.*) }
}
