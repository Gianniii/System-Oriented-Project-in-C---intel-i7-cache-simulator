#include "tlb_hrchy_mng.h"
#include "error.h"
#include "addr_mng.h"


/**
 * @brief Initialize a TLB entry
 * @param vaddr pointer to virtual address, to extract tlb tag
 * @param paddr pointer to physical address, to extract physical page number
 * @param tlb_entry pointer to the entry to be initialized
 * @param tlb_type to distinguish between different TLBs
 * @return  error code
 */

int tlb_entry_init( const virt_addr_t * vaddr,
                    const phy_addr_t * paddr,
                    void * tlb_entry,
                    tlb_t tlb_type) {
						
						
	M_REQUIRE_NON_NULL(vaddr);
	M_REQUIRE_NON_NULL(paddr);
	M_REQUIRE_NON_NULL(tlb_entry);
	M_REQUIRE(tlb_type == L1_DTLB || tlb_type == L1_ITLB || tlb_type == L2_TLB, 
			  ERR_BAD_PARAMETER, "%s", "tlb has non existing type");

	uint32_t tag = virt_addr_t_to_virtual_page_number(vaddr);
	
	if(tlb_type == L1_ITLB) {
		//uint_t automatically do right logical shift
		tag = tag >> 4;
		((l1_itlb_entry_t*)tlb_entry)->tag = tag;
		((l1_itlb_entry_t*)tlb_entry)->phy_page_num = paddr->phy_page_num;
		((l1_itlb_entry_t*)tlb_entry)->v = (uint8_t) 1;
	} else if(tlb_type == L1_DTLB) {
		tag = tag >> 4;
		((l1_dtlb_entry_t*)tlb_entry)->tag = tag;
		((l1_dtlb_entry_t*)tlb_entry)->phy_page_num = paddr->phy_page_num;
		((l1_dtlb_entry_t*)tlb_entry)->v = (uint8_t) 1;
	} else if(tlb_type ==  L2_TLB) {
		tag = tag >> 6;
		((l2_tlb_entry_t*)tlb_entry)->tag = tag;
		((l2_tlb_entry_t*)tlb_entry)->phy_page_num = paddr->phy_page_num;
		((l2_tlb_entry_t*)tlb_entry)->v = (uint8_t) 1;
	} else {
		return ERR_BAD_PARAMETER;
	}
	return ERR_NONE;
}

/**
 * @brief Clean a TLB (invalidate, reset...).
 *
 * This function erases all TLB data.
 * @param  tlb (generic) pointer to the TLB
 * @param tlb_type an enum to distinguish between different TLBs
 * @return  error code
 */

int tlb_flush(void *tlb, tlb_t tlb_type) {
	M_REQUIRE_NON_NULL(tlb);
	M_REQUIRE(tlb_type == L1_DTLB || tlb_type == L1_ITLB || tlb_type == L2_TLB, 
			  ERR_BAD_PARAMETER, "%s", "tlb has non existing type");
	size_t i = 0;
	if(tlb_type == L1_ITLB) {
		for(i = 0; i < L1_ITLB_LINES; i++) {
		}
	} else if(tlb_type == L1_DTLB) {
		for(i = 0; i < L1_DTLB_LINES; i++) {
		}
	
	} else if(tlb_type ==  L2_TLB) {
		//TODO FINISH THIS  
		for(i = 0; i < L1_DTLB_LINES; i++) {
		}
	} else {
		return ERR_BAD_PARAMETER;
	}
	return ERR_NONE;
}
