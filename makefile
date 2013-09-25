#find MPI path
MPIC++ = $(shell which mpiCC)
MPI_ROOT = $(dir $(MPIC++))

CC = mpicc
NVCC = nvcc

CFLAGS = 
#NVFLAGS = -Xcompiler -O2 -lm -arch=sm_20
NVFLAGS = -gencode=arch=compute_13,code=\"sm_13,compute_13\" -c -O3 \
	-I$(MPI_ROOT)/../include
NVLIBS = -L/usr/local/cuda/lib64 -lcudart -lcuda

GCC=gcc
GCCFLAGS=-g -Wall -std=c99
EXEC=rtt
DEPS=my_mpi.h sockettome.h
SOURCES=rtt.c my_mpi.c sockettome.c
OBJECTS=$(SOURCES:.c=.o)

all: interp_gpu interp_serial $(SOURCES) $(EXEC)

clean: 
	rm -f interp interp_gpu.o $(OBJECTS) interp_serial interp_serial.o

interp_gpu.o: interp_gpu.cu
	$(NVCC) $(NVFLAGS) -c interp_gpu.cu interp_gpu.o

interp_gpu: interp_gpu.o
	$(CC) $(NVLIBS) -o interp_gpu interp_gpu.o

%.o: %.c $(DEPS)
	$(GCC) $(GCCFLAGS) -c -o $@ $<

interp_serial.o: interp_serial.c
	$(GCC) $(GCCFLAGS) -c interp_serial.c -o interp_serial.o

interp_serial: interp_serial.o
	$(GCC) $(GCCFLAGS) interp_serial.o -o interp_serial

$(EXEC): $(OBJECTS)
	$(GCC) $(OBJECTS) -o $@
