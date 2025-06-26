#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "list.h"

struct list_node *list_add(struct list_node **head, void *data)
{
	struct list_node *h = *head;
	struct list_node *node = malloc(sizeof(struct list_node));

	if (!node) 
		return NULL;

	node->prev = NULL;
	node->next = NULL;
	node->data = data;

	if (!h) {
		node->next = node->prev = node;
		*head = node;
	}
	else {
		struct list_node *tail = h->prev;

		node->next = h;
		node->prev = tail;

		tail->next = node;
		h->prev = node;
	} 

	return node;
}

/*
int list_remove(struct list_node **head, void *data)
{

}
*/