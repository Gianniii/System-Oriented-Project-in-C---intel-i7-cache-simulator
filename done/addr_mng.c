#include <stdio.h>
#include "addr.h"
#include "addr_mng.h"	
#include "error.h"
#include "inttypes.h"

// Week 4 

//=========================================================================
// Helper methods
//=========================================================================

/**
 * @Brief Extracts bits from a u_int bitstring from start (included) to stop 
 * 		  (excluded)
 * @param sample The bitstring from which to extract
 * @param start bit index/placeholder from which to clip
 * @param stop bit index/placeholder till which to clip
 * @return the right justified extracted u_int bitstring
 */
uint64_t extractBits64(uint64_t sample, const uint8_t start, const uint8_t stop) {
    if (start < 0 || stop > 64 || start >= stop) {
        fprintf(stderr, "Bad Argument given to extractBits64\n");
        return ERR_BAD_PARAMETER; //TODO How to handle?
    }
    return (sample << (64 - stop)) >> (64 - stop + start);
}

//=========================================================================
// "Main" methods
//=========================================================================

int init_virt_addr(virt_addr_t * vaddr,
                   uint16_t pgd_entry,
                   uint16_t pud_entry, uint16_t pmd_entry,
                   uint16_t pte_entry, uint16_t page_offset){
	//arguments bitfield size must not be larger then corresponding bitfields
	if(pud_entry >= ((1 << PUD_ENTRY) + 1)) return ERR_BAD_PARAMETER;
	if(pgd_entry >= ((1 << PGD_ENTRY) + 1)) return ERR_BAD_PARAMETER;
	if(pmd_entry >= ((1 << PMD_ENTRY) + 1)) return ERR_BAD_PARAMETER;
	if(pte_entry >= ((1 << PTE_ENTRY) + 1)) return ERR_BAD_PARAMETER;
	if(page_offset >= ((1 << PAGE_OFFSET) + 1)) return ERR_BAD_PARAMETER;
	
	
	//assign values to  bitfields
	vaddr->reserved = (uint16_t)0; //reserved bits always set to 0
	vaddr->pgd_entry = pgd_entry;
	vaddr->pud_entry = pud_entry;
	vaddr->pmd_entry = pmd_entry;
	vaddr->pte_entry = pte_entry;
	vaddr->page_offset = page_offset;
	
	return ERR_NONE;
}

int init_virt_addr64(virt_addr_t * vaddr, uint64_t vaddr64){
	return init_virt_addr(vaddr, 
			extractBits64(vaddr64, 39, 48),
			extractBits64(vaddr64, 30, 39),
			extractBits64(vaddr64, 21, 30),
			extractBits64(vaddr64, 12, 21),
			extractBits64(vaddr64, 0, 12)
		);
}

int init_phy_addr(phy_addr_t* paddr, uint32_t page_begin, uint32_t page_offset){ 
    uint32_t page_num = page_begin >> PAGE_OFFSET;
   
    
	//arguments bitfield size must not be larger then corresponding bitfields
	if(page_num >= ((1 << PHY_PAGE_NUM) + 1)){return ERR_BAD_PARAMETER;}
	if(page_offset >= ((1 << PAGE_OFFSET) + 1)){return ERR_BAD_PARAMETER;}
	
	//assign values to bitfields
	paddr->phy_page_num = page_num;
	paddr->page_offset = (uint16_t)page_offset;//page_offset is unit16_t
	return ERR_NONE;
}

uint64_t virt_addr_t_to_uint64_t(const virt_addr_t * vaddr) {
	return virt_addr_t_to_virtual_page_number(vaddr) << PAGE_OFFSET
			| vaddr->page_offset
		;
}

uint64_t virt_addr_t_to_virtual_page_number(const virt_addr_t * vaddr) {
	return (uint64_t)(vaddr->pgd_entry << (PTE_ENTRY + PMD_ENTRY + PUD_ENTRY)
			| vaddr->pud_entry << (PTE_ENTRY + PMD_ENTRY)
			| vaddr->pmd_entry << PTE_ENTRY
			| vaddr->pte_entry);
		
}

int print_virtual_address(FILE* where, const virt_addr_t* vaddr){
	
	int nb = fprintf(where, "PGD=0x%" PRIX16 "; PUD=0x%" PRIX16 "; PMD=0x%" PRIX16 "; PTE=0x%" PRIX16
	               "; offset=0x%" PRIX16, vaddr->pgd_entry, vaddr->pud_entry, vaddr->pmd_entry,
	               vaddr->pte_entry, vaddr->page_offset);      
	return nb;
}

int print_physical_address(FILE* where, const phy_addr_t* paddr){
	int nb = fprintf(where,"page num =0x%" PRIX32 "; offset=0x%" PRIX32, paddr->phy_page_num,
	                 paddr->page_offset);
	return nb;
}
