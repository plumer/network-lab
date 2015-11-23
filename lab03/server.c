#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

#include <signal.h>
#include <unistd.h>

#include "commons.h"
#include "database.h"
#include "game.h"

#define MAXLINE 4096
#define NUM_THREADS 256
#define SERV_PORT 23333
#define LISTENQ 8

struct thread_arg_t{
	long thread_id; 
	int connfd;
	int game_role;
	uint16_t user_id;

	struct thread_arg_t * opponent_arg;
	pthread_mutex_t mtx;
	pthread_cond_t challengee_input;
	int has_opponent_input;
	int battle_end;
	char opponent_input[32];
};

pid_t thread_ids[NUM_THREADS];
struct thread_arg_t* thread_args[NUM_THREADS];

void terminate(int sig);
static volatile int continue_running = 1;
void *handle_client(void *threadarg);

void load_user_info();
void save_user_info();

int registrate(char * packet, const char * usrn, const char *pswd, const char *em);
int login(char * packet, const char *usrn, const char *pswd);
int board_list(char * packet);
int online_list(char * packet);
int challenge(char *packet, int challengerID, int challengeeID);
int fight(char *sendline, char * a_input, char * b_input, 
		struct thread_arg_t * challenger, struct thread_arg_t * challengee);

int main(int argc, char **argv) {
	int listenfd, connfd;
	socklen_t clilen;
	struct sockaddr_in cliaddr, servaddr;
	
	listenfd = socket (AF_INET, SOCK_STREAM, 0);

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(SERV_PORT);
	
	const int on = 1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &(on), sizeof(on));

	bind (listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
	listen(listenfd, LISTENQ);
	printf("Server running... waiting for connections.\n");
	

	(void) signal(SIGINT, terminate);
	

	int num_of_threads = 0;
	init_pool();
	load_user_info();
	while (continue_running) {
		clilen = sizeof(cliaddr);

		connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
		printf("Received request..\n");

		// prepare thread argument
		thread_args[num_of_threads] = (struct thread_arg_t *)malloc(sizeof(struct thread_arg_t));
		thread_args[num_of_threads] -> connfd = connfd;
		int child_thread = pthread_create(
				&(thread_args[num_of_threads]->thread_id), 
				NULL, handle_client, 
				(void *)(thread_args[num_of_threads]) );

		num_of_threads++;
	}
	printf("free!\n");
	return 0;
}

void terminate(int sig) {
	continue_running = 0;
	save_user_info();
	exit(1);
}

