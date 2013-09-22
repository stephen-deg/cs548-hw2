//#include <mpi.h>
#include "my_mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>

#define DIRECTORY "/home/temp523/homework/2/part1/"
#define ROOT 0
#define MAX_ITER 20
#define NUM_MESSAGES 9

// Used to return the min/max elements in the array
typedef struct {
    double min;
    double max;
    double avg;
} minmaxavg;

// Foreward declarations
void experiment_a(int, int);
void findMinMaxAvg(double*, int, minmaxavg*);

int main(int argc, char** argv) {
    // Initialize the world
    MPI_Init(&argc, &argv);
    printf("%s back from init.\n", argv[argc+3]);
    fflush(stdout);
    // Find out what rank this process is
    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    // Find out the total number of processes
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    // Wait for all processes to print their rank before continuing
    printf("%d calling barrier.\n", world_rank);
    fflush(stdout);
    MPI_Barrier(MPI_COMM_WORLD);
    printf("%d barriered.\n", world_rank);
    fflush(stdout);

    // Do experiment A
    experiment_a(world_size, world_rank);

    // Sync up and exit cleanly
    printf("%d calling finalize.\n", world_rank);
    fflush(stdout);
    MPI_Finalize();
    printf("%d finalized.\n", world_rank);
    fflush(stdout);

    return 0;
}

/* Experiment A is to have a single node (ROOT) send and receive messages to
all other nodes, one at a time, recording the round trip time for each. To
achieve the best accuracy the first time will be ignored, and the remaining
times will be averaged. */
void experiment_a(int world_size, int world_rank) {
    char* data[NUM_MESSAGES];  // Data to send back and forth
    int sizes[NUM_MESSAGES];   // Size of the data messages
    int size = 64;  // Starting message size is 4 bytes

    // Allocate all of the message buffers
    for(int i = 0; i < NUM_MESSAGES; i++) {
        sizes[i] = size;
        data[i] = malloc(sizeof(char) * size);
        size *= 2;  // double the amount of data
    }


    // If we are the root node then we will be sending the messages to the other
    // processes and recording the round trip time.
    if(world_rank == ROOT) {
        // File to write output to for easy gnuplot reading
        char* filename = DIRECTORY"rtt.experiment.a.out";
        FILE *fp = fopen(filename, "w");
        if(fp == NULL) {
            fprintf(stderr, "Could not open %s for writing\n", filename);
            exit(1);
        }

        // Go through each message size 64B -> 16KB
        for(int k = 0; k < NUM_MESSAGES; k++) {
            MPI_Barrier(MPI_COMM_WORLD);
            // Go through each node for the same message size before sending
            // the next larger message
            printf("%d", sizes[k]);
            fprintf(fp, "%d", sizes[k]);
            for(int i = 1; i < world_size; i++) {
                // used to store the RTTs
                double rtts[MAX_ITER-2*k];
                int nIters = MAX_ITER-2*k;

                for(int j = 0; j < nIters; j++) {
                    // Send a message to process rank i
                    // Record starting time and send it
                    struct timeval start, end;
                    gettimeofday(&start, NULL);
                    MPI_Send(data[k], sizes[k], MPI_CHAR, i, 0, MPI_COMM_WORLD);

                    // Wait for the response and record receiving time
                    MPI_Recv(data[k], sizes[k], MPI_CHAR, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    gettimeofday(&end, NULL);

                    // Round trip time = end time - start time
                    rtts[j] = ((double)(end.tv_sec - start.tv_sec) -
                               (double)(end.tv_usec - start.tv_usec)/1e6);
                }

                // Find the min/max/avg time for this message size for this node
                minmaxavg mma;
                mma.min = LONG_MAX;
                mma.max = LONG_MIN;
                mma.avg = 0;
                // +1 to skip the first value
                findMinMaxAvg(rtts+1, nIters-1, &mma);
                printf(" %e %e %e", mma.avg, mma.min, mma.max);
                fprintf(fp, " %e %e %e", mma.avg, mma.min, mma.max);
            }
            printf("\n");
            fprintf(fp, "\n");
        }
        fclose(fp);
    }
    // Otherwise we're a peon node and are waiting for a message to respond to.
    else {
        // Loop over every message size
        for(int i = 0; i < NUM_MESSAGES; i++) {
            MPI_Barrier(MPI_COMM_WORLD);
            // For every message size we will receive N messages
            int nIters = MAX_ITER - 2*i;
            for(int j = 0; j < nIters; j++) {
                // Accept incoming message
                MPI_Recv(data[i], sizes[i], MPI_CHAR, ROOT, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                // Respond to the message
                MPI_Send(data[i], sizes[i], MPI_CHAR, ROOT, 0, MPI_COMM_WORLD);
            }
        }
    }

    // No memory leaks
    for(int i = 0; i < NUM_MESSAGES; i++) {
        free(data[i]);
    }
}

/* Given an array of doubles, find the smallest element, largest element, and
the average */
void findMinMaxAvg(double* arr, int length, minmaxavg* vals) {
    double total = 0;
    for(int i = 0; i < length; i++) {
        total += arr[i];

        // check for new min
        if(arr[i] < vals->min)
            vals->min = arr[i];

        // check for new max
        if(arr[i] > vals->max)
            vals->max = arr[i];
    }

    // find the average of the array
    vals->avg = total / (double)length;
}
