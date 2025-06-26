#ifndef LIST_H
#define LIST_H

#include "list.h"

struct list_node {
	struct list_node *next;
	struct list_node *prev;
	void *data;
};

struct list_node *list_add(struct list_node **head, void *data);
int list_remove(struct list_node **head, void *data);

#endif //LIST_H