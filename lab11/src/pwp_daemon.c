#include "pwp_daemon.h"
#include "fileio.h"
#include <signal.h>
#include <pthread.h>
#include <stdint.h>
#include <assert.h>
// 保存所有的连接
peer_t *conns = NULL;
// 连接的计数
int conns_num = 0;

// 首先main进程为两个机器建立TCP的P2P连接，然后使用这个函数获得一个peer_t
peer_t *new_peer_t(){
	// print conns
	peer_t *trace = conns;
	printf("current conns is :\n");
	while (trace != NULL){
		printf("sockfd=%d ip=%s\n", trace->sockfd, trace->peer_ip);
		trace = trace->next;
	}

	// 初始化这个连接的所有信息
	peer_t *peer_conn = (peer_t *)malloc(sizeof(peer_t));
	peer_conn->request_piece = -1;
	peer_conn->timestamp = clock();
	peer_conn -> is_seed = -1;
	peer_conn -> sockfd = -1;
	peer_conn -> am_choking = 1;
	peer_conn -> am_interested = 0;
	peer_conn -> peer_choking = 1;
	peer_conn -> peer_interested = 0;
	memset(peer_conn -> peer_id, 0, 20);
	memset(peer_conn -> my_peer_id, 0, 20);
	memset(peer_conn -> peer_ip, 0, 16);
	peer_conn -> timestamp = clock();

	// 这边两句话是把这个peer_t插入到链表里,头部插入
	peer_conn -> next = conns;
	conns = peer_conn;
	return peer_conn;
}

// 关闭一个连接需要做下面两件事：
// 查找g_pieces_status，减少owner_num和owners
// 减少g_sent_num和g_sock_sent
// 根据g_piece_index里的分片piece index如果这个分片的peers数目等于0，就从正在下载的分片列表里移除
// 释放peer_t的空间
void close_conn(peer_t *conn){
	// 查找g_pieces_status, 减少owner_num和owners
	int i;	
	for (i = 0; i < g_torrentmeta->num_pieces; i++){	// 从所有的piece
		int j;
		for (j = 0; j < MAX_REQUEST; j++){							//这个piece的所有连接
			if (g_pieces_status[i].owners[j] == conn->sockfd){	//找到了
				g_pieces_status[i].owners[j] = -1;
				g_pieces_status[i].owner_num--;
			}
		}
	}

	// 释放peer_t的空间
	if (conns == conn) {	// 如果是链表的第一个节点
		conns = conn->next;
		close(conn->sockfd);
		if (conn) free(conn);
		conn = NULL;
	}
	else{	//如果不是链表的第一个节点
		peer_t *trace = conns;
		// 遍历链表找到这个连接
		while (trace != NULL && trace->next != conn){
			trace = trace->next;
		}
		// 没有找到
		if (trace == NULL) {
			printf("Error close conn: connection not found\n");
			return;
		}
		trace->next = conn->next;
		// 关闭连接
		close(conn->sockfd);
		// 释放内存
		if (conn) free(conn);
		conn = NULL;
	}
}

// 这个线程函数负责发送keep alive信息
void *keep_alive(void *conn){
	peer_t *peer_conn = (peer_t *)conn;
	uint8_t length_prefix[5];
	// 全部填充为0
	memset(length_prefix, 0, 5);
	while (1){
		printf("keep_alive for socket=%d: sent\n", peer_conn->sockfd);
		int result = send(peer_conn->sockfd, length_prefix, 4, 0);
		if (result == -1) {
			printf("keep alive disconnected\n");
			break;
		}
		sleep(ALIVE_INTERVAL);
	}
	pthread_exit(NULL);
}

// 这个线程检查所有的连接是否超时
void *peer_timeout_checker(void *arg){
	while (1){
		int i = 0;
		//获取系统时间
		clock_t cur_time = clock();
		peer_t *trace = conns;
		// 遍历所有的连接
		while (trace != NULL){
			//如果超时
			if (cur_time - trace->timestamp > ALIVE_INTERVAL*1000){
				printf("Connect socket=%d timeout, close\n", trace->sockfd);
				///关闭trace
				//先从链表里摘除
				peer_t *next = trace->next;
				//然后关闭
				close_conn(trace);
				trace = next;
				continue;
			}
			trace = trace->next;
		}
		// 每隔一段时间检查一次
		sleep(ALIVE_CHECK);
	}
}

