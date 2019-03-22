#pragma once

/**
 * @file commands.h
 * @brief Processor commands(/instructions) simulation
 *
 * @author Jean-Cédric Chappelier
 * @date 2018-19
 */

#include "mem_access.h" // for mem_access_t
#include "addr.h" // for virt_addr_t
#include <stdio.h> // for size_t, FILE
#include <stdint.h> // for uint32_t

/* TODO WEEK 05:
 * Définir ici les types
 *      command_word_t
 *      command_t
 *  et program_t
 * (et supprimer ces sept lignes de commentaire).
 * 
 * 
 */
 
 typedef enum cwords{Read, Write} command_word_t;
 
 typedef struct{
	 command_word_t order;  //indique lecture ou écriture
	 mem_access_t type;     //type d'iformation chercher(instruction ou donnée)
	 size_t data_size;      //indique taille en cotect des donnée manipuler(mot ou byte)
	 word_t write_data;     //si necessaire, valeur à écrire
	 virt_addr_t vaddr;     //addresse virtuelle oue acceder
 }command_t;
 
 typedef struct{
	 command_t listing[100]; //contient les commandes
	 size_t nb_lines;        //nombres de commandes dans listing
	 size_t allocated;       //sem6
 }program_t;

/**
 * @brief A useful macro to loop over all program lines.
 * X is the name of the variable to be used for the line;
 * and P is the program to be looped over.
 * X will be of type `const command_t*`
 * and P has to be of type `program_t*`.
 *
 * Example usage:
 *    for_all_lines(line, program) { do_something_with(line); }
 *
 */
#define for_all_lines(X, P) const command_t* end_pgm_ = (P)->listing + (P)->nb_lines; \
    for(const command_t* X = (P)->listing; X < end_pgm_; ++X)


/**
 * @brief "Constructor" for program_t: initialize a program.
 * @param program (modified) the program to be initialized.
 * @return ERR_NONE of ok, appropriate error code otherwise.
 */
int program_init(program_t* program);

/**
 * @brief add a command (line) to a program. Reallocate memory if necessary.
 * @param program (modified) the program where to add to.
 * @param command the command to be added.
 * @return ERR_NONE of ok, appropriate error code otherwise.
 */
int program_add_command(program_t* program, const command_t* command);

/**
 * @brief Tool function to down-reallocate memory to the minimal required size. Typically used once a program will no longer be extended.
 * @param program (modified) the program to be rescaled.
 * @return ERR_NONE of ok, appropriate error code otherwise.
 */
int program_shrink(program_t* program);

/**
 * @brief Print the content of a program to a stream.
 * @param output the stream to print to.
 * @param program the program to be printed.
 * @return ERR_NONE of ok, appropriate error code otherwise.
 */
int program_print(FILE* output, const program_t* program);

/**
 * @brief Read a program (list of commands) from a file.
 * @param filename the name of the file to read from.
 * @param program the program to be filled from file.
 * @return ERR_NONE of ok, appropriate error code otherwise.
 */
int program_read(const char* filename, program_t* program);
