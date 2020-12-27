all: client server

client: client.c packet.h
	gcc -pthread client.c -o client

server: server.c packet.h session.h
	gcc -pthread server.c -o server

server_debug: server.c packet.h session.h
	gcc -pthread -g -Wall server.c -o server

clean:
	rm -f client server