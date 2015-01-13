#ifndef __LIST_H__
#define __LIST_H__
#include "config.h"

typedef struct client_node {
        char client_id[ADDR_LEN];
	int pid;
	struct client_node *next;
	struct client_node *prev;
} client_node_t;

typedef struct client_list {
	client_node_t* head; 
	client_node_t* tail; 
	int size;
} client_list_t;

void init_client_list(client_list_t *list);
void add_to_client_list(client_list_t *list, char *client, int pid);
void print_client_list(client_list_t *list);
void free_client_list(client_list_t *list);
void remove_from_client_list(client_list_t* list, char *client);
int find_in_client_list(client_list_t* list, char *client);

#endif
