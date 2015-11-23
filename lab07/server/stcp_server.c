#include <stdlib.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include "stcp_server.h"
#include <assert.h>
#include "../common/constants.h"

#define BUF_LEN 4096

server_tcb_t *tcb[MAX_TRANSPORT_CONNECTIONS];
pthread_t trd[MAX_TRANSPORT_CONNECTIONS];
int conn_ct;

int sip_connfd;
pthread_t handle_t;
pthread_t checker_t;
pthread_attr_t attr;


/*面向应用层的接口*/

//
//
//  我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//  注意: 当实现这些函数时, 你需要考虑FSM中所有可能的状态, 这可以使用switch语句来实现.
//
//  目标: 你的任务就是设计并实现下面的函数原型.
//

// stcp服务器初始化
//
// 这个函数初始化TCB表, 将所有条目标记为NULL. 它还针对重叠网络TCP套接字描述符conn初始化一个STCP层的全局变量,
// 该变量作为sip_sendseg和sip_recvseg的输入参数. 最后, 这个函数启动seghandler线程来处理进入的STCP段.
// 服务器只有一个seghandler.
//

void stcp_server_init(int conn) {
	conn_ct = 0;	//连接计数清零

	int i = 0;	//TCB初始化
	for (; i < MAX_TRANSPORT_CONNECTIONS; i ++) tcb[i] = NULL;

	sip_connfd = conn;	//sip层socket

	// sip层handler线程启动
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	int res = pthread_create(&handle_t, &attr, seghandler, NULL);
	if (res) {
		printf("THREAD ERROR: create thread 4 seghandler failed\n");
		exit(-1);
	}
	printf("\tGOOD MSG: create thread seghandler ok\n");

	//计时线程启动，确保closewait
	res = pthread_create(&checker_t, &attr, checker, NULL);
	if (res){
		printf("THREAD ERROR: create thread 4 checker failed\n");
	}
	printf("\tGOOD MSG: create thread checker ok\n");
}

// 创建服务器套接字
//
// 这个函数查找服务器TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化, 例如, TCB state被设置为CLOSED, 服务器端口被设置为函数调用参数server_port. 
// TCB表中条目的索引应作为服务器的新套接字ID被这个函数返回, 它用于标识服务器的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.

int stcp_server_sock(unsigned int server_port) {
	// stcp服务器socket达到上限
	if (conn_ct == MAX_TRANSPORT_CONNECTIONS) return -1;
	// 建立新的socket，返回服务器套接字【ID:0-9】
	else {
		int cur = 0;
		for (; cur < MAX_TRANSPORT_CONNECTIONS; cur++){
			if (tcb[cur] == NULL) break;
		}
		tcb[cur] = (server_tcb_t *)malloc(sizeof(server_tcb_t));
		// 注意，这里没有用到的都初始化为-1
		tcb[cur]->server_nodeID = -1;
		tcb[cur]->server_portNum = server_port;
		tcb[cur]->client_nodeID = -1;
		tcb[cur]->client_portNum = -1;
		tcb[cur]->state = CLOSED;
		tcb[cur]->wait_start = 0;
		tcb[cur]->expect_seqNum = -1;
		tcb[cur]->recvBuf = (char *)malloc(BUF_LEN*sizeof(char));
		tcb[cur]->usedBufLen = 0;
		tcb[cur]->bufMutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init(tcb[cur]->bufMutex, NULL);
		conn_ct ++;
		printf("\tGOOD MSG: build socket for port:%d\n\t\tSTATISTICS MSG: server has %d sockets now\n", server_port, conn_ct);
		return cur;
	}
}

// 接受来自STCP客户端的连接
//
// 这个函数使用sockfd获得TCB指针, 并将连接的state转换为LISTENING. 它然后进入忙等待(busy wait)直到TCB状态转换为CONNECTED 
// (当收到SYN时, seghandler会进行状态的转换). 该函数在一个无穷循环中等待TCB的state转换为CONNECTED,  
// 当发生了转换时, 该函数返回1. 你可以使用不同的方法来实现这种阻塞等待.
//
void state_shift(int sig){
	if (sig == SIGUSR1)
		printf("listening -> connected\n");
}

int stcp_server_accept(int sockfd) {
	if (tcb[sockfd] == NULL) {
		printf("TCB ERROR: when accept tcb %d is null\n", sockfd);
		exit (1);
		return 0;
	}
	else {
		tcb[sockfd]->state = LISTENING;
		while (tcb[sockfd]->state != CONNECTED) {
			signal(SIGUSR1, state_shift);
			usleep(100000);
		}
		printf("\tGOOD MSG: socket %d is connected\n", sockfd);
		return 1;
	}
}

// 接收来自STCP客户端的数据
//
// 这个函数接收来自STCP客户端的数据. 你不需要在本实验中实现它.
//
int stcp_server_recv(int sockfd, void* buf, unsigned int length) {
	return 1;
}

