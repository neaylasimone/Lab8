// mmu.c
// Main MMU simulator (allocate, deallocate, coalesce)
// Assumes util.c provides parse_file(FILE*, int[][2], int*, int*)

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "list.h"
#include "util.h"   /* parse_file is in util.c */

void TOUPPER(char * arr){
    for(int i=0;i<(int)strlen(arr);i++){
        arr[i] = toupper((unsigned char)arr[i]);
    }
}

void get_input(char *args[], int input[][2], int *n, int *size, int *policy)
{
    FILE *input_file = fopen(args[1], "r");
    if (!input_file) {
        fprintf(stderr, "Error: Invalid filepath\n");
        fflush(stdout);
        exit(0);
    }

    parse_file(input_file, input, n, size);
    fclose(input_file);

    TOUPPER(args[2]);

    if ((strcmp(args[2],"-F") == 0) || (strcmp(args[2],"-FIFO") == 0))
        *policy = 1;
    else if ((strcmp(args[2],"-B") == 0) || (strcmp(args[2],"-BESTFIT") == 0))
        *policy = 2;
    else if ((strcmp(args[2],"-W") == 0) || (strcmp(args[2],"-WORSTFIT") == 0))
        *policy = 3;
    else {
        printf("usage: ./mmu <input file> -{F | B | W }  \n(F=FIFO | B=BESTFIT | W-WORSTFIT)\n");
        exit(1);
    }
}

/* helper: add int address-sorted node into list (used by coalesce temporary sort) */
void list_add_ascending_by_address(list_t *l, block_t *blk) {
    if (!l || !blk) return;
    list_add_ascending_by_address(l, blk); /* this symbol exists in list.c; keep consistency */
}

/* allocate_memory: fully policy-aware (1=FIFO,2=BEST,3=WORST) */
void allocate_memory(list_t *freelist, list_t *alloclist, int pid, int blocksize, int policy)
{
    node_t *current = freelist->head;
    node_t *selected = NULL;
    block_t *blk;

    int best_diff = 2147483647;
    int worst_diff = -1;

    while (current != NULL) {
        blk = current->blk;
        int size = blk->end - blk->start + 1;
        if (size >= blocksize) {
            int diff = size - blocksize;
            if (policy == 1) { /* FIFO */
                selected = current;
                break;
            } else if (policy == 2) { /* BEST */
                if (diff < best_diff) {
                    best_diff = diff;
                    selected = current;
                }
            } else { /* WORST */
                if (diff > worst_diff) {
                    worst_diff = diff;
                    selected = current;
                }
            }
        }
        current = current->next;
    }

    if (selected == NULL) {
        printf("Error: Memory Allocation %d blocks\n", blocksize);
        return;
    }

    /* remove selected node from free list but preserve its block pointer */
    blk = selected->blk;
    list_remove_node(freelist, selected);

    int original_end = blk->end;

    /* allocate the front portion to PID by reusing blk */
    blk->pid = pid;
    blk->end = blk->start + blocksize - 1;

    /* insert into allocated list sorted by physical address */
    list_add_ascending_by_address(alloclist, blk);

    /* if leftover fragment exists, create it and insert according to policy */
    if (blk->end < original_end) {
        block_t *fragment = (block_t *)malloc(sizeof(block_t));
        fragment->pid = 0;
        fragment->start = blk->end + 1;
        fragment->end = original_end;

        if (policy == 1)      list_add_to_back(freelist, fragment);
        else if (policy == 2) list_add_ascending_by_blocksize(freelist, fragment);
        else                  list_add_descending_by_blocksize(freelist, fragment);
    }
}

/* deallocate_memory */
void deallocate_memory(list_t *alloclist, list_t *freelist, int pid, int policy)
{
    node_t *current = alloclist->head;
    block_t *blk;

    int found = 0;
    node_t *cur = alloclist->head;
    node_t *prev = NULL;
    while (cur) {
        if (cur->blk && cur->blk->pid == pid) {
            found = 1;
            break;
        }
        prev = cur;
        cur = cur->next;
    }

    if (!found) {
        printf("Error: Can't locate Memory Used by PID: %d\n", pid);
        return;
    }

    /* cur points to node to remove; extract block pointer then remove node */
    blk = cur->blk;
    list_remove_node(alloclist, cur);

    /* mark free */
    blk->pid = 0;

    /* insert into free list based on policy */
    if (policy == 1) list_add_to_back(freelist, blk);
    else if (policy == 2) list_add_ascending_by_blocksize(freelist, blk);
    else list_add_descending_by_blocksize(freelist, blk);
}

/* coalesce: remove nodes from list, sort by address, coalesce adjacent nodes and return new list */
list_t* coalese_memory(list_t * list)
{
    list_t *temp_list = list_alloc();
    block_t *blk;

    while ((blk = list_remove_from_front(list)) != NULL) {
        /* insert sorted by address */
        /* reuse the list_add_ascending_by_address function implemented in list.c directly */
        list_add_ascending_by_address(temp_list, blk);
    }

    /* coalesce adjacent nodes */
    list_coalese_nodes(temp_list);

    return temp_list;
}

void print_list(list_t * list, char * message)
{
    node_t *current = list->head;
    block_t *blk;
    int i = 0;

    printf("%s:\n", message);

    while(current != NULL)
    {
        blk = current->blk;
        printf("Block %d:\t START: %d\t END: %d", i, blk->start, blk->end);

        if(blk->pid != 0)
            printf("\t PID: %d\n", blk->pid);
        else
            printf("\n");

        current = current->next;
        i += 1;
    }
}

/* main (left mostly as given in assignment) */
int main(int argc, char *argv[])
{
    int PARTITION_SIZE, inputdata[200][2], N = 0, Memory_Mgt_Policy;

    list_t *FREE_LIST = list_alloc();   // list that holds all free blocks (PID is always zero)
    list_t *ALLOC_LIST = list_alloc();  // list that holds all allocated blocks
    int i;

    if(argc != 3) {
        printf("usage: ./mmu <input file> -{F | B | W }  \n(F=FIFO | B=BESTFIT | W-WORSTFIT)\n");
        exit(1);
    }

    get_input(argv, inputdata, &N, &PARTITION_SIZE, &Memory_Mgt_Policy);

    /* allocate the initial partition */
    block_t * partition = (block_t *)malloc(sizeof(block_t));
    partition->start = 0;
    partition->end = PARTITION_SIZE + partition->start - 1;
    partition->pid = 0;

    list_add_to_front(FREE_LIST, partition);

    for(i = 0; i < N; i++) {
        printf("************************\n");
        if(inputdata[i][0] != -99999 && inputdata[i][0] > 0) {
            printf("ALLOCATE: %d FROM PID: %d\n", inputdata[i][1], inputdata[i][0]);
            allocate_memory(FREE_LIST, ALLOC_LIST, inputdata[i][0], inputdata[i][1], Memory_Mgt_Policy);
        }
        else if (inputdata[i][0] != -99999 && inputdata[i][0] < 0) {
            printf("DEALLOCATE MEM: PID %d\n", abs(inputdata[i][0]));
            deallocate_memory(ALLOC_LIST, FREE_LIST, abs(inputdata[i][0]), Memory_Mgt_Policy);
        }
        else {
            printf("COALESCE/COMPACT\n");
            FREE_LIST = coalese_memory(FREE_LIST);
        }

        printf("************************\n");
        print_list(FREE_LIST, "Free Memory");
        print_list(ALLOC_LIST,"\nAllocated Memory");
        printf("\n\n");
    }

    list_free(FREE_LIST);
    list_free(ALLOC_LIST);

    return 0;
}
