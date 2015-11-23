
#include "btdata.h"
#define BE_DEBUG
#include "bencode.h"
#include "util.h"
#include <ctype.h>
#include <assert.h>

// 读取并处理来自Tracker的HTTP响应, 确认它格式正确, 然后从中提取数据. 
// 一个Tracker的HTTP响应格式如下所示:
// HTTP/1.1 200 OK		 (17个字节,包括最后的\r\n)
// Content-Type: text/plain (26个字节)
// Content-Length: X		(到第一个空格为16个字节) 注意: X是一个数字
// Pragma: no-cache (18个字节)
// \r\n	(空行, 表示数据的开始)
// data						注意: 从这开始是数据, 但并没有一个data标签
tracker_response* preprocess_tracker_response(int sockfd)
{ 
	char rcvline[MAXLINE];
	char tmp[MAXLINE];
	char* data;
	int len;
	int offset = 0;
	int datasize = -1;
	// printf("Reading tracker response...\n");

	// HTTP LINE
	len = recv(sockfd,rcvline,17,0);
	if (len < 0) {
		perror("Error, cannot read socket from tracker");
		exit(-6);
	}
	strncpy(tmp,rcvline,17);	
	if (strncmp(tmp,"HTTP/1.1 200 OK\r\n",strlen("HTTP/1.1 200 OK\r\n"))) {
		perror("Error, didn't match HTTP line");
		exit(-6);
	}
	

	// Content-Type
	const char * content_type = "Content-Type: text/plain\r\n";
	memset(rcvline,0x00,MAXLINE);
	memset(tmp,0x0,MAXLINE);
	len = recv(sockfd, rcvline, strlen(content_type), 0);
	if(len <= 0) {
		perror("Error, cannot read socket from tracker");
		exit(-6);
	}
	if (strncmp(rcvline, content_type, strlen(content_type))) {
		perror("Error, didn't match Content-Type line");
		exit(-6);
	}

	// Content-Length
	const char * content_length = "Content-Length: ";
	memset(rcvline,0x00,MAXLINE);
	memset(tmp,0x0,MAXLINE);
	len = recv(sockfd,rcvline,16,0);
	if(len <= 0)
	{
		perror("Error, cannot read socket from tracker");
		exit(-6);
	}
	strncpy(tmp,rcvline,16);
	if(strncmp(tmp, content_length ,strlen(content_length)))
	{
		perror("Error, didn't match Content-Length line");
		exit(-6);
	}
	memset(rcvline,0x00,MAXLINE);
	memset(tmp,0x0,MAXLINE);

	// 读取Content-Length的数据部分
	char c[2];
	char num[MAXLINE];
	int count = 0;
	c[0] = 0; c[1] = 0;
	while(c[0] != '\r' && c[1] != '\n')
	{
		len = recv(sockfd,rcvline,1,0);
		if(len <= 0)
		{
			perror("Error, cannot read socket from tracker");
			exit(-6);
		}
		num[count] = rcvline[0];
		c[0] = c[1];
		c[1] = num[count];
		count++;
	}
	datasize = atoi(num);
	//printf("Content-Length = %d\n",datasize);
	
	// 大湿胸说，下一行的memset改成0比较好。之前是0xff
	memset(rcvline,0x00,MAXLINE);
	memset(num,0x0,MAXLINE);

/*	// Pragma行
	len = recv(sockfd,rcvline,18,0);
	if(len <= 0)
	{
		perror("Error, cannot read socket from tracker");
		exit(-6);
	}
*/	// 去除响应中额外的\r\n空行
	char startc;
	while (1) {
		len = recv(sockfd, &startc, 1, 0);
		if(len <= 0) {
			perror("Error, cannot read socket from tracker");
			exit(-6);
		}
		if ( isspace(startc) )
			continue;
		else {
			break;
		}
	}

	// 分配空间并读取数据, 为结尾的\0预留空间
	int i; 
	data = (char*)malloc((datasize+1)*sizeof(char));
	memset(data, 0, (datasize+1) * sizeof(char) );
	data[0] = startc;
	for (i = 1; i < datasize; i++) {
		len = recv(sockfd,data+i,1,0);
		if(len < 0)
		{
			perror("Error, cannot read socket from tracker");
			exit(-6);
		}
	}
	data[datasize] = '\0';
	/*
	for(i=0; i<datasize; i++)
		printf("%c",data[i]);
	printf("\n");
	puts("received data above");
	*/
	// 分配, 填充并返回tracker_response结构.
	tracker_response* ret;
	ret = (tracker_response*)malloc(sizeof(tracker_response));
	if(ret == NULL)
	{
		printf("Error allocating tracker_response ptr\n");
		return 0;
	}
	ret->size = datasize;
	ret->data = data;

	return ret;
}

