all: bankserver bankclient

bankclient:	
	gcc -g -pthread bankclient.c -lpthread -o bankclient
bankserver:
	gcc -g -pthread bankserver.c -lpthread -o bankserver 
clean:
	rm -f bankclient bankserver
