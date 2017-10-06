//
// Copyright (c) 2017 Peter Dinda  All Rights Reserved
//

#ifndef _ndpc_lists
#define _ndpc_lists


// doubly linked circular list

typedef struct list_node {
    struct list_node *prev, *next;
} list_node_t;

typedef struct list {
    list_node_t *first, *last;
} list_t;

typedef enum {BEFORE, AFTER} list_insert_loc_t;

inline void list_insert(list_t *list, list_node_t *curnode, list_node_t *newnode, list_insert_loc_t where)
{
    if (!curnode || !list->first) { 
	// first node
	list->first = list->last = newnode;
	newnode->prev = newnode->next = newnode;
    } else {
	if (where==BEFORE) { 
	    if (curnode==list->first) { 
		list->first = newnode;
		// this will update curnode->next in the 
		// case where we have a single node
		list->last->next = newnode;
	    }
	    curnode->prev->next = newnode;
	    newnode->prev = curnode->prev;
	    curnode->prev = newnode;
	    newnode->next = curnode;
	} else if (where==AFTER) { 
	    if (curnode==list->last) { 
		list->last = newnode;
		// this will update curnode->prev in the 
		// case where we have a single node
		list->first->prev = newnode;
	    }
	    curnode->next->prev = newnode;
	    newnode->next = curnode->next;
	    curnode->next = newnode;
	    newnode->prev = curnode;
	}
    }
}


inline void list_insert_first(list_t *list, list_node_t *newnode)
{
    list_insert(list,list->first,newnode,BEFORE);
}

inline void list_insert_last(list_t *list, list_node_t *newnode)
{
    list_insert(list,list->last,newnode,AFTER);
}

inline int list_empty(list_t *list)
{
    return (list->first==0 && list->last==0);
}

inline void list_remove(list_t *list, list_node_t *node)
{
    if (node->next == node) {
	// only node in list
	list->first = list->last = 0;
    } else {
	if (node==list->first) { 
	    // first node in list
	    list->first = node->next;
	}
	if (node==list->last) { 
	    // last node in list
	    list->last = node->prev;
	}
	node->next->prev = node->prev;
	node->prev->next = node->next;
    }
}

#define list_entry(ptr, type, listmember) ((type *)((char *)(ptr)-(unsigned long long)(&((type *)0)->listmember)))


#define list_foreach_forward(nodeptr, listptr) for ((nodeptr)=(listptr)->first; (nodeptr); (nodeptr)= (void*) ((long long)(nodeptr)->next & (-(long long)!((nodeptr)->next==(listptr)->first))))

#define list_foreach_backward(nodeptr, listptr) for ((nodeptr)=(listptr)->last; (nodeptr); (nodeptr)=(nodeptr)->prev & (-(!((nodeptr)->prev==(listptr)->last)))) 

#define list_foreach(nodeptr,listptr) list_foreach_forward(nodeptr,listptr)


#endif