// 这个线程负责接收peer发来的所有msg
void *peer_handler(void *conn){
	peer_t *peer_conn = (peer_t *)conn;
	uint32_t length_prefix, piece_index, begin, length;
	uint8_t id;
	int result;
	printf("peer_handler for socket=%d created\n", peer_conn->sockfd);
	// 无限循环
	while (1){
		result = 0;
		while (result != 4){
			memset(&length_prefix, 0, 4);
			result = readn(peer_conn->sockfd, (char *)&length_prefix, 4);
			if (result > 0) printf("recv length_prefix = %02x %02x %02x %02x", ((char *)&length_prefix)[0], ((char *)&length_prefix)[1], ((char *)&length_prefix)[2], ((char *)&length_prefix)[3]);
			if (result == -1) break;
		}
		length_prefix = ntohl(length_prefix);			// 网络字节序

		if (result != 4) {
			print_pos();
			printf("peer handler readn error, result=%d\n", result);
			return NULL;
		}

		// 根据length_prefix判断消息的类型
		switch(length_prefix){
			case 0: // keep alive消息，更新 timestamp
				printf("recv keep alive msg, refresh timestamp\n");
				peer_conn->timestamp = clock();
				break;
			case 1: // choke、unchock、interested、not interested
				printf("recv control msg\n");
				result = readn(peer_conn->sockfd, (char *)&id, 1);
				printf("id=%d, result=%d\n", id, result);
				if (result != 1) {
					print_pos();
					printf("peer handler recvlinef disconnect\n");
					return NULL;
				}
				switch(id){
					case 0: peer_conn->peer_choking = 1; break;// chock消息
					case 1: peer_conn->peer_choking = 0; 			// unchock
						printf("recv unchock, send request\n");
						request_piece(peer_conn, 5);
						break;
					case 2: {	// interested
						peer_conn->peer_interested = 1; 
						send_unchock(peer_conn);
						printf("recv insterested, send unchoke\n");
						}
						break;
					case 3: peer_conn->peer_interested = 0; break;// not interested
				}
				break;
			case 5: // have
				printf("recv have msg\n");
				// 接收id
				result = readn(peer_conn->sockfd, (char *)&id, 1);
				if (result != 1) {
					print_pos();
					printf("peer handler recvlinef disconnect\n");
					return NULL;
				}
				// 接收分片号
				result = readn(peer_conn->sockfd, (char *)&piece_index, 4);
				if (result != 4) {
					print_pos();
					printf("peer handler recvlinef disconnect\n");
					return NULL;
				}
				piece_index = ntohl(piece_index);
				// 更新piece信息
				peerHave(peer_conn, piece_index);
				break;
			case 13: // request
				printf("recv request msg\n");
				result = readn(peer_conn->sockfd, (char *)&id, 1);
				if (result != 1) {
					print_pos();
					printf("peer handler recvlinef disconnect\n");
					return NULL;
				}
				result = readn(peer_conn->sockfd, (char *)&piece_index, 4);
				if (result != 4) {
					print_pos();
					printf("peer handler recvlinef disconnect\n");
					return NULL;
				}
				result = readn(peer_conn->sockfd, (char *)&begin, 4);
				if (result != 4) {
					print_pos();
					printf("peer handler recvlinef disconnect\n");
					return NULL;
				}
				result = readn(peer_conn->sockfd, (char *)&length, 4);
				if (result != 4) {
					print_pos();
					printf("peer handler recvlinef disconnect\n");
					return NULL;
				}

				piece_index = ntohl(piece_index);
				begin = ntohl(begin);
				length = ntohl(length);
				printf("peer %d request piece %d begin=%d length=%d\n", peer_conn->sockfd, piece_index, begin, length);
				// 长度错误，关闭连接
				if (length > (1<<17)){
					close_conn(peer_conn);
				///关闭连接
				}
				else {
								//阻塞，回应控制信息
					if (peer_conn->am_choking == 1) {	//发送choke包给peer告诉peer他被阻塞了
						send_chock(peer_conn);
						if (result == -1) {
							print_pos();
							printf("peer handler recvlinef disconnect\n");
							return NULL;
						}
					}
					else {
						// 正常情况，填充piece消息并发送
						send_piece(peer_conn, length, piece_index, begin);
					}
				}
				break;
			default: // bitfield消息或者piece消息
				readn(peer_conn->sockfd, (char *)&id, 1);
				switch(id){
					case 5: {// bitfield消息
							printf("recv bitfield msg\n");
							int bf_len = (int)length_prefix - 1;
							printf("bitfield length = %d\ncontent = ", bf_len);
							uint8_t bf[MAXLINE];
							memset(bf, 0, MAXLINE);
							readn(peer_conn->sockfd, (char *)bf, bf_len);
							int m;
							for (m = 0; m < bf_len; m++){
								printf("%02X ", bf[m]);
							}
							printf("\n");
							// 上面是接收全部消息和打印

							// 更新piece信息
							int result = peerBitfield(peer_conn, bf, bf_len);
							// 更新出错，关闭连接
							if (result <= 0) {
								close_conn(peer_conn);
								printf("when recv bitfield, bitfield error, close conn\n");
								pthread_exit(NULL);
								//////关闭连接
							}

							// 更新完成
							printf("set bitfield msg ok is_seed=%d\n", peer_conn->is_seed);
							// 如果我不是种子，发送intersted消息
							if (peer_conn->is_seed == 0){
								send_interested(peer_conn);
							}
						}
						break;
					case 7: {// piece消息
							printf("recv piece msg\n");
							uint8_t block[SUB_PIECE_LEN+1];
							result = readn(peer_conn->sockfd, (char *)&piece_index, 4);
							result = readn(peer_conn->sockfd, (char *)&begin, 4);
							if (result != 4) {
								print_pos();
								printf("peer handler recvlinef disconnect\n");
								return NULL;
							}
							result = readn(peer_conn->sockfd, (char *)&block, length_prefix-9);
							if (result != length_prefix-9) {
								print_pos();
								printf("peer handler recvlinef disconnect\n");
								return NULL;
							}
							print_pos();
							piece_index = ntohl(piece_index);
							begin = ntohl(begin);
							// 上面是接收消息

							// 处理接收到的piece
							recvPiece(peer_conn, piece_index, begin, length_prefix-9, block);

							// 请求1个新的piece      |
							request_piece(peer_conn, 1);
						}
						break;
					default :break;
				}
				break;
		}
	}
}