void *handle_client(void *threadarg) {

	struct thread_arg_t * args = (struct thread_arg_t *) threadarg;
	int connfd = args -> connfd;
	args -> user_id = -1;
	long tid = args -> thread_id;
	args -> opponent_arg = NULL;
	args -> game_role = ROLE_UNDEFINED;
	args -> has_opponent_input = 0;
	memset(args -> opponent_input, 0 , sizeof(args -> opponent_input));

	printf("thread id: #%ld\n", tid);
	printf("Child thread created for dealing with client requests\n");
	int n;
	char recvline[MAXLINE];
	char sendline[MAXLINE];
	while ( (n = recv(connfd, recvline, MAXLINE, 0)) > 0) {
	//	printf("Packe t length: %d \n", n); 
		memset(sendline, 0, MAXLINE);

		struct packet_head_t * pac_head = (struct packet_head_t *)recvline;

		if (pac_head -> opcode == OP_REGISTER_RQT) {

			struct register_body_t * reg_info =
				(struct register_body_t *)(recvline + HEAD_LEN);
			int sendlen = registrate(sendline,
					reg_info -> username, reg_info -> password, reg_info -> email);
			send(connfd, sendline, sendlen, 0);

		} else if (pac_head -> opcode == OP_LOGIN_RQT) {

			struct login_body_t * login_info = 
				(struct login_body_t *) (recvline + HEAD_LEN);
			int sendlen = login(sendline, 
					login_info -> username, login_info -> password);
			send(connfd, sendline, sendlen, 0);
			if (sendlen == HEAD_LEN + sizeof(struct login_ack_body_t)) {
				// login success
				args -> user_id = get_id_by_name(login_info -> username);
				printf("user logging in success, id = #%d\n", args -> user_id);
			}

		} else if (pac_head -> opcode == OP_BOARD_RQT){
			
			int sendlen = board_list(sendline);
			send(connfd, sendline, sendlen, 0);

		} else if (pac_head -> opcode == OP_CURR_USRLST_RQT) {

			int sendlen = online_list(sendline);
			send(connfd, sendline, sendlen, 0);

		} else if (pac_head -> opcode == OP_CHALLENGE_RQT) {

			char * tmp = recvline + HEAD_LEN;
			uint16_t a_id = *(uint16_t *)(tmp);
			uint16_t b_id = *(uint16_t *)(tmp + sizeof(uint16_t));

			printf("** challenging info: challenger #%d, receptor #%d\n",
					a_id, b_id);

			memcpy(sendline, recvline, n);
			struct thread_arg_t * dest_args = NULL;
			int i;
			for (i = 0; i < NUM_THREADS; ++i) if (thread_args[i]) {
				printf("** current args: user_id = #%d\n", thread_args[i] -> user_id);
				if (thread_args[i] -> user_id == b_id) { // yes we've found it
					printf("** challenging info: dest user #%d found\n", b_id);
					dest_args = thread_args[i];
					break;
				}
			}
			if (dest_args == NULL) {
				printf("** challenge error: the dest user is not online\n");
			} else if ( !locate_user(dest_args -> user_id) -> isOnline ) {
				printf("** challenge error: the dest user is not online, but the server is not running correctly\n");
			} else if ( locate_user(dest_args -> user_id) -> isGaming ) {
				printf("** challenge error: the dest user is gaming, not able to accept challenge\n");
			} else {
				// retransmit this to the dest user
				send(dest_args -> connfd, sendline, n, 0); 
			}

		} else if (pac_head -> opcode == OP_CHALLENGE_RPL) {
			
			// resend this package to the challenger whatsoever

			uint16_t challenger_id = *(uint16_t *)(recvline + HEAD_LEN);
			
			int i = 0;
			for (; i < NUM_THREADS; ++i) if (thread_args[i]) {
				if (thread_args[i] -> user_id == challenger_id) {
					args -> opponent_arg = thread_args[i];
					break;
				}
			}
			assert(args -> opponent_arg);
			memcpy(sendline, recvline, n);
			send(args -> opponent_arg -> connfd, sendline, n, 0);

			// do some flag modification

			if (pac_head -> flags == FLAG_REJECT) {
				printf("** challenge fail: the dest user rejected\n");
			} else if (pac_head -> flags == FLAG_ACCEPT) {
				args -> has_opponent_input = 0;						//TODO:critical area
				args -> game_role = ROLE_CHALLENGEE;
				set_gaming_status(args -> user_id, USER_GAMING);

				args -> opponent_arg -> opponent_arg = args;
				args -> opponent_arg -> game_role = ROLE_CHALLENGER;
				set_gaming_status(args -> opponent_arg -> user_id, USER_GAMING);
			}
		} else if (pac_head -> opcode == OP_BATTLE_INPUT) {
			char input[32];
			struct battle_body_t * battle_input = (struct battle_body_t *)(recvline + HEAD_LEN);
			memcpy(input, battle_input -> input, sizeof(input));

			if (args -> game_role == ROLE_CHALLENGER) {
				int counter = 0;
				while (!(args -> has_opponent_input)) {
					counter++;
					if (counter >= 3) {
						counter = 10;
						break;
					}
					sleep(1);
				}

				if (counter >= 10) {
					printf("challengee does not input\n");
					memset(sendline, 0, MAXLINE);
					struct packet_head_t* p = (struct packet_head_t *)sendline;
					p -> opcode = OP_BATTLE_RESULT;
					p -> flags = FLAG_FLED;
					p -> opr_cnt = 8;
					struct battle_result_t * br = (struct battle_result_t *)(sendline + HEAD_LEN);
					br -> a_id = args -> user_id;
					br -> b_id = args -> opponent_arg -> user_id;
					br -> a_win = br -> b_lose = 8;
					br -> a_lose = br -> b_win = 0;
					br -> a_draw = br -> b_draw = 0;
					update_statistics(args -> user_id, 8,0,0);
					update_statistics(args -> opponent_arg -> user_id, 0, 0, 8);
					send(args -> connfd, sendline, HEAD_LEN + BATTLE_RESULT_LEN, 0);
					send(args -> opponent_arg -> connfd, sendline, HEAD_LEN + BATTLE_RESULT_LEN, 0);
				} else {
					fight(sendline, input, args->opponent_input, args, args -> opponent_arg);
				}
				
				args -> opponent_arg -> battle_end = 1;
				// challenger's clean up
				args -> game_role = ROLE_UNDEFINED;
				args -> opponent_arg = NULL;
				args -> has_opponent_input = 0;
				set_gaming_status(args -> user_id, USER_IDLE);
				memset(args -> opponent_input, 0, sizeof(args -> opponent_input));

			} else if (args -> game_role == ROLE_CHALLENGEE) {
				args -> battle_end = 0;
				args -> opponent_arg -> has_opponent_input |= 1;
				memcpy( input, args -> opponent_arg -> opponent_input, sizeof(input));
				
				while (!args -> battle_end) {
					sleep(1);
				}

				// receptor's clean up
				args -> game_role = ROLE_UNDEFINED;
				args -> opponent_arg = NULL;
				set_gaming_status(args -> user_id, USER_IDLE);

			} else {
				assert(0);
			}


		} else{
			printf("unrecognized opcode: %d\n", pac_head -> opcode);
		}
		memset(recvline, 0, MAXLINE);
	}
	if (n == 0) {
		if (args -> user_id != -1) {
			set_online_status(args -> user_id, USER_OFFLINE);
		}
		args -> user_id = -1;
	}
	if (n < 0)
		printf("Read error\n");
	pthread_exit(0);
}


