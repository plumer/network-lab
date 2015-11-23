//
// 文件名: seg.c

// 描述: 这个文件包含用于发送和接收STCP段的接口sip_sendseg() and sip_rcvseg(), 及其支持函数的实现. 
//
// 创建日期: 2015年
//

#include "seg.h"
#include "stdio.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

//
//
//	用于客户端和服务器的SIP API 
//	=======================================
//
//	我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//	注意: sip_sendseg()和sip_recvseg()是由网络层提供的服务, 即SIP提供给STCP.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

// 通过重叠网络(在本实验中，是一个TCP连接)发送STCP段. 因为TCP以字节流形式发送数据, 
// 为了通过重叠网络TCP连接发送STCP段, 你需要在传输STCP段时，在它的开头和结尾加上分隔符. 
// 即首先发送表明一个段开始的特殊字符"!&"; 然后发送seg_t; 最后发送表明一个段结束的特殊字符"!#".	
// 成功时返回1, 失败时返回-1. sip_sendseg()首先使用send()发送两个字符, 然后使用send()发送seg_t,
// 最后使用send()发送表明段结束的两个字符.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int sip_sendseg(int connection, seg_t* segPtr)
{
	const char * begin = "!&";
	const char * end = "!#";
	int r = send(connection, begin, strlen(begin), 0);
	if (r < 0) return -1;
	r = send(connection, segPtr, sizeof(seg_t), 0);
	if (r < 0) return -1;
	r = send(connection, end, strlen(end), 0);
	if (r < 0) return -1;
	return 1;

}

// 通过重叠网络(在本实验中，是一个TCP连接)接收STCP段. 我们建议你使用recv()一次接收一个字节.
// 你需要查找"!&", 然后是seg_t, 最后是"!#". 这实际上需要你实现一个搜索的FSM, 可以考虑使用如下所示的FSM.
// SEGSTART1 -- 起点 
// SEGSTART2 -- 接收到'!', 期待'&' 
// SEGRECV -- 接收到'&', 开始接收数据
// SEGSTOP1 -- 接收到'!', 期待'#'以结束数据的接收
// 这里的假设是"!&"和"!#"不会出现在段的数据部分(虽然相当受限, 但实现会简单很多).
// 你应该以字符的方式一次读取一个字节, 将数据部分拷贝到缓冲区中返回给调用者.
//
// 注意: 还有一种处理方式可以允许"!&"和"!#"出现在段首部或段的数据部分. 具体处理方式是首先确保读取到!&，然后
// 直接读取定长的STCP段首部, 不考虑其中的特殊字符, 然后按照首部中的长度读取段数据, 最后确保以!#结尾.
//
// 注意: 在你剖析了一个STCP段之后,	你需要调用seglost()来模拟网络中数据包的丢失. 
// 在sip_recvseg()的下面是seglost()的代码.
// 
// 如果段丢失了, 就返回1, 否则返回0. 如果连接没有建立or其他错误，返回2
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// 
#define SEGSTART1 0
#define SEGSTART2 1
#define SEGHDR	2
#define SEGDATA 3
#define SEGSTOP1 4
#define SEGSTOP2 5

int sip_recvseg(int connection, seg_t* segPtr) {
	char buf[2];
	memset(buf, 0, 2);
	int curState = SEGSTART1;

	char *hdr = (char *)malloc(sizeof(stcp_hdr_t)+1);
	char *dat = (char *)malloc(MAX_SEG_LEN*sizeof(char)+1);

	memset(hdr, 0, sizeof(stcp_hdr_t)+1);
	memset(dat, 0, MAX_SEG_LEN+1);
	int hdr_ct = 0;
	int data_ct = 0;
	int data_len = 0;

//	struct sockaddr a;
//	socket_t len;
//	getpeername(connection, &a, &len);

/*struct sockaddr_in serv, guest;
char serv_ip[20];
char guest_ip[20];
int serv_len = sizeof(serv);
int guest_len = sizeof(guest);
getsockname(connection, (struct sockaddr *)&serv, &serv_len);
getpeername(connection, (struct sockaddr *)&guest, &guest_len);
inet_ntop(AF_INET, &serv.sin_addr, serv_ip, sizeof(serv_ip));
inet_ntop(AF_INET, &guest.sin_addr, guest_ip, sizeof(guest_ip));
printf("host %s:%d guest %s:%d\n", serv_ip, ntohs(serv.sin_port), guest_ip, ntohs(guest.sin_port));
*/

	int n;
	while ((n = recv(connection, buf, 1, 0)) > 0){
//		printf("sig recv %d byte\n", n);
		switch(curState){
			case SEGSTART1: 
				if (buf[0] == '!') curState = SEGSTART2;
				break;
			case SEGSTART2:
				if (buf[0] == '&') curState = SEGHDR;
				break;
			case SEGHDR:
				{
					if (hdr_ct < sizeof(stcp_hdr_t)){
						hdr[hdr_ct] = buf[0];
						hdr_ct ++;
					}
					if (hdr_ct == sizeof(stcp_hdr_t)) curState = SEGDATA;
				}
				break;
			case SEGDATA:
				{
					if (data_ct < data_len){
						dat[data_ct] = buf[0];
						data_ct ++;
					}
					if (data_ct == data_len) curState = SEGSTOP1;
				}
				break;
			case SEGSTOP1:
				if (buf[0] == '!') curState = SEGSTOP2;
				break;
			case SEGSTOP2:
				if (buf[0] == '#') {
					memset(&(segPtr->header), 0, sizeof(stcp_hdr_t));
					memcpy(&(segPtr->header), hdr, sizeof(stcp_hdr_t));
					memset(segPtr->data, 0, MAX_SEG_LEN);
					if (data_len != 0) memcpy(segPtr->data, dat, data_len);
					return seglost();
				}
				break;
			default: break;
		}
		memset(buf, 2, 0);
	}
//	if (seglost()) return 0;
	return 2;
}

int seglost() {
	int random = rand() % 100;
	if(random<PKT_LOSS_RATE*100) 
		return 1;
	else 
		return 0;
}

