#include "vmm.h"

#include "arch/x86_64/boot/limine_requests.h"
#include "arch/x86_64/cpu/isr.h"
#include "arch/x86_64/cpu/panic.h"
#include "kernel.h"
#include "mm/pmm.h"

#define PAGE_TABLE_ENTRIES 512

#define PTE_PRESENT (1ULL << 0)
#define PTE_WRITABLE (1ULL << 1)
#define PTE_USER (1ULL << 2)
#define PTE_CACHE_DISABLE (1ULL << 4)
#define PTE_NO_EXECUTE (1ULL << 63)
#define PTE_ADDR_MASK 0x000ffffffffff000ULL

/* Two dedicated virtual-address regions this kernel bump-allocates from,
 * chosen to sit well clear of KERNEL_VMA (kernel/arch/x86_64/linker.ld)
 * and of each other. See docs/ARCHITECTURE.md's memory layout table. */
#define VMM_DYNAMIC_VBASE 0xffffffffb0000000ULL
#define VMM_MMIO_VBASE 0xffffffffc0000000ULL

#define MAX_LAZY_REGIONS 8

struct lazy_region {
    uint64_t start;
    uint64_t end;
    uint32_t flags;
    bool used;
};

static uint64_t hhdm_offset;
static uint64_t dynamic_next_vaddr = VMM_DYNAMIC_VBASE;
static uint64_t mmio_next_vaddr = VMM_MMIO_VBASE;
static struct lazy_region lazy_regions[MAX_LAZY_REGIONS];

static uint64_t *phys_to_virt_table(uint64_t phys) {
    return (uint64_t *)(uintptr_t)(phys + hhdm_offset);
}

static uint64_t *current_pml4(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return phys_to_virt_table(cr3 & PTE_ADDR_MASK);
}

static uint64_t translate_flags(uint32_t flags) {
    uint64_t pte_flags = PTE_PRESENT;
    if (flags & VMM_WRITABLE) {
        pte_flags |= PTE_WRITABLE;
    }
    if (flags & VMM_USER) {
        pte_flags |= PTE_USER;
    }
    if (flags & VMM_NO_CACHE) {
        pte_flags |= PTE_CACHE_DISABLE;
    }
    if (flags & VMM_NO_EXECUTE) {
        pte_flags |= PTE_NO_EXECUTE;
    }
    return pte_flags;
}

static void page_table_indices(uint64_t virt, unsigned *pml4_i, unsigned *pdpt_i, unsigned *pd_i,
                               unsigned *pt_i) {
    *pml4_i = (unsigned)((virt >> 39) & (PAGE_TABLE_ENTRIES - 1));
    *pdpt_i = (unsigned)((virt >> 30) & (PAGE_TABLE_ENTRIES - 1));
    *pd_i = (unsigned)((virt >> 21) & (PAGE_TABLE_ENTRIES - 1));
    *pt_i = (unsigned)((virt >> 12) & (PAGE_TABLE_ENTRIES - 1));
}

/* Present intermediate entries are always Writable and User so that a
 * leaf's own Writable/User/NX flags are what actually govern access --
 * the CPU ANDs permissions across all four levels, so a restrictive
 * intermediate entry silently overrides a more permissive leaf. This is
 * not a hypothetical: an earlier version left User unset here, and every
 * mapping stayed effectively supervisor-only regardless of the leaf's
 * own flags, which nothing caught until Phase 5's ring-3 demo tried to
 * execute from a page whose leaf PTE genuinely did say User=1 -- see
 * docs/ROADMAP.md. */
static uint64_t *ensure_next_level(uint64_t *table, unsigned index) {
    if (!(table[index] & PTE_PRESENT)) {
        uint64_t new_table_phys = pmm_alloc_frame();
        if (new_table_phys == 0) {
            return NULL;
        }
        table[index] = new_table_phys | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    }
    return phys_to_virt_table(table[index] & PTE_ADDR_MASK);
}

bool vmm_map(uint64_t virt, uint64_t phys, uint32_t flags) {
    unsigned pml4_i, pdpt_i, pd_i, pt_i;
    page_table_indices(virt, &pml4_i, &pdpt_i, &pd_i, &pt_i);

    uint64_t *pdpt = ensure_next_level(current_pml4(), pml4_i);
    uint64_t *pd = pdpt != NULL ? ensure_next_level(pdpt, pdpt_i) : NULL;
    uint64_t *pt = pd != NULL ? ensure_next_level(pd, pd_i) : NULL;
    if (pt == NULL) {
        return false;
    }

    pt[pt_i] = (phys & PTE_ADDR_MASK) | translate_flags(flags);
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
    return true;
}

