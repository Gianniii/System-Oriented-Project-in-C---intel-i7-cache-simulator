#include "tlb.h"
#include "addr.h"
#include "list.h"
#include "addr_mng.h"
#include "error.h"
#include "tlb_mng.h"
#include "page_walk.h"
#include <stdlib.h>



int tlb_entry_init( const virt_addr_t * vaddr,
                    const phy_addr_t * paddr,
                    tlb_entry_t * tlb_entry) {
	M_REQUIRE_NON_NULL(tlb_entry);
	M_REQUIRE_NON_NULL(vaddr);
	M_REQUIRE_NON_NULL(paddr);
	
	tlb_entry->tag = virt_addr_t_to_virtual_page_number(vaddr);
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

int tlb_insert( uint32_t line_index,
                const tlb_entry_t * tlb_entry,
                tlb_entry_t * tlb) {
	M_REQUIRE_NON_NULL(tlb_entry);
	M_REQUIRE_NON_NULL(tlb);
	// correcteur: missing validity check of line_index (hint: out of bounds)
	
	tlb[line_index].tag = tlb_entry->tag;
	tlb[line_index].phy_page_num = tlb_entry->phy_page_num;
	tlb[line_index].v = tlb_entry->v;
	
	return ERR_NONE;
}

int tlb_hit(const virt_addr_t * vaddr,
            phy_addr_t * paddr,
            const tlb_entry_t * tlb,
            replacement_policy_t * replacement_policy) {
	int hit = 0;
	if(vaddr == NULL || paddr == NULL || tlb == NULL || replacement_policy == NULL) {
		return 0;
	}
	uint64_t virt_page_num = virt_addr_t_to_virtual_page_number(vaddr);

	uint32_t hit_index = 0;
	size_t i = TLB_LINES - 1;
	//size_t est unsigned (donc peu pas faire while(i>=0)
	// correcteur: you can use for_all_nodes_reverse instead of a for loop
	//             this way, you won't need your while loop to find the node
	for(i = TLB_LINES - 1; i < TLB_LINES; --i) {
		if(tlb[i].tag == virt_page_num  && tlb[i].v == 1) {
			hit = 1; 
			hit_index = i;
		}
	}
	// correcteur: fix indentation
	if(hit == 1) {
		paddr->page_offset = vaddr->page_offset;
		paddr->phy_page_num = tlb[hit_index].phy_page_num;
		//find the node at the hit index in the list and move the node 
		//to the back of the list using the replacement policy
		node_t* n = replacement_policy->ll->front;
		while(n->value != hit_index){
			n = n->next; 
		}
		replacement_policy->move_back(replacement_policy->ll, n);
		// correcteur: once you have a hit, you don't need to proceed with the loop
		//             you can just return one
	}
	return hit;
}

int tlb_search( const void * mem_space,
                const virt_addr_t * vaddr,
                phy_addr_t * paddr,
                tlb_entry_t * tlb,
                replacement_policy_t * replacement_policy,
                int* hit_or_miss) {
	// correcteur: missing sanity checks
	int error = ERR_NONE;
	//if its a hit the paddr is set correctly from the TLB
	*hit_or_miss = tlb_hit(vaddr, paddr, tlb, replacement_policy);
	//if its a miss use pagewalk to find the padd
	if(*hit_or_miss == 0) { //if its a miss
		error = page_walk(mem_space, vaddr, paddr);
		if(error == ERR_NONE) {
			//init new entry, insert it, at correct index(replace least used index) and then update the LRU list
			tlb_entry_t newEntry;
			// correcteur: missing test and propagation of error from tlb_entry_init
			tlb_entry_init(vaddr, paddr, &newEntry);
			//the value of the front is the LRU index so we insert the new entry at the index(replace least used entry in tlb)
			// correcteur: missing test and propagation of error from tlb_insert
			tlb_insert(replacement_policy->ll->front->value, &newEntry, tlb);
			//we then update the LRU index by moving the front using the replacement policy(ie moving it to the back of the list)
			replacement_policy->move_back(replacement_policy->ll, replacement_policy->ll->front);
		}
	}
	return error;
}
