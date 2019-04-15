
#include "commands.h"
#include "error.h"
#include "addr_mng.h"

static inline pte_t read_page_entry(const pte_t * start, pte_t page_start, uint16_t index) { 
    return start[(page_start / 4) + index]; 
}

int page_walk(const void* mem_space, const virt_addr_t* vaddr, phy_addr_t* paddr) {
    M_REQUIRE_NON_NULL(mem_space);
    M_REQUIRE_NON_NULL(vaddr);
    M_REQUIRE_NON_NULL(paddr);

    const pte_t* start = (const pte_t *) mem_space;

    pte_t pud_start = read_page_entry(start, 0, vaddr->pgd_entry);
    pte_t pmd_start = read_page_entry(start, pud_start, vaddr->pud_entry);
    pte_t pte_start = read_page_entry(start, pmd_start, vaddr->pmd_entry);
    u_int32_t pte_value = read_page_entry(start, pte_start, vaddr->pte_entry);

    // TODO @Michael Should we mask and OR the bits in Physical page number and offset?
    // And should this be done in addr_mng.c?
    // UPDATE This should work. Check it!
    init_phy_addr(paddr, pte_value, vaddr->page_offset);

    return ERR_NONE;
}
