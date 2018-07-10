#include <stdio.h>

struct node
{
	int c;
	struct node* next;
};

void list_init(struct node **phead);

void list_add(struct node *head,int c);

int list_get_c(struct node *head);

void list_destroy(struct node *head);




