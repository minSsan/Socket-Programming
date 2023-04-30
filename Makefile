CC = gcc

myserver.o: myserver.c
	$(CC) myserver.c -o myserver

clean:
	rm -rf *.o myserver