// 关闭STCP服务器
//
// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
//

int stcp_server_close(int sockfd) {
	if (tcb[sockfd] == NULL) {
		printf("TCB ERROR: when close tcb %d is null\n", sockfd);
		return -1;
	}
	else {
		if (tcb[sockfd]->state == CLOSED){
			printf("\tGOOD MSG: when close, tcb state is CLOSED\n");
		}
		else if (tcb[sockfd]->state == CLOSEWAIT){
			printf("\tGOOD MSG: when close, tcb state is CLOSEWAIT, wait timeout\n");
			while (tcb[sockfd]->state != CLOSED){
				usleep(100000);
			}

			printf("\tGOOD MSG: when close, tcb state CLOSEWAIT->CLOSED ok\n");
		}
		else {
			printf("CLOSE WARNING: when close, tcb state is error\n");
		}
		pthread_mutex_lock(tcb[sockfd]->bufMutex);
		free(tcb[sockfd]->recvBuf);
		pthread_mutex_unlock(tcb[sockfd]->bufMutex);

		pthread_mutex_destroy(tcb[sockfd]->bufMutex);
		free(tcb[sockfd]->bufMutex);

		free(tcb[sockfd]);
		tcb[sockfd] = NULL;

		conn_ct --;
		printf("\tGOOD MSG: close socket %d\n", sockfd);
		return 1;
	}
}

// 处理进入段的线程
//
// 这是由stcp_server_init()启动的线程. 它处理所有来自客户端的进入数据. seghandler被设计为一个调用sip_recvseg()的无穷循环, 
// 如果sip_recvseg()失败, 则说明重叠网络连接已关闭, 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作.
// 请查看服务端FSM以了解更多细节.
//

