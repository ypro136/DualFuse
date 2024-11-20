#include <data_structures/linked_list.h>
#include <liballoc.h>


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

 #include <types.h>

// Linked Lists (singly, non-circular); basically just allocated structs
// pointing at each other!

/*
 * Requirement for this DS utility is that the next entry of the linked list is
 * prepended! Please consider this during usage, otherwise results WILL be
 * fatal!
 *
 * +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 * |  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |
 * | <-    void* next   -> | <-      rest       -> |
 * +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 */

void *linked_list_allocate(void **LLfirstPtr, uint32_t structSize) {
  LLheader *target = (LLheader *)malloc(structSize);
  memset(target, 0, structSize);

  LLheader *curr = (LLheader *)(*LLfirstPtr);
  while (1) {
    if (curr == 0) {
      // means this is our first one
      *LLfirstPtr = target;
      break;
    }
    if (curr->next == 0) {
      // next is non-existent (end of linked list)
      curr->next = target;
      break;
    }
    curr = curr->next; // cycle
  }

  target->next = 0; // null ptr
  return target;
}

bool linkedlist_unregister(void **LLfirstPtr, const void *LLtarget) {
  LLheader *LLfirstCopy = *LLfirstPtr;

  LLheader *curr = (LLheader *)(*LLfirstPtr);
  while (curr) {
    if (curr->next && curr->next == LLtarget)
      break;
    curr = curr->next;
  }

  if (LLfirstCopy == LLtarget) {
    // target is the first one
    *LLfirstPtr = LLfirstCopy->next;
    return true;
  } else if (!curr)
    return false;

  LLheader *target = curr->next;
  curr->next = target->next; // remove reference

  return true;
}

bool LinkedListRemove(void **LLfirstPtr, void *LLtarget) {
  bool res = linkedlist_unregister(LLfirstPtr, LLtarget);
  free(LLtarget);
  return res;
}

bool LinkedListDuplicate(void **LLfirstPtrSource, void **LLfirstPtrTarget,
                         uint32_t structSize) {
  LLheader *browse = (LLheader *)(LLfirstPtrSource);
  while (browse) {
    LLheader *new_linkedlist_header = linked_list_allocate(LLfirstPtrTarget, structSize);
    memcpy((void *)((size_t) new_linkedlist_header + sizeof(new_linkedlist_header->next)),
           (void *)((size_t)browse + sizeof(browse->next)),
           structSize - sizeof(browse->next));
    browse = browse->next;
  }

  return true;
}

void linkedlist_push_front_unsafe(void **LLfirstPtr, void *LLtarget) {
  if (*LLfirstPtr == 0) {
    *LLfirstPtr = LLtarget;
    return;
  }

  void *next = *LLfirstPtr;
  *LLfirstPtr = LLtarget;
  LLheader *target = (LLheader *)(LLtarget);
  target->next = next;
}