/**
 * @file cache_mng.c
 * @brief cache management functions
 *
 * @author Mirjana Stojilovic
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

int cache_entry_init(const void * mem_space,
                     const phy_addr_t * paddr,
                     void * cache_entry,
                     cache_t cache_type){
    M_REQUIRE_NON_NULL(mem_space);
	M_REQUIRE_NON_NULL(paddr);
	M_REQUIRE_NON_NULL(cache_entry);
	M_REQUIRE(cache_type == L1_ICACHE || cache_type == L1_DCACHE || cache_type == L2_CACHE,
			  ERR_BAD_PARAMETER, "%s", "tlb has non existing type");


    uint32_t phy_addr = paddr->phy_page_num << paddr->page_offset;//TODO: macro or helper method for this

	//uint32_t index;
	//the tag must be shifted so as to remove the index in the virtual address
	size_t i = 0;
	uint32_t tag;
	uint32_t alligned_phy_addr = phy_addr & 0xFFFFFFF0; //set 4 msb bits to 0 so that it is a multiple of 16
	//const word_t* start = (const word_t*) ((mem_space + alligned_phy_addr)/sizeof(word_t)); 
	
	//TODO: Check if this is correct
	const word_t* start = (const word_t*)mem_space + alligned_phy_addr/sizeof(word_t); //not 100% sure (i think mem_space points to addr0 and 
	//since the phy addr is in byte we divie by 4 to get word addr and return a pointer on words at that addr
	if(cache_type == L1_ICACHE) {
	 	tag = phy_addr >> L1_ICACHE_TAG_REMAINING_BITS;
		((l1_icache_entry_t*)cache_entry)->tag = tag;
		((l1_icache_entry_t*)cache_entry)->age = (uint8_t) 0;
		((l1_icache_entry_t*)cache_entry)->v = (uint8_t) 1;
		for(i = 0; i < 4; i++) {
			((l1_icache_entry_t*)cache_entry)->line[i] = start[i];
		}
	} else if(cache_type == L1_DCACHE) {
		tag = phy_addr >> L1_DCACHE_TAG_REMAINING_BITS;
		((l1_dcache_entry_t*)cache_entry)->tag = tag;
		((l1_dcache_entry_t*)cache_entry)->age = (uint8_t) 0;
		((l1_dcache_entry_t*)cache_entry)->v = (uint8_t) 1;
		for(i = 0; i < 4; i++) {
			(((l1_dcache_entry_t*)cache_entry)->line)[i] = start[i];
		}
	} else if(cache_type ==  L2_CACHE) {
		tag = phy_addr >> L2_CACHE_TAG_REMAINING_BITS;
		((l2_cache_entry_t*)cache_entry)->tag = tag;
		((l2_cache_entry_t*)cache_entry)->age = (uint8_t) 0;
		((l2_cache_entry_t*)cache_entry)->v = (uint8_t) 1;
		for(i = 0; i < 4; i++) {
			((l2_cache_entry_t*)cache_entry)->line[i] = start[i];
		}
	} else {
		return ERR_BAD_PARAMETER;
	}

	return ERR_NONE;
}


int cache_flush(void *cache, cache_t cache_type) {
	M_REQUIRE_NON_NULL(cache);
	M_REQUIRE(cache_type == L1_ICACHE || cache_type == L1_DCACHE || cache_type == L2_CACHE,
			  ERR_BAD_PARAMETER, "%s", "tlb has non existing type");
	if(cache_type == L1_ICACHE) {
	 	memset(cache, 0, L1_ICACHE_LINES * sizeof(l1_icache_entry_t));
	} else if(cache_type == L1_DCACHE) {
			memset(cache, 0, L1_DCACHE_LINES * sizeof(l1_dcache_entry_t));
	} else if(cache_type ==  L2_CACHE) {
			memset(cache, 0, L2_CACHE_LINES * sizeof(l2_cache_entry_t));
	} else {
		return ERR_BAD_PARAMETER;
	}
	return ERR_NONE;
}

int cache_insert(uint16_t cache_line_index,
                 uint8_t cache_way,
                 const void * cache_line_in,
                 void * cache,
                 cache_t cache_type) {
	M_REQUIRE_NON_NULL(cache);
	M_REQUIRE_NON_NULL(cache_line_in);
	M_REQUIRE(cache_type == L1_ICACHE || cache_type == L1_DCACHE || cache_type == L2_CACHE,
			  ERR_BAD_PARAMETER, "%s", "tlb has non existing type");
	
	
	//TODO CAN USE GIVEN MACROS FROM CACHE.H TO MAKE I NICER
	int index_in_cache;
	
	if(cache_type == L1_ICACHE) {
		index_in_cache = cache_line_index * L1_ICACHE_WAYS  + cache_way;
		M_REQUIRE(index_in_cache < L1_ICACHE_LINES * L1_ICACHE_WAYS, ERR_BAD_PARAMETER, "%s", "index out of bounds");
	 	//((l1_icache_entry_t*)cache)[index_in_cache] = *(l1_icache_entry_t*) cache_line_in;
	 	cache_entry(L1_ICACHE, L1_ICACHE_WAYS, cache_line_index, cache_way) = *(l1_icache_entry_t*) cache_line_in
	} else if(cache_type == L1_DCACHE) {
		index_in_cache = cache_line_index * L1_DCACHE_WAYS  + cache_way;
		M_REQUIRE(index_in_cache < L1_DCACHE_LINES * L1_DCACHE_WAYS, ERR_BAD_PARAMETER, "%s", "index out of bounds");
		cache_entry(L1_ICACHE, L1_DCACHE_WAYS, cache_line_index, cache_way) = *(l1_icache_entry_t*) cache_line_in
	} else if(cache_type ==  L2_CACHE) {
		index_in_cache = cache_line_index * L2_CACHE_WAYS  + cache_way;
		M_REQUIRE(index_in_cache < L2_CACHE_LINES * L2_CACHE_WAYS, ERR_BAD_PARAMETER, "%s", "index out of bounds");
		cache_entry(L1_ICACHE, L2_CACHE_WAYS, cache_line_index, cache_way) = *(l1_icache_entry_t*) cache_line_in
	} else {
		return ERR_BAD_PARAMETER;
	}
	return ERR_NONE;
}


/**
 * @brief Check if a instruction/data is present in one of the caches.
 *
 * On hit, update hit infos to corresponding index
 *         and update the cache-line-size chunk of data passed as the pointer to the function.
 * On miss, update hit infos to HIT_WAY_MISS or HIT_INDEX_MISS.
 *
 * @param mem_space starting address of the memory space
 * @param cache pointer to the beginning of the cache
 * @param paddr pointer to physical address
 * @param p_line pointer to a cache-line-size chunk of data to return
 * @param hit_way (modified) cache way where hit was detected, HIT_WAY_MISS on miss
 * @param hit_index (modified) cache line index where hit was detected, HIT_INDEX_MISS on miss
 * @param cache_type to distinguish between different caches
 * @return error code
 */

int cache_hit (const void * mem_space,
               void * cache,
               phy_addr_t * paddr,
               const uint32_t ** p_line,
               uint8_t *hit_way,
               uint16_t *hit_index,
               cache_t cache_type) {
				   //check in that line in every way, then return the hit index and the way
				   //TODO:hit index ?
	return ERR_NONE;
}
