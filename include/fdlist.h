#ifndef FDLIST_H
#define FDLIST_H

/* $Id$ */

#define FD_DESC_SZ	(100)

typedef void (*IOCallbackFunc)(int fd, int revents, void *data);

typedef enum FDCloseMethod { FDCLOSE_SOCKET=0, FDCLOSE_FILE=1, FDCLOSE_NONE=3 } FDCloseMethod;

typedef struct fd_entry {
	int fd;
	char desc[FD_DESC_SZ];
	IOCallbackFunc read_callback;
	IOCallbackFunc write_callback;
	void *data;
	time_t deadline;
	unsigned char is_open;
	FDCloseMethod close_method;
	unsigned int backend_flags;
} FDEntry;

extern MODVAR FDEntry fd_table[MAXCONNECTIONS + 1];

extern int fd_open(int fd, const char *desc, FDCloseMethod close_method);
extern int fd_close(int fd);
extern void fd_unnotify(int fd);
extern int fd_socket(int family, int type, int protocol, const char *desc);
extern int fd_accept(int sockfd);
extern void fd_desc(int fd, const char *desc);
extern int fd_fileopen(const char *path, unsigned int flags);

#define FD_SELECT_READ		0x1
#define FD_SELECT_WRITE		0x2

extern void fd_setselect(int fd, int flags, IOCallbackFunc iocb, void *data);
extern void fd_select(int delay);		/* backend-specific */
extern void fd_refresh(int fd);			/* backend-specific */
extern void fd_fork(); /* backend-specific */

#endif
