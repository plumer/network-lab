
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../common/constants.h"
#include "../topology/topology.h"
#include "dvtable.h"

int dventry_setcost(dv_entry_t *dve, int dest_node, int cost);
int dventry_getcost(dv_entry_t *dve, int dest_node);

//这个函数动态创建距离矢量表.
//距离矢量表包含n+1个条目, 其中n是这个节点的邻居数,剩下1个是这个节点本身.
//距离矢量表中的每个条目是一个dv_t结构,它包含一个源节点ID和一个有N个dv_entry_t结构的数组, 其中N是重叠网络中节点总数.
//每个dv_entry_t包含一个目的节点地址和从该源节点到该目的节点的链路代价.
//距离矢量表也在这个函数中初始化.从这个节点到其邻居的链路代价使用提取自topology.dat文件中的直接链路代价初始化.
//其他链路代价被初始化为INFINITE_COST.
//该函数返回动态创建的距离矢量表.
dv_t* dvtable_create()
{
	int nbr_count = topology_getNbrNum();
	int * nbr_array = topology_getNbrArray();
	int node_count = topology_getNodeNum();
	int * node_array = topology_getNodeArray();
	dv_t * dvtable = (dv_t *)malloc( sizeof(dv_t) * (nbr_count + 1) );
	memset(dvtable, 0, sizeof(dv_t) * (nbr_count+1) );

	int i, j;
	// the first dv_t has nodeID of itself.
	dvtable[0].nodeID = topology_getMyNodeID();
	dvtable[0].dvEntry = (dv_entry_t *)malloc(sizeof(dv_entry_t)*(node_count));
	memset(dvtable[0].dvEntry, 0, sizeof(dv_entry_t)*(node_count));
	for (j = 0; j < node_count; ++j) {
		dvtable[0].dvEntry[j].nodeID = node_array[j];
		dvtable[0].dvEntry[j].cost = topology_getCost(dvtable[0].nodeID, node_array[j]);
		if (dvtable[0].nodeID == dvtable[0].dvEntry[j].nodeID)
			dvtable[0].dvEntry[j].cost = 0;

	}


	
	for (i = 1; i < nbr_count+1; ++i) {
		dvtable[i].nodeID = nbr_array[i-1];
		dvtable[i].dvEntry = (dv_entry_t *)malloc( sizeof(dv_entry_t)*node_count);
		memset(dvtable[i].dvEntry, 0, sizeof(dv_entry_t)*node_count);
		for (j = 0; j < node_count; ++j) {
			dvtable[i].dvEntry[j].nodeID = node_array[j];
			dvtable[i].dvEntry[j].cost = INFINITE_COST;
		}
	}
	return dvtable;
}

//这个函数删除距离矢量表.
//它释放所有为距离矢量表动态分配的内存.
void dvtable_destroy(dv_t* dvtable)
{
	int i;
	int nbr_count = topology_getNbrNum();
	for (i = 0; i < nbr_count + 1; ++i) {
		free(dvtable[i].dvEntry);
	}
	free(dvtable);
	return;
}

//这个函数设置距离矢量表中2个节点之间的链路代价.
//如果这2个节点在表中发现了,并且链路代价也被成功设置了,就返回1,否则返回-1.
int dvtable_setcost(dv_t* dvtable,int fromNodeID,int toNodeID, unsigned int cost)
{
	int i, j;
	int nbr_count = topology_getNbrNum();
	int node_count = topology_getNodeNum();
	for (i = 0; i < nbr_count + 1; ++i) {
		if (dvtable[i].nodeID == fromNodeID) break;
	}
	if (i == nbr_count + 1 || dvtable[i].nodeID != fromNodeID) 
		return -1;
	for (j = 0; j < node_count; ++j) {
		if (dvtable[i].dvEntry[j].nodeID == toNodeID) {
			dvtable[i].dvEntry[j].cost = cost;
			return 1;
		}
	}
	return -1;
}

