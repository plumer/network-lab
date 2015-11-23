//文件名: sip/sip.c
	//你需要编写这里的代码.
//
//描述: 这个文件实现SIP进程  
//
//创建日期: 2015年

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <assert.h>
#include <sys/utsname.h>
#include <pthread.h>
#include <unistd.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "../topology/topology.h"
#include "sip.h"

#define SIP_WAITTIME 10
/**************************************************************/
//声明全局变量
/**************************************************************/
int son_conn; 			//到重叠网络的连接
int stcp_conn;			//到STCP的连接
nbr_cost_entry_t* nct;			//邻居代价表
dv_t* dv;				//距离矢量表
pthread_mutex_t* dv_mutex;		//距离矢量表互斥量
routingtable_t* routingtable;		//路由表
pthread_mutex_t* routingtable_mutex;	//路由表互斥量

/**************************************************************/
//实现SIP的函数
/**************************************************************/

//SIP进程使用这个函数连接到本地SON进程的端口SON_PORT
//成功时返回连接描述符, 否则返回-1
int connectToSON() {
	int conn = socket(AF_INET, SOCK_STREAM, 0);
	if (conn < 0){
		print_pos();
		printf("ERROR: create son_conn failed\n");
		return -1;
	}
	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(struct sockaddr_in));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(SON_PORT);
	//servaddr.sin_addr.s_addr = htonl(INADDR_ANY);	//local
	inet_aton("127.0.0.1", &(servaddr.sin_addr));

	int result;
	int retry_ct = 10;
	do {
		result = connect(conn, (struct sockaddr *)&servaddr, sizeof(struct sockaddr));
		if (result == -1){
			print_pos();
			printf("ERROR: sip connect to son error, retry\n");
		}

		if (result == -1) sleep(1);
		retry_ct --;
	} while (result == -1 && retry_ct > 0);

	if (retry_ct == 0 && result == -1) {
		print_pos();
		printf("ERROR: sip connect retry too many times\n");
		return -1;
	}
	return conn;
}

void display(int me, pkt_routeupdate_t *dat){
	int i;
	for (i = 0; i < dat->entryNum; i ++)
		printf("%d --> %d cost=%d\n", me, dat->entry[i].nodeID, dat->entry[i].cost);
}
//这个线程每隔ROUTEUPDATE_INTERVAL时间就发送一条路由更新报文
//在本实验中, 这个线程只广播空的路由更新报文给所有邻居, 
//我们通过设置SIP报文首部中的dest_nodeID为BROADCAST_NODEID来发送广播
void* routeupdate_daemon(void* arg) {
	sip_pkt_t tmp;
	pkt_routeupdate_t route_data;	//使用这个来填充tmp的data部分
	while (1) {
		memset(&tmp, 0, sizeof(sip_pkt_t));
		memset(&route_data, 0, sizeof(pkt_routeupdate_t));

		// 条目数
		route_data.entryNum = topology_getNodeNum();
		// 邻居们和我的node id
		int *nodes = topology_getNodeArray();
		int mynode = topology_getMyNodeID();
		if (mynode == -1) {
			printf("ERROR: get my node failed\n");
		}

		// 填充路由更新表
		int i;
		for (i = 0; i < route_data.entryNum; i++){
			route_data.entry[i].nodeID = nodes[i];
			route_data.entry[i].cost = dvtable_getcost(dv, mynode, nodes[i]);
		}
		
		// 填充sip_pkt_t
		tmp.header.src_nodeID = topology_getMyNodeID();
		tmp.header.dest_nodeID = BROADCAST_NODEID;
		tmp.header.type = ROUTE_UPDATE;
		tmp.header.length = sizeof(pkt_routeupdate_t);
		memcpy(tmp.data, &route_data, sizeof(pkt_routeupdate_t));

		// 输出路由更新报文
/*		printf("\n--------A ROUTE UPDATE includes %d entries---------\n", route_data.entryNum);
		printf("-----from-neighbor-%d-----\n", tmp.header.src_nodeID);
		display(mynode, &route_data);
		printf("--------------------------------------------------\n\n");*/
		// 填充完成，发送
		printf("MSG: send broad cast...\n");
		int result = son_sendpkt(BROADCAST_NODEID, &tmp, son_conn);
		if (result == -1) {
			print_pos();
			printf("ERROR: broadcast son_sendpkt failed\n");
		} 
		sleep(ROUTEUPDATE_INTERVAL);
	}
	pthread_exit(NULL);
}