// peer有分片piece index
// 把peer的socket加到这个分片状态信息的owners里面去
void peerHave(peer_t *info, uint32_t piece_index) {
//	printf("socket %d has piece %d\n", info->sockfd, piece_index);
	// 我不是种子 && 分片号正确 && 分片未完成 && 有这个分片的peer不超过5个
	if (info->is_seed == 0 && piece_index < g_torrentmeta->num_pieces && g_pieces_status[piece_index].complete == 0 && g_pieces_status[piece_index].owner_num < 5){
					//锁
		pthread_mutex_lock(&(g_pieces_status[piece_index].piece_mutex));
		// peer数更新
		g_pieces_status[piece_index].owner_num++;
		// peer对应的socket更新
		int i;
		for (i = 0; i < 5; i ++){
			if (g_pieces_status[piece_index].owners[i] == -1){
				g_pieces_status[piece_index].owners[i] = info->sockfd;
				break;
			}
		}
		if (i == 5) printf("Error! when break connect, forget del owners\n");
		pthread_mutex_unlock(&(g_pieces_status[piece_index].piece_mutex));
	}
	else {
		printf("ERROR HAVE: socket %d has piece %d\n", info->sockfd, piece_index);
	}
}

// 填充一个piece消息
void fillMsg(peer_t *conn, uint8_t* msg, uint32_t piece_index, uint32_t begin, uint32_t length) {
	memset(msg, 0, length);
	read_file(msg, begin+piece_index*g_torrentmeta->piece_len, length);
}

