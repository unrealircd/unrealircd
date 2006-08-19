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

/* Hybrid Support Headers Begin */
#include <sys/resource.h>

#ifdef RLIMIT_FDMAX
# define RLIMIT_FD_MAX RLIMIT_FDMAX
#else
# ifdef RLIMIT_NOFILE
#  define RLIMIT_FD_MAX RLIMIT_NOFILE
# else
#  ifdef RLIMIT_OPEN_MAX
#   define RLIMIT_FD_MAX RLIMIT_OPEN_MAX
#  else
#   warning No file descriptor limit was found
#  endif
# endif
#endif

#define COMM_SELECT_READ    1
#define COMM_SELECT_WRITE   2

/* How long can comm_select() wait for network events [milliseconds] */
#define SELECT_DELAY    500

#define LOWEST_SAFE_FD  4   /* skip stdin, stdout, stderr, and profiler */

/* Path to /dev/null */
#define PATH_DEVNULL "/dev/null"

extern const unsigned char ToUpperTab[];
#define ToUpper(c) (ToUpperTab[(unsigned char)(c)])


/*
 * NOTE: The following functions are NOT the same as strcasecmp
 * and strncasecmp! These functions use the Finnish (RFC1459)
 * character set. Do not replace!
 *
 * irccmp - case insensitive comparison of s1 and s2
 */
extern int irccmp(const char *, const char *);




typedef struct _dlink_node dlink_node;
typedef struct _dlink_list dlink_list;

struct _dlink_node
{
	void *data;
	dlink_node *prev;
	dlink_node *next;
};
  
struct _dlink_list
{
	dlink_node *head;
	dlink_node *tail;
	unsigned long length;
};

extern void dlinkAdd(void *data, dlink_node * m, dlink_list * list);
extern void dlinkAddBefore(dlink_node *b, void *data, dlink_node *m, dlink_list *list);
extern void dlinkAddTail(void *data, dlink_node *m, dlink_list *list);
extern void dlinkDelete(dlink_node *m, dlink_list *list);
extern void dlinkMoveList(dlink_list *from, dlink_list *to);
extern dlink_node *dlinkFind(dlink_list *m, void *data);
extern dlink_node *dlinkFindDelete(dlink_list *m, void *data);

#ifndef NDEBUG
void mem_frob(void *data, int len);
#else
#define mem_frob(x, y) 
#endif

/* These macros are basically swiped from the linux kernel
 * they are simple yet effective
 */

/*
 * Walks forward of a list.  
 * pos is your node
 * head is your list head
 */
#define DLINK_FOREACH(pos, head) for (pos = (head); pos != NULL; pos = pos->next)
   		
/*
 * Walks forward of a list safely while removing nodes 
 * pos is your node
 * n is another list head for temporary storage
 * head is your list head
 */
#define DLINK_FOREACH_SAFE(pos, n, head) for (pos = (head), n = pos ? pos->next : NULL; pos != NULL; pos = n, n = pos ? pos->next : NULL)
#define DLINK_FOREACH_PREV(pos, head) for (pos = (head); pos != NULL; pos = pos->prev)
              		       
/* Returns the list length */
#define dlink_list_length(list) (list)->length

/*
 * The functions below are included for the sake of inlining
 * hopefully this will speed up things just a bit
 * 
 */


/* 
 * dlink_ routines are stolen from squid, except for dlinkAddBefore,
 * which is mine.
 *   -- adrian
 */
extern inline void dlinkAdd(void *data, dlink_node * m, dlink_list * list)
{
 m->data = data;
 m->prev = NULL;
 m->next = list->head;
 /* Assumption: If list->tail != NULL, list->head != NULL */
 if (list->head != NULL)
   list->head->prev = m;
 else if (list->tail == NULL)
   list->tail = m;
 list->head = m;
 list->length++;
}

extern inline void dlinkAddBefore(dlink_node *b, void *data, dlink_node *m, dlink_list *list)
{
    /* Shortcut - if its the first one, call dlinkAdd only */
    if (b == list->head)
        dlinkAdd(data, m, list);
    else {
        m->data = data;
        b->prev->next = m;
        m->prev = b->prev;
        b->prev = m; 
        m->next = b;
	list->length++;
    }
}

extern inline void dlinkAddTail(void *data, dlink_node *m, dlink_list *list)
{
 m->data = data;
 m->next = NULL;
 m->prev = list->tail;
 /* Assumption: If list->tail != NULL, list->head != NULL */
 if (list->tail != NULL)
   list->tail->next = m;
 else if (list->head == NULL)
   list->head = m;
 list->tail = m;
 list->length++;
}

