// TODO Verify and do we need to include anything?

#include "cache.h"
#include "cache_mng.h"

#define LRU_age_increase(TYPE, WAYS, WAY_INDEX, LINE_INDEX) \
    foreach_way(m_way_iterator, WAYS) { \
        TYPE* cache_e = cache_entry(TYPE, WAYS, LINE_INDEX, m_way_iterator); \
        if (m_way_iterator == WAY_INDEX) { \
            cache_e->age = 0; \
        } else { \
            if(cache_e->age < WAYS - 1) cache_e->age =  cache_e->age + 1; \
        } \
    }

#define LRU_age_update(TYPE, WAYS, WAY_INDEX, LINE_INDEX) \
    uint8_t temp = cache_age(TYPE, WAYS, LINE_INDEX, WAY_INDEX); \
    foreach_way(m_way_iterator, WAYS) { \
        TYPE* cache_e = cache_entry(TYPE, WAYS, LINE_INDEX, m_way_iterator); \
        if (m_way_iterator == WAY_INDEX) { \
            cache_e->age = 0; \
        } else if (cache_e->age < temp) { \
            cache_e->age++; \
        } \
    }