//接收到一个bitfield
int peerBitfield(peer_t *conn, uint8_t *bf, uint32_t bf_len) {
	uint32_t i = 0;
	int j = 0;
	for (; j < bf_len; j++){
		uint8_t cur = bf[j];
		uint8_t ps[8];
		ps[0] = !!(cur & 0x80);
		ps[1] = !!(cur & 0x40);
		ps[2] = !!(cur & 0x20);
		ps[3] = !!(cur & 0x10);
		ps[4] = !!(cur & 0x08);
		ps[5] = !!(cur & 0x04);
		ps[6] = !!(cur & 0x02);
		ps[7] = !!(cur & 0x01);
		int k = 0;
		for (; k < 8; k++){
						// 用peerhave对piece信息进行更新
			if (ps[k] == 1) peerHave(conn, i);
			i ++;
		}
		// 分片号出错
		if (i >= g_torrentmeta->num_pieces) return 1;
	}
	return -1;
}

// 收到一个piece
void recvPiece(peer_t *conn, uint32_t piece_index, uint32_t begin, uint32_t length, uint8_t *block) {
	printf("recv piece index=%u begin=%u length=%u\n", piece_index, begin, length);

	pthread_mutex_lock( &(g_pieces_status[piece_index].piece_mutex) );
	// 把数据复制到缓冲区
	memcpy(g_pieces_status[piece_index].data+begin, block, length);
	// 设置子分片状态
	g_pieces_status[piece_index].sub_status[begin/SUB_PIECE_LEN] = COMPLETE;
	
	// 检查这个分片是否已经完成
	// 校验hash，查看分片是否已经完成
	int flag = 1, i;
	// 分片的所有子分片是否完成
	for (i = 0; i < g_pieces_status[piece_index].sub_num; i++){
		if (g_pieces_status[piece_index].sub_status[i] != COMPLETE){
			flag = 0;
			break;
		}
	}

	// 如果子分片完成并且hash正确
	if (flag == 1 && is_piece_complete(piece_index)){
//		printf("a piece complete, index=%ddata=%s\n", piece_index, g_pieces_status[piece_index].data);
		printf("#%d piece complete\n", piece_index);
		int result = save_piece(piece_index);
		printf("save_piece result=%d\n", result);
		if (result == 1) g_pieces_status[piece_index].complete = COMPLETE;
		else g_pieces_status[piece_index].complete = INCOMPLETE;
		// 释放空间
		// 写过文件之后，data还没有被释放

		if (g_pieces_status[piece_index].data) free(g_pieces_status[piece_index].data);
		g_pieces_status[piece_index].data = NULL;
		if (g_pieces_status[piece_index].sub_status) free(g_pieces_status[piece_index].sub_status);
		g_pieces_status[piece_index].sub_status = NULL;
		// send have msg
		send_have(piece_index);
		printf("MSG: have msg send to all peers\n");
		if (is_file_complete()){
			send_not_interested();
		}
	}
	pthread_mutex_unlock(&g_pieces_status[piece_index].piece_mutex);
//	pthread_cond_signal(&request_cond);
	return;
}

// 返回我的peer id
char *get_my_peer_id() {
	return g_my_id;
}

// 检查info hash
int check_info_hash(char *info_hash){
	if (memcmp(info_hash, g_infohash, 20) == 0) return 1;
	return -1;
}