void *seghandler(void* arg) {
	seg_t tmp;
	while (1){
		int k = sip_recvseg(sip_connfd, &tmp);
		if( k == 1) {
//			usleep(100000);
			printf("\t\t\tWARNING: I lost a package\n");
		}
		else if (k == 2){
			usleep(100000);
//			printf("unknown error\n");
		}
		else {
			switch(tmp.header.type){
				case SYN: printf("\t\t\t\tRECV SYN, client:%d server:%d\n", tmp.header.src_port, tmp.header.dest_port); break;
				case FIN: printf("\t\t\t\tRECV FIN, client:%d server:%d\n", tmp.header.src_port, tmp.header.dest_port); break;
				case DATA: printf("\t\t\t\tRECV DATA, client:%d server:%d\n", tmp.header.src_port, tmp.header.dest_port); break;
				default: break;
			}

			int i = 0, k = -1;
			for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
				if (tcb[i] != NULL) {
			//		printf("search socket:%d\n tcb: No.%d, state:%d, client:%d, server:%d\n", i, i, tcb[i]->state, tcb[i]->client_portNum, tcb[i]->server_portNum);
			//		printf("\t tmp: type:%d, src:%d, dest:%d\n", tmp.header.type, tmp.header.src_port, tmp.header.dest_port);
					if (tcb[i]->server_portNum == tmp.header.dest_port) {
						// 3种正常情况
						if (tmp.header.type == SYN && tcb[i]->state == LISTENING){
							printf("\tGOOD MSG: package right case 1: recv SYN\n");
							k = i;
							break;
						}
						if (tmp.header.type == DATA && tcb[i]->state == CONNECTED && tmp.header.src_port == tcb[i]->client_portNum){
							printf("\tGOOD MSG: package right case 2: recv DATA\n");
							k = i;
							break;
						}
						if (tmp.header.type == FIN && tcb[i]->state == CONNECTED && tmp.header.src_port == tcb[i]->client_portNum){
							printf("\tGOOD MSG: package right case 3: recv FIN\n");
							k = i;
							break;
						}
						// 错误情况2种
						if (tmp.header.type == SYN && tcb[i]->state == CONNECTED && tcb[i]->client_portNum == tmp.header.src_port){
							printf("PACKAGE ERROR: error case 1: recv SYN when CONNECTED\n");
							printf("\tERROR MSG: socket:%d client:%d server:%d\n", i, tcb[i]->client_portNum, tcb[i]->server_portNum);
							k = i;
							break;
						}
						if (tmp.header.type == FIN && tcb[i]->state == CLOSEWAIT && tcb[i]->client_portNum == tmp.header.src_port){
							printf("PACKAGE ERROR: error case 2: recv FIN when CLOSEWAIT\n");
							printf("\tERROR MSG: socket:%d client:%d server:%d\n", i, tcb[i]->client_portNum, tcb[i]->server_portNum);
							k = i;
							break;
						}
					}
				}
				else continue;
			}
			if (k == -1) {
				printf("PACKAGE ERROR: no such connect 4 package\n");
				printf("\tERROR MSG: package client:%d server:%d\n", tmp.header.src_port, tmp.header.dest_port);
			}
			else {
				switch(tcb[k]->state){
					case CLOSED: break;
					case LISTENING: 
						     if (tmp.header.type == SYN) {
							     tcb[k]->state = CONNECTED;
							     tcb[k]->client_portNum = tmp.header.src_port;

							     seg_t ack;
							     ack.header.src_port = tmp.header.dest_port;
							     ack.header.dest_port = tmp.header.src_port;
							     ack.header.seq_num = tmp.header.seq_num;
							     ack.header.ack_num = tmp.header.ack_num;
							     ack.header.length = 0;
							     ack.header.type = SYNACK;
							     ack.header.rcv_win = 0;
							     ack.header.checksum = 0;
							     memset(ack.data, 0, MAX_SEG_LEN);

							     printf("\tGOOD MSG: send SYNACK 4 client:%d -> server:%d\n", ack.header.dest_port, ack.header.src_port);
							     if (sip_sendseg(sip_connfd, &ack)) printf("\tGOOD MSG: send SYNACK ok\n");
							     else printf("SEND ERROR: send SYNACK failed\n");
						     }
						     break;
					case CONNECTED:
						     if (tmp.header.type == FIN){
							     tcb[k]->state = CLOSEWAIT;
							     tcb[k]->wait_start = clock();
							     seg_t ack;
							     ack.header.src_port = tmp.header.dest_port;
							     ack.header.dest_port = tmp.header.src_port;
							     ack.header.seq_num = tmp.header.seq_num;
							     ack.header.ack_num = tmp.header.ack_num;
							     ack.header.length = 0;
							     ack.header.type = FINACK;
							     ack.header.rcv_win = 0;
							     ack.header.checksum = 0;
							     memset(ack.data, 0, MAX_SEG_LEN);

							     printf("\tGOOD MSG: send FINACK 4 client:%d -> server:%d\n", ack.header.dest_port, ack.header.src_port);
							     if (sip_sendseg(sip_connfd, &ack)) printf("\tGOOD MSG: send FINACK ok\n");
							     else printf("SEND ERROR: send FINACK failed\n");
						     }
						     else if (tmp.header.type == DATA){
//							     copy data
//							     sip_send();
						     }
						     else if (tmp.header.type == SYN){
							     seg_t ack;
							     ack.header.src_port = tmp.header.dest_port;
							     ack.header.dest_port = tmp.header.src_port;
							     ack.header.seq_num = tmp.header.seq_num;
							     ack.header.ack_num = tmp.header.ack_num;
							     ack.header.length = 0;
							     ack.header.type = SYNACK;
							     ack.header.rcv_win = 0;
							     ack.header.checksum = 0;
							     memset(ack.data, 0, MAX_SEG_LEN);

							     printf("SEND ERROR: SYNACK lost\n"); 
							     printf("\tGOOD MSG: send SYNACK 4 client:%d -> server:%d\n", ack.header.dest_port, ack.header.src_port);
							     if (sip_sendseg(sip_connfd, &ack)) printf("\tGOOD MSG: send SYNACK ok\n");
							     else printf("SEND ERROR: send SYNACK failed\n");
						     }
						     break;
					case CLOSEWAIT: 
						     if (tmp.header.type == FIN){
							     tcb[k]->wait_start = clock();
							     seg_t ack;
							     ack.header.src_port = tmp.header.dest_port;
							     ack.header.dest_port = tmp.header.src_port;
							     ack.header.seq_num = tmp.header.seq_num;
							     ack.header.ack_num = tmp.header.ack_num;
							     ack.header.length = 0;
							     ack.header.type = FINACK;
							     ack.header.rcv_win = 0;
							     ack.header.checksum = 0;
							     memset(ack.data, 0, MAX_SEG_LEN);

							     printf("SEND ERROR: FINACK lost\n"); 
							     printf("\tGOOD MSG: send FINACK 4 client:%d -> server:%d\n", ack.header.dest_port, ack.header.src_port);
							     if (sip_sendseg(sip_connfd, &ack)) printf("\tGOOD MSG: send FINACK ok\n");
							     else printf("SEND ERROR: send FINACK failed\n");
						     }
						     break;
					default: break;
				}
			}
		}
	}
  return 0;
}
void *checker(void *arg){
	while (1){
		int i = 0;
		clock_t cur_time = clock();
		for (; i < MAX_TRANSPORT_CONNECTIONS; i++){
			if (tcb[i] == NULL) continue;
			else if (tcb[i]->state == CLOSEWAIT){
				if ((cur_time - tcb[i]->wait_start) > (CLOSEWAIT_TIMEOUT * CLOCKS_PER_SEC/1000)) {
					tcb[i]->state = CLOSED;
				}
			}
		}
		usleep(200000);
	}
	return NULL;
}
