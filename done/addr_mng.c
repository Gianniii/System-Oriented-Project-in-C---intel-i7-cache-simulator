#include <stdio.h>
#include "addr.h"
#include "addr_mng.h"	
#include "error.h"
#include "inttypes.h"


int print_physical_address(FILE* where, const phy_addr_t* paddr){
	int nb = fprintf(where,"page num =0x%" PRIX32 "; offset=0x%", paddr->phy_page_num,
	                 paddr->page_offset);
	return nb;
}

int print_virtual_address(FILE* where, const virt_addr_t* vaddr){
	
	int nb = fprintf(where, "PGD=0x%" PRIX16 "; PUD=0x%" PRIX16 "; PMD=0x%" PRIX16 "; PTE=0x%" PRIX16
	               "; offset=0x%" PRIX16, vaddr->pgd_entry, vaddr->pud_entry, vaddr->pmd_entry,
	               vaddr->pte_entry, vaddr->page_offset);      
	return nb;
}

int init_virt_addr64(virt_addr_t * vaddr, uint64_t vaddr64){
	
}

int init_virt_addr(virt_addr_t * vaddr,
                   uint16_t pgd_entry,
                   uint16_t pud_entry, uint16_t pmd_entry,
                   uint16_t pte_entry, uint16_t page_offset){
	//arguments bitfield size must not be larger then corresponding bitfields
	if(pud_entry > (1 << (PUD_ENTRY + 1))) return ERR_BAD_PARAMETER;
	if(pgd_entry > (1 << (PGD_ENTRY + 1))) return ERR_BAD_PARAMETER;
	if(pmd_entry > (1 << (PMD_ENTRY + 1))) return ERR_BAD_PARAMETER;
	if(pte_entry > (1 << (PTE_ENTRY + 1))) return ERR_BAD_PARAMETER;
	if(page_offset > (1 << (PAGE_OFFSET + 1))) return ERR_BAD_PARAMETER;
	
	//assign values to  bitfields
	vaddr->pgd_entry = pgd_entry;
	vaddr->pud_entry = pud_entry;
	vaddr->pmd_entry = pmd_entry;
	vaddr->pte_entry = pte_entry;
	vaddr->page_offset = page_offset;
	
	return ERR_NONE;
}


int init_phy_addr(phy_addr_t* paddr, uint32_t page_begin, uint32_t page_offset){ 
    uint32_t page_num = page_begin >> page_offset;
	//arguments bitfield size must not be larger then corresponding bitfields
	if(page_num >= (1 << (PHY_PAGE_NUM + 1))) {return ERR_BAD_PARAMETER;}
	if(page_offset >= (1 << (PAGE_OFFSET + 1))) {return ERR_BAD_PARAMETER;}
	
	//assign values to bitfields
	paddr->phy_page_num = pagenum;
	paddr->page_offset = page_offset;//he sais page_offset is unit16_t ???
	return ERR_NONE;
}

// Helper methods

/**
 * @Brief Extracts bits from a u_int bitstring from start (included) to stop 
 * 		  (excluded)
 * @param sample The bitstring from which to extract
 * @param start bit index/placeholder from which to clip
 * @param stop bit index/placeholder till which to clip
 * @return the right justified extracted u_int bitstring
 */
u_int64_t extractBits64(u_int64_t sample, u_int8_t start, u_int8_t stop) {
    if (start < 0 || stop > 64 || start >= stop) {
        fprintf(stderr, "Bad Argument given to extractBits64\n");
        return ERR_BAD_PARAMETER; //TODO How to handle?
    }
    return (sample << (64 - stop)) >> (64 - stop + start);
} 