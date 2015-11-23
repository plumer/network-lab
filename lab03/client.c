#include "commons.h"

#include <ncurses.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>

#define SERVER_PORT 23333
#define SERVER_IP "114.212.190.186"
//#define SERVER_IP "114.212.135.103"
//#define SERVER_IP "127.0.0.1"
char movename[128][32] = { "ROCK",
	"PAPER", 
	"SCISSORS",
	"LIZARD", 
	"SPOCK", 
	"vable Curses",
	"Imperio",
	"Crucio",
	"Avada Kedavra",
	"Morsmordre",
	"Petrificus Totalus",
	"Densaugeo",
	"Furnunculus",
	"Obliviate",
	"Tarantollegra",
	"Serpensortia",
	"Rictusempra",
	"Locomotoe Mortis",
	"Waddiwasi",
	"Reducto",
	"Stupefy",
	"Relashio",
	"Diffindo",
	"Impedimento",
	"Expecto Patronum",
	"Fubute Ubcabtaten",
	"Enervate",
	"Colloportus",
	"Protego",
	"Alohomora",
	"Disillusionment",
	"Prior Incantato",
	"Wingardium Leviosa",
	"Apareciym",
	"Quietus",
	"Accio",
	"Ridikuius",
	"Enforgio",
	"Reducio",
	"Legilimens",
	"Lumos",
	"Nox",
	"Sonorus",
	"Puierus",
	"Reparo",
	"Iocomotor trunk",
	"Poine me",
	"Pack",
	"Scourgify",
	"Incendio",
	"Imperuious",
	"Ferula",
	"Mobiliarbus",
	"Disapparation",
	"Muggle-Repelling Charm",
	"Dissendium",
	"Orchideous",
	"Avis",
	"Flagrate",
	"protego",
	"specialis revelio",
	"reparo",
	"diffindo",
	"levicorpus",
	"liberacorpus",
	"muffliato",
	"epliskey",
	"oppugno",
	"sectumsepra",
	"aguamenti",
	"petrificus totalus",
	"expelliarmus",
	"reducto",
	"avada kedavra",
	"crucio",
	"stupefy",
	"impedimenta",
	"incarcerous",
	"lumos",
	"accio",
	"n-vbl"
};
WINDOW *board;
WINDOW *boardInside;
WINDOW *online;
WINDOW *onlineInside;
WINDOW *process;
WINDOW *processInside;
WINDOW *input;
WINDOW *inputInside;
WINDOW *tools;

// 在线用户
// 每次更新列表的同时刷新屏幕
struct online_item_t onliner[128];
int onlineCount;

// 排行榜
// 每次更新列表的同时刷新屏幕
struct board_item_t boarder[128];
int boardCount;

// 本回合战斗结果
// 每次获得新的结果就更新屏幕
struct battle_each_t bs;
int battleCount;

// 刷新屏幕
void clrWin(void *win);			//清屏
void refreshBoard(void *win);		//刷新board
void refreshList(void *win);		//刷新online list
void refreshProcessChallenge(void *win);
void refreshProcessAckChallenge(void *win);
void refreshProcessBattleInfo(void *win);
void refreshProcessBattleEach(void *win);
void refreshProcessBattleResult(void *win);

// 登录&注册结果
uint16_t imChallenging;
uint16_t challenging;
uint16_t challengeOK;
uint16_t loginOK;
uint16_t registOK;

void initConnect();	//初始化连接
void exitProgram();	//异常退出程序的时候，关闭socket

void sendMSG(char *message, int mlen);	//发送消息
void *recvMSG();			//接收消息

// 发包函数们
void login();		//登录
void regist();		//注册
void getBoard();	//排行榜
void getOnlineList();	//在线列表
void getTools();	//备用
void *refreshMSG();	//随时刷新排行榜和在线列表		
void challenge(int index);	//挑战
void ackChallenge();
void battle(char *command);	//战斗
void escape();			//逃跑

// 界面函数们
void showIndex();	//运行程序界面
void showLogin();	//登录界面
void showRegist();	//注册界面
void showMenu();	//登录成功后，程序主界面

