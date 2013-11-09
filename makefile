main=test_tatl
CC=gcc
OBJ=$(main).o tatl.o eztcp.o basic_map.o
DEPS=tatl.h eztcp.h
CFLAGS=-lpthread -Wall

$(main): $(main).o $(OBJ)
	 $(CC) -o $@ $^ $(CFLAGS)

%.o: %.c $(DEPS)
	$(CC) -c $< $(CFLAGS)

clean:
	rm -rf $(main) $(OBJ)
