//文件名: son/son.c
//
//描述: 这个文件实现SON进程 
//SON进程首先连接到所有邻居, 然后启动listen_to_neighbor线程, 每个该线程持续接收来自一个邻居的进入报文, 并将该报文转发给SIP进程. 
//然后SON进程等待来自SIP进程的连接. 在与SIP进程建立连接之后, SON进程持续接收来自SIP进程的sendpkt_arg_t结构, 并将接收到的报文发送到重叠网络中. 
//
//创建日期: 2015年
//#define _BSD_SOURC
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <sys/utsname.h>
#include <assert.h>
#include <errno.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "../sip/sip.h"
#include "son.h"
#include "../topology/topology.h"
#include "neighbortable.h"

//你应该在这个时间段内启动所有重叠网络节点上的SON进程
#define SON_START_DELAY 10

/**************************************************************/
//声明全局变量
/**************************************************************/

//将邻居表声明为一个全局变量 
nbr_entry_t* nt; 
//将与SIP进程之间的TCP连接声明为一个全局变量
int sip_conn; 

/**************************************************************/
//实现重叠网络函数
/**************************************************************/

// 这个线程打开TCP端口CONNECTION_PORT, 等待节点ID比自己大的所有邻居的进入连接,
// 在所有进入连接都建立后, 这个线程终止. 
in_addr_t getIPByNode(int id){
	int ct = topology_getNbrNum();
	int i;
	for (i = 0; i < ct; i ++)
		if (nt[i].nodeID == id) return nt[i].nodeIP;
	print_pos();
	printf("ERROR: no such node, return ip = 0\n");
	return 0;
}
int getConnByNode(int id){
	int ct = topology_getNbrNum();
	int i;
	for (i = 0; i < ct; i++)
		if (nt[i].nodeID == id) return nt[i].conn;
	print_pos();
	printf("ERROR: no such node, return conn = -1\n");
	return -1;
}
void* waitNbrs(void* arg) {
	//你需要编写这里的代码.
	int connfd, listenfd;
	socklen_t clilen;
	struct sockaddr_in cliaddr, servaddr;

	clilen = sizeof(cliaddr);
	memset(&cliaddr, 0, sizeof(cliaddr));
	memset(&servaddr, 0, sizeof(servaddr));

	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(CONNECTION_PORT);

	print_pos();
	printf("\tMSG: son listen son at %s:%d\n", inet_ntoa(servaddr.sin_addr), CONNECTION_PORT);
	const int on = 1;
	int res;
	res = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(const int));
	if (res != 0) {
		print_pos();
		printf("SON ERROR: setsockopt failed\n");
	}
	res = bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
	if (res != 0) {
		print_pos();
		printf("SON ERROR: bind failed\n");
	}
	res = listen(listenfd, 10);
	if (res != 0) {
		print_pos();
		printf("SON ERROR: listen failed\n");
	}

	// get my node id
	int myNodeID = topology_getMyNodeID();
//	print_pos();
//	printf("\tMSG: MyNodeID = %d\n", myNodeID);
	if (myNodeID == -1){
		print_pos();
		printf("ERROR: get my node id failed\n");
		exit(1);
	}
	// get my neighbor number
	int nbrNum = topology_getNbrNum();
	int *nbr = topology_getNbrArray();

	// count connect nbr
	int t = 0, i = 0;
	for (; t < nbrNum; t++) if (nbr[t] > myNodeID) i++;

//	print_pos();
//	printf("MSG: I should accept %d connects\n", i);
	for (; i > 0; i --){
//		print_pos();
//		printf("MSG: wait son connect...\n");
		connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
		if (connfd == -1){
			print_pos();
			printf("MSG: accept failed, retry, errno=%s\n", strerror(errno));
			i++;
			continue;
		}
		// get neighbor node id
		int nbrNodeID = topology_getNodeIDfromip(&cliaddr.sin_addr);

//		print_pos();
//		printf("MSG: accept %s\n", inet_ntoa(cliaddr.sin_addr));
		//		if (nbrNodeID > myNodeID){
//		print_pos();
//		printf("\tMSG: setup connection from %d to %d\n", nbrNodeID, myNodeID);
		int res = nt_addconn(nt, nbrNodeID, connfd);
		if (res == -1) {
			printf("ERROR: nt_addconn error\n");
			pthread_exit(NULL);
		}
//		print_pos();
//		printf("MSG: add node=%d, conn=%d\t to nt\n", nbrNodeID, connfd);
		//		}
	}
	printf("MSG: son connect neighbors ok\n");
	pthread_exit(NULL);
}

