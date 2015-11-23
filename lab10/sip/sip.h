//文件名: sip/sip.h
//
//描述: 这个文件定义用于SIP进程的数据结构和函数  
//
//创建日期: 2015年

#ifndef NETWORK_H
#define NETWORK_H
#include "nbrcosttable.h"
#include "dvtable.h"
#include "routingtable.h"
#include "../common/seg.h"
//SIP进程使用这个函数连接到本地SON进程的端口SON_PORT
//成功时返回连接描述符, 否则返回-1
int connectToSON();

//这个线程每隔ROUTEUPDATE_INTERVAL时间就发送一条路由更新报文
//在本实验中, 这个线程只广播空的路由更新报文给所有邻居, 
//我们通过设置SIP报文首部中的dest_nodeID为BROADCAST_NODEID来发送广播
void* routeupdate_daemon(void* arg);

//这个线程处理来自SON进程的进入报文
//它通过调用son_recvpkt()接收来自SON进程的报文
//在本实验中, 这个线程在接收到报文后, 只是显示报文已接收到的信息, 并不处理报文
void* pkthandler(void* arg); 

//这个函数终止SIP进程, 当SIP进程收到信号SIGINT时会调用这个函数 
//它关闭所有连接, 释放所有动态分配的内存
void sip_stop();

//这个函数打开端口SIP_PORT并等待来自本地STCP进程的TCP连接.
//在连接建立后, 这个函数从STCP进程处持续接收包含段及其目的节点ID的sendseg_arg_t. 
//接收的段被封装进数据报(一个段在一个数据报中), 
//然后使用son_sendpkt发送该报文到下一跳. 下一跳节点ID提取自路由表.
//当本地STCP进程断开连接时, 这个函数等待下一个STCP进程的连接.
void waitSTCP();
void display(int me, pkt_routeupdate_t *dat);

#endif
