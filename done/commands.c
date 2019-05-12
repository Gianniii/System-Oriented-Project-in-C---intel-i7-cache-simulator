#include <stdlib.h>
#include <inttypes.h>
#include "commands.h"
#include "addr.h"
#include "addr_mng.h"	
#include "error.h"

static inline int program_resize(program_t* program, size_t to_allocate);

static inline int read_command(FILE*, command_t* command);

static inline int handle_read(FILE* input, command_t* command);

static inline int handle_write(FILE* input, command_t* command);

static inline int handle_instruction(FILE* input, command_t* command);

static inline int set_vaddr(FILE* input, command_t* command);

static inline void skip_whitespaces(FILE* input);

static inline int handle_read_data(FILE* input, command_t* command);

static inline int handle_write_data(FILE* input, command_t* command);

static inline void set_write_data(FILE* input, command_t* command);

static inline int set_data_size(FILE* input, command_t* command); 


int program_init(program_t* program){
    M_REQUIRE_NON_NULL(program);

    M_EXIT_IF_NULL(program->listing = calloc(LISTING_PADDING, sizeof(command_t)), sizeof(command_t));

    program->nb_lines = 0;
    program->allocated = LISTING_PADDING * sizeof(command_t);
    
    return ERR_NONE;
}

int program_free(program_t* program) {
    M_REQUIRE_NON_NULL(program);
    M_REQUIRE_NON_NULL(program->listing);

    free(program->listing);
    program->listing = NULL;
    program->nb_lines = program->allocated = 0;

    return ERR_NONE;
}

/**
 * @brief Helper method that resizes the listing of the program.
 * @param program (modified) the program who's listing to resize.
 * @param to_allocate the desired new size of the listing array.
 */
int program_resize(program_t* program, size_t to_allocate) {
    M_REQUIRE_NON_NULL(program);
    M_REQUIRE_NON_NULL(program->listing);

    size_t new_size = to_allocate * sizeof(command_t);
    M_EXIT_IF_NULL(program->listing = realloc(program->listing, new_size), new_size); 
    program->allocated = new_size;

    return ERR_NONE;
}

int program_print(FILE* output, const program_t* program){
    M_REQUIRE_NON_NULL(output);
    M_REQUIRE_NON_NULL(program);
    M_REQUIRE_NON_NULL(program->listing);
      
     //fprintf every entry(command) in the listing
     size_t i;
     for(i = 0; i < program->nb_lines; ++i) {
        command_t command = program->listing[i];
        //output if its a Read or Write
        if(command.order == READ) {
            fprintf(output, "R");
        } else {
            fprintf(output, "W");
        }
         //if command type is an Instruction just print I followed by the address and return
        if(command.type == INSTRUCTION) {
            //if its an instruction can just print  I + addr;
            fprintf(output, " I");
            uint64_t addr = virt_addr_t_to_uint64_t(&command.vaddr);
            fprintf(output, " @0x%016" PRIX64, addr);  
            fprintf(output, "\n");
            continue; //continue on next command
        } else {
            fprintf(output, " D");
        }
        
        //if its command type is data 
        //output data size
        if(command.data_size == sizeof(char)){
            fprintf(output, "B"); 
        } else {
            fprintf(output, "W");
        }
        //if its a write output the write data
        if(command.order == WRITE) {
            if(command.data_size == sizeof(char)) {
                 fprintf(output, " 0x%02" PRIX32, command.write_data);
            } else {
                fprintf(output, " 0x%08" PRIX32, command.write_data);
            }
        }
        //finally print vaddr and \n
        uint64_t addr = virt_addr_t_to_uint64_t(&command.vaddr);
            fprintf(output, " @0x%016" PRIX64, addr);  
        
        fprintf(output, "\n");
     }
     
     return ERR_NONE;	 
}

int program_shrink(program_t* program){
    M_REQUIRE_NON_NULL(program);
    M_REQUIRE_NON_NULL(program->listing);

    if (program->nb_lines <= LISTING_PADDING && program->allocated > LISTING_PADDING) {
        M_EXIT_IF_ERR(program_resize(program, LISTING_PADDING),
                "program_resize() failed. Cannot resize listing");
    } else if (program->nb_lines != program->allocated) {
        M_EXIT_IF_ERR(program_resize(program, program->nb_lines),
                "program_resize() failed. Cannot resize listing");
    }

    return ERR_NONE;
}

