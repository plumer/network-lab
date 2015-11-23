#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <syslog.h>
#include <string.h>

#define PACKET_SIZE 2048
#define ERROR	 	0
#define SUCCESS 	1


// checksum algorithm

unsigned short cal_chksum(unsigned short * addr, int len) {
	int nleft=len;
	int sum = 0;
	unsigned short * w = addr;
	unsigned short answer = 0;

	while (nleft > 1) {
		sum += *w++;
		nleft -= 2;
	}

	if (nleft == 1) {
		*(unsigned char *)(&answer) = *(unsigned char *)w;
		sum += answer;
	}

	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	answer = ~sum;
	return answer;
}

int ping (char *ips, int timeout) {
	struct timeval timeo;
	int sockfd;
	struct sockaddr_in addr;
	struct sockaddr_in from;

	struct timeval *tval;
	struct ip *iph;
	struct icmp *icmp;

	char sendpacket[PACKET_SIZE];
	char recvpacket[PACKET_SIZE];

	int n;
	pid_t pid;
	int maxfds = 0;
	fd_set readfds;

	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ips);
	
	sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);

	if (sockfd < 0) {
//		syslog(LOG_INFO, "IP:%s, socket error", ips);
		printf("IP: %s, socket error\n", ips);
		return ERROR;
	}

	timeo.tv_sec = timeout / 1000;
	timeo.tv_usec = timeout % 1000;

	if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeo, sizeof(timeo)) == -1) {
//		syslog(LOG_INFO, "IP:%s, setsockopt error", ips);
		printf("IP: %s, setsockopt error", ips);
		return ERROR;
	}

	memset(sendpacket, 0, sizeof(sendpacket));

	pid = getpid();
	int i, packsize;

	icmp = (struct icmp*) sendpacket;
	icmp -> icmp_type = ICMP_ECHO;
	icmp -> icmp_code = 0;
	icmp -> icmp_cksum = 0;
	icmp -> icmp_seq = 0;
	icmp -> icmp_id = pid;
	packsize = 8 + 56;
	tval = (struct timeval *)icmp -> icmp_data;
	gettimeofday(tval, NULL);
	icmp -> icmp_cksum = cal_chksum( (unsigned short*)icmp, packsize);

	n = sendto(sockfd, (char *)&sendpacket, packsize, 0, (struct sockaddr *)&addr, sizeof(addr));
	if (n < 1) {
//		syslog(LOG_INFO, "IP:%s, sendto error", ips);
		printf("IP: %s, sendto error", ips);
		return ERROR;
	}

	while (1) {
		FD_ZERO(&readfds);
		FD_SET(sockfd, &readfds);
		maxfds = sockfd + 1;
		n = select(maxfds, &readfds, NULL, NULL, &timeo);
		if (n <= 0) {
//			syslog(LOG_INFO, "IP:%s, Time out error", ips);
			printf( "IP: %s, Time out error", ips);
			close(sockfd);
			return ERROR;
		}

		memset(recvpacket, 0, sizeof(recvpacket));
		int fromlen = sizeof(from);
		n = recvfrom(sockfd, recvpacket, sizeof(recvpacket), 0, (struct sockaddr *)&from, &fromlen);
		if (n < 1) {
			break;
		}

		// check if this packet is a respond to PING of this machine
		char *from_ip = (char *)inet_ntoa(from.sin_addr);
		syslog(LOG_INFO, "from IP: %s", from_ip);
		if (strcmp(from_ip, ips) != 0) {
//			syslog(LOG_INFO, "IP: %s, IP network", ips);
			printf("IP: %s, IP network", ips);
			break;
		}

		iph = (struct ip *) recvpacket;
		icmp = (struct icmp *)(recvpacket + (iph -> ip_hl << 2));

//		syslog(LOG_INFO, "IP: %s, icmp->icmp_type: %d icmp-> icmp_id: %d", ips, icmp->icmp_type, icmp->icmp_id);
		printf("IP: %s, icmp->icmp_type: %d icmp->icmp_id: %d", ips, icmp->icmp_type, icmp->icmp_id);		
		if (icmp->icmp_type == ICMP_ECHOREPLY && icmp->icmp_id == pid) {
			break;
		} else {
			continue;
		}

	}
	close(sockfd);

//	syslog(LOG_INFO, "IP: %s, Success", ips);
	printf("IP: %s, success", ips);
	return SUCCESS;
}

int main(int argc, char* argv[]) {
	
	if (argc != 2) {
		printf("usage: %s 1.2.3.4\n", argv[0]);
		return 1;
	}
	int i;
	for (i = 0; i < 1; ++i) {
		ping(argv[1], 1000);
	}
	return 0;

}
