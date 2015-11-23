#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

#include "util.h"
void convert_endian(uint8_t *str, int len){
	int i;
	for (i = 0; i < len; i +=4){
		uint8_t c = str[i];
		str[i] = str[i+3];
		str[i+3] = c;
		c = str[i+1];
		str[i+1] = str[i+2];
		str[i+2] = c;
	}
}

int connect_to_host(char* ip, int port)
{
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
	{
		perror("Could not create socket");
		return(-1);
	}

	struct sockaddr_in addr;
	memset(&addr,0,sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ip);
	addr.sin_port = htons(port);

	if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("Error connecting to socket");
		return(-1);
	}

	return sockfd;
}

int make_listen_port(int port)
{
	int sockfd;

	sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sockfd <0)
	{
		perror("Could not create socket");
		return 0;
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
	{
			perror("Could not bind socket");
			return 0;
	}

	if(listen(sockfd, 20) < 0)
	{
		perror("Error listening on socket");
		return 0;
	}

	return sockfd;
}

// 计算一个打开文件的字节数
int file_len(FILE* fp)
{
	int sz;
	fseek(fp , 0 , SEEK_END);
	sz = ftell(fp);
	rewind (fp);
	return sz;
}

// recvline(int fd, char **line)
// 描述: 从套接字fd接收字符串
// 输入: 套接字描述符fd, 将接收的行数据写入line
// 输出: 读取的字节数
int recvline(int fd, char **line)
{
	int retVal;
	int lineIndex = 0;
	int lineSize	= 128;
	
	*line = (char *)malloc(sizeof(char) * lineSize);
	
	if (*line == NULL)
	{
		perror("malloc");
		return -1;
	}
	
	while ((retVal = read(fd, *line + lineIndex, 1)) == 1)
	{
		if ('\n' == (*line)[lineIndex])
		{
			(*line)[lineIndex] = 0;
			break;
		}
		
		lineIndex += 1;
		
		/*
			如果获得的字符太多, 就重新分配行缓存.
		*/
		if (lineIndex > lineSize)
		{
			lineSize *= 2;
			char *newLine = realloc(*line, sizeof(char) * lineSize);
			
			if (newLine == NULL)
			{
				retVal		= -1; /* realloc失败 */
				break;
			}
			
			*line = newLine;
		}
	}
	
	if (retVal < 0)
	{
		free(*line);
		return -1;
	}
	#ifdef NDEBUG
	else
	{
		fprintf(stdout, "%03d> %s\n", fd, *line);
	}
	#endif

	return lineIndex;
}
/* End recvline */

// recvlinef(int fd, char *format, ...)
// 描述: 从套接字fd接收字符串.这个函数允许你指定一个格式字符串, 并将结果存储在指定的变量中
// 输入: 套接字描述符fd, 格式字符串format, 指向用于存储结果数据的变量的指针
// 输出: 读取的字节数
int recvlinef(int fd, char *format, ...)
{
	va_list argv;
	va_start(argv, format);
	
	int retVal = -1;
	char *line;
	int lineSize = recvline(fd, &line);
	
	if (lineSize > 0)
	{
		retVal = vsscanf(line, format, argv);
		free(line);
	}
	
	va_end(argv);
	
	return retVal;
}
/* End recvlinef */

int reverse_byte_orderi(int i)
{
	unsigned char c1, c2, c3, c4;
	c1 = i & 0xFF;
	c2 = (i >> 8) & 0xFF;
	c3 = (i >> 16) & 0xFF;
	c4 = (i >> 24) & 0xFF;
	return ((int)c1 << 24) + ((int)c2 << 16) + ((int)c3 << 8) + c4;
}

// 这个函数接收n个字节
// 如果正常，返回0，否则返回还差多少个字节没有接收
int recvn(int fd, char *buffer, int n, int flag){
	memset(buffer, 0, n);
	char bf[2];
	int i = 0;
	while (n > 0){
		memset(bf, 0, 2);
		int res = recv(fd, bf, 1, flag);
		if (res <= 0) {
			printf("recvn error, return %d\n", res);
			break;
		}
		buffer[i] = bf[0];
		i++;
		n--;
	}
	return n == 0 ? 0:-1;
}

/* readn - read exactly n bytes */
int readn(int fd, char *bp, size_t len)
{
	int cnt;
	int rc;

	cnt = len;
	while ( cnt > 0 )
	{
		rc = recv( fd, bp, cnt, 0 );
		if ( rc < 0 )				/* read error? */
		{
			if ( errno == EINTR )	/* interrupted? */
				continue;			/* restart the read */
			return -1;				/* return error */
		}
		if ( rc == 0 )				/* EOF? */
			return len - cnt;		/* return short count */
		bp += rc;
		cnt -= rc;
	}
	return len;
}
