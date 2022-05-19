all: server client

server: server.c
	gcc -Wall -o server server.c -pthread

client: client.c
	gcc -Wall -o client client.c -pthread

clean:
	rm -f server client
	rm -f *.o
