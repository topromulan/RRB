
#include <stdlib.h>
#include <errno.h>

void err(char *explanation)
{
	perror(explanation);
	// why this segfault
	//fprintf(stderr, "Uh oh - %s - %s\n", explanation, strerror(errno));
	exit(1);
}

