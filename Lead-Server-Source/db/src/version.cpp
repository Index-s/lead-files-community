#include <stdio.h>
#include <stdlib.h>

void WriteVersion()
{
#ifndef __WIN32__
	FILE* fp(fopen("VERSION.txt", "w"));

	if (NULL != fp)
	{
		fprintf(fp, "game perforce revision: %s\n", __P4_VERSION__);
		fprintf(fp, "%s@%s:%s\n", __USER__, __HOSTNAME__, __PWD__);
		fclose(fp);
	}
	else
	{
		fprintf(stderr, "cannot open VERSION.txt\n");
		exit(0);
	}
#endif
}

// TODO: Remove workaround by using mysqlclient build from vcpkg
//       This is just needed to make the compiler shut up for now.
extern "C" {
	FILE* __cdecl __iob_func(void) {
		static FILE* iob[3] = { stdin, stdout, stderr };
		return iob[0];
	}
}

