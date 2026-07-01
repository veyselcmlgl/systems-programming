CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11 

all: neHosServer neHosClient 


neHosServer: neHosServer.c
	$(CC) $(CFLAGS) -o neHosServer neHosServer.c 

neHosClient: neHosClient.c
	$(CC) $(CFLAGS) -o neHosClient neHosClient.c 


clean:
	rm -f neHosServer neHosClient
	rm -rf