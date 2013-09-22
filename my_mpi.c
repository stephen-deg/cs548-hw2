#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "my_mpi.h"
#include "sockettome.h"

/* Global variables */
static STATE my_state;
static node *nodeTable;
static envData worldData;
static data myData;

#define DEBUG 0

/* Author: Stephen Ranshous */
int MPI_Barrier(MPI_Comm comm)
{
    if (my_state == INVALID)
    {
        fprintf(stderr,"after finalize MPI calls are not allowed");
        exit(1);
    }

    /* If I am ROOT I am going to do a receive from each of the processes
     * to ensure everyone has hit their barrier. Once I receive everyones
     * message I will do a broadcast to tell everyone to resume. */
    int ack = 0;
    if (myData.node == 0)
    {
        int i, allGood = ALL_GOOD;
        for (i = 1; i < worldData.nodes; i++)
        {
        	#if DEBUG 
            printf("listening for node %d\n",i);
            fflush(stdout);
            #endif

            MPI_Recv(&ack, 4, MPI_CHAR, i, MPI_ANY_TAG, MPI_Comm_World, MPI_STATUS_IGNORE);

            #if DEBUG
            printf("got back from node %d\n",i);
            fflush(stdout);
            #endif

            if (ack != MPI_ACK)
            {
                allGood = SHIT_ITS_NOT_ALL_GOOD;
            }
        }

        ack = allGood == ALL_GOOD ? MPI_ACK : MPI_NACK;

        for (i = 1; i < worldData.nodes; i++)
        {
        	#if DEBUG 
            printf("sending to node %d\n",i);
            fflush(stdout);
            #endif

            MPI_Send(&ack, 4, MPI_CHAR, i, MPI_ANY_TAG, MPI_Comm_World);

            #if DEBUG 
            printf("sent to node %d\n",i);
            fflush(stdout);
            #endif
        }
    }
    /* If I am NOT ROOT I am going to do a send to the ROOT to let it know
     * that I have hit my barrier. I will then block til I receive a message
     * back saying that I can continue */
    else
    {
        ack = MPI_ACK;
        #if DEBUG 
        printf("node %d sending to root\n",myData.node);
        fflush(stdout);
        #endif

        MPI_Send(&ack, 4, MPI_CHAR, 0, MPI_ANY_TAG, MPI_Comm_World);

        #if DEBUG 
        printf("node %d waiting for root to release it\n",myData.node);
        fflush(stdout);
        #endif

        MPI_Recv(&ack, 4, MPI_CHAR, 0, MPI_ANY_TAG, MPI_Comm_World, MPI_STATUS_IGNORE);

        #if DEBUG 
        printf("node %d released by root\n",myData.node);
        fflush(stdout);
        #endif

        if (ack == MPI_NACK)
        {
            printf("error in barrier call\n");
            fflush(stdout);
            exit(1);
        }
    }


    return MPI_SUCCESS;
}

/* Author: Stephen Ranshous */
int MPI_Finalize(void)
{
    if (my_state == INVALID)
    {
        perror("after finalize MPI calls are not allowed");
        exit(1);
    }

    /* If I am ROOT I am going to do a receive from each of the processes
     * to ensure that they all finish correctly. Once I receive every response
     * then I broadcast a message saying everyone terminate gracefully. */
    int ack = 0;
    if (myData.node == 0)
    {
        int i, allGood = ALL_GOOD;
        for (i = 1; i < worldData.nodes; i++)
        {
            MPI_Recv(&ack, 4, MPI_CHAR, i, MPI_ANY_TAG, MPI_Comm_World, MPI_STATUS_IGNORE);

            if (ack != MPI_ACK)
            {
                allGood = SHIT_ITS_NOT_ALL_GOOD;
            }
        }

        ack = allGood == ALL_GOOD ? MPI_ACK : MPI_NACK;

        for(i = 1; i < worldData.nodes; i++)
        {
            MPI_Send(&ack, 4, MPI_CHAR, i, MPI_ANY_TAG, MPI_Comm_World);
        }
    }
    else
    {
        ack = MPI_ACK;
        MPI_Send(&ack, 4, MPI_CHAR, 0, MPI_ANY_TAG, MPI_Comm_World);
        MPI_Recv(&ack, 4, MPI_CHAR, 0, MPI_ANY_TAG, MPI_Comm_World, MPI_STATUS_IGNORE);

        if (ack == MPI_NACK)
        {
            fprintf(stderr, "Rank %d error in finalize", myData.node);
            fflush(stderr);
            exit(1);
        }
    }

    /* If I am NOT ROOT then I am going to send a message to ROOT saying I am
     * done and that I'm waiting to finish. I will then wait for the reply so
     * I know whether to terminate successfully or not. */

    /* After returning from this function there can be no more MPI_xx calls.
     * This is not true in general (there exists a subset of MPI calls still
     * valid), but for this project it is true. */
    my_state = INVALID;
    return MPI_SUCCESS;
}

