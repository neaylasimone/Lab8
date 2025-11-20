// list.c
// Clean, corrected linked-list implementation for MMU project

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"

/* node and list definitions */
struct node {
    block_t *blk;
    struct node *next;
};

struct list {
    node_t *head;
};

/* Allocate a new empty list */
list_t *list_alloc() {
    list_t *l = (list_t *)malloc(sizeof(list_t));
    if (!l) return NULL;
    l->head = NULL;
    return l;
}

/* Free the list structure only (caller's responsibility to free blocks if needed) */
void list_free(list_t *l) {
    if (!l) return;
    /* free nodes and blocks to avoid leaks */
    node_t *cur = l->head;
    while (cur) {
        node_t *tmp = cur;
        cur = cur->next;
        if (tmp->blk) free(tmp->blk);
        free(tmp);
    }
    free(l);
}

/* Create a node that wraps block pointer (node owns neither block nor frees it) */
node_t *node_alloc(block_t *blk) {
    node_t *n = (node_t *)malloc(sizeof(node_t));
    if (!n) return NULL;
    n->blk = blk;
    n->next = NULL;
    return n;
}

/* Free node only (does NOT free node->blk) */
void node_free(node_t *node) {
    if (!node) return;
    free(node);
}

/* Print list for debugging */
void list_print(list_t *l) {
    if (!l) {
        printf("list is NULL\n");
        return;
    }
    node_t *cur = l->head;
    int i = 0;
    if (!cur) {
        printf("list is empty\n");
        return;
    }
    while (cur) {
        block_t *b = cur->blk;
        printf("Block %d:\t START: %d\t END: %d", i, b->start, b->end);
        if (b->pid != 0) printf("\t PID: %d", b->pid);
        printf("\n");
        cur = cur->next;
        i++;
    }
}

/* List length */
int list_length(list_t *l) {
    if (!l) return 0;
    int c = 0;
    node_t *cur = l->head;
    while (cur) { c++; cur = cur->next; }
    return c;
}

/* Add to back */
void list_add_to_back(list_t *l, block_t *blk) {
    if (!l || !blk) return;
    node_t *n = node_alloc(blk);
    if (!l->head) { l->head = n; return; }
    node_t *cur = l->head;
    while (cur->next) cur = cur->next;
    cur->next = n;
}

/* Add to front */
void list_add_to_front(list_t *l, block_t *blk) {
    if (!l || !blk) return;
    node_t *n = node_alloc(blk);
    n->next = l->head;
    l->head = n;
}

/* Add at index (0 = front). If index >= length, append to back. */
void list_add_at_index(list_t *l, block_t *blk, int index) {
    if (!l || !blk) return;
    if (index <= 0 || l->head == NULL) {
        list_add_to_front(l, blk);
        return;
    }
    int i = 0;
    node_t *cur = l->head;
    while (cur->next && i < index-1) { cur = cur->next; i++; }
    node_t *n = node_alloc(blk);
    n->next = cur->next;
    cur->next = n;
}

/* Insert allocated blocks in ascending physical address order */
void list_add_ascending_by_address(list_t *l, block_t *blk) {
    if (!l || !blk) return;
    node_t *cur = l->head;
    node_t *prev = NULL;
    while (cur && blk->start > cur->blk->start) {
        prev = cur;
        cur = cur->next;
    }
    node_t *n = node_alloc(blk);
    if (!prev) {
        n->next = l->head;
        l->head = n;
    } else {
        n->next = prev->next;
        prev->next = n;
    }
}

/* Best-fit insertion: ascending block size */
void list_add_ascending_by_blocksize(list_t *l, block_t *newblk) {
    if (!l || !newblk) return;
    int newsz = newblk->end - newblk->start + 1;
    node_t *cur = l->head;
    node_t *prev = NULL;
    while (cur) {
        int csz = cur->blk->end - cur->blk->start + 1;
        if (newsz < csz) break;
        prev = cur;
        cur = cur->next;
    }
    node_t *n = node_alloc(newblk);
    if (!prev) {
        n->next = l->head;
        l->head = n;
    } else {
        n->next = prev->next;
        prev->next = n;
    }
}

/* Worst-fit insertion: descending block size */
void list_add_descending_by_blocksize(list_t *l, block_t *blk) {
    if (!l || !blk) return;
    int newsz = blk->end - blk->start + 1;
    node_t *cur = l->head;
    node_t *prev = NULL;
    while (cur) {
        int csz = cur->blk->end - cur->blk->start + 1;
        if (newsz > csz) break;
        prev = cur;
        cur = cur->next;
    }
    node_t *n = node_alloc(blk);
    if (!prev) {
        n->next = l->head;
        l->head = n;
    } else {
        n->next = prev->next;
        prev->next = n;
    }
}