void vmm_unmap(uint64_t virt) {
    unsigned pml4_i, pdpt_i, pd_i, pt_i;
    page_table_indices(virt, &pml4_i, &pdpt_i, &pd_i, &pt_i);

    uint64_t *pml4_table = current_pml4();
    if (!(pml4_table[pml4_i] & PTE_PRESENT)) {
        return;
    }
    uint64_t *pdpt = phys_to_virt_table(pml4_table[pml4_i] & PTE_ADDR_MASK);
    if (!(pdpt[pdpt_i] & PTE_PRESENT)) {
        return;
    }
    uint64_t *pd = phys_to_virt_table(pdpt[pdpt_i] & PTE_ADDR_MASK);
    if (!(pd[pd_i] & PTE_PRESENT)) {
        return;
    }
    uint64_t *pt = phys_to_virt_table(pd[pd_i] & PTE_ADDR_MASK);

    pt[pt_i] = 0;
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

bool vmm_register_lazy_region(uint64_t start, uint64_t end, uint32_t flags) {
    for (int i = 0; i < MAX_LAZY_REGIONS; i++) {
        if (!lazy_regions[i].used) {
            lazy_regions[i].start = start;
            lazy_regions[i].end = end;
            lazy_regions[i].flags = flags;
            lazy_regions[i].used = true;
            return true;
        }
    }
    return false;
}

bool vmm_handle_page_fault(uint64_t fault_addr, uint64_t error_code) {
    /* SDM Vol. 3A, 4.7 "Page-Fault Exceptions": error code bit 0 clear
     * means the fault was caused by a not-present page, not a
     * permission violation. Only that case is ever a lazy-mapping
     * opportunity -- a permission violation (writing to a read-only
     * page, executing NX memory) is always a real bug. */
    if ((error_code & 0x1) != 0) {
        return false;
    }

    uint64_t page = align_down(fault_addr, PAGE_SIZE);
    for (int i = 0; i < MAX_LAZY_REGIONS; i++) {
        if (lazy_regions[i].used && page >= lazy_regions[i].start && page < lazy_regions[i].end) {
            uint64_t frame = pmm_alloc_frame();
            if (frame == 0) {
                return false;
            }
            return vmm_map(page, frame, lazy_regions[i].flags);
        }
    }
    return false;
}

static void page_fault_isr(struct interrupt_frame *frame) {
    uint64_t cr2;
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
    if (!vmm_handle_page_fault(cr2, frame->error_code)) {
        panic_exception(frame);
    }
}

void vmm_init(void) {
    hhdm_offset = boot_get_hhdm_offset();
    interrupt_register_handler(14, page_fault_isr);
}

void *vmm_alloc_guarded(uint64_t page_count, uint32_t flags) {
    uint64_t region_start = dynamic_next_vaddr + PAGE_SIZE;
    uint64_t region_end = region_start + page_count * PAGE_SIZE;
    dynamic_next_vaddr = region_end + PAGE_SIZE;

    /* A failure partway through leaks the frames already mapped in this
     * loop -- acceptable for a template with no frame-reclaiming
     * "unmap this whole range" helper yet, but a real allocator built on
     * top of this should track and roll back partial failures. */
    for (uint64_t i = 0; i < page_count; i++) {
        uint64_t frame = pmm_alloc_frame();
        if (frame == 0) {
            return NULL;
        }
        if (!vmm_map(region_start + i * PAGE_SIZE, frame, flags)) {
            pmm_free_frame(frame);
            return NULL;
        }
    }
    return (void *)(uintptr_t)region_start;
}

void *vmm_map_mmio(uint64_t phys_addr) {
    uint64_t virt = mmio_next_vaddr;
    mmio_next_vaddr += PAGE_SIZE;

    if (!vmm_map(virt, phys_addr, VMM_WRITABLE | VMM_NO_CACHE | VMM_NO_EXECUTE)) {
        panic("vmm_map_mmio: failed to map phys 0x%lx", phys_addr);
    }
    return (void *)(uintptr_t)virt;
}
