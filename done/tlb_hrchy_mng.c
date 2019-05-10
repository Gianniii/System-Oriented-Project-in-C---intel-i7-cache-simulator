#include "tlb_hrchy_mng.h"
#include "error.h"
#include "addr_mng.h"
#include "tlb_hrchy.h"
#include "tlb.h"


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
	
	
	//the tag must be shifted so as to remove the index in the virtual address
	if(tlb_type == L1_ITLB) {
		tag = tag >> L1_ITLB_LINES_BITS;
		((l1_itlb_entry_t*)tlb_entry)->tag = tag;
		((l1_itlb_entry_t*)tlb_entry)->phy_page_num = paddr->phy_page_num;
		((l1_itlb_entry_t*)tlb_entry)->v = (uint8_t) 1;
	} else if(tlb_type == L1_DTLB) {
		tag = tag >> L1_DTLB_LINES_BITS;
		((l1_dtlb_entry_t*)tlb_entry)->tag = tag;
		((l1_dtlb_entry_t*)tlb_entry)->phy_page_num = paddr->phy_page_num;
		((l1_dtlb_entry_t*)tlb_entry)->v = (uint8_t) 1;
	} else if(tlb_type ==  L2_TLB) {
		tag = tag >> L2_TLB_LINES_BITS;
		((l2_tlb_entry_t*)tlb_entry)->tag = tag;
		((l2_tlb_entry_t*)tlb_entry)->phy_page_num = paddr->phy_page_num;
		((l2_tlb_entry_t*)tlb_entry)->v = (uint8_t) 1;
	} else {
		return ERR_BAD_PARAMETER;
	}
	return ERR_NONE;
}

int tlb_flush(void *tlb, tlb_t tlb_type) {
	M_REQUIRE_NON_NULL(tlb);
	M_REQUIRE(tlb_type == L1_DTLB || tlb_type == L1_ITLB || tlb_type == L2_TLB, 
			  ERR_BAD_PARAMETER, "%s", "tlb has non existing type");
	
	if(tlb_type == L1_ITLB) {
		memset(tlb, 0, L1_ITLB_LINES * sizeof(l1_itlb_entry_t));
		
	} else if(tlb_type == L1_DTLB) {
		memset(tlb, 0, L1_DTLB_LINES * sizeof(l1_dtlb_entry_t));
		
	} else if(tlb_type ==  L2_TLB) {
		memset(tlb, 0, L2_TLB_LINES * sizeof(l2_tlb_entry_t));
		
	} else {
		return ERR_BAD_PARAMETER;
	}
	return ERR_NONE;
}


int tlb_insert( uint32_t line_index,
                const void * tlb_entry,
                void * tlb,
                tlb_t tlb_type) {
	M_REQUIRE_NON_NULL(tlb_entry);
	M_REQUIRE_NON_NULL(tlb);
	M_REQUIRE(tlb_type == L1_DTLB || tlb_type == L1_ITLB || tlb_type == L2_TLB, 
			  ERR_BAD_PARAMETER, "%s", "tlb has non existing type");
	
	if(tlb_type == L1_ITLB) {
		M_REQUIRE(line_index < L1_ITLB_LINES, ERR_BAD_PARAMETER, "%s", "IndexOutBounds");
		l1_itlb_entry_t* c_tlb = (l1_itlb_entry_t*)tlb;
		const l1_itlb_entry_t* c_tlb_entry = (const l1_itlb_entry_t*) tlb_entry;
		c_tlb[line_index].tag = c_tlb_entry ->tag;
		c_tlb[line_index].phy_page_num = c_tlb_entry->phy_page_num;
		c_tlb[line_index].v = c_tlb_entry->v;
		
	} else if(tlb_type == L1_DTLB) {
		M_REQUIRE(line_index < L1_DTLB_LINES,  ERR_BAD_PARAMETER, "%s", "IndexOutBounds");
		l1_dtlb_entry_t* c_tlb = (l1_dtlb_entry_t*)tlb;
		const l1_dtlb_entry_t* c_tlb_entry = (const l1_dtlb_entry_t*) tlb_entry;
		c_tlb[line_index].tag = c_tlb_entry ->tag;
		c_tlb[line_index].phy_page_num= c_tlb_entry->phy_page_num;
		c_tlb[line_index].v = c_tlb_entry->v;
		
	} else if(tlb_type ==  L2_TLB) {
		M_REQUIRE(line_index < L2_TLB_LINES,  ERR_BAD_PARAMETER, "%s", "IndexOutBounds");
		l2_tlb_entry_t* c_tlb = (l2_tlb_entry_t*)tlb;
		const l2_tlb_entry_t* c_tlb_entry = (const l2_tlb_entry_t*) tlb_entry;
		c_tlb[line_index].tag = c_tlb_entry ->tag;
		c_tlb[line_index].phy_page_num = c_tlb_entry->phy_page_num;
		c_tlb[line_index].v = c_tlb_entry->v;
		
	} else {
		return ERR_BAD_PARAMETER;
	} 
		
	return ERR_NONE;
}

/**
 * @brief Check if a TLB entry exists in the TLB.
 *
 * On hit, return success (1) and update the physical page number passed as the pointer to the function.
 * On miss, return miss (0).
 *
 * @param vaddr pointer to virtual address
 * @param paddr (modified) pointer to physical address
 * @param tlb pointer to the beginning of the tlb
 * @param tlb_type to distinguish between different TLBs
 * @return hit (1) or miss (0)
 */

