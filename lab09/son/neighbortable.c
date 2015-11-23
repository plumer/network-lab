//文件名: son/neighbortable.c
//
//描述: 这个文件实现用于邻居表的API
//
//创建日期: 2015年
#define _BSD_SOURCE
#include "neighbortable.h"
#include "../common/constants.h"
#include "../topology/topology.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#define max(a, b) ((a)>(b)?(a):(b))

//这个函数首先动态创建一个邻居表. 然后解析文件topology/topology.dat, 填充所有条目中的nodeID和nodeIP字段, 将conn字段初始化为-1.
//返回创建的邻居表.
nbr_entry_t* nt_create(){
	int nbr_num = topology_getNbrNum();
	nbr_entry_t* res = (nbr_entry_t *)malloc(nbr_num*sizeof(nbr_entry_t));

	char myhname[100];
	memset(myhname, 0, 100);
	int result = gethostname(myhname, sizeof(myhname));
	if (result != 0) {
		printf("ERROR: get my hostname failed\n");
		exit(1);
	}

	topology_readfile();
	topo_t *dat = getTopoTable();
	int datNum = getTopoTableNum();
	printf("datnum = %d\n", datNum);
	int i;
	int j = 0;
	for (i = 0; i < datNum; i ++){
		// memcmp比较到长度较大的那个的结尾
		int complen[2] = {max(strlen(dat[i].from_hostname), strlen(myhname)), max(strlen(dat[i].to_hostname), strlen(myhname))};

		if (memcmp(dat[i].from_hostname, myhname, complen[0]) == 0){
			// from == myhostname
			res[j].nodeID = dat[i].to_node;
			res[j].nodeIP = dat[i].to_IP.s_addr;
			res[j].conn = -1;
			j ++;
			putchar('1');
		}
		else if (memcmp(dat[i].to_hostname, myhname, complen[1]) == 0){
			// to == myhostname
			res[j].nodeID = dat[i].from_node;
			res[j].nodeIP = dat[i].from_IP.s_addr;
			res[j].conn = -1;
			j++;
			putchar('2');
		} else putchar('3');
		putchar('\t');
		printf("dat[%d]: fromID \'%s\'= %d, toID \'%s\'= %d, cost = %d\n",
				i, dat[i].from_hostname, dat[i].from_node,
					   dat[i].to_hostname,   dat[i].to_node, dat[i].cost);
	}
	print_pos();
	printf("\tCreate neighbor table ok: %d neighbors\n", j);
	for (i = 0; i < j; i ++){
		//struct in_addr tmp;
		//tmp.in_addr_t = res[i].nodeIP;
		printf("Item %d: node=%d ip=%s\n", i+1, res[i].nodeID, inet_ntoa(*(struct in_addr *)(&(res[i].nodeIP))));
	}
	return res;
}

//这个函数删除一个邻居表. 它关闭所有连接, 释放所有动态分配的内存.
void nt_destroy(nbr_entry_t* nt){
	int nbr_ct = topology_getNbrNum();
	int i;
	for (i = 0; i < nbr_ct; i ++){
		if (nt[i].conn != -1){
			close(nt[i].conn);
			nt[i].conn = -1;
		}
	}
	free(nt);
}

//这个函数为邻居表中指定的邻居节点条目分配一个TCP连接. 如果分配成功, 返回1, 否则返回-1.
int nt_addconn(nbr_entry_t* nt, int nodeID, int conn){
	int nbr_ct = topology_getNbrNum();
	int i;
	for (i = 0; i < nbr_ct; i ++){
		if (nt[i].nodeID == nodeID){
			if (nt[i].conn == -1) {
				nt[i].conn = conn;
				return 1;
			} else {
				printf("ERROR: node has been used\n");
				return -1;
			}
		}
	}
	printf("ERROR: no such node\n");
	return -1;
}