// 发送count个子分片请求
void request_piece(peer_t *tcb, int count){
	// 请求count个分片
				// 如果要向这个peer请求一个新的分片
	if (tcb->request_piece == -1) tcb->request_piece = select_piece(tcb);
	// 找不到需要下载的子分片，下载完成
	if (tcb->request_piece == -1) {
		// 等待所有子分片接收完毕
		sleep(3);
		printf("all piece download ok");
		int fi = 0;
		for (fi = 0; fi < g_torrentmeta -> num_files; ++fi) {
			int result = complete_file(g_files_progress + fi);
			printf("complete file result=%d\n",result);
		}
	}
	else {//找到了需要下载的子分片
		int i, j;
		// 循环发送count个请求
		for (i = 0; i < count;){
			// 遍历当前请求分片的所有子分片
			for (j = 0; j < g_pieces_status[tcb->request_piece].sub_num && i < count; j++){
				// 如果子分片未完成
				if (g_pieces_status[tcb->request_piece].sub_status != NULL && g_pieces_status[tcb->request_piece].sub_status[j] == INCOMPLETE){
					// 发送这个子分片的请求
					send_request(tcb, j*g_pieces_status[tcb->request_piece].sub_len);
					// 设置这个子分片为下载中
					g_pieces_status[tcb->request_piece].sub_status[j] = DOWNLOADING;
					printf("send request index=%d begin=%d length=%d\n", tcb->request_piece, j*g_pieces_status[tcb->request_piece].sub_len, g_pieces_status[tcb->request_piece].sub_len);
					i ++;
				}
			}
			// 如果当前分片所有子分片的都已经被请求过，但是请求数目还没有到count
			if (i < count) {
				// 选择一个新的分片
				int temp = select_piece(tcb);
				if (temp != -1) tcb->request_piece = temp;
				else {//没有分片可以下载，下载完成
					//等待所有子分片接收完毕
					sleep(3);
					printf("all piece download ok\n");
					int fi = 0;
					for (fi = 0; fi < g_torrentmeta -> num_files; ++fi) {
						int result = complete_file(g_files_progress + fi);
						printf("complete file result=%d\n",result);
					}
					break;
				}
			}
		}
	}
}
// 选择一个分片进行下载
// 如果做最少优先，就在这里修改
int select_piece(peer_t *tcb){
	int i;
	assert(g_num_pieces > 0);//分片数目大于0
	// 遍历所有的分片
	int min_num = 100;
	int min_index = -1;
	for (i = 0; i < g_num_pieces; i++){
		// 如果当前peer有这个分片,并且这个分片没有下载完成
		if (is_piece_socket(tcb->sockfd, i)){
			if (g_pieces_status[i].owner_num < min_num){
				min_num = g_pieces_status[i].owner_num;
				min_index = i;
			}
		}
	}
	i = min_index;
	if (i == -1) return -1;
	//开始下载这个分片
	g_pieces_status[i].complete = DOWNLOADING;
	// 如果sub status为空，这个分片还没有开始下载
	if (g_pieces_status[i].sub_status == NULL){
		// 开始下载这个分片
		g_pieces_status[i].sub_status = (int *)malloc(g_pieces_status[i].sub_num * sizeof(int));
		int j;
		assert(g_pieces_status[i].sub_num > 0);
		// 初始化这个分片的所有子分片状态
		for (j = 0; j < g_pieces_status[i].sub_num; j++){
			g_pieces_status[i].sub_status[j] = INCOMPLETE;
		}
	}
	// 这个分片还没有开始下载
	if (g_pieces_status[i].data == NULL){
		// 分配空间
		g_pieces_status[i].data = (char *)malloc(g_torrentmeta -> piece_len);
		// 初始化
		memset(g_pieces_status[i].data, 0, g_torrentmeta -> piece_len);
	}
	// 返回这个分片
	printf("select piece %d, who has %d peers\n", min_index, min_num);
	return min_index;
}

int is_piece_socket(int socket, int piece){
//	printf("is socket %d have piece %d?\n", socket, piece);
	if (socket <= 0 || piece < 0) return 0;
	// 当前分片必须是未完成
	if (g_pieces_status[piece].complete != INCOMPLETE) return 0;
	int i;
	for (i = 0; i < MAX_REQUEST; i++){
		// 当前分片状态是未完成并且peer有这个分片
		if (g_pieces_status[piece].complete == INCOMPLETE && g_pieces_status[piece].owners[i] == socket) {
//			printf("peer %d has piece %d [%d peers have this piece]\n", socket, piece, g_pieces_status[piece].owner_num);
			return 1;
		}
	}
	return 0;
}

//作废
int is_req_socket(int socket){
	if (socket == -1) return 0;
	int i;
	for (i = 0; i < MAX_REQUEST; i++){
		if (g_sock_sent[i] == socket) return 1;
	}
	return 0;
}

