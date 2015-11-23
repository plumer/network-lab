#include "commons.h"
#include "database.h"
#include <string.h>
#include <stdio.h>


struct user user_pool[POOL_MAX];
static volatile int user_cnt = 0;	// number of users in database

void 
init_pool() {
	memset(user_pool, 0, sizeof(user_pool));
	int i;
	for (i = 0; i < POOL_MAX; ++i) {
		user_pool[i].id = i;
		user_pool[i].isUsed = 0;
	}
}

int 
add_new_user(const char * name, const char * passwd, const char * email) {
	if (user_cnt == POOL_MAX)
		return ERRNO_POOL_FULL;
	int i;
	for (i = 0; i < user_cnt; ++i) {
		if (strncmp(user_pool[i].name, name, 31) == 0)
			return ERRNO_USERNAME_CONFLICT;
	}

	struct user * new_user = user_pool + user_cnt;
	user_cnt++;	// this is in critical area;
	new_user -> isUsed = 1;
	strncpy(new_user -> name, name, 31);
	strncpy(new_user -> passwd, passwd, 31);
	strncpy(new_user -> email, email, 63);
	(new_user->name)[31] = (new_user->passwd)[31] = (new_user->email)[63] = '\0';
	return 0;
}

int
load_user(const struct user * u) {
	if (u -> id >= POOL_MAX || u->id < 0) {
		printf("loading failure: %d\n", u->id);
		return -1;
	}
	struct user * up = &user_pool[u->id];
	memcpy(up, u, sizeof(struct user));
	up->isUsed = 1;
	user_cnt++;
	printf("loading success: %d, %s, %s, total number of users: %d\n", 
			up -> id, up-> name, up->passwd, user_cnt);
	return 0;
}

int
get_id_by_name(const char * name) {
	int i;
	for (i = 0; i < POOL_MAX; ++i) {
		if (strncmp(user_pool[i].name, name, 31) == 0)
			return i;
	}
	return -1;
}

const struct user *
locate_user(int id) {
	if (id >= POOL_MAX || id < 0)
		return (void *) 0;
	else
		return user_pool + id;
}

void
set_online_status(int id, int status) {
	if (status == 0) {
		user_pool[id].isOnline = 0;
		set_gaming_status(id, 0);
	} else
		user_pool[id].isOnline = 1;
}

int
is_online(int id) {
	return user_pool[id].isOnline;
}

void
set_gaming_status(int id, int status) {
	if (status == 0 || !user_pool[id].isOnline)
		user_pool[id].isGaming = USER_IDLE;
	else
		user_pool[id].isGaming = USER_GAMING;
}

int
is_gaming(int id) {
	return user_pool[id].isGaming;
}

void
update_statistics(int id, int win, int lose, int draw) {
	user_pool[id].games_won += win;
	user_pool[id].games_lost += lose;
	user_pool[id].games_drawn += draw;
	// to be continued with score and rank
	user_pool[id].score += (win * 3 + draw - lose * 2);
}


