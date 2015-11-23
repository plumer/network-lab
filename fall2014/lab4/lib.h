#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <linux/if_ether.h>
#include <arpa/inet.h>
#include <netinet/ether.h>
#define M 32
struct route_item {
	struct in_addr dst_ip;
	struct in_addr gateway;	// next hop address
	uint32_t netmask_bits;	// e.g. 24 stands for 255.255.255.0
	char interface[8];		// which interface to send from
};

struct arp_item {
	struct in_addr ip_addr;	
	struct ether_addr eth_addr;
	char interface[8];		// seems somehow redundant
};

struct interface_item {
	char interface[8];
	struct in_addr ip_addr;
	uint32_t ip_netmask;	// also in bits
	struct ether_addr eth_addr;
};


uint32_t subnet(struct in_addr * ip_addr, uint32_t mask_bit);
int load_interface(struct interface_item * iface_table);
int load_route(struct route_item * route_table);
int load_arp(struct arp_item * arp_table);

int ip_equal(struct in_addr *ip1, struct in_addr * ip2);
int ip_equal_m(struct in_addr *ip1, struct in_addr *ip2, uint32_t mask);
int eth_equal(struct ether_addr *eth1, struct ether_addr *eth2);
/*
int search_route_by_dst(struct in_addr * ip_addr);
int search_arp_by_ip(struct in_addr * ip_addr);
int search_interface_by_name(const char *);
int search_interface_by_ip(struct in_addr * ip_addr);
*/