// 这里的三个专门用来注册和登录
char username[100];	//用户名
char password[100];	//密码
char email[100];	//邮箱

// 然后是用户信息&对战双方信息
struct login_ack_body_t myInfo;
uint16_t a_id;
uint16_t b_id;
char a_name[32];
char b_name[32];
char a_input[32];
char b_input[32];
struct battle_result_t curResult;

int sockfd;			//连接套接字
struct sockaddr_in server_addr;	//服务器地址

int main(){
	initConnect();
	showIndex();
	exitProgram();
	return 0;
}

void initConnect(){
	// 创建套接字
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		printf("create socket error!\n");
		exit(-1);
	}
	// 清零
	memset(&server_addr, 0, sizeof(struct sockaddr_in));
	// 填充服务器地址
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

	// 连接
	if ((connect(sockfd, (struct sockaddr*)(&server_addr), sizeof(struct sockaddr))) == -1) {
		printf("connect error!\n");
		exit(1);
	}

	loginOK = 0;
	registOK = 0;
	challengeOK = 0;
	challenging = 0;
	battleCount = 0;

	memset(a_name, 0, 32);
	memset(b_name, 0, 32);
	memset(a_input, 0, 32);
	memset(b_input, 0, 32);
}

void exitProgram(){
	delwin(board);
	delwin(online);
	delwin(process);
	delwin(tools);
	delwin(input);
	delwin(boardInside);
	delwin(onlineInside);
	delwin(processInside);
	delwin(inputInside);
	endwin();

	close(sockfd);
}

void sendMSG(char *message, int mlen){
	char buffer[MAX_PACKAGE_LEN+1];
	memset(buffer, 0, MAX_PACKAGE_LEN+1);
	memcpy(buffer, message, mlen);

	if (send(sockfd, buffer, MAX_PACKAGE_LEN, 0) == -1){
		printf("send error!\n");
		exitProgram();	//异常处理【至少要把socket关了
		exit(-1);
	}

}

