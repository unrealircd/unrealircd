/* A small utility to clean a def file */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
	FILE *fd, *fdout;
	char buf[1024];

	if (argc < 3)
		exit(1);

	if (!(fd = fopen(argv[1], "r")))
		exit(2);
	
	if (!(fdout = fopen(argv[2], "w")))
		exit(3);

	while (fgets(buf, 1023, fd))
	{
		if (*buf == '\t') 
		{
			char *symbol = strtok(buf, " ");

			if (!strncmp(symbol, "\t_real@", 7))
				continue;

			fprintf(fdout, "%s\r\n", symbol);	
		
		}
		else
			fprintf(fdout, "%s", buf);

	}
	return 0;
}
