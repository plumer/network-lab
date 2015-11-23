#include "fileio.h"
#include "sha1.h"
#include "btdata.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>


// returns the temp name of corresponding file name.
// the string is dynamically allocated.
// caller should manually free the memory.
char *
append_postfix(const char * str, const char * postfix) {
	int new_str_len = strlen( str ) + strlen( postfix ) + 1;
	char * new_str = (char *)malloc( sizeof(char) * new_str_len );
	memset( new_str, 0, sizeof(char) * new_str_len );
	strncpy( new_str, str, strlen( str ) );
	strncat( new_str, postfix, strlen( postfix ));
	assert( new_str[new_str_len-1] == 0);
	return new_str;
}

char *
get_temp_file_name(const char * file_name) {
	return append_postfix(file_name, ".bt");
}

char *
get_config_file_name(const char * file_name) {
	return append_postfix(file_name, ".bt.cfg");
}

FILE *
create_file(const char * file_name, uint32_t size) {
	char * temp_file_name = get_temp_file_name(file_name);
	FILE * f = fopen(temp_file_name, "w");
	int ret_size = truncate(temp_file_name, size);
	if (ret_size == size)
		return f;
	else
		return NULL;
}

int
write_file(file_progress_t *fp, const char * content, uint32_t start, uint32_t length) {
	char * temp_file_name = get_temp_file_name(fp -> file_name);

	FILE * f = fopen(temp_file_name, "r+");
	if ( !f ) {
		printf("temp file of \'%s\' notexist\n", fp -> file_name);
		return -1;
	}

	fseek(f, start, SEEK_SET);
	fwrite(content, sizeof(char), length, f);
	fclose(f);

	// set correct complete bits

	if ( (start - (fp -> piece_length - fp -> start_offset) ) % fp -> piece_length != 0 )
		//uh-oh
		;
	else {
		int start_index = (start - fp -> start_offset) / fp -> piece_length;
		if ( length % fp -> piece_length == 0 ) {
			int num_completed_pieces = length / fp -> piece_length;
			int i = 0;
			for (; i < num_completed_pieces; ++i) { // TODO: bound not clear
				fp -> piece_progress[i].completed = 1;
			}
		} else {
			// TODO: what if end_offset
		}
	}
	free(temp_file_name);
	return 1;
}

int complete_file(file_progress_t * fp) {
	int complete = 1;
	int i = 0;
	for (; i < fp -> num_pieces; ++i) {
		if (fp -> piece_progress[i].completed == 0) {
			//printf("file '%s' at piece %d incomplete\n", fp -> file_name,
			//		fp -> piece_progress[i].piece_index);
			complete = 0;
			break;
		}
	}
	if (complete == 0) {
		return -1;
	} else {
		const char * temp_file_name = get_temp_file_name(fp -> file_name);
		FILE * test_temp_file = fopen(temp_file_name, "r");
		if ( !test_temp_file )
			return -1;
		else
			fclose(test_temp_file);
		int r = rename(temp_file_name, fp -> file_name);
		if (r == 0) {
			char * cfg_name = get_config_file_name(fp -> file_name);
			remove(cfg_name);
			return 1;
		}
		else
			return -1;
	}
}