//填充一个握手消息
void fill_handshake(uint8_t *hdsk_msg){
	hdsk_msg[0] = 19;
	memcpy(hdsk_msg+1, "BitTorrent protocol", 19);
	memcpy(hdsk_msg+28, g_torrentmeta->info_hash, 20);
	convert_endian(hdsk_msg+28, 20);
	memcpy(hdsk_msg+48, g_my_id, 20);
}
// 关闭所有连接
void close_all_connections(){
	peer_t *trace = conns;
	while (trace != NULL){
		close_conn(trace);
		// 每次都从头部删除
		trace = conns;
	}
}
//检查ip是否重复
int dup_peer_ip(const char * ip) {
	if (ip == NULL) return 0;
	peer_t * tmp = conns;
	while (tmp != NULL) {
		if (tmp -> peer_ip == NULL) continue;
		if ( strlen(tmp -> peer_ip) == strlen(ip) &&
			strncmp(tmp -> peer_ip, ip, strlen(ip)) == 0)
			return 1;
		tmp = tmp -> next;
	}
	return 0;
}
// 发送chock消息
void send_chock(peer_t *tcb){
	uint8_t choke_msg[6];
	memset(choke_msg, 0, 6);
	*(uint32_t *)choke_msg = htonl(1);
	choke_msg[4] = 0x0;
	int result = send(tcb->sockfd, choke_msg, 5, 0);
	printf("send unchock result=%d\n", result);
}
// 发送unchock
void send_unchock(peer_t *tcb){
	uint8_t unchoke_msg[6];
	memset(unchoke_msg, 0, 6);
	*((uint32_t *)unchoke_msg) = htonl(1);
	unchoke_msg[4] = 1;
	int result = send(tcb->sockfd, unchoke_msg, 5, 0);
	printf("send unchock result=%d\n", result);
}
// 发送interested
void send_interested(peer_t *tcb){
	uint8_t msg[6];
	memset(msg, 0, 6);
	*((uint32_t *)msg) = htonl(1);
	msg[4] = 2;
	int result = send(tcb->sockfd, msg, 5, 0);
	if (result == -1) printf("send interested error\n");
}
// 发送not interested
void send_not_interested(){
	uint8_t msg[6];
	memset(msg, 0, 6);
	*((uint32_t *)msg) = htonl(1);
	msg[4] = 3;
	peer_t *trace = conns;
	while (trace != NULL){
		int result = send(trace->sockfd, msg, 5, 0);
		if (result == -1) printf("send not interested to socket %d error\n", trace->sockfd);
		trace = trace->next;
	}
}
// 发送have
void send_have(int piece_index){
	uint8_t msg[10];
	memset(msg, 0, 10);
	*((uint32_t *)msg) = htonl(5);
	msg[4] = 4;
	*((uint32_t *)(msg+5)) = htonl(piece_index);
	peer_t *trace = conns;
	while (trace != NULL){
		int result = send(trace->sockfd, msg, 9, 0);
		if (result == -1) printf("send have to socket %d error\n", trace->sockfd);
		trace = trace->next;
	}
}
// 发送bitfield
void send_bitfield(peer_t *tcb){
	int bfd_len_flag = g_num_pieces % 8 == 0? 0:1;
	int bfd_len = g_num_pieces/8+bfd_len_flag;
	uint8_t *msg = (uint8_t *)malloc(bfd_len + 6);
	memset(msg, 0, bfd_len+6);
	// length prefix
	*((uint32_t *)msg) = htonl(bfd_len+1);
	// id
	msg[4] = 5;
	int m;
	// bf
	for (m = 0; m < g_num_pieces; m += 8) {
		uint8_t byte = 0;
		int bit = 0;
		for (bit = 0; bit < 8 && m + bit < g_num_pieces; bit++) {
			byte |= (g_pieces_status[m+bit].complete == COMPLETE) ? 
				(1 << (8-1-bit)) : 0;
		}
		msg[5+m/8] = byte;
	}
	// 发送bitfield消息
	int result = send(tcb->sockfd, msg, bfd_len+5, 0);
	if (result == -1) printf("send bitfield error\n");
	// free
	if (msg) free(msg);
	msg = NULL;
}
// 发送请求
void send_request(peer_t *tcb, uint32_t begin){
	uint8_t msg[18];
	memset(msg, 0, 18);
	*((uint32_t *)msg) = htonl(13);
	msg[4] = 6;
	*((uint32_t *)(msg+5)) = htonl(tcb->request_piece);
	*((uint32_t *)(msg+9)) = htonl(begin);
	// 如果是最后一个分片的最后一个子分片
	if (tcb->request_piece == g_torrentmeta->num_pieces-1 && begin/SUB_PIECE_LEN == g_pieces_status[tcb->request_piece].sub_num-1)
		*((uint32_t *)(msg+13)) = htonl((g_torrentmeta->length % g_torrentmeta->piece_len) % SUB_PIECE_LEN);
	// 否则
	else *((uint32_t *)(msg+13)) = htonl(g_pieces_status[tcb->request_piece].sub_len);
	// 发送
	int result = send(tcb->sockfd, msg, 17, 0);
	if (result == -1) printf("send request error\n");
	else {
		// 设置子分片下载状态为下载中
		g_pieces_status[tcb->request_piece].sub_status[begin/SUB_PIECE_LEN] = DOWNLOADING;
	}
}

