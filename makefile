DEBUF = #-DUNIX
all:server client chat
server:server.cpp
	g++ -o svrver server.cpp LibeventSvr.cpp -levent -lpthread -levent_pthreads $(DEBUF)
client:client.cpp
	g++ -o client client.cpp LibeventCli.cpp -levent -lpthread -levent_pthreads $(DEBUF)
chat:chatRoom.c
	gcc -o chat chatRoom.c -levent -lpthread 
.PHONY:clean
clean:
	rm -f svrver client chat test.sock
