CC=gcc
OBJ=tatl.o eztcp.o sassyhash.o linked.o
DEPS=tatl.h eztcp.h
CFLAGS=-lpthread -Wall

all: client server

client: client.o $(OBJ)
	 $(CC) -o $@ $^ $(CFLAGS)

server: server.o $(OBJ)
	 $(CC) -o $@ $^ $(CFLAGS)

%.o: %.c $(DEPS)
	$(CC) -c $< $(CFLAGS)

clean:
	rm -rf $(OBJ) client.o server.o client server