//这个函数返回距离矢量表中2个节点之间的链路代价.
//如果这2个节点在表中发现了,就返回链路代价,否则返回INFINITE_COST.
unsigned int dvtable_getcost(dv_t* dvtable, int fromNodeID, int toNodeID)
{
	int i, j;
	int nbr_count = topology_getNbrNum();
	int node_count = topology_getNodeNum();
	for (i = 0; i < nbr_count + 1; ++i) {
		if (dvtable[i].nodeID == fromNodeID) break;
	}
	if (i == nbr_count + 1 || dvtable[i].nodeID != fromNodeID) 
		return INFINITE_COST;
	for (j = 0; j < node_count; ++j) {
		if (dvtable[i].dvEntry[j].nodeID == toNodeID)
			return dvtable[i].dvEntry[j].cost;
	}
	return INFINITE_COST;
}

//这个函数打印距离矢量表的内容.
void dvtable_print(dv_t* dvtable)
{
	int i, j;
	int nbr_count = topology_getNbrNum();
	int node_count = topology_getNodeNum();

	for (i = 0; i < nbr_count + 1; ++i) {
		printf("from node %d:\t", dvtable[i].nodeID);
		for (j = 0; j < node_count; ++j) {
			printf("[ #%3d = %3d ]", 
				dvtable[i].dvEntry[j].nodeID, dvtable[i].dvEntry[j].cost);
		}
		putchar('\n');
	}
	return;
}

int route_dvt_update(nbr_cost_entry_t *nct, dv_t *dvt, routingtable_t * rtt,
	pkt_routeupdate_t *data, int updater, pthread_mutex_t *dv_mutex) {
	int my_node_id = topology_getMyNodeID();
	int nbr_count = topology_getNbrNum();
	int entry_num = data -> entryNum;
	int i;

	pthread_mutex_lock(dv_mutex);
	dv_entry_t * my_dv_entry = NULL;
	for (i = 0; i < nbr_count + 1; ++i) {
		if (dvt[i].nodeID == my_node_id)
			my_dv_entry = dvt[i].dvEntry;
	}
	if (my_dv_entry == NULL) {
		printf("panic: cannot find my own dist-vec entry\n");
	} else {
		int curr_fmtu = dvtable_getcost(dvt, my_node_id, updater);
		for (i = 0; i < entry_num; ++i) {
			if ( curr_fmtu + data -> entry[i].cost < 
				dventry_getcost(my_dv_entry, data -> entry[i].nodeID)) {
				dventry_setcost(my_dv_entry, data -> entry[i].nodeID, 
					curr_fmtu + data -> entry[i].cost);
				routingtable_setnextnode(rtt, data -> entry[i].nodeID, updater);
			}
		}
	}


	dv_entry_t * updater_dv_entry = NULL;
	for (i = 0; i < nbr_count + 1; ++i) {
		if (dvt[i].nodeID == updater)
			updater_dv_entry = dvt[i].dvEntry;
	}
	if (updater_dv_entry == NULL) {
		printf("panic: cannot find updater's dist-vec entry\n");
	} else {
		for (i = 0; i < entry_num; ++i) {
			if (dventry_getcost(updater_dv_entry, data -> entry[i].nodeID) >
				data -> entry[i].cost) 
				dventry_setcost(updater_dv_entry, data -> entry[i].nodeID, data -> entry[i].cost);
		}
	}
	pthread_mutex_unlock(dv_mutex);
	dvtable_print(dvt);
	return 1;
}

int dventry_getcost(dv_entry_t *dve, int dest_node) {
	int node_num = topology_getNodeNum();
	int i = 0;
	for (i = 0; i < node_num; ++i) {
		if (dve[i].nodeID == dest_node)
			return dve[i].cost;
	}
	return INFINITE_COST;
}

int dventry_setcost(dv_entry_t *dve, int dest_node,  int cost) {
	int node_num = topology_getNodeNum();
	int i = 0;
	for (i = 0; i < node_num; ++i) {
		if (dve[i].nodeID == dest_node) {
			dve[i].cost = cost;
			return 1;
		}
	}
	printf("panic: %s dest_node not found.\n", __FUNCTION__);
	return -1;
}