#include "tlb.h"
#include "addr.h"
#include "list.h"
#include "addr_mng.h"
#include "error.h"
#include "tlb_mng.h"



int tlb_entry_init( const virt_addr_t * vaddr,
                    const phy_addr_t * paddr,
                    tlb_entry_t * tlb_entry) {
	M_REQUIRE_NON_NULL(tlb_entry);
	M_REQUIRE_NON_NULL(vaddr);
	M_REQUIRE_NON_NULL(paddr);
	
	tlb_entry->tag = virt_addr_t_to_virtual_page_number(vaddr); // extract bits [12,48[
	tlb_entry->phy_page_num = paddr->phy_page_num;
	tlb_entry->v = (uint8_t) 1;
	
	return ERR_NONE;
}


int tlb_flush(tlb_entry_t * tlb) {
	M_REQUIRE_NON_NULL(tlb);
	tlb_entry_t* copy = tlb;
	copy = memset(tlb, 0, TLB_LINES * sizeof(tlb_entry_t));
	M_REQUIRE_NON_NULL_CUSTOM_ERR(copy, ERR_MEM);
	return ERR_NONE;
}


//IF ALL IT DOES IS UPDATE THE tlb_entry at given index then why do we need LRU here?
//i dont even have the a replacement policy pointer here ? so that a prob?
/**
 * @brief Insert an entry to a tlb.
 * Eviction policy is least recently used (LRU). 
 *
 * @param line_index the number of the line to overwrite
 * @param tlb_entry pointer to the tlb entry to insert
 * @param tlb pointer to the TLB
 * @return  error code
 */
 //TODO: if full should evict first element? how do i know if its full though?(last elem != null?) and what do i do with older entry of the line?
int tlb_insert( uint32_t line_index,
                const tlb_entry_t * tlb_entry,
                tlb_entry_t * tlb) {
	M_REQUIRE_NON_NULL(tlb_entry);
	M_REQUIRE_NON_NULL(tlb);
	
	tlb[line_index].tag = tlb_entry->tag;
	tlb[line_index].phy_page_num = tlb_entry->phy_page_num;
	tlb[line_index].v = tlb_entry->v;
	
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
 * @return hit (1) or miss (0)
 */
int tlb_hit(const virt_addr_t * vaddr,
            phy_addr_t * paddr,
            const tlb_entry_t * tlb,
            replacement_policy_t * replacement_policy) {
	int hit = 0;
	if(vaddr == NULL || paddr == NULL || tlb == NULL || replacement_policy == NULL) {
		return 0;
	}
	
	uint64_t virt_page_num = virt_addr_t_to_virtual_page_number(vaddr);
	
	size_t hit_index = 0;
	size_t i = TLB_LINES - 1;
	for(i = TLB_LINES - 1; i >= 0; --i) {
		if(tlb[i].tag == virt_page_num && tlb[i].v == 1) {
			hit = 1; 
			hit_index = i;
		}
	}
	
	if(hit) {
		paddr->page_offset = vaddr->page_offset;
		paddr->phy_page_num = virt_page_num ;
		size_t i = 0;
		//find the node at the hit index in the last
		node_t* n = replacement_policy->ll->front;
		for(i = 0; i < hit_index; i++) {
			n = n->next;
			
		}
		replacement_policy->move_back(replacement_policy->ll, n);
	}
	
	return hit;
}

/**
 * @brief Ask TLB for the translation.
 *
 * @param mem_space pointer to the memory space
 * @param vaddr pointer to virtual address
 * @param paddr (modified) pointer to physical address (returned from TLB)
 * @param tlb pointer to the beginning of the TLB
 * @param hit_or_miss (modified) hit (1) or miss (0)
 * @return error code
 */
int tlb_search( const void * mem_space,
                const virt_addr_t * vaddr,
                phy_addr_t * paddr,
                tlb_entry_t * tlb,
                replacement_policy_t * replacement_policy,
                int* hit_or_miss);
