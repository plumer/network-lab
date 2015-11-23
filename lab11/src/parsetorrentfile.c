
#include "bencode.h"
#include "util.h"
#include "sha1.h"
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

// 注意: 这个函数只能处理单文件模式torrent
torrentmetadata_t* parsetorrentfile(char* filename)
{
	int i;
	be_node* ben_res;
	FILE* f;
	int flen;
	char* data;
	torrentmetadata_t* ret;

	// 打开文件, 获取数据并解码字符串
	f = fopen(filename, "r");
	flen = file_len(f);
	data = (char *)malloc( flen*sizeof(char) );
	fread( (void*)data, sizeof(char), flen, f);
	fclose(f);
	ben_res = be_decoden(data,flen);

	// 遍历节点, 检查文件结构并填充相应的字段.
	if (ben_res->type != BE_DICT)
	{
		perror("Torrent file isn't a dictionary");
		exit(-13);
	}

	ret = (torrentmetadata_t*)malloc(sizeof(torrentmetadata_t));
	if (ret == NULL)
	{
		perror("Could not allocate torrent meta data");
		exit(-13);
	}

	// 计算这个torrent的info_hash值
	// 注意: SHA1函数返回的哈希值存储在一个整数数组中, 对于小端字节序主机来说,
	// 在与tracker或其他peer返回的哈希值进行比较时, 需要对本地存储的哈希值
	// 进行字节序转换. 当你生成发送给tracker的请求时, 也需要对字节序进行转换.
	// 然而大湿胸改了数据结构，info_hash现在是char[20]类型的数组，无需考虑字节序转换
	// 当然上面那句话可能是个FLAG，说不定这样做反而会出问题呢23333
	char* info_loc, *info_end;
	info_loc = strstr(data,"infod");	// 查找info键, 它的值是一个字典
	info_loc += 4; // 将指针指向值开始的地方
	info_end = data+flen-1;
	// 去掉结尾的e
	if(*info_end == 'e')
	{
		--info_end;
	}

	char* p;
	int len = 0;
	for(p=info_loc; p<=info_end; p++) len++;

	// 计算上面字符串的SHA1哈希值以获取info_hash
	SHA1Context sha;
	SHA1Reset(&sha);
	SHA1Input(&sha,(const unsigned char*)info_loc,len);
	if(!SHA1Result(&sha))
	{
		printf("FAILURE\n");
	}

	// 这里看起来是把Message_Digest按照字节序拷贝进了info_hash，
	// 所以info_hash用字符数组应该没有问题
	memcpy(ret->info_hash,sha.Message_Digest,20);
	/*
	printf("SHA1:\n");
	for(i=0; i<20; i++)
	{
		printf("%02X",ret->info_hash[i]);
	}
	printf("\n");
	*/
	// 检查键并提取对应的信息
	int filled=0;
	for (i = 0; ben_res->val.d[i].val != NULL; i++) {
		int j;
		if(!strncmp(ben_res->val.d[i].key,"announce",strlen("announce"))) {
			if ( strlen(ben_res -> val.d[i].key) != strlen("announce") )
				continue;
			char * announce_val = ben_res -> val.d[i].val -> val.s;
			// printf("debug: announce_val = %s\n", announce_val);
			ret->announce = (char*)malloc( (1+strlen(announce_val))*sizeof(char));
			memset(ret -> announce, 0, (1+strlen(announce_val))* sizeof(char));
			strncpy(ret->announce, announce_val ,strlen(announce_val));
			filled++;
		}
		// info是一个字典, 它还有一些其他我们关心的键
		if (!strncmp(ben_res->val.d[i].key,"info",strlen("info"))) {
			be_dict* idict;
			if(ben_res->val.d[i].val->type != BE_DICT)
			{
				perror("Expected dict, got something else");
				exit(-3);
			}
			idict = ben_res->val.d[i].val->val.d;
			// 检查这个字典的键
			for(j=0; idict[j].val != NULL; j++)
			{ 
				if(!strncmp(idict[j].key,"length",strlen("length")))
				{
					ret->length = idict[j].val->val.i;
					ret -> mode = METADATA_SINGLE;
					filled++;
				}
				if(!strncmp(idict[j].key,"name",strlen("name")))
				{
					char * tname = idict[j].val -> val.s;
					ret->name = (char*)malloc( (strlen(tname)+1) * sizeof(char) );
					memcpy(ret->name,tname,strlen(tname));
					ret->name[ strlen(tname) ] = 0;
					filled++;
				}
				if(!strncmp(idict[j].key,"piece length",strlen("piece length")))
				{
					ret->piece_len = idict[j].val->val.i;
					int p = ret -> piece_len;
					if ( (p & (p-1)) != 0)
						printf("warning: piece length is not a power of 2 to an integer\n");
					filled++;
				}
				if(!strncmp(idict[j].key,"pieces",strlen("pieces")))
				{
					int num_pieces = ret->length/ret->piece_len;
					if(ret->length % ret->piece_len != 0)
						num_pieces++;
					ret->pieces = (char*)malloc(num_pieces*20);
					memcpy(ret->pieces,idict[j].val->val.s,num_pieces*20);
					ret->num_pieces = num_pieces;
					filled++;
				}
				if (!strncmp(idict[j].key, "files", strlen("files"))) {
					ret -> mode = METADATA_MULTIPLE;
					ret -> length = 0;
					int file_count = 0;
					be_node ** file_list = idict[j].val -> val.l;
					while (file_list[file_count])
						file_count++;
					ret -> num_files = file_count;
					ret -> file_info = (file_info_t *)malloc( sizeof(file_info_t) * file_count);
					int file_index = 0;
					for(; file_index < file_count; ++file_index) {
						be_dict * dict = file_list[file_index] -> val.d;
						int t = 0;
						for (; ; t++) {
							if (dict[t].val == NULL) break;
							else if ( !strncmp(dict[t].key, "length", strlen("length")) ) {
								ret -> file_info[file_index].length = dict[t].val -> val.i;
								ret -> length += ret -> file_info[file_index].length;
							} else if ( !strncmp(dict[t].key, "path", strlen("path")) ) {
								const char * path = dict[t].val -> val.l[0] -> val.s;
								ret -> file_info[file_index].path =
									(char *)malloc( sizeof(char) * (strlen(path)+1) );
								strncpy(ret -> file_info[file_index].path, path, strlen(path));
								ret->file_info[file_index].path[strlen(path)] = 0;
							} else {
								assert(0);
							}
						}
					}
					filled ++;
				}


			} // for循环结束
		} // info键处理结束
	}

	// 确认已填充了必要的字段

	be_free(ben_res);	

	if(filled < 5)
	{
		printf("filled = %d\n", filled);
		printf("Did not fill necessary field\n");
		return NULL;
	}
	else {
		ret -> num_pieces = (ret -> length) / (ret -> piece_len) 
				+ !! ((ret -> length) % (ret -> piece_len));

		// 修改file_info，使用g_filelocation和文件名最后一个'/'字符之后的串组成文件名。
		// TODO : 如果g_filelocation最后有'/'?
		if ( ret -> mode == METADATA_SINGLE) {
			const char * last_part;
			if (strstr(ret -> name, "/") == NULL) last_part = ret -> name;
			else {
				last_part = ret -> name + strlen(ret -> name) - 1;
				while ( *last_part != '/') last_part --;
				last_part ++;
			}
			char * dest_name = (char *)malloc( sizeof(char) * 
					(strlen(last_part) + 1 + strlen(g_filelocation) + 1));
			memset( dest_name, 0, sizeof(char) * 
					(strlen(last_part) + 1 + strlen(g_filelocation) + 1));
			strncpy(dest_name, g_filelocation, strlen(g_filelocation));
			strncat(dest_name, "/", strlen("/"));
			strncat(dest_name, last_part, strlen(last_part));
			assert(dest_name[strlen(last_part) + 1 + strlen(g_filelocation)] == '\0');
			free(ret -> name);
			ret -> name = dest_name;

			ret -> file_info = (file_info_t *)malloc( sizeof(file_info_t) );
			ret -> file_info -> length = ret -> length;
			ret -> file_info -> path = 
				(char*)malloc( sizeof(char) * (1+strlen(ret -> name)));
			memset(ret -> file_info -> path, 0, sizeof(char) * (1+strlen(ret -> name)));
			strncpy(ret -> file_info -> path, ret -> name, strlen(ret -> name));
			ret -> num_files = 1;

		} else {
			const char * dir_last_part = NULL;
			if ( strstr(ret -> name, "/") == NULL )
				dir_last_part = NULL;
			else {
				dir_last_part = ret -> name + strlen(ret -> name) - 1;
				while ( *dir_last_part != '/' )
					dir_last_part --;
				dir_last_part ++;
			}

			char * dest_dir = (char *)malloc( sizeof(char) * 
					( strlen(dir_last_part) + 1 + strlen(g_filelocation) +1) );
			memset( dest_dir, 0, sizeof(char) * 
					( strlen(dir_last_part) + 1 + strlen(g_filelocation) + 1) );
			strncpy(dest_dir, g_filelocation, strlen(g_filelocation));
			strncat(dest_dir, "/", strlen("/"));
			strncat(dest_dir, dir_last_part, strlen(dir_last_part) );
	
			
			mkdir(dest_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

			int fi = 0;
			for (; fi < ret -> num_files; ++fi) {
				const char * last_part = NULL;
				if ( strstr(ret -> file_info[fi].path, "/") == NULL ) {
					last_part = ret -> file_info[fi].path;
				} else {
					last_part = ret -> file_info[fi].path + strlen(ret -> file_info[fi].path) - 1;
					while ( *last_part != '/' )
						last_part --;
					last_part ++;
				}
				char * dest_file_name = (char *)malloc ( sizeof(char) *
						( strlen(dest_dir) + 1 + strlen(last_part) + 1 ) );
				memset( dest_file_name, 0, sizeof(char) *
						( strlen(dest_dir) + 1 + strlen(last_part) + 1 ) );
				strncpy( dest_file_name, dest_dir, strlen(dest_dir) );
				strncat( dest_file_name, "/", strlen("/") );
				strncat( dest_file_name, last_part, strlen(last_part) );
				free(ret -> file_info[fi].path);
				ret -> file_info[fi].path = dest_file_name;
			}
		}
		return ret;
	}
}