/* Execution profiles show that this function is called the most
 * often of all non-spontaneous functions. So it had better be
 * efficient. */
extern inline void dlinkDelete(dlink_node *m, dlink_list *list)
{
 /* Assumption: If m->next == NULL, then list->tail == m
  *      and:   If m->prev == NULL, then list->head == m
  */
 if (m->next)
   m->next->prev = m->prev;
 else {
   assert(list->tail == m);
   list->tail = m->prev;
 }
 if (m->prev)
   m->prev->next = m->next;
 else {
   assert(list->head == m);
   list->head = m->next;
 }
 /* Set this to NULL does matter */
 m->next = m->prev = NULL;
  list->length--;
}

/*
 * dlinkFind
 * inputs	- list to search 
 *		- data
 * output	- pointer to link or NULL if not found
 * side effects	- Look for ptr in the linked listed pointed to by link.
 */
extern inline dlink_node *dlinkFind(dlink_list *list, void *data)
{
  dlink_node *ptr;

  DLINK_FOREACH(ptr, list->head)
  {
    if (ptr->data == data)
      return(ptr);
  }

  return(NULL);
}

extern inline void dlinkMoveList(dlink_list *from, dlink_list *to)
{
  /* There are three cases */
  /* case one, nothing in from list */

    if (from->head == NULL)
      return;

  /* case two, nothing in to list */
  /* actually if to->head is NULL and to->tail isn't, thats a bug */

    if (to->head == NULL)
    {
       to->head = from->head;
       to->tail = from->tail;
       from->head = from->tail = NULL;
       to->length = from->length;
       from->length = 0;
       return;
    }

  /* third case play with the links */

    from->tail->next = to->head;
    from->head->prev = to->head->prev;
    to->head->prev = from->tail;
    to->head = from->head;
    from->head = from->tail = NULL;
    to->length += from->length;
    from->length = 0;

  /* I think I got that right */
}

extern inline dlink_node *dlinkFindDelete(dlink_list *list, void *data)
{
  dlink_node *m;

  DLINK_FOREACH(m, list->head)
  {
    if (m->data == data)
    {
      if (m->next)
        m->next->prev = m->prev;
      else
      {
        assert(list->tail == m);
        list->tail = m->prev;
      }
      if (m->prev)
        m->prev->next = m->next;
      else
      {
        assert(list->head == m);
        list->head = m->next;
      }
      /* Set this to NULL does matter */
      m->next = m->prev = NULL;
      list->length--;

      return(m);
    }
  }

  return(NULL);
}

extern dlink_list callback_list;  /* listing/debugging purposes */

typedef void *CBFUNC(va_list);

struct Callback
{
  char *name;
  dlink_list chain;
  dlink_node node;
  unsigned int called;
  time_t last;
};

extern struct Callback *register_callback(const char *, CBFUNC *);
extern void *execute_callback(struct Callback *, ...);
extern struct Callback *find_callback(const char *);
extern dlink_node *install_hook(struct Callback *, CBFUNC *);
extern void uninstall_hook(struct Callback *, CBFUNC *);
extern void *pass_callback(dlink_node *, ...);
extern void stats_hooks(struct Client *);

#define is_callback_present(c) (!!dlink_list_length(&c->chain))

/* Hybrid Support Headers End */














/* tests show that about 7 fds are not registered by fdlist.c, these
 * include std* descriptors + some others (by OpenSSL etc.). Note this is
 * intentionally too high, we don't want to eat fds up to the last one */
#define LEAKED_FDS       10

/* how many (privileged) clients can exceed max_clients */
#define MAX_BUFFER       60

#define MAXCLIENTS_MAX   (hard_fdlimit - LEAKED_FDS - MAX_BUFFER)
#define MAXCLIENTS_MIN   32

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

#define CLIENT_HEAP_SIZE 1024
#define FD_HASH_SIZE CLIENT_HEAP_SIZE

extern int number_fd;
extern int hard_fdlimit;
extern fde_t *fd_hash[];
extern fde_t *fd_next_in_loop;
extern struct Callback *fdlimit_cb;

extern void fdlist_init(void);
extern fde_t *lookup_fd(int);
extern void fd_open(fde_t *, int, int, const char *);
extern void fd_close(fde_t *);
extern void fd_dump(struct Client *);
#ifndef __GNUC__
extern void fd_note(fde_t *, const char *format, ...);
#else
extern void  fd_note(fde_t *, const char *format, ...)
	  __attribute__((format (printf, 2, 3)));
#endif
extern void close_standard_fds(void);
extern void close_fds(fde_t *);
extern void recalc_fdlimit(void *);

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