void *recvMSG(){
	// 收消息和打印
	char recvline[MAX_PACKAGE_LEN+1];
	memset(recvline, 0, MAX_PACKAGE_LEN+1);

	int n = 0;

	// 这个函数的主要作用就是收包 & 分发
	while ((n = recv(sockfd, recvline, MAX_PACKAGE_LEN, 0)) > 0){
		struct packet_head_t *h = (struct packet_head_t *)recvline;
		int flag = 1;
		switch(h->opcode) {
			case OP_LOGIN_RPL:{	//登录 
				flag = 0;
				if (h->flags == FLAG_ACCEPT) {
					loginOK = 1;
					memset(&myInfo, 0, sizeof(struct login_ack_body_t));
					struct login_ack_body_t *usrInfo = (struct login_ack_body_t *)(recvline + HEAD_LEN);
					myInfo.id = usrInfo->id;
					myInfo.score = usrInfo->score;
					myInfo.rank = usrInfo->rank;
					myInfo.games_won = usrInfo->games_won;
					myInfo.games_lost = usrInfo->games_lost;
					myInfo.games_drawn = usrInfo->games_drawn;
				}
				else loginOK = 0;
					  }
				break;
			case OP_REGISTER_RPL:{ 	//注册
				flag = 0;
				if (h->flags == FLAG_ACCEPT) {
					registOK = 1;
				}
				else {
					uint8_t errCode = *(recvline+HEAD_LEN);
					switch(errCode){
						case ERRNO_USERNAME_CONFLICT: printf("user name conflict\n"); break;
						default: break;
					}
					registOK = 0;
				}
					     }
				break;
			case OP_BOARD_RPL:{	//排行榜
				struct board_item_t *tmp;
				boardCount = h->opr_cnt;
				int i = 0;
				for (; i < h->opr_cnt; i ++) {
					tmp = (struct board_item_t *)(recvline + HEAD_LEN + i*BOARD_ITEM_LEN);
					boarder[i].id = tmp->id;
					boarder[i].rank = tmp->rank;
					boarder[i].score = tmp->score;
					memset(boarder[i].name, 0, 32);
					memcpy(boarder[i].name, tmp->name, 31);
				}
				refreshBoard(boardInside);
					  }
				break;
			case OP_CURR_USRLST_RPL:{	//在线用户列表
				struct online_item_t *tmp;
				onlineCount = h->opr_cnt;
				int i = 0;
				for (; i < h->opr_cnt; i ++) {
					tmp = (struct online_item_t *)(recvline + HEAD_LEN + i*ONLINE_ITEM_LEN);
					onliner[i].id = tmp->id;
					onliner[i].status = tmp->status;
					memset(onliner[i].name, 0, 32);
					memcpy(onliner[i].name, tmp->name, 31);
				}
				refreshList(onlineInside);
					      }
				break;
			case OP_CHALLENGE_RPL:{		// challenge reply
				if (h->flags == FLAG_ACCEPT) challengeOK = 1;
				else {
					challengeOK = 0;
					imChallenging = 1;
				}

				challenging = 0;
				refreshProcessChallenge(processInside);
					      }
				break;
			case OP_CHALLENGE_RQT:{		// challenge request
				struct challenge_body_t *data = (struct challenge_body_t *)(recvline+HEAD_LEN);
				a_id = data->a_id;
				b_id = myInfo.id;
				memset(a_name, 0, 32);
				memcpy(a_name, data->a_name, 31);
				memset(b_name, 0, 32);
				memcpy(b_name, username, 31);
				challenging = 1;

				refreshProcessAckChallenge(processInside);
					      }
				break;
			case OP_BATTLE_MSG:{
				if (h->flags == FLAG_BATTLE_INTRO){
					struct battle_info_t *data = (struct battle_info_t *)(recvline+HEAD_LEN);
					a_id = data->a_id;
					b_id = data->b_id;
					memset(a_input, 0, 32);
					memcpy(a_input, data->a_input, 31);
					memset(b_input, 0, 32);
					memcpy(b_input, data->b_input, 31);

					refreshProcessBattleInfo(processInside);
				}
				else if (h->flags == FLAG_BATTLE_EACH){
					struct battle_each_t *data = (struct battle_each_t *)(recvline+HEAD_LEN);
					bs.a_id = data->a_id;
					bs.a_move = data->a_move;
					bs.b_id = data->b_id;
					bs.b_move = data->b_move;
					bs.result = data->result;

					refreshProcessBattleEach(processInside);
				}
					   }
				break;
			case OP_BATTLE_RESULT:{
				struct battle_result_t *data = (struct battle_result_t *)(recvline+HEAD_LEN);
				curResult.a_id = data->a_id;
				curResult.a_win = data->a_win;
				curResult.a_lose = data->a_lose;
				curResult.a_draw = data->a_draw;
				curResult.b_id = data->b_id;
				curResult.b_win = data->b_win;
				curResult.b_lose = data->b_lose;
				curResult.b_draw = data->b_draw;

				refreshProcessBattleResult(processInside);
					      }
				break;
			default: break;
		}
		if (flag == 0) break;	//当注册&登录时，只recv一次，所以跳出循环
	}
	// n <= 0:接收出错
	if (n <= 0){
		printf("recv error\n");
		exitProgram();
		pthread_exit(NULL);
		exit(1);
	}
	return NULL;
}

