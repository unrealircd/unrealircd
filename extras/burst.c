/*
** burst.c
**
** Nicolas Pioch, Dec 93
** <Nicolas.Pioch@enst.fr>
** Carsten V. Munk Nov 2000
*/

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/signal.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>

#ifdef POSIX
#include <sys/select.h>
#endif

#define MYBUFSIZE 2048

char *jupedservername, *uplinkservername, *jupereason;
int	options;
char *password;
typedef struct aClient {
  int socket;
  char buffer[MYBUFSIZE+1];
  int read;
  int write;
} aClient;

void fatalrestart(aClient *robotptr, const char *s);

void fatal(const char *s) {
  perror(s);
  exit(1);
}

typedef void (*signalhandler)(int);
static void signal_catcher(int sig, int code, struct sigcontext *scp) {
  fprintf(stderr, "Caught signal %d with code %d\n", sig, code);

  switch(sig) {
  case SIGFPE:  /* Floating point exception */
  case SIGILL:  /* Illegal instruction */
  case SIGSEGV: /* Segmentation violation */
  case SIGBUS:  /* Bus error */
  case SIGSYS:  /* Bad argument to system call */
    fatal("Fatal signal received, aborting...");
    break;

  case SIGQUIT:
  case SIGINT:
  case SIGTERM: /* On se barre */
    fatal("AAAAAAAAArrrrrrrrgh!");
    break;
  }

  signal(sig, (signalhandler)signal_catcher);
}


typedef union socket_address {
  struct sockaddr_un unixx;
  struct sockaddr_in inet;
} socket_address;

void name_to_number(int address_family, const char *hostname, int port,
		    socket_address *addr, int *len2) {
  u_long iaddr;
  int len;
  struct hostent *host;
  char *inaddr;
  bzero ((char *) addr, sizeof (socket_address) );

  if ( address_family == AF_UNIX ) {
    strcpy(addr->unixx.sun_path,hostname);
    *len2=sizeof( struct sockaddr_un );
  } else {
    if ((hostname) && (hostname[0])) {
      if (hostname[0] >= '0' && hostname[0] <= '9') {
        iaddr = inet_addr(hostname);
        inaddr = (char *) &iaddr;
        len=sizeof(iaddr);
      } else {
        host=gethostbyname (hostname);
        if (!host)
          fatal("Unknown host");
        inaddr=host->h_addr_list[0];
        len=host->h_length;
      }
    bcopy(inaddr, (char *)&addr->inet.sin_addr, len);
    }
  addr->inet.sin_port=htons((u_short) port);
  *len2=sizeof(struct sockaddr_in);
  }
  addr->inet.sin_family = address_family;
}

int read_data(aClient *robotptr) {
  int nbytes;

  if ((nbytes=read(robotptr->socket, &(robotptr->buffer[robotptr->write]),
		  (sizeof(robotptr->buffer) - robotptr->write - 1))) < 0)
    fatalrestart(robotptr, "read on socket");

  if (nbytes == 0) {
    close(robotptr->socket);
    fatalrestart(robotptr, "Connection Closed");
  }

  robotptr->write+=nbytes;
  robotptr->buffer[robotptr->write]=0;

  return(nbytes);
}

void send_data(int socketfd, const char *data, int length) {
  int length2;
  length2=write(socketfd, data, length);
  if (length2 != length)
    fprintf(stderr, "*** write truncated to %d chars...\n", length2);
}

int create_client(const char *hostname, const int hostport) {
  int descript;
  socket_address hostaddr;
  int adlen;

  /* winlocal
  if ((descript=socket(PF_INET, SOCK_STREAM, 0)) < 0)
  */
  if ((descript=socket(PF_INET, SOCK_STREAM, 0)) == -1)		// winlocal
    fatal("socket");

  name_to_number(AF_INET, hostname, hostport, &hostaddr, &adlen);

  if (connect(descript, &hostaddr.inet, adlen) < 0)
    fatal("connect");

  return(descript);
}

void parse_line(aClient *robotptr) {
  char *toparse;

  toparse=(robotptr->buffer + robotptr->read);

  printf("%s\n",toparse);
  /* Special handling for PING messages */
  if (!strncasecmp(toparse, "PING ", 5) || !strncasecmp(toparse, "8 ", 2)){
    toparse=strtok(toparse, "\r\n");
    toparse[1]='\0';
    send_data(robotptr->socket, toparse, strlen(toparse));
    return;
  }
  if (!strncasecmp(toparse, "AO", 2) || !strncasecmp(toparse, "NETINFO", 7))
  {
	if (!(options & 0x80))
	  	exit(1);
  }
}

