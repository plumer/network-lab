#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h> 
#include <assert.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <error.h>
#include <errno.h>
#include <fcntl.h>
#include "fileio.h"
#include "util.h"
#include "fileio.h"
#include "btdata.h"
#include "bencode.h"
#include "pwp_daemon.h"

//#define MAXLINE 4096 
#define MAX_CONNECTION_NUM 10
#define MAX_REQUEST 5
#define MAX_DOWNLOAD_PIECES 10
// pthread数据
// 函数声明
void init();
void *wait_handshake(void *sock);
void *listen_to_peer();

// 初始化
void init(){
	g_peerport = 10982;
	int i;
	g_done = 0;
	g_tracker_response = NULL;
}

// 对方跟我建立连接之后，发送和等待握手
void *wait_handshake(void *sock){
	// 连接成功，发送握手信息
	int sockfd = *(int *)sock;
	uint8_t hdsk_msg[70];
	// 握手
	int result = handshake(sockfd, hdsk_msg);
	// 发送握手消息成功
	if (result == 1){
		// 创建连接描述符
		peer_t *tcb = new_peer_t();
		tcb->sockfd = sockfd;	//填写sockfd
		tcb->is_seed = 1;	//我是种子
		memcpy(tcb->peer_id, hdsk_msg+48, 20);	//对方的id
		memcpy(tcb->peer_id, g_my_id, 20);	//我的id
		// am_chocking = 0
		tcb->am_choking = 0;	//我不阻塞对方
		// 启动peer_handler线程接收peer消息
		pthread_t listen_tid;
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
		int res = pthread_create(&listen_tid, &attr, peer_handler, tcb);
		if (res){
			print_pos();
			printf("create thread error: when create peer_handler\n");
			exit(1);
		}
		// 启动keep alive线程保持连接
		res = pthread_create(&listen_tid, &attr, keep_alive, tcb);
		if (res){
			print_pos();
			printf("create thread error: when create keep_alive\n");
			exit(1);
		}
		// 发送我的bitfield给对方
		send_bitfield(tcb);
		
		printf("after handshake, create keep alive & peer_handler thread ok\n");
	}
	else {
		// close connection
		printf("handshake msg hash & id check failed\n");
		close(sockfd);
	}
	pthread_exit(NULL);
}
// 监听peer的连接
void *listen_to_peer(){
	int listenfd, connfd;
	socklen_t clilen = sizeof(struct sockaddr_in);
	struct sockaddr_in cliaddr, servaddr;

	listenfd = socket(AF_INET, SOCK_STREAM, 0);

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(g_peerport);

	const int on = 1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(const int));
	bind(listenfd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr_in));
	listen(listenfd, MAX_CONNECTION_NUM);

	while (1){
		connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
		// peer发起了一个连接
		printf("accept socket=%d from ip=%s\n", connfd, inet_ntoa(cliaddr.sin_addr));
		// 检查ip是否重复，如果重复就忽略
		if (dup_peer_ip(inet_ntoa(cliaddr.sin_addr))) continue;
		// 如果接收错误，就下一个
		if (connfd <= 0) continue;
		// 正常，创建握手线程
		pthread_t listen_tid;
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
		int res = pthread_create(&listen_tid, &attr, wait_handshake, &connfd);
		if (res){
			print_pos();
			printf("CREATE LISTEN TO PEER THREAD ERROR\n");
			exit(1);
		}
		printf("a peer connected, listen to another peer\n");
	}
}
int main(int argc, char **argv) {
	int sockfd = -1;
	char rcvline[MAXLINE];
	char tmp[MAXLINE];
	FILE* f;
	int rc;
	int i;
	signal(SIGPIPE, SIG_IGN);

	// 注意: 你可以根据自己的需要修改这个程序的使用方法
	if(argc < 4){
		printf("Usage: SimpleTorrent <torrent file> <ip of this machine (XXX.XXX.XXX.XXX form)> <downloaded file location> [isseed]\n");
		printf("\t isseed is optional, 1 indicates this is a seed and won't contact other clients\n");
		printf("\t 0 indicates a downloading client and will connect to other clients and receive connections\n");
		exit(-1);
	}

	int iseed = 0;
	if( argc > 4 ) {
		iseed = !!atoi(argv[4]);
	}
		
	if( iseed ) {
		printf("SimpleTorrent running as seed.\n");
	}
	
	init();
	srand(time(NULL));

	int val[5];
	for(i = 0; i < 5; i++){
		val[i] = rand();
	}
	memcpy(g_my_id, (char*)val, 20);
	strncpy(g_my_ip, argv[2], strlen(argv[2]));
	g_my_ip[strlen(argv[2])+1] = '\0';
	
	g_filename = argv[3];
	g_filelocation = argv[3];
	char isSeed = argv[4][0];

	// parse the torrent file and read into *g_torrentmeta*
	g_torrentmeta = parsetorrentfile(argv[1]);
	memcpy(g_infohash, g_torrentmeta -> info_hash, 20);

	g_filelen = g_torrentmeta -> length;
	g_num_pieces = g_torrentmeta -> num_pieces;
	// 这里初始化所有分片的状态
	g_pieces_status = (piece_status_t *)malloc(g_num_pieces*sizeof(piece_status_t));

	// 有没有断点续传文件和初始化分片状态表
	if (argv[4][0] == '0'){	//我不是种子
		g_files_progress = (file_progress_t *)malloc(g_torrentmeta->num_files * sizeof(file_progress_t));
		int fi = 0;
		int totallength = 0;
		assert(g_torrentmeta->num_files > 0);
		//逐文件
		for (; fi < g_torrentmeta->num_files; fi++){
			// 逐文件地读取进度信息
			// 跳过已经下载完成的文件
			// 将进度信息写入g_pieces_status
			int pc_len = g_torrentmeta->piece_len;
			file_progress_t *tmp = read_progress(g_torrentmeta->file_info[fi].path);
			if (tmp == NULL) {
				printf("MSG: no progress file\n");
				// 无法读取进度文件，是否已经下载完成？
				FILE * content_file = fopen(g_torrentmeta -> file_info[fi].path, "r");
				if ( content_file ) {
					// 文件已经存在，假装下载完成了
					fclose(content_file);
					continue;
				}
				// 文件并不存在，创建进度文件，顺便创建内容文件。
				tmp = create_progress(g_torrentmeta->file_info[fi].path, totallength, g_torrentmeta->file_info[fi].length, pc_len);
				create_file(g_torrentmeta -> file_info[fi].path, g_torrentmeta -> file_info[fi].length);
				int i;
				// 分片总数>0
				assert(tmp->num_pieces > 0);
				//初始化文件的所有分片
				for (i = 0; i < tmp->num_pieces; i++) {
					int fpi = tmp->piece_progress[i].piece_index;
					//分片都是未完成
					g_pieces_status[fpi].complete = 0;
					g_pieces_status[fpi].owner_num = 0;
					// 也没有peer拥有这个分片
					int j;
					for (j = 0; j < MAX_REQUEST; j++){
						g_pieces_status[fpi].owners[j] = -1;
					}
					// 子分片大小
					g_pieces_status[fpi].sub_len = SUB_PIECE_LEN;
					// 最后一个分片
					if (fpi == g_torrentmeta->num_pieces-1){
						// 子分片大小恰好整除分片大小
						int piece_flag = (g_torrentmeta->length % g_torrentmeta->piece_len % SUB_PIECE_LEN == 0)? 0:1;
						// 没有整除
						g_pieces_status[fpi].sub_num = ((g_torrentmeta->length%g_torrentmeta->piece_len/SUB_PIECE_LEN)+piece_flag);
					}
					// 正常情况
					else g_pieces_status[fpi].sub_num = g_torrentmeta->piece_len/SUB_PIECE_LEN;
					// 初始化所有分片的所有子分片状态
					g_pieces_status[fpi].sub_status = (int *)malloc(g_pieces_status[fpi].sub_num * sizeof(int));
					for (j = 0; j < g_pieces_status[fpi].sub_num; j++) g_pieces_status[fpi].sub_status[j] = INCOMPLETE;
					// 为这个分片分配空间
					g_pieces_status[fpi].data = (uint8_t *)malloc(g_torrentmeta->piece_len);
					memset(g_pieces_status[fpi].data, 0, g_torrentmeta->piece_len);
					// 初始化锁
					pthread_mutex_init(&(g_pieces_status[fpi].piece_mutex), NULL);
				}
			}
			else {
				printf("MSG: have progress file\n");
				// 进度文件已存在并且读取完成
				// 将tmp中分片的进度全部读入g_pieces_status
				int i;
				for (i = 0; i < /*g_num_pieces*/tmp -> num_pieces; i++){ // TODO 这里有问题
					//g_pieces_status[i].complete = tmp->piece_progress[i].completed; 这一行和下一行都不对
					//g_pieces_status[i].owner_num = 0;
					// 根据进度文件还原文件状态
					int fpi = tmp -> piece_progress[i].piece_index;
					g_pieces_status[ fpi ].complete = tmp -> piece_progress[i].completed;
					// 连接清0
					g_pieces_status[ fpi ].owner_num = 0;
					int j;
					for (j = 0; j < MAX_REQUEST; j++){
						g_pieces_status[fpi].owners[j] = -1;
					}
					// 分片状态
					g_pieces_status[fpi].sub_len = SUB_PIECE_LEN;
					// 如果是最后一个分片
					if (fpi == g_torrentmeta->num_pieces-1){
						int piece_flag = (g_torrentmeta->length % g_torrentmeta->piece_len % SUB_PIECE_LEN == 0)? 0:1;
						g_pieces_status[fpi].sub_num = ((g_torrentmeta->length%g_torrentmeta->piece_len/SUB_PIECE_LEN)+piece_flag);
					}
					else g_pieces_status[fpi].sub_num = g_torrentmeta->piece_len/SUB_PIECE_LEN;
					// 分片的子分片状态，分片没有下载完成的时候
					if (g_pieces_status[fpi].complete != 1){
						// 初始化子分片状态
						g_pieces_status[fpi].sub_status = (int *)malloc(g_pieces_status[fpi].sub_num * sizeof(int));
						for (j = 0; j < g_pieces_status[fpi].sub_num; j++) 
							g_pieces_status[fpi].sub_status[j] = tmp->piece_progress[fpi].completed;
						// 初始化子分片数据
						g_pieces_status[fpi].data = (uint8_t *)malloc(g_torrentmeta->piece_len);
						memset(g_pieces_status[fpi].data, 0, g_torrentmeta->piece_len);
					}
					else {
						g_pieces_status[fpi].sub_status = NULL;
						g_pieces_status[fpi].data = NULL;
					}
					pthread_mutex_init(&(g_pieces_status[fpi].piece_mutex), NULL);
				}
			}
			memcpy(&(g_files_progress[fi]), tmp, sizeof(file_progress_t));
			// g_files_progress中可能存在空项 - 下载完成的文件被跳过
			totallength += g_torrentmeta->file_info[fi].length;
			if (tmp) free(tmp);
			tmp = NULL;
		}
	}
	else {
		// 程序运行在做种状态，无需file_progress
		int i;
		for (i = 0; i < g_num_pieces; i++){
			// 所有分片设置为已完成
			g_pieces_status[i].complete = 1;
			g_pieces_status[i].owner_num = 0;
			int j;
			for (j = 0; j < MAX_REQUEST; j++){
				g_pieces_status[i].owners[j] = -1;
			}
			// 子分片大小和数目
			g_pieces_status[i].sub_len = SUB_PIECE_LEN;
			if (i == g_torrentmeta->num_pieces-1){
				int piece_flag = (g_torrentmeta->length % g_torrentmeta->piece_len % SUB_PIECE_LEN == 0)? 0:1;
				g_pieces_status[i].sub_num = ((g_torrentmeta->length%g_torrentmeta->piece_len/SUB_PIECE_LEN)+piece_flag);
			}
			else g_pieces_status[i].sub_num = g_torrentmeta->piece_len/SUB_PIECE_LEN;
			g_pieces_status[i].sub_num = g_torrentmeta->piece_len/SUB_PIECE_LEN;
			g_pieces_status[i].sub_status = NULL;
			g_pieces_status[i].data = NULL;
			pthread_mutex_init(&(g_pieces_status[i].piece_mutex), NULL);
		}
	}
	
	puts("file progress read, files created");

	announce_url_t* announce_info;
	announce_info = parse_announce_url(g_torrentmeta -> announce);
	// 提取tracker url中的IP地址
	printf("HOSTNAME: %s\n",announce_info->hostname);
	struct hostent *record;
	record = gethostbyname(announce_info->hostname);
	if (record==NULL) { 
		printf("gethostbyname(%s) failed", announce_info->hostname);
		exit(1);
	}
	struct in_addr* address;
	address =(struct in_addr * )record->h_addr_list[0];
	printf("Tracker IP Address: %s\n", inet_ntoa(* address));
	strcpy(g_tracker_ip,inet_ntoa(*address));
	g_tracker_port = announce_info->port;

	if (announce_info) free(announce_info);
	announce_info = NULL;

	// 初始化
	// 设置信号句柄
	signal(SIGINT,client_shutdown);

	// 设置监听peer的线程
	pthread_t listen_tid;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	int res = pthread_create(&listen_tid, &attr, listen_to_peer, NULL);
	if (res){
		print_pos();
		printf("CREATE LISTEN TO PEER THREAD ERROR\n");
		exit(1);
	}
	printf("listen to peer thread created ok\n");

	// 定期联系Tracker服务器
	int firsttime = 1;
	int mlen;
	char* MESG;
	MESG = make_tracker_request(BT_STARTED,&mlen);
	while(!g_done){// while loop
		if(sockfd <= 0){
			//创建套接字发送报文给Tracker
			printf("Creating socket to tracker...\n");
			sockfd = connect_to_host(g_tracker_ip, g_tracker_port);
		}
		
		printf("Sending request to tracker...\n");
		
		if(!firsttime){
			free(MESG);
			// -1 指定不发送event参数
			MESG = make_tracker_request(-1,&mlen);
			printf("MESG: ");
			for(i=0; i<mlen; i++)
				printf("%c",MESG[i]);
			printf("\n");
		}
		send(sockfd, MESG, mlen, 0);
		firsttime = 0;
		memset(rcvline,0x0,MAXLINE);
		memset(tmp,0x0,MAXLINE);
		
		// 读取并处理来自Tracker的响应
		tracker_response* tr;
		tr = preprocess_tracker_response(sockfd); 
	 
		// 关闭套接字, 以便再次使用
		shutdown(sockfd,SHUT_RDWR);
		close(sockfd);
		sockfd = 0;

		printf("Decoding response...\n");
		char* tmp2 = (char*)malloc(tr->size*sizeof(char));
		memcpy(tmp2,tr->data,tr->size*sizeof(char));

		printf("Parsing tracker data\n");
		g_tracker_response = get_tracker_data(tmp2,tr->size);
		
		if (tmp2) {
			free(tmp2);
			tmp2 = NULL;
		}

		printf("Num Peers: %d\n",g_tracker_response->numpeers);
		// 根据tracker返回的所有PEER信息，检查已有连接，建立新连接。
		for (i = 0; i < g_tracker_response -> numpeers; i++) {//for loop
			print_pos();
			printf("--------------- peer #%d------------------\n", i);
			printf("Peer id: ");
			int idl;
			for(idl=0; idl<20; idl++)
				printf("%02X ",(unsigned char)g_tracker_response->peers[i].id[idl]);
			printf("\n");
			printf("Peer ip: %s\n",g_tracker_response->peers[i].ip);
			printf("Peer port: %d\n",g_tracker_response->peers[i].port);

			// 0说明需要下载，1说明是一个做种线程
			// 下载线程需要主动发起连接，申请下载，做种线程不需要主动发起连接
			if (argv[4][0] == '0'){//if connect
				printf("I want to download, to connect peer\n");
				// 检查IP是否为本机
				printf("argv[2]=%s:\t", argv[2]);
				if (memcmp(argv[2], g_tracker_response->peers[i].ip, min(strlen(argv[2]), strlen(g_tracker_response->peers[i].ip))) == 0) {
					printf("It is myself, next\n");
					continue;
				} else if (dup_peer_ip(g_tracker_response -> peers[i].ip)) {
					printf("existing peer\n");
					continue;
				} 
				int sockfd = connect_to_host(g_tracker_response->peers[i].ip, g_tracker_response->peers[i].port);
				if (sockfd <= 0) {
					printf("connect to host error\n");
					continue;
				}
				uint8_t hdsk_msg[70];
				int result = handshake(sockfd, hdsk_msg);
				// 检查hash
				if (result == 0) {
					// hash 检查不正确
					close(sockfd);
					continue;
				}
				// create peer_handler thread
				printf("info hash and id right");
				peer_t *tcb = new_peer_t();
				tcb->sockfd = sockfd;//填写sockfd
				memcpy(tcb->peer_id, hdsk_msg+48, 20);
				memcpy(tcb->peer_id, g_my_id, 20);
				tcb->is_seed = 0;
				tcb->am_choking = 0;
				strncpy(tcb -> peer_ip, g_tracker_response -> peers[i].ip, 
						strlen(g_tracker_response -> peers[i].ip));
				pthread_t listen_tid;
				pthread_attr_t attr;
				pthread_attr_init(&attr);
				pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
				res = pthread_create(&listen_tid, &attr, peer_handler, tcb);
				if (res) {
					print_pos();
					printf("create thread error: when create peer_handler\n");
					exit(1);
				}
				else printf("create peer_handler ok\n");
				res = pthread_create(&listen_tid, &attr, keep_alive, tcb);
				if (res){
					print_pos();
					printf("create thread error: when create keep_alive\n");
					exit(1);
				}
				else printf("create keep alive ok\n");
				send_bitfield(tcb);
				printf("bitfield msg from seed to down sent\n");

			}//if connect
		}//for loop
		// 必须等待td->interval秒, 然后再发出下一个GET请求
		sleep(g_tracker_response->interval);
	}// while loop
	// 睡眠以等待其他线程关闭它们的套接字, 只有在用户按下ctrl-c时才会到达这里
	sleep(2);

	int stop_mlen;
	char * stop_msg = make_tracker_request(BT_STOPPED, &mlen);
	send(sockfd, stop_msg, stop_mlen, 0);
	if (g_files_progress) {
		int result = write_progress(g_files_progress);
		printf("write progress result = %d\n", result);
	}
	close_all_connections();
	return 0;
}
