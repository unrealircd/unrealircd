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

// TODO: Document whether servers are included or excluded in these lists...

MODVAR struct list_head unknown_list;		/**< Local clients in handshake (may become a user or server later) */
MODVAR struct list_head control_list;		/**< Local "control channel" clients */
MODVAR struct list_head lclient_list;		/**< Local clients (users only, right?) */
MODVAR struct list_head client_list;		/**< All clients - local and remote (not in handshake) */
MODVAR struct list_head server_list;		/**< Locally connected servers */
MODVAR struct list_head oper_list;		/**< Locally connected IRC Operators */
MODVAR struct list_head global_server_list;	/**< All servers (local and remote) */
MODVAR struct list_head dead_list;		/**< All dead clients (local and remote) that will soon be freed in the main loop */

static mp_pool_t *client_pool = NULL;
static mp_pool_t *local_client_pool = NULL;
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
	INIT_LIST_HEAD(&control_list);
	INIT_LIST_HEAD(&global_server_list);
	INIT_LIST_HEAD(&dead_list);

	client_pool = mp_pool_new(sizeof(Client), 512 * 1024);
	local_client_pool = mp_pool_new(sizeof(LocalClient), 512 * 1024);
	user_pool = mp_pool_new(sizeof(User), 512 * 1024);
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
	Client *client = mp_pool_get(client_pool);
	memset(client, 0, sizeof(Client));

#ifdef	DEBUGMODE
	if (!from)
		cloc.inuse++;
	else
		crem.inuse++;
#endif

	/* Note: all fields are already NULL/0, no need to set here */
	client->direction = from ? from : client;	/* 'from' of local client is self! */
	client->uplink = servr;
	client->status = CLIENT_STATUS_UNKNOWN;

	INIT_LIST_HEAD(&client->client_node);
	INIT_LIST_HEAD(&client->client_hash);
	INIT_LIST_HEAD(&client->id_hash);

	strlcpy(client->ident, "unknown", sizeof(client->ident));
	if (!from)
	{
		/* Local client */
		const char *id;
		
		client->local = mp_pool_get(local_client_pool);
		memset(client->local, 0, sizeof(LocalClient));
		
		INIT_LIST_HEAD(&client->lclient_node);
		INIT_LIST_HEAD(&client->special_node);

		client->local->fake_lag = client->local->last_msg_received =
		client->lastnick = client->local->creationtime =
		client->local->idle_since = TStime();
		client->local->class = NULL;
		client->local->passwd = NULL;
		client->local->sockhost[0] = '\0';
		client->local->authfd = -1;
		client->local->fd = -1;

		dbuf_queue_init(&client->local->recvQ);
		dbuf_queue_init(&client->local->sendQ);

		while (hash_find_id((id = uid_get()), NULL) != NULL)
			;
		strlcpy(client->id, id, sizeof client->id);
		add_to_id_hash_table(client->id, client);
	}
	return client;
}

void free_client(Client *client)
{
	if (!list_empty(&client->client_node))
		list_del(&client->client_node);

	if (MyConnect(client))
	{
		if (!list_empty(&client->lclient_node))
			list_del(&client->lclient_node);
		if (!list_empty(&client->special_node))
			list_del(&client->special_node);

		RunHook(HOOKTYPE_FREE_CLIENT, client);
		if (client->local)
		{
			if (client->local->listener)
			{
				if (client->local->listener && !IsOutgoing(client))
				{
					ConfigItem_listen *listener = client->local->listener;
					listener->clients--;
					if (listener->flag.temporary && (listener->clients == 0))
					{
						/* Call listen cleanup */
						listen_cleanup();
					}
				}
			}
			safe_free(client->local->passwd);
			safe_free(client->local->error_str);
			if (client->local->hostp)
				unreal_free_hostent(client->local->hostp);
			
			mp_pool_release(client->local);
		}
		if (*client->id)
		{
			/* This is already del'd in exit_one_client, so we
			 * only have it here in case a shortcut was taken,
			 * such as from add_connection() to free_client().
			 */
			del_from_id_hash_table(client->id, client);
		}
	}
	
	safe_free(client->ip);

	mp_pool_release(client);
}

