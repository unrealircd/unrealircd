#ifndef _IRCD_DOG3_FDLIST
#define _IRCD_DOG3_FDLIST

/* $Id$ */

#define FD_DESC_SZ	(100)

typedef void (*IOCallbackFunc)(int fd, int revents, void *data);

typedef struct fd_entry {
	int fd;
	char desc[FD_DESC_SZ];
	IOCallbackFunc read_callback;
	unsigned char read_oneshot;
	IOCallbackFunc write_callback;
	unsigned char write_oneshot;
	void *data;
	time_t deadline;
	unsigned char is_open;
	unsigned int backend_flags;
} FDEntry;

extern MODVAR FDEntry fd_table[MAXCONNECTIONS + 1];

extern int fd_open(int fd, const char *desc);
extern void fd_close(int fd);
extern int fd_socket(int family, int type, int protocol, const char *desc);
extern int fd_accept(int sockfd);
extern void fd_desc(int fd, const char *desc);

#define FD_SELECT_READ		0x1
#define FD_SELECT_WRITE		0x2
#define FD_SELECT_ONESHOT	0x4

extern void fd_setselect(int fd, int flags, IOCallbackFunc iocb, void *data);
extern void fd_select(time_t delay);		/* backend-specific */
extern void fd_refresh(int fd);			/* backend-specific */

typedef struct fdstruct {
	int  entry[MAXCONNECTIONS + 2];
	int  last_entry;
} fdlist;

void addto_fdlist(int a, fdlist * b);
void delfrom_fdlist(int a, fdlist * b);
void init_fdlist(fdlist * b);

#ifndef NO_FDLIST
extern MODVAR fdlist oper_fdlist;
#endif


#ifndef TRUE
#define TRUE 1
#endif

#define LOADCFREQ 5
#define LOADRECV 35
#define FDLISTCHKFREQ  2

#endif /*
        * _IRCD_DOG3_FDLIST 
        */
