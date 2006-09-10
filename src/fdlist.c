/************************************************************************
 *   Unreal Internet Relay Chat Daemon, src/fdlist.c
 *   Copyright (C) Mika Nystrom
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id$ */

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "h.h"
#include "config.h"
#include "fdlist.h"
#include "proto.h"
#include <string.h>
#ifndef _WIN32
#include <sys/resource.h>
#endif
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>

/*
 * Hybrid Support Routines At first stage we can use it. In time i will port
 * them to Unreal stuff.
 */
#ifdef NEW_IO


const unsigned char ToUpperTab[] = {
  0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa,
  0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x13, 0x14,
  0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d,
  0x1e, 0x1f,
  ' ', '!', '"', '#', '$', '%', '&', 0x27, '(', ')',
  '*', '+', ',', '-', '.', '/',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  ':', ';', '<', '=', '>', '?',
  '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I',
  'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S',
  'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '^',
  0x5f,
  '`', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I',
  'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S',
  'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '^',
  0x7f,
  0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
  0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
  0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
  0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
  0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9,
  0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
  0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9,
  0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
  0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9,
  0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
  0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9,
  0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
  0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
  0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
  0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9,
  0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};


/*
 * irccmp - case insensitive comparison of two 0 terminated strings.
 *
 *      returns  0, if s1 equal to s2
 *               1, if not
 */
int
irccmp(const char *s1, const char *s2)
{
  const unsigned char *str1 = (const unsigned char *) s1;
  const unsigned char *str2 = (const unsigned char *) s2;

  assert(s1 != NULL);
  assert(s2 != NULL);

  while (ToUpper(*str1) == ToUpper(*str2))
  {
    if (*str1 == '\0')
      return 0;
    str1++;
    str2++;
  }

  return 1;
}



/*
 * dlink_ routines are stolen from squid, except for dlinkAddBefore,
 * which is mine.
 *   -- adrian
 */
void dlinkAdd(void *data, dlink_node * m, dlink_list * list)
{
 m->data = data;
 m->prev = NULL;
 m->next = list->head;

 /* Assumption: If list->tail != NULL, list->head != NULL */
 if (list->head != NULL)
   list->head->prev = m;
 else /* if (list->tail == NULL) */
   list->tail = m;

 list->head = m;
 list->length++;
}

void
dlinkAddBefore(dlink_node *b, void *data, dlink_node *m, dlink_list *list)
{
    /* Shortcut - if its the first one, call dlinkAdd only */
    if (b == list->head)
    {
        dlinkAdd(data, m, list);
    }
    else
    {
      m->data = data;
      b->prev->next = m;
      m->prev = b->prev;
      b->prev = m;
      m->next = b;
      list->length++;
    }
}

void
dlinkAddTail(void *data, dlink_node *m, dlink_list *list)
{
  m->data = data;
  m->next = NULL;
  m->prev = list->tail;
  /* Assumption: If list->tail != NULL, list->head != NULL */
  if (list->tail != NULL)
    list->tail->next = m;
  else /* if (list->head == NULL) */
    list->head = m;

  list->tail = m;
  list->length++;
}

/* Execution profiles show that this function is called the most
 * often of all non-spontaneous functions. So it had better be
 * efficient. */
void
dlinkDelete(dlink_node *m, dlink_list *list)
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
 * inputs - list to search
 *    - data
 * output - pointer to link or NULL if not found
 * side effects - Look for ptr in the linked listed pointed to by link.
 */
dlink_node *
dlinkFind(dlink_list *list, void *data)
{
  dlink_node *ptr;

  DLINK_FOREACH(ptr, list->head)
  {
    if (ptr->data == data)
      return(ptr);
  }

  return(NULL);
}

