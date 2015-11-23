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
segBuf_t * seg_q;
pthread_t seghandler_thread;
pthread_t sendBuf_timer_thread[MAX_TRANSPORT_CONNECTIONS];
unsigned int sendBuf_timer_loop[MAX_TRANSPORT_CONNECTIONS];
void interrupt(int signal) {
	if (signal != SIGUSR1)
		printf("unknown signal %d", signal);
	print_pos();
	printf("interrupted\n");
}

int print_list(segBuf_t *head, segBuf_t * unsent) {
	puts("inside print_list");
	segBuf_t * q = head;
	int count = 0;
	while (q != unsent) {
		printf("[ seq = %d ]\t",q -> seg.header.seq_num);
		q = q -> next;
		count++;
		if(count >= GBN_WINDOW) break;
	}
	putchar('\n');
	return count;
}
int get_timestamp() {
	struct timespec t;
	if (clock_gettime(CLOCK_REALTIME, &t) == 0)
		return t.tv_sec % (365*86400)*1000 + t.tv_nsec / 1000000;
	else
		assert(0);
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
	memset(sendBuf_timer_loop, 0, sizeof(sendBuf_timer_loop));
	stcp_client_conn = conn;
	seg_q = NULL;
	
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
			tcb_table[i] -> buf_mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
			pthread_mutex_init(tcb_table[i] -> buf_mutex, NULL);
			tcb_table[i] -> buf_cond = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
			pthread_cond_init(tcb_table[i] -> buf_cond, NULL);
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
	p -> next_seqNum = 0;
	p -> unAck_segNum = 0;
	seg_t segment;
	memset(&segment, 0, sizeof(segment));
	stcp_hdr_t * h = &(segment.header);
	
	h -> src_port = p -> client_portNum;
	h -> dest_port = server_port;
	h -> seq_num = 0;
	h -> type = SYN;
	h -> length = 0;
	h -> checksum = 0;
//	h -> checksum = checksum(&segment);

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
		sendBuf_timer_loop[sockfd] = 1;
		pthread_create(&sendBuf_timer_thread[sockfd], NULL, sendBuf_timer, p);
		return 1;
	} else if (send_count >= SYN_MAX_RETRY) {
		p -> state = CLOSED;
		p -> server_portNum = 0;
		return -1;
	}
	assert(0);
	return 0;
}

