.c.o:
	gcc -c $<

OBJ=taskpool.o  workerpool.o socklib.o list.o threadpool.o mpool.o
#CFLAGS = gcc -Wall -O2  -g -pg -DLinux -DMEMWATCH -DMW_STDIO -pipe
CFLAGS = gcc -Wall -O2  -g -pg -DLinux  -pipe


netmain:$(OBJ)
	$(CFLAGS)  -o netmain $(OBJ) main.c memwatch.c -lpthread

clean:
	rm -f *.o
	rm -f netmain
