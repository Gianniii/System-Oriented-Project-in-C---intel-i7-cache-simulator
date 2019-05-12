#include "tlb_hrchy_mng.h"
#include "error.h"
#include "addr_mng.h"
#include "tlb_hrchy.h"
#include "tlb.h"
#include "page_walk.h"

// Used by M_TLB_ENTRY_T(m_tlb_type) to get a ..._entry_t from an type
#define M_L1_ITLB_ENTRY l1_itlb_entry_t
#define M_L1_DTLB_ENTRY l1_dtlb_entry_t
#define M_L2_TLB_ENTRY l2_tlb_entry_t

// Used to get a ..._entry_t from a tlb_type (enum)
#define M_TLB_ENTRY_T(m_tlb_type) M_ ## m_tlb_type ## _ENTRY

#define M_EXECUTE_MACRO_ON_CORRECT_TLB_TYPE(MACRO) \
		if (tlb_type == L1_ITLB) { \
			MACRO(L1_ITLB); \
		} else if (tlb_type == L1_DTLB) { \
			MACRO(L1_DTLB); \
		} else { \
			MACRO(L2_TLB); \
		}

int tlb_entry_init( const virt_addr_t * vaddr,
                    const phy_addr_t * paddr,
                    void * tlb_entry,
                    tlb_t tlb_type) {

	M_REQUIRE_NON_NULL(vaddr);
	M_REQUIRE_NON_NULL(paddr);
	M_REQUIRE_NON_NULL(tlb_entry);
	M_REQUIRE(tlb_type == L1_DTLB || tlb_type == L1_ITLB || tlb_type == L2_TLB,
			  ERR_BAD_PARAMETER, "%s", "tlb has non existing type");

	uint64_t tag = virt_addr_t_to_virtual_page_number(vaddr);

	#define M_IF_TLB_ENTRY_INIT(m_tlb_type) \
		tag = tag >> (m_tlb_type ## _LINES_BITS); \
		((M_TLB_ENTRY_T(m_tlb_type)*)tlb_entry)->tag = tag; \
		((M_TLB_ENTRY_T(m_tlb_type)*)tlb_entry)->phy_page_num = paddr->phy_page_num; \
		((M_TLB_ENTRY_T(m_tlb_type)*)tlb_entry)->v = (uint8_t) 1;

	M_EXECUTE_MACRO_ON_CORRECT_TLB_TYPE(M_IF_TLB_ENTRY_INIT)
	return ERR_NONE;

	#undef M_IF_TLB_ENTRY_INIT
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

	#define M_IF_TLB_INSERT(m_tlb_type) \
		M_REQUIRE(line_index < (m_tlb_type ## _LINES), ERR_BAD_PARAMETER, "%s", "IndexOutBounds"); \
		M_TLB_ENTRY_T(m_tlb_type)* c_tlb = (M_TLB_ENTRY_T(m_tlb_type)*)tlb; \
		const M_TLB_ENTRY_T(m_tlb_type)* c_tlb_entry = (const M_TLB_ENTRY_T(m_tlb_type)*) tlb_entry; \
		c_tlb[line_index].tag = c_tlb_entry ->tag; \
		c_tlb[line_index].phy_page_num = c_tlb_entry->phy_page_num; \
		c_tlb[line_index].v = c_tlb_entry->v;

	M_EXECUTE_MACRO_ON_CORRECT_TLB_TYPE(M_IF_TLB_INSERT)

	return ERR_NONE;

	#undef M_IF_TLB_INSERT
}


int tlb_hit( const virt_addr_t * vaddr,
             phy_addr_t * paddr,
             const void  * tlb,
             tlb_t tlb_type) {
	if(vaddr == NULL || paddr == NULL || tlb == NULL) {
		return 0;
	}

	uint64_t vpn = virt_addr_t_to_virtual_page_number(vaddr); // Virtual Page Number

	uint8_t v;
	uint64_t tag;
	uint32_t phy_page_num;

	#define M_TLB_HIT(m_tlb_type) \
		uint32_t line_index = vpn % (m_tlb_type ## _LINES); \
		M_TLB_ENTRY_T(m_tlb_type) c_tlb = ((M_TLB_ENTRY_T(m_tlb_type)*) tlb)[line_index]; \
		v = c_tlb.v; \
		tag = c_tlb.tag; \
		phy_page_num = c_tlb.phy_page_num; \
		if (v && (tag == vpn >> (m_tlb_type ## _LINES_BITS))) { \
			paddr->phy_page_num = phy_page_num; \
			paddr->page_offset = vaddr->page_offset; \
			return 1; \
		}

	M_EXECUTE_MACRO_ON_CORRECT_TLB_TYPE(M_TLB_HIT)

	return 0;

	#undef M_TLB_HIT
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
			*hit_or_miss = 1;
			return ERR_NONE;
		}
	} else {
		if (tlb_hit(vaddr, paddr, l1_dtlb, L1_DTLB)) {
			*hit_or_miss = 1;
			return ERR_NONE;
		}
	}

	// TODO MANY ERROR CHECKS
	// *** "L1 Miss" ***

	uint64_t vpn = virt_addr_t_to_virtual_page_number(vaddr); // Virtual Page Number

	if (tlb_hit(vaddr, paddr, l2_tlb, L2_TLB)) {
		*hit_or_miss = 1;

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
	*hit_or_miss = 0;

	l2_tlb_entry_t old_l2_entry = l2_tlb[vpn % L2_TLB_LINES];

	M_EXIT_IF_ERR(page_walk(mem_space, vaddr, paddr), "page_walk failed");

	// Create new L2 TLB entry and insert it.
	l2_tlb_entry_t new_l2_entry;
	tlb_entry_init(vaddr, paddr, &new_l2_entry, L2_TLB);
	tlb_insert(vpn % L2_TLB_LINES, &new_l2_entry, l2_tlb, L2_TLB);

	// Create new L1 TLB entry and insert it.
	if (access == INSTRUCTION) {
		l1_itlb_entry_t new_l1i_entry;
		tlb_entry_init(vaddr, paddr, &new_l1i_entry, L1_ITLB);
		tlb_insert(vpn % L1_ITLB_LINES, &new_l1i_entry, l1_itlb, L1_ITLB);

		if (old_l2_entry.v) {
			l1_dtlb_entry_t* curr_l1d_entry = l1_dtlb + (vpn % L1_DTLB_LINES);
			if (curr_l1d_entry->v && (curr_l1d_entry->phy_page_num == old_l2_entry.phy_page_num)) {
				curr_l1d_entry->v = 0;
			}
		}
	} else {
		l1_dtlb_entry_t new_l1d_entry;
		tlb_entry_init(vaddr, paddr, &new_l1d_entry, L1_DTLB);
		tlb_insert(vpn % L1_DTLB_LINES, &new_l1d_entry, l1_dtlb, L1_DTLB);

		if (old_l2_entry.v) {
			l1_itlb_entry_t* curr_l1i_entry = l1_itlb + (vpn % L1_ITLB_LINES);
			if (curr_l1i_entry->v && (curr_l1i_entry->phy_page_num == old_l2_entry.phy_page_num)) {
				curr_l1i_entry->v = 0;
			}
		}
	}

	return ERR_NONE;
}

#undef M_L1_ITLB_ENTRY
#undef M_L1_DTLB_ENTRY
#undef M_L2_TLB_ENTRY
#undef M_TLB_ENTRY_T
#undef M_EXECUTE_MACRO_ON_CORRECT_TLB_TYPE