void
dlinkMoveList(dlink_list *from, dlink_list *to)
{
  /* There are three cases */
  /* case one, nothing in from list */
  if(from->head == NULL)
    return;

  /* case two, nothing in to list */
  /* actually if to->head is NULL and to->tail isn't, thats a bug */

   if(to->head == NULL)
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

dlink_node *
dlinkFindDelete(dlink_list *list, void *data)
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




dlink_list callback_list = {NULL, NULL, 0};

struct Callback *register_callback(const char *name, CBFUNC *func)
{
  struct Callback *cb;

  if (name != NULL)
    if ((cb = find_callback(name)) != NULL)
    {
      if (func != NULL)
        dlinkAddTail(func, MyMalloc(sizeof(dlink_node)), &cb->chain);
      return (cb);
    }

  cb = MyMalloc(sizeof(struct Callback));
  if (func != NULL)
    dlinkAdd(func, MyMalloc(sizeof(dlink_node)), &cb->chain);
  if (name != NULL)
  {
    DupString(cb->name, name);
    dlinkAdd(cb, &cb->node, &callback_list);
  }
  return (cb);
}

/*
 * find_callback()
 *
 * Finds a named callback.
 *
 * inputs:
 *   name  -  name of the callback
 * output: pointer to Callback structure or NULL if not found
 */
struct Callback *
find_callback(const char *name)
{
  struct Callback *cb;
  dlink_node *ptr;

  DLINK_FOREACH(ptr, callback_list.head)
  {
    cb = ptr->data;
    if (!irccmp(cb->name, name))
      return (cb);
  }

  return (NULL);
}

/*
 * install_hook()
 *
 * Installs a hook for the given callback.
 *
 * inputs:
 *   cb      -  pointer to Callback structure
 *   hook    -  address of hook function
 * output: pointer to dlink_node of the hook (used when
 *         passing control to the next hook in the chain);
 *         valid till uninstall_hook() is called
 *
 * NOTE: The new hook is installed at the beginning of the chain,
 * so it has full control over functions installed earlier.
 */
dlink_node *install_hook(struct Callback *cb, CBFUNC *hook)
{
  dlink_node *node = MyMalloc(sizeof(dlink_node));

  dlinkAdd(hook, node, &cb->chain);
  return (node);
}

/*
 * uninstall_hook()
 *
 * Removes a specific hook for the given callback.
 *
 * inputs:
 *   cb      -  pointer to Callback structure
 *   hook    -  address of hook function
 * output: none
 */
void
uninstall_hook(struct Callback *cb, CBFUNC *hook)
{
  /* let it core if not found */
  dlink_node *ptr = dlinkFind(&cb->chain, hook);

  dlinkDelete(ptr, &cb->chain);
  MyFree(ptr);
}




#endif



#ifndef NEW_IO
extern fdlist default_fdlist;
extern fdlist busycli_fdlist;
extern fdlist serv_fdlist;
extern fdlist oper_fdlist;

#define FDLIST_DEBUG

void addto_fdlist(int fd, fdlist * listp)
{
	int  index;
#ifdef FDLIST_DEBUG
	int i;
#endif

	/* I prefer this little 5-cpu-cycles-check over memory corruption. -- Syzop */
	if ((fd < 0) || (fd >= MAXCONNECTIONS))
	{
		sendto_realops("[BUG] trying to add fd #%d to %p (%p/%p/%p/%p), range is 0..%d",
			fd, listp, &default_fdlist, &busycli_fdlist, &serv_fdlist, &oper_fdlist,
			MAXCONNECTIONS);
		ircd_log(LOG_ERROR, "[BUG] trying to add fd #%d to %p (%p/%p/%p/%p), range is 0..%d",
			fd, listp, &default_fdlist, &busycli_fdlist, &serv_fdlist, &oper_fdlist,
			MAXCONNECTIONS);
		return;
	}

#ifdef FDLIST_DEBUG
	for (i = listp->last_entry; i; i--)
	{
		if (listp->entry[i] == fd)
		{
			char buf[2048];
			ircsprintf(buf, "[BUG] addto_fdlist() called for duplicate entry! fd=%d, fdlist=%p, client=%s (%p/%p/%p/%p)",
				fd, listp, local[fd] ? local[fd]->name : "<null>", &default_fdlist, &busycli_fdlist, &serv_fdlist, &oper_fdlist);
			sendto_realops("%s", buf);
			ircd_log(LOG_ERROR, "%s", buf);
			return;
		}
	}
#endif

	if ((index = ++listp->last_entry) >= MAXCONNECTIONS)
	{
		/*
		 * list too big.. must exit 
		 */
		--listp->last_entry;
		ircd_log(LOG_ERROR, "fdlist.c list too big, must exit...");
		abort();
	}
	else
		listp->entry[index] = fd;
	return;
}

void delfrom_fdlist(int fd, fdlist * listp)
{
	int  i;
#ifdef FDLIST_DEBUG
	int cnt = 0;
#endif

	/* I prefer this little 5-cpu-cycles-check over memory corruption. -- Syzop */
	if ((fd < 0) || (fd >= MAXCONNECTIONS))
	{
		sendto_realops("[BUG] trying to remove fd #%d to %p (%p/%p/%p/%p), range is 0..%d",
			fd, listp, &default_fdlist, &busycli_fdlist, &serv_fdlist, &oper_fdlist,
			MAXCONNECTIONS);
		ircd_log(LOG_ERROR, "[BUG] trying to remove fd #%d to %p (%p/%p/%p/%p), range is 0..%d",
			fd, listp, &default_fdlist, &busycli_fdlist, &serv_fdlist, &oper_fdlist,
			MAXCONNECTIONS);
		return;
	}

#ifdef FDLIST_DEBUG
	for (i = listp->last_entry; i; i--)
	{
		if (listp->entry[i] == fd)
			cnt++;
	}
	if (cnt > 1)
	{
		char buf[2048];
		ircsprintf(buf, "[BUG] delfrom_fdlist() called, duplicate entries detected! fd=%d, fdlist=%p, client=%s (%p/%p/%p/%p)",
			fd, listp, local[fd] ? local[fd]->name : "<null>", &default_fdlist, &busycli_fdlist, &serv_fdlist, &oper_fdlist);
		sendto_realops("%s", buf);
		ircd_log(LOG_ERROR, "%s", buf);
		return;
	}
#endif


	for (i = listp->last_entry; i; i--)
	{
		if (listp->entry[i] == fd)
			break;
	}
	if (!i)
		return;		/*
				 * could not find it! 
				 */
	/*
	 * swap with last_entry 
	 */
	if (i == listp->last_entry)
	{
		listp->entry[i] = 0;
		listp->last_entry--;
		return;
	}
	else
	{
		listp->entry[i] = listp->entry[listp->last_entry];
		listp->entry[listp->last_entry] = 0;
		listp->last_entry--;
		return;
	}
}

void init_fdlist(fdlist * listp)
{
	listp->last_entry = 0;
	memset((char *)listp->entry, '\0', sizeof(listp->entry));
	return;
}

#else /* ifndef NEW_IO */
fde_t *fd_hash[FD_HASH_SIZE];
fde_t *fd_next_in_loop = NULL;
int number_fd = LEAKED_FDS;
int hard_fdlimit = 0;
struct Callback *fdlimit_cb = NULL;

static void *changing_fdlimit(va_list args)
{
  int old_fdlimit = hard_fdlimit;

  hard_fdlimit = va_arg(args, int);
/* FIXME: Hybrid uses ServerInfo structure for server specific flags
 * have to convert it to Unreal
 * if (ServerInfo.max_clients > MAXCLIENTS_MAX)
 */
  if (1024 > MAXCLIENTS_MAX)
  {
    if (old_fdlimit != 0)
      sendto_realops("HARD_FDLIMIT changed to %d, adjusting MAXCLIENTS to %d", hard_fdlimit, MAXCLIENTS_MAX);

 /* FIXME:    ServerInfo.max_clients = MAXCLIENTS_MAX; */
  }

  return NULL;
}

void fdlist_init(void)
{
  memset(&fd_hash, 0, sizeof(fd_hash));

  fdlimit_cb = register_callback("changing_fdlimit", changing_fdlimit);
  eventAddIsh("recalc_fdlimit", recalc_fdlimit, NULL, 58);
  recalc_fdlimit(NULL);
}

void
recalc_fdlimit(void *unused)
{
#ifdef _WIN32
  /* this is what WSAStartup() usually returns. Even though they say
   * the value is for compatibility reasons and should be ignored,
   * we actually can create even more sockets... */
  hard_fdlimit = 32767;
#else
  int fdmax;
  struct rlimit limit;

  if (!getrlimit(RLIMIT_FD_MAX, &limit))
  {
    limit.rlim_cur = limit.rlim_max;
    setrlimit(RLIMIT_FD_MAX, &limit);
  }

  fdmax = getdtablesize();

  /* allow MAXCLIENTS_MIN clients even at the cost of MAX_BUFFER and
   * some not really LEAKED_FDS */
  fdmax = IRCD_MAX(fdmax, LEAKED_FDS + MAX_BUFFER + MAXCLIENTS_MIN);

  /* under no condition shall this raise over 65536
   * for example user ip heap is sized 2*hard_fdlimit */
  fdmax = IRCD_MIN(fdmax, 65536);

  if (fdmax != hard_fdlimit)
    execute_callback(fdlimit_cb, fdmax);
#endif
}

static inline unsigned int hash_fd(int fd)
{
#ifdef _WIN32
  return ((((unsigned) fd) >> 2) % FD_HASH_SIZE);
#else
  return (((unsigned) fd) % FD_HASH_SIZE);
#endif
}

fde_t * lookup_fd(int fd)
{
  fde_t *F = fd_hash[hash_fd(fd)];

  while (F)
  {
    if (F->fd == fd)
      return (F);
    F = F->hnext;
  }

  return (NULL);
}

/* Called to open a given filedescriptor */
void fd_open(fde_t *F, int fd, int is_socket, const char *desc)
{
  unsigned int hashv = hash_fd(fd);
  assert(fd >= 0);

  F->fd = fd;
  F->comm_index = -1;
  if (desc)
    strlcpy(F->desc, desc, sizeof(F->desc));
  /* Note: normally we'd have to clear the other flags,
   * but currently F is always cleared before calling us.. */
  F->flags.open = 1;
  F->flags.is_socket = is_socket;
  F->hnext = fd_hash[hashv];
  fd_hash[hashv] = F;

  number_fd++;
}

/* Called to close a given filedescriptor */
void fd_close(fde_t *F)
{
  unsigned int hashv = hash_fd(F->fd);

  if (F == fd_next_in_loop)
    fd_next_in_loop = F->hnext;

  if (F->flags.is_socket)
    comm_setselect(F, COMM_SELECT_WRITE | COMM_SELECT_READ, NULL, NULL, 0);

  if (F->dns_query != NULL)
  {
    delete_resolver_queries(F->dns_query);
    MyFree(F->dns_query);
  }

#ifdef HAVE_LIBCRYPTO
  if (F->ssl)
    SSL_free(F->ssl);
#endif

  if (fd_hash[hashv] == F)
    fd_hash[hashv] = F->hnext;
  else {
    fde_t *prev;

    /* let it core if not found */
    for (prev = fd_hash[hashv]; prev->hnext != F; prev = prev->hnext)
      ;
    prev->hnext = F->hnext;
  }

  /* Unlike squid, we're actually closing the FD here! -- adrian */
#ifdef _WIN32
  if (F->flags.is_socket)
    closesocket(F->fd);
  else
    CloseHandle((HANDLE)F->fd);
#else
  close(F->fd);
#endif
  number_fd--;

  memset(F, 0, sizeof(fde_t));
}

/*
 * fd_note() - set the fd note
 *
 * Note: must be careful not to overflow fd_table[fd].desc when
 *       calling.
 */
void fd_note(fde_t *F, const char *format, ...)
{
  va_list args;

  if (format != NULL)
  {
    va_start(args, format);
    vsnprintf(F->desc, sizeof(F->desc), format, args);
    va_end(args);
  }
  else
    F->desc[0] = '\0';
}

/* Make sure stdio descriptors (0-2) and profiler descriptor (3)
 * always go somewhere harmless.  Use -foreground for profiling
 * or executing from gdb */
#ifndef _WIN32
void close_standard_fds(void)
{
  int i;

  for (i = 0; i < LOWEST_SAFE_FD; i++)
  {
    close(i);
    if (open(PATH_DEVNULL, O_RDWR) < 0)
      exit(-1); /* we're hosed if we can't even open /dev/null */
  }
}
#endif

void close_fds(fde_t *one)
{
  int i;
  fde_t *F;

  for (i = 0; i < FD_HASH_SIZE; i++)
    for (F = fd_hash[i]; F != NULL; F = F->hnext)
      if (F != one)
      {
#ifdef _WIN32
        if (F->flags.is_socket)
          closesocket(F->fd);
        else
    CloseHandle((HANDLE)F->fd);
#else
        close(F->fd);
#endif
      }
}


#endif /* ifndef NEW_IO */
