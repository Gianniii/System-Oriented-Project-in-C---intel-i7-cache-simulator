#include <stdio.h>
#include <stdlib.h>
#include "list.h"
#include "util.h" // for SIZE_T_FMT
#include "error.h"



int is_empty_list(const list_t* this){
	M_REQUIRE_NON_NULL(this);
	if(this->front == NULL) return 1;
	if(this->back == NULL) return 1;
	return 0;
}

void init_list(list_t* this){
	this->front = NULL;
	this->back = NULL;
}

/**
 * @brief clear the whole list (make it empty)
 * @param this list to clear
 */
void clear_list(list_t* this) {
	free(this->back);
	free(this->front);
	init_list(this);
}


node_t* push_back(list_t* this, const list_content_t* value) {
	//check arguments are valid
	if(this == NULL || value == NULL) return NULL;
	//allocated memory for new node
	node_t* newNode;
	newNode = calloc(1, sizeof(node_t));
	if(newNode == NULL) {
		return NULL;
	}
	newNode->value = *value;
	newNode->previous = this->back;
	newNode->next = NULL;
	if(is_empty_list(this)){
		this->front = newNode;
	} else {
		//if list was not null can rewire old_last elem so its next points to our new node
		this->back->next = newNode;
	}
	this->back = newNode;
	return newNode;
}

void move_back(list_t* this, node_t* n) {
	//printf("goeshere\n");
	
	if(!is_empty_list(this)){
		//if is not the first element of the list need to remove the node and then place it at the back
		if(n->previous != NULL) {
			n->previous->next = n->next; 
			n->next->previous = n->previous;
			push_back(this, &n->value);
			free(n);
		} else if(n->next != NULL) { //if n is the first element but not the only element
			this->front = n->next;
			n->next->previous = NULL;
			push_back(this, &n->value);
		}
		
	}
}

/**
 * @brief add a new value at the begining of the list
 * @param this list where to add to
 * @param value value to be added
 * @return a pointer to the newly inserted element or NULL in case of error
 */
node_t* push_front(list_t* this, const list_content_t* value){
	//check arguments are valid
	if(this == NULL || value == NULL) return NULL;
	
	//allocated memory for new node
	node_t* newNode;
	newNode = calloc(1, sizeof(node_t));
	if(newNode == NULL) {
		return NULL;
	}
	newNode->value = *value;
	newNode->previous = NULL;
	newNode->next = this->front;
	if(is_empty_list(this)) {
		this->back = newNode;
	} else {
		this->front->previous = newNode;
	}
	
	this->front = newNode;
	
	return newNode;
}

/**
 * @brief remove the last value
 * @param this list to remove from
 */
void pop_back(list_t* this){
	if(!is_empty_list(this)) {
		this->back->previous->next = NULL;
		node_t* newLast = this->back->previous;
		free(this->back);
		this->back = newLast;
	}
}

/**
 * @brief remove the first value
 * @param this list to remove from
 */
void pop_front(list_t* this) {
	if(!is_empty_list(this)) {
		this->front->next->previous = NULL;
		node_t* newFirst = this->front->next;
		free(this->front);
		this->front = newFirst;
	}
}

/**
 * @brief print a list (on one single line, no newline)
 * @param stream where to print to
 * @param this the list to be printed
 * @return number of printed characters
 */
int print_list(FILE* stream, const list_t* this) {
	int number_printed_characters = 0;
	int first_interation = 0;
	fprintf(stream, "(");
	for_all_nodes(n, this) {
		if(first_interation == 1) {
			fprintf(stream, ", ");
		} 
		//printf("%"PRIX32"\n", n->value); value is way too bit should be between 0 and 127 cuz it represents index ...
		number_printed_characters += print_node(stream, n->value);
		first_interation = 1;
	}
	fprintf(stream, ")");
	return number_printed_characters;
}

/**
 * @brief print a list reversed way
 * @param stream where to print to
 * @param this the list to be printed
 * @return number of printed characters
 */
int print_reverse_list(FILE* stream, const list_t* this) {
	int number_printed_characters = 0;
	int first_iteration = 0;
	fprintf(stream, "(");
	for_all_nodes_reverse(n, this) {
		if(first_iteration == 1) {
			fprintf(stream, ", ");
		}
		number_printed_characters += print_node(stream, n->value);
		first_iteration = 1;
	}
	fprintf(stream, ")");
	return number_printed_characters;
}



