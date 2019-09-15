/************************************************************************
 *   Unreal Internet Relay Chat, src/list.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Finland
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

#include "unrealircd.h"

void free_link(Link *);
Link *make_link();

ID_Copyright
    ("(C) 1988 University of Oulu, Computing Center and Jarkko Oikarinen");
ID_Notes("2.24 4/20/94");

#ifdef	DEBUGMODE
static struct liststats {
	int  inuse;
} cloc, crem, users, servs, links;

#endif

MODVAR int  flinks = 0;
MODVAR int  freelinks = 0;
MODVAR Link *freelink = NULL;
MODVAR Member *freemember = NULL;
MODVAR Membership *freemembership = NULL;
MODVAR int  numclients = 0;

/* unless documented otherwise, these are all local-only, except client_list. */
MODVAR struct list_head client_list, lclient_list, server_list, oper_list, unknown_list, global_server_list;

static mp_pool_t *user_pool = NULL;
static mp_pool_t *link_pool = NULL;

void initlists(void)
{
#ifdef	DEBUGMODE
	memset(&cloc, 0, sizeof(cloc));
	memset(&crem, 0, sizeof(crem));
	memset(&users, 0, sizeof(users));
	memset(&servs, 0, sizeof(servs));
	memset(&links, 0, sizeof(links));
#endif

	INIT_LIST_HEAD(&client_list);
	INIT_LIST_HEAD(&lclient_list);
	INIT_LIST_HEAD(&server_list);
	INIT_LIST_HEAD(&oper_list);
	INIT_LIST_HEAD(&unknown_list);
	INIT_LIST_HEAD(&global_server_list);

	user_pool = mp_pool_new(sizeof(ClientUser), 512 * 1024);
	link_pool = mp_pool_new(sizeof(Link), 512 * 1024);
}

/*
** Create a new Client structure and set it to initial state.
**
**	from == NULL,	create local client (a client connected
**			to a socket).
**
**	from,	create remote client (behind a socket
**			associated with the client defined by
**			'from'). ('from' is a local client!!).
*/
Client *make_client(Client *from, Client *servr)
{
	Client *cptr = NULL;

	cptr = safe_alloc(sizeof(Client));

#ifdef	DEBUGMODE
	if (!from)
		cloc.inuse++;
	else
		crem.inuse++;
#endif

	/* Note:  structure is zero (calloc) */
	cptr->direction = from ? from : cptr;	/* 'from' of local client is self! */
	cptr->user = NULL;
	cptr->serv = NULL;
	cptr->srvptr = servr;
	cptr->status = CLIENT_STATUS_UNKNOWN;

	INIT_LIST_HEAD(&cptr->client_node);
	INIT_LIST_HEAD(&cptr->client_hash);
	INIT_LIST_HEAD(&cptr->id_hash);

	(void)strcpy(cptr->ident, "unknown");
	if (!from)
	{
		/* Local client */
		const char *id;
		
		cptr->local = safe_alloc(sizeof(LocalClient));
		
		INIT_LIST_HEAD(&cptr->lclient_node);
		INIT_LIST_HEAD(&cptr->special_node);

		cptr->local->since = cptr->local->lasttime =
		cptr->lastnick = cptr->local->firsttime = TStime();
		cptr->local->class = NULL;
		cptr->local->passwd = NULL;
		cptr->local->sockhost[0] = '\0';
		cptr->local->authfd = -1;
		cptr->local->fd = -1;

		dbuf_queue_init(&cptr->local->recvQ);
		dbuf_queue_init(&cptr->local->sendQ);

		while (hash_find_id((id = uid_get()), NULL) != NULL)
			;
		strlcpy(cptr->id, id, sizeof cptr->id);
		add_to_id_hash_table(cptr->id, cptr);
	}
	return (cptr);
}

void free_client(Client *cptr)
{
	if (!list_empty(&cptr->client_node))
		list_del(&cptr->client_node);

	if (MyConnect(cptr))
	{
		if (!list_empty(&cptr->lclient_node))
			list_del(&cptr->lclient_node);
		if (!list_empty(&cptr->special_node))
			list_del(&cptr->special_node);

		RunHook(HOOKTYPE_FREE_CLIENT, cptr);
		if (cptr->local)
		{
			safe_free(cptr->local->passwd);
			safe_free(cptr->local->error_str);
			if (cptr->local->hostp)
				unreal_free_hostent(cptr->local->hostp);
			
			safe_free(cptr->local);
		}
		if (*cptr->id)
		{
			/* This is already del'd in exit_one_client, so we
			 * only have it here in case a shortcut was taken,
			 * such as from add_connection() to free_client().
			 */
			del_from_id_hash_table(cptr->id, cptr);
		}
	}
	
	safe_free(cptr->ip);

	safe_free(cptr);
}