//没有用到
#define PSTRLEN 0
#define PSTR 1
#define RESERVED 2
#define INFO_HASH 3
#define PEER_ID 4
#define FINISHED 5
int handshake(int sockfd, uint8_t *hdsk_msg){
	memset(hdsk_msg, 0, 70);
	// 填充一个握手消息
	fill_handshake(hdsk_msg);
	printf("start handshake, pstrlen=%d,pstr=%s,reserved=%s\ninfo_hash=", hdsk_msg[0], hdsk_msg+1, hdsk_msg+20);
	int idl;
	for(idl=0; idl<20; idl++)
		printf("%02X ",(uint8_t)(hdsk_msg+28)[idl]);
	printf("\npeer_id=");
	for(idl=0; idl<20; idl++)
		printf("%02X ",(uint8_t)(hdsk_msg+48)[idl]);
	printf("\n");
	// 发送握手消息
	if ( send(sockfd, hdsk_msg, sizeof(handshake_msg_t), 0) <= 0) {
		print_pos();
		printf("send handshake fail");
		return -1;
	}
	// 握手消息发送完毕
	printf("handshake msg send ok\n");

	int cur_state = PSTRLEN;
	int res;
	memset(hdsk_msg, 0, 70);
	// 接收直到收到握手消息的第一个字节
	while (hdsk_msg[0] != 19){
		res = readn(sockfd, hdsk_msg, 1);
		if (res != 1) {
			printf("readn return %d\n", res);
			return 0;
		}
	}
	// 读取握手消息的后面68个字节
	res = readn(sockfd, hdsk_msg+1, 67);
	printf("handshake ack, pstrlen=%d,pstr=%s,reserved=%s\ninfo_hash=", hdsk_msg[0], hdsk_msg+1, hdsk_msg+20);
	for(idl=0; idl<20; idl++)
		printf("%02X ",(uint8_t)(hdsk_msg+28)[idl]);
	printf("\npeer_id=");
	for(idl=0; idl<20; idl++)
		printf("%02X ",(uint8_t)(hdsk_msg+48)[idl]);
	printf("\n");

	if (res <= 0) {
		printf("when wait shakehand");
		print_pos();
		exit(1);
	}
	// 接收正确
	else printf("wait handshake, recv handshake msg ok\n");
	// hash的大小端转换
	convert_endian(hdsk_msg+28, 20);
	// 检查hash
	if (check_info_hash(hdsk_msg+28) > 0) {
		printf("recv handshake msg hash check ok\n");
		// if ok, handshake, create tcb
		return 1;
	}
	else {
		printf("handshake msg hash or id error\n");
		return 0;
	}

}
// 发送一个分片
void send_piece(peer_t *tcb, uint32_t length, uint32_t piece_index, uint32_t begin){
	uint8_t msg[SUB_PIECE_LEN+20];
	memset(msg, 0, SUB_PIECE_LEN+20);
	*((uint32_t *)msg) = htonl(length + 9);
	msg[4] = 7;
	*((uint32_t *)(msg+5)) = htonl(piece_index);
	*((uint32_t *)(msg+9)) = htonl(begin);
	// 填充piece消息的block部分
	fillMsg(tcb, msg+13, piece_index, begin, length);

	// 发送头部和block部分
	int result = send(tcb->sockfd, msg, length+13, 0);
	if (result == -1) {
		print_pos();
		printf("peer handler recvlinef disconnect\n");
	}
}
// 检查文件是否下载完成
int is_file_complete(){
	int i;
	// 所有的分片状态都是已完成
	for (i = 0; i < g_torrentmeta->num_pieces; i++){
		if (g_pieces_status[i].complete != COMPLETE) return 0;
	}
	return 1;
}
