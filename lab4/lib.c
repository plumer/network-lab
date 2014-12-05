#include "lib.h"

/********** ip subnet mask **********/

uint32_t subnet(struct in_addr * ip_addr, uint32_t mask_bit) {
	return *(uint32_t *)ip_addr & ((1 << mask_bit)-1);
}

/*************** data structure definition ****************/



/***************** init loader functions ******************/

int load_interface(struct interface_item * iface_table) {
	char line_buf[256];
	FILE * itf_file;
	char * p;
	int i_c = 0;
	itf_file = fopen("../configuration/interface", "r");
	if ( !itf_file ) {
		printf("Couldn't open interface file\n");
		return -1;
	}
	do {
		memset(line_buf, 0, sizeof(line_buf));
		fgets( line_buf, 255, itf_file );
		if (!*line_buf) break;
		p = strtok(line_buf, " ");
		strncpy(iface_table[i_c].interface, p, 7);
		p = strtok(NULL, " ");
		inet_aton(p, &(iface_table[i_c].ip_addr));
		p = strtok(NULL, " ");
		iface_table[i_c].ip_netmask = atoi(p);
		p = strtok(NULL, " ");
		ether_aton_r(p, &(iface_table[i_c].eth_addr));
		i_c++;
	} while ( !feof(itf_file) ) ;
//	puts("out of while loop");
	fclose(itf_file);
	return i_c;
}

int load_route(struct route_item * route_table) {
	char line_buf[256];
	FILE * route_file;
	char * p;
	int r_c = 0;
	route_file = fopen("../configuration/routeitem", "r");
	if( !route_file ) {
		printf("Couldn't open route item file\n");
		return -1;
	}

	while (!feof(route_file)) {
		memset(line_buf, 0, sizeof(line_buf));
		fgets( line_buf, 255, route_file );
		if (!*line_buf) break;
		p = strtok(line_buf, " \n");
		inet_aton(p, &(route_table[r_c].dst_ip));
		p = strtok(NULL, " \n");
		inet_aton(p, &(route_table[r_c].gateway));
		p = strtok(NULL, " \n");
		route_table[r_c].netmask_bits = atoi(p);
		p = strtok(NULL, " \n");
		strncpy(route_table[r_c].interface, p, 7);
		r_c++;
	}
	return r_c;
}

int load_arp(struct arp_item * arp_table) {
	char line_buf[256];
	FILE * arp_file;
	char * p;
	int a_c = 0;

	arp_file = fopen("../configuration/barpcache", "r");
	if ( !arp_file ) {
		printf("Couldn't open arp cache file\n");
		return -1;
	}

	while (!feof(arp_file)) {
		memset(line_buf, 0, sizeof(line_buf));
		fgets( line_buf, 255, arp_file );
		if (!*line_buf) break;
		p = strtok(line_buf, " \n");
		inet_aton(p, &(arp_table[a_c].ip_addr));
		p = strtok(NULL, " \n");
		ether_aton_r(p, &(arp_table[a_c].eth_addr));
		p = strtok(NULL, " \n");
		strncpy(arp_table[a_c].interface, p, strlen(p));
		a_c++;
	}
	return a_c;
}

// return non-zero if two struct in_addr variables have the same value
int ip_equal(struct in_addr *ip1, struct in_addr *ip2) {
	return (*(uint32_t*)ip1) == (*(uint32_t*)ip2);
}

// return non-zero if two struct in_addr have the same value masked by mask represented in bits
int ip_equal_m(struct in_addr *ip1, struct in_addr *ip2, uint32_t mask) {
	uint32_t i1 = *(uint32_t *)ip1;
	uint32_t i2 = *(uint32_t *)ip2;
	uint32_t mask_value = (1 << mask) - 1;
	return (i1 & mask_value) == (i2 & mask_value);
}

// return non-zero if two ethernet addresses are the same
int eth_equal(struct ether_addr *eth1, struct ether_addr *eth2) {
	const char * c1 = (const char *)eth1;
	const char * c2 = (const char *)eth2;
	return ( strncmp(c1, c2, 6) == 0);
}