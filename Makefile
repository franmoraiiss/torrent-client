all:
	gcc ./server/server.c -o ./server/server
	gcc ./clients/client9001/client9001.c -o ./clients/client9001/client9001
	gcc ./clients/client9002/client9002.c -o ./clients/client9002/client9002

clean:
	rm ./server/server ./clients/client9001/client9001 ./clients/client9002/client9002