// 发送数据给STCP服务器. 这个函数使用套接字ID找到TCB表中的条目. 
// 然后它使用提供的数据创建segBuf, 将它附加到发送缓冲区链表中. 
// 如果发送缓冲区在插入数据之前为空, 一个名为sendbuf_timer的线程就会启动. 
// 每隔SENDBUF_ROLLING_INTERVAL时间查询发送缓冲区以检查是否有超时事件发生.
// 这个函数在成功时返回1，否则返回-1. 
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int stcp_client_send(int sockfd, void* data, unsigned int length) {
	if (tcb_table[sockfd] == NULL) return -1;
	client_tcb_t * tp = tcb_table[sockfd];

	const char * dp = (const char *) data;
	int l = length;
	do {
// mutex irrelevant begin
		segBuf_t * buf_tmp = (segBuf_t *)malloc(sizeof(segBuf_t));
		if (buf_tmp == NULL) {
			print_pos();
			printf("ERROR: malloc segBuf error\n");
			return -1;
		}

		memset(buf_tmp, 0, sizeof(segBuf_t));
		stcp_hdr_t * h = &(buf_tmp -> seg.header);
		h -> src_port = tp -> client_portNum;
		h -> dest_port = tp -> server_portNum;
		h -> seq_num = tp -> next_seqNum;
		h -> type = DATA;

		if (l > MAX_SEG_LEN) {
			memcpy(buf_tmp->seg.data, dp, MAX_SEG_LEN);
			l -= MAX_SEG_LEN;
			dp += MAX_SEG_LEN;
			h -> length = MAX_SEG_LEN;
			tp -> next_seqNum += MAX_SEG_LEN;
		} else {
			memcpy( buf_tmp -> seg.data, dp, l);
			h -> length = l;
			tp -> next_seqNum += l;
			dp = NULL;
			l = 0;
		}
		buf_tmp -> next = NULL;

// mutex irrelevant end
	
		pthread_mutex_lock(tp -> buf_mutex);
		if (tp -> sendBufHead == NULL) {
			tp -> sendBufHead = buf_tmp;
			tp -> sendBufTail = buf_tmp;
			tp -> sendBufunSent = buf_tmp;
		} else {
			assert(tp -> sendBufTail);
			tp -> sendBufTail -> next = buf_tmp;
			tp -> sendBufTail = buf_tmp;
		}
		pthread_mutex_unlock(tp -> buf_mutex);
	} while (l > 0);
//	puts("leaving send");
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
	print_pos();
	if (tcb_table[sockfd] == NULL)
		return -1;
	assert(tcb_table[sockfd] -> state == CONNECTED);
	client_tcb_t *p = tcb_table[sockfd];
	seg_t fin_seg;
	memset(&fin_seg, 0, sizeof(fin_seg));
	stcp_hdr_t * h = &(fin_seg.header);
	h -> src_port = p -> client_portNum;
	h -> dest_port = p -> server_portNum;
	h -> seq_num = p -> next_seqNum;
	h -> type = FIN;
	h -> length = 0;
	h -> checksum = 0;
	h -> checksum = checksum(&fin_seg);

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
	
	sendBuf_timer_loop[sockfd] = 0;	// tell the thread to end
	pthread_mutex_destroy(p -> buf_mutex);
	p -> sendBufHead = p -> sendBufunSent = p -> sendBufTail = NULL;
	
	print_pos();
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
	free(tcb_table[sockfd] -> buf_mutex);
	free(tcb_table[sockfd] -> buf_cond);
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
		if (r == -1)
			assert(0);
		if (r == 1) {
			printf("subjectively lost\n");
			continue;
		}
		if (r == 2) {
			printf("ERROR: recv\n");
			pthread_exit(0);
		}
		stcp_hdr_t * h = &(seg.header);

//		printf("received a pack to port %d\n", h -> dest_port);
		if (h -> type == SYNACK) {
			// find the tcb entry with corresponding h -> dest_port
//			printf("received a SYNACK segment to port %d\n", h -> dest_port);
			int i = 0;
			for (; i < MAX_TRANSPORT_CONNECTIONS; ++i) if (tcb_table[i]) {
				if (tcb_table[i] -> client_portNum == h -> dest_port) {
					if (tcb_table[i] -> state == SYNSENT) {
						tcb_table[i] -> state = CONNECTED;
						tcb_table[i] ->  next_seqNum = h -> seq_num;
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
			// printf("received a FINACK segment to port %d\n", h -> dest_port);
			int i = 0;
			for (; i < MAX_TRANSPORT_CONNECTIONS; ++i) if (tcb_table[i]) {
				if (tcb_table[i] -> client_portNum == h -> dest_port) {
					if (tcb_table[i] -> state == FINWAIT) {
						tcb_table[i] -> state = CLOSED;
						tcb_table[i] -> next_seqNum = h -> seq_num;
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
		} else if (h -> type == DATAACK) {
			// find the tcb entry with corresponding h -> dest_port
			// printf("received a data ack to port %d, ack_seq %d, expect = %d\n",
				//	h -> dest_port, h -> ack_num, h -> seq_num);
			int i = 0;
			client_tcb_t * correct_tcb = NULL;
			for (; i < MAX_TRANSPORT_CONNECTIONS; ++i) if (tcb_table[i]) {
				if ( tcb_table[i] -> client_portNum == h -> dest_port ) {
					correct_tcb = tcb_table[i];
					break;
				}
			}
			assert(correct_tcb);
// v start critical region 	
			pthread_mutex_lock(correct_tcb -> buf_mutex);
			if (correct_tcb -> sendBufHead != NULL){
				if (h -> seq_num <= h -> ack_num) {
					printf("I should retransmit\n");
					assert(h -> seq_num >= correct_tcb -> sendBufHead -> seg.header.seq_num);
					segBuf_t * p = correct_tcb -> sendBufHead;
					while (p -> seg.header.seq_num < h -> seq_num) {
						correct_tcb -> sendBufHead = correct_tcb -> sendBufHead -> next;
						free(p);
						p = correct_tcb -> sendBufHead;
						assert(correct_tcb -> unAck_segNum > 0);
						correct_tcb -> unAck_segNum --;

						if (p == NULL) {
							correct_tcb -> sendBufTail = NULL;
							break;
						}
					}
				} else if (correct_tcb -> sendBufHead -> seg.header.seq_num == h -> ack_num) {
					print_pos();
					printf("good ACK, ack_num = %d, port = %d\n", h -> ack_num, correct_tcb -> client_portNum);
					segBuf_t * p = correct_tcb -> sendBufHead;
					correct_tcb -> sendBufHead = correct_tcb -> sendBufHead -> next;
					if (correct_tcb -> sendBufHead == NULL) {
						correct_tcb -> sendBufunSent = NULL;
						correct_tcb -> sendBufTail = NULL;
					}
					// print_pos();
					// printf("remove from list seq %d\n", p -> seg.header.seq_num);
					free(p);
					// print_pos();
					// printf("unAck_segNum = %d\n", correct_tcb -> unAck_segNum);
					assert(correct_tcb -> unAck_segNum > 0);
					correct_tcb -> unAck_segNum --;
				} else if (correct_tcb -> sendBufHead -> seg.header.seq_num < h -> ack_num) {
					print_pos();
					printf("reasonable ACK, ack_num = %d\n", h -> ack_num);
					segBuf_t * p = correct_tcb -> sendBufHead;
					while (p -> seg.header.seq_num <= h -> ack_num) {
						
						correct_tcb -> sendBufHead = correct_tcb -> sendBufHead -> next;
						// print_pos();
						// printf("remove from list seq %d\n", p -> seg.header.seq_num);
						free(p);
						p = correct_tcb -> sendBufHead;
						// printf("unAck_segNum = %d\n", correct_tcb -> unAck_segNum);
						assert(correct_tcb -> unAck_segNum > 0);
						correct_tcb -> unAck_segNum --;

						if (p == NULL) {
							assert(correct_tcb -> unAck_segNum == 0);
							assert(correct_tcb -> sendBufunSent == NULL);
							correct_tcb -> sendBufTail = NULL;
							break;
						}
					}
				} else {
					print_pos();
					printf("Bad Ack, bufhead -> seg_num %d > recv -> seg_num %d",
							correct_tcb -> sendBufHead -> seg.header.seq_num, h -> ack_num);
				}
			}
			else {
				printf("MSG: strange error sendBufHead == NULL\n");
			}
			pthread_mutex_unlock(correct_tcb -> buf_mutex);
// ^ end critical region
		} else {
			printf("unknown type of segment, type %d", h -> type);
		}
	}
}


// 这个线程持续轮询发送缓冲区以触发超时事件. 如果发送缓冲区非空, 它应一直运行.
// 如果(当前时间 - 第一个已发送但未被确认段的发送时间) > DATA_TIMEOUT, 就发生一次超时事件.
// 当超时事件发生时, 重新发送所有已发送但未被确认段. 当发送缓冲区为空时, 这个线程将终止.
// while (true) {
//   检查是否能继续发送，即检查unsent是否到末尾，或者unack是否太大
//     如果能
//       发一个
//
//   检查是否需要重传
//     如果需要
//       重传
//     如果不需要
//       睡眠
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
void* sendBuf_timer(void* clienttcb)
{
	client_tcb_t * tcbp = (client_tcb_t*)clienttcb;
	int sfd = 0;
	for (; sfd < MAX_TRANSPORT_CONNECTIONS; ++sfd) {
		if (tcb_table[sfd] == tcbp) {
			break;
		}
	}
	if (sfd == 0) assert(tcb_table[sfd] == tcbp);
	assert(sfd < MAX_TRANSPORT_CONNECTIONS);
	while (sendBuf_timer_loop[sfd]) {
		assert(tcbp);
		
		pthread_mutex_lock(tcbp -> buf_mutex);
		if (tcbp -> sendBufHead == NULL) {	// nothing in buf
			pthread_mutex_unlock(tcbp -> buf_mutex);
			usleep(100000);
		} else {
			segBuf_t * p = tcbp -> sendBufHead;
			int timelag = get_timestamp() - p -> sentTime;
			if (timelag >= SENDBUF_POLLING_INTERVAL / 1000000) {
				while (p != tcbp -> sendBufunSent) {
					sip_sendseg(stcp_client_conn, &(p -> seg) );
					p -> sentTime = get_timestamp();
					p = p -> next;
				}
			}
			assert(tcbp -> unAck_segNum >= 0 && tcbp -> unAck_segNum <= 10);
			while (tcbp -> unAck_segNum < GBN_WINDOW) {
				if (tcbp -> sendBufunSent) {
					sip_sendseg(stcp_client_conn, &(tcbp -> sendBufunSent -> seg) );
					tcbp -> sendBufunSent -> sentTime = get_timestamp();
					tcbp -> unAck_segNum++;
					tcbp -> sendBufunSent = tcbp -> sendBufunSent -> next;
				} else {
					break;
				}
			}
			timelag = get_timestamp() - tcbp -> sendBufHead -> sentTime;
			assert(timelag >= 0 && timelag <= 100);
			pthread_mutex_unlock(tcbp -> buf_mutex);
			usleep(100000 - timelag * 1000);
		}	
	}
	if (tcbp -> unAck_segNum > 0) {
		print_pos();
		printf("port %d: exiting before all packages are successfully sent with ACK T_T\n", tcbp -> client_portNum);
		print_list(tcbp -> sendBufHead, tcbp -> sendBufunSent);
		print_list(tcbp -> sendBufunSent, NULL);
	} else if (tcbp -> unAck_segNum < 0) {
		print_pos();
		printf("port %d: minus\n", tcbp -> client_portNum);
	} else if (tcbp -> sendBufHead) {
		print_pos();
		printf("port %d: still segs not sent\n",  tcbp -> client_portNum);
		print_list(tcbp -> sendBufHead, tcbp -> sendBufunSent);
	}
	pthread_exit(0);
}