/*
** 'make_user' add's an User information block to a client
** if it was not previously allocated.
*/
ClientUser *make_user(Client *cptr)
{
	ClientUser *user;

	user = cptr->user;
	if (!user)
	{
		user = mp_pool_get(user_pool);
		memset(user, 0, sizeof(ClientUser));

#ifdef	DEBUGMODE
		users.inuse++;
#endif
		user->swhois = NULL;
		user->away = NULL;
		user->flood.away_t = 0;
		user->flood.away_c = 0;
		user->refcnt = 1;
		user->joined = 0;
		user->channel = NULL;
		user->invited = NULL;
		user->server = NULL;
		strlcpy(user->svid, "0", sizeof(user->svid));
		user->whowas = NULL;
		user->snomask = 0;
		if (cptr->ip)
		{
			/* initially set cptr->user->realhost to IP */
			strlcpy(user->realhost, cptr->ip, sizeof(user->realhost));
		} else {
			*user->realhost = '\0';
		}
		user->virthost = NULL;
		cptr->user = user;		
	}
	return user;
}

Server *make_server(Client *cptr)
{

	Server *serv = cptr->serv;

	if (!serv)
	{
		serv = safe_alloc(sizeof(Server));
#ifdef	DEBUGMODE
		servs.inuse++;
#endif
		*serv->by = '\0';
		serv->users = 0;
		serv->up = NULL;
		cptr->serv = serv;
	}
	if (strlen(cptr->id) > 3)
	{
		/* Probably the auto-generated UID for a server that
		 * still uses the old protocol (without SID).
		 */
		del_from_id_hash_table(cptr->id, cptr);
		*cptr->id = '\0';
	}
	return cptr->serv;
}

/*
** free_user
**	Decrease user reference count by one and realease block,
**	if count reaches 0
*/
void free_user(ClientUser *user, Client *cptr)
{
	if (user->refcnt == 0)
		sendto_realops("[BUG] free_user: ref count for '%s' was already 0!?", user->username);
	else
		--user->refcnt;
	if (user->refcnt == 0)
	{
		RunHook2(HOOKTYPE_FREE_USER, user, cptr);
		if (user->away)
			safe_free(user->away);
		if (user->swhois)
		{
			SWhois *s, *s_next;
			for (s = user->swhois; s; s = s_next)
			{
				s_next = s->next;
				safe_free(s->line);
				safe_free(s->setby);
				safe_free(s);
			}
		}
		if (user->virthost)
			safe_free(user->virthost);
		if (user->operlogin)
			safe_free(user->operlogin);
		mp_pool_release(user);
#ifdef	DEBUGMODE
		users.inuse--;
#endif
	}
}

/*
 * taken the code from ExitOneClient() for this and placed it here.
 * - avalon
 */
void remove_client_from_list(Client *cptr)
{
	list_del(&cptr->client_node);
	if (IsServer(cptr))
	{
		ircstats.servers--;
	}
	if (IsUser(cptr))
	{
		if (IsInvisible(cptr))
		{
			ircstats.invisible--;
		}
		if (IsOper(cptr) && !IsHideOper(cptr))
		{
			ircstats.operators--;
			VERIFY_OPERCOUNT(cptr, "rmvlist");
		}
		ircstats.clients--;
		if (cptr->srvptr && cptr->srvptr->serv)
			cptr->srvptr->serv->users--;
	}
	if (IsUnknown(cptr) || IsConnecting(cptr) || IsHandshake(cptr)
		|| IsTLSHandshake(cptr)
	)
		ircstats.unknown--;

	if (IsUser(cptr))	/* Only persons can have been added before */
	{
		add_history(cptr, 0);
		off_history(cptr);	/* Remove all pointers to cptr */
	}
	
	if (cptr->user)
		(void)free_user(cptr->user, cptr);
	if (cptr->serv)
	{
		safe_free(cptr->serv->features.usermodes);
		safe_free(cptr->serv->features.chanmodes[0]);
		safe_free(cptr->serv->features.chanmodes[1]);
		safe_free(cptr->serv->features.chanmodes[2]);
		safe_free(cptr->serv->features.chanmodes[3]);
		safe_free(cptr->serv->features.software);
		safe_free(cptr->serv->features.nickchars);
		safe_free(cptr->serv);
#ifdef	DEBUGMODE
		servs.inuse--;
#endif
	}
#ifdef	DEBUGMODE
	if (cptr->local && cptr->local->fd == -2)
		cloc.inuse--;
	else
		crem.inuse--;
#endif
	if (!list_empty(&cptr->client_node))
		abort();
	if (!list_empty(&cptr->client_hash))
		abort();
	if (!list_empty(&cptr->id_hash))
		abort();
	(void)free_client(cptr);
	numclients--;
	return;
}