/* Author: Stephen Ranshous */
int MPI_Comm_rank(MPI_Comm comm, int *rank)
{
    /* Look up my rank */
    *rank = myData.node;
    return MPI_SUCCESS;
}

/* Author: Stephen Ranshous */
int MPI_Comm_size(MPI_Comm comm, int *size)
{
    /* Look up how many entries are in the table
     * or access the global variable holding this value */
    *size = worldData.nodes;
    return MPI_SUCCESS;
}

/* Author: Stephen DeGuglielmo */
void MPI_Init(int *argc, char ***argv)
{
    //Grab the MPI parameters
    int num = *argc;
    int mpiParamsStart = num-4;
    int np = atoi((*argv)[mpiParamsStart]);
    char name[256];
    int rank, port, socketFD;
    rank = atoi((*argv)[mpiParamsStart+3]);

    //If we are unable to get the current hostname, cannot continue
    if(gethostname(name, 255) == -1)
    {
        perror("hostname");
        exit(1);
    }

    //initialize the global data
    nodeTable = malloc(sizeof(node) * np);

    worldData.nodes = np;
    worldData.rootHost = strdup((*argv)[mpiParamsStart+1]);
    worldData.rootPort = atoi((*argv)[mpiParamsStart+2]);

    nodeTable[0].hostname = worldData.rootHost;
    nodeTable[0].rank = 0;
    nodeTable[0].port = worldData.rootPort;

    //Then start the MPI_Init code
    //if(strcmp(name, (*argv)[mpiParamsStart+1]) == 0) //Check to see if we are the designated root node
    if(rank == 0) //Check to see if we are the designated root node
    {
        //Serve a socket on our port for other nodes to connect to 
        socketFD = serve_socket(worldData.rootPort);
        port = worldData.rootPort;
        int i;

        #if DEBUG 
        printf("root served socket\n");
        fflush(stdout);
        #endif

        //Then, loop through the other number of nodes waiting on connections from them
        for(i = 1; i < np; i++)
        {

        	#if DEBUG 
            printf("root waiting on connection from a node\n");
            fflush(stdout);
            #endif

            int fd = accept_connection(socketFD); //this will block until a connection with a node is made
            
            #if DEBUG 
            printf("root got connection to a node\n");
            fflush(stdout);
            #endif

            //Then, start the handshake process
            //The first message will be an int describing the length of the payload to follow
            int nextMsgLength;
            if(read(fd, &nextMsgLength, sizeof(int)) == -1)
            {
                printf("Error reading hello message length for %d\n",i);
                fflush(stdout);
                exit(1);
            }
            else
            {
                //Then, read the payload
                mpiInitMsg msg;
                if(read(fd, &msg, nextMsgLength) == -1)
                {
                    printf("Error reading hello message for %d\n",i);
                    fflush(stdout);
                    exit(1);
                }
                else
                {
                    int node = msg.node;
                    //The payload contains the hostname and port of the remote node
                    //We assign it a rank and a fd to communicate with (response comes below after all other nodes have completed this first send)
                    nodeTable[node].hostname = strdup(msg.hostname);
                    nodeTable[node].rank = node;
                    nodeTable[node].port = msg.port;
                    nodeTable[node].fp = fd;

                    //Then reply back to the node with its rank
                    msg.node = node;
                    msg.type = 1;
                    nextMsgLength = sizeof(msg);
                    write(fd, &nextMsgLength, sizeof(int));
                    write(fd, &msg, nextMsgLength);
                }
            }
        }

        //All nodes have completed the send, now we can send the node table entires back to the other nodes

        //Loop through the entries in the node table
        for(i=1; i < np; i++)
        {

            node nodeData = nodeTable[i];
            mpiInitMsg msg;
            strcpy(msg.hostname, nodeData.hostname);
            msg.node = nodeData.rank;
            msg.port = nodeData.port;
            msg.type = 1;

            int j;
            //Send the entry back to every other node
            for(j = 1; j < np; j++)
            {
                if(j == i)
                    continue;

                int sendToFd = nodeTable[j].fp;

                int msgSize = sizeof(msg);
                write(sendToFd,&msgSize, sizeof(int));
                write(sendToFd, &msg, msgSize);
            }

        }

        mpiInitMsg acknowledge;
        acknowledge.type = 2;
        acknowledge.node = 0;

        //Finally, send the init_acknowledge msg to release all the other nodes from the init function
        for(i = 1; i < np; i++)
        {
            node nodeData = nodeTable[i];
            int msgSize = sizeof(acknowledge);
            write(nodeData.fp,&msgSize, sizeof(int));
            write(nodeData.fp,&acknowledge, msgSize);
        }
    }
    else //Else, we are not the root node
    {
        int startingPort = 9101;
        int count = 0;
        socketFD = -1;
        //First, serve our local socket
        while(socketFD == -1)
        {
            if(count > 50)
            {
                printf("Error finding an available port\n");
                fflush(stdout);
                exit(1);
            }

            startingPort++;
            socketFD = serve_socket(startingPort);
            count++;
        }

        port = startingPort;

        #if DEBUG 
        printf("rank: %d served socket on %d\n",rank,port);
        fflush(stdout);
        #endif

        //Then setup our mpi_init message to the root node
        mpiInitMsg initMsg;
        initMsg.type = 0;
        strcpy(initMsg.hostname, name);
        initMsg.port = startingPort;
        initMsg.node = rank;
        int rootFD = -1;
        int tries = 0;

        //Request connection to root
        while(rootFD == -1)
        {
            if(tries > 3)
            {
              printf("rank: %d DIED while attempting to connect to root\n",rank);
              fflush(stdout);
              exit(1);
            }

            #if DEBUG 
            if(tries > 1)
            {
                printf("rank: %d error attemtping request conn to root\n",rank);
                fflush(stdout);
            }

            printf("rank: %d attemtping request conn to root\n",rank);
            fflush(stdout);
            #endif

            rootFD = request_connection(worldData.rootHost, worldData.rootPort);
            tries++;
        }
        nodeTable[0].fp = rootFD;

        #if DEBUG 
        printf("rank: %d got connection to root\n",rank);
        fflush(stdout);
        #endif

        //Send our data to the root
        int msgLength = sizeof(initMsg);
        write(rootFD, &msgLength, sizeof(int));
        write(rootFD, &initMsg, msgLength);

        //Then listen for the reply back telling us our rank
        read(rootFD, &msgLength, sizeof(int));
        read(rootFD, &initMsg, msgLength);
        if(rank != initMsg.node)
        {
            printf("Error setting up ranks\n");
            fflush(stdout);
            exit(1);
        }

        rank = initMsg.node;

        //Then, get the nodeTable entries for every other node from the root
        int i;
        for(i = 1; i < np; i++)
        {
            if(i == rank)
                continue;

            int nodeMsgSize;
            mpiInitMsg replyMsg;
            read(rootFD, &nodeMsgSize, sizeof(int));
            read(rootFD, &replyMsg, nodeMsgSize);
            node *tmpNode = &nodeTable[replyMsg.node];
            tmpNode->hostname = strdup(replyMsg.hostname);
            tmpNode->rank = replyMsg.node;
            tmpNode->port = replyMsg.port;
            tmpNode->fp = rootFD;

            #if DEBUG 
            printf("Rank %d got table entry for rank %d: Host: %s, port: %d using fd: %d\n",rank, (nodeTable[replyMsg.node]).rank,(nodeTable[replyMsg.node]).hostname,(nodeTable[replyMsg.node]).port,(nodeTable[replyMsg.node]).fp);
            fflush(stdout);
            #endif
        }

        //Now listen for the "done" message telling the node it is free to leave the mpi_init function
        read(rootFD, &msgLength, sizeof(int));
        read(rootFD, &initMsg, msgLength);
    }

    //Setup my data
    myData.node = rank;
    myData.port = port;
    myData.hostname = strdup(name);
    myData.socketFd = socketFD;

    //Adjust the argc before continuing onto the users' program
    (*argc) -= 4;
}

