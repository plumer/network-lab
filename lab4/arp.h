#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <netpacket/packet.h>
#include <net/if.h>
#include <sys/ioctl.h>

void getmac(char * my_ip, // in string format
	char *target_ip, // in string format
	unsigned char *my_mac, // in bytes
	unsigned char *target_mac, // in bytes
	char *send_iface);