int registrate(char * packet, const char *username, const char * passwd, const char * email) {
	printf("** registration, '%s', '%s', '%s'\n",
			username, passwd, email);

	// try to add user using add_new_user
	struct packet_head_t * pac_head = (struct packet_head_t *) packet;
	int result = add_new_user(username, passwd, email);	
	
	pac_head -> opcode = OP_REGISTER_RPL;
	pac_head -> opr_cnt = 1;
	*(uint8_t *)(packet + HEAD_LEN) = result;
	if (result == 0) {	// success
		pac_head -> flags = FLAG_ACCEPT;
	} else {			// fail
		printf("** registration error, error code: %d", result);
		pac_head -> flags = FLAG_REJECT;
	}
	return HEAD_LEN + sizeof(uint8_t);
}

int login(char *packet, const char *username, const char *passwd) {
	printf("[ login() ]: login, '%s', '%s'\n", username, passwd);					// debugging
	int test_id = get_id_by_name(username);
	struct packet_head_t * pac_head = (struct packet_head_t *) packet;
	
	pac_head -> opcode = OP_LOGIN_RPL;
	pac_head -> opr_cnt = 1;
	if (test_id == -1) {					// didn't find the user by name
		printf("** login error : user %s not exist\n", username);
		pac_head -> flags = FLAG_REJECT;
		*(uint8_t *)(packet + HEAD_LEN) = ERRNO_USER_NOT_EXIST;
		return HEAD_LEN + sizeof(uint8_t);
	}
	// now user exists
	const struct user * user_info = locate_user(test_id);
	int cmp_result = strncmp(passwd, user_info -> passwd, 32-1);	// compare passwd
	if (cmp_result != 0) {
		printf("** login error: password do not match\n");
		pac_head -> flags = FLAG_REJECT;
		*(uint8_t *)(packet + HEAD_LEN) = ERRNO_PASSWD_FAIL;
		return HEAD_LEN + sizeof(uint8_t);
	} else {
		pac_head -> flags = FLAG_ACCEPT;
		printf("[ login() ]: login success: ");
		set_online_status(test_id, USER_ONLINE);
		

		struct login_ack_body_t * ack_info = (struct login_ack_body_t *)(packet+HEAD_LEN);
		ack_info -> id = user_info -> id;
		ack_info -> score = user_info -> score;
		ack_info -> rank = user_info -> rank;
		ack_info -> games_won = user_info -> games_won;
		ack_info -> games_lost = user_info -> games_lost;
		ack_info -> games_drawn = user_info -> games_drawn;

		printf("userid: %d, isOnline: %d, score: %d\n",
				ack_info->id, user_info->isOnline, ack_info->score);
		return HEAD_LEN + sizeof(struct login_ack_body_t);
	}
}

