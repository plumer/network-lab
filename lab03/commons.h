#ifndef __COMMONS_H__
#define __COMMONS_H__

#include <stdint.h>
#include <assert.h>

// 定义最大包长度为4096个Byte
#define MAX_PACKAGE_LEN 4096

// 定义包头&长度
struct packet_head_t {
	uint8_t opcode;
	uint8_t flags;
	uint8_t opr_cnt;
};
#define HEAD_LEN sizeof(struct packet_head_t)

// 定义登录data&长度
struct login_body_t {
	char username[32];
	char password[32];
};
#define LOGIN_BODY_LEN sizeof(struct login_body_t)

struct login_ack_body_t {
	uint16_t id;
	uint32_t score;
	uint32_t rank;
	uint32_t games_won;
	uint32_t games_lost;
	uint32_t games_drawn;
};

// 定义注册data&长度
struct register_body_t {
	char username[32];
	char password[32];
	char email[64];
};
#define REGIST_BODY_LEN sizeof(struct register_body_t)

// 定义在线item&长度&最大返回条目数
#define GAMING 100
#define IDLE 101

#define MAX_ONLINE_RESULT 128	//最多返回在线的128人
struct online_item_t{
	uint16_t id;
	uint16_t status;		// whether the user is in a game
	char name[32];
};
#define ONLINE_ITEM_LEN sizeof(struct online_item_t)

struct board_item_t {
	uint16_t id;
	uint32_t rank;
	uint32_t score;
	char name[32];
};
#define BOARD_ITEM_LEN sizeof(struct board_item_t)

struct challenge_body_t {
	uint16_t a_id;
	uint16_t b_id;
	char a_name[32];
	char b_name[32];
};
#define CHALLENGE_BODY_LEN sizeof(struct challenge_body_t)

struct challenge_ack_t {
	uint16_t a_id;
	uint16_t b_id;
};
#define CHALLENGE_ACK_LEN sizeof(struct challenge_ack_t)

struct battle_body_t {
	uint16_t id;
	char input[32];	//battle input
};
#define BATTLE_BODY_LEN sizeof(struct battle_body_t)
struct battle_info_t {
	uint16_t a_id;
	char a_input[32];
	uint16_t b_id;
	char b_input[32];
};
#define BATTLE_INFO_LEN sizeof(struct battle_info_t)
struct battle_each_t {
	uint16_t a_id;
	uint8_t a_move;
	uint16_t b_id;
	uint8_t b_move;
	uint8_t result;	//0: a win; 1: a lose 2: draw
};
#define BATTLE_EACH_LEN sizeof(struct battle_each_t)
struct battle_result_t {
	uint16_t a_id;
	uint8_t a_win;
	uint8_t a_lose;
	uint8_t a_draw;
	uint16_t b_id;
	uint8_t b_win;
	uint8_t b_lose;
	uint8_t b_draw;
};
#define BATTLE_RESULT_LEN sizeof(struct battle_result_t)
/////////////////////////////////////////////
// Operation code between server and client

#define OP_REGISTER_RQT		16
#define OP_REGISTER_RPL		17
#define OP_LOGIN_RQT		18
#define OP_LOGIN_RPL		19
#define OP_LOGOUT_RQT		20
#define OP_LOGOUT_RPL		21

#define OP_CURR_USRLST_RQT	22
#define OP_CURR_USRLST_RPL	23
#define OP_BOARD_RQT		24
#define OP_BOARD_RPL		25

// this part may be refined furthur.
#define OP_BATTLE_INPUT		26
#define OP_BATTLE_MSG		27
#define OP_BATTLE_RESULT	28

// Operation code between client and client

#define OP_CHALLENGE_RQT	64
#define OP_CHALLENGE_RPL	65

// FLAGS

#define FLAG_ACCEPT			16
#define FLAG_REJECT			17
#define FLAG_FLED			18

#define FLAG_BATTLE_INTRO	24
#define FLAG_BATTLE_EACH	25

// error number

#define ERRNO_POOL_FULL		16
#define ERRNO_USERNAME_CONFLICT	17	
#define ERRNO_USER_NOT_EXIST	18
#define ERRNO_PASSWD_FAIL		19

// game role

#define ROLE_CHALLENGER			200
#define ROLE_CHALLENGEE			201
#define ROLE_UNDEFINED			202

// game


#endif
