all: son/son sip/sip client/app_simple_client server/app_simple_server client/app_stress_client server/app_stress_server   

CFLAGS = -Wall -pedantic -g -std=gnu99
LDFLAGS = -lrt

common/pkt.o: common/pkt.c common/pkt.h common/constants.h
	gcc $(CFLAGS) $(LDFLAGS) -c common/pkt.c -o common/pkt.o
topology/topology.o: topology/topology.c 
	gcc $(CFLAGS) $(LDFLAGS) -c topology/topology.c -o topology/topology.o
son/neighbortable.o: son/neighbortable.c
	gcc $(CFLAGS) $(LDFLAGS) -c son/neighbortable.c -o son/neighbortable.o
son/son: topology/topology.o common/pkt.o son/neighbortable.o son/son.c 
	gcc $(CFLAGS) $(LDFLAGS) -pthread son/son.c topology/topology.o common/pkt.o son/neighbortable.o -o son/son
sip/nbrcosttable.o: sip/nbrcosttable.c
	gcc $(CFLAGS) $(LDFLAGS) -c sip/nbrcosttable.c -o sip/nbrcosttable.o
sip/dvtable.o: sip/dvtable.c
	gcc $(CFLAGS) $(LDFLAGS) -c sip/dvtable.c -o sip/dvtable.o
sip/routingtable.o: sip/routingtable.c
	gcc $(CFLAGS) $(LDFLAGS) -c sip/routingtable.c -o sip/routingtable.o
sip/sip: common/pkt.o common/seg.o topology/topology.o sip/nbrcosttable.o sip/dvtable.o sip/routingtable.o sip/sip.c 
	gcc $(CFLAGS) $(LDFLAGS) -pthread sip/nbrcosttable.o sip/dvtable.o sip/routingtable.o common/pkt.o common/seg.o topology/topology.o sip/sip.c -o sip/sip 
client/app_simple_client: client/app_simple_client.c common/seg.o client/stcp_client.o topology/topology.o 
	gcc $(CFLAGS) $(LDFLAGS) -pthread client/app_simple_client.c common/seg.o client/stcp_client.o topology/topology.o -o client/app_simple_client 
client/app_stress_client: client/app_stress_client.c common/seg.o client/stcp_client.o topology/topology.o 
	gcc $(CFLAGS) $(LDFLAGS) -pthread client/app_stress_client.c common/seg.o client/stcp_client.o topology/topology.o -o client/app_stress_client 
server/app_simple_server: server/app_simple_server.c common/seg.o server/stcp_server.o topology/topology.o 
	gcc $(CFLAGS) $(LDFLAGS) -pthread server/app_simple_server.c common/seg.o server/stcp_server.o topology/topology.o -o server/app_simple_server
server/app_stress_server: server/app_stress_server.c common/seg.o server/stcp_server.o topology/topology.o 
	gcc $(CFLAGS) $(LDFLAGS) -pthread server/app_stress_server.c common/seg.o server/stcp_server.o topology/topology.o -o server/app_stress_server
common/seg.o: common/seg.c common/seg.h
	gcc $(CFLAGS) $(LDFLAGS) -c common/seg.c -o common/seg.o
client/stcp_client.o: client/stcp_client.c client/stcp_client.h 
	gcc $(CFLAGS) $(LDFLAGS) -c client/stcp_client.c -o client/stcp_client.o
server/stcp_server.o: server/stcp_server.c server/stcp_server.h
	gcc $(CFLAGS) $(LDFLAGS) -c server/stcp_server.c -o server/stcp_server.o

clean:
	rm -rf common/*.o
	rm -rf topology/*.o
	rm -rf son/*.o
	rm -rf son/son
	rm -rf sip/*.o
	rm -rf sip/sip 
	rm -rf client/*.o
	rm -rf server/*.o
	rm -rf client/app_simple_client
	rm -rf client/app_stress_client
	rm -rf server/app_simple_server
	rm -rf server/app_stress_server
	rm -rf server/receivedtext.txt



