#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include "util.h"
#include "list.h"

void parse_file(FILE *f, int input[][2], int *n, int *PARTITION_SIZE)
{
    // Read partition size
    if (fscanf(f, "%d", PARTITION_SIZE) != 1) {
        fprintf(stderr, "Error: Unable to read PARTITION_SIZE\n");
        exit(1);
    }

    // Read commands (PID, size)
    while (1) {
        int pid, size;

        int r = fscanf(f, "%d %d", &pid, &size);
        if (r != 2) break;

        input[*n][0] = pid;
        input[*n][1] = size;

        (*n)++;
    }
}
