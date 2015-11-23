#ifndef __FILEIO_H__
#define __FILEIO_H__

/**
该源代码声明了一系列函数，
用于BitTorrent客户端对文件进行操作。
其中包括创建文件、随机内容填充，以及对断点续传的支持。
是否在本代码中实现对文件访问的互斥性保证 - 有待后续补足。
*/

#include <stdio.h>
#include <stdint.h>
#include <pthread.h>

// 描述文件下载进度的数据结构。
// 每一个文件都应该有一个这样的数据结构。

// 由于支持多文件传输，有必要举例说明：
// 设某一种子有3个文件，大小分别为15000KB，17000KB，19000KB。
// 分片长度为512KB，则总共有(15000+17000+19000)/512(上取整)=100个分片
//		第一个文件：15000KB = 512KB * 29 + 152KB，所以有30个分片。
//		                      [0,1,...,28]  [29]
//		piece_progress 数组有30个元素，piece_index为[0,1,2,...29]，
//		并且end_offset = 152。
//
//		第二个文件：17000KB = (512-152:360)KB + 512KB * 32 + 256KB，所以有34个分片。
//		                      [29]    [30,31,...61] [62]
//		piece_index为[29,30,31,...61,62]。
//		start_offset为152，end_offset为256。
//
//		第三个文件：19000 = (512-256:256) + 512KB * 36 + 312KB
//		                    [62]            [63,64,...98] [99]
//		piece_index为[62,63,....98,99]。
//		start_offset为256，end_offset为312。
//	验证：512KB * 99 + 312 = 51000

typedef struct _piece_progress {
	uint32_t piece_index;		// 分片号
	uint8_t completed;			// 是否下载完毕
} piece_progress_t;

typedef struct _file_progress {
	char file_name[256];		// 文件名
	FILE * file_stream;			// 文件流指针
	uint32_t length;			// 文件长度
	uint32_t num_pieces;		// 分片数量
	uint32_t piece_length;		// 分片长度，一般是2的整次幂
	piece_progress_t * piece_progress;			// 上述两个信息的数组，长度为num_pieces
	uint32_t start_offset;		// 头一分片从哪里开始，[0, piece_length - 1]
	uint32_t end_offset;		// 尾一分片在哪里结束，[1, piece_length]
	pthread_mutex_t mutex;		// 读和写的互斥变量
} file_progress_t;


// 创建一个文件，并且指明该文件有多大。
// 文件大小不得超过4GB。
// 如果成功，文件指针被返回，文件被创建，但不处于打开状态，并且被加上".bt"以标识未完成。
// 一般，调用者将成功返回的指针存入file_progress_t数据结构当中。
FILE * 
create_file(const char * file_name, uint32_t size);

// 向fp所指向的文件中，从start位置开始length个字节，
// 写入content所指定的内容。
// (!这个功能好难啊)如果指定的位置覆盖了一整个分片，则设置该分片的completed域为1。
// 将分片的completed置位的功能 - 这个函数有点难做
int
write_file(file_progress_t *fp, const char * content, uint32_t start, uint32_t length);

// 检查fp所描述的文件是否全部下载完成（即bitfield是否全1）。
// 如果下载完成，则将文件重命名，并将".bt.cfg"文件删去，返回1。
int
complete_file(file_progress_t * fp);


// 根据文件名、文件起始偏移量(字节)、文件大小(字节)、分片长度(字节)
// 生成进度数据结构并返回。数据结构中的文件指针不设置。
file_progress_t *
create_progress(const char * file_name, uint32_t startfrom, 
		uint32_t length, uint32_t piece_length);

// 从数据结构中得到文件名，在文件最后加上".bt.cfg"作为进度信息文件的文件名。
// 将进度信息写入.bt.cfg文件。
// 一般在退出程序时使用。
int
write_progress(file_progress_t *dp);

// 根据file_name，检查是否存在断点续传文件，
// 如果是，读取这个文件，返回这个文件的进度描述数据结构；
// 如果不存在，返回NULL
file_progress_t *
read_progress(const char * file_name);


// 把g_file从头开始偏移offset个字节的长度为length的数据复制到到msg里面去，msg已经malloc过了
// 注意：假设长度最多跨越两个文件，跨越更多个文件的场合再说。
void read_file(uint8_t *msg, int offset, int length);


//这个函数校验当前分片的hash是否跟种子的一致，如果一致返回1， 否则返回-1
// 需要校验的piece的数据为 g_pieces_status[piece_index].data
// 需要校验的piece的hash等信息在 g_torrentmeta 里
int is_piece_complete(uint32_t piece_index);

// 保存一个分片
// 参数是分片的序号
// 分片长度在g_torrentmeta -> piece_len
// 分片数据在g_pieces_status[piece_index].data
int save_piece(int piece_index);


#endif // __FILEIO_H__