/*
** 'make_user' add's an User information block to a client
** if it was not previously allocated.
*/
User *make_user(Client *client)
{
	User *user;

	user = client->user;
	if (!user)
	{
		user = mp_pool_get(user_pool);
		memset(user, 0, sizeof(User));

#ifdef	DEBUGMODE
		users.inuse++;
#endif
		strlcpy(user->account, "0", sizeof(user->account));
		if (client->ip)
		{
			/* initially set client->user->realhost to IP */
			strlcpy(user->realhost, client->ip, sizeof(user->realhost));
		} else {
			*user->realhost = '\0';
		}
		user->virthost = NULL;
		client->user = user;		
	}
	return user;
}

Server *make_server(Client *client)
{
	Server *serv = client->server;

	if (!serv)
	{
		serv = safe_alloc(sizeof(Server));
#ifdef	DEBUGMODE
		servs.inuse++;
#endif
		*serv->by = '\0';
		serv->users = 0;
		client->server = serv;
	}
	if (strlen(client->id) > 3)
	{
		/* Probably the auto-generated UID for a server that
		 * still uses the old protocol (without SID).
		 */
		del_from_id_hash_table(client->id, client);
		*client->id = '\0';
	}
	return client->server;
}

/*
** free_user
**	Decrease user reference count by one and realease block,
**	if count reaches 0
*/
void free_user(Client *client)
{
	RunHook(HOOKTYPE_FREE_USER, client);
	safe_free(client->user->away);
	if (client->user->swhois)
	{
		SWhois *s, *s_next;
		for (s = client->user->swhois; s; s = s_next)
		{
			s_next = s->next;
			safe_free(s->line);
			safe_free(s->setby);
			safe_free(s);
		}
		client->user->swhois = NULL;
	}
	safe_free(client->user->virthost);
	safe_free(client->user->operlogin);
	safe_free(client->user->snomask);
	mp_pool_release(client->user);
#ifdef	DEBUGMODE
	users.inuse--;
#endif
	client->user = NULL;
}

/*
 * taken the code from ExitOneClient() for this and placed it here.
 * - avalon
 */
void remove_client_from_list(Client *client)
{
	list_del(&client->client_node);
	if (MyConnect(client))
	{
		if (!list_empty(&client->lclient_node))
			list_del(&client->lclient_node);
		if (!list_empty(&client->special_node))
			list_del(&client->special_node);
	}
	if (IsServer(client))
	{
		irccounts.servers--;
	}
	if (IsUser(client))
	{
		if (IsInvisible(client))
		{
			irccounts.invisible--;
		}
		if (IsOper(client) && !IsHideOper(client))
		{
			irccounts.operators--;
			VERIFY_OPERCOUNT(client, "rmvlist");
		}
		irccounts.clients--;
		if (client->uplink && client->uplink->server)
			client->uplink->server->users--;
	}
	if (IsUnknown(client) || IsConnecting(client) || IsHandshake(client)
		|| IsTLSHandshake(client)
	)
		irccounts.unknown--;

	if (IsUser(client))	/* Only persons can have been added before */
	{
		add_history(client, 0);
		off_history(client);	/* Remove all pointers to client */
	}
	
	if (client->user)
		free_user(client);
	if (client->server)
	{
		safe_free(client->server->features.usermodes);
		safe_free(client->server->features.chanmodes[0]);
		safe_free(client->server->features.chanmodes[1]);
		safe_free(client->server->features.chanmodes[2]);
		safe_free(client->server->features.chanmodes[3]);
		safe_free(client->server->features.software);
		safe_free(client->server->features.nickchars);
		safe_free(client->server);
#ifdef	DEBUGMODE
		servs.inuse--;
#endif
	}
#ifdef	DEBUGMODE
	if (client->local && client->local->fd == -2)
		cloc.inuse--;
	else
		crem.inuse--;
#endif
	if (!list_empty(&client->client_node))
		abort();
	if (!list_empty(&client->client_hash))
		abort();
	if (!list_empty(&client->id_hash))
		abort();
	numclients--;
	/* Add to killed clients list */
	list_add(&client->client_node, &dead_list);
	// THIS IS NOW DONE IN THE MAINLOOP --> free_client(client);
	SetDead(client);
	SetDeadSocket(client);
	return;
}