file_progress_t *
create_progress(const char * file_name, uint32_t startfrom,
		uint32_t length, uint32_t piece_length) {
	file_progress_t * new_fp = (file_progress_t *)malloc( sizeof(file_progress_t) );
	memset(new_fp, 0, sizeof(file_progress_t) );
	assert( strlen(file_name) < sizeof( new_fp -> file_name ) / sizeof(char) );
	strcpy(new_fp -> file_name, file_name);

	new_fp -> start_offset = startfrom % piece_length;
	new_fp -> end_offset = (startfrom + length) % piece_length;
	if (new_fp -> end_offset == 0)
		new_fp -> end_offset = piece_length;
	new_fp -> piece_length = piece_length;
	new_fp -> num_pieces = (length + new_fp -> start_offset + piece_length - 1)
		/ piece_length;

	int start_piece_index = startfrom / piece_length;
	new_fp -> piece_progress = (piece_progress_t *)
		malloc (sizeof(piece_progress_t) * new_fp -> num_pieces);
	int i;
	for (i = 0; i < new_fp -> num_pieces; ++i) {
		new_fp -> piece_progress[i].piece_index = start_piece_index + i;
		new_fp -> piece_progress[i].completed = 0;
	}
	assert(new_fp -> piece_progress[ new_fp -> num_pieces-1 ].piece_index ==
			(startfrom + length + piece_length - 1)/piece_length - 1);
	assert(length == new_fp -> num_pieces * new_fp -> piece_length -
			new_fp -> start_offset - (new_fp -> piece_length - new_fp -> end_offset));
	new_fp -> length = length;
	pthread_mutex_init( &(new_fp -> mutex), NULL);
	return new_fp;
}

int
write_progress(file_progress_t *dp) {
	char * progress_file_name = get_config_file_name(dp -> file_name);

	FILE * progress_file = fopen(progress_file_name, "w+");
	if (!progress_file) {
		printf("progres file name cannot open\n");
		return -1;
	}

	fprintf(progress_file, "%s\n", dp -> file_name);
	fprintf(progress_file, "%d*%d\n", dp -> num_pieces, dp -> piece_length);
	fprintf(progress_file, "%d-%d\n", dp -> start_offset, dp -> end_offset);
	int i;
	for (i = 0; i < dp -> num_pieces; ++i) {
		fprintf(progress_file, "%d,%d\n",
				dp -> piece_progress[i].piece_index,
				dp -> piece_progress[i].completed);
	}
	fclose(progress_file);
	free(progress_file_name);
	return 1;
}

file_progress_t *
read_progress(const char * file_name) {
//	const char * file_name = g_torrentmeta -> name;
	char * progress_file_name = get_config_file_name(file_name);

	FILE * progress_file = fopen(progress_file_name, "r");
	if (!progress_file)
		return NULL;
	char buf[1024];
	file_progress_t * dp = (file_progress_t *)malloc( sizeof(file_progress_t) );
	memset( dp, 0, sizeof(dp) );

	// get file name
	memset(buf, 0, sizeof(buf));
	if ( fgets(buf, sizeof(buf), progress_file) == NULL) return NULL;
	if ( strlen(buf) != strlen(file_name) + 1 ||
		 strncmp(buf, file_name, strlen(file_name)) ) {
		printf("warning: file name mismatch\n");
	}
	strncpy(dp -> file_name, file_name, strlen(file_name));

	// num_pieces and piece length
	memset(buf, 0, sizeof(buf));
	if ( fgets(buf, sizeof(buf), progress_file) == NULL) return NULL;
	char *p = strtok(buf, "*");
	dp -> num_pieces = atoi(p);
	p = strtok(NULL, "*");
	dp -> piece_length = atoi(p);

	// start and end offset
	memset(buf, 0, sizeof(buf));
	if ( fgets(buf, sizeof(buf), progress_file) == NULL) return NULL;
	p = strtok(buf, "-");
	dp -> start_offset = atoi(p);
	p = strtok(NULL, "-");
	dp -> end_offset = atoi(p);

	dp -> piece_progress = malloc( sizeof( *(dp -> piece_progress) ) * dp -> num_pieces );
	memset(dp -> piece_progress, 0, sizeof(*(dp -> piece_progress) ) * dp -> num_pieces );
	int i = 0;
	for (; i < dp -> num_pieces; ++i) {
		memset(buf, 0, sizeof(buf));
		if (fgets(buf, sizeof(buf), progress_file) == NULL) return NULL;
		p = strtok(buf, ",");
		dp -> piece_progress[i].piece_index = atoi(p);
		p = strtok(NULL, ",");
		dp -> piece_progress[i].completed = atoi(p);
	}
	fclose(progress_file);
	free(progress_file_name);
	pthread_mutex_init( &(dp -> mutex), NULL);
	dp -> length = dp -> num_pieces * dp -> piece_length - dp -> start_offset - (dp -> piece_length - dp -> end_offset);
	return dp;

}

