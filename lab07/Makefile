all: client/app_client.c server/app_server.c client/stcp_client.c server/stcp_server.c common/seg.c
	gcc -g -pthread server/app_server.c common/seg.c server/stcp_server.c -o server/lab7_server
	gcc -g -pthread client/app_client.c common/seg.c client/stcp_client.c -o client/lab7_client
	ctags -R

server: server/app_server.c server/stcp_server.c common/seg.c
	gcc -g -pthread server/app_server.c common/seg.c server/stcp_server.c -o server/lab7_server

client: client/app_client.o client/stcp_client.o common/seg.o common/misc.o
	gcc -g -pthread client/app_client.c common/seg.c client/stcp_client.c -o client/lab7_client


clean:
	rm -rf client/*.o
	rm -rf server/*.o
	rm -rf common/*.o
	rm -rf server/tags
	rm -rf tags
	rm -rf client/lab7_client
	rm -rf server/lab7_server

tags:
	ctags -R
