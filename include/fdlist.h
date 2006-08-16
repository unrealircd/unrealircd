#ifndef _IRCD_DOG3_FDLIST
#define _IRCD_DOG3_FDLIST

/* $Id$ */
#ifndef NEW_IO
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
#else /* ifndef NEW_IO */

#define FD_DESC_SZ 128  /* hostlen + comment */

/* enums better then defines for debugging issue */
enum {
    COMM_OK,
    COMM_ERR_BIND,
    COMM_ERR_DNS,
    COMM_ERR_TIMEOUT,
    COMM_ERR_CONNECT,
    COMM_ERROR,
    COMM_ERR_MAX
};

/* This is to get around the fact that some implementations have ss_len and
 * others do not
 */
struct irc_ssaddr
{
	struct sockaddr_storage ss;
	unsigned char   ss_len;
	in_port_t       ss_port;
};


/* For Callback functions arguments */
struct _fde;

/* Callback for completed IO events */
typedef void PF(struct _fde *, void *);

/* Callback for completed connections */
/* int fd, int status, void * */
typedef void CNCB(struct _fde *, int, void *);

typedef struct _fde {
  /* New-school stuff, again pretty much ripped from squid */
  /*
   * Yes, this gives us only one pending read and one pending write per
   * filedescriptor. Think though: when do you think we'll need more?
   */
  int fd;   /* So we can use the fde_t as a callback ptr */
  int comm_index; /* where in the poll list we live */
  int evcache;          /* current fd events as set up by the underlying I/O */
  char desc[FD_DESC_SZ];
  PF *read_handler;
  void *read_data;
  PF *write_handler;
  void *write_data;
  PF *timeout_handler;
  void *timeout_data;
  time_t timeout;
  PF *flush_handler;
  void *flush_data;
  time_t flush_timeout;
/*  struct DNSQuery *dns_query; at hybrid 7.2.2 */
	struct DNSReq *dns_query;
  struct {
    unsigned int open:1;
    unsigned int is_socket:1;
#ifdef USE_SSL
    unsigned int pending_read:1;
#endif
  } flags;

  struct {
    /* We don't need the host here ? */
    struct irc_ssaddr S;
    struct irc_ssaddr hostaddr;
    CNCB *callback;
    void *data;
    /* We'd also add the retry count here when we get to that -- adrian */
  } connect;
#ifdef USE_SSL
  SSL *ssl;
#endif
  struct _fde *hnext;
} fde_t;


#endif /* ifndef NEW_IO */

#ifndef TRUE
#define TRUE 1
#endif

#define LOADCFREQ 5
#define LOADRECV 35
#define FDLISTCHKFREQ  2

#endif /*
        * _IRCD_DOG3_FDLIST 
        */
