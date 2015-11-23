#ifndef __COMMONS_H__
#define __COMMONS_H__

#include <stdint.h>

struct packet_head_t {
	uint8_t opcode;
	uint8_t flags;
	uint8_t opr_cnt;
};

struct login_body_t {
	char user_name[32];
	char password[32];
};

struct register_body_t {
	char user_name[32];
	char password[32];
	char email[64];
};

// Operation code between server and client

#define OP_REGISTER_RQT		16
#define OP_REGISTER_RPL		17
#define OP_LOGIN_RQT		18
#define OP_LOGIN_RPL		19
#define OP_LOGOUT_RQT		20
#define OP_LOGOUT_RPL		21

#define OP_CURR_USRLST_RQT	22
#define OP_CURR_USRLST_RPL	23
#define OP_RANKING_RQT		24
#define OP_RANKING_RPL		25

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

#define FLAG_BATTLE_INTRO	24
#define FLAG_BATTLE_EACH	25

// error number

#define ERRNO_POOL_FULL		16
#define ERRNO_USERNAME_CONFLICT	17	
#define ERRNO_USER_NOT_EXIST	18
#define ERRNO_PASSWD_FAIL		19
#endif