/* Remove from back */
block_t* list_remove_from_back(list_t *l) {
    if (!l || !l->head) return NULL;
    node_t *cur = l->head;
    if (!cur->next) {
        block_t *b = cur->blk;
        l->head = NULL;
        free(cur);
        return b;
    }
    while (cur->next->next) cur = cur->next;
    block_t *b = cur->next->blk;
    free(cur->next);
    cur->next = NULL;
    return b;
}

/* Get from front (without removing) */
block_t* list_get_from_front(list_t *l) {
    if (!l || !l->head) return NULL;
    return l->head->blk;
}

/* Remove from front */
block_t* list_remove_from_front(list_t *l) {
    if (!l || !l->head) return NULL;
    node_t *n = l->head;
    block_t *b = n->blk;
    l->head = n->next;
    free(n);
    return b;
}

/* Remove at index */
block_t* list_remove_at_index(list_t *l, int index) {
    if (!l || !l->head) return NULL;
    if (index <= 0) return list_remove_from_front(l);
    node_t *cur = l->head;
    int i = 0;
    while (cur->next && i < index-1) { cur = cur->next; i++; }
    if (!cur->next) return NULL;
    node_t *target = cur->next;
    block_t *b = target->blk;
    cur->next = target->next;
    free(target);
    return b;
}

/* Compare blocks for equality */
bool compareBlks(block_t* a, block_t *b) {
    if (!a || !b) return false;
    return (a->pid == b->pid && a->start == b->start && a->end == b->end);
}

/* Helper: size >= check */
bool compareSize(int a, block_t *b) {
    if (!b) return false;
    return (a <= (b->end - b->start + 1));
}

/* Helper: pid match */
bool comparePid(int a, block_t *b) {
    if (!b) return false;
    return (a == b->pid);
}

/* list_is_in (exact block) */
bool list_is_in(list_t *l, block_t* value) {
    if (!l) return false;
    node_t *cur = l->head;
    while (cur) {
        if (compareBlks(value, cur->blk)) return true;
        cur = cur->next;
    }
    return false;
}

/* get element at index (0-based) */
block_t* list_get_elem_at(list_t *l, int index) {
    if (!l) return NULL;
    node_t *cur = l->head;
    int i = 0;
    while (cur) {
        if (i == index) return cur->blk;
        cur = cur->next; i++;
    }
    return NULL;
}

/* get index of block (exact match) */
int list_get_index_of(list_t *l, block_t* value) {
    if (!l) return -1;
    node_t *cur = l->head;
    int i = 0;
    while (cur) {
        if (compareBlks(value, cur->blk)) return i;
        cur = cur->next; i++;
    }
    return -1;
}

/* list_is_in_by_size */
bool list_is_in_by_size(list_t *l, int Size) {
    if (!l) return false;
    node_t *cur = l->head;
    while (cur) {
        if (compareSize(Size, cur->blk)) return true;
        cur = cur->next;
    }
    return false;
}

/* list_is_in_by_pid */
bool list_is_in_by_pid(list_t *l, int pid) {
    if (!l) return false;
    node_t *cur = l->head;
    while (cur) {
        if (comparePid(pid, cur->blk)) return true;
        cur = cur->next;
    }
    return false;
}

/* Remove a specific node from list (node must belong to list).
   This frees the node structure but NOT the block pointer (so caller can reuse it). */
void list_remove_node(list_t *l, node_t *node) {
    if (!l || !node) return;
    if (l->head == node) {
        l->head = node->next;
        node_free(node);
        return;
    }
    node_t *cur = l->head;
    node_t *prev = NULL;
    while (cur && cur != node) {
        prev = cur;
        cur = cur->next;
    }
    if (!cur) return;
    prev->next = cur->next;
    node_free(cur);
}

/* Get index of first block >= Size */
int list_get_index_of_by_Size(list_t *l, int Size) {
    if (!l) return -1;
    node_t *cur = l->head;
    int i = 0;
    while (cur) {
        if (compareSize(Size, cur->blk)) return i;
        cur = cur->next; i++;
    }
    return -1;
}

/* Get index of block with pid */
int list_get_index_of_by_Pid(list_t *l, int pid) {
    if (!l) return -1;
    node_t *cur = l->head;
    int i = 0;
    while (cur) {
        if (comparePid(pid, cur->blk)) return i;
        cur = cur->next; i++;
    }
    return -1;
}

/* Coalesce adjacent blocks (assumes list sorted by address). Merges node A and B when A.end + 1 == B.start.
   This function modifies the list in place. */
void list_coalese_nodes(list_t *l) {
    if (!l || !l->head) return;
    node_t *cur = l->head;
    while (cur && cur->next) {
        node_t *nxt = cur->next;
        if (cur->blk->end + 1 == nxt->blk->start && cur->blk->pid == 0 && nxt->blk->pid == 0) {
            /* merge blocks: extend cur.end, free nxt node and its block */
            cur->blk->end = nxt->blk->end;
            free(nxt->blk);
            cur->next = nxt->next;
            free(nxt);
            continue; /* try to merge again with new next */
        }
        cur = cur->next;
    }
}
