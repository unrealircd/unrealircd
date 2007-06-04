#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

void parse(char *path, char *from, char *to)
{
  FILE *f = fopen(path, "r");
  if (f == NULL)
  {
    fprintf(stderr, "Error opening file %s: %s\n", path, strerror(errno));
    exit(-5);
  } 
  char buf[2048], *s;
  int line = 0;
  while ((s = fgets(buf, 2047, f)) != NULL)
  {
     line++;
     if (strncmp(from, buf, strlen(from)) == 0)
     {
         // start pumping
        while ((s = fgets(buf, 2047, f)) != NULL)
        {
            line++;
            if (to != NULL && (strncmp(to, buf, strlen(to)) == 0))
            {
              exit(0);
            }
            printf("%s", s);
        }
        if (to != NULL)
        {
          fprintf(stderr, "EOF reached trying to find %s in %s\n",
              to, path); 
          exit(-2);
        }
        exit(0);
     }
  }
}

int main(int argc, char **argv)
{
  parse(argv[1], argv[2], argv[3]);
  return 0;
}
