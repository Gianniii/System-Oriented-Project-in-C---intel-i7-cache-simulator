/**
 * @memory.c
 * @brief memory management functions (dump, init from file, etc.)
 *
 * @author Jean-Cédric Chappelier
 * @date 2018-19
 */

#if defined _WIN32  || defined _WIN64
#define __USE_MINGW_ANSI_STDIO 1
#endif

#include "memory.h"
#include "page_walk.h"
#include "addr_mng.h"
#include "util.h" // for SIZE_T_FMT
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h> // for memset()
#include <inttypes.h> // for SCNx macros
#include <assert.h>

/**
 * @brief Reads contents of the file and puts them at dest.
 * @param filename name of binary file to load. Should contain precicely 4096 bytes.
 * @param dest pointer to the begining for the memory space into which the
 *        4096 bytes should be loaded. Should already be initialized!
 */
static inline int page_file_read(const char* filename, void* dest);

/**
 * @brief Converts our "physical address" to a valid pointer in the "memory"
 */
static inline void* paddr_to_ptr(void* mem_start, phy_addr_t paddr);

// ======================================================================
/**
 * @brief Tool function to print an address.
 *
 * @param show_addr the format how to display addresses; see addr_fmt_t type in memory.h
 * @param reference the reference address; i.e. the top of the main memory
 * @param addr the address to be displayed
 * @param sep a separator to print after the address (and its colon, printed anyway)
 *
 */
static void address_print(addr_fmt_t show_addr, const void* reference,
                          const void* addr, const char* sep)
{
    switch (show_addr) {
    case POINTER:
        (void)printf("%p", addr);
        break;
    case OFFSET:
        (void)printf("%zX", (const char*)addr - (const char*)reference);
        break;
    case OFFSET_U:
        (void)printf(SIZE_T_FMT, (const char*)addr - (const char*)reference);
        break;
    default:
        // do nothing
        return;
    }
    (void)printf(":%s", sep);
}

// ======================================================================
/**
 * @brief Tool function to print the content of a memory area
 *
 * @param reference the reference address; i.e. the top of the main memory
 * @param from first address to print
 * @param to first address NOT to print; if less that `from`, nothing is printed;
 * @param show_addr the format how to display addresses; see addr_fmt_t type in memory.h
 * @param line_size how many memory byted to print per stdout line
 * @param sep a separator to print after the address and between bytes
 *
 */
static void mem_dump_with_options(const void* reference, const void* from, const void* to,
                                  addr_fmt_t show_addr, size_t line_size, const char* sep)
{
    assert(line_size != 0);
    size_t nb_to_print = line_size;
    for (const uint8_t* addr = from; addr < (const uint8_t*) to; ++addr) {
        if (nb_to_print == line_size) {
            address_print(show_addr, reference, addr, sep);
        }
        (void)printf("%02"PRIX8"%s", *addr, sep);
        if (--nb_to_print == 0) {
            nb_to_print = line_size;
            putchar('\n');
        }
    }
    if (nb_to_print != line_size) putchar('\n');
}

// ======================================================================
// See memory.h for description
int vmem_page_dump_with_options(const void *mem_space, const virt_addr_t* from,
                                addr_fmt_t show_addr, size_t line_size, const char* sep)
{
#ifdef DEBUG
    debug_print("mem_space=%p\n", mem_space);
    (void)fprintf(stderr, __FILE__ ":%d:%s(): virt. addr.=", __LINE__, __func__);
    print_virtual_address(stderr, from);
    (void)fputc('\n', stderr);
#endif
    phy_addr_t paddr;
    zero_init_var(paddr);

    M_EXIT_IF_ERR(page_walk(mem_space, from, &paddr),
                  "calling page_walk() from vmem_page_dump_with_options()");
#ifdef DEBUG
    (void)fprintf(stderr, __FILE__ ":%d:%s(): phys. addr.=", __LINE__, __func__);
    print_physical_address(stderr, &paddr);
    (void)fputc('\n', stderr);
#endif

    const uint32_t paddr_offset = ((uint32_t) paddr.phy_page_num << PAGE_OFFSET);
    const char * const page_start = (const char *)mem_space + paddr_offset;
    const char * const start = page_start + paddr.page_offset;
    const char * const end_line = start + (line_size - paddr.page_offset % line_size);
    const char * const end   = page_start + PAGE_SIZE;
    debug_print("start=%p (offset=%zX)\n", (const void*) start, start - (const char *)mem_space);
    debug_print("end  =%p (offset=%zX)\n", (const void*) end, end   - (const char *)mem_space) ;
    mem_dump_with_options(mem_space, page_start, start, show_addr, line_size, sep);
    const size_t indent = paddr.page_offset % line_size;
    if (indent == 0) putchar('\n');
    address_print(show_addr, mem_space, start, sep);
    for (size_t i = 1; i <= indent; ++i) printf("  %s", sep);
    mem_dump_with_options(mem_space, start, end_line, NONE, line_size, sep);
    mem_dump_with_options(mem_space, end_line, end, show_addr, line_size, sep);
    return ERR_NONE;
}

