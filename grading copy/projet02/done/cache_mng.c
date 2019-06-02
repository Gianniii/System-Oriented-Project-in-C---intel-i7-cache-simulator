/**
 * @file cache_mng.c
 * @brief cache management functions
 *
 * @date 2018-19
 */

#include "error.h"
#include "util.h"
#include "cache_mng.h"

#include <inttypes.h> // for PRIx macros

//=========================================================================
#define PRINT_CACHE_LINE(OUTFILE, TYPE, WAYS, LINE_INDEX, WAY, WORDS_PER_LINE) \
    do { \
            fprintf(OUTFILE, "V: %1" PRIx8 ", AGE: %1" PRIx8 ", TAG: 0x%03" PRIx16 ", values: ( ", \
                        cache_valid(TYPE, WAYS, LINE_INDEX, WAY), \
                        cache_age(TYPE, WAYS, LINE_INDEX, WAY), \
                        cache_tag(TYPE, WAYS, LINE_INDEX, WAY)); \
            for(int i_ = 0; i_ < WORDS_PER_LINE; i_++) \
                fprintf(OUTFILE, "0x%08" PRIx32 " ", \
                        cache_line(TYPE, WAYS, LINE_INDEX, WAY)[i_]); \
            fputs(")\n", OUTFILE); \
    } while(0)

#define PRINT_INVALID_CACHE_LINE(OUTFILE, TYPE, WAYS, LINE_INDEX, WAY, WORDS_PER_LINE) \
    do { \
            fprintf(OUTFILE, "V: %1" PRIx8 ", AGE: -, TAG: -----, values: ( ---------- ---------- ---------- ---------- )\n", \
                        cache_valid(TYPE, WAYS, LINE_INDEX, WAY)); \
    } while(0)

#define DUMP_CACHE_TYPE(OUTFILE, TYPE, WAYS, LINES, WORDS_PER_LINE)  \
    do { \
        for(uint16_t index = 0; index < LINES; index++) { \
            foreach_way(way, WAYS) { \
                fprintf(output, "%02" PRIx8 "/%04" PRIx16 ": ", way, index); \
                if(cache_valid(TYPE, WAYS, index, way)) \
                    PRINT_CACHE_LINE(OUTFILE, const TYPE, WAYS, index, way, WORDS_PER_LINE); \
                else \
                    PRINT_INVALID_CACHE_LINE(OUTFILE, const TYPE, WAYS, index, way, WORDS_PER_LINE);\
            } \
        } \
    } while(0)

//=========================================================================
// see cache_mng.h
int cache_dump(FILE* output, const void* cache, cache_t cache_type)
{
    M_REQUIRE_NON_NULL(output);
    M_REQUIRE_NON_NULL(cache);

    fputs("WAY/LINE: V: AGE: TAG: WORDS\n", output);
    switch (cache_type) {
    case L1_ICACHE:
        DUMP_CACHE_TYPE(output, l1_icache_entry_t, L1_ICACHE_WAYS,
                        L1_ICACHE_LINES, L1_ICACHE_WORDS_PER_LINE);
        break;
    case L1_DCACHE:
        DUMP_CACHE_TYPE(output, l1_dcache_entry_t, L1_DCACHE_WAYS,
                        L1_DCACHE_LINES, L1_DCACHE_WORDS_PER_LINE);
        break;
    case L2_CACHE:
        DUMP_CACHE_TYPE(output, l2_cache_entry_t, L2_CACHE_WAYS,
                        L2_CACHE_LINES, L2_CACHE_WORDS_PER_LINE);
        break;
    default:
        debug_print("%d: unknown cache type", cache_type);
        return ERR_BAD_PARAMETER;
    }
    putc('\n', output);

    return ERR_NONE;
}

/**
 * @brief Initialize a cache entry (write to the cache entry for the first time)
 *
 * @param mem_space starting address of the memory space
 * @param paddr pointer to physical address, to extract the tag
 * @param cache_entry pointer to the entry to be initialized
 * @param cache_type to distinguish between different caches
 * @return  error code
 */
int cache_entry_init(const void * mem_space,
                     const phy_addr_t * paddr,
                     void * cache_entry,
                     cache_t cache_type){
						 	M_REQUIRE_NON_NULL(vaddr);
    M_REQUIRE_NON_NULL(mem_space);
	M_REQUIRE_NON_NULL(paddr);
	M_REQUIRE_NON_NULL(cache_entry);
	M_REQUIRE(cache_type == L1_ICACHE || cache_type == L1_DCACHE || cache_type == L2_CACHE,
			  ERR_BAD_PARAMETER, "%s", "tlb has non existing type");


    uint32_t tag = paddr->phy_page_num << paddr->page_offset: //TODO: macro or helper method for this

	//uint32_t index;
	//the tag must be shifted so as to remove the index in the virtual address
	if(tlb_type == L1_ICACHE {
	 	tag = tag >> L1_ICACHE_TAG_REMAINING_BITS;
		((l1_icache_entry_t*)tlb_entry)->tag = tag;
		((l1_icache_entry_t*)tlb_entry)->age = (uint8_t) 0;
		((l1_icache_entry_t*)tlb_entry)->v = (uint8_t) 1;
	} else if(tlb_type == L1_DCACHE) {
		tag = tag >> L1_DCACHE_TAG_REMAINING_BITS;
		((l1_dcache_entry_t*)tlb_entry)->tag = tag;
		((l1_dcache_entry_t*)tlb_entry)->age = (uint8_t) 0;
		((l1_dcache_entry_t*)tlb_entry)->v = (uint8_t) 1;
	} else if(tlb_type ==  L2_CACHE) {
		tag = tag >> L2_CACHE_TAG_REMAINING_BITS;
		((l2_cache_entry_t*)tlb_entry)->tag = tag;
		((l2_cache_entry_t*)tlb_entry)->age = (uint8_t) 0;
		((l2_cache_entry_t*)tlb_entry)->v = (uint8_t) 1;
	} else {
		return ERR_BAD_PARAMETER;
	}

	//TODO : INIT THE LINE IN CACHE
    const pte_t* start = (const pte_t *) mem_space;


	return ERR_NONE;
}