// 这个函数连接到节点ID比自己小的所有邻居.
// 在所有外出连接都建立后, 返回1, 否则返回-1.
int connectNbrs() {
	int sockfd[MAX_NODE_NUM];
	int myNodeID = topology_getMyNodeID();
	int *nbr = topology_getNbrArray();
	int t = topology_getNbrNum();
//	print_pos();
//	printf("MSG:  topology nbr num = %d\n", t);
	int i;
//	for (i = 0; i < t; i ++) printf("MSG: nbr item %d\n", nbr[i]);

	struct sockaddr_in servaddr;
	// get conn count
	int conn_ct = 0;
	for (i = 0; i < t; i ++) {
		if (nbr[i] < myNodeID) conn_ct++;
	}

	// create socket
	for (i = 0; i < conn_ct; i ++){
		sockfd[i] = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd[i] < 0){
			print_pos();
			printf("\tError: create connect Nbrs socket failed\n");
			return -1;
		}
	}

	// set up tcp connect
	int j = 0;
	for (i = 0; i < t; i ++){
		if (nbr[i] < myNodeID) {
			memset(&servaddr, 0, sizeof(struct sockaddr_in));
			servaddr.sin_family = AF_INET;
			servaddr.sin_port = htons(CONNECTION_PORT);
			servaddr.sin_addr.s_addr = getIPByNode(nbr[i]);

//			print_pos();
//			printf("MSG: prepare to connect %s:%d, node=%d\n", inet_ntoa(servaddr.sin_addr), CONNECTION_PORT, nbr[i]);
			// set up connection
			int result;
			int retry_ct = 10;
			do {
//				print_pos();
//				printf("\tMSG: try to connect %d -> %d\n", myNodeID, nbr[i]);
				result = connect(sockfd[j], (struct sockaddr *)&servaddr, sizeof(struct sockaddr));
				if (result == -1){
					print_pos();
					printf("ERROR: connect %d->%d, failed, retry, errno=%s\n", myNodeID, nbr[i], strerror(errno));
				}
//				else printf("GOOD: connect success\n");
				retry_ct --;
				if (result == -1) sleep(1);
			} while (result == -1 && retry_ct > 0);

			// store in neighbor table
			if (result != -1){
				result = nt_addconn(nt, nbr[i], sockfd[j]);
				if (result == -1){
					print_pos();
					printf("\tERROR: nt_addconn error\n");
					return -1;

				}
				printf("MSG: add nt node=%d, conn=%d\n", nbr[i], sockfd[j]);
				j ++;
			} else {
				print_pos();
				printf("ERROR: tcp conn failed, retry too many times\n");
				return -1;
			}
		}
	}
	return 1;
}

//每个listen_to_neighbor线程持续接收来自一个邻居的报文. 它将接收到的报文转发给SIP进程.
//所有的listen_to_neighbor线程都是在到邻居的TCP连接全部建立之后启动的. 
void* listen_to_neighbor(void* arg) {
	int index = *(int *)arg;
//	printf("MSG: listen to neighbor node=%d\n", nt[index].nodeID);
	sip_pkt_t tmp;
	if (nt[index].conn == -1) {
		print_pos();
		printf("ERROR: connect not set for node %d\n", nt[index].nodeID);
		pthread_exit(NULL);
	}
	while (1) {
		memset(&tmp, 0, sizeof(sip_pkt_t));
		// recv a pkt from son
//		print_pos();
//		printf("MSG: prepare to recvpkt from socket=%d\n", nt[index].conn);
		int result;
		result = recvpkt(&tmp, nt[index].conn);
		if (result == -1){
			print_pos();
			printf("ERROR: recvpkt from neighbor node=%d socket=%d failed\n", nt[index].nodeID, nt[index].conn);
			sleep(1);
		}
//		else printf("MSG: recvpkt sip_pkt_t ok, send to sip...\n");
		// forward the pkt to sip
		result = forwardpktToSIP(&tmp, sip_conn);
		if (result == -1){
			print_pos();
			printf("ERROR: forward pkt to sip failed\n");
		}
//		else printf("MSG: send to sip ok\n");
	}
  	pthread_exit(NULL);
}

