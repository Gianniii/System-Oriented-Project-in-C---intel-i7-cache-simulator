#include <stdio.h>
#include "addr.h"
#include "addr_mng.h"	
#include "error.h"
#include "inttypes.h"
#include "commands.h"

int program_init(program_t* program){
    M_REQUIRE_NON_NULL(program);

    M_EXIT_IF_NULL(program->listing = calloc(LISTING_PADDING, sizeof(command_t)), sizeof(command_t));

    program->nb_lines = 0;
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

// Helper method
int program_resize(program_t* program, size_t to_allocate) {
    M_REQUIRE_NON_NULL(program);
    M_REQUIRE_NON_NULL(program->listing);

    M_EXIT_IF_TOO_LONG(to_allocate, SIZE_MAX);

    // TODO @Michael Review stuff below
    // This is basic.
    // size_t new_size = to_allocate *sizeof(command_t);
    // M_EXIT_IF_NULL(program->listing = realloc(program->listing, new_size, new_size);

    // Below is a more "stable" approach

    command_t* const old_listing = program->listing;
    size_t old_allocated = program->allocated;

    program->allocated = to_allocate;
    if ((program->listing = realloc(program->listing, program->allocated * sizeof(command_t))) == NULL) {
        program->listing = old_listing;
        program->allocated = old_allocated;

        M_EXIT_ERR_NOMSG(ERR_MEM); // The "error" is handled so is this return correct?
    }

    return ERR_NONE;
}

int program_print(FILE* output, const program_t* program){
    //make sure output is non_null
    if(output == NULL) {
        return ERR_BAD_PARAMETER;
    }
    // TODO @Michael How about this?
    // M_REQUIRE_NON_NULL(output);
    // M_REQUIRE_NON_NULL(program);
      
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
            //print_virtual_address(output, &command.vaddr);
            uint64_t addr = virt_addr_t_to_uint64_t(&command.vaddr);
            fprintf(output, " @0x%016" PRIX64, addr);  
            fprintf(output, "\n");
            continue; //continue on next commant 
        } else {
            fprintf(output, " D");
        }
        
        //if its command type is data 
        //output data size
        if(command.data_size == sizeof(char)){
            fprintf(output, "B"); 
        } else {
            fprintf(output, "W"); //DOES WORD INCLUDE ALL OTHER SIZES OR NEEDA BE POWER OF 2 ??
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
    return ERR_NONE;
}

int program_add_command(program_t* program, const command_t* command){
    //TODO: other checks ? 
    M_REQUIRE_NON_NULL(command);
    M_REQUIRE_NON_NULL(program);
    if(command->type == INSTRUCTION) {
        M_REQUIRE(command->data_size == sizeof(word_t), ERR_BAD_PARAMETER,
                  "data size incorrect for an instruction", "");
        M_REQUIRE(command->order == READ, ERR_BAD_PARAMETER, "instruction are read only" , "");
    } 
    if(command->type == DATA) {
        M_REQUIRE(command->data_size == sizeof(word_t) || command->data_size == sizeof(char), 
                  ERR_BAD_PARAMETER, "data_size incorrect for data access", "");
    }

    M_REQUIRE((command->vaddr.page_offset % command->data_size) == 0, ERR_BAD_PARAMETER,
               "page_offset must be a multiple of data size", "");
     
    M_REQUIRE(program->nb_lines < SIZE_OF_LISTING - 1, ERR_MEM, "programin listing is full" , "");
    //add command to the program //ONLY FOR WEEK 5 SINCE LISTING IS STATIC
    if(program->nb_lines+1 >= SIZE_OF_LISTING) return ERR_MEM;
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
        command_t command = read_command(input);
        program_add_command(program, &command);
        //must also skip after incase there are only white spaces left before the end of the file
        skip_whitespaces(input);
    }
    
    fclose(input) ;

    return ERR_NONE; /* pour le moment... en attendant le cours 10 */
}

command_t read_command(FILE* input){
    command_t newCommand;
    char order = fgetc(input); //!= EOF
    if(order == 'R') {
        newCommand.order = READ;
        handle_read(input, &newCommand);
    } else if(order == 'W') {
        newCommand.order = WRITE;
        handle_write(input, &newCommand);
    }
    
    return newCommand;
}

void handle_read(FILE* input, command_t* command){
    skip_whitespaces(input);
    char type = fgetc(input);
    if(type == 'I') {
        command->type = INSTRUCTION;
        handle_instruction(input, command);
    } else if(type == 'D') {
        command->type = DATA;
        handle_read_data(input, command);
    }
}

void handle_write(FILE* input, command_t* command){
    skip_whitespaces(input);
    char type = fgetc(input);
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
     fgetc(input); //read the @
     fgetc(input); //read the 0
     fgetc(input);// read x 
     uint64_t addr = 0;
     fscanf(input, "%"SCNx64, &addr);
     
     init_virt_addr64(&command->vaddr, addr);
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