/*
 * although only a small routine, it appears in a number of places
 * as a collection of a few lines...functions like this *should* be
 * in this file, shouldnt they ?  after all, this is list.c, isnt it ?
 * -avalon
 */
void add_client_to_list(Client *client)
{
	list_add(&client->client_node, &client_list);
}

/** Make a new link entry.
 * @note  When you no longer need it, call free_link()
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

/** Returns the length (entry count) of a +beI list */
int link_list_length(Link *lp)
{
	int  count = 0;

	for (; lp; lp = lp->next)
		count++;
	return count;
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
	/* And update 'item', prev/next should point nowhere anymore */
	item->prev = item->next = NULL;
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

void _add_name_list(NameList **list, const char *name)
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

void _del_name_list(NameList **list, const char *name)
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
NameList *find_name_list(NameList *list, const char *name)
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

/** Find an entry in a NameList by running match_simple() on it.
 * @ingroup ListFunctions
 */
NameList *find_name_list_match(NameList *list, const char *name)
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

void add_nvplist(NameValuePrioList **lst, int priority, const char *name, const char *value)
{
	va_list vl;
	NameValuePrioList *e = safe_alloc(sizeof(NameValuePrioList));
	safe_strdup(e->name, name);
	if (value && *value)
		safe_strdup(e->value, value);
	AddListItemPrio(e, *lst, priority);
}

NameValuePrioList *find_nvplist(NameValuePrioList *list, const char *name)
{
	NameValuePrioList *e;

	for (e = list; e; e = e->next)
	{
		if (!strcasecmp(e->name, name))
		{
			return e;
		}
	}
	return NULL;
}

void add_fmt_nvplist(NameValuePrioList **lst, int priority, const char *name, FORMAT_STRING(const char *format), ...)
{
	char value[512];
	va_list vl;
	*value = '\0';
	if (format)
	{
		va_start(vl, format);
		vsnprintf(value, sizeof(value), format, vl);
		va_end(vl);
	}
	add_nvplist(lst, priority, name, value);
}

void free_nvplist(NameValuePrioList *lst)
{
	NameValuePrioList *e, *e_next;
	for (e = lst; e; e = e_next)
	{
		e_next = e->next;
		safe_free(e->name);
		safe_free(e->value);
		safe_free(e);
	}
}

#define nv_find_by_name(stru, name)	do_nv_find_by_name(stru, name, ARRAY_SIZEOF((stru)))

long do_nv_find_by_name(NameValue *table, const char *cmd, int numelements)
{
	int start = 0;
	int stop = numelements-1;
	int mid;
	while (start <= stop) {
		mid = (start+stop)/2;

		if (smycmp(cmd,table[mid].name) < 0) {
			stop = mid-1;
		}
		else if (strcmp(cmd,table[mid].name) == 0) {
			return table[mid].value;
		}
		else
			start = mid+1;
	}
	return 0;
}

#define nv_find_by_value(stru, value)	do_nv_find_by_value(stru, value, ARRAY_SIZEOF((stru)))
const char *do_nv_find_by_value(NameValue *table, long value, int numelements)
{
	int i;

	for (i=0; i < numelements; i++)
		if (table[i].value == value)
			return table[i].name;

	return NULL;
}