int board_list(char * packet) {
//	printf("** getting board list\n");
	struct packet_head_t * pac_head = (struct packet_head_t *) packet;

	pac_head -> opcode = OP_BOARD_RPL;
	pac_head -> opr_cnt = 0;

	struct board_item_t * rank_info[POOL_MAX];

	int i, rank_i = 0;
	for (i = 0; i < POOL_MAX; ++i) {
		const struct user * u = locate_user(i);
		if ( u -> isUsed ) {
			rank_info[i] = (struct board_item_t *)malloc(sizeof(struct board_item_t));
			rank_info[i] -> id = u -> id;
			rank_info[i] -> rank = u -> rank;
			rank_info[i] -> score = u -> score;
			memcpy(rank_info[i] -> name, u -> name, sizeof(u->name)-1);
			rank_i ++;
		}
	}

	pac_head -> opr_cnt = rank_i;
	
	// selection sorting
	int j;
	struct board_item_t * tmp;
	for (i = 0; i < rank_i; ++i) {
		int greatest_index = i;
		for (j = i+1; j < rank_i; ++j) {
			if ( rank_info[j] -> score > rank_info[greatest_index] -> score ) {
				greatest_index = j;
			}
		}
		if (greatest_index != i) {
			tmp = rank_info[i];
			rank_info[i] = rank_info[greatest_index];
			rank_info[greatest_index] = tmp;
		}
		rank_info[i] -> rank = i+1;
	}
	
	// copy to packet
	tmp = (struct board_item_t *)(packet + HEAD_LEN);
	for (i = 0; i < rank_i; ++i) {
		memcpy(tmp, rank_info[i], sizeof(struct board_item_t));
		tmp++;
	}
	return (char *)tmp - packet;
}

int online_list(char *packet) {
//	printf("** getting online user list\n");
	struct packet_head_t * pac_head = (struct packet_head_t *) packet;
	pac_head -> opcode = OP_CURR_USRLST_RPL;
	struct online_item_t * tmp = (struct online_item_t *) (packet + HEAD_LEN);

	int count = 0, i;
	for (i = 0; i < POOL_MAX; ++i) {
		const struct user * u = locate_user(i);
		if ( u -> isUsed && u -> isOnline ) {
			tmp -> id = u -> id;
			tmp -> status = u -> isGaming;
			memcpy(tmp -> name, u -> name, sizeof(u->name)-1);
			tmp -> name[sizeof(u->name)-1] = '\0';
			tmp ++;
			count++;
		}
	}

	pac_head -> opr_cnt = count;

	return ((char *) tmp) - packet;
}

void load_user_info() {
	const char * file_name = "user.dat";
	FILE * f = fopen(file_name, "r");
	if (!f) return;

	// file exists
	struct user buf;
	char str[MAXLINE];
	while ( !feof(f) ) {
		memset(&buf, 0, sizeof(buf));
		int id_tmp;
		int continue_reading = fscanf(f, "%d,%d,%d,%d,",
				&id_tmp, &buf.games_won, &buf.games_lost, &buf.games_drawn);
		buf.id = (uint16_t)id_tmp;
		if (continue_reading < 4 || continue_reading == EOF) {
			break;
		}
		fgets(str, MAXLINE, f);
		char * username = strtok(str, ",");
		memcpy(buf.name, username, strlen(username));
		char * password = strtok(NULL, ",");
		memcpy(buf.passwd, password, strlen(password));
		char * email = strtok(NULL, ",");
		memcpy(buf.email, email, strlen(email));
		load_user(&buf);
		printf("user: #%d, %s, %s, %s\n", buf.id, buf.name, buf.passwd, buf.email);
	}

	fclose(f);
}

