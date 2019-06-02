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
#include "lru.h"

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h> // for PRIx macros

//=========================================================================
// Our macros

#define L1_PHY_ADDR_BYTE_INDEX 0
#define L1_PHY_ADDR_WORD_INDEX 2
#define L1_PHY_ADDR_LINE_INDEX 4
#define L1_PHY_ADDR_TAG_INDEX 10
#define L1_PHY_ADDR_TAG_END_INDEX 32

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
// Helper functions

static inline int find_or_make_empty_way(
        void * l1_cache, 
        cache_t l1_cache_type, 
        void * l2_cache, 
        uint16_t l1_cache_line_index,
        uint16_t l2_cache_line_index,
        cache_replace_t replace,
        uint8_t* bool_cold_start,
        uint8_t* insert_way);

/**
 * @brief TODO
 */
static inline uint32_t get_addr(const phy_addr_t * paddr) {
    return (paddr->phy_page_num << PAGE_OFFSET) | (paddr->page_offset);
}

/**
 * @brief Looks for an empty way in the cache at the given line.
 * 
 * @param cache the cache
 * @param cache_type its type
 * @param cache_line_index the index
 * 
 * @return the index of the 1st empty way that was found. Otherwise, returns -1 
 */
static inline int find_empty_way(void * cache, cache_t cache_type, uint16_t cache_line_index) {
    // TODO Remove this later. I left it to better understand the macro
    // foreach_way(i, L1_ICACHE_WAYS) {
    //     if (!cache_valid(M_CACHE_ENTRY_T(L1_ICACHE), L1_ICACHE_WAYS, cache_line_index, i)) {
    //         return i;
    //     }
    // }
    
    #define M_FIND_EMPTY_WAY(m_cache_type) \
        foreach_way(i, m_cache_type ## _WAYS) { \
            if (!cache_valid(M_CACHE_ENTRY_T(m_cache_type), m_cache_type ## _WAYS, cache_line_index, i)) { \
                return i; \
            } \
        }

    M_EXPAND_ALL_CACHE_TYPES(M_FIND_EMPTY_WAY)

    return -1;
    #undef M_find_empty_way
}

/**
 * @Brief Extracts bits from a u_int bitstring from start (included) to stop 
 * 		  (excluded)
 * @param sample The bitstring from which to extract
 * @param start bit index/placeholder from which to clip
 * @param stop bit index/placeholder till which to clip
 * @return the right justified extracted u_int bitstring
 */
uint32_t extractBits32(uint32_t sample, const uint8_t start, const uint8_t stop) {
    return (sample << (32 - stop)) >> (32 - stop + start);
}

uint8_t extract_byte_select(uint32_t phy_addr) {
    return extractBits32(phy_addr, L1_PHY_ADDR_BYTE_INDEX, L1_PHY_ADDR_WORD_INDEX);
}

uint8_t extract_word_select(uint32_t phy_addr) {
    return extractBits32(phy_addr, L1_PHY_ADDR_WORD_INDEX, L1_PHY_ADDR_LINE_INDEX);
}

uint16_t extract_l1_line_select(uint32_t phy_addr) {
    return extractBits32(phy_addr, L1_PHY_ADDR_LINE_INDEX, L1_PHY_ADDR_TAG_INDEX);
}

uint16_t extract_l2_line_select(uint32_t phy_addr) {
    return extractBits32(phy_addr, 4, 12); // TODO no magic
}

#define extract_tag(phy_addr, m_cache_type) (phy_addr >> m_cache_type ## _TAG_REMAINING_BITS)

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
                     cache_t cache_type) {
    M_REQUIRE_NON_NULL(mem_space);
    M_REQUIRE_NON_NULL(paddr);
    M_REQUIRE_NON_NULL(cache_entry);
    M_REQUIRE(cache_type == L1_ICACHE || cache_type == L1_DCACHE || cache_type == L2_CACHE,
              ERR_BAD_PARAMETER, "%s", "tlb has non existing type");


    uint32_t phy_addr = get_addr(paddr);

    //uint32_t index;
    size_t i = 0;
    uint32_t tag;
    uint32_t alligned_phy_addr = phy_addr & 0xFFFFFFF0; //set 4 msb bits to 0 so that it is a multiple of 16
    

    const word_t* start = (const word_t*)mem_space + alligned_phy_addr/sizeof(word_t);
    
    if(cache_type == L1_ICACHE) {
        tag = phy_addr >> L1_ICACHE_TAG_REMAINING_BITS;
        ((l1_icache_entry_t*)cache_entry)->tag = tag;
        ((l1_icache_entry_t*)cache_entry)->age = (uint8_t) 0;
        ((l1_icache_entry_t*)cache_entry)->v = (uint8_t) 1;
        memcpy(((l1_icache_entry_t*)cache_entry)->line, start, sizeof(word_t) * L1_ICACHE_WORDS_PER_LINE);
    } else if(cache_type == L1_DCACHE) {
        tag = phy_addr >> L1_DCACHE_TAG_REMAINING_BITS;
        ((l1_dcache_entry_t*)cache_entry)->tag = tag;
        ((l1_dcache_entry_t*)cache_entry)->age = (uint8_t) 0;
        ((l1_dcache_entry_t*)cache_entry)->v = (uint8_t) 1;
        memcpy(((l1_icache_entry_t*)cache_entry)->line, start, sizeof(word_t) * L1_ICACHE_WORDS_PER_LINE);
    } else if(cache_type ==  L2_CACHE) {
        tag = phy_addr >> L2_CACHE_TAG_REMAINING_BITS;
        ((l2_cache_entry_t*)cache_entry)->tag = tag;
        ((l2_cache_entry_t*)cache_entry)->age = (uint8_t) 0;
        ((l2_cache_entry_t*)cache_entry)->v = (uint8_t) 1;
        memcpy(((l2_cache_entry_t*)cache_entry)->line, start, sizeof(word_t) * L2_CACHE_WORDS_PER_LINE);
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
    } else if(cache_type == L1_DCACHE) {
        cache_size = L1_DCACHE_LINES * L1_DCACHE_WAYS * sizeof(l1_dcache_entry_t);
    } else {
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
        M_REQUIRE(cache_line_index < m_cache_type ## _LINES, ERR_BAD_PARAMETER, "%s", "cache_line_index out of bounds"); \
        M_REQUIRE(cache_way < m_cache_type ## _WAYS, ERR_BAD_PARAMETER, "%s", "cache_way out of bounds"); \
        M_CACHE_ENTRY_T(m_cache_type)* cache_line = cache_entry(M_CACHE_ENTRY_T(m_cache_type), m_cache_type ## _WAYS, cache_line_index, cache_way); \
        *cache_line = *((M_CACHE_ENTRY_T(m_cache_type)*) cache_line_in); \

    M_EXPAND_ALL_CACHE_TYPES(M_CACHE_INSERT)
    return ERR_NONE;
    #undef M_CACHE_INSERT
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
    //M_REQUIRE_NON_NULL(mem_space); //memspace is unused here. Are you sure?
    M_REQUIRE_NON_NULL(cache);
    M_REQUIRE_NON_NULL(paddr);
    M_REQUIRE_NON_NULL(p_line);
    M_REQUIRE_NON_NULL(hit_way);
    M_REQUIRE_NON_NULL(hit_index);
    M_REQUIRE(cache_type == L1_ICACHE || cache_type == L1_DCACHE || cache_type == L2_CACHE,
              ERR_BAD_PARAMETER, "%s", "tlb has non existing type");

    uint32_t phy_addr = get_addr(paddr);
    uint32_t line_index;
    uint32_t tag;

    uint8_t hit = 0;
    
     // TODO Use macro to treat all 3 types
    if (cache_type == L1_ICACHE) {
        line_index = (phy_addr / L1_ICACHE_LINE) % L1_ICACHE_LINES;
        tag = phy_addr >> L1_ICACHE_TAG_REMAINING_BITS;

        foreach_way(i, L1_ICACHE_WAYS) {
            l1_icache_entry_t* cache_entry = cache_entry(l1_icache_entry_t, L1_ICACHE_WAYS, line_index, i);
            if (cache_entry->v && cache_entry->tag == tag) {
                hit = 1;
                *hit_way = i;
                *hit_index = line_index;
                *p_line = cache_entry->line;
                
                LRU_age_update(l1_icache_entry_t, L1_ICACHE_WAYS, i, line_index) // TODO check me

                return ERR_NONE; // break;
            }
        }
    } else if(cache_type == L1_DCACHE) {
		line_index = (phy_addr / L1_DCACHE_LINE) % L1_DCACHE_LINES;
        tag = phy_addr >> L1_DCACHE_TAG_REMAINING_BITS;

        foreach_way(i, L1_ICACHE_WAYS) {
            l1_dcache_entry_t* cache_entry = cache_entry(l1_dcache_entry_t, L1_DCACHE_WAYS, line_index, i);
            if (cache_entry->v && cache_entry->tag == tag) {
                hit = 1;
                *hit_way = i;
                *hit_index = line_index;
                *p_line = cache_entry->line;
                
                LRU_age_update(l1_icache_entry_t, L1_DCACHE_WAYS, i, line_index) // TODO check me

                return ERR_NONE; // break;
            }
        }
	} else if(cache_type == L2_CACHE) {
		line_index = (phy_addr / L2_CACHE_LINE) % L2_CACHE_LINES;
        tag = phy_addr >> L2_CACHE_TAG_REMAINING_BITS;

        foreach_way(i, L2_CACHE_WAYS) {
            l2_cache_entry_t* cache_entry = cache_entry(l2_cache_entry_t, L2_CACHE_WAYS, line_index, i);
            if (cache_entry->v && cache_entry->tag == tag) {
                hit = 1;
                *hit_way = i;
                *hit_index = line_index;
                *p_line = cache_entry->line;
                
                LRU_age_update(l2_cache_entry_t, L2_CACHE_WAYS, i, line_index) // TODO check me

                return ERR_NONE; // break;
            }
        }
	}

    if (!hit) { // TODO Maybe `hit` is not needed
        *hit_way = HIT_WAY_MISS;
        *hit_index = HIT_INDEX_MISS;
    }

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
               cache_replace_t replace) {
    M_REQUIRE_NON_NULL(mem_space);
    M_REQUIRE_NON_NULL(paddr);
    M_REQUIRE_NON_NULL(l1_cache);
    M_REQUIRE_NON_NULL(l2_cache);
    M_REQUIRE_NON_NULL(word);
    M_REQUIRE(access == INSTRUCTION || access == DATA, ERR_BAD_PARAMETER, "%s", "Non existing access type");
    M_REQUIRE(replace == LRU, ERR_BAD_PARAMETER, "%s", "Non existing replacement policy");

    // TODO add address verification

    uint8_t hit_way;
    uint16_t hit_index;
    const uint32_t* p_line;
    uint32_t phy_addr = get_addr(paddr);
    M_REQUIRE(phy_addr % L1_ICACHE_WORDS_PER_LINE == 0, ERR_BAD_PARAMETER, "%s", "paddr is not aligned");
    
    uint16_t l1_cache_line_index = extract_l1_line_select(phy_addr);
    uint16_t l2_cache_line_index = extract_l2_line_select(phy_addr);

    debug_print("%s", "============ cache_read() ============");

    // *** Searching Level 1 Cache ***
    debug_print("%s", "Searching Level 1 Cache");
    if (1) { // access == INSTRUCTION
        M_EXIT_IF_ERR_NOMSG(cache_hit(mem_space, l1_cache, paddr, &p_line, &hit_way, &hit_index, L1_ICACHE));
        if (hit_way != HIT_WAY_MISS) {
            *word = p_line[extract_word_select(phy_addr)];
            debug_print("%s", "L1 Hit! - return ...");
            return ERR_NONE;
        }
    }

    // *** L1 Miss - Searching Level 2 Cache ***
    debug_print("%s", "L1 Miss - Searching Level 2 Cache");

    l1_icache_entry_t l1_new_entry;
    M_EXIT_IF_ERR_NOMSG(cache_hit(mem_space, l2_cache, paddr, &p_line, &hit_way, &hit_index, L2_CACHE));
    if (hit_way != HIT_WAY_MISS) {
        debug_print("%s", "L2 Hit!");
        if (1) { // access == INSTRUCTION
            // *** Create new l1_cache_entry ***
            l1_new_entry.v = 1;
            memcpy(l1_new_entry.line, p_line, sizeof(word_t) * L1_ICACHE_WORDS_PER_LINE);
            l1_new_entry.tag = extract_tag(phy_addr, L1_ICACHE);
            l1_new_entry.age = 0;

            void * cache = l2_cache;
            cache_valid(l2_cache_entry_t, L2_CACHE_WAYS, hit_index, hit_way) = 0;
            // ***
        } else {} //TODO DATA
    } else {
        // *** L2 Miss - Searching Memory
        debug_print("%s", "L2 Miss - Searching Memory");
        M_EXIT_IF_ERR_NOMSG(cache_entry_init(mem_space, paddr, &l1_new_entry, L1_ICACHE)); // TODO Handle error
        p_line = l1_new_entry.line;
    }

    // Inserting new_entry
    debug_print("%s", "Inserting new_entry");
    uint8_t cold_start;
    uint8_t empty_way;
    M_EXIT_IF_ERR_NOMSG(find_or_make_empty_way(l1_cache, L1_ICACHE, l2_cache, l1_cache_line_index, l2_cache_line_index, replace, &cold_start, &empty_way));

    debug_print("%s %d", "empty way: ", empty_way);
    void * cache = l1_cache;
    l1_new_entry.age = cache_age(l1_icache_entry_t, L1_ICACHE_WAYS, l1_cache_line_index, empty_way);

    debug_print("%s", "================= Before =================");
    foreach_way(i, L1_ICACHE_WAYS) {
        PRINT_CACHE_LINE(stderr, l1_icache_entry_t, L1_ICACHE_WAYS, l1_cache_line_index, i, 4);
    }
    debug_print("%s", "=========================================");
    M_EXIT_IF_ERR_NOMSG(cache_insert(l1_cache_line_index, empty_way, &l1_new_entry, l1_cache, L1_ICACHE)); // TODO Handle error
    debug_print("%s", "================= After Insert =================");
    foreach_way(i, L1_ICACHE_WAYS) {
        PRINT_CACHE_LINE(stderr, l1_icache_entry_t, L1_ICACHE_WAYS, l1_cache_line_index, i, 4);
    }
    debug_print("%s", "=========================================");
    if (replace == LRU) {
        if (cold_start) {
            debug_print("cold_start = %d \tage_increase", cold_start);
            LRU_age_increase(l1_icache_entry_t, L1_ICACHE_WAYS, empty_way, l1_cache_line_index);
        } else {
            debug_print("cold_start = %d \tage_update", cold_start);
            LRU_age_update(l1_icache_entry_t, L1_ICACHE_WAYS, empty_way, l1_cache_line_index);
        }
    }
    debug_print("%s", "================= After =================");
    foreach_way(i, L1_ICACHE_WAYS) {
        PRINT_CACHE_LINE(stderr, l1_icache_entry_t, L1_ICACHE_WAYS, l1_cache_line_index, i, 4);
    }
    debug_print("%s", "=========================================");

    debug_print("%d", extract_word_select(phy_addr));
    // PRINT_CACHE_LINE(stderr, l1_icache_entry_t, L1_ICACHE_WAYS, l1_cache_line_index, empty_way, 4);
    *word = p_line[extract_word_select(phy_addr)];

    return ERR_NONE;
}

/**
 * @brief Looks for an empty way in the l1_cache. If found return the empty_way. 
 *        Otherwise, will perform the required eviction from the l1_cache in order
 *        to make space for the new entry and return its way.
 * 
 * @param bool_cold_start (modified) was this a cold start? 
 */
static inline int find_or_make_empty_way( // TODO Handle errors
        void * l1_cache, 
        cache_t l1_cache_type, 
        void * l2_cache, 
        uint16_t l1_cache_line_index,
        uint16_t l2_cache_line_index,
        cache_replace_t replace,
        uint8_t* bool_cold_start,
        uint8_t* insert_way) {

    int l1_insert_way = find_empty_way(l1_cache, L1_ICACHE, l1_cache_line_index);
    uint8_t l1_cold_start = (l1_insert_way != -1);

    if (!l1_cold_start) { // No "cold start". Eviction!
        // *** Find oldest entry in l1_cache. It will be evicted ***
        void * cache = l1_cache;
        {
            uint8_t way_max = 128;
            uint8_t max = 0;
            debug_print("%s", "--- Searching L1 Cache ---");
            foreach_way(i, L1_ICACHE_WAYS) {
                uint8_t age = cache_age(l1_icache_entry_t, L1_ICACHE_WAYS, l1_cache_line_index, i);
                PRINT_CACHE_LINE(stderr, l1_icache_entry_t, L1_ICACHE_WAYS, l1_cache_line_index, i, 4);
                if (max < age) {
                    max = age;
                    way_max = i;
                }
            }
            l1_insert_way = way_max;
        }
        // ******

        // *** Moving evicted entry to l2_cache ***
        
        // Make a copy of the l1_entry to evict
        l1_icache_entry_t l1_old_entry = *(cache_entry(l1_icache_entry_t, L1_ICACHE_WAYS, l1_cache_line_index, l1_insert_way));

        int l2_insert_way = find_empty_way(l2_cache, L2_CACHE, l2_cache_line_index);
        uint8_t l2_cold_start = (l2_insert_way != -1);

        cache = l2_cache;

        if (!l2_cold_start) {
            uint8_t way_max = 128;
            uint8_t max = 0;
            foreach_way(i, L2_CACHE_WAYS) {
            uint8_t age = cache_age(l2_cache_entry_t, L2_CACHE_WAYS, l2_cache_line_index, i);
                if (max < age) {
                    max = age;
                    way_max = i;
                }
            }
            l2_insert_way = way_max;
        }
        
        // === Creating new l2_entry from evicted l1_entry ===
        l2_cache_entry_t l2_new_entry;
        l2_new_entry.v = 1;
        l2_new_entry.tag = l1_old_entry.tag >> (L1_ICACHE_TAG_BITS - L2_CACHE_TAG_BITS);
        l2_new_entry.age = cache_age(l2_cache_entry_t, L2_CACHE_WAYS, l2_cache_line_index, l2_insert_way);
        memcpy(l2_new_entry.line, l1_old_entry.line, sizeof(word_t) * L1_ICACHE_WORDS_PER_LINE);
        // ======

        cache_insert(l2_cache_line_index, l2_insert_way, &l2_new_entry, l2_cache, L2_CACHE);
        if (replace == LRU) {
            if (l2_cold_start) {
                LRU_age_increase(l2_cache_entry_t, L2_CACHE_WAYS, l2_insert_way, l2_cache_line_index);    
            } else {
                LRU_age_update(l2_cache_entry_t, L2_CACHE_WAYS, l2_insert_way, l2_cache_line_index);
            }
        }
        // ******
    }

    *insert_way = l1_insert_way;
    *bool_cold_start = l1_cold_start;
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
                    cache_replace_t replace) {
    M_REQUIRE_NON_NULL(mem_space);
    M_REQUIRE_NON_NULL(p_paddr);
    M_REQUIRE_NON_NULL(l1_cache);
    M_REQUIRE_NON_NULL(l2_cache);
    M_REQUIRE_NON_NULL(p_byte);
    M_REQUIRE(access == INSTRUCTION || access == DATA, ERR_BAD_PARAMETER, "%s", "Non existing access type");
    M_REQUIRE(replace == LRU, ERR_BAD_PARAMETER, "%s", "Non existing replacement policy");

    phy_addr_t paddr = *p_paddr;
    paddr.page_offset = (p_paddr->page_offset - (p_paddr->page_offset % L1_ICACHE_WORDS_PER_LINE));
    word_t word;
    cache_read(mem_space, &paddr, access, l1_cache, l2_cache, &word, replace);

    *p_byte = ((byte_t*)(&word))[p_paddr->page_offset % L1_ICACHE_WORDS_PER_LINE];

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
                cache_replace_t replace) {

    M_REQUIRE_NON_NULL(mem_space);
    M_REQUIRE_NON_NULL(paddr);
    M_REQUIRE_NON_NULL(l1_cache);
    M_REQUIRE_NON_NULL(l2_cache);    

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
                     cache_replace_t replace) {
    return ERR_NONE;
}               
