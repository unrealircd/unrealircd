 /* compile with: gcc -lsscript -o example example.c */

#include <stdio.h>
#include <errno.h>
#include <time.h>
main(int argc, char *argv[])
{
/* initializing variables */
 char result[255]=".";
 char *p;
 int port = atoi(argv[2]);
 int sockfd=0,i;
time_t t,d;
/* connect somewhere */
 printf("Connecting to %s:%i .. \n", argv[1], port);
/* call to sscript_connect to connect to the server */
 sockfd=sscript_connect(sscript_resolve_host(argv[1]),port,NULL);

/* if it returns -1, then print the error code */
 if(sockfd<1)
 {
  printf("An error occured: %d\n",errno);
  exit(1);
 }

/* call to sscript_read and copy the result in 'result' */
 p = (char *)sscript_time_read(sockfd,5);
  if (p)
  {
  	strcpy(result,p);
  	t = atol(result);
  	d = t - time(NULL);
  	printf("TS difference from timeserver is %li (%li)\n", d, t);
  }
/* print the result */
/* disconnects */
 sscript_disconnect(sockfd);
}

