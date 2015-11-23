#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <assert.h>
#include <netpacket/packet.h>

#include "lib.h"
#include "arp.h"

#define BUFFER_MAX 2048
#define HEXA_TO_NUM(c) ((c>'0' && c<'9') ? (c-'0') : (c|0x20-'a'))


struct route_item route_table[M];
struct arp_item arp_table[M];
struct interface_item iface_table[M];

int route_cnt = 0, arp_cnt = 0, iface_cnt = 0;

int main(int argc, char * argv[]) {
	int sock_fd;
	int n_read;
	uint8_t * eth_head;
	uint8_t * ip_head;
	uint8_t ip_proto_type;
	uint8_t buffer[BUFFER_MAX];
	uint8_t * p;
	int k;
	printf("Hello\n");
	iface_cnt = load_interface(iface_table);
	printf("interfaces loaded:\n");
	route_cnt = load_route(route_table);
	printf("route rules loaded\n");
//	arp_cnt = load_arp(arp_table);
//	printf("arp cache loaded\n");

	sock_fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (sock_fd < 0) {
		printf("error create raw socket\n");
		return -1;
	}	

	while (1) {
		n_read = recvfrom(sock_fd, buffer, 2048, 0, NULL, NULL);
		if (n_read < 42) {
			printf("error when recv msg \n");
			return -1;
		}
		eth_head = buffer;
		p = eth_head;
		printf("\nMAC address: %.2x:%02x:%02x:%02x:%02x:%02x ==> %.2x:%02x:%02x:%02x:%02x:%02x, n_read = %d", 
			p[6], p[7], p[8], p[9], p[10], p[11], p[0], p[1], p[2], p[3], p[4], p[5], n_read);

		if ( htons( *(uint16_t *)(eth_head+12) ) != 0x0800 ) {
			printf("\nOops! this is not an IP packet: %x\n", 
				htons( *(uint16_t *)(eth_head+12) ) );
			continue;
			// not ip packet: skip this packet
		} 
		
		ip_head = eth_head + 14;
		p = ip_head + 12;
		printf("\nIP: %d.%d.%d.%d ==> %d.%d.%d.%d",
			p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
		ip_proto_type = ip_head[9];
		if (ip_proto_type == IPPROTO_ICMP) {
			printf("\nThis is an ICMP packet\n");
		} else {
			printf("\nOops, this is NOT an ICMP packet: %x\n", ip_proto_type);
			continue;
			// not icmp packet: skip this packet
		}
		
		// now we extract the recv_dst_ip ip address
		// if dst address is myself
		//   print("I get it");
		// else if dst address has the same subnet with src address
		//   keep silent
		// else
		//   check all route items:
		//   find the one with matching gateway (with subnet mask)
		//   then send to the corresponding dst_ip:
		//     refill ether dst address and send

		struct in_addr recv_dst_ip, recv_src_ip;
		char ipbuf[32];
		sprintf(ipbuf, "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
		inet_aton(ipbuf, &recv_src_ip);
		puts(ipbuf);
		sprintf(ipbuf, "%d.%d.%d.%d", p[4], p[5], p[6], p[7]);
		puts(ipbuf);
		inet_aton(ipbuf, &recv_dst_ip);
		printf("recv_src_ip: %s,",inet_ntoa(recv_src_ip));
		printf(" recv_dst_ip: %s\n", inet_ntoa(recv_dst_ip));
		
		// is this dst_addr one of mine?
		int i = -1, route_flag = 0, index = 0;
		for (; index < iface_cnt; ++index) {
			if (ip_equal(&recv_dst_ip, &(iface_table[index].ip_addr))) {
				i = index;
				break;
			}
		}
		if (i != -1) continue; 	// no, this packet is not sent to me
		
		printf("this package is not sent to me\n");

		// is this dst_addr one of my subnets?
		// if yes, then the source ip address must be from another subnet
		//   otherwise this packet shouldn't be send to me
		i = -1;
		for (index = 0; index < iface_cnt; ++index) {
			if ( ip_equal_m(&recv_dst_ip, 
							&(iface_table[index].ip_addr), 
							iface_table[index].ip_netmask )
				) {
				i = index;
				break;
			}
		}
		
		// i == -1: the packet is not to one of my subnets: route needed
		// i != -1: the packet is within one of my subnets: forward needed
		
		struct ether_addr destination_ether_addr;
		struct ether_addr *dst_eth = &destination_ether_addr;
		char * send_iface;
		
		printf("%s\n", (i == -1? "route":"forward"));
		
		if (i == -1) {
			printf("I have to route this packet - src ip and dst ip are NOT in the same subnet\n");		
			// can I found a route item that has the same recv_dst_ip masked by corresponding netmask?
			i = -1;
			for (index = 0; index < route_cnt; ++index) {
				if ( ip_equal_m(
						&recv_dst_ip, 
						&(route_table[index].dst_ip), 
						route_table[index].netmask_bits
					)) {
					i = index;
					break;
				}
			}
			
			// i == -1: no route items available, routing failed
			// i != -1: route item found, go on to next step: get ethernet address
			
			if (i == -1) {
				printf("sorry, no route info found in table\n");
				continue;
			} 
			printf("yes! I can route for that: \n");
			struct route_item * r = route_table + i;
			printf("destination subnet: %s/%d, ",inet_ntoa(r->dst_ip), r->netmask_bits);
			printf("gateway: %s, from interface: %s\n",inet_ntoa(r->gateway), r->interface);
			send_iface = route_table[i].interface; // this is the interface from which I send my packet

			// getting ethernet address of the gateway
			// the code commented out checks the arp cache - obsolete
			// instead we send arp request packets and wait reply
/*	
			i = -1;
			// search for ether addr in arp_table using r->gateway
			for (index = 0; index < arp_cnt; ++index) {
				if ( ip_equal( &r->gateway, &(arp_table[index].ip_addr) ) ) {
					i = index;
					dst_eth = &(arp_table[index].eth_addr);
					break;
				}
			}
*/
			// to get ethernet address, we should know which interface to send ARP request packet from
			int iface_num = -1;
			for (index = 0; index < iface_cnt; ++index) {
				if ( strcmp(iface_table[index].interface, r->interface) == 0) {
					iface_num = index;
					break;
				}
			}
			assert(iface_num != -1);
			
			char gateway_char_buf[16];
			strcpy(gateway_char_buf, inet_ntoa(r->gateway));
			// calling getmac with arp packet information:
			// my ip address, my mac address, target ip address
			//   the function will fill in *dst_eth
			getmac( inet_ntoa(iface_table[iface_num].ip_addr),
					gateway_char_buf,
					(unsigned char*)(&(iface_table[iface_num].eth_addr)),
					(unsigned char*)(dst_eth),
					iface_table[iface_num].interface );		
		} else {
			
			// forward_iface:
			// the interface from which the router FORWARDs the packet
			struct interface_item * forward_iface = iface_table + i;
			send_iface = forward_iface -> interface;
			
			char recv_dst_ip_char_buf[16];
			strcpy(recv_dst_ip_char_buf, inet_ntoa(recv_dst_ip));
			getmac(
				inet_ntoa(forward_iface->ip_addr),
				recv_dst_ip_char_buf,
				(unsigned char*)(&forward_iface->eth_addr),
				(unsigned char*)dst_eth,
				forward_iface->interface
			);
		}

		
		// now I have the dst ether address.
		printf("\"%s\"\n", send_iface);
		struct ether_addr *src_eth = NULL;
		i = -1;
		for (index = 0; index < iface_cnt; ++index) {
			printf("%d, interface: \"%s\"\n", index, iface_table[index].interface);
			if (strcmp(send_iface, iface_table[index].interface) == 0) {
				src_eth = &(iface_table[index].eth_addr);
				break;
			}
		}
		assert(src_eth != NULL);
		
		// fill in!
		p = eth_head;
		uint8_t * reader = dst_eth -> ether_addr_octet;
		memcpy(p, reader, 6);
		p = eth_head + 6;
		reader = src_eth -> ether_addr_octet;
		memcpy(p, reader, 6);
		
		struct sockaddr_ll addr;
		memset(&addr, 0, sizeof(addr));
		struct ifreq ifrq;
		strcpy(ifrq.ifr_name, send_iface);
		ioctl(sock_fd, SIOCGIFINDEX, &ifrq);
		addr.sll_ifindex = ifrq.ifr_ifindex;
		addr.sll_family = PF_PACKET;
		
		int send_result = sendto(sock_fd, eth_head, n_read, 0,
			(struct sockaddr *)&addr, sizeof(addr));
		if (send_result == -1)
			printf("uh-oh. resend wrong.\n");
	}
	return -1;
}