void save_user_info() {
	const char *file_name = "user.dat";
	FILE *f = fopen(file_name, "w+");
	int i = 0;
	for (i = 0; i < POOL_MAX; ++i) {
		const struct user * u = locate_user(i);
		if ( u->isUsed)
			fprintf(f, "%d,%d,%d,%d,%s,%s,%s",
					u->id, u->games_won, u->games_lost, u->games_drawn,
					u->name, u->passwd, u->email);
	}
	fclose(f);
}

int fight(char * sendline, char * a_input, char * b_input,
		struct thread_arg_t * a_args, struct thread_arg_t *b_args) {
	struct packet_head_t * send_pac_head;
	uint32_t a_moves = generate_hash(a_input);
	uint32_t b_moves = generate_hash(b_input);
	int move_number = 0;

	int awincount = 0, alosecount = 0, drawcount = 0;
	for (; move_number < MOVES_TOTAL; ++move_number) {
		int result = rpsls(a_moves % 5, b_moves % 5);
		if (result == A_WIN) {
			awincount++;
		} else if(result == B_WIN) {
			alosecount++;
		} else {
			drawcount++;
		}

		memset(sendline, 0, MAXLINE);
		send_pac_head = (struct packet_head_t *)(sendline);
		send_pac_head -> opcode = OP_BATTLE_MSG;
		send_pac_head -> flags = FLAG_BATTLE_EACH;
		send_pac_head -> opr_cnt = 5;

		struct battle_each_t * battle_each_info = (struct battle_each_t *)(sendline + HEAD_LEN);
		battle_each_info -> a_id = a_args -> user_id;
		battle_each_info -> a_move = a_moves % 5;
		battle_each_info -> b_id = b_args -> user_id;
		battle_each_info -> b_move = b_moves % 5;
		battle_each_info -> result = result;
		
		printf("[ %s ], sending each info: (%d, %d), (%d, %d), %d\n",
				__FUNCTION__, battle_each_info -> a_id, battle_each_info -> a_move,
				battle_each_info -> b_id, battle_each_info -> b_move, battle_each_info -> result);

		send(a_args -> connfd, sendline, HEAD_LEN + BATTLE_EACH_LEN, 0);
		send(b_args -> connfd, sendline, HEAD_LEN + BATTLE_EACH_LEN, 0);
		
		sleep(1);

		a_moves /= 5;
		b_moves /= 5; 
	}
	update_statistics(a_args -> user_id, awincount, alosecount, drawcount);
	update_statistics(b_args -> user_id, alosecount, awincount, drawcount);

	memset(sendline, 0, MAXLINE);
	send_pac_head = (struct packet_head_t *)(sendline);
	send_pac_head -> opcode = OP_BATTLE_RESULT;
	send_pac_head -> flags = 0;
	send_pac_head -> opr_cnt = 8;
	
	struct battle_result_t * bat_res = (struct battle_result_t *)(sendline + HEAD_LEN);
	bat_res -> a_id = a_args -> user_id;
	bat_res -> a_win = awincount;
	bat_res -> a_lose = alosecount;
	bat_res -> a_draw = drawcount;
	bat_res -> b_id = b_args -> user_id;
	bat_res -> b_win = alosecount;
	bat_res -> b_lose = awincount;
	bat_res -> b_draw = drawcount;

	send(a_args -> connfd, sendline, HEAD_LEN + BATTLE_RESULT_LEN, 0);
	send(b_args -> connfd, sendline, HEAD_LEN + BATTLE_RESULT_LEN, 0);

	return 0;
}	
