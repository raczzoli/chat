#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "list.h"

struct list_node *list_add_node(struct list_node **head, void *data)
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

int list_remove_node(struct list_node **head, struct list_node *node)
{
	struct list_node *h;
	
	h = *head;

	if (!h || !node)
		return -ENOENT;

	if (node->next == node && node->prev == node) {
		/*
		 * if node next and prev points to itself it means
		 * the node is also the head, and after removing it
		 * head will be NULL
		 */
		*head = NULL;
	}
	else {
		/*
		 * if the node is also the head (it is the first element), 
		 * after removal the head will be the next element
		 */
		if (node == h)
			*head = node->next;

		node->prev->next = node->next;
		node->next->prev = node->prev;
	}

	free(node);
	return 0;
}

struct list_node *list_get_data_node(struct list_node **head, void *data)
{
	struct list_node *h, *curr;
	
	h = curr = *head;

	if (!h)
		return NULL;

	do {
		if (curr->data == data) 
			return curr;

		curr = curr->next;
	}
	while (curr != h);

	return NULL;
}
