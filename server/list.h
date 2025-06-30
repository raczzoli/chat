#ifndef LIST_H
#define LIST_H

#include "list.h"

struct list_node {
	struct list_node *next;
	struct list_node *prev;
	void *data;
};

struct list_node *list_add_node(struct list_node **head, void *data);
int list_remove_node(struct list_node **head, struct list_node *node);
struct list_node *list_get_data_node(struct list_node **head, void *data);

#endif //LIST_H