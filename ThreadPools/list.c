#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "list.h"
#include "memwatch.h"

list_head *list_init(list_head *head)
{
	head->prev=head;
	head->next=head;
	
	return head;
}

int list_empty(const list_head *head)
{
	return head->next == head;
}


void list_add(list_head *new,list_head *head)
{
	new->prev=NULL;
	new->next=NULL;

	if(list_empty(head)){//empty
		new->prev=new;
		new->next=new;
		head->next=new;
		head->prev=new;
	}else{
		new->next=head->next;
		head->next->prev=new;

		new->prev=head->prev;
		head->prev->next=new;

		head->prev=new;
	}
	

	return;
}

void list_add_tail(struct list_head *new, struct list_head *head)
{
	new->prev=NULL;
        new->next=NULL;
        if(list_empty(head)){//empty
                new->prev=new;
                new->next=new;
                head->next=new;
                head->prev=new;
        }else{
                head->next->next=new;
		new->prev=head->next;
                head->next=new;
        }

        return;
}

void list_del(list_head *entry,struct list_head *head)
{
	list_head *prev;
	list_head *next;

	if(list_empty(head)) return;

	if(head->next==entry&&head->prev==entry){//one node
		head->next=head;
		head->prev=head;
		return;
	}
	if(head->next==entry) head->next=entry->next;
	if(head->prev==entry) head->prev=entry->prev;


	prev=entry->prev;
	next=entry->next;

	prev->next=next;
	next->prev=prev;		

	return;	
}

