all: client server

client:
	gcc -o tsp-client tsp-client.c sscript.c

server:
	gcc -o tsp-server-run tsp-server-run.c
	gcc -o tsp-skin socker.c

clean:
	rm -f *~
	rm -f tsp-server-run tsp-client tsp-skin