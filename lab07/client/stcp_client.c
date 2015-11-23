#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include "stcp_client.h"


int stcp_client_conn;
pthread_t seghandler_thread;
void interrupt(int signal) {
	if (signal != SIGUSR1)
		printf("unknown signal %d", signal);
	print_pos();
	printf("interrupted\n");
}



/*面向应用层的接口*/

//
//	我们在下面提供了每个函数调用的原型定义和细节说明, 
//	但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//	注意: 当实现这些函数时, 你需要考虑FSM中所有可能的状态, 这可以使用switch语句来实现.
//
//	目标: 你的任务就是设计并实现下面的函数原型.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// stcp客户端初始化
//
// 这个函数初始化TCB表, 将所有条目标记为NULL.	
// 它还针对重叠网络TCP套接字描述符conn初始化一个STCP层的全局变量, 
// 该变量作为sip_sendseg和sip_recvseg的输入参数.
// 最后, 这个函数启动seghandler线程来处理进入的STCP段. 客户端只有一个seghandler.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void stcp_client_init(int conn) {
	// initialize tcb table
	memset(tcb_table, 0, sizeof(tcb_table));
	stcp_client_conn = conn;
	
	pthread_create(&seghandler_thread, NULL, seghandler, NULL);
	return;
}

// 创建一个客户端TCB条目, 返回套接字描述符
//
// 这个函数查找客户端TCB表以找到第一个NULL条目, 
// 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化. 
// 例如, TCB state被设置为CLOSED，客户端端口被设置为函数调用参数client_port. 
// TCB表中条目的索引号应作为客户端的新套接字ID被这个函数返回, 它用于标识客户端的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_sock(unsigned int client_port) {
	int i;
	for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; ++i) {
		if (!tcb_table[i]) {
			tcb_table[i] = (client_tcb_t *)malloc(sizeof(client_tcb_t));
			memset(tcb_table[i], 0, sizeof(client_tcb_t));
			tcb_table[i] -> client_portNum = client_port;
			tcb_table[i] -> state = CLOSED;
			return i;
		}
	}
	assert(i == MAX_TRANSPORT_CONNECTIONS);
	return -1;
}

// 连接STCP服务器
//
// 这个函数用于连接服务器. 
// 它以套接字ID和服务器的端口号作为输入参数. 套接字ID用于找到TCB条目.	
// 这个函数设置TCB的服务器端口号,然后使用sip_sendseg()发送一个SYN段给服务器.	
// 在发送了SYN段之后, 一个定时器被启动. 
// 如果在SYNSEG_TIMEOUT时间之内没有收到SYNACK, SYN 段将被重传. 
// 如果收到了, 就返回1. 
// 否则, 如果重传SYN的次数大于SYN_MAX_RETRY, 就将state转换到CLOSED, 并返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_connect(int sockfd, unsigned int server_port) {
	if (tcb_table[sockfd] == NULL)
		return -1;
	client_tcb_t * p = tcb_table[sockfd];
	p -> server_portNum = server_port;
	seg_t segment;
	memset(&segment, 0, sizeof(segment));
	stcp_hdr_t * h = &(segment.header);
	
	h -> src_port = p -> client_portNum;
	h -> dest_port = server_port;
	h -> type = SYN;
	h -> length = 0;

	int send_count = 0;
	p -> state = SYNSENT;
	do {
		int send_res = sip_sendseg(stcp_client_conn, &segment);
		assert(send_res == 1);
		
		struct timespec timeout;
		timeout.tv_nsec = SYN_TIMEOUT;
		signal(SIGUSR1, interrupt);	
		usleep(SYN_TIMEOUT/1000);
		if (p -> state != CONNECTED) {
			print_pos();
			printf("connecting: wait one more time\n");
			send_count++;
		}
	} while (p -> state != CONNECTED && send_count < SYN_MAX_RETRY);	
	if (p -> state == CONNECTED) {
		return 1;
	} else if (send_count >= SYN_MAX_RETRY) {
		p -> state = CLOSED;
		p -> server_portNum = 0;
		return -1;
	}
	assert(0);
	return 0;
}

// 发送数据给STCP服务器
//
// 这个函数发送数据给STCP服务器. 你不需要在本实验中实现它。
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_send(int sockfd, void* data, unsigned int length) {
	return 1;
}

