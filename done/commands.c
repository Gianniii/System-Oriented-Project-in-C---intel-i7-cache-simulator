#include <stdio.h>
#include "addr.h"
#include "addr_mng.h"	
#include "error.h"
#include "inttypes.h"
#include "commands.h"

int program_init(program_t* program){
	//set everything to 0
	memset(program, 0, sizeof(program->listing)*sizeof(program->listing[0]));
	program->nb_lines = 0;
	program->allocated = sizeof(program->listing);
	return ERR_NONE;
}

int program_print(FILE* output, const program_t* program){
	//TODO THIS PRINT DOESNT LINE UP ADDRESSES TO THE LEFT SO DEFINITELY WILL HAVE TO CHANGE THAT
	//for now lets just see what it prints then we can tweak it to fit the exactly how its suppose to look like as we test it
	
	//make sure output is non_null
	 if(output == NULL) {
		 return ERR_BAD_PARAMETER;
	 }
	  
	 //fprintf every entry(command) in the listing
	 size_t i;
	 for(i = 0; i < sizeof(program->listing); i++) {
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
			fprintf(output, "@00000000" "0x%" PRIX16 "0x%" PRIX16 "0x%" PRIX16 "0x%" PRIX16, 
		           command.vaddr.pgd_entry, command.vaddr.pud_entry, command.vaddr.pmd_entry,
	               command.vaddr.pte_entry);   
			return ERR_NONE;
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
			 fprintf(output, "0x%08" PRIX32, command.write_data);
		}
		
	    //NOT SURE OF VADDR FORMAT!
		fprintf(output, " @0000" "0x%09" PRIX16 "0x%09" PRIX16 "0x%09" PRIX16 "0x%09" PRIX16 "0x%12" PRIX16,
		        command.vaddr.pgd_entry, command.vaddr.pud_entry, command.vaddr.pmd_entry,
	             command.vaddr.pte_entry, command.vaddr.page_offset); 
        
        fprintf(output, "\n");
     }
     
     return ERR_NONE;	 
}

int program_shrink(program_t* program){
	M_REQUIRE_NON_NULL(program);
	return ERR_NONE;
}

int program_add_command(program_t* program, const command_t* command){
	//check parameters TODO: other checks ? 
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
	program->listing[program->nb_lines] = *command;
	++program->nb_lines;
	return ERR_NONE;
}
