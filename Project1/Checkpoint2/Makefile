all: mylib.so server

mylib.o: mylib.c
	gcc -Wall -fPIC -DPIC -c mylib.c

mylib.so: mylib.o
	ld -shared -o mylib.so mylib.o -ldl

server: server.c
	gcc -Wall server.c -o server

clean:
	rm -f *.o *.so server
handin:
	tar cvzf ./mysolution.tgz Makefile *.c

