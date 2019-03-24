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
		if(command.order == Read) {
			fprintf(output, "R");
		} else {
			fprintf(output, "L");
		}
		
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
		
		if(command.data_size == 8){
			fprintf(output, "B"); 
		} else {
		    fprintf(output, "W"); //DOES WORD INCLUDE ALL OTHER SIZES OR NEEDA BE POWER OF 2 ??
		    fprintf(output, "0x%08" PRIX32, command.write_data);
		}
		
	    //NOT SURE OF VADDR FORMAT!
		fprintf(output, " @00000000" "0x%" PRIX16 "0x%" PRIX16 "0x%" PRIX16 "0x%" PRIX16, 
		        command.vaddr.pgd_entry, command.vaddr.pud_entry, command.vaddr.pmd_entry,
	             command.vaddr.pte_entry); 
        
        fprintf(output, "\n");
     }
     
     return ERR_NONE;	 
}

int program_shrink(program_t* program){
	//TODO anything else need to check??
	if(programming == NULL) ERR_BAD_PARAMETER;
	
	return ERR_NONE;
}
