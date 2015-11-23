//文件名: client/app_stress_client.c
//
//描述: 这是压力测试版本的客户端程序代码. 客户端首先通过在客户端和服务器之间创建TCP连接,启动重叠网络层.
//然后它调用stcp_client_init()初始化STCP客户端. 它通过调用stcp_client_sock()和stcp_client_connect()创建套接字并连接到服务器.
//然后它读取文件sendthis.txt中的文本数据, 将文件的长度和文件数据发送给服务器. 
//经过一段时候后, 客户端调用stcp_client_disconnect()断开到服务器的连接. 
//最后,客户端调用stcp_client_close()关闭套接字. 重叠网络层通过调用son_stop()停止.

//创建日期: 2015年

//输入: 无

//输出: STCP客户端状态

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../common/constants.h"
#include "stcp_client.h"

//创建一个连接, 使用客户端端口号87和服务器端口号88. 
#define CLIENTPORT1 87
#define SERVERPORT1 88

//在发送文件后, 等待5秒, 然后关闭连接.
#define WAITTIME 5

const char * server_ip = "114.212.130.203";
//const char * server_ip = "127.0.0.1";

//这个函数通过在客户和服务器之间创建TCP连接来启动重叠网络层. 它返回TCP套接字描述符, STCP将使用该描述符发送段. 如果TCP连接失败, 返回-1.
int son_start() {

  //你需要编写这里的代码.
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		printf("son socket create error\n");
		return -1;
	}
	struct sockaddr_in server_a;
	memset(&server_a, 0, sizeof(server_a));
	server_a.sin_family = AF_INET;
	server_a.sin_port = htons(SON_PORT);
	inet_pton(AF_INET, server_ip, &server_a.sin_addr);
	//printf("hello~\n");
	if (
		connect(sockfd, (struct sockaddr *)(&server_a), sizeof(struct sockaddr)) == -1
	) {
		printf("TCP connect error\n");
		return -1;
	}

	return sockfd;
}

//这个函数通过关闭客户和服务器之间的TCP连接来停止重叠网络层
void son_stop(int son_conn) {

  //你需要编写这里的代码.
	close(son_conn);
}

int main() {
	//用于丢包率的随机数种子
	srand(time(NULL));

	//启动重叠网络层并获取重叠网络层TCP套接字描述符	
	int son_conn = son_start();
	if(son_conn<0) {
		printf("fail to start overlay network\n");
		exit(1);
	}

	//初始化stcp客户端
	stcp_client_init(son_conn);

	//在端口87上创建STCP客户端套接字, 并连接到STCP服务器端口88.
	int sockfd = stcp_client_sock(CLIENTPORT1);
	if(sockfd<0) {
		printf("fail to create stcp client sock");
		exit(1);
	}
	if(stcp_client_connect(sockfd,SERVERPORT1)<0) {
		printf("fail to connect to stcp server\n");
		exit(1);
	}
	printf("client connected to server, client port:%d, server port %d\n",CLIENTPORT1,SERVERPORT1);
	
	//获取sendthis.txt文件长度, 创建缓冲区并读取文件中的数据.
	FILE *f;
	f = fopen("sendthis.txt","r");
	assert(f!=NULL);
	fseek(f,0,SEEK_END);
	int fileLen = ftell(f);
	fseek(f,0,SEEK_SET);
	char *buffer = (char*)malloc(fileLen);
	fread(buffer,fileLen,1,f);
	fclose(f);

	print_pos();
	printf("readfile end\n");
	//首先发送文件长度, 然后发送整个文件.
	stcp_client_send(sockfd,&fileLen,sizeof(int));
	print_pos();
	printf("send file length end\n");
    stcp_client_send(sockfd, buffer, fileLen);
	printf("send file end\n");
	free(buffer);

	//等待一段时间, 然后关闭连接.
	sleep(17);

	if(stcp_client_disconnect(sockfd)<0) {
		printf("fail to disconnect from stcp server\n");
		exit(1);
	}
	if(stcp_client_close(sockfd)<0) {
		printf("fail to close stcp client\n");
		exit(1);
	}
	
	//停止重叠网络层
	son_stop(son_conn);
}
