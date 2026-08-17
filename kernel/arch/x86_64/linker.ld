OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(kernel_entry)

/* Higher-half kernel at -2GiB, matching -mcmodel=kernel: the compiler
 * assumes every symbol lives in the top (or bottom) 2GiB of the address
 * space and encodes references accordingly. See docs/ARCHITECTURE.md for
 * the full layout table and docs/FAQ.md for why this split. */
KERNEL_VMA = 0xffffffff80000000;

PHDRS
{
    text   PT_LOAD FLAGS((1 << 0) | (1 << 2)); /* R+X, never W */
    rodata PT_LOAD FLAGS((1 << 2));            /* R only */
    data   PT_LOAD FLAGS((1 << 1) | (1 << 2)); /* R+W, never X */
}

SECTIONS
{
    . = KERNEL_VMA;

    __kernel_start = .;

    .text : {
        *(.text .text.*)
    } :text

    . = ALIGN(CONSTANT(MAXPAGESIZE));
    __rodata_start = .;

    .rodata : {
        *(.rodata .rodata.*)
    } :rodata

    . = ALIGN(CONSTANT(MAXPAGESIZE));
    __data_start = .;

    .data : {
        *(.data .data.*)

        /* Limine request structs must be writable: the bootloader patches
         * the .response field of each one in place before jumping to us.
         * KEEP prevents an eventual --gc-sections pass from dropping
         * structs that nothing in the kernel appears to reference. */
        KEEP(*(.limine_requests_start_marker))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end_marker))
    } :data

    .bss : {
        *(COMMON)
        *(.bss .bss.*)
    } :data

    __kernel_end = .;

    /DISCARD/ : {
        *(.eh_frame*)
        *(.note .note.*)
        *(.comment)
    }
}
