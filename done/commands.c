#include <stdio.h>
#include "addr.h"
#include "addr_mng.h"	
#include "error.h"
#include "inttypes.h"
#include "commands.h"

int program_init(program_t* program){
	memset(program, 0, sizeof(program->listing)*sizeof(program->listing[0]); //set to 0 
	program->nb_lines = 0;
	program->allocated = sizeof(program->listing);
}

int program_print(FILE* output, const program_t* program){
	 if(output == NULL) {
		 fprintf(stderr,
            "Erreur: le fichier %s ne peut etre ouvert en écriture !\n",
            nom_fichier);
		 return ERR_BAD_PARAMETER;
	 }
	 
	 size_t i;
	 for(i = 0; i < sizeof(program->listing); i++) {
		command_t command = programing->listing[i];
		if(command.order == Read) {
			fprintf(output, "R ");
		} else {
			fprintf(output, "L ");
		}
		
		if(command.type == INSTRUCTION) {
			fprintf(output, "I ");
		} else {
			fprintf(output, "D ");
		}
		
		
		//still needa print data if there is 
		
		print_virtual_address(output, *program.addr); //correct?
		
        
        
        fprintf(output, "\n");
        
        //what return ? 	 
}
