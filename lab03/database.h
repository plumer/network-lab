#ifndef __DATABASE_H__
#define __DATABASE_H__

#define POOL_MAX 64

struct user_meta {
	uint16_t id;
	uint16_t isUsed;	// non-zero:该用户内存空间被占用
	uint16_t isOnline;
	uint16_t isGaming;
	uint32_t score;
	uint32_t rank;
	uint32_t games_won;
	uint32_t games_lost;
	uint32_t games_drawn;
	uint32_t reserved;	// 就是为了填满160字节的，156字节看着难受uint16_t
}; // 32 Bytes

struct user {
	uint16_t id;
	uint16_t isUsed;	// non-zero:该用户内存空间被占用
	uint16_t isOnline;
	uint16_t isGaming;
	uint32_t score;
	uint32_t rank;
	uint32_t games_won;
	uint32_t games_lost;
	uint32_t games_drawn;
	uint32_t reserved;	// 就是为了填满160字节的，156字节看着难受
	char name[32];
	char passwd[32];
	char email[64];
};	// 160 Bytes per user

void	init_pool();

// if success, 0 is returned. If not, error code is returned.
int		add_new_user(
		const char *name, const char * passwd, const char *email);
int		load_user(const struct user *);

int		get_id_by_name(const char *name);		// -1 returned on fail
const struct user *
		locate_user(int id);

#define USER_ONLINE 1
#define USER_OFFLINE 0
void	set_online_status(int id, int status);
int		is_online(int id);

#define USER_GAMING 1
#define USER_IDLE 0
// set will fail when user is not online
void	set_gaming_status(int id, int status);
int		is_gaming(int id);

// only increment is passed in
void	update_statistics(
		int id, int win, int lose, int draw);
#endif
