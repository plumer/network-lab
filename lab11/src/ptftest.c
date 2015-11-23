#include "btdata.h"
#include <stdio.h>


int main(int argc, char ** argv) {
	if (argc != 2)
		return 1;
	torrentmetadata_t * tmd = parsetorrentfile(argv[1]);

	if (tmd) {
		printf("announce: %s\n", tmd -> announce);
		printf("length: %d\n", tmd -> length);
		printf("piece_len: 0x%x\n", tmd -> piece_len);
		printf("num_pieces: %d\n", tmd -> num_pieces);

		if (tmd -> mode == METADATA_SINGLE) {
			printf("filename: %s\n", tmd -> name);
		} else if (tmd -> mode == METADATA_MULTIPLE) {
			printf("files: \n");
			int i;
			for (i = 0; i < tmd -> num_files; ++i) {
				printf("  length: %8lld, name = %s\n",
						tmd -> file_info[i].length, tmd -> file_info[i].path);
			}
		}
	}
	return 0;
}
