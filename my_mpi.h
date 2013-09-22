#ifndef __MY_MPI_H__
#define __MY_MPI_H__

#ifndef MPI_Comm_World
#define MPI_Comm_World 0
#endif

#ifndef MPI_COMM_WORLD
#define MPI_COMM_WORLD 0
#endif


#ifndef MPI_SUCCESS
#define MPI_SUCCESS 0
#endif

#ifndef MPI_ERR_COMM
#define MPI_ERR_COMM 1
#endif

#ifndef MPI_ANY_TAG
#define MPI_ANY_TAG 0
#endif

#ifndef MPI_STATUS_IGNORE
#define MPI_STATUS_IGNORE 0
#endif

#ifndef MPI_CHAR
#define MPI_CHAR 0
#endif

#define MPI_ACK 1
#define MPI_NACK 2

#define ALL_GOOD 42
#define SHIT_ITS_NOT_ALL_GOOD 37

typedef int MPI_Comm;

typedef enum {
    VALID,
    INVALID
} STATE;

typedef struct
{
    int nodes;
    int rootPort;
    char * rootHost;
} envData;

typedef struct
{
    int rank;
    char * hostname;
    int port;
    int fp;
} node;

typedef struct
{
    int node;
    int port;
    char * hostname;
    int socketFd;
} data;

typedef struct
{
    int type;
    char hostname[256];
    int node, port;
} mpiInitMsg;

void MPI_Init(int*, char***);
int MPI_Barrier(MPI_Comm);
int MPI_Finalize(void);
int MPI_Comm_rank(MPI_Comm, int*);
int MPI_Comm_size(MPI_Comm, int*);
void MPI_Send(void*, int, int, int, int, int);
void MPI_Recv(void*, int, int, int, int, int, int);

#endif /* __MY_MPI_H__ */
