#include "bencode.h"
#include "util.h"
#include <assert.h>

void printindent(int);
void printfile(be_node *, int);

int main(int argc, char ** argv) {
	int i;
	be_node * ben_res;
	FILE * f;
	int flen;
	char * data;

	f = fopen(argv[1], "r");
	if (!f) {
		printf("file open fail\n");
		exit(1);
	}

	flen = file_len(f);
	data = (char *) malloc ( sizeof(char) * flen );
	fread(data, sizeof(char), flen, f);
	fclose(f);
	ben_res = be_decoden(data, flen);

	printfile(ben_res, 0);
	return 0;
}


void printfile(be_node *n, int indent) {
	int i = 0;
	if (n -> type == BE_INT) {
		printindent(indent);
		printf("[Integer: %lld]\n", n -> val.i);
	} else if (n -> type == BE_STR) {
		printindent(indent);
		printf("[String: \'%s\']\n", n -> val.s);
	} else if (n -> type == BE_DICT) {
		be_dict * d = n -> val.d;
		putchar('\n');
		printindent(indent);
		printf("[Dictionary:]\n");
		for (i = 0; ; ++i) {
			if (d[i].val) {
				printindent(indent+2);
				printf("[Key: \'%s\'], ", d[i].key);
				printfile(d[i].val, indent+2);
			} else {
				break;
			}
		}
		printindent(indent);
		printf("[Dictionary End]\n");

	} else if (n -> type == BE_LIST) {
		be_node ** l = n -> val.l;
		putchar('\n');
		printindent(indent);
		printf("[List:]\n");
		for (i = 0; ; ++i) {
			if (l[i]) {
				printfile(l[i], indent+2);
			} else {
				break;
			}
		}
		printindent(indent);
		printf("[List End]\n");
	} else {
		assert(0);
	}
}

void printindent(int i) {
	while (i--)
		putchar(' ');
}
