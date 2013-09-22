CC=gcc
CFLAGS=-g -Wall -std=c99
LDFLAGS=
EXEC=rtt
DEPS=my_mpi.h sockettome.h
SOURCES=rtt.c my_mpi.c sockettome.c
OBJECTS=$(SOURCES:.c=.o)

all: $(SOURCES) $(EXEC)

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

$(EXEC): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

clean:
	rm -f $(OBJECTS)
