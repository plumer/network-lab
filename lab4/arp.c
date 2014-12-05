#include "arp.h"

//如果只是想让对方断网，那就把mac源都设成MAC_TRICK，
#define IP_SOURCE "192.168.5.1"
//目标机器的MAC
//目标机器的IP
#define IP_TARGET "192.168.5.2"

struct arp_packet
{
	//DLC Header
	unsigned char mac_target[ETH_ALEN];		//接收方mac 广播FF
	unsigned char mac_source[ETH_ALEN];		//发送方mac
	unsigned short ethertype;					//Ethertype - 0x0806是ARP帧的类型值

	//ARP Frame
	unsigned short hw_type;						//硬件类型 - 以太网类型值0x1
	unsigned short proto_type;					//上层协议类型 - IP协议(0x0800)
	unsigned char mac_addr_len;				//MAC地址长度
	unsigned char ip_addr_len;					//IP地址长度
	unsigned short operation_code;			//操作码 - 0x1表示ARP请求包,0x2表示应答包
	unsigned char mac_sender[ETH_ALEN];		//发送方mac
	unsigned char ip_sender[4];				//发送方ip
	unsigned char mac_receiver[ETH_ALEN];	//接收方mac
	unsigned char ip_receiver[4];				//接收方ip
	unsigned char padding[18];					//填充数据
};

void die(const char *pre);

void getmac(char * my_ip, char *target_ip, unsigned char *my_mac, unsigned char *target_mac,char *send_iface)
{	
	printf("%s, %s, %s\n", my_ip, target_ip, send_iface);
	int sfd, len;
	struct arp_packet ap;
	struct in_addr inaddr_sender, inaddr_receiver;
	struct sockaddr_ll sl;
	unsigned char *MY_mac_addr=my_mac;
	sfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if(-1 == sfd)
	{
		perror("socket");
	}
	memset(&ap, 0, sizeof(struct arp_packet));
	//以太帧头
	//广播
	memset(ap.mac_target, 0xff, sizeof(ap.mac_target));
	//本机MAC
	memcpy(ap.mac_source , MY_mac_addr,sizeof(ap.mac_source));
	
	//ARP固定的
	ap.ethertype = htons(0x0806);
	ap.hw_type = htons(0x1);
	ap.proto_type = htons(0x0800);
	ap.mac_addr_len = ETH_ALEN;
	ap.ip_addr_len = 4;
	ap.operation_code = htons(0x1);
	
	//ARP中本机MAC
	memcpy(ap.mac_sender , MY_mac_addr,sizeof(ap.mac_sender));
	//本机ip
	inet_aton(my_ip, &inaddr_sender);
	memcpy(&ap.ip_sender, &inaddr_sender, sizeof(inaddr_sender));
	//下面这句没用，不影响
	memset(ap.mac_receiver, 0xff, sizeof(ap.mac_receiver));
	
	//目的ip
	inet_aton(target_ip, &inaddr_receiver);
	memcpy(&ap.ip_receiver, &inaddr_receiver, sizeof(inaddr_receiver));

	struct ifreq ifrq;
	strcpy(ifrq.ifr_name, send_iface);
	ioctl(sfd, SIOCGIFINDEX, &ifrq);
	memset(&sl, 0, sizeof(sl));
	sl.sll_family = AF_PACKET;
	//	sl.sll_ifindex = IFF_BROADCAST;//非常重要
	sl.sll_ifindex = ifrq.ifr_ifindex;
	len = sendto(sfd, &ap, sizeof(ap), 0, (struct sockaddr*)&sl, sizeof(sl));


	int n_read;
	unsigned char *eth_head;
	unsigned char *p;
	unsigned char buffer[2048];
	unsigned short type;
	struct ether_addr wanted_eth_addr;
	struct in_addr my_ip_addr;
	
	while(1)
	{
		n_read = recvfrom(sfd, buffer, 2048, 0, NULL, NULL);
		if (n_read < 42) {
			printf("error when recv msg \n");
			return;
		}
		eth_head = buffer;
		p = eth_head;
		type = htons(*(unsigned short *)(eth_head+12));
		if(type == 0x0806 || type == 0x8035)
		{
			uint8_t * arp_ptr = (eth_head + 14);
			uint8_t * arp_head_ptr = arp_ptr; 
			uint8_t haddr_len = arp_head_ptr[4];
			uint8_t * addr_ptr = arp_ptr + 8; // 8 is the size of arp header
			addr_ptr = arp_ptr + 8 + haddr_len;
//			printf("\nProtocol address: %d.%d.%d.%d ==> %d.%d.%d.%d", 
	//			addr_ptr[0], addr_ptr[1], addr_ptr[2], addr_ptr[3],
		//		addr_ptr[8], addr_ptr[9], addr_ptr[10], addr_ptr[11]);//source ip
			
			//printf("\nMAC address: %.2x:%02x:%02x:%02x:%02x:%02x ==> %.2x:%02x:%02x:%02x:%02x:%02x\n",
	//		p[6], p[7], p[8], p[9], p[10], p[11], p[0], p[1], p[2], p[3], p[4], p[5]);
			char tmp_ip[16];
			char tmp_mac[6];
			sprintf(tmp_ip,"%d.%d.%d.%d",addr_ptr[0], addr_ptr[1], addr_ptr[2], addr_ptr[3]);
			if (!strcmp(tmp_ip,target_ip)) {
				*(target_mac)=p[6];
				*(target_mac+1)=p[7];
				*(target_mac+2)=p[8];
				*(target_mac+3)=p[9];
				*(target_mac+4)=p[10];
				*(target_mac+5)=p[11];
				
				break;
			}
		}
	}

}

void die(const char *pre)
{
	perror(pre);
	exit(1);
}
