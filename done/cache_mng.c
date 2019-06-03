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
// Helper functions & Macros

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

#define L1_LINETAG_TO_L2_LINETAG(IN_L1_TAG, IN_L1_LINE, OUT_L2_TAG, OUT_L2_LINE) \
    do { \
        OUT_L2_TAG = extractBits32(IN_L1_TAG, 0, L2_CACHE_TAG_BITS); \
        OUT_L2_LINE = (extractBits32(IN_L1_TAG, 0, 3) << 6) | IN_L1_LINE; \
    } while(0)

#define L2_LINETAG_TO_L1_LINETAG(IN_L2_TAG, IN_L2_LINE, OUT_L1_TAG, OUT_L1_LINE) \
    do { \
        OUT_L1_TAG = (IN_L2_TAG << 3) | extractBits32(IN_L2_LINE, 6, 9); \
        OUT_L1_LINE = extractBits32(IN_L2_LINE, 0, 6); \
    } while(0)

#define TRANSFER_ENTRY_INFO(DEST_ENTRY, SRC_ENTRY, DEST_TAG) \
    do { \
        (DEST_ENTRY)->v = 1; \
        (DEST_ENTRY)->tag = (DEST_TAG); \
        memcpy((DEST_ENTRY)->line, (SRC_ENTRY)->line, sizeof(word_t) * L1_DCACHE_WORDS_PER_LINE); \
    } while(0)

#define my_cache_cast(CACHE, M_CACHE_TYPE) ((M_CACHE_ENTRY_T(M_CACHE_TYPE) *) CACHE)

