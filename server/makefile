CC=gcc
OBJ=tatl_core.o tatl_server.o eztcp.o sassyhash.o linked.o vegCrypt.o
DEPS=tatl_core.h tatl_client.h tatl_server.h eztcp.h sassyhash.h linked.h vegCrypt.h
CFLAGS=-lpthread -Wall -lgcrypt -lm -g -Igmp/include 

all: server

server: server.o $(OBJ)
	 $(CC) -o $@ $^ $(CFLAGS)

%.o: %.c $(DEPS)
	$(CC) -c $< $(CFLAGS)

clean:
	rm -rf *.o server
