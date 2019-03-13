#include <stdio.h>
#include "addr.h"
#include "addr_mng.h"
#include "inttypes.h"
int print_physical_address(FILE* where, const phy_addr_t* paddr){
	//TODO
	return 0;
}

int print_virtual_address(FILE* where, const virt_addr_t* vaddr){
	
	int nb = fprintf(where, "PGD=0x%" PRIX16 " PUD=0x%" PRIX16 " PMD=0x%" PRIX16 "PTE=0x%" PRIX16
	               " offset=0x%" PRIX16, vaddr->pgd_entry, vaddr->pud_entry, vaddr->pmd_entry,
	               vaddr->pte_entry, vaddr->page_offset);      
	return nb;
}

int init_virt_addr64(virt_addr_t * vaddr, uint64_t vaddr64){
	
}