void showIndex(){
	// 界面画图
	WINDOW *container;
	if (initscr() == NULL){
		printf("init screen error\n");
		exitProgram();
		exit(1);
	}

	int x, y;
	getmaxyx(stdscr, y, x);
	if ((container = newwin(19, x-6, 3, 3)) == NULL) {
		endwin();
		printf("init screen error\n");
		exitProgram();
		exit(1);
	}
	box(container, 0, 0);
	refresh();
	mvwhline(container,4,3,ACS_HLINE,x-12);
	mvwaddstr(container, 2, 3, "Welcome to RPSLS Game! Login or Regist? ");
	mvwaddstr(container, 5, 8, "Login");
	mvwaddstr(container, 5, x/2-5, "Regist");
	mvwaddstr(container, 5, x-20, "Exit");
	wrefresh(container);

	// 界面逻辑
	noecho();
	move(8,13);
	char ch = getch();
	int fg = 0;	//fg = 0: login; fg = 1:cancel
	while (ch != '\n') {
		if (fg == 0) {
			move(8, x/2+2);
			fg = 1;
		}
		else if (fg == 1){
			move(8, x-15);
			fg = 2;
		}
		else{
			move(8, 13);
			fg = 0;
		}
		ch = getch();
	}
	echo();
	delwin(container);
	endwin();
	if (fg == 0) showLogin();
	else if (fg == 1) showRegist();
}

