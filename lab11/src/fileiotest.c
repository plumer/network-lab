#include "fileio.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
	file_progress_t * fp = create_progress("Wildlife.wmv", 12746, 254451, 16 * 1024);
	if (fp)
		write_progress(fp);
	free(fp);

	fp = read_progress("Wildlife.wmv");
	free(fp);
	return 0;

}