//这个线程处理来自SON进程的进入报文
//它通过调用son_recvpkt()接收来自SON进程的报文
//在本实验中, 这个线程在接收到报文后, 只是显示报文已接收到的信息, 并不处理报文 
void* pkthandler(void* arg) {
	sip_pkt_t pkt;

	while(son_recvpkt(&pkt,son_conn)>0) {
		// 如果是我的包，发给stcp
		if (pkt.header.type == SIP && pkt.header.dest_nodeID == topology_getMyNodeID()){
			int result = forwardsegToSTCP(stcp_conn, pkt.header.src_nodeID, (seg_t *)(pkt.data));
			if (result == -1) {
				print_pos();
				printf("ERROR: pkthandler son->sip->stcp send failed\n");
			}
		}
		// 否则，发给son
		else if (pkt.header.type == SIP && pkt.header.dest_nodeID != topology_getMyNodeID()){
			int nextNode = routingtable_getnextnode(routingtable, pkt.header.dest_nodeID);
			int result = son_sendpkt(nextNode, &pkt, son_conn);
			if (result == -1) {
				print_pos();
				printf("ERROR: pkthandler son->son send failed\n");
			}
		}
		else if (pkt.header.type == ROUTE_UPDATE){
			int result = route_dvt_update(nct, dv, routingtable, 
				(pkt_routeupdate_t *)(pkt.data), pkt.header.src_nodeID, routingtable_mutex);
			if (result == -1) {
				print_pos();
				printf("Error: update routing table failed\n");
			}
			else {
/*				printf("\n----RECV A ROUTE UPDATE include %d entries----\n", ((pkt_routeupdate_t *)(pkt.data))->entryNum);
				display(pkt.header.src_nodeID, (pkt_routeupdate_t *)(pkt.data));
				printf("----------------------------------------------\n\n");*/
			}
		}
	}
	printf("ERROR: son_recvpkt failed\n");
	close(son_conn);
	son_conn = -1;
	pthread_exit(NULL);
}

//这个函数终止SIP进程, 当SIP进程收到信号SIGINT时会调用这个函数. 
//它关闭所有连接, 释放所有动态分配的内存.
void sip_stop() {
	close(stcp_conn);
	printf("MSG: close stcp_conn ok\n");
	close(son_conn);
	printf("MSG: close son_conn ok\n");
	exit(1);
}

void waitSTCP() {
	//你需要编写这里的代码.
	int connfd, listenfd;
	socklen_t clilen;
	clilen = sizeof(struct sockaddr_in);
	struct sockaddr_in cliaddr, servaddr;
	memset(&cliaddr, 0, sizeof(cliaddr));
	memset(&servaddr, 0, sizeof(servaddr));

	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(SIP_PORT);

	printf("MSG: listen stcp at %s:%d\n", inet_ntoa(servaddr.sin_addr), SIP_PORT);
	const int on = 1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(const int));
	bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
	listen(listenfd, MAX_TRANSPORT_CONNECTIONS);

	while (1){
		connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
		if (connfd == -1){
			print_pos();
			printf("MSG: accept stcp failed, retry\n");
			sleep(1);
			continue;
		}
		else {
			stcp_conn = connfd;
			int dest = -1;
			seg_t seg;
			memset(&seg, 0, sizeof(seg_t));
			sip_pkt_t send_data;
			memset(&send_data, 0, sizeof(sip_pkt_t));

			// getsegToSend
			int getResult;
			while (1){
				getResult = getsegToSend(stcp_conn, &dest, &(seg));
				if (getResult == 1){
					send_data.header.src_nodeID = topology_getMyNodeID();
					send_data.header.dest_nodeID = dest;
					send_data.header.length = sizeof(seg_t);
					send_data.header.type = SIP;
					memcpy(send_data.data, &seg, sizeof(seg_t));
					// search routing table for next node id
					int nextNode = routingtable_getnextnode(routingtable, dest);
					if (nextNode == -1){
						print_pos();
						printf("Error: unknown next node id\n");
					}
					int result = son_sendpkt(nextNode, &send_data, son_conn);
					if (result == -1) {
						print_pos();
						printf("Error: sip son_sendpkt failed\n");
					}
				}
				else if (getResult == -1) {
					print_pos();
					printf("Error: checksum error\n");
				}
				else if (getResult == 2) break;
			}
			printf("MSG: stcp disconnect, wait for another stcp\n");
		}
	}
	printf("MSG: exit stcp loop\n");
	pthread_exit(NULL);

	return;
}

int main(int argc, char *argv[]) {
	printf("SIP layer is starting, pls wait...\n");

	//初始化全局变量
	nct = nbrcosttable_create();
	dv = dvtable_create();
	dv_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(dv_mutex,NULL);
	routingtable = routingtable_create();
	routingtable_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(routingtable_mutex,NULL);
	son_conn = -1;
	stcp_conn = -1;

	nbrcosttable_print(nct);
	dvtable_print(dv);
	routingtable_print(routingtable);

	//注册用于终止进程的信号句柄
	signal(SIGINT, sip_stop);

	//连接到本地SON进程 
	son_conn = connectToSON();
	if(son_conn<0) {
		printf("can't connect to SON process\n");
		exit(1);		
	}
	
	//启动线程处理来自SON进程的进入报文 
	pthread_t pkt_handler_thread; 
	pthread_create(&pkt_handler_thread,NULL,pkthandler,(void*)0);

	//启动路由更新线程 
	pthread_t routeupdate_thread;
	pthread_create(&routeupdate_thread,NULL,routeupdate_daemon,(void*)0);	

	printf("SIP layer is started...\n");
	printf("waiting for routes to be established\n");
	sleep(SIP_WAITTIME);
	routingtable_print(routingtable);

	//等待来自STCP进程的连接
	printf("waiting for connection from STCP process\n");
	waitSTCP(); 
}