// --------------------------------------------------
#define my_cache_entry(CACHE, M_CACHE_TYPE, LINE_INDEX, WAY) \
        (my_cache_cast(CACHE, M_CACHE_TYPE) + (LINE_INDEX) * (M_CACHE_TYPE ## _WAYS) + (WAY))

#define ZERO_4_LSBS 0xFFFFFFF0

// Finds the line in memory
static inline word_t* find_line_in_mem(const void* mem_space, uint32_t phy_addr) {
    uint32_t alligned_phy_addr = phy_addr & ZERO_4_LSBS;
    return (word_t*) mem_space + alligned_phy_addr/sizeof(word_t);
}
#undef ZERO_4_LSBS

// Applies the correct increase or update policy on given cache block
static inline void recompute_ages(void* cache, cache_t cache_type, uint16_t cache_line, uint8_t way_index, uint8_t bool_cold_start, cache_replace_t replace);

// Find an empty way of a cache line
static inline int find_empty_way(void * cache, cache_t cache_type, uint16_t cache_line_index);

// Finds the oldest way of a cache line
static inline uint8_t find_oldest_way(void* cache, cache_t cache_type, uint16_t cache_line);

// Turns the phy_addr_t to and more useful uint32
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
static inline uint32_t extractBits32(uint32_t sample, const uint8_t start, const uint8_t stop) {
    return (sample << (32 - stop)) >> (32 - stop + start);
}

// Gets the byte_select from a phy_addr (same for l1 and l2)
static inline uint8_t extract_byte_select(uint32_t phy_addr) {
    return extractBits32(phy_addr, L1_PHY_ADDR_BYTE_INDEX, L1_PHY_ADDR_WORD_INDEX);
}

// Gets the word_select from a phy_addr (same for l1 and l2)
static inline uint8_t extract_word_select(uint32_t phy_addr) {
    return extractBits32(phy_addr, L1_PHY_ADDR_WORD_INDEX, L1_PHY_ADDR_LINE_INDEX);
}

static inline uint16_t extract_l1_line_select(uint32_t phy_addr) {
    return extractBits32(phy_addr, L1_PHY_ADDR_LINE_INDEX, L1_PHY_ADDR_TAG_INDEX);
}

#define L2_LINE_INDEX_START 4
#define L2_LINE_INDEX_END 13

static inline uint16_t extract_l2_line_select(uint32_t phy_addr) {
    return extractBits32(phy_addr, L2_LINE_INDEX_START, L2_LINE_INDEX_END);
}

#undef L2_LINE_INDEX_START
#undef L2_LINE_INDEX_END

#define extract_tag(phy_addr, m_cache_type) (phy_addr >> m_cache_type ## _TAG_REMAINING_BITS)

// Performs everything to correctly move a cache_entry from l2 to l1
static inline void handle_l2_to_l1(void* l1_cache, void* l2_cache, uint16_t src_l2_line, uint8_t src_l2_way, cache_replace_t replace) {
    l1_icache_entry_t new_l1_entry;
    l2_cache_entry_t* old_l2_entry = my_cache_entry(l2_cache, L2_CACHE, src_l2_line, src_l2_way);

    uint32_t dest_l1_tag; uint16_t dest_l1_line;
    L2_LINETAG_TO_L1_LINETAG(old_l2_entry->tag, src_l2_line, dest_l1_tag, dest_l1_line);  
    TRANSFER_ENTRY_INFO(&new_l1_entry, old_l2_entry, dest_l1_tag);

    old_l2_entry->v = 0; // Invalidate l2_entry

    // Searching where to place old_l1_entry
    int l1_empty_way = find_empty_way(l1_cache, L1_ICACHE, dest_l1_line);
    uint8_t l1_cold_start = (l1_empty_way != -1);

    if (l1_cold_start) {
        l1_icache_entry_t* l1_empty_entry = my_cache_entry(l1_cache, L1_ICACHE, dest_l1_line, l1_empty_way);
        TRANSFER_ENTRY_INFO(l1_empty_entry, &new_l1_entry, dest_l1_tag);
        recompute_ages(l1_cache, L1_ICACHE, dest_l1_line, l1_empty_way, l1_cold_start, replace);
    } else {
        uint8_t l1_oldest_way = find_oldest_way(l1_cache, L1_ICACHE, dest_l1_line);
        l1_icache_entry_t* old_l1_entry_ptr = my_cache_entry(l1_cache, L1_ICACHE, dest_l1_line, l1_oldest_way);
        l1_icache_entry_t old_l1_entry = *old_l1_entry_ptr;

        TRANSFER_ENTRY_INFO(old_l1_entry_ptr, &new_l1_entry, dest_l1_tag);
        recompute_ages(l1_cache, L1_ICACHE, dest_l1_line, l1_oldest_way, l1_cold_start, replace);

        // =========== look for space in l2 cache =============
        uint32_t dest_l2_tag; uint16_t dest_l2_line; // Fining index for l2_cache
        L1_LINETAG_TO_L2_LINETAG(old_l1_entry.tag, dest_l1_line, dest_l2_tag, dest_l2_line);

        int l2_empty_way = find_empty_way(l2_cache, L2_CACHE, dest_l2_line);
        uint8_t l2_cold_start = (l2_empty_way != -1);
        if (l2_cold_start) {
            l2_cache_entry_t* empty_l2_entry = my_cache_entry(l2_cache, L2_CACHE, dest_l2_line, l2_empty_way);
            TRANSFER_ENTRY_INFO(empty_l2_entry, &old_l1_entry, dest_l2_tag);
            recompute_ages(l2_cache, L2_CACHE, dest_l2_line, l2_empty_way, l2_cold_start, replace);
        } else {
            uint8_t l2_oldest_way = find_oldest_way(l2_cache, L2_CACHE, dest_l2_line);
            l2_cache_entry_t* oldest_l2_entry = my_cache_entry(l2_cache, L2_CACHE, dest_l2_line, l2_oldest_way);
            TRANSFER_ENTRY_INFO(oldest_l2_entry, &old_l1_entry, dest_l2_tag);
            recompute_ages(l2_cache, L2_CACHE, dest_l2_line, l2_oldest_way, l2_cold_start, replace);
        }
    }
}

// Performs everything to correctly set a l1_cache_entry from a given src_entry (an initialised struct containing the wanted info)
static inline void handle_mem_to_l1(void* l1_cache, void* l2_cache, uint16_t dest_l1_line, l1_dcache_entry_t* src_entry, cache_replace_t replace) {
    uint32_t dest_l1_tag = src_entry->tag;

    // Searching where to place old_l1_entry
    int l1_empty_way = find_empty_way(l1_cache, L1_ICACHE, dest_l1_line);
    uint8_t l1_cold_start = (l1_empty_way != -1);

    if (l1_cold_start) {
        l1_icache_entry_t* l1_empty_entry = my_cache_entry(l1_cache, L1_ICACHE, dest_l1_line, l1_empty_way);
        TRANSFER_ENTRY_INFO(l1_empty_entry, src_entry, dest_l1_tag);
        recompute_ages(l1_cache, L1_ICACHE, dest_l1_line, l1_empty_way, l1_cold_start, replace);
    } else {
        uint8_t l1_oldest_way = find_oldest_way(l1_cache, L1_ICACHE, dest_l1_line);
        l1_icache_entry_t* old_l1_entry_ptr = my_cache_entry(l1_cache, L1_ICACHE, dest_l1_line, l1_oldest_way);
        l1_icache_entry_t old_l1_entry = *old_l1_entry_ptr;

        TRANSFER_ENTRY_INFO(old_l1_entry_ptr, src_entry, dest_l1_tag);
        recompute_ages(l1_cache, L1_ICACHE, dest_l1_line, l1_oldest_way, l1_cold_start, replace);

        // =========== look for space in l2 cache =============
        uint32_t dest_l2_tag; uint16_t dest_l2_line; // Fining index for l2_cache
        L1_LINETAG_TO_L2_LINETAG(old_l1_entry.tag, dest_l1_line, dest_l2_tag, dest_l2_line);

        int l2_empty_way = find_empty_way(l2_cache, L2_CACHE, dest_l2_line);
        uint8_t l2_cold_start = (l2_empty_way != -1);
        if (l2_cold_start) {
            l2_cache_entry_t* empty_l2_entry = my_cache_entry(l2_cache, L2_CACHE, dest_l2_line, l2_empty_way);
            TRANSFER_ENTRY_INFO(empty_l2_entry, &old_l1_entry, dest_l2_tag);
            recompute_ages(l2_cache, L2_CACHE, dest_l2_line, l2_empty_way, l2_cold_start, replace);
        } else {
            uint8_t l2_oldest_way = find_oldest_way(l2_cache, L2_CACHE, dest_l2_line);
            l2_cache_entry_t* oldest_l2_entry = my_cache_entry(l2_cache, L2_CACHE, dest_l2_line, l2_oldest_way);
            TRANSFER_ENTRY_INFO(oldest_l2_entry, &old_l1_entry, dest_l2_tag);
            recompute_ages(l2_cache, L2_CACHE, dest_l2_line, l2_oldest_way, l2_cold_start, replace);
        }
    }
}

// Writes data "through the cache"
static inline void write_though(void* mem_space, uint32_t phy_addr, const uint32_t* p_line) {
    word_t* start = find_line_in_mem(mem_space, phy_addr);
    memcpy(start, p_line, sizeof(word_t) * L1_ICACHE_WORDS_PER_LINE);
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
int cache_dump(FILE* output, const void* cache, cache_t cache_type) {
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

    uint32_t tag;

    const word_t* start = find_line_in_mem(mem_space, phy_addr);

    #define M_CACHE_ENTRY_INIT(m_cache_type) \
        tag = phy_addr >> m_cache_type ## _TAG_REMAINING_BITS; \
        M_CACHE_ENTRY_T(m_cache_type)* cast_entry = (M_CACHE_ENTRY_T(m_cache_type)*)cache_entry; \
        cast_entry->tag = tag; \
        cast_entry->age = (uint8_t) 0; \
        cast_entry->v = (uint8_t) 1; \
        memcpy(cast_entry->line, start, sizeof(word_t) * m_cache_type ## _WORDS_PER_LINE);

    M_EXPAND_ALL_CACHE_TYPES(M_CACHE_ENTRY_INIT);
    #undef M_CACHE_ENTRY_INIT

    return ERR_NONE;
}

int cache_flush(void *cache, cache_t cache_type) {
    M_REQUIRE_NON_NULL(cache);
    M_REQUIRE(cache_type == L1_ICACHE || cache_type == L1_DCACHE || cache_type == L2_CACHE,
              ERR_BAD_PARAMETER, "%s", "tlb has non existing type");
    
    size_t cache_size;

    #define M_CACHE_FLUSH(m_cache_type) \
        cache_size = (m_cache_type ## _LINES) * (m_cache_type ## _WAYS) * sizeof(M_CACHE_ENTRY_T(m_cache_type));

    M_EXPAND_ALL_CACHE_TYPES(M_CACHE_FLUSH)
    #undef M_CACHE_FLUSH

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
        M_REQUIRE(cache_line_index < m_cache_type ## _LINES, ERR_BAD_PARAMETER, "cache_line_index out of bounds. cache_line=%d", cache_line_index); \
        M_REQUIRE(cache_way < m_cache_type ## _WAYS, ERR_BAD_PARAMETER, "cache_way out of bounds. cache_way=%d", cache_way); \
        M_CACHE_ENTRY_T(m_cache_type)* cache_line = cache_entry(M_CACHE_ENTRY_T(m_cache_type), m_cache_type ## _WAYS, cache_line_index, cache_way); \
        *cache_line = *((M_CACHE_ENTRY_T(m_cache_type)*) cache_line_in);

    M_EXPAND_ALL_CACHE_TYPES(M_CACHE_INSERT)
    return ERR_NONE;
    #undef M_CACHE_INSERT
}

int cache_hit (const void * mem_space,
               void * cache,
               phy_addr_t * paddr,
               const uint32_t ** p_line,
               uint8_t *hit_way,
               uint16_t *hit_index,
               cache_t cache_type) {
    //M_REQUIRE_NON_NULL(mem_space); // No test since mem_space in unused here!
    M_REQUIRE_NON_NULL(cache);
    M_REQUIRE_NON_NULL(paddr);
    M_REQUIRE_NON_NULL(p_line);
    M_REQUIRE_NON_NULL(hit_way);
    M_REQUIRE_NON_NULL(hit_index);
    M_REQUIRE(cache_type == L1_ICACHE || cache_type == L1_DCACHE || cache_type == L2_CACHE,
              ERR_BAD_PARAMETER, "%s", "tlb has non existing type");

    cache_replace_t replace = LRU; // Since no argument was specified!

    uint32_t phy_addr = get_addr(paddr);
    uint32_t line_index;
    uint32_t tag;

    if (cache_type == L1_ICACHE) {
        line_index = (phy_addr / L1_ICACHE_LINE) % L1_ICACHE_LINES;
        tag = phy_addr >> L1_ICACHE_TAG_REMAINING_BITS;

        foreach_way(i, L1_ICACHE_WAYS) {
            l1_icache_entry_t* cache_entry = cache_entry(l1_icache_entry_t, L1_ICACHE_WAYS, line_index, i);
            if (cache_entry->v && cache_entry->tag == tag) {
                *hit_way = i;
                *hit_index = line_index;
                *p_line = cache_entry->line;

                recompute_ages(cache, L1_ICACHE, line_index, i, 0, replace);

                return ERR_NONE;
            }
        }
    } else if(cache_type == L1_DCACHE) {
		line_index = (phy_addr / L1_DCACHE_LINE) % L1_DCACHE_LINES;
        tag = phy_addr >> L1_DCACHE_TAG_REMAINING_BITS;

        foreach_way(i, L1_ICACHE_WAYS) {
            l1_dcache_entry_t* cache_entry = cache_entry(l1_dcache_entry_t, L1_DCACHE_WAYS, line_index, i);
            if (cache_entry->v && cache_entry->tag == tag) {
                *hit_way = i;
                *hit_index = line_index;
                *p_line = cache_entry->line;
                
                recompute_ages(cache, L1_ICACHE, line_index, i, 0, replace);

                return ERR_NONE;
            }
        }
	} else if(cache_type == L2_CACHE) {
		line_index = (phy_addr / L2_CACHE_LINE) % L2_CACHE_LINES;
        tag = phy_addr >> L2_CACHE_TAG_REMAINING_BITS;

        foreach_way(i, L2_CACHE_WAYS) {
            l2_cache_entry_t* cache_entry = cache_entry(l2_cache_entry_t, L2_CACHE_WAYS, line_index, i);
            if (cache_entry->v && cache_entry->tag == tag) {
                *hit_way = i;
                *hit_index = line_index;
                *p_line = cache_entry->line;
                
                recompute_ages(cache, L2_CACHE, line_index, i, 0, replace);

                return ERR_NONE;
            }
        }
	}

    // Set fields to miss if "Cache Miss"
    *hit_way = HIT_WAY_MISS;
    *hit_index = HIT_INDEX_MISS;

    return ERR_NONE;
}

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

    uint8_t hit_way;
    uint16_t hit_index;
    const uint32_t* p_line;
    uint32_t phy_addr = get_addr(paddr);
    M_REQUIRE(phy_addr % L1_ICACHE_WORDS_PER_LINE == 0, ERR_BAD_PARAMETER, "%s", "paddr is not aligned");

    debug_print("%s", "======================== cache_read() =========================");

    // *** Searching Level 1 Cache ***
    debug_print("%s", "Searching Level 1 Cache");
    if (access == INSTRUCTION || access == DATA) {
        M_EXIT_IF_ERR_NOMSG(cache_hit(mem_space, l1_cache, paddr, &p_line, &hit_way, &hit_index, L1_ICACHE));
        if (hit_way != HIT_WAY_MISS) {
            *word = p_line[extract_word_select(phy_addr)];
            debug_print("%s", "L1 Hit! - return ...");
            return ERR_NONE;
        }
    }

    // *** L1 Miss - Searching Level 2 Cache ***
    debug_print("%s", "L1 Miss - Searching Level 2 Cache");
    M_EXIT_IF_ERR_NOMSG(cache_hit(mem_space, l2_cache, paddr, &p_line, &hit_way, &hit_index, L2_CACHE));
    if (hit_way != HIT_WAY_MISS) {
        debug_print("%s", "L2 Hit!");
        if (access == INSTRUCTION || access == DATA) {
            *word = p_line[extract_word_select(phy_addr)];
            handle_l2_to_l1(l1_cache, l2_cache, hit_index, hit_way, replace);

            return ERR_NONE;
        }
    }

    // *** L2 Miss - Searching Memory
    debug_print("%s", "L2 Miss - Searching Memory");
    l1_icache_entry_t l1_new_entry;
    M_EXIT_IF_ERR_NOMSG(cache_entry_init(mem_space, paddr, &l1_new_entry, L1_ICACHE));
    p_line = l1_new_entry.line;

    // Inserting new_entry
    debug_print("%s", "Inserting new_entry");
    handle_mem_to_l1(l1_cache, l2_cache, extract_l1_line_select(phy_addr), &l1_new_entry, replace);

    *word = p_line[extract_word_select(phy_addr)];
    return ERR_NONE;
}

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
    paddr.page_offset = (p_paddr->page_offset - (p_paddr->page_offset % sizeof(word_t)));
    word_t word;
    cache_read(mem_space, &paddr, access, l1_cache, l2_cache, &word, replace);

    *p_byte = ((byte_t*)(&word))[p_paddr->page_offset % sizeof(word_t)];

    return ERR_NONE;
}

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
    M_REQUIRE_NON_NULL(word);

    uint8_t hit_way;
    uint16_t hit_index;
    uint32_t* p_line;
    uint32_t phy_addr = get_addr(paddr);
    M_REQUIRE(phy_addr % L1_ICACHE_WORDS_PER_LINE == 0, ERR_BAD_PARAMETER, "%s", "paddr is not aligned");
    uint8_t word_index = extract_word_select(phy_addr);
    
    // === Searching L1_DCACHE ===
    M_EXIT_ERR_NOMSG(cache_hit(mem_space, l1_cache, paddr, (const uint32_t**) &p_line, &hit_way, &hit_index, L1_DCACHE));
    if (hit_way != HIT_WAY_MISS) {
        p_line[word_index] = *word;
        recompute_ages(l1_cache, L1_DCACHE, hit_index, hit_way, 0, replace);
        write_though(mem_space, phy_addr, p_line);
        return ERR_NONE;
    }

    // ==========Check L2_CACHE========
    M_EXIT_ERR_NOMSG(cache_hit(mem_space, l2_cache, paddr, (const uint32_t**) &p_line, &hit_way, &hit_index, L2_CACHE));
    if(hit_way  != HIT_WAY_MISS) {
        p_line[word_index] = *word;
        recompute_ages(l2_cache, L2_CACHE, hit_index, hit_way, 0, replace);
        handle_l2_to_l1(l1_cache, l2_cache, hit_index, hit_way, replace);
        write_though(mem_space, phy_addr, p_line);
    }

    // ============ L1 & L2 Miss, Fetching from Memory ==================
    l1_dcache_entry_t read_entry;
    cache_entry_init(mem_space, paddr, &read_entry, L1_DCACHE);
    read_entry.line[word_index] = *word;
    write_though(mem_space, phy_addr, read_entry.line);

    uint16_t l1_line = extract_l1_line_select(phy_addr);
    handle_mem_to_l1(l1_cache, l2_cache, l1_line, &read_entry, replace);

    return ERR_NONE;
}

int cache_write_byte(void * mem_space,
                     phy_addr_t * paddr,
                     void * l1_cache,
                     void * l2_cache,
                     uint8_t p_byte,
                     cache_replace_t replace) {
    
    M_REQUIRE_NON_NULL(mem_space);
    M_REQUIRE_NON_NULL(paddr);
    M_REQUIRE_NON_NULL(l1_cache);
    M_REQUIRE_NON_NULL(l2_cache);
    M_REQUIRE(replace == LRU, ERR_BAD_PARAMETER, "%s", "Non existing replacement policy");

    phy_addr_t w_paddr = *paddr;
    w_paddr.page_offset = (paddr->page_offset - (paddr->page_offset % sizeof(word_t)));
    word_t word;
    cache_read(mem_space, paddr, DATA, l1_cache, l2_cache, &word, replace);
    word |= (p_byte >> (w_paddr.page_offset % sizeof(word_t)));
    // cache_write(mem_space, &w_paddr, l1_cache, l2_cache, &word, replace)
    
    return ERR_NONE;
}               

static inline void recompute_ages(void* cache, cache_t cache_type, uint16_t cache_line, uint8_t way_index, uint8_t bool_cold_start, cache_replace_t replace) {
    if (cache_type == L1_ICACHE || cache_type == L1_DCACHE) {
        if (replace == LRU) {
            if (bool_cold_start) {
                debug_print("cold_start = %d \tage_increase", bool_cold_start);
                LRU_age_increase(l1_icache_entry_t, L1_ICACHE_WAYS, way_index, cache_line);
            } else {
                debug_print("cold_start = %d \tage_update", bool_cold_start);
                LRU_age_update(l1_icache_entry_t, L1_ICACHE_WAYS, way_index, cache_line);
            }
        }
    } else {
        if (replace == LRU) {
            if (bool_cold_start) {
                LRU_age_increase(l2_cache_entry_t, L2_CACHE_WAYS, way_index, cache_line);    
            } else {
                LRU_age_update(l2_cache_entry_t, L2_CACHE_WAYS, way_index, cache_line);
            }
        }
    }
}

static inline uint8_t find_oldest_way(void* cache, cache_t cache_type, uint16_t cache_line) {
    if (cache_type == L1_ICACHE || cache_type == L1_DCACHE) {
        uint8_t way_max = 128;
        uint8_t max = 0;
        foreach_way(i, L1_ICACHE_WAYS) {
            uint8_t age = cache_age(l1_icache_entry_t, L1_ICACHE_WAYS, cache_line, i);
            if (max < age) {
                max = age;
                way_max = i;
            }
        }
        return way_max;
    } else {
        uint8_t way_max = 128;
        uint8_t max = 0;
        foreach_way(i, L2_CACHE_WAYS) {
            uint8_t age = cache_age(l2_cache_entry_t, L2_CACHE_WAYS, cache_line, i);
            if (max < age) {
                max = age;
                way_max = i;
            }
        }
        return way_max;
    }
}
