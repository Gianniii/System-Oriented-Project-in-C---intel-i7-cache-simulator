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

#include <stdio.h>
#include <inttypes.h> // for PRIx macros

//=========================================================================
// Our macros

// Used by M_TLB_ENTRY_T(m_tlb_type) to get a ..._entry_t from an type
#define M_L1_ICACHE_ENTRY l1_icache_entry_t
#define M_L1_DCACHE_ENTRY l1_dcache_entry_t
#define M_L2_CACHE_ENTRY l2_cache_entry_t

// Used to get a ..._entry_t from a tlb_type (enum)
// To be used strictly with parameters L1_ICACHE, L1_DCACHE or L2_CACHE
#define M_CACHE_ENTRY_T(m_cache_type) M_ ## m_cache_type ## _ENTRY

// Creates a if else if .. statement with a select macro for all 3 cache types
#define M_EXPAND_ALL_CACHE_TYPES(MACRO) \
	if (cache_type == L1_ICACHE) { \
		MACRO(L1_ICACHE); \
	} else if (cache_type == L1_DCACHE) { \
		MACRO(L1_DCACHE); \
	} else { \
		MACRO(L2_CACHE); \
	}

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


    uint32_t phy_addr = (paddr->phy_page_num << PAGE_OFFSET) | (paddr->page_offset); //TODO: macro or helper method for this

    //uint32_t index;
    //the tag must be shifted so as to remove the index in the virtual address
    size_t i = 0;
    uint32_t tag;
    uint32_t alligned_phy_addr = phy_addr & 0xFFFFFFF0; //set 4 msb bits to 0 so that it is a multiple of 16
    
    
    printf("phy_addr : 0x%" PRIX32 "\n", phy_addr);
    const word_t* start = (const word_t*)mem_space + alligned_phy_addr/sizeof(word_t);
    
    if(cache_type == L1_ICACHE) {
         tag = phy_addr >> L1_ICACHE_TAG_REMAINING_BITS;
        ((l1_icache_entry_t*)cache_entry)->tag = tag;
        ((l1_icache_entry_t*)cache_entry)->age = (uint8_t) 0;
        ((l1_icache_entry_t*)cache_entry)->v = (uint8_t) 1;
        for(i = 0; i < 4; i++) { // TODO Probabably can be replaced with foreach_way(var, ways)
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
        printf("tag corresponding to : 0x%" PRIX32 "is : 0x%" PRIX32 "\n", phy_addr, tag); // TODO Remove
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
    
    size_t cache_size;
    if(cache_type == L1_ICACHE) {
        cache_size = L1_ICACHE_LINES * L1_ICACHE_WAYS * sizeof(l1_icache_entry_t);
         //still needa add ways or smthn? @michael I think so. I did it
    } else if(cache_type == L1_DCACHE) {
        cache_size = L1_DCACHE_LINES * L1_DCACHE_WAYS * sizeof(l1_dcache_entry_t);
    } else { // We already test for valid types at the top of function
        cache_size = L2_CACHE_LINES * L2_CACHE_WAYS * sizeof(l2_cache_entry_t);
    }

    memset(cache, 0, cache_size);
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

    #define M_CACHE_INSERT(m_cache_type) \
        M_REQUIRE(cache_line_index <= m_cache_type ## _LINES, ERR_BAD_PARAMETER, "%s", "cache_line_index out of bounds"); \
        M_CACHE_ENTRY_T(m_cache_type)* cache_line = cache_entry(M_CACHE_ENTRY_T(m_cache_type), m_cache_type ## _WAYS, cache_line_index, cache_way); \
        *cache_line = *((M_CACHE_ENTRY_T(m_cache_type)*) cache_line_in); \

    M_EXPAND_ALL_CACHE_TYPES(M_CACHE_INSERT)
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



 /** @brief Ask cache for a word of data.
 *  Exclusive policy (https://en.wikipedia.org/wiki/Cache_inclusion_policy)
 *      Consider the case when L2 is exclusive of L1. Suppose there is a
 *      processor read request for block X. If the block is found in L1 cache,
 *      then the data is read from L1 cache and returned to the processor. If
 *      the block is not found in the L1 cache, but present in the L2 cache,
 *      then the cache block is moved from the L2 cache to the L1 cache. If
 *      this causes a block to be evicted from L1, the evicted block is then
 *      placed into L2. This is the only way L2 gets populated. Here, L2
 *      behaves like a victim cache. If the block is not found neither in L1 nor
 *      in L2, then it is fetched from main memory and placed just in L1 and not
 *      in L2.
 *
 * @param mem_space pointer to the memory space
 * @param paddr pointer to a physical address
 * @param access to distinguish between fetching instructions and reading/writing data
 * @param l1_cache pointer to the beginning of L1 CACHE
 * @param l2_cache pointer to the beginning of L2 CACHE
 * @param word pointer to the word of data that is returned by cache
 * @param replace replacement policy
 * @return error code
 */
int cache_read(const void * mem_space,
               phy_addr_t * paddr,
               mem_access_t access,
               void * l1_cache,
               void * l2_cache,
               uint32_t * word,
               cache_replace_t replace){
                   return ERR_NONE;
               }

/**
 * @brief Write to cache a byte of data. Endianess: LITTLE.
 *
 * @param mem_space pointer to the memory space
 * @param paddr pointer to a physical address
 * @param l1_cache pointer to the beginning of L1 ICACHE
 * @param l2_cache pointer to the beginning of L2 CACHE
 * @param p_byte pointer to the byte to be returned
 * @param replace replacement policy
 * @return error code
 */
int cache_write_byte(void * mem_space,
                     phy_addr_t * paddr,
                     void * l1_cache,
                     void * l2_cache,
                     uint8_t p_byte,
                     cache_replace_t replace){
                         return ERR_NONE;
                     }
                     
                     /**
 * @brief Change a word of data in the cache.
 *  Exclusive policy (see cache_read)
 *
 * @param mem_space pointer to the memory space
 * @param paddr pointer to a physical address
 * @param l1_cache pointer to the beginning of L1 CACHE
 * @param l2_cache pointer to the beginning of L2 CACHE
 * @param word const pointer to the word of data that is to be written to the cache
 * @param replace replacement policy
 * @return error code
 */
int cache_write(void * mem_space,
                phy_addr_t * paddr,
                void * l1_cache,
                void * l2_cache,
                const uint32_t * word,
                cache_replace_t replace){
                    return ERR_NONE;
                }
                
                
                
                /**
 * @brief Ask cache for a byte of data. Endianess: LITTLE.
 *
 * @param mem_space pointer to the memory space
 * @param p_addr pointer to a physical address
 * @param access to distinguish between fetching instructions and reading/writing data
 * @param l1_cache pointer to the beginning of L1 CACHE
 * @param l2_cache pointer to the beginning of L2 CACHE
 * @param byte pointer to the byte to be returned
 * @param replace replacement policy
 * @return error code
 */
int cache_read_byte(const void * mem_space,
                    phy_addr_t * p_paddr,
                    mem_access_t access,
                    void * l1_cache,
                    void * l2_cache,
                    uint8_t * p_byte,
                    cache_replace_t replace){
                        return ERR_NONE;
                    }


