#ifndef _IRCD_DOG3_FDLIST
#define _IRCD_DOG3_FDLIST

/* $Id$ */

typedef struct fdstruct {
	int  entry[MAXCONNECTIONS + 2];
	int  last_entry;
} fdlist;

void addto_fdlist(int a, fdlist * b);
void delfrom_fdlist(int a, fdlist * b);
void init_fdlist(fdlist * b);

#ifndef TRUE
#define TRUE 1
#endif

#define LOADCFREQ 5
#define LOADRECV 35
#define FDLISTCHKFREQ  2

#endif /*
        * _IRCD_DOG3_FDLIST 
        */