int tlb_hit( const virt_addr_t * vaddr,
             phy_addr_t * paddr,
             const void  * tlb,
             tlb_t tlb_type) {
	if(vaddr == NULL || paddr == NULL || tlb == NULL || tlb_type == NULL) {
		return 0;
	}

	uint64_t vpn = virt_addr_t_to_virtual_page_number(vaddr); // Virtual Page Number

	uint8_t v;
	uint32_t tag;
	uint32_t phy_page_num;
	switch (tlb_type) {
	case L1_ITLB:
		uint32_t line_index = vpn % L1_ITLB_LINES;
		l1_itlb_entry_t* c_tlb = (l1_itlb_entry_t*) tlb;
		v = c_tlb->v;
		tag = c_tlb->tag;
		phy_page_num = c_tlb->phy_page_num;
		break;
	case L1_DTLB:
		uint32_t line_index = vpn % L1_DTLB_LINES;
		l1_dtlb_entry_t* c_tlb = (l1_dtlb_entry_t*) tlb;
		v = c_tlb->v;
		tag = c_tlb->tag;
		phy_page_num = c_tlb->phy_page_num;
		break;
	case L2_TLB:
		uint32_t line_index = vpn % L2_TLB_LINES;
		l2_tlb_entry_t* c_tlb = (l2_tlb_entry_t*)tlb;
		v = c_tlb->v;
		tag = c_tlb->tag;
		phy_page_num = c_tlb->phy_page_num;
		break;
	}
	
	if (v && (tag == vpn)) {
		paddr->phy_page_num = phy_page_num;
		// paddr->page_offset = vaddr->page_offset; // DONT DO THIS!
		return 1;
	}
	
	return 0;
}

/**
 * @brief Ask TLB for the translation.
 *
 * @param mem_space pointer to the memory space
 * @param vaddr pointer to virtual address
 * @param paddr (modified) pointer to physical address (returned from TLB)
 * @param access to distinguish between fetching instructions and reading/writing data
 * @param l1_itlb pointer to the beginning of L1 ITLB
 * @param l1_dtlb pointer to the beginning of L1 DTLB
 * @param l2_tlb pointer to the beginning of L2 TLB
 * @param hit_or_miss (modified) hit (1) or miss (0)
 * @return error code
 */

int tlb_search( const void * mem_space,
                const virt_addr_t * vaddr,
                phy_addr_t * paddr,
                mem_access_t access,
                l1_itlb_entry_t * l1_itlb,
                l1_dtlb_entry_t * l1_dtlb,
                l2_tlb_entry_t * l2_tlb,
                int* hit_or_miss) {

	M_REQUIRE_NON_NULL(mem_space);
	M_REQUIRE_NON_NULL(vaddr);
	M_REQUIRE_NON_NULL(paddr);
	M_REQUIRE_NON_NULL(l1_itlb);
	M_REQUIRE_NON_NULL(l1_dtlb);
	M_REQUIRE_NON_NULL(l2_tlb);	

	if (access == INSTRUCTION) {
		if (tlb_hit(vaddr, paddr, l1_itlb, L1_ITLB)) {
			hit_or_miss = 1;
			return ERR_NONE;
		}
	} else {
		if (tlb_hit(vaddr, paddr, l1_dtlb, L1_DTLB)) {
			hit_or_miss = 1;
			return ERR_NONE;
		}
	}

	// *** "L1 Miss" ***

	uint64_t vpn = virt_addr_t_to_virtual_page_number(vaddr); // Virtual Page Number

	if (tlb_hit(vaddr, paddr, l2_tlb, L2_TLB)) {
		hit_or_miss = 1;

		if (access == INSTRUCTION) {
			l1_itlb_entry_t new_l1i_entry;
			tlb_entry_init(vaddr, paddr, &new_l1i_entry, L1_ITLB);
			tlb_insert(vpn % L1_ITLB_LINES, &new_l1i_entry, l1_itlb, L1_ITLB);
		} else {
			l1_dtlb_entry_t new_l1d_entry;
			tlb_entry_init(vaddr, paddr, &new_l1d_entry, L1_ITLB);
			tlb_insert(vpn % L1_ITLB_LINES, &new_l1d_entry, l1_itlb, L1_ITLB);
		}

		return ERR_NONE;
	}

	// *** "L2 Miss" ***
	hit_or_miss = 0;

	M_EXIT_IF_ERR(page_walk(mem_space, vaddr, paddr), "page_walk failed");

	// Create new L2 TLB entry and insert it.
	l2_tlb_entry_t new_l2_entry;
	tlb_entry_init(vaddr, paddr, &new_l2_entry, L2_TLB);
	tlb_insert(vpn % L2_TLB_LINES, &new_l2_entry, l2_tlb, L2_TLB);

	// Create new L1 TLB entry and insert it.
	switch (access) {
	case INSTRUCTION:
		l1_itlb_entry_t new_l1i_entry;
		tlb_entry_init(vaddr, paddr, &new_l1i_entry, L1_ITLB);
		tlb_insert(vpn % L1_ITLB_LINES, &new_l1i_entry, l1_itlb, L1_ITLB);
		break;
	case DATA:
		l1_dtlb_entry_t new_l1d_entry;
		tlb_entry_init(vaddr, paddr, &new_l1d_entry, L1_DTLB);
		tlb_insert(vpn % L1_DTLB_LINES, &new_l1d_entry, l1_dtlb, L1_DTLB);
		break;
	// TODO Should case default throw an error?
	}

	// TODO Invalidate if needed

	return ERR_NONE;
}
