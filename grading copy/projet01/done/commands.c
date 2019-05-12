#include <stdlib.h>
#include <inttypes.h>
#include "commands.h"
#include "addr.h"
#include "addr_mng.h"	
#include "error.h"

static inline int program_resize(program_t* program, size_t to_allocate);

static inline command_t read_command(FILE*);

static inline void handle_write(FILE* input, command_t* command);

static inline void handle_read(FILE* input, command_t* command);

static inline void handle_write(FILE* input, command_t* command);

static inline void handle_instruction(FILE* input, command_t* command);

static inline void set_vaddr(FILE* input, command_t* command);

static inline void skip_whitespaces(FILE* input);

static inline void handle_read_data(FILE* input, command_t* command);

static inline void handle_write_data(FILE* input, command_t* command);

static inline void set_write_data(FILE* input, command_t* command);

static inline void set_data_size(FILE* input, command_t* command); 


int program_init(program_t* program){
    M_REQUIRE_NON_NULL(program);

    M_EXIT_IF_NULL(program->listing = calloc(LISTING_PADDING, sizeof(command_t)), sizeof(command_t));

    program->nb_lines = 0;
    // correcteur: allocated needs to represent the amount of allocated memory in byte as you did in program_resize (see https://moodle.epfl.ch/mod/forum/discuss.php?d=16922)
    program->allocated = LISTING_PADDING;
    
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
        // correcteur: definie constants/macros for R, W, ...
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

    // correcteur: non-compliant with the specified behaviour (see the handout)
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

    // correcteur: missing check of invalid order (hint: authorized enum value)
    // correcteur: missing check of invalid type
    // correcteur: missing check of invalid write (hint: cannot write instruction)
    // correcteur: missing check of invalid write (hint: write data too large for write size)

    if(command->type == INSTRUCTION) {
        M_REQUIRE(command->data_size == sizeof(word_t), ERR_BAD_PARAMETER, "%s", 
                  "data size incorrect for an instruction");
        M_REQUIRE(command->order == READ, ERR_BAD_PARAMETER, "%s", "instruction are read only");
    } 
    if(command->type == DATA) {
        // correcteur: this test should be done whatever the command type is
        M_REQUIRE(command->data_size == sizeof(word_t) || command->data_size == sizeof(char), 
                  ERR_BAD_PARAMETER, "%s", "data_size incorrect for data access");
    }

    M_REQUIRE((command->vaddr.page_offset % command->data_size) == 0, ERR_BAD_PARAMETER,
               "%s", "page_offset must be a multiple of data size");
               
    // Week 6: Dynamic allocation. Adds the command to our and enlarges listing if its too small.
    // correcteur: needs to be adapted once program_init fixed
    while (program->nb_lines >= program->allocated) {
        // correcteur: missing overflow check
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
    // correcteur: missing handling of error code of program_init
    program_init(program);

    FILE* input = NULL;
    input = fopen(filename, "r");
    M_REQUIRE_NON_NULL(input);
    //read command 1 by 1 and add it to program
    while(!feof(input) && !ferror(input)) {
        skip_whitespaces(input);
        // correcteur: missing detection of command reading failure, i.e. empty command in your case (hint: would be cleaner to make read_command return an error code)
        command_t command = read_command(input);
        // correcteur: missing handling of error code of program_add_command
        program_add_command(program, &command);
        //skip whitespaces just incase there are white spaces left at the end of the file
        skip_whitespaces(input);
    }
    
    // correcteur: missing check of ferror and propagation of ERR_IO
    fclose(input) ;

    // correcteur: missing program shrinking -2

    return ERR_NONE; /* pour le moment... en attendant le cours 10 */
}

// correcteur: add another parameter (commant_t*) and make read_command (and every auxiliary function) return an error code so that you can propagate any error
command_t read_command(FILE* input){
    command_t newCommand;
    char order = fgetc(input); //!= EOF
    if(order == 'R') {
        newCommand.order = READ;
        handle_read(input, &newCommand);
    } else if(order == 'W') {
        newCommand.order = WRITE;
        handle_write(input, &newCommand);
    } else {    
        memset(&newCommand, 0, sizeof(command_t));
        fprintf(stderr, "read_command() - Invalid argument");
    }
    
    return newCommand;
}

void handle_read(FILE* input, command_t* command){
    skip_whitespaces(input);
    char type = fgetc(input);
    // correcteur: missing default case to detect garbage or EOF
    if(type == 'I') {
        command->type = INSTRUCTION;
        handle_instruction(input, command);
    } else if(type == 'D') {
        command->type = DATA;
        handle_read_data(input, command);
    }
    // correcteur: missing initialization of write_data to a default value (hint: 0)
}

void handle_write(FILE* input, command_t* command){
    skip_whitespaces(input);
    char type = fgetc(input);
    // correcteur: missing default case to detect garbage or EOF
    if(type == 'I') {
        command->type = INSTRUCTION;
        handle_instruction(input, command);
    } else if(type == 'D') {
        command->type = DATA;
        handle_write_data(input, command);
    }
}

void handle_instruction(FILE* input, command_t* command) {
    skip_whitespaces(input);
    command->data_size = sizeof(word_t); // by default instructions data size is a word
    set_vaddr(input, command);
    
}
//does the necessary steps the command is of order read and of type data
void handle_read_data(FILE* input, command_t* command) {
    skip_whitespaces(input);
    set_data_size(input, command);
    set_vaddr(input, command);
}

//does the necessary steps the command is of order write and of type data
void handle_write_data(FILE* input, command_t* command) {
    skip_whitespaces(input);
    set_data_size(input, command);
    set_write_data(input, command);
    set_vaddr(input, command);
}

//assumes next thing to read is data, and sets it in the command
void set_data_size(FILE* input, command_t* command) {
    skip_whitespaces(input);
    char data_size = fgetc(input);
    // correcteur: missing default case to detect garbage or EOF
    if(data_size == 'B') {
        command->data_size = sizeof(char); // 1 byte
    } 
    if(data_size == 'W') {
        command->data_size = sizeof(word_t);
    } 
}
//assumes next thing to read is is vaddr, and sets in the command
void set_vaddr(FILE* input, command_t* command){
     skip_whitespaces(input);
     uint64_t addr = 0;
     // correcteur: missing detection of EOF
     fscanf(input, "@0x%"SCNx64, &addr);
     // correcteur: missing test and propagation of error code of init_virt_addr64
     init_virt_addr64(&command->vaddr, addr);
}
//assumes next thing to read is is write data, and sets in the command
void set_write_data(FILE* input, command_t* command) {
    skip_whitespaces(input);
    word_t word = 0;
    // correcteur: missing detection of EOF
    fscanf(input, "%"SCNx32, &word);
    command->write_data = word;
}

//skip whitespaces if there are 
void skip_whitespaces(FILE* input){
    fscanf(input, " ");
}
