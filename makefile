all: test_tatl

test_tatl: test_tatl.o tatl.o eztcp.o
	gcc test_tatl.o tatl.o eztcp.o -o test_tatl

test_tatl.o: test_tatl.c
	gcc -c test_tatl.c

tatl.o: tatl.c
	gcc -c tatl.c

eztcp.o: eztcp.c
	gcc -c eztcp.c

clean:
	rm -rf test_tatl *.o
