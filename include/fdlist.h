#ifndef FDLIST_H
#define FDLIST_H

/* $Id$ */

#define FD_DESC_SZ	(100)

typedef void (*IOCallbackFunc)(int fd, int revents, void *data);

typedef struct fd_entry {
	int fd;
	char desc[FD_DESC_SZ];
	IOCallbackFunc read_callback;
	IOCallbackFunc write_callback;
	void *data;
	time_t deadline;
	unsigned char is_open;
	unsigned int backend_flags;
} FDEntry;

extern MODVAR FDEntry fd_table[MAXCONNECTIONS + 1];

extern int fd_open(int fd, const char *desc);
extern void fd_close(int fd);
extern int fd_unmap(int fd);
extern void fd_unnotify(int fd);
extern int fd_socket(int family, int type, int protocol, const char *desc);
extern int fd_accept(int sockfd);
extern void fd_desc(int fd, const char *desc);
extern int fd_fileopen(const char *path, unsigned int flags);

#define FD_SELECT_READ		0x1
#define FD_SELECT_WRITE		0x2

/* Socket buffer sizes for clients and servers
 * Note that these have been carefully chosen so you shouldn't touch these.
 * If you make these much higher then be aware that this means you loose
 * send queue / receive queue protection, otherwise known as "Excess flood"
 * and "SendQ Exceeded", since these queues are ON TOP of UnrealIRCd's
 * own queueing mechanism, it uses the OS TCP Socket queue.
 */
#define USER_SOCKET_RECEIVE_BUFFER	8192
#define USER_SOCKET_SEND_BUFFER		32768
#define SERVER_SOCKET_RECEIVE_BUFFER	131072
#define SERVER_SOCKET_SEND_BUFFER	131072

extern void fd_setselect(int fd, int flags, IOCallbackFunc iocb, void *data);
extern void fd_select(time_t delay);		/* backend-specific */
extern void fd_refresh(int fd);			/* backend-specific */
extern void fd_fork(); /* backend-specific */

#endif