void showLogin(){
	// 界面画图
	if(initscr() == NULL) exit(1);
	//  h  w
	int y, x;
	WINDOW *win;
	WINDOW *usernm;
	WINDOW *passwd;
	getmaxyx(stdscr,y,x);
	//             h   w   starty   startx
	if((win=newwin(19,x-6,3,3)) == NULL){
		endwin();
		exit(1);
	}
	box(win,0,0);
	mvwhline(win,4,3,ACS_HLINE,x-12);
	mvwaddstr(win,2,x/2-5,"Login");
	if((usernm=newwin(3,x-19,9,14)) == NULL){
		endwin();
		exit(1);
	}
	refresh();
	box(usernm, 0, 0);
	if((passwd=newwin(3,x-19,13,14))==NULL){
		endwin();
		exit(1);
	}
	refresh();
	box(passwd, 0, 0);
	mvwaddstr(win,7,2,"Username:");
	mvwaddstr(win,11,2,"Password:");
	mvwaddstr(win,15,10, "Login");
	mvwaddstr(win,15,x-21,"Cancel");
	wrefresh(win);
	wrefresh(usernm);
	wrefresh(passwd);

	// 界面逻辑
	memset(username, 0, 100);
	memset(password, 0, 100);

	move(10,15);
	getstr(username);
	move(14,15);
	getstr(password);
	move(18,15);
	noecho();
	char ch = getch();
	int fg = 0;	//fg = 0: login; fg = 1:cancel
	while (ch != '\n') {
		if (fg == 0) {
			move(18, x-16);
			fg = 1;
		}
		else{
			move(18, 15);
			fg = 0;
		}
		ch = getch();
	}
	echo();
	delwin(win);
	delwin(usernm);
	delwin(passwd);
	endwin();
	if (fg == 0) login();
	else showIndex();
}
void showRegist(){
	// 界面画图
	if(initscr() == NULL) exit(1);
	//  h  w
	int y, x;

	WINDOW *win;
	WINDOW *usernm;
	WINDOW *passwd;
	WINDOW *mail;
	getmaxyx(stdscr,y,x);
	//             h   w   starty   startx
	if((win=newwin(19,x-6,3,3)) == NULL){
		endwin();
		exit(1);
	}
	box(win,0,0);
	mvwhline(win,2,3,ACS_HLINE,x-12);
	mvwaddstr(win,1,x/2-5,"Regist");
	if((usernm=newwin(3,x-19,6,14)) == NULL){
		endwin();
		exit(1);
	}
	refresh();
	box(usernm, 0, 0);
	if((passwd=newwin(3,x-19,10,14))==NULL){
		endwin();
		exit(1);
	}
	refresh();
	box(passwd, 0, 0);
	if((mail = newwin(3, x-19, 14, 14)) == NULL){
		endwin();
		exit(1);
	}
	refresh();
	box(mail, 0, 0);

	mvwaddstr(win,4,2,"Username:");
	mvwaddstr(usernm,1,x-30,"6-31 Bytes");
	mvwaddstr(win,8,2,"Password:");
	mvwaddstr(passwd,1, x-30, "6-31 Bytes");
	mvwaddstr(win,12,5,"Email:");
	mvwaddstr(mail,1, x-30, "X@Y.Z");
	mvwaddstr(win,16,10, "Regist");
	mvwaddstr(win,16,x-21,"Cancel");

	wrefresh(win);
	wrefresh(usernm);
	wrefresh(passwd);
	wrefresh(mail);

	// 界面逻辑
	memset(username, 0, 100);
	memset(password, 0, 100);

	move(7,15);
	getstr(username);
	move(11,15);
	getstr(password);
	move(15,15);
	getstr(email);
	move(19, 15);
	noecho();
	char ch = getch();
	int fg = 0;	//fg = 0: login; fg = 1:cancel
	while (ch != '\n') {
		if (fg == 0) {
			move(19, x-16);
			fg = 1;
		}
		else{
			move(19, 15);
			fg = 0;
		}
		ch = getch();
	}
	echo();
	delwin(win);
	delwin(usernm);
	delwin(passwd);
	delwin(mail);
	endwin();
	if (fg == 0) regist();
	else showIndex();
}
void showMenu(){
	// 界面画图
	if (initscr() == NULL) {
		exit(1);
	}
	box(stdscr, 0, 0);
	refresh();

	int y, x;
	getmaxyx(stdscr, y, x);
	if ((board = newwin(y/3, x/4, 3, 1)) == NULL){
		endwin();
		exit(1);
	}
	box(board, 0, 0);
	refresh();
	if ((boardInside = newwin(y/3-4, x/4-2, 6, 2)) == NULL){
		endwin();
		exit(1);
	}
	refresh(); 
	if ((online = newwin(y*2/3-4, x/4, y/3+3, 1)) == NULL){
		endwin();
		exit(1);
	}
	box(online, 0, 0);
	refresh();
	if ((onlineInside = newwin(y*2/3-8, x/4-2, y/3+6, 2)) == NULL){
		endwin();
		exit(1);
	}
	refresh();
	if ((process = newwin(y*2/3, x/2, 3, x/4+1)) == NULL){
		endwin();
		exit(1);
	}
	box(process, 0, 0);
	refresh();
	if ((processInside = newwin(y*2/3-6, x/2-2, 6, x/4+2)) == NULL){
		endwin();
		exit(1);
	}
	refresh();
	scrollok(processInside, 1);
	if ((tools = newwin(y*2/3, x/4-2, 3, x*3/4+1)) == NULL){
		endwin();
		exit(1);
	}
	box(tools, 0, 0);
	refresh();
	if ((input = newwin(y/3-4, x*3/4-2, y*2/3+3, x/4+1)) == NULL){
		endwin();
		exit(1);
	}
	box(input, 0, 0);
	refresh();
	if ((inputInside = newwin(y/3-6, x*3/4-4, y*2/3+4, x/4+2)) == NULL){
		endwin();
		exit(1);
	}
	refresh();
	scrollok(inputInside, 1);

	mvwhline(stdscr, 2, 1, ACS_HLINE, x-2);
	mvwhline(board, 2, 1, ACS_HLINE, x/4-2);
	mvwhline(online, 2, 1, ACS_HLINE, x/4-2);
	mvwhline(process, 2, 1, ACS_HLINE, x/2-2);
	mvwhline(process, y*2/3-3, 1, ACS_HLINE, x/2-2);
	mvwhline(tools, 2, 1, ACS_HLINE, x/4-4);

	mvwprintw(stdscr, 1, 2, "%d:%s", myInfo.id, username);
	mvwprintw(stdscr, 1, x/4, "Rank:%d", myInfo.rank);
	mvwprintw(stdscr, 1, x/2, "Score:%d", myInfo.score);
	mvwprintw(stdscr, 1, x/2+x/4, "W%d L%d D%d", myInfo.games_won, myInfo.games_lost, myInfo.games_drawn);

	mvwaddstr(board, 1, 2, "fb:Board");
	mvwaddstr(online, 1, 2, "fo:Online List");
	mvwaddstr(process, 1, 2, "Fight Broadcast");
	mvwaddstr(tools, 1, 2, "ft:Tools");
	mvwaddstr(process, y*2/3-2, 2, "Enter=Confirm #=exit");

	// 创建收包线程
	pthread_t pid_recv;
	int err_n;
	if ((err_n = pthread_create(&pid_recv, NULL, recvMSG, NULL)) > 0) {
		printf("create recv thread error\n");
		exitProgram();
		exit(1);
	}

	refresh();
	wrefresh(board);
	wrefresh(online);
	wrefresh(process);
	wrefresh(tools);
	wrefresh(input);
	wrefresh(processInside);
	wrefresh(boardInside);
	wrefresh(onlineInside);
	
	// 创建更新信息线程
	pthread_t t_bd_ol;
	if ((err_n = pthread_create(&t_bd_ol, NULL, refreshMSG, NULL)) > 0) {
		printf("create refresh thread error\n");
		exitProgram();
		exit(1);
	}

	// 主界面逻辑
	char command[100];
	memset(command, 0, 100);
	imChallenging = 1;
	while (command[0] != '#'){
		if (challengeOK == 1) {
			memset(command, 0, 100);
			wprintw(processInside, "Battle is on! Input string: ");
			wrefresh(processInside);
			wgetstr(inputInside, command);
			wprintw(processInside, "%s\n", command);
			wrefresh(processInside);
			battle(command);
			challengeOK = 0;	//init challengeOK
			imChallenging = 1;
		}
		if (imChallenging == 0){
			memset(command, 0, 100);
			wgetstr(inputInside, command);
			continue;
		}
		else {
			memset(command, 0, 100);
			wgetstr(inputInside, command);
			wprintw(processInside, "You input: %s\n", command);
			wrefresh(processInside);
			if (command[0] == '#') break;
		}
		// ack challeng
		if (challenging == 1) {
			if (command[0] == '1') {
				challengeOK = 1;
				challenging = 0;
				ackChallenge();
				wprintw(processInside, "You accept %s 's challenge!\n", a_name);
				wrefresh(processInside);
				memset(command, 0, 100);
				wprintw(processInside, "Battle is on! Input string: ");
				wrefresh(processInside);
				wgetstr(inputInside, command);
				wprintw(processInside, "%s\n", command);
				wrefresh(processInside);
				battle(command);
				challengeOK = 0;	//init challengeOK
				continue;
			}
			else if (command[0] == '0') {
				challengeOK = 0;
				challenging = 0;
				ackChallenge();
				wprintw(processInside, "You reject %s 's challenge!\n", a_name);
				wrefresh(processInside);
				continue;
			}
			else {
				wprintw(processInside, "Input error!\nAccept[1] or Reject[0]?\n");
				wrefresh(processInside);
				continue;
			}
		}

		if (command[0] == 'f'){
			switch(command[1]){
				case 'b':getBoard(); break;
				case 'o':getOnlineList(); break;
				case 't':getTools(); break;
			}
		}
		else if (command[0] <= '9' && command[0] >= '0') {
			imChallenging = 0;
			challenge(command[0]-'0');
			wprintw(processInside, "Waiting %s ack\n", b_name);
			wrefresh(processInside);
		}
	}

	printf("exit normally\n");
	exitProgram();
}
void login(){
	// 发送缓冲区
	char buffer[MAX_PACKAGE_LEN+1];
	memset(buffer, 0, MAX_PACKAGE_LEN+1);
	// 填充头部
	struct packet_head_t *head = (struct packet_head_t *)buffer;
	head->opcode = OP_LOGIN_RQT;
	head->flags = 0;
	head->opr_cnt = 2;
	// 填充数据
	struct login_body_t *data = (struct login_body_t *)(buffer+HEAD_LEN);
	memcpy(data->username, username, 31);
	memcpy(data->password, password, 31);
	// 发送
	sendMSG(buffer, HEAD_LEN + LOGIN_BODY_LEN);
	// 接收回应
	recvMSG();
	if (loginOK == 1) {
		printw("login ok");
		showMenu();
	}
	else showLogin();
}
void regist(){
	// 发送缓冲区
	char buffer[MAX_PACKAGE_LEN+1];
	memset(buffer, 0, MAX_PACKAGE_LEN+1);
	// 填充头部
	struct packet_head_t *head = (struct packet_head_t *)buffer;
	head->opcode = OP_REGISTER_RQT;
	head->flags = 0;
	head->opr_cnt = 3;
	// 填充数据
	struct register_body_t *data = (struct register_body_t *)(buffer+HEAD_LEN);
	memcpy(data->username, username, 31);
	memcpy(data->password, password, 31);
	memcpy(data->email, email, 63);
	// 发送
	sendMSG(buffer, HEAD_LEN + REGIST_BODY_LEN);

	// 接收回应
	recvMSG();
	
	if (registOK == 1) showLogin();
	else showRegist();
}

