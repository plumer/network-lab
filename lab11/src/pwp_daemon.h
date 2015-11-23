#ifndef PWP_DAEMON_H_
#define PWP_DAEMON_H_

#include "util.h"
#include "btdata.h"
#include <time.h>

#define ALIVE_INTERVAL 120
#define ALIVE_CHECK 5
// 这个函数malloc一个peer_t，然后返回
peer_t *new_peer_t();

// 这个函数关闭一个peer_t，注意，当前我们只允许两台机器之间建立一个p2p连接
void close_conn(peer_t *conn);

// 这个线程函数遍历tcb表，发现一个连接timeout就断开这个连接
void *peer_timeout_checker(void *arg);

// 这个线程函数接收并处理来自peer的所有信息
void *peer_handler(void *conn);

// 这个线程函数keep alive一个P2P连接
void *keep_alive(void *conn);

// 整个进程的一个线程，负责发送request请求，当我是种子的时候，不会创建这个线程
void request_piece(peer_t *tcb, int count);

// peer告诉我他又多了piece_index这个分片，我更新我的pieces info
void peerHave(peer_t *conn, uint32_t piece_index);

// 这个函数把当前tcb[index]正在传输的文件的第piece_index块的从begin开始的length字节的数据填充到msg里去
// 因为不确定msg的长度，所以使用动态数组当场分配
void fillMsg(peer_t *conn, uint8_t *msg, uint32_t piece_index, uint32_t begin, uint32_t length);

// 这个函数使用peer发给我的bitfield来初始化pieces_info
// 如果字节bf_len字节数不对返回-1，如果空闲bit不是0也返回-1
// 其他正常情况都返回1
int peerBitfield(peer_t *info, uint8_t *bf, uint32_t bf_len);

// 这个函数把收到的数据block存到这个连接正在传输的文件的对应位置
void recvPiece(peer_t *info, uint32_t piece_index, uint32_t begin, uint32_t length, uint8_t *block);

// 这个函数获取我自己的peer_id
char *get_my_peer_id();

// 这个函数进行hash校验,校验通过返回1，不通过返回-1
int check_info_hash(char *info_hash);
// 这个函数进行id校验，校验通过返回1，不通过返回-1
int check_peer_id(char *peer_id);
// 检查已有的peer_t中是否有IP地址为参数中ip者，有则返回1，没有返回0
int dup_peer_ip(const char * ip);

//这个函数初始化g_pieces_status;
void init_pieces_status(uint8_t *resumption, char is_seed);

// 这个函数查找当前的g_sock_sent，是否当前的socket且当前socket不为0，如果存在返回1，否则返回0
int is_req_socket(int socket);

// 这个函数填充一个握手信息
void fill_handshake(uint8_t *hdsk_msg);

// 这个函数在退出的时候关闭所有的连接，然后释放所有申请的内存
void close_all_connections();

// 这个函数选择一个下载的分片
int select_piece(peer_t *tcb);

//  这个函数判断一个分片是不是可以通过一个peer来下载
int is_piece_socket(int socket, int index);

// all send function
void send_chock(peer_t *tcb);
void send_unchock(peer_t *tcb);
void send_interested(peer_t *tcb);
void send_not_interested();
void send_have(int piece_index);
void send_bitfield(peer_t *tcb);
void send_request(peer_t *tcb, uint32_t begin);
void send_piece(peer_t *tcb, uint32_t length, uint32_t piece_index, uint32_t begin);
int is_file_complete();

int handshake(int sockfd, uint8_t *hdsk_msg);
#endif