//这个函数打开TCP端口SON_PORT, 等待来自本地SIP进程的进入连接. 
//在本地SIP进程连接之后, 这个函数持续接收来自SIP进程的sendpkt_arg_t结构, 并将报文发送到重叠网络中的下一跳. 
//如果下一跳的节点ID为BROADCAST_NODEID, 报文应发送到所有邻居节点.
void waitSIP() {
	int listenfd;
	socklen_t clilen;
	clilen = sizeof(struct sockaddr_in);
	struct sockaddr_in cliaddr, servaddr;
	memset(&cliaddr, 0, sizeof(cliaddr));
	memset(&servaddr, 0, sizeof(servaddr));

	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	
	servaddr.sin_family = AF_INET;
	// servaddr.sin_addr.s_addr = INADDR_ANY;
	inet_aton("127.0.0.1", &servaddr.sin_addr);
	servaddr.sin_port = htons(SON_PORT);

	const int on = 1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(const int));
	bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
	listen(listenfd, MAX_NODE_NUM);

	while (1) {
		// wait for sip connection
		memset(&cliaddr, 0, sizeof(struct sockaddr));
		sip_conn = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
		if (sip_conn == -1) {
			print_pos();
			printf("ERROR: accept sip connection failed\n");
			sleep(1);
			continue;
		}

		/* recv from sip and son forward start here */
//		print_pos();
//		printf("MSG: sip access ok, sip_conn = %d, begin to convey sip->son\n", sip_conn);
		sip_pkt_t tmp;
		int nNode, result;
		while (1){
			memset(&tmp, 0, sizeof(sip_pkt_t));
			result = getpktToSend(&tmp, &nNode, sip_conn);
			if(result == -1) {
				print_pos();
				printf("ERROR: get pkt from sip failed\nMSG: Break loop wait another sip connect\n");
				sleep(1);
				break;
			}
			// print_pos();
			// printf("MSG: get sip_pkt_t to send ok, send to nbr...\n");
			// if broad cast, send to every neighbor
			if (nNode == BROADCAST_NODEID){
				int ct = topology_getNbrNum();
				int i;
				for (i = 0; i < ct; i++){
					// print_pos();
					// printf("MSG: broadcast send to socket %d\n", nt[i].conn);
					result = sendpkt(&tmp, nt[i].conn);
					if (result == -1){
						print_pos();
						printf("ERROR: send pkt to broadcast failed\n");
						continue;
					}
				}
//				printf("MSG: send sip_pkt_t to broadcast ok\n");
			} else{	// others, send to next hop
				int destConn = getConnByNode(nNode);
				if (destConn == -1) {
					print_pos();
					printf("ERROR: conn not set up\n");
				}
				result = sendpkt(&tmp, destConn);
				if (result == -1){
					printf("ERROR: send pkt to next node failed\n");
					continue;
				}
//				printf("MSG: send sip_pkt_t to nbr ok, socket=%d\n", destConn);
			}
		}
	}
}

//这个函数停止重叠网络, 当接收到信号SIGINT时, 该函数被调用.
//它关闭所有的连接, 释放所有动态分配的内存.
void son_stop() {
	// close sip connect
	close(sip_conn);
	printf("MSG: close sip_conn ok\n");
	nt_destroy(nt);
	printf("MSG: destroy nbr table ok\n");
	exit(1);
}

int main() {
	//启动重叠网络初始化工作
	printf("Overlay network: Node %d initializing...\n",topology_getMyNodeID());	

	//创建一个邻居表
	nt = nt_create();
	//将sip_conn初始化为-1, 即还未与SIP进程连接
	sip_conn = -1;
	
	//注册一个信号句柄, 用于终止进程
	signal(SIGINT, son_stop);

	//打印所有邻居
	int nbrNum = topology_getNbrNum();
	printf("Topology_getNbrNum = %d\n", nbrNum);
	int i;
	for(i=0;i<nbrNum;i++) {
		printf("Overlay network: neighbor %d:%d\n",i+1,nt[i].nodeID);
	}

	//启动waitNbrs线程, 等待节点ID比自己大的所有邻居的进入连接
	pthread_t waitNbrs_thread;
	pthread_create(&waitNbrs_thread,NULL,waitNbrs,(void*)0);

	//等待其他节点启动
	sleep(SON_START_DELAY);
	
	//连接到节点ID比自己小的所有邻居
	connectNbrs();

	//等待waitNbrs线程返回
	pthread_join(waitNbrs_thread,NULL);	

	//此时, 所有与邻居之间的连接都建立好了
	
	//创建线程监听所有邻居
	for(i=0;i<nbrNum;i++) {
		int* idx = (int*)malloc(sizeof(int));
		*idx = i;
		pthread_t nbr_listen_thread;
		pthread_create(&nbr_listen_thread,NULL,listen_to_neighbor,(void*)idx);
	}
	printf("Overlay network: node initialized...\n");
	printf("Overlay network: waiting for connection from SIP process...\n");

	//等待来自SIP进程的连接
	waitSIP();
}