void getBoard(){
	char buffer[MAX_PACKAGE_LEN+1];
	memset(buffer, 0, MAX_PACKAGE_LEN);

	struct packet_head_t *h = (struct packet_head_t *)buffer;
	h->opcode = OP_BOARD_RQT;
	h->flags = 0;
	h->opr_cnt = 0;

	sendMSG(buffer, HEAD_LEN);
}
void getOnlineList(){
	char buffer[MAX_PACKAGE_LEN+1];
	memset(buffer, 0, MAX_PACKAGE_LEN);

	struct packet_head_t *h = (struct packet_head_t *)buffer;
	h->opcode = OP_CURR_USRLST_RQT;
	h->flags = 0;
	h->opr_cnt = 0;

	sendMSG(buffer, HEAD_LEN);
}
void refreshBoard(void *win){
	clrWin(win);
	wrefresh(win);
	int i = 0; 
	for (; i < boardCount; i++) {
		wprintw((WINDOW *)win, "id:%d name:%s rank:%d score:%d\n", boarder[i].id, boarder[i].name, boarder[i].rank, boarder[i].score);
	}
	wrefresh(win);
}
void refreshList(void *win){
	clrWin(win);
	wrefresh(win);
	int i = 0;
	for (; i < onlineCount; i ++) {
		if (onliner[i].status == GAMING) wprintw(win, "id:%d name:%s Gaming\n", onliner[i].id, onliner[i].name);
		else wprintw((WINDOW *)win, "id:%d name:%s Idle\n", onliner[i].id, onliner[i].name);
	}
	wrefresh(win);
}
void refreshProcessChallenge(void *win){
	if (challengeOK == 1) wprintw(win, "id:%d %s accept your challenge!\n", b_id, b_name);
	else wprintw((WINDOW *)win, "id:%d %s reject your challenge!\n", b_id, b_name);
	wrefresh(win);
}
void refreshProcessAckChallenge(void *win){
	wprintw((WINDOW *)win, "id:%d %s is challeging you!\n Accept[1] or Reject[0]?\n", a_id, a_name);
	wrefresh(win);
}
void refreshProcessBattleInfo(void *win){
	wprintw((WINDOW *)win, "%s input %s, %s input %s, battle is on!\n", a_name, a_input, b_name, b_input);
	wrefresh(win);
}
void refreshProcessBattleEach(void *win){
	battleCount ++;
	if (a_id == myInfo.id) {
		switch(bs.result){
		case 0: wprintw((WINDOW *)win, "you use %s, %s use %s, you win\n", movename[bs.a_move], b_name, movename[bs.b_move]); break;
		case 1: wprintw((WINDOW *)win, "you use %s, %s use %s, you draw\n", movename[bs.a_move], b_name, movename[bs.b_move]); break;
		case 2: wprintw((WINDOW *)win, "you use %s, %s use %s, lose\n", movename[bs.a_move], b_name, movename[bs.b_move]); break;
		}
	}
	else {
		switch(bs.result){
		case 0: wprintw((WINDOW *)win, "%s use %s, you use %s, you lose\n", a_name, movename[bs.a_move], movename[bs.b_move]); break;
		case 1: wprintw((WINDOW *)win, "%s use %s, you use %s, you draw\n", a_name, movename[bs.a_move], movename[bs.b_move]); break;
		case 2: wprintw((WINDOW *)win, "%s use %s, you use %s, win\n", a_name, movename[bs.a_move], movename[bs.b_move]); break;
		}
	}
	wrefresh((WINDOW *)win);
}
void refreshProcessBattleResult(void *win){
	int w, l, d;
	if (myInfo.id == curResult.a_id) {
		w = (int)curResult.a_win;
		l = (int)curResult.a_lose;
		d = (int)curResult.a_draw;
	}
	else {
		w = (int)curResult.b_win;
		l = (int)curResult.b_lose;
		d = (int)curResult.b_draw;
	}
	if (w > l) {
		if (battleCount == 0) wprintw((WINDOW *)win, "%s escape\n", b_name);
		wprintw((WINDOW *)win, "You win %d, lose %d, draw %d\nFinal Result: You won!\n", w, l, d);
	}
	else if (w < l) wprintw((WINDOW *)win, "You win %d, lose %d, draw %d\nFinal Result: You lost!\n", w, l, d);
	else wprintw((WINDOW *)win, "You win %d, lose %d, draw %d\nFinal Result: drawn!\n", w, l, d);
	wrefresh((WINDOW *)win);
	battleCount = 0;
}
void clrWin(void *win){
	werase((WINDOW *)win);
	wrefresh(win);
}
void challenge(int index){
	char buffer[MAX_PACKAGE_LEN+1];
	memset(buffer, 0, MAX_PACKAGE_LEN+1);

	struct packet_head_t *h = (struct packet_head_t *)buffer;
	h->opcode = OP_CHALLENGE_RQT;
	h->flags = 0;
	h->opr_cnt = 4;

	struct challenge_body_t *data = (struct challenge_body_t *)(buffer+HEAD_LEN);
	data->a_id = myInfo.id;
	data->b_id = onliner[index].id;
	memcpy(data->a_name, username, strlen(username));
	memcpy(data->b_name, onliner[index].name, strlen(onliner[index].name));

	// set a_id/b_id & a_name/b_name
	a_id = myInfo.id;
	b_id = onliner[index].id;
	memset(a_name, 0, 32);
	memcpy(a_name, username, 31);
	memset(b_name, 0, 32);
	memcpy(b_name, onliner[index].name, 31);

	sendMSG(buffer, HEAD_LEN+CHALLENGE_BODY_LEN);
}
void ackChallenge(){
	char buffer[MAX_PACKAGE_LEN+1];
	memset(buffer, 0, MAX_PACKAGE_LEN+1);

	struct packet_head_t *h = (struct packet_head_t *)buffer;
	h->opcode = OP_CHALLENGE_RPL;
	if (challengeOK == 1) h->flags = FLAG_ACCEPT;
	else h->flags = FLAG_REJECT;
	h->opr_cnt = 2;

	struct challenge_ack_t *data = (struct challenge_ack_t *)(buffer+HEAD_LEN);
	data->a_id = a_id;
	data->b_id = myInfo.id;

	sendMSG(buffer, HEAD_LEN+CHALLENGE_ACK_LEN);
}
void getTools(){
}
void battle(char *command){
	char buffer[MAX_PACKAGE_LEN+1];
	memset(buffer, 0, MAX_PACKAGE_LEN+1);

	struct packet_head_t *h = (struct packet_head_t *)buffer;
	h->opcode = OP_BATTLE_INPUT;
	h->flags = 0;
	h->opr_cnt = 2;

	struct battle_body_t *data = (struct battle_body_t *)(buffer+HEAD_LEN);
	data->id = myInfo.id;
	memcpy(data->input, command, 31);

	sendMSG(buffer, HEAD_LEN + BATTLE_BODY_LEN);
}
void escape(){
}
void *refreshMSG(){
	while (1) {
		getBoard();
		getOnlineList();
		sleep(1);
	}
	pthread_exit(NULL);
}
