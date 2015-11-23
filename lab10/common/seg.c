#include "seg.h"

//STCP进程使用这个函数发送sendseg_arg_t结构(包含段及其目的节点ID)给SIP进程.
//参数sip_conn是在STCP进程和SIP进程之间连接的TCP描述符.
//如果sendseg_arg_t发送成功,就返回1,否则返回-1.
int sip_sendseg(int sip_conn, int dest_nodeID, seg_t* segPtr){
	const char * begin = "!&";
	const char * end = "!#";
	int r = send(sip_conn, begin, strlen(begin), 0);

	if (r < 0) {
		printf("Error: stcp use sip_sendseg send to sip failed, when send !&\n");
		return -1;
	}
	segPtr->header.checksum = 0;
	segPtr->header.checksum = checksum(segPtr);

	sendseg_arg_t sendseg;
	sendseg.nodeID = dest_nodeID;
	memcpy(&(sendseg.seg), segPtr, sizeof(seg_t));

	r = send(sip_conn, &sendseg, sizeof(sendseg_arg_t), 0);
	if (r < 0) {
		printf("Error: stcp use sip_sendseg send to sip failed, when send sendseg_t\n");
		return -1;
	}
	r = send(sip_conn, end, strlen(end), 0);
	if (r < 0) {
		printf("Error: stcp use sip_sendseg send to sip failed, when send !#\n");
		return -1;
	}
	return 1;
}

//STCP进程使用这个函数来接收来自SIP进程的包含段及其源节点ID的sendseg_arg_t结构.
//参数sip_conn是STCP进程和SIP进程之间连接的TCP描述符.
//当接收到段时, 使用seglost()来判断该段是否应被丢弃并检查校验和.
//如果成功接收到sendseg_arg_t就返回1, 否则返回-1.
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
#define SEGNODE 2
#define SEGHDR	3
#define SEGDATA 4
#define SEGSTOP1 5
#define SEGSTOP2 6

int sip_recvseg(int sip_conn, int *src_nodeID, seg_t* segPtr) {
	char buf[2];
	memset(buf, 0, 2);
	int curState = SEGSTART1;

	char *hdr = (char *)malloc(sizeof(stcp_hdr_t)+1);
	char *dat = (char *)malloc(MAX_SEG_LEN*sizeof(char)+1);
	char nod[sizeof(int)+1];

	memset(hdr, 0, sizeof(stcp_hdr_t)+1);
	memset(dat, 0, MAX_SEG_LEN+1);
	memset(nod, 0, sizeof(int)+1);

	int hdr_ct = 0;
	int data_ct = 0;
	int data_len = 0;

	int n;
	int node_ct = 0;
	int node_len = 0;
	while ((n = recv(sip_conn, buf, 1, 0)) > 0){
		switch(curState){
			case SEGSTART1: 
				if (buf[0] == '!') curState = SEGSTART2;
				break;
			case SEGSTART2:
				if (buf[0] == '&') {
					curState = SEGNODE;
					node_len = sizeof(int);
				}
				break;
			case SEGNODE:
				{
					if (node_ct < node_len){
						nod[node_ct] = buf[0];
						node_ct ++;
					}
					if (node_ct == node_len) curState = SEGHDR;
				}
				break;
			case SEGHDR:
				{
					if (hdr_ct < sizeof(stcp_hdr_t)){
						hdr[hdr_ct] = buf[0];
						hdr_ct ++;
					}
					if (hdr_ct == sizeof(stcp_hdr_t)) {
						data_len = ((stcp_hdr_t *)hdr)->length;
						curState = SEGDATA;
					}
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
					// set src_nodeID;
					memcpy(src_nodeID, nod, sizeof(int));
					// set seg_t
					memset(&(segPtr->header), 0, sizeof(stcp_hdr_t));
					memcpy(&(segPtr->header), hdr, sizeof(stcp_hdr_t));
					memset(segPtr->data, 0, MAX_SEG_LEN);
					if (segPtr->header.length != 0) memcpy(segPtr->data, dat, segPtr->header.length);
					// if seglost, return -1
					if (seglost(segPtr) == 1) return -1;
					// if not lost, seglost=0, return follow
					else {
						// if checkchecksum right, return 1
						if (checkchecksum(segPtr) == 1) return 1;
						// if checkchecksum error, return -1
						else {
							printf("SIP CHECKSUM ERROR\n");
							return -1;
						}
					}
				}
				break;
			default: break;
		}
		memset(buf, 2, 0);
	}
	printf("MSG: tcp connection of stcp-sip is break, stcp sip_recvseg failed\n");
	// if connection error return 2
	return 2;
}
//SIP进程使用这个函数接收来自STCP进程的包含段及其目的节点ID的sendseg_arg_t结构.
//参数stcp_conn是在STCP进程和SIP进程之间连接的TCP描述符.
//如果成功接收到sendseg_arg_t就返回1, 否则返回-1.
int getsegToSend(int stcp_conn, int* dest_nodeID, seg_t* segPtr){
	char buf[2];
	memset(buf, 0, 2);
	int curState = SEGSTART1;

	char *hdr = (char *)malloc(sizeof(stcp_hdr_t)+1);
	char *dat = (char *)malloc(MAX_SEG_LEN*sizeof(char)+1);
	char nod[sizeof(int)+1];

	memset(hdr, 0, sizeof(stcp_hdr_t)+1);
	memset(dat, 0, MAX_SEG_LEN+1);
	memset(nod, 0, sizeof(int)+1);

	int hdr_ct = 0;
	int data_ct = 0;
	int data_len = 0;

	int n;
	int node_ct = 0;
	int node_len = 0;
	while ((n = recv(stcp_conn, buf, 1, 0)) > 0){
		switch(curState){
			case SEGSTART1: 
				if (buf[0] == '!') curState = SEGSTART2;
				break;
			case SEGSTART2:
				if (buf[0] == '&') {
					curState = SEGNODE;
					node_len = sizeof(int);
				}
				break;
			case SEGNODE:
				{
					if (node_ct < node_len){
						nod[node_ct] = buf[0];
						node_ct ++;
					}
					if (node_ct == node_len) curState = SEGHDR;
				}
				break;
			case SEGHDR:
				{
					if (hdr_ct < sizeof(stcp_hdr_t)){
						hdr[hdr_ct] = buf[0];
						hdr_ct ++;
					}
					if (hdr_ct == sizeof(stcp_hdr_t)) {
						data_len = ((stcp_hdr_t *)hdr)->length;
						curState = SEGDATA;
					}
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
					// set src_nodeID;
					memcpy(dest_nodeID, nod, sizeof(int));
					// set seg_t
					memset(&(segPtr->header), 0, sizeof(stcp_hdr_t));
					memcpy(&(segPtr->header), hdr, sizeof(stcp_hdr_t));
					memset(segPtr->data, 0, MAX_SEG_LEN);
					if (segPtr->header.length != 0) memcpy(segPtr->data, dat, segPtr->header.length);
					if (checkchecksum(segPtr) == 1) return 1;
					else {
						print_pos();
						printf("SIP CHECKSUM ERROR\n");
						return -1;
					}
				}
				break;
			default: break;
		}
		memset(buf, 2, 0);
	}
	printf("MSG: sip getsegToSend failed, tcp conn of sip-stcp break\n");
	return 2;
}