int program_add_command(program_t* program, const command_t* command){
    M_REQUIRE_NON_NULL(command);
    M_REQUIRE_NON_NULL(program);
    M_REQUIRE_NON_NULL(program->listing);
    if(command->type == INSTRUCTION) {
        M_REQUIRE(command->data_size == sizeof(word_t), ERR_BAD_PARAMETER, "%s", 
                  "data size incorrect for an instruction");
        M_REQUIRE(command->order == READ, ERR_BAD_PARAMETER, "%s", "instruction are read only");
    } 
    if(command->type == DATA) {
        M_REQUIRE(command->data_size == sizeof(word_t) || command->data_size == sizeof(char), 
                  ERR_BAD_PARAMETER, "%s", "data_size incorrect for data access");
    }

    M_REQUIRE((command->vaddr.page_offset % command->data_size) == 0, ERR_BAD_PARAMETER,
               "%s", "page_offset must be a multiple of data size");
               
    // Week 6: Dynamic allocation. Adds the command to our and enlarges listing if its too small.
    while (program->nb_lines * sizeof(command_t) >= program->allocated) {
        M_EXIT_IF_ERR(program_resize(program, program->allocated * program->allocated), 
                "program_resize() failed. Cannot resize listing");
    }
    program->listing[program->nb_lines] = *command;
    ++program->nb_lines;

    return ERR_NONE;
}

int program_read(const char* filename, program_t* program){
    M_REQUIRE_NON_NULL(program);
    M_REQUIRE_NON_NULL(filename);
    program_init(program);

    FILE* input = NULL;
    input = fopen(filename, "r");
    M_REQUIRE_NON_NULL(input);
    //read command 1 by 1 and add it to program
    while(!feof(input) && !ferror(input)) {
        skip_whitespaces(input);
        command_t command;
        int error = read_command(input, &command);
        M_REQUIRE(error == 1, ERR_IO, "%s", "bad input");
        program_add_command(program, &command);
        //skip whitespaces just incase there are white spaces left at the end of the file
        skip_whitespaces(input);
    }
    
    fclose(input) ;
    
    program_shrink(program);

    return ERR_NONE;
}

int read_command(FILE* input, command_t* newCommand){
    int error = 1;
    char order = fgetc(input); //!= EOF
    if(order == 'R') {
        newCommand->order = READ;
        error = handle_read(input, newCommand);
    } else if(order == 'W') {
        newCommand->order = WRITE;
        error = handle_write(input, newCommand);
    } else {    
        memset(&newCommand, 0, sizeof(command_t));
        error = 0;
        fprintf(stderr, "read_command() - Invalid argument");
    }
    
    return error;
}

int handle_read(FILE* input, command_t* command){
	int error = 1;
    skip_whitespaces(input);
    char type = fgetc(input);
    if(type == 'I') {
        command->type = INSTRUCTION;
        error = handle_instruction(input, command);
    } else if(type == 'D') {
        command->type = DATA;
        error = handle_read_data(input, command);
    } else {
		error = 0;
	}
	return error;
}

int handle_write(FILE* input, command_t* command){
	int error = 1;
    skip_whitespaces(input);
    char type = fgetc(input);
    if(type == 'I') {
        command->type = INSTRUCTION;
        handle_instruction(input, command);
    } else if(type == 'D') {
        command->type = DATA;
        handle_write_data(input, command);
    } else {
		error = 0;
	}
	return error;
}

int handle_instruction(FILE* input, command_t* command) {
	int error = 1;
    skip_whitespaces(input);
    command->data_size = sizeof(word_t); // by default instructions data size is a word
    error = set_vaddr(input, command);
    return error;
    
}
//does the necessary steps the command is of order read and of type data
int handle_read_data(FILE* input, command_t* command) {
	int error = 1;
    skip_whitespaces(input);
    error = set_data_size(input, command);
    error = set_vaddr(input, command);
    return error;
}

//does the necessary steps the command is of order write and of type data
int handle_write_data(FILE* input, command_t* command) {
	int error = 0;
    skip_whitespaces(input);
    error = set_data_size(input, command);
    set_write_data(input, command);
    error = set_vaddr(input, command);
    return error;
}

//assumes next thing to read is data, and sets it in the command
int set_data_size(FILE* input, command_t* command) {
    skip_whitespaces(input);
    int error = 1;
    char data_size = fgetc(input);
    if(data_size == 'B') {
        command->data_size = sizeof(char); // 1 byte
    } 
    if(data_size == 'W') {
        command->data_size = sizeof(word_t);
    } else {
		error = 0;
	}
	return error;
}
//assumes next thing to read is is vaddr, and sets in the command
int set_vaddr(FILE* input, command_t* command){
     skip_whitespaces(input);
     uint64_t addr = 0;
     fscanf(input, "@0x%"SCNx64, &addr);
     int error;
     error = init_virt_addr64(&command->vaddr, addr);
     return (error == ERR_NONE) ? 1 : 0;
}
//assumes next thing to read is is write data, and sets in the command
void set_write_data(FILE* input, command_t* command) {
    skip_whitespaces(input);
    word_t word = 0;
    fscanf(input, "%"SCNx32, &word);
    command->write_data = word;
}

//skip whitespaces if there are 
void skip_whitespaces(FILE* input){
    fscanf(input, " ");
}
