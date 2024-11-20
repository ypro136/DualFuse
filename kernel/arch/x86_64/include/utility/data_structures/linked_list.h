 #include <types.h>

#ifndef LINKED_LIST_H
#define LINKED_LIST_H

typedef struct LLheader LLheader;

struct LLheader {
  LLheader *next;
  // ...
};

void *linked_list_allocate(void **LLfirstPtr, uint32_t structSize);
bool  linkedlist_unregister(void **LLfirstPtr, const void *LLtarget);
bool  LinkedListRemove(void **LLfirstPtr, void *LLtarget);
bool  LinkedListDuplicate(void **LLfirstPtrSource, void **LLfirstPtrTarget, uint32_t structSize);
void  linkedlist_push_front_unsafe(void **LLfirstPtr, void *LLtarget);

#endif