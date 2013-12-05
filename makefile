CC=gcc
OBJ=tatl_core.o tatl_client.o tatl_server.o eztcp.o sassyhash.o linked.o vegCrypt.o
DEPS=tatl_core.h tatl_client.h tatl_server.h eztcp.h sassyhash.h linked.h vegCrypt.h
CFLAGS=-lpthread -Wall -lgmp -lgcrypt -lm

all: client server

client: client.o $(OBJ)
	 $(CC) -o $@ $^ $(CFLAGS)

server: server.o $(OBJ)
	 $(CC) -o $@ $^ $(CFLAGS)

%.o: %.c $(DEPS)
	$(CC) -c $< $(CFLAGS)

clean:
	rm -rf *.o client server
