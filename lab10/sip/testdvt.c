#include "dvtable.h"

int main() {
	dv_t * dvt = dvtable_create();
	dvtable_print(dvt);
	dvtable_destroy(dvt);
	return 0;
}