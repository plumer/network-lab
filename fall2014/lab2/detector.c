#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <linux/if_ether.h>
#include <arpa/inet.h>

//#include <linux/in.h>

#define BUFFER_MAX 2048
#define IPV6_ADDR_LEN 128/8
int main(int argc, char* argv[]) {
	int sock_fd;
	int proto;
	unsigned short type;
	unsigned short version;
	unsigned short head_len;
	unsigned short service_type;
	unsigned short datagram_len;
	unsigned short datagram_ID;
	unsigned short checksum;
	int n_read;
	unsigned char buffer[BUFFER_MAX];
	unsigned char *eth_head;
	unsigned char *ip_head;
	unsigned char *tcp_head;
	unsigned char *udp_head;
	unsigned char *icmp_head;
	unsigned char *p;
	struct sockaddr src_addr;
	printf("ETH_P_IP = %x\n", htons(ETH_P_IP));
	if ( (sock_fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0 ) {
		printf("error create raw socket\n");
		return -1;
	}
	
	while (1) {
		n_read = recvfrom(sock_fd, buffer, 2048, 0, NULL, NULL);
//		printf("src_addr: %2x:%2x:/so///", src_addr.sa_data);
		if (n_read < 42) {
			printf("error when recv msg \n");
			return -1;
		}
		eth_head = buffer;
		p = eth_head;
		printf("\nMAC address: %.2x:%02x:%02x:%02x:%02x:%02x ==> %.2x:%02x:%02x:%02x:%02x:%02x\n",
			p[6], p[7], p[8], p[9], p[10], p[11], p[0], p[1], p[2], p[3], p[4], p[5]);

		type = htons(*(unsigned short *)(eth_head+12));
		switch (type) {
			case 0x0800: puts("This is an IP-protocol packet");break;
			case 0x0806: puts("This is an ARP-protocol packet"); break;
			case 0x8035: puts("This is an RARP-protocol packet"); break;
			case 0x86dd: puts("This is an IPv6-protocol packet"); break;
			default: printf("The protocol is %04x\n", type); break;
		}

		if (type == 0x0800) { // if this is an IP-protocol packet
			ip_head = eth_head + 14;
			
			version = (unsigned short)(*ip_head) >> 4;
			head_len = (unsigned short)(*ip_head) & 0xF;
			service_type = (unsigned short)*(ip_head+1);
			datagram_len = htons(*(unsigned short*)(ip_head+2));
			datagram_ID = htons(*(unsigned short *)(ip_head+4));
			checksum = htons( *(unsigned short *)(ip_head+10));
			
			printf("version number: 0x%X\t", version);
			printf("head length: %dbytes\t", head_len);
			printf("service type: 0x%X\n", service_type);
			printf("datagram length: %d\t", datagram_len);
			printf("datagram ID: %d\t", datagram_ID);
			printf("head checksum: 0x%X\n", checksum);
	
			p = ip_head + 12;
			printf("IP: %d.%d.%d.%d ==> %d.%d.%d.%d\t", p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
			proto = (ip_head + 9)[0];
			printf("Protocol: ");
			switch (proto) {
				case IPPROTO_ICMP: printf("icmp\n"); break;
				case IPPROTO_IGMP: printf("igmp\n"); break;	
				case IPPROTO_IPIP: printf("ipip\n"); break;	
				case IPPROTO_TCP:  printf("tcp\n"); break;	
				case IPPROTO_UDP:  printf("udp\n"); break;	
				default: printf("some unknown protocol\n"); break;
			}
		} else if (type == 0x86dd) { // if this is an IPv6-protocol packet
			ip_head = eth_head + 14;
			int i;
			
			version = (unsigned short)(*ip_head) >> 4;
			unsigned short priority = (unsigned short)(*ip_head) & 0xF;
			unsigned int flow_label = htons( *(unsigned long*)ip_head ) & 0x00FFFFFF;
			uint16_t payload_length = htons( *(unsigned short *)(ip_head + 4) );
			uint8_t next_header = (uint8_t)(*ip_head + 6);
			uint8_t hop_limit = (uint8_t)(*ip_head + 7);

			const uint8_t * src_addr = ip_head + 8;
			const uint8_t * dst_addr = ip_head + 24;

			printf("version number: 0x%X\t", version);
			printf("Priority: 0x%X\t", priority);
			printf("Flow label: 0x%X\n", flow_label);
			printf("Payload length: %d\t", payload_length);
			printf("Next header type: 0x%X\t", next_header);
			printf("Hop limit: %d\n", hop_limit);
			printf("Source address: ");
			for (i = 0; i < IPV6_ADDR_LEN; ++i) {
				printf("%d.", src_addr[i]);
			}
			printf("\nDestination address: ");
			for (i = 0; i < IPV6_ADDR_LEN; ++i) {
				printf("%d.", dst_addr[i]);
			}
			printf("\n");

		} else if (type == 0x0806 || type == 0x8035) {	// if this is an ARP or RARP-protocol packet
			uint8_t * arp_ptr = (eth_head + 14);
			uint8_t * arp_head_ptr = arp_ptr; 
			printf("hardware type: 0x%X\t", htons( *(uint16_t*)(arp_head_ptr) ) );
			printf("protocol type: 0x%X\t", htons( *(uint16_t*)(arp_head_ptr+2) ) );
			printf("ARP operation: 0x%X\n", htons( *(uint16_t*)(arp_head_ptr+6) ) );
			uint8_t haddr_len = arp_head_ptr[4];
			uint8_t paddr_len = arp_head_ptr[5];
			uint8_t * addr_ptr = arp_ptr + 8; // 8 is the size of arp header
			printf("hardware address: ");
			int i;
			for (i = 0; i < haddr_len; ++i)
				printf("%d.", addr_ptr[i]);
			printf(" ==> ");
			for (addr_ptr += (haddr_len + 4), i = 0; i < haddr_len; ++i) {
				printf("%d.", addr_ptr[i]);
			}
			addr_ptr = arp_ptr + 8 + haddr_len;
			printf("\nProtocol address: %d.%d.%d.%d ==> ", 
				addr_ptr[0], addr_ptr[1], addr_ptr[2], addr_ptr[3]);
			addr_ptr += (4 + haddr_len);
			printf("%d.%d.%d.%d\n",
				addr_ptr[0], addr_ptr[1], addr_ptr[2], addr_ptr[3]);
		}
	}
	return -1;
}
