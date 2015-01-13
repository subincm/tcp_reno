#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client_list.h"

inline void init_client_list(client_list_t *list)
{
  memset(list, 0, sizeof(client_list_t));
}

void add_to_client_list(client_list_t *list, char *client, int pid)
{
  client_node_t* entry = (client_node_t *)calloc(1, sizeof(client_node_t));
  /* Add the entry */
  strncpy(entry->client_id,client, ADDR_LEN);
  entry->pid = pid;

  /* Adjust the List */
  entry->prev = list->tail;

  if (list->tail) {
    list->tail->next = entry;
  }
  if (0 == list->size) {
    list->head = entry;
  }

  list->tail = entry;
  list->size += 1;
}

void delete_from_client_list(client_list_t *list, int pid)
{
  client_node_t* entry = list->head;

  if (!entry)
    return;

  /* Special Case where the entry is the first in the list */
  if(entry->pid == pid) 
  {
    list->head = entry->next;

    if (list->head)
      list->head->prev = NULL;

    list->size -= 1;

    if (0 == list->size)
      list->tail = NULL;

    return;
  }

  entry = entry->next;
  while (entry) {
    if(entry->pid == pid) 
    {
      entry->prev->next = entry->next;
      if (entry->next) {
        entry->next->prev = entry->prev;
      }
      else {
        list->tail = entry->prev;
      }
      list->size -= 1;

      free(entry);
      break;
    }
    entry = entry->next;
  }
}

void print_client_list(client_list_t* list)
{
  client_node_t* entry = list->head;
  printf("Size: %d\n", list->size);
  printf("Entries:\n");
  while(entry)
  {
    printf("[%s , %d]\n", entry->client_id, entry->pid);
    entry = entry->next;
  }
}

void free_client_list_t(client_list_t* list)
{
  client_node_t* entry = list->head;
  client_node_t* curr;

  while (entry)
  {
    curr = entry;
    entry = entry->next;
    free(curr);
  }
}

int find_in_client_list(client_list_t* list, char *client)
{
  client_node_t* entry = list->head;
  while(entry)
  {
    if(strcmp(entry->client_id, client) == 0)
        return entry->pid;
    entry = entry->next;
  }
  return -1;
}
