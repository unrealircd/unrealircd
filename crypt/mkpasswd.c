/* simple password generator by Nelson Minar (minar@reed.edu)
 * copyright 1991, all rights reserved.
 * You can use this code as long as my name stays with it.
 *
 * $Id$
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char *getpass();

int main(argc, argv)
int argc;
char *argv[];
{
  static char saltChars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";
  char salt[3];
  char * plaintext;
  int i;

  if (argc < 2) {
    srandom(time(0));		/* may not be the BEST salt, but its close */
    salt[0] = saltChars[random() % 64];
    salt[1] = saltChars[random() % 64];
    salt[2] = 0;
  }
  else {
    salt[0] = argv[1][0];
    salt[1] = argv[1][1];
    salt[2] = '\0';
    if ((strchr(saltChars, salt[0]) == NULL) || (strchr(saltChars, salt[1]) == NULL))
      fprintf(stderr, "illegal salt %s\n", salt), exit(1);
  }

  plaintext = getpass("Enter Password: ");

  printf("Encryption: %s\n", crypt(plaintext, salt));
  return 0;
}