file_progress_t *
find_progress(const char * file_name) {
	int i;
	for (i = 0; i < g_torrentmeta -> num_files; ++i) {
		if ( strlen(file_name) == strlen(g_files_progress[i].file_name) &&
			strncmp(file_name, g_files_progress[i].file_name, strlen(file_name)) == 0)
			return g_files_progress + i;
	}
	return NULL;
}

int is_piece_complete(uint32_t piece_index) {
	extern piece_status_t * g_pieces_status;
	char * piece_data = g_pieces_status[piece_index].data;
	int piece_len = g_torrentmeta -> piece_len;
	char existing_hash[20];
	memcpy(existing_hash, g_torrentmeta -> pieces + piece_index * 20, 20);

	SHA1Context sha;
	SHA1Reset( &sha );
	// TODO: piece_len并不一定是分片长度 - 万一是最后一片呢？
	SHA1Input( &sha, (const unsigned char *)piece_data, piece_len);
	if ( !SHA1Result( &sha )) {
		printf("fail\n");
	}
	char * calculated_hash = (char *)sha.Message_Digest;
	int compare = memcmp(calculated_hash, existing_hash, 20);

	return (compare == 0) ? 1 : -1 ;
}

// msg的空间已经分配好了【我去掉了那个迷之二级指针
void read_file(uint8_t * msg, int offset, int length) {
	// 首先确定offset的位置在哪个文件中
	// 待读取长度 <- length
	// 当前文件 <- 起始文件
	// 文件起读位置 <- offset所指定的起始文件的起点
	// while 待读取长度 > 0
	//		获得当前文件还剩多少空间可以读
	//		若待读取长度较小
	//			直接读入待读取长度那么多的内容
	//			跳出循环
	//		否则
	//			读入上一步得到的长度那么多的内容
	//			待读取长度 -= 刚才读入的长度
	//			文件起读位置 = 0
	//			当前文件 = 下一个文件

	char * start_file_name = NULL;
	int i = 0;
	int total_length = 0;
	int first_offset = 0;
	for (; i < g_torrentmeta -> num_files; ++i) {
		total_length += g_torrentmeta -> file_info[i].length;
		if (offset >= total_length) {
			continue;
		} else {
			start_file_name = g_torrentmeta -> file_info[i].path;
			total_length -= g_torrentmeta -> file_info[i].length;
			first_offset = offset - total_length;
			break;
		}
	}

	if ( !start_file_name )
		return;

	// 去掉了迷之二级指针，所以改了
	char * data = msg;
	int length_to_read = length;
	int read_offset = first_offset;
	char *curr_file_name = start_file_name;

	while (length_to_read > 0) {
		FILE * curr_file = fopen(curr_file_name, "r");
		if (!curr_file) {
			char * temp_name = get_temp_file_name(curr_file_name);
			curr_file = fopen(temp_name, "r");
			free(temp_name);
		}
		if (!curr_file)
			break;
		fseek(curr_file, 0, SEEK_END);
		int residue = ftell(curr_file) - read_offset;
		fseek(curr_file, read_offset, SEEK_SET);
		if (length_to_read <= residue) {
			fread(data, sizeof(char), length_to_read, curr_file);
			fclose(curr_file);
			break;
		} else {
			int nread = fread(data, sizeof(char), residue, curr_file);
			assert(nread == residue);
			length_to_read -= residue;
			data += residue;
			read_offset = 0;
			fclose(curr_file);
			i++;
			if (i == g_torrentmeta -> num_files)
				break;
			curr_file_name = g_torrentmeta -> file_info[i].path;
		}
	}
	return;
}