/*
 * although only a small routine, it appears in a number of places
 * as a collection of a few lines...functions like this *should* be
 * in this file, shouldnt they ?  after all, this is list.c, isnt it ?
 * -avalon
 */
void add_client_to_list(Client *cptr)
{
	list_add(&cptr->client_node, &client_list);
}

/** Make a new link entry.
 * @notes When you no longer need it, call free_link()
 *        NEVER call free() or safe_free() on it.
 */
Link *make_link(void)
{
	Link *l = mp_pool_get(link_pool);
	memset(l, 0, sizeof(Link));
#ifdef	DEBUGMODE
	links.inuse++;
#endif
	return l;
}

/** Releases a link that was previously created with make_link() */
void free_link(Link *lp)
{
	mp_pool_release(lp);

#ifdef	DEBUGMODE
	links.inuse--;
#endif
}

Ban *make_ban(void)
{
	Ban *lp;

	lp = safe_alloc(sizeof(Ban));
#ifdef	DEBUGMODE
	links.inuse++;
#endif
	return lp;
}

void free_ban(Ban *lp)
{
	safe_free(lp);
#ifdef	DEBUGMODE
	links.inuse--;
#endif
}

void add_ListItem(ListStruct *item, ListStruct **list)
{
	item->next = *list;
	item->prev = NULL;
	if (*list)
		(*list)->prev = item;
	*list = item;
}

/* (note that if you end up using this, you should probably
 *  use a circular linked list instead)
 */
void append_ListItem(ListStruct *item, ListStruct **list)
{
	ListStruct *l;

	if (!*list)
	{
		*list = item;
		return;
	}

	for (l = *list; l->next; l = l->next);
	l->next = item;
	item->prev = l;
}

void del_ListItem(ListStruct *item, ListStruct **list)
{
	if (!item)
		return;

	if (item->prev)
		item->prev->next = item->next;
	if (item->next)
		item->next->prev = item->prev;
	if (*list == item)
		*list = item->next; /* new head */
}

/** Add item to list with a 'priority'.
 * If there are multiple items with the same priority then it will be
 * added as the last item within.
 */
void add_ListItemPrio(ListStructPrio *new, ListStructPrio **list, int priority)
{
	ListStructPrio *x, *last = NULL;
	
	if (!*list)
	{
		/* We are the only item. Easy. */
		*list = new;
		return;
	}
	
	for (x = *list; x; x = x->next)
	{
		last = x;
		if (x->priority >= priority)
			break;
	}

	if (x)
	{
		if (x->prev)
		{
			/* We will insert ourselves just before this item */
			new->prev = x->prev;
			new->next = x;
			x->prev->next = new;
			x->prev = new;
		} else {
			/* We are the new head */
			*list = new;
			new->next = x;
			x->prev = new;
		}
	} else
	{
		/* We are the last item */
		last->next = new;
		new->prev = last;
	}
}

/* NameList functions */

void _add_name_list(NameList **list, char *name)
{
	NameList *e = safe_alloc(sizeof(NameList)+strlen(name));
	strcpy(e->name, name); /* safe, allocated above */
	AddListItem(e, *list);
}

void _free_entire_name_list(NameList *n)
{
	NameList *n_next;

	for (; n; n = n_next)
	{
		n_next = n->next;
		safe_free(n);
	}
}

void _del_name_list(NameList **list, char *name)
{
	NameList *e = find_name_list(*list, name);
	if (e)
	{
		DelListItem(e, *list);
		safe_free(e);
		return;
	}
}

/** Find an entry in a NameList - case insensitive comparisson.
 * @ingroup ListFunctions
 */
NameList *find_name_list(NameList *list, char *name)
{
	NameList *e;

	for (e = list; e; e = e->next)
	{
		if (!strcasecmp(e->name, name))
		{
			return e;
		}
	}
	return NULL;
}

/** Find an entry in a NameList by running match_simpl() on it.
 * @ingroup ListFunctions
 */
NameList *find_name_list_match(NameList *list, char *name)
{
	NameList *e;

	for (e = list; e; e = e->next)
	{
		if (match_simple(e->name, name))
		{
			return e;
		}
	}
	return NULL;
}