/* Author: Mike O'Brien */
void MPI_Send(void * buffer, int count, int type, int dest, int tag, int comm)
{

    if (dest >= worldData.nodes) //check that dest is in bounds
    {
        fprintf(stderr,"Node %d doesn't exist\n",dest);
        exit(1);
    }

    //get port and hostna

    node sendNode = nodeTable[dest];

    #if DEBUG 
    printf("Rank %d is sending a message to rank %d. Named: %s port %d\n",myData.node,dest, sendNode.hostname, sendNode.port);

    printf("Rank %d got FD to rank %d\n",myData.node, dest);
    fflush(stdout);
    #endif

    //write
    int bWritten = 0;
    while(bWritten != count)
    {
        bWritten += write(sendNode.fp, (void*) buffer+bWritten, (count*sizeof(char) - bWritten));
    }

    #if DEBUG 
    printf("Rank %d wrote %d bytes of data to rank %d at fd %d\n",myData.node, bWritten, dest, sendNode.fp);
    fflush(stdout);
    #endif

    return;
}

/* Author: Mike O'Brien */
void MPI_Recv(void * buffer, int count, int type, int source, int tag, int comm, int status)
{

    if (source >= worldData.nodes) //check that dest is in bounds
    {
        fprintf(stderr,"Node %d doesn't exist\n",source);
        exit(1);
    }

    node recNode = nodeTable[source];

    //read
    int bRead = 0;
    while(bRead != count)
    {
        bRead += read(recNode.fp, (void*) buffer+bRead, (count*sizeof(char) - bRead));
    }

    #if DEBUG 
    printf("rank %d read %d bytes of data from %d at fd %d\n", myData.node, bRead, source, recNode.fp);
    #endif
    
    return;
}