int save_piece(int piece_index) {
	// 首先确定piece_index的起始位置在哪个文件中
	// 待写入长度 <- 分片长度 done 最后一分片？
	// 当前文件 <- 起始文件
	// 文件起写位置 <- 分片所指定的在起始文件的起点
	// while 当前文件剩余长度 > 0
	//		获得当前文件还剩多少空间可以写
	//		如果待写入长度比较短
	//			直接在当前文件起写位置写入指定内容
	//			跳出循环
	//		写入上一步得到的长度那么多的内容
	//		待写入长度 -= 刚才写入的长度
	//		文件起写位置 = 0
	//		当前文件 = 下一个文件

	// which file is it in?
	file_info_t * files = g_torrentmeta -> file_info;
	int i;
	int total_length = 0, first_offset = 0;
	file_progress_t * start_fp = NULL;
	for (i = 0; i < g_torrentmeta -> num_files; ++i) {
		total_length += g_files_progress[i].length;
		if (piece_index * g_torrentmeta -> piece_len >= total_length) {
			continue;
		} else {
			start_fp = g_files_progress + i;
			total_length -= files[i].length;
			first_offset = piece_index * g_torrentmeta -> piece_len - total_length;
			break;
		}
	}

	if ( !start_fp )
		return -1;

	const char * data = g_pieces_status[piece_index].data;
	// done: 可能分片长度不满，因为是最后一片
	int length_to_write = g_torrentmeta -> piece_len;

	if ( piece_index == g_torrentmeta -> num_pieces - 1 ) {
		//assert(i == g_torrentmeta -> num_files - 1);
		//assert(g_torrentmeta -> length % g_torrentmeta -> piece_len == start_fp -> end_offset);
		// TODO: if last piece is enwrapped within the last piece?
		// length_to_write = start_fp -> end_offset;
		length_to_write = g_torrentmeta -> length % g_torrentmeta -> piece_len;
	}
	int write_offset = first_offset;
	file_progress_t * curr_fp = start_fp;

	while (length_to_write > 0) {
		FILE * curr_file = fopen(curr_fp -> file_name, "r");
		if ( !curr_file ) {
			char * temp_file_name = get_temp_file_name(curr_fp -> file_name);
			curr_file = fopen( temp_file_name, "r" );
			free(temp_file_name);
		}
		if ( !curr_file )
			return -1;
		fseek(curr_file, 0, SEEK_END);
		int residue = ftell(curr_file) - write_offset;
		fclose(curr_file);
		if (length_to_write <= residue) {
			write_file(curr_fp, data, write_offset, length_to_write);
			int t = piece_index - curr_fp -> piece_progress[0].piece_index;
			assert(curr_fp -> piece_progress[t].piece_index == piece_index);
			//printf("file %s at piece %d is complete\n", curr_fp -> file_name,
			//		curr_fp -> piece_progress[ t ].piece_index);
			curr_fp -> piece_progress[ t ].completed = 1;
			break;
		}
		write_file(curr_fp, data, write_offset, residue);
		length_to_write -= residue;
		data += residue;
		if (write_offset != 0) {
			//printf("file %s at piece %d is complete\n", curr_fp -> file_name,
			//		curr_fp -> piece_progress[ curr_fp -> num_pieces - 1].piece_index);
			curr_fp -> piece_progress[ curr_fp -> num_pieces - 1 ].completed = 1;
		}
		else {
			//printf("file %s at piece %d is complete\n", curr_fp -> file_name,
			//		curr_fp -> piece_progress[ 0 ].piece_index);
			curr_fp -> piece_progress[ 0 ].completed = 1;
		}
		write_offset = 0;
		curr_fp ++;
	}

	//没办法了。。。先这么搞吧。。。
	g_pieces_status[piece_index].data = NULL;
	return 1;
}
