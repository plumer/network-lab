// 文件名: common/pkt.c
// 创建日期: 2015年

#include "pkt.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
const char *begin = "!&", *end = "!#";
#define PKTSTART1 4
#define PKTSTART2 5
#define PKTRECV 6
#define PKTSTOP1 7

// get *length* bytes of data from *connfd* ,
// write to *content*, using the state machine.
int recv_state_machine(void * content, int length, int connfd) {
	if (connfd < 0) {
		print_pos();
		return -1;
	}
	int state = -1;
	char c;
	int len = 0, success = 0;
	char * fill = (char *)content;
	state = PKTSTART1;
	while ( recv(connfd, &c, 1, MSG_NOSIGNAL) > 0 ) {
		//putchar(c);
		switch (state) {
			case PKTSTART1:
				if (c == '!') state = PKTSTART2;	// start state
				break;
			case PKTSTART2:
				if (c == '&') state = PKTRECV;		// start state
				else state = PKTSTART1;
				break;
			case PKTRECV:
				if (c == '!') state = PKTSTOP1;		// about to end?
				else if (len < length) fill[len++] = c;	// recv data
				//else return -1;						// size too long
				break;
			case PKTSTOP1:
				if (c == '!') {						// the prev '!' is just data
					if (len < length) fill[len++] = c;	// recv data
					//else return -1;					// size too long
				} else if (c == '#') {				// yes!
					success = 1;
					return 1;
				} else {							// the prev '!' is just data, continue to recv
					state = PKTRECV;
					if (len+1 < length) {
						fill[len++] = '!';
						fill[len++] = c;
					}
					//else return -1;					// too long
				}
				break;
			default:
				break;
		}
	}
	if (success == 1) return 1;
	else return -1;
}

// son_sendpkt()由SIP进程调用, 其作用是要求SON进程将报文发送到重叠网络中. 
// SON进程和SIP进程通过一个本地TCP连接互连.
// 在son_sendpkt()中, 报文及其下一跳的节点ID被封装进数据结构sendpkt_arg_t, 并通过TCP连接发送给SON进程. 
// 参数son_conn是SIP进程和SON进程之间的TCP连接套接字描述符.
// 当通过SIP进程和SON进程之间的TCP连接发送数据结构sendpkt_arg_t时, 
// 使用'!&'和'!#'作为分隔符, 按照'!& sendpkt_arg_t结构 !#'的顺序发送.
// 如果发送成功, 返回1, 否则返回-1.
int son_sendpkt(int nextNodeID, sip_pkt_t* pkt, int son_conn)
{
	sendpkt_arg_t packet;
	memset(&packet, 0, sizeof(packet));
	packet.nextNodeID = nextNodeID;
	memcpy(&(packet.pkt), pkt, sizeof(packet.pkt));

	int l = sizeof(int) + sizeof(sip_hdr_t) + pkt -> header.length;
	print_pos();
	printf("actual length = %d\n", l);
	int r = send(son_conn, begin, strlen(begin), MSG_NOSIGNAL);
	if (r == -1) return -1;
	r = send(son_conn, &packet, l, MSG_NOSIGNAL);
	if (r == -1) return -1;
	r = send(son_conn, end, strlen(end), MSG_NOSIGNAL);
	if (r == -1) return -1;
	return 1;
}

// son_recvpkt()函数由SIP进程调用, 其作用是接收来自SON进程的报文. 
// 参数son_conn是SIP进程和SON进程之间TCP连接的套接字描述符. 
// 报文通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收 
// 如果成功接收报文, 返回1, 否则返回-1.
int son_recvpkt(sip_pkt_t* pkt, int son_conn)
{
	int r = recv_state_machine(pkt, sizeof(sip_pkt_t), son_conn);
	if (r < 0) return -1;
	print_pos();
	printf("length = %d, \'%s\'\n", pkt -> header.length, pkt -> data);
	return 1;
}

// 这个函数由SON进程调用, 其作用是接收数据结构sendpkt_arg_t.
// 报文和下一跳的节点ID被封装进sendpkt_arg_t结构.
// 参数sip_conn是在SIP进程和SON进程之间的TCP连接的套接字描述符. 
// sendpkt_arg_t结构通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收
// 如果成功接收sendpkt_arg_t结构, 返回1, 否则返回-1.
int getpktToSend(sip_pkt_t* pkt, int* nextNode,int sip_conn)
{
	sendpkt_arg_t packet;
	memset(&packet, 0, sizeof(packet));
	int r = recv_state_machine(&packet, sizeof(packet), sip_conn);
	if (r < 0) return -1;
	print_pos();
	printf("length = %d, \'%s\'\n", packet.pkt.header.length, packet.pkt.data);
	*nextNode = packet.nextNodeID;
	//int l = packet.pkt.header.length;
	memcpy(pkt, &(packet.pkt), sizeof(packet.pkt));
	return 1;
}

// forwardpktToSIP()函数是在SON进程接收到来自重叠网络中其邻居的报文后被调用的. 
// SON进程调用这个函数将报文转发给SIP进程. 
// 参数sip_conn是SIP进程和SON进程之间的TCP连接的套接字描述符. 
// 报文通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#, 按照'!& 报文 !#'的顺序发送. 
// 如果报文发送成功, 返回1, 否则返回-1.
int forwardpktToSIP(sip_pkt_t* pkt, int sip_conn)
{
	int r = send(sip_conn, begin, strlen(begin), MSG_NOSIGNAL);
	if (r == -1) return -1;
	
	int l = sizeof(sip_hdr_t) + pkt -> header.length;
	print_pos();
	printf("actual length = %d\n", l);
	r = send(sip_conn, pkt, l, MSG_NOSIGNAL);
	if (r == -1) return -1;

	r = send(sip_conn, end, strlen(end), MSG_NOSIGNAL);
	if (r == -1) return -1;
	return 1;
}

// sendpkt()函数由SON进程调用, 其作用是将接收自SIP进程的报文发送给下一跳.
// 参数conn是到下一跳节点的TCP连接的套接字描述符.
// 报文通过SON进程和其邻居节点之间的TCP连接发送, 使用分隔符!&和!#, 按照'!& 报文 !#'的顺序发送. 
// 如果报文发送成功, 返回1, 否则返回-1.
int sendpkt(sip_pkt_t* pkt, int conn)
{
	int r = send(conn, begin, strlen(begin), MSG_NOSIGNAL);
	if (r == -1) return -1;
	int l = sizeof(sip_hdr_t) + pkt -> header.length;
	// print_pos();
	// printf("actual length = %d\n", l);
	r = send(conn, pkt, l, MSG_NOSIGNAL);
	if (r == -1) return -1;
	r = send(conn, end, strlen(end), MSG_NOSIGNAL);
	if (r == -1) return -1;
	return 1;
}

// recvpkt()函数由SON进程调用, 其作用是接收来自重叠网络中其邻居的报文.
// 参数conn是到其邻居的TCP连接的套接字描述符.
// 报文通过SON进程和其邻居之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收 
// 如果成功接收报文, 返回1, 否则返回-1.
int recvpkt(sip_pkt_t* pkt, int conn)
{
	int r = recv_state_machine(pkt, sizeof(sip_pkt_t), conn);
	// print_pos();
	// printf("header.length = %d, \'%s\'\n", pkt -> header.length, pkt -> data);
	if (r < 0) return -1;
	else return 1;
}