//SIP进程使用这个函数发送包含段及其源节点ID的sendseg_arg_t结构给STCP进程.
//参数stcp_conn是STCP进程和SIP进程之间连接的TCP描述符.
//如果sendseg_arg_t被成功发送就返回1, 否则返回-1.
int forwardsegToSTCP(int stcp_conn, int src_nodeID, seg_t* segPtr){
	const char * begin = "!&";
	const char * end = "!#";
	int r = send(stcp_conn, begin, strlen(begin), 0);

	if (r < 0) {
		printf("Error: sip forwardsegToSTCP failed, when send !&\n");
		return -1;
	}
//	segPtr->header.checksum = 0;
//	segPtr->header.checksum = checksum(segPtr);

	sendseg_arg_t sendseg;
	sendseg.nodeID = src_nodeID;
	memcpy(&(sendseg.seg), segPtr, sizeof(seg_t));

	r = send(stcp_conn, &sendseg, sizeof(sendseg_arg_t), 0);
	if (r < 0) {
		printf("Error: sip forwardsegToSTCP failed, when send sendseg_t\n");
		return -1;
	}
	r = send(stcp_conn, end, strlen(end), 0);
	if (r < 0) {
		printf("Error: sip forwardsegToSTCP failed, when send !#\n");
		return -1;
	}
	return 1;
}

// 一个段有PKT_LOST_RATE/2的可能性丢失, 或PKT_LOST_RATE/2的可能性有着错误的校验和.
// 如果数据包丢失了, 就返回1, 否则返回0. 
// 即使段没有丢失, 它也有PKT_LOST_RATE/2的可能性有着错误的校验和.
// 我们在段中反转一个随机比特来创建错误的校验和.
int seglost(seg_t * segPtr) {
	int random = rand() % 100;
	if(random<PKT_LOSS_RATE*100) {
		print_pos();
		printf("lost package: type = %d, seq_num = %d, ack_num = %d",
				segPtr->header.type, segPtr -> header.seq_num,segPtr -> header.ack_num);
		//50%可能性丢失段
		if(rand()%2==0) {
			printf("seg lost!!!\n");
			return 1;
		} else { //50%可能性是错误的校验和 
			//获取数据长度
			int len = sizeof(stcp_hdr_t)+segPtr->header.length;
			//获取要反转的随机位
			int errorbit = rand() % (len*8);
			//反转该比特
			char* temp = (char*)segPtr;
			temp = temp + errorbit/8;
			*temp = *temp ^ (1 << (errorbit % 8));
			return 0;
		}
	}
	return 0;
}

//这个函数计算指定段的校验和.
//校验和覆盖段首部和段数据. 你应该首先将段首部中的校验和字段清零, 
//如果数据长度为奇数, 添加一个全零的字节来计算校验和.
//校验和计算使用1的补码.
unsigned short checksum(seg_t* segment)
{
	int count = sizeof(stcp_hdr_t) + segment->header.length;
	unsigned int sum = 0;
	unsigned char *addr = (unsigned char*)segment;

	while( count > 1 )  {
		sum += *((unsigned short *)addr);
		addr += 2;
		count -= 2;
		unsigned short tmp = sum>>16;
		if (tmp > 0){
			sum = sum & 0x0000ffff;
			sum += tmp;
		}
	}

	if( count > 0 ){
		unsigned char a[2];
		a[0] = addr[0];
		a[1] = 0x00;

		sum += *((unsigned short *)a);
		unsigned short tmp = sum>>16;
		tmp = tmp >> 16;
		if (tmp > 0){
			sum = sum&0x0000ffff;
			sum += tmp;
		}
	}

	return  ~sum;
}

//这个函数检查段中的校验和, 正确时返回1, 错误时返回-1
int checkchecksum(seg_t* segment) {
	return checksum(segment) == 0? 1:-1;
}