int mem_init_from_dumpfile(const char* filename, void** memory, size_t* mem_capacity_in_bytes) {
    M_REQUIRE_NON_NULL(filename);
    M_REQUIRE_NON_NULL(memory);
    M_REQUIRE_NON_NULL(mem_capacity_in_bytes);

    FILE* file = fopen(filename, "r");
    M_REQUIRE_NON_NULL_CUSTOM_ERR(file, ERR_IO);

    // Determines the size of the file.
    fseek(file, 0L, SEEK_END);
    *mem_capacity_in_bytes = (size_t) ftell(file);
    rewind(file);

    M_EXIT_IF(*mem_capacity_in_bytes % PAGE_SIZE != 0, ERR_BAD_PARAMETER, "%s does not contain a multiple of PAGE_SIZE lines", filename);

    // Allocates the memory and finalises fields and closes the file if an error occurs.
    if ((*memory = calloc(*mem_capacity_in_bytes, 1)) == NULL) {
        *mem_capacity_in_bytes = 0;
        fclose(file);
        M_EXIT_ERR(ERR_MEM, "mem_init_from_dumpfile - Failed to allocate \
                memory of size %zu bytes", *mem_capacity_in_bytes);
    }

    // Reads the file contents and finalises fields and closes the file if an error occurs.
    if (*mem_capacity_in_bytes != fread(*memory, 1, *mem_capacity_in_bytes, file)) {
        fclose(file);
        free(*memory);
        *memory = NULL;
        *mem_capacity_in_bytes = 0;
        M_EXIT_ERR(ERR_IO, "mem_init_from_dumpfile - Failed to read memory \
                contents of size %zu bytes", *mem_capacity_in_bytes);
    }

    fclose(file);
    return ERR_NONE;
}

#define _STR(x) #x
#define STR(x) _STR(x)

