#ifndef LIST_H__
#define LIST_H__

typedef struct list_head{
        struct list_head *prev;
        struct list_head *next;
}list_head;


extern list_head *list_init(list_head *head);
extern int list_empty(const list_head *head);
extern void list_add(list_head *new,list_head *head);
extern void list_add_tail(struct list_head *new, struct list_head *head);
extern void list_del(list_head *entry,struct list_head *head);

#endif
