#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include "bencode.h"
#include "fileio.h"

#ifndef BTDATA_H
#define BTDATA_H

/**************************************
 * 一些常量定义
********
******************************/
/*
#define HANDSHAKE_LEN 68  // peer握手消息的长度, 以字节为单位
#define BT_PROTOCOL "BitTorrent protocol"
#define INFOHASH_LEN 20
#define PEER_ID_LEN 20
#define MAXPEERS 100
#define KEEP_ALIVE_INTERVAL 3
*/
// 最大已发送请求数
#define MAX_REQUEST 5
// 如果所有的请求都已经发送，就sleep REQUEST_INTERVAL秒再检查
#define REQUEST_INTERVAL 1
// 最大可同时下载分片数
#define MAX_DOWNLOAD_PIECES 10
// 最多有多少个peer连接
#define MAX_PEER 128

// 分片和子分片状态
#define COMPLETE 1	//发送request并收到piece或seed
#define DOWNLOADING 2	//已经发送了request但是还没有收到piece
#define INCOMPLETE 0	//未发送request

#define SUB_PIECE_LEN 16384
#define BT_STARTED 0
#define BT_STOPPED 1
#define BT_COMPLETED 2

#define METADATA_SINGLE 1
#define METADATA_MULTIPLE 2

/**************************************
 * 数据结构
**************************************/
// Tracker HTTP响应的数据部分
typedef struct _tracker_response {
	int size;       // B编码字符串的字节数
	char* data;     // B编码的字符串
} tracker_response;

typedef struct _file_info {
	unsigned long long length;// 文件长度，以字节为单位
	char * path;		// 文件名，动态分配
} file_info_t;

// 元信息文件中包含的数据
typedef struct _torrentmetadata {
	unsigned char info_hash[20];		// torrent的info_hash值(info键对应值的SHA1哈希值)
	char* announce;			// tracker的URL，动态分配
	int length;			// 文件长度, 以字节为单位
	char* name;				// 种子名
	int mode;				// METADATA_SINGLE or METADATA_MULTIPLE
	int num_files;
	file_info_t * file_info;// 动态分配，长度由num_files指定
	int piece_len;			// 每一个分片的字节数
	int num_pieces;			// 分片数量
	char* pieces;			// 针对所有分片的20字节长的SHA1哈希值连接而成的字符串，根据num_pieces进行动态分配
} torrentmetadata_t;


// 包含在announce url中的数据(例如, 主机名和端口号)
typedef struct _announce_url_t {
	char* hostname;
	int port;
} announce_url_t;

// 由tracker返回的响应中peers键的内容
typedef struct _peerdata {
	char id[21]; // 20用于null终止符
	int port;
	char *ip; // Null终止
} peerdata;

// 包含在tracker响应中的数据
typedef struct _tracker_data {
	int interval;
	int numpeers;
	peerdata* peers;
} tracker_data;

typedef struct _tracker_request {
	int info_hash[5];
	char peer_id[20];
	int port;
	int uploaded;
	int downloaded;
	int left;
	char ip[16]; // 自己的IP地址, 格式为XXX.XXX.XXX.XXX, 最后以'\0'结尾
} tracker_request;

// 针对到一个peer的已建立连接, 维护相关数据
typedef struct _peer_t {
	int sockfd;
	int is_seed;		// 我是种子=1, 不是种子=0
	int am_choking;        	// 我阻塞peer
	int am_interested;     	// 我对peer感兴趣
	int peer_choking;       // peer阻塞我
	int peer_interested;  	// peer对我感兴趣
	char peer_id[20];  	// 对方的peer id
	char my_peer_id[20];	// 我的peer id
	int request_piece;	// 当前我请求下载的分片序号
	char peer_ip[16];
	clock_t timestamp;	// 时间戳，用于keep alive
	struct _peer_t *next;	// 使用一个链表来保存所有的连接，所以需要这个域
} peer_t;

// piece消息的消息头
typedef struct _piece_head {
	uint32_t length_prefix;
	uint8_t id;
	uint32_t piece_index;
	uint32_t begin;
} piece_h;

// piece分片的状态，记录如下信息：
// 分片是否下载完成
// 如果分片没有下载完成：
//	哪些peer拥有这个分片【最多记录5个
//	这个分片的各个子分片是否已经下载完成
// 连接断开时、收到bitfield消息时、收到have消息时、需要更新这个数据结构
typedef struct _piece_status {
	int complete;		//分片下载完成=1，没有下载完成=0
	int owner_num;		//现在存了几个socket，必须<=5
	int owners[MAX_REQUEST];		//哪些peer拥有这个分片，初始化为-1，实际记录的是socket
	uint32_t sub_len;	//子分片长度
	uint32_t sub_num;	//子分片数目
	int *sub_status;	//子分片状态
	char *data;		//分片临时存放的位置，仅当req_piece_index里面有当前分片的index的时候，这个数组才不是空，
				//分片一旦下载完成，data就置空
	pthread_mutex_t	piece_mutex;	//互斥变量
} piece_status_t;

typedef struct _handshake_msg {
	uint8_t pstrlen;
	uint8_t pstr[19];
	uint8_t reserved[8];
	uint8_t info_hash[20];
	uint8_t peer_id[20];
} handshake_msg_t;

typedef struct _bitfield {
	uint32_t len;
	uint8_t id;
	uint8_t *bitfield;
} bitfield_t;
/**************************************
 * 全局变量 
**************************************/
char	g_my_ip[128]; // 格式为XXX.XXX.XXX.XXX, null终止
int		g_peerport; // peer监听的端口号
int		g_infohash[5]; // 要共享或要下载的文件的SHA1哈希值, 每个客户端同时只能处理一个文件
char	g_my_id[20];

int g_done; // 表明程序是否应该终止

torrentmetadata_t* g_torrentmeta;
char *	g_filedata;      // 文件的实际数据【这个不再使用
FILE * g_file;		//下载下来的文件或者做种的文件
int		g_filelen;
int		g_num_pieces;
char *	g_filename;
char *  g_filelocation;

char	g_tracker_ip[16]; // tracker的IP地址, 格式为XXX.XXX.XXX.XXX(null终止)
int		g_tracker_port;
tracker_data * g_tracker_response;

// 这些变量用在函数make_tracker_request中, 它们需要在客户端执行过程中不断更新.
int		g_uploaded;	// 响应一个request之后进行更新
int		g_downloaded;	// 收到一个分片之后进行更新
int		g_left;		// 收到一个分片之后进行更新

// 分片的状态，根据num_pieces进行动态分配
// 在第一个没有断点续传的版本中，如果是种子，就设置为所有分片完成（1），如果不是种子，就设置为所有分片未完成（0）
// 在有断点续传的版本中，如果是种子，就设置为所有分片完成。
// 如果不是种子，先检查是否有未下载完成的文件，如果有，读取分片状态信息，如果没有，设置所有分片状态为未完成
piece_status_t *g_pieces_status;

// 全局最多同时发送 MAX_REQUEST 个子分片请求
// 初始化和recv一个piece的时候，置为-1
pthread_mutex_t request_mutex;
pthread_cond_t request_cond;
int g_sent_num;	//已经发送的req但是没有收到piece的请求数
int g_sock_sent[MAX_REQUEST];	//当前这么多请求发送的socket

pthread_mutex_t req_piece_mutex;
int req_piece_num;	//已经开始下载，但是没有下载完成的分片数
int req_piece_index[MAX_DOWNLOAD_PIECES];	//当前所正在下载的分片的index，初始化为-1

file_progress_t *g_files_progress;
#endif