int mem_init_from_description(const char* master_filename, void** memory, size_t* mem_capacity_in_bytes) {
    M_REQUIRE_NON_NULL(master_filename);
    M_REQUIRE_NON_NULL(memory);
    M_REQUIRE_NON_NULL(mem_capacity_in_bytes);
    debug_print("master_filename = %s", master_filename);

    // Open the master_file
    FILE* master_file = fopen(master_filename, "r");
    M_REQUIRE_NON_NULL_CUSTOM_ERR(master_file, ERR_IO);

    // Read TOTAL MEMORY SIZE
    if(fscanf(master_file, " %zu ", mem_capacity_in_bytes) != 1) {
        fclose(master_file);
        M_EXIT_ERR(ERR_IO, "%s", "mem_init_from_description() read TOTAL MEMORY SIZE failed");
    }
    debug_print("*mem_capacity_in_bytes = %zu", *mem_capacity_in_bytes);

    // Allocate the memory
    if ((*memory = calloc(*mem_capacity_in_bytes, 1)) == NULL) {
        fclose(master_file);
        M_EXIT_ERR(ERR_MEM, "mem_init_from_description() - Failed to allocate \
                memory of size %zu bytes", *mem_capacity_in_bytes);
    }
    debug_print("*memory = %p", *memory);

    // Macro to exit, close file and free memory if an error is thrown in function call bellow
    #define M_MEMORY_DESC_EXIT_IF(test, err) \
        if (test) { \
            fclose(master_file); \
            free(*memory); \
            *memory = NULL; \
            M_EXIT_ERR_NOMSG(err); \
        }

    // Read PGD PAGE FILENAME
    char target_filename[FILENAME_MAX];
    M_MEMORY_DESC_EXIT_IF(fscanf(master_file, " %"STR(FILENAME_MAX)"s ", target_filename) != 1, ERR_IO);
    debug_print("pgd_filename = %s", target_filename);

    // Read and load the PGD_PAGE
    int err;
    M_MEMORY_DESC_EXIT_IF((err = page_file_read(target_filename, *memory)) != ERR_NONE, err);
    memset(target_filename, 0, FILENAME_MAX);

    // Read NUMBER OF TRANSLATION PAGES
    size_t n_translation_pages;
    M_MEMORY_DESC_EXIT_IF(fscanf(master_file, " %zu ", &n_translation_pages) != 1, ERR_IO)
    debug_print("n_translation_pages = %zu", n_translation_pages);

    // Load all translation tables
    for (size_t i = 0; i < n_translation_pages; i++) {
        uint32_t index_offset;
        M_MEMORY_DESC_EXIT_IF(fscanf(master_file, " 0x%"SCNx32" ", &index_offset) != 1, ERR_IO);
        M_MEMORY_DESC_EXIT_IF(fscanf(master_file, " %"STR(FILENAME_MAX)"s ", target_filename) != 1, ERR_IO);
        debug_print("translation_page %zu : index_offset = %x\ttp_filename = %s", i, index_offset, target_filename);

        // Casts are optional, but we use them since void* pointer arithmetic causes warnings at compilation
        M_MEMORY_DESC_EXIT_IF((err = page_file_read(target_filename, (void*)((uint64_t)*memory + (uint64_t)index_offset))), err);
        memset(target_filename, 0, FILENAME_MAX);
    }

    #ifdef DEBUG // Counter used by the loop below to have more coherent debug information
    size_t debug_counter = 0;
    #endif
    // Load all Data tables
    while (!feof(master_file)) {
        uint64_t vaddr64;
        M_MEMORY_DESC_EXIT_IF(fscanf(master_file, " 0x%"SCNx64" ", &vaddr64) != 1, ERR_IO);
        M_MEMORY_DESC_EXIT_IF(fscanf(master_file, " %"STR(FILENAME_MAX)"s ", target_filename) != 1, ERR_IO);

        debug_print("data_page %zu : vaddr64 = %"SCNx64"\tdata_filename = %s", debug_counter++, vaddr64, target_filename);

        virt_addr_t vaddr;
        M_MEMORY_DESC_EXIT_IF((err = init_virt_addr64(&vaddr, vaddr64)) != ERR_NONE, err);
        phy_addr_t paddr;
        M_MEMORY_DESC_EXIT_IF((err = page_walk(*memory, &vaddr, &paddr)) != ERR_NONE, err);

        M_MEMORY_DESC_EXIT_IF((err = page_file_read(target_filename, paddr_to_ptr(*memory, paddr))), err);
        memset(target_filename, 0, FILENAME_MAX);
    }

    fclose(master_file);

    return ERR_NONE;
    #undef M_MEMORY_DESC_EXIT_IF
}

#undef _STR
#undef STR

static inline int page_file_read(const char* filename, void* dest) {
    FILE* file = fopen(filename, "r");
    M_REQUIRE_NON_NULL_CUSTOM_ERR(file, ERR_IO);

    debug_print("filename= %s\tdest= %p", filename, dest);

    if (PAGE_SIZE != fread(dest, 1, PAGE_SIZE, file)) {
        fclose(file);
        M_EXIT_ERR(ERR_IO, "page_file_read - Failed to read memory \
                contents of size %d bytes", PAGE_SIZE);
    }

    fclose(file);
    return ERR_NONE;
}

static inline void* paddr_to_ptr(void* mem_start, phy_addr_t paddr) {
    return  (void*) (
              ((uint64_t) mem_start)
            + ((((uint64_t) paddr.phy_page_num) << PAGE_OFFSET)
            | ((uint64_t)paddr.page_offset))
        );
}
