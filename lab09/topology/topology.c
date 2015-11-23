//文件名: topology/topology.c
//
//描述: 这个文件实现一些用于解析拓扑文件的辅助函数 
//
//创建日期: 2015年
#define _BSD_SOURCE
#include "topology.h"
#include "../common/constants.h"
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>


topo_t topo_table[TOPO_MAX];
int topo_count = 0;
int linear_search(int *array, int n, int key) {
	int res = -1, i = 0;
	for (; i < n; ++i) {
		if (array[i] == key) res = i;
	}
	return res;
}

int topology_readfile() {
	print_pos();
	FILE *f;
	//f = fopen("/home/b121220130/lab09/topology/topology.dat", "r");
	f = fopen("topology/topology.dat", "r");
	if (!f) {
		printf("cannot open file\n");
		return -1;
	}
	topo_count = 0;
	char line[256];
	while (!feof(f)) {
		print_pos();putchar('\n');
		if (fgets(line, sizeof(line), f) == NULL) break;
		print_pos();putchar('\n');
		topo_t * topo_p = topo_table + topo_count;
		memset(topo_p, 0, sizeof(topo_t));
		char * p = strtok(line, " ");
		strncpy(topo_p -> from_hostname, p, 32-1);
		p = strtok(NULL, " ");
		strncpy(topo_p -> to_hostname, p, 32-1);
		p = strtok(NULL, " ");
		topo_p -> cost = atoi(p);

		char ip[128];
		memset(ip, 0, sizeof(ip));
		struct hostent * h_ent;
		
		sprintf(ip, "%s.nju.edu.cn", topo_p -> from_hostname);
		h_ent = gethostbyname(ip);
		memcpy(&topo_p -> from_IP, h_ent -> h_addr_list[0], sizeof(in_addr_t));
		memset(ip, 0, sizeof(ip));
		sprintf(ip, "%s.nju.edu.cn", topo_p -> to_hostname);
		h_ent = gethostbyname(ip);
		memcpy(&topo_p -> to_IP, h_ent ->h_addr_list[0], sizeof(in_addr_t));

		topo_p -> from_node = topology_getNodeIDfromname(topo_p -> from_hostname);
		topo_p -> to_node = topology_getNodeIDfromname(topo_p -> to_hostname);

		topo_count++;
	}
	if (topo_count >= TOPO_MAX) return -1;
	return 0;
}

// 这个函数返回topology.dat文件的所有内容
topo_t *getTopoTable(){
	if (topo_count == 0) topology_readfile();
	return topo_table;
}
// 这个函数返回topology.dat的条目数
int getTopoTableNum(){
	if (topo_count == 0) topology_readfile();
	return topo_count;
}

//这个函数返回指定主机的节点ID.
//节点ID是节点IP地址最后8位表示的整数.
//例如, 一个节点的IP地址为202.119.32.12, 它的节点ID就是12.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromname(char* hostname) 
{
	static char hname[128];
	static struct hostent * h_ent;
	sprintf(hname,"%s.nju.edu.cn", hostname);
	h_ent = gethostbyname(hname);
	struct in_addr * host_addr = (struct in_addr *)(h_ent -> h_addr_list[0]);
	return topology_getNodeIDfromip(host_addr);
}

//这个函数返回指定的IP地址的节点ID.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromip(struct in_addr* addr)
{
	// printf("%s", inet_ntoa(*addr));
	const char *p = (const char *) addr;
	return (unsigned char)p[3];
}

//这个函数返回本机的节点ID
//如果不能获取本机的节点ID, 返回-1.
int topology_getMyNodeID()
{
	static char hname[128];
	memset(hname, 0, sizeof(hname));
	static struct hostent * h_ent;
	gethostname(hname, sizeof(hname));

	// h_ent = gethostent();
	h_ent = gethostbyname(hname);
	struct in_addr * host_addr = (struct in_addr *)(h_ent -> h_addr_list[0]);
	return topology_getNodeIDfromip(host_addr);
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回邻居数.
int topology_getNbrNum()
{
	if (topo_count == 0) topology_readfile();
	int own_id = topology_getMyNodeID();
	int i = 0, neighborcount = 0;
	for (; i < topo_count; ++i) {
		if (topo_table[i].from_node == own_id ||
			topo_table[i].to_node == own_id)
			neighborcount++;
	}
	return neighborcount;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回重叠网络中的总节点数.
int topology_getNodeNum()
{
	if (topo_count == 0) topology_readfile();
	int * node_array = (int *)malloc(sizeof(int) * TOPO_MAX);
	memset(node_array, 0, sizeof(int) * TOPO_MAX);
	int node_count = 0, i = 0;
	for (; i < topo_count; ++i) {
		int n = topo_table[i].from_node;
		int s = linear_search(node_array, node_count, n);
		if (s < 0) node_array[node_count++] = n;
		n = topo_table[i].to_node;
		s = linear_search(node_array, node_count, n);
		if (s < 0) node_array[node_count++] = n;
	}
	return node_count;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含重叠网络中所有节点的ID. 
int* topology_getNodeArray()
{
	if (topo_count == 0) topology_readfile();
	int * node_array = (int *)malloc(sizeof(int) * TOPO_MAX);
	memset(node_array, 0, sizeof(int) * TOPO_MAX);
	int node_count = 0, i = 0;
	for (; i < topo_count; ++i) {
		int n = topo_table[i].from_node;
		int s = linear_search(node_array, node_count, n);
		if (s < 0) node_array[node_count++] = n;
		n = topo_table[i].to_node;
		s = linear_search(node_array, node_count, n);
		if (s < 0) node_array[node_count++] = n;
	}
	return node_array;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含所有邻居的节点ID.  
int* topology_getNbrArray()
{
	if (topo_count == 0) topology_readfile();
	int neighborcount = topology_getNbrNum();
	int * neighbor_array = (int *)malloc(sizeof(int) * neighborcount);
	int own_id = topology_getMyNodeID();
	int i = 0, c = 0, neighbor_id = -1;
	for (; i < topo_count; ++i) {
		if (topo_table[i].from_node == own_id) 
			neighbor_id = topo_table[i].to_node;
		else if (topo_table[i].to_node == own_id) 
			neighbor_id = topo_table[i].from_node;
		else continue;
		int s = linear_search(neighbor_array, c, neighbor_id);
		if (s < 0) neighbor_array[c++] = neighbor_id;
	}
	assert(c == neighborcount);
	return neighbor_array;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回指定两个节点之间的直接链路代价. 
//如果指定两个节点之间没有直接链路, 返回INFINITE_COST.
unsigned int topology_getCost(int fromNodeID, int toNodeID)
{
	if (topo_count == 0) topology_readfile();
	int i = 0;
	for (; i < topo_count; ++i) {
		topo_t * p = topo_table + i;
		if ( (p -> from_node == fromNodeID && p -> to_node == toNodeID) ||
			 (p -> to_node == fromNodeID && p -> from_node == toNodeID) ) {
			return p -> cost;
		}
	}
	return INFINITE_COST;
}
