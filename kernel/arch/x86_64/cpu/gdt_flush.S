/* Loading a new GDT doesn't take effect on CS until CS itself is
 * reloaded, and CS can't be assigned with a plain `mov` -- the only way
 * to change it is a control transfer that specifies a new selector,
 * which in 64-bit mode means a far return. See OSDev Wiki, "GDT Tutorial".
 *
 * void gdt_flush(uint64_t gdtr_addr, uint16_t code_selector, uint16_t data_selector);
 *   rdi = gdtr_addr, si = code_selector, dx = data_selector (System V ABI)
 */

.text
.global gdt_flush
.type gdt_flush, @function
gdt_flush:
    lgdt (%rdi)

    movzwq %dx, %rax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    movzwq %si, %rax
    lea 1f(%rip), %rcx
    pushq %rax
    pushq %rcx
    lretq
1:
    ret