// 解码B编码的数据, 将解码后的数据放入tracker_data结构
tracker_data* get_tracker_data(char* data, int len)
{
	tracker_data* ret;
	be_node* ben_res;
	ben_res = be_decoden(data,len);
	if ( ben_res->type != BE_DICT) {
		perror("Data not of type dict");
		exit(-12);
	}
 
	ret = (tracker_data*)malloc(sizeof(tracker_data));
	if (ret == NULL) {
		perror("Could not allcoate tracker_data");
		exit(-12);
	}

	// 遍历键并测试它们
	int i;
	for (i = 0; ben_res -> val.d[i].val != NULL; i++) { 
		//printf("%s\n",ben_res->val.d[i].key);
		// 检查是否有失败键
		if(!strncmp(ben_res->val.d[i].key,"failure reason",strlen("failure reason"))) {
			printf("Error: %s",ben_res->val.d[i].val->val.s);
			exit(-12);
		}
		// interval键
		if(!strncmp(ben_res->val.d[i].key,"interval",strlen("interval"))) {
			ret->interval = (int)ben_res->val.d[i].val->val.i;
		}
		// peers键
		if(!strncmp(ben_res->val.d[i].key,"peers",strlen("peers"))) { 
			be_node* peer_list = ben_res->val.d[i].val;
			get_peers(ret,peer_list);
		}
	}
 
	be_free(ben_res);

	return ret;
}

void get_peers(tracker_data * td, be_node * peer_list) {
	// this peer_list is a b-encoded string.
	assert(peer_list -> type == BE_STR);
	unsigned char * s = peer_list -> val.s;
	long long l = *(long long *)(s - sizeof(long long));
	if (l % 6 == 0) {
		td -> numpeers = l / 6;
	} else if (l % 6 == 4) {
		td -> numpeers = (l + 2) / 6;
	} else {
		printf("panic: l = %lld\n", l);
		td -> numpeers = 0;
	}
	td -> peers = (peerdata *)malloc( sizeof(peerdata) * td -> numpeers );
	int i = 0;
	for (; i < td -> numpeers; ++i) {
		memset(td -> peers[i].id, 0, sizeof( td -> peers[i].id ) );
		td -> peers[i].ip = (char *)malloc(sizeof(char) * 16);
		memset(td -> peers[i].ip, 0, sizeof(char)*16 );
		sprintf(td -> peers[i].ip, "%u.%u.%u.%u", s[0], s[1], s[2], s[3]);
		//printf("port[%d]:%d\n", i, (unsigned int)(s[4]) *256 + (unsigned int)s[5]);
		td -> peers[i].port = (unsigned int)(s[4]) *256 + (unsigned int)s[5];
		s += 6;
	}
}

// 处理来自Tracker的字典模式的peer列表
/*
void get_peers(tracker_data* td, be_node* peer_list)
{
	printf("dumping peer_list\n");
	be_dump(peer_list);
	int i;
	int numpeers = 0;
	
	// 计算列表中的peer数
	for (i=0; peer_list->val.l[i] != NULL; i++)
	{
		// 确认元素是一个字典
		if(peer_list->val.l[i]->type != BE_DICT)
		{
			perror("Expecting dict, got something else");
			exit(-12);
		}
		
		// 找到一个peer, 增加numpeers
		numpeers++;
	}
	
	printf("Num peers: %d\n",numpeers);

	// 为peer分配空间
	td->numpeers = numpeers;
	td->peers = (peerdata*)malloc(numpeers*sizeof(peerdata));
	if(td->peers == NULL)
	{
		perror("Couldn't allocate peers");
		exit(-12);
	}
	
	// 获取每个peer的数据
	for (i=0; peer_list->val.l[i] != NULL; i++)
	{
		get_peer_data(&(td->peers[i]),peer_list->val.l[i]);
	}

	return;

}*/

// 给出一个peerdata的指针和一个peer的字典数据, 填充peerdata结构
void get_peer_data(peerdata* peer, be_node* ben_res)
{
	int i;
	
	if(ben_res->type != BE_DICT)
	{
		perror("Don't have a dict for this peer");
		exit(-12);
	}
	
	// 遍历键并填充peerdata结构
	for (i=0; ben_res->val.d[i].val != NULL; i++)
	{ 
		//printf("%s\n",ben_res->val.d[i].key);
		
		// peer id键
		if(!strncmp(ben_res->val.d[i].key,"peer id",strlen("peer id")))
		{
			//printf("Peer id: %s\n", ben_res->val.d[i].val->val.s);
			memcpy(peer->id,ben_res->val.d[i].val->val.s,20);
			peer->id[20] = '\0';
			/*
			 *		int idl;
			 *		printf("Peer id: ");
			 *		for(idl=0; idl<len; idl++)
			 *			printf("%02X ",(unsigned char)peer->id[idl]);
			 *		printf("\n");
			 */
		}
		// ip键
		if(!strncmp(ben_res->val.d[i].key,"ip",strlen("ip")))
		{
			int len;
			//printf("Peer ip: %s\n",ben_res->val.d[i].val->val.s);
			len = strlen(ben_res->val.d[i].val->val.s);
			peer->ip = (char*)malloc((len+1)*sizeof(char));
			strcpy(peer->ip,ben_res->val.d[i].val->val.s);
		}
		// port键
		if(!strncmp(ben_res->val.d[i].key,"port",strlen("port")))
		{
			//printf("Peer port: %d\n",ben_res->val.d[i].val->val.i);
			peer->port = ben_res->val.d[i].val->val.i;
		}
	}
}