// 断开到STCP服务器的连接
//
// 这个函数用于断开到服务器的连接. 它以套接字ID作为输入参数. 套接字ID用于找到TCB表中的条目.	
// 这个函数发送FIN segment给服务器. 在发送FIN之后, state将转换到FINWAIT, 并启动一个定时器.
// 如果在最终超时之前state转换到CLOSED, 则表明FINACK已被成功接收. 否则, 如果在经过FIN_MAX_RETRY次尝试之后,
// state仍然为FINWAIT, state将转换到CLOSED, 并返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_disconnect(int sockfd) {
	if (tcb_table[sockfd] == NULL)
		return -1;
	assert(tcb_table[sockfd] -> state == CONNECTED);
	client_tcb_t *p = tcb_table[sockfd];
	seg_t fin_seg;
	memset(&fin_seg, 0, sizeof(fin_seg));
	stcp_hdr_t * h = &(fin_seg.header);
	h -> src_port = p -> client_portNum;
	h -> dest_port = p -> server_portNum;
	h -> type = FIN;
	h -> length = 0;
	
	int send_count = 0;
	p -> state = FINWAIT;
	do {
		int send_res = sip_sendseg(stcp_client_conn, &fin_seg);
		if (send_res != 1) {
			print_pos();
			printf("panic, FIN seg send fail\n");
			return -1;
		}
		
		signal(SIGUSR1, interrupt);
//		printf("%s: sleeping\n", __FUNCTION__);
		usleep(FIN_TIMEOUT / 1000);
		if (p -> state != CLOSED) {
			send_count++;
			puts("waiting for next time");
		}
	} while (p -> state != CLOSED && send_count < FIN_MAX_RETRY);
	
	if (p -> state == CLOSED) {
		p -> server_portNum = 0;
		return 1;
	} else if (send_count >= FIN_MAX_RETRY) {
		p -> state = CLOSED;
		p -> server_portNum = 0;
		return -1;
	}
	assert(0);
	return 0;
}

// 关闭STCP客户
//
// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_close(int sockfd) {
	if (tcb_table[sockfd] == NULL)
		return -1;
	int curr_state = tcb_table[sockfd] -> state;
	free(tcb_table[sockfd]);
	tcb_table[sockfd] = NULL;
	return (curr_state == CLOSED) ? 1 : -1;
}

// 处理进入段的线程
//
// 这是由stcp_client_init()启动的线程. 它处理所有来自服务器的进入段. 
// seghandler被设计为一个调用sip_recvseg()的无穷循环. 如果sip_recvseg()失败, 则说明重叠网络连接已关闭,
// 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作. 请查看客户端FSM以了解更多细节.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void *seghandler(void* arg) {
	seg_t seg;
	while (1) {
		memset(&seg, 0, sizeof(seg));
		int r = sip_recvseg(stcp_client_conn, &seg);
		printf("received a packet\n");
		if (r == -1)
			assert(0);
		if (r == 1) {
			printf("subjectively lost\n");
			continue;
		}
		assert(r == 0);
		stcp_hdr_t * h = &(seg.header);
		if (h -> type == SYNACK) {
			// find the tcb entry with corresponding h -> dest_port
			printf("received a SYNACK segment to port %d\n", h -> dest_port);
			int i = 0;
			for (; i < MAX_TRANSPORT_CONNECTIONS; ++i) if (tcb_table[i]) {
				if (tcb_table[i] -> client_portNum == h -> dest_port) {
					if (tcb_table[i] -> state == SYNSENT) {
						tcb_table[i] -> state = CONNECTED;
						raise(SIGUSR1);
						break;
					} else if (tcb_table[i] -> state == CONNECTED) {
						printf("repeated SYNACK\n");
						break;
					} else {
						printf("panic: #%d tcb (port %d) is not SYNSENT\n",
								i, tcb_table[i] -> client_portNum);
						assert(0);
					}
				}
			}
			if (i == MAX_TRANSPORT_CONNECTIONS)
				printf("SYNACK to unknown client port %d\n",
						h -> dest_port);
		} else if (h -> type == FINACK) {
			// find the tcb entry with corresponding h -> dest_port
			printf("received a FINACK segment to port %d\n", h -> dest_port);
			int i = 0;
			for (; i < MAX_TRANSPORT_CONNECTIONS; ++i) if (tcb_table[i]) {
				if (tcb_table[i] -> client_portNum == h -> dest_port) {
					if (tcb_table[i] -> state == FINWAIT) {
						tcb_table[i] -> state = CLOSED;
						raise(SIGUSR1);
						break;
					} else if (tcb_table[i] -> state == CLOSED) {
						printf("repeated FINACK\n");
						break;
					} else {
						printf("panic: #%d tcb (port %d) is not FINWAIT\n",
								i, tcb_table[i] -> client_portNum);
						assert(0);
					}
				}
			}
			if (i == MAX_TRANSPORT_CONNECTIONS)
				printf("FINACK to unknown client port %d\n",
						h -> dest_port);
		}
	}
}



