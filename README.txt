simple http server
server.c
contain implementation of HTTP server
threadpool.c
contain implementation of threadpool for the server
README
this file
additional info:
how to use:
	compile:gcc server.c -lpthread -o server
	run: ./server <port> <pool-size> <max-request>

by Elya Athlan