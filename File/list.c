#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include "list.h"

void list_init(struct node **phead)
{
	assert(phead != NULL);
	*phead = (struct node *)malloc(sizeof(struct node));
	assert(*phead != NULL);
	(*phead)->next = NULL;
}

void list_add(struct node *head,int c)
{
	assert(head != NULL);
	struct node *p = (struct node *)malloc(sizeof(struct node));
	assert(p != NULL);

	p->c = c;
	p->next = NULL;

	while(head->next != NULL)
	{
		head = head->next;
	}
	head->next = p;
}

int list_get_c(struct node *head)
{
	assert(head != NULL);
	int fd = -1;
	struct node *p = head->next;

	if(p == NULL)
	{
		return -1;
	}
	
	head->next = p->next;
	fd = p->c;
	free(p);

	return fd;
}
void list_destroy(struct node *head)
{
	assert(head != NULL);

	struct node *p = head->next;
	struct node *q;

	while(p != NULL)
	{
		q = p->next;
		p->next = q->next;
		free(q);
	}
}

