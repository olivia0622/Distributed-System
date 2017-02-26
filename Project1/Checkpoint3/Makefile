all: mylib.so server

mylib.o: mylib.c
	gcc -g -Wall -fPIC -DPIC -c mylib.c

mylib.so: mylib.o
	ld -shared -o mylib.so mylib.o -ldl

server: server.c
	gcc -g -Wall -I../include -L../lib -ldirtree server.c -o server
clean:
	rm -f *.o *.so server
handin:
	tar cvzf ./mysolution.tgz Makefile *.c *.pdf

