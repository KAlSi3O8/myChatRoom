all : 
	@echo choose server or client

server : Server/server_linux.c
	@gcc Server/server_linux.c -o out/server -lpthread

client : Client/client.c
	@gcc Client/client.c -o out/client -lws2_32