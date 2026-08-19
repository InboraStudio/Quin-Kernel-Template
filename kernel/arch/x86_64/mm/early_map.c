#include "early_map.h"

#include "arch/x86_64/boot/limine_requests.h"
#include "arch/x86_64/cpu/panic.h"
#include "kernel.h"
#include "lib/string.h"

#define EARLY_MMIO_VBASE 0xffffffffc0000000ULL
#define EARLY_MMIO_MAX_PAGES 8

#define PTE_PRESENT (1ULL << 0)
#define PTE_WRITABLE (1ULL << 1)
#define PTE_CACHE_DISABLE (1ULL << 4)
#define PTE_ADDR_MASK 0x000ffffffffff000ULL

#define PAGE_TABLE_ENTRIES 512

static uint64_t hhdm_offset;
static uint64_t kernel_phys_base;
static uint64_t kernel_virt_base;
static uint64_t next_mmio_vaddr = EARLY_MMIO_VBASE;

/* Backing storage for any PDPT/PD/PT pages this needs to create. Two
 * mappings (LAPIC, IOAPIC) that happen to land in the same 2MiB region
 * need at most 3 new table pages total; 8 is headroom, not a real limit
 * anyone should expect to approach. */
static uint8_t page_pool[EARLY_MMIO_MAX_PAGES][4096] ALIGNED(4096);
static unsigned page_pool_used;

void early_map_init(void) {
    hhdm_offset = boot_get_hhdm_offset();

    struct boot_kernel_address addr;
    if (!boot_get_kernel_address(&addr)) {
        panic("early_map: bootloader did not answer the executable-address request");
    }
    kernel_phys_base = addr.physical_base;
    kernel_virt_base = addr.virtual_base;
}

static uint64_t phys_to_virt(uint64_t phys) {
    return phys + hhdm_offset;
}

static uint64_t *table_at(uint64_t entry) {
    return (uint64_t *)(uintptr_t)phys_to_virt(entry & PTE_ADDR_MASK);
}

static uint64_t alloc_table_page_phys(void) {
    if (page_pool_used >= EARLY_MMIO_MAX_PAGES) {
        panic("early_map: page-table-page pool exhausted");
    }
    uint8_t *page = page_pool[page_pool_used++];
    memset(page, 0, 4096);
    return (uint64_t)(uintptr_t)page - kernel_virt_base + kernel_phys_base;
}

static uint64_t *ensure_next_level(uint64_t *table, unsigned index) {
    if (!(table[index] & PTE_PRESENT)) {
        table[index] = alloc_table_page_phys() | PTE_PRESENT | PTE_WRITABLE;
    }
    return table_at(table[index]);
}

void *early_map_mmio(uint64_t phys_addr) {
    uint64_t vaddr = next_mmio_vaddr;
    next_mmio_vaddr += 0x1000;

    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    uint64_t *pml4 = (uint64_t *)(uintptr_t)phys_to_virt(cr3 & PTE_ADDR_MASK);

    unsigned pml4_i = (unsigned)((vaddr >> 39) & (PAGE_TABLE_ENTRIES - 1));
    unsigned pdpt_i = (unsigned)((vaddr >> 30) & (PAGE_TABLE_ENTRIES - 1));
    unsigned pd_i = (unsigned)((vaddr >> 21) & (PAGE_TABLE_ENTRIES - 1));
    unsigned pt_i = (unsigned)((vaddr >> 12) & (PAGE_TABLE_ENTRIES - 1));

    uint64_t *pdpt = ensure_next_level(pml4, pml4_i);
    uint64_t *pd = ensure_next_level(pdpt, pdpt_i);
    uint64_t *pt = ensure_next_level(pd, pd_i);

    pt[pt_i] = (phys_addr & PTE_ADDR_MASK) | PTE_PRESENT | PTE_WRITABLE | PTE_CACHE_DISABLE;
    __asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");

    return (void *)(uintptr_t)vaddr;
}
