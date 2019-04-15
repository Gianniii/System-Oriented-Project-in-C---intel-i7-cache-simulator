#include <stdio.h>
#include "memory.h"
#include "error.h"

int mem_init_from_dumpfile(const char* filename, void** memory, size_t* mem_capacity_in_bytes) {
    M_REQUIRE_NON_NULL(filename);
    M_REQUIRE_NON_NULL(memory);
    M_REQUIRE_NON_NULL(mem_capacity_in_bytes);

    FILE* file = NULL;
    M_REQUIRE_NON_NULL(file = fopen(filename, "r"));
    // @Michael or should this have M_EXIT_ERR_NOMSG(ERR_IO)

    // va tout au bout du fichier
    fseek(file, 0L, SEEK_END);
    // indique la position, et donc la taille (en octets)
    *mem_capacity_in_bytes = (size_t) ftell(file);
    // revient au début du fichier (pour le lire par la suite)
    rewind(file);

    // TODO Make sure file is closed
    M_EXIT_IF_NULL(*memory = calloc(*mem_capacity_in_bytes, 1), *mem_capacity_in_bytes);

    // TODO use single function. NO Need to loop!
    for (size_t i = 0; i < *mem_capacity_in_bytes; i++) {
        memory[i] = getc(file);
    }

    return ERR_NONE;
}

/**
 * @brief Create and initialize the whole memory space from a provided
 * (metadata text) file containing an description of the memory.
 * Its format is:
 *  line1:           TOTAL MEMORY SIZE (size_t)
 *  line2:           PGD PAGE FILENAME
 *  line4:           NUMBER N OF TRANSLATION PAGES (PUD+PMD+PTE)
 *  lines5 to (5+N): LIST OF TRANSLATION PAGES, expressed with two info per line:
 *                       INDEX OFFSET (uint32_t in hexa) and FILENAME
 *  remaining lines: LIST OF DATA PAGES, expressed with two info per line:
 *                       VIRTUAL ADDRESS (uint64_t in hexa) and FILENAME
 *
 * @param filename the name of the memory content description file to read from
 * @param memory (modified) pointer to the begining of the memory
 * @param mem_capacity_in_bytes (modified) total size of the created memory
 * @return error code, *p_memory shall be NULL in case of error
 *
 */
int mem_init_from_description(const char* master_filename, void** memory, size_t* mem_capacity_in_bytes) {
    return ERR_NONE;
}