void parse_data(aClient *robotptr) {
  int next;

  while((robotptr->read < robotptr->write)
	&& ((robotptr->buffer[robotptr->read] == '\r')
	    || (robotptr->buffer[robotptr->read] == '\n')))
    robotptr->read++;

  for(next=robotptr->read; next<=robotptr->write; next++) {
    if ((robotptr->buffer[next] == '\r')
	|| (robotptr->buffer[next] == '\n'))
      break;
  }

  if (next<robotptr->write) {
    robotptr->buffer[next]=0;
    next++;
    parse_line(robotptr);
    robotptr->read=next;
    parse_data(robotptr);
  }

  if (robotptr->read == robotptr->write) {
    robotptr->read=robotptr->write=0;
  } else {
    bcopy((robotptr->buffer + robotptr->read),
	  (robotptr->buffer), (robotptr->write-robotptr->read));
    robotptr->write-=robotptr->read;
    robotptr->read=0;
  }
}

void run_client(aClient *robotptr) {
  fd_set mask, returnmask[3];
  int selectval;

  FD_ZERO(&mask);
  FD_SET(robotptr->socket, &mask);

  while (1) {
    bcopy((char *)&mask,(char *)returnmask,sizeof(fd_set));
    FD_ZERO(returnmask+1);
    bcopy((char *)&mask,(char *)(returnmask+2),sizeof(fd_set));

    selectval=select(FD_SETSIZE,
		     returnmask, returnmask+1, returnmask+2, NULL);

    if (selectval) {
      read_data(robotptr);
      parse_data(robotptr);
    }
  }
}


void initialize(aClient *robotptr) {
  char passphrase[MYBUFSIZE];
  robotptr->socket=create_client(uplinkservername, atol(jupereason));
  sprintf(passphrase,
	  "PROTOCTL %s %s %s %s %s %s %s %s %s\r\n", 
	  	(options & 0x1) ? "NOQUIT" : "",
	  	(options & 0x2) ? "TOKEN" : "",
	  	(options & 0x4) ? "NICKv2" : "",
	  	(options & 0x8) ? "SJOIN" : "",
	  	(options & 0x10) ? "SJOIN2" : "",
	  	(options & 0x20) ? "UMODE2" : "",
	  	(options & 0x40) ? "NS" : "",
	  	(options & 0x100) ? "SJ3" : "",
	  	(options & 0x200) ? "SJB64" : "");
  write(robotptr->socket, passphrase, strlen(passphrase));
  sprintf(passphrase, "PASS %s\r\nSERVER %s 1 :[Burst analysis].\r\n",
	  password,jupedservername);
  write(robotptr->socket, passphrase, strlen(passphrase));
}

void fatalrestart(aClient *robotptr, const char *s) {
  perror(s);
  close(robotptr->socket);
  sleep(10);
  initialize(robotptr);
  run_client(robotptr);
}

int main(int argc, char* argv[]) {
  aClient robot;
  char *p;
  
  if (argc != 6) {
    fprintf(stderr, "IRCd burst generator, by Carsten V. Munk 2000, based off jupe.c by Nicolas Pioch\n");
    fprintf(stderr,
	    "Usage: %s <burstservername> <uplink> <port> <options> <password>\n", argv[0]);
    fprintf(stderr, "Options can be a combination of these:\n");
    fprintf(stderr, "1 = Enable NOQUIT  2 = Enable TOKEN\n");
    fprintf(stderr, "3 = Enable NICKv2  4 = Enable SJOIN\n");
    fprintf(stderr, "5 = Enable SJOIN2  6 = Enable UMODE2\n");
    fprintf(stderr, "7 = Enable NS      8 = Ignore end of burst\n");
    fprintf(stderr, "9 = Enable SJ3     a = Enable SJB64\n");
    fatal("incorrect argument count");
    exit(1);
  }
  
  jupedservername=argv[1];
  uplinkservername=argv[2];
  jupereason=argv[3];
  password = argv[5];
  p = argv[4];
  for (; *p; p++)
  {
  	if (*p == '1')
  		options |= 0x1;
  	if (*p == '2')
  		options |= 0x2;
  	if (*p == '3')
  		options |= 0x4;
  	if (*p == '4')
  		options |= 0x8;
  	if (*p == '5')
  		options |= 0x10;
  	if (*p == '6')
  		options |= 0x20;
	if (*p == '7') 
		options |= 0x40;
	if (*p == '8') 
		options |= 0x80;
	if (*p == '9')
		options |= 0x100;
	if (*p == 'a')
		options |= 0x200;
  }
  /* installe le signal_catcher */
  signal(SIGTERM,(signalhandler)signal_catcher);

  robot.read=robot.write=0;

  initialize(&robot);
  run_client(&robot);

  exit(0);
}
