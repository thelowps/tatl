CC=gcc
OBJ=tatl_core.o tatl_client.o tatl_server.o eztcp.o sassyhash.o linked.o
DEPS=tatl_core.h tatl_client.h tatl_server.h eztcp.h sassyhash.h linked.h
CFLAGS=-lpthread -Wall

all: client client2 server

client: client.o $(OBJ)
	 $(CC) -o $@ $^ $(CFLAGS)

client2: client2.o $(OBJ)
	 $(CC) -o $@ $^ $(CFLAGS)

server: server.o $(OBJ)
	 $(CC) -o $@ $^ $(CFLAGS)

%.o: %.c $(DEPS)
	$(CC) -c $< $(CFLAGS)

clean:
	rm -rf *.o client client2 server
