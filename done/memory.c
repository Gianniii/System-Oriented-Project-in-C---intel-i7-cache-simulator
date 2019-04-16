#include <stdio.h>
#include "memory.h"
#include "error.h"

#define PAGE_SIZE 4096

/**
 * @brief Reads contents of the file and puts them at dest.
 * @param filename name of binary file to load. Should contain precicely 4096 bytes.
 * @param dest pointer to the begining for the memory space into which the 4096 bytes should be loaded.
 */
static inline int page_file_read(const char* filename, void* dest) {
    FILE* file = NULL;
    M_REQUIRE_NON_NULL_CUSTOM_ERR(file = fopen(filename, "r"), ERR_IO);

    if ((dest = calloc(PAGE_SIZE, 1)) == NULL) {
        fclose(file);
        M_EXIT_ERR(ERR_MEM, "page_file_read - Failed to allocate \
                memory of size %zu bytes", *mem_capacity_in_bytes);
    }

    if (PAGE_SIZE != fread(dest, 1, PAGE_SIZE, file)) {
        fclose(file);
        M_EXIT_ERR(ERR_IO, "page_file_read - Failed to read memory \
                contents of size %zu bytes", *mem_capacity_in_bytes);
    }

    fclose(file);    
}

int mem_init_from_dumpfile(const char* filename, void** memory, size_t* mem_capacity_in_bytes) {
    M_REQUIRE_NON_NULL(filename);
    M_REQUIRE_NON_NULL(memory);
    M_REQUIRE_NON_NULL(mem_capacity_in_bytes);

    FILE* file = NULL;
    M_REQUIRE_NON_NULL_CUSTOM_ERR(file = fopen(filename, "r"), ERR_IO);

    // va tout au bout du fichier
    fseek(file, 0L, SEEK_END);
    // indique la position, et donc la taille (en octets)
    *mem_capacity_in_bytes = (size_t) ftell(file);
    // revient au début du fichier (pour le lire par la suite)
    rewind(file);

    if ((*memory = calloc(*mem_capacity_in_bytes, 1)) == NULL) {
        fclose(file);
        M_EXIT_ERR(ERR_MEM, "mem_init_from_dumpfile - Failed to allocate \
                memory of size %zu bytes", *mem_capacity_in_bytes);
    }
    
    // TODO Remove
    // // TODO use single function. NO Need to loop!
    // for (size_t i = 0; i < *mem_capacity_in_bytes; i++) {
    //     *memory[i] = getc(file);
    // }

    if (*mem_capacity_in_bytes != fread(*memory, 1, *mem_capacity_in_bytes, file)) {
        fclose(file);
        M_EXIT_ERR(ERR_IO, "mem_init_from_dumpfile - Failed to read memory \
                contents of size %zu bytes", *mem_capacity_in_bytes);
    }

    fclose(file);

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
    M_REQUIRE_NON_NULL(master_filename);
    M_REQUIRE_NON_NULL(memory);
    M_REQUIRE_NON_NULL(mem_capacity_in_bytes);

    FILE* master_file = NULL;
    M_REQUIRE_NON_NULL_CUSTOM_ERR(master_file = fopen(master_filename, "r"), ERR_IO);
    
    *mem_capacity_in_bytes = fscanf(master_file, " %zu "); // TODO how to handle whitespaces
    // pdgfilename
    size_t n_translation_pages;
    // --

    return ERR_NONE;
}
