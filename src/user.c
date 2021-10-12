/*
 *   Unreal Internet Relay Chat Daemon, src/user.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers.
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

/** @file
 * @brief User-related functions
 */

/* s_user.c 2.74 2/8/94 (C) 1988 University of Oulu, Computing Center and Jarkko Oikarinen */

#include "unrealircd.h"

MODVAR int dontspread = 0;
static char buf[BUFSIZE];

/** Inhibit labeled/response reply. This means it will result in an empty ACK
 *  because we cannot handle the command via labeled-reponse. Rare, but
 *  possible in for example /TRACE which multiple servers handle and which
 *  has no clear end.
 */
MODVAR int labeled_response_inhibit = 0;

/** Force a labeled/response reply (of course, only if a label is present etc.).
 * This is used in case the "a remote server is handling the request" was
 * incorrect and there were 0 responses. This is the case for PRIVMSG.
 * It will force an empty ACK.
 * No, this cannot be merged with the other one. Also, the other one
 * (labeled_response_inhibit) has priority over this one (labeled_response_force).
 */
MODVAR int labeled_response_force = 0;

/** Inhibit labeled/response END. Only used in /LIST.
 */
MODVAR int labeled_response_inhibit_end = 0;

/** Set to 1 if an UTF8 incompatible nick character set is in use */
MODVAR int non_utf8_nick_chars_in_use = 0;

/** Set a new vhost on the user
 * @param client	The client (user)
 * @param host		The new vhost
 */
void iNAH_host(Client *client, char *host)
{
	if (!client->user)
		return;

	userhost_save_current(client);

	safe_strdup(client->user->virthost, host);
	if (MyConnect(client))
		sendto_server(NULL, 0, 0, NULL, ":%s SETHOST :%s", client->id, client->user->virthost);
	client->umodes |= UMODE_SETHOST;

	userhost_changed(client);

	sendnumeric(client, RPL_HOSTHIDDEN, client->user->virthost);
}

/** Convert a user mode string to a bitmask - only used by config.
 * @param umode		The user mode string
 * @returns the user mode value (long)
 */
long set_usermode(char *umode)
{
	int  newumode;
	int  what;
	char *m;
	int i;

	newumode = 0;
	what = MODE_ADD;
	for (m = umode; *m; m++)
		switch (*m)
		{
		  case '+':
			  what = MODE_ADD;
			  break;
		  case '-':
			  what = MODE_DEL;
			  break;
		  case ' ':
		  case '\n':
		  case '\r':
		  case '\t':
			  break;
		  default:
		 	 for (i = 0; i <= Usermode_highest; i++)
		 	 {
		 	 	if (!Usermode_Table[i].flag)
		 	 		continue;
		 	 	if (*m == Usermode_Table[i].flag)
		 	 	{
		 	 		if (what == MODE_ADD)
			 	 		newumode |= Usermode_Table[i].mode;
			 	 	else
			 	 		newumode &= ~Usermode_Table[i].mode;
		 	 	}
		 	 } 	  
		}

	return (newumode);
}

/** Convert a target pointer to an 8 bit hash, used for target limiting. */
unsigned char hash_target(void *target)
{
	uintptr_t v = (uintptr_t)target;
	/* ircu does >> 16 and 8 but since our sizeof(Client) is
	 * towards 512 (and hence the alignment), that bit is useless.
	 * So we do >> 17 and 9.
	 */
	return (unsigned char)((v >> 17) ^ (v >> 9));
}

/** target_limit_exceeded
 * @param client   The client.
 * @param target The target client
 * @param name   The name of the target client (used in the error message)
 * @retval Returns 1 if too many targets were addressed (do not send!), 0 if ok to send.
 */
int target_limit_exceeded(Client *client, void *target, const char *name)
{
	u_char hash = hash_target(target);
	int i;
	int max_concurrent_conversations_users, max_concurrent_conversations_new_user_every;
	FloodSettings *settings;

	if (ValidatePermissionsForPath("immune:max-concurrent-conversations",client,NULL,NULL,NULL))
		return 0;

	if (client->local->targets[0] == hash)
		return 0;

	settings = get_floodsettings_for_user(client, FLD_CONVERSATIONS);
	max_concurrent_conversations_users = settings->limit[FLD_CONVERSATIONS];
	max_concurrent_conversations_new_user_every = settings->period[FLD_CONVERSATIONS];

	if (max_concurrent_conversations_users <= 0)
		return 0; /* unlimited */

	/* Shouldn't be needed, but better check here than access out-of-bounds memory */
	if (max_concurrent_conversations_users > MAXCCUSERS)
		max_concurrent_conversations_users = MAXCCUSERS;

	for (i = 1; i < max_concurrent_conversations_users; i++)
	{
		if (client->local->targets[i] == hash)
		{
			/* Move this target hash to the first position */
			memmove(&client->local->targets[1], &client->local->targets[0], i);
			client->local->targets[0] = hash;
			return 0;
		}
	}

	if (TStime() < client->local->nexttarget)
	{
		/* Target limit reached */
		client->local->nexttarget += 2; /* punish them some more */
		client->local->since += 2; /* lag them up as well */

		flood_limit_exceeded_log(client, "max-concurrent-conversations");
		sendnumeric(client, ERR_TARGETTOOFAST, name, client->local->nexttarget - TStime());

		return 1;
	}

	/* If not set yet or in the very past, then adjust it.
	 * This is so client->local->nexttarget=0 will become client->local->nexttarget=currenttime-...
	 */
	if (TStime() > client->local->nexttarget +
	    (max_concurrent_conversations_users * max_concurrent_conversations_new_user_every))
	{
		client->local->nexttarget = TStime() - ((max_concurrent_conversations_users-1) * max_concurrent_conversations_new_user_every);
	}

	client->local->nexttarget += max_concurrent_conversations_new_user_every;

	/* Add the new target (first move the rest, then add us at position 0 */
	memmove(&client->local->targets[1], &client->local->targets[0], max_concurrent_conversations_users - 1);
	client->local->targets[0] = hash;

	return 0;
}

/** De-duplicate a string of "x,x,y,z" to "x,y,z"
 * @param buffer	Input string
 * @returns The new de-duplicated buffer (temporary storage, only valid until next canonize call)
 */
char *canonize(char *buffer)
{
	static char cbuf[2048];
	char *s, *t, *cp = cbuf;
	int  l = 0;
	char *p = NULL, *p2;

	*cp = '\0';

	if (!buffer)
		return NULL;

	/* Ohh.. so lazy. But then again, this should never happen with a 2K buffer anyway. */
	if (strlen(buffer) >= sizeof(cbuf))
		buffer[sizeof(cbuf)-1] = '\0';

	for (s = strtoken(&p, buffer, ","); s; s = strtoken(&p, NULL, ","))
	{
		if (l)
		{
			for (p2 = NULL, t = strtoken(&p2, cbuf, ","); t;
			    t = strtoken(&p2, NULL, ","))
				if (!mycmp(s, t))
					break;
				else if (p2)
					p2[-1] = ',';
		}
		else
			t = NULL;
		if (!t)
		{
			if (l)
				*(cp - 1) = ',';
			else
				l = 1;
			strcpy(cp, s);
			if (p)
				cp += (p - s);
		}
		else if (p2)
			p2[-1] = ',';
	}
	return cbuf;
}

/** Get snomasks as a string.
 * @param client	The client
 * @returns string of snomasks (temporary storage)
 */
char *get_snomask_string(Client *client)
{
	int i;
	char *m;

	m = buf;

	*m++ = '+';
	for (i = 0; i <= Snomask_highest && (m - buf < BUFSIZE - 4); i++)
		if (Snomask_Table[i].flag && client->user->snomask & Snomask_Table[i].mode)
			*m++ = Snomask_Table[i].flag;
	*m = 0;
	return buf;
}

/** Get user modes as a string.
 * @param client	The client
 * @returns string of user modes (temporary storage)
 */
char *get_usermode_string(Client *client)
{
	int  i;
	char *m;

	m = buf;
	*m++ = '+';
	for (i = 0; (i <= Usermode_highest) && (m - buf < BUFSIZE - 4); i++)
		if (Usermode_Table[i].flag && (client->umodes & Usermode_Table[i].mode))
			*m++ = Usermode_Table[i].flag;
	*m = '\0';
	return buf;
}


/** Get user modes as a string - this one does not work on 'client' but directly on 'umodes'.
 * @param umodes	The user modes that are set
 * @returns string of user modes (temporary storage)
 */
char *get_usermode_string_raw(long umodes)
{
	int  i;
	char *m;

	m = buf;
	*m++ = '+';
	for (i = 0; (i <= Usermode_highest) && (m - buf < BUFSIZE - 4); i++)
		
		if (Usermode_Table[i].flag && (umodes & Usermode_Table[i].mode))
			*m++ = Usermode_Table[i].flag;
	*m = '\0';
	return buf;
}

/** Get snomasks as a string - this one does not work on 'client' but directly on 'sno'.
 * @param sno	The snomasks that are set
 * @returns string of snomasks (temporary storage)
 */
char *get_snomask_string_raw(long sno)
{
	int i;
	char *m;

	m = buf;

	*m++ = '+';
	for (i = 0; i <= Snomask_highest && (m - buf < BUFSIZE - 4); i++)
		if (Snomask_Table[i].flag && sno & Snomask_Table[i].mode)
			*m++ = Snomask_Table[i].flag;
	*m = 0;
	return buf;
}

/** Set a new snomask on the user.
 * The user is not informed of the change by this function.
 * @param client	The client
 * @param snomask	The snomask to add or delete (eg: "+k-c")
 */
void set_snomask(Client *client, char *snomask)
{
	int what = MODE_ADD; /* keep this an int. -- Syzop */
	char *p;
	int i;
	if (snomask == NULL) {
		client->user->snomask = 0;
		return;
	}
	
	for (p = snomask; p && *p; p++) {
		switch (*p) {
			case '+':
				what = MODE_ADD;
				break;
			case '-':
				what = MODE_DEL;
				break;
			default:
		 	 for (i = 0; i <= Snomask_highest; i++)
		 	 {
		 	 	if (!Snomask_Table[i].flag)
		 	 		continue;
		 	 	if (*p == Snomask_Table[i].flag)
		 	 	{
					if (Snomask_Table[i].allowed && !Snomask_Table[i].allowed(client,what))
						continue;
		 	 		if (what == MODE_ADD)
			 	 		client->user->snomask |= Snomask_Table[i].mode;
			 	 	else
			 	 		client->user->snomask &= ~Snomask_Table[i].mode;
		 	 	}
		 	 }				
		}
	}
}

/** Build the MODE line with (modified) user modes for this user.
 * @author Originally by avalon.
 */
void build_umode_string(Client *client, long old, long sendmask, char *umode_buf)
{
	int i;
	long flag;
	char *m;
	int what = MODE_NULL;

	/*
	 * build a string in umode_buf to represent the change in the user's
	 * mode between the new (client->flag) and 'old'.
	 */
	m = umode_buf;
	*m = '\0';
	for (i = 0; i <= Usermode_highest; i++)
	{
		if (!Usermode_Table[i].flag)
			continue;
		flag = Usermode_Table[i].mode;
		if (MyUser(client) && !(flag & sendmask))
			continue;
		if ((flag & old) && !(client->umodes & flag))
		{
			if (what == MODE_DEL)
				*m++ = Usermode_Table[i].flag;
			else
			{
				what = MODE_DEL;
				*m++ = '-';
				*m++ = Usermode_Table[i].flag;
			}
		}
		else if (!(flag & old) && (client->umodes & flag))
		{
			if (what == MODE_ADD)
				*m++ = Usermode_Table[i].flag;
			else
			{
				what = MODE_ADD;
				*m++ = '+';
				*m++ = Usermode_Table[i].flag;
			}
		}
	}
	*m = '\0';
}

/** Send usermode change to other servers.
 * @param client	The client
 * @param show_to_user	Set to 1 to show the MODE change to the user
 * @param old		The old user modes set on the client
 */
void send_umode_out(Client *client, int show_to_user, long old)
{
	Client *acptr;

	build_umode_string(client, old, SEND_UMODES, buf);

	list_for_each_entry(acptr, &server_list, special_node)
	{
		if ((acptr != client) && (acptr != client->direction) && *buf)
		{
			sendto_one(acptr, NULL, ":%s UMODE2 %s",
			           client->name, buf);
		}
	}

	if (MyUser(client) && show_to_user)
	{
		build_umode_string(client, old, ALL_UMODES, buf);
		if (*buf)
			sendto_one(client, NULL, ":%s MODE %s :%s", client->name, client->name, buf);
	}
}

static MaxTarget *maxtargets = NULL; /**< For set::max-targets-per-command configuration */

static void maxtarget_add_sorted(MaxTarget *n)
{
	MaxTarget *e;

	if (!maxtargets)
	{
		maxtargets = n;
		return;
	}

	for (e = maxtargets; e; e = e->next)
	{
		if (strcmp(n->cmd, e->cmd) < 0)
		{
			/* Insert us before */
			if (e->prev)
				e->prev->next = n;
			else
				maxtargets = n; /* new head */
			n->prev = e->prev;

			n->next = e;
			e->prev = n;
			return;
		}
		if (!e->next)
		{
			/* Append us at end */
			e->next = n;
			n->prev = e;
			return;
		}
	}
}

/** Find a maxtarget structure for a cmd (internal) */
MaxTarget *findmaxtarget(char *cmd)
{
	MaxTarget *m;

	for (m = maxtargets; m; m = m->next)
		if (!strcasecmp(m->cmd, cmd))
			return m;
	return NULL;
}

/** Set a maximum targets per command restriction */
void setmaxtargets(char *cmd, int limit)
{
	MaxTarget *m = findmaxtarget(cmd);
	if (!m)
	{
		char cmdupper[64], *i, *o;
		if (strlen(cmd) > 63)
			cmd[63] = '\0';
		for (i=cmd,o=cmdupper; *i; i++)
			*o++ = toupper(*i);
		*o = '\0';
		m = safe_alloc(sizeof(MaxTarget));
		safe_strdup(m->cmd, cmdupper);
		maxtarget_add_sorted(m);
	}
	m->limit = limit;
}

/** Free all set::max-targets-per-command configuration (internal) */
void freemaxtargets(void)
{
	MaxTarget *m, *m_next;

	for (m = maxtargets; m; m = m_next)
	{
		m_next = m->next;
		safe_free(m->cmd);
		safe_free(m);
	}
	maxtargets = NULL;
}

/** Return the maximum number of targets permitted for a command */
int max_targets_for_command(char *cmd)
{
	MaxTarget *m = findmaxtarget(cmd);
	if (m)
		return m->limit;
	return 1; /* default to 1 */
}

void set_isupport_targmax(void)
{
	char buf[512], tbuf[64];
	MaxTarget *m;

	*buf = '\0';
	for (m = maxtargets; m; m = m->next)
	{
		if (m->limit == MAXTARGETS_MAX)
			snprintf(tbuf, sizeof(tbuf), "%s:", m->cmd);
		else
			snprintf(tbuf, sizeof(tbuf), "%s:%d", m->cmd, m->limit);

		if (*buf)
			strlcat(buf, ",", sizeof(buf));
		strlcat(buf, tbuf, sizeof(buf));
	}
	ISupportSet(NULL, "TARGMAX", buf);
}

/** Called between config test and config run */
void set_targmax_defaults(void)
{
	/* Free existing... */
	freemaxtargets();

	/* Set the defaults */
	setmaxtargets("PRIVMSG", 4);
	setmaxtargets("NOTICE", 1);
	setmaxtargets("TAGMSG", 1);
	setmaxtargets("NAMES", 1); // >1 is not supported
	setmaxtargets("WHOIS", 1);
	setmaxtargets("WHOWAS", 1); // >1 is not supported
	setmaxtargets("KICK", 4);
	setmaxtargets("LIST", MAXTARGETS_MAX);
	setmaxtargets("JOIN", MAXTARGETS_MAX);
	setmaxtargets("PART", MAXTARGETS_MAX);
	setmaxtargets("SAJOIN", MAXTARGETS_MAX);
	setmaxtargets("SAPART", MAXTARGETS_MAX);
	setmaxtargets("KILL", MAXTARGETS_MAX);
	setmaxtargets("DCCALLOW", MAXTARGETS_MAX);
	/* The following 3 are space-separated (and actually the previous
	 * mentioned DCCALLOW is both space-and-comma separated).
	 * It seems most IRCd's don't list space-separated targets list
	 * in TARGMAX... On the other hand, why not? It says nowhere in
	 * the TARGMAX specification that it's only for comma-separated
	 * commands. So let's be nice and consistent and inform the
	 * clients about the limits for such commands as well:
	 */
	setmaxtargets("USERHOST", MAXTARGETS_MAX); // not configurable
	setmaxtargets("USERIP", MAXTARGETS_MAX); // not configurable
	setmaxtargets("ISON", MAXTARGETS_MAX); // not configurable
	setmaxtargets("WATCH", MAXTARGETS_MAX); // not configurable
}

/** Is the user handshake finished and can register_user() be called?
 * This checks things like: do we have a NICK, USER, nospoof,
 * and any other things modules may add:
 * eg: the cap module checks if client capability negotiation
 * is in progress
 */
int is_handshake_finished(Client *client)
{
	Hook *h;
	int n;

	for (h = Hooks[HOOKTYPE_IS_HANDSHAKE_FINISHED]; h; h = h->next)
	{
		n = (*(h->func.intfunc))(client);
		if (n == 0)
			return 0; /* We can stop already */
	}

	/* I figured these can be here, in the core: */
	if (client->user && *client->user->username && client->name[0] && IsNotSpoof(client))
		return 1;

	return 0;
}

/** Should we show connection info to the user?
 * This depends on the set::show-connect-info setting but also
 * on various other properties, such as serversonly ports,
 * websocket, etc.
 * If someone needs it, then we can also call a hook here. Just tell us.
 */
int should_show_connect_info(Client *client)
{
	if (SHOWCONNECTINFO &&
	    !client->serv &&
	    !IsServersOnlyListener(client->local->listener) &&
	    !client->local->listener->websocket_options)
	{
		return 1;
	}
	return 0;
}

/* (helper function for uid_get) */
static char uid_int_to_char(int v)
{
	if (v < 10)
		return '0'+v;
	else
		return 'A'+v-10;
}

/** Acquire a new unique UID */
const char *uid_get(void)
{
	Client *acptr;
	static char uid[IDLEN];
	static int uidcounter = 0;

	uidcounter++;
	if (uidcounter == 36*36)
		uidcounter = 0;

	do
	{
		snprintf(uid, sizeof(uid), "%s%c%c%c%c%c%c",
			me.id,
			uid_int_to_char(getrandom8() % 36),
			uid_int_to_char(getrandom8() % 36),
			uid_int_to_char(getrandom8() % 36),
			uid_int_to_char(getrandom8() % 36),
			uid_int_to_char(uidcounter / 36),
			uid_int_to_char(uidcounter % 36));
		acptr = find_client(uid, NULL);
	} while (acptr);

	return uid;
}

/** Get cloaked host for user */
char *getcloak(Client *client)
{
	if (!*client->user->cloakedhost)
	{
		/* need to calculate (first-time) */
		make_cloakedhost(client, client->user->realhost, client->user->cloakedhost, sizeof(client->user->cloakedhost));
	}

	return client->user->cloakedhost;
}

/** Calculate the cloaked host for a client.
 * @param client	The client
 * @param curr		The real host or real IP
 * @param buf		Buffer to store the new cloaked host in
 * @param buflen	Length of the buffer (should be HOSTLEN+1)
 */
void make_cloakedhost(Client *client, char *curr, char *buf, size_t buflen)
{
	char host[256], *mask, *p, *q;

	/* Convert host to lowercase and cut off at 255 bytes just to be sure */
	for (p = curr, q = host; *p && (q < host+sizeof(host)-1); p++, q++)
		*q =  tolower(*p);
	*q = '\0';

	/* Call the cloaking layer */
	if (RCallbacks[CALLBACKTYPE_CLOAK_EX] != NULL)
		mask = RCallbacks[CALLBACKTYPE_CLOAK_EX]->func.pcharfunc(client, host);
	else if (RCallbacks[CALLBACKTYPE_CLOAK] != NULL)
		mask = RCallbacks[CALLBACKTYPE_CLOAK]->func.pcharfunc(host);
	else
		mask = curr;

	strlcpy(buf, mask, buflen);
}

/** Called after a user is logged in (or out) of a services account */
void user_account_login(MessageTag *recv_mtags, Client *client)
{
	if (MyConnect(client))
	{
		find_shun(client);
		if (find_tkline_match(client, 0) && IsDead(client))
			return;
	}
	RunHook2(HOOKTYPE_ACCOUNT_LOGIN, client, recv_mtags);
}

/** Should we hide the idle time of 'target' to user 'client'?
 * This depends on the set::hide-idle-time policy.
 */
int hide_idle_time(Client *client, Client *target)
{
	/* First of all, IRCOps bypass the restriction */
	if (IsOper(client))
		return 0;

	/* Other than that, it depends on the settings: */
	switch (iConf.hide_idle_time)
	{
		case HIDE_IDLE_TIME_NEVER:
			return 0;
		case HIDE_IDLE_TIME_ALWAYS:
			return 1;
		case HIDE_IDLE_TIME_USERMODE:
		case HIDE_IDLE_TIME_OPER_USERMODE:
			if (target->umodes & UMODE_HIDLE)
				return 1;
			return 0;
		default:
			return 0;
	}
}

/** Check if the name of the security-group contains only valid characters.
 * @param name	The name of the group
 * @returns 1 if name is valid, 0 if not (eg: illegal characters)
 */
int security_group_valid_name(char *name)
{
	char *p;
	if (strlen(name) > SECURITYGROUPLEN)
		return 0; /* Too long */
	for (p = name; *p; p++)
	{
		if (!isalnum(*p) && !strchr("_-", *p))
			return 0; /* Character not allowed */
	}
	return 1;
}

/** Find a security-group.
 * @param name	The name of the security group
 * @returns A SecurityGroup struct, or NULL if not found.
 */
SecurityGroup *find_security_group(char *name)
{
	SecurityGroup *s;
	for (s = securitygroups; s; s = s->next)
		if (!strcasecmp(name, s->name))
			return s;
	return NULL;
}

/** Checks if a security-group exists.
 * This function takes the 'unknown-users' magic group into account as well.
 * @param name	The name of the security group
 * @returns 1 if it exists, 0 if not
 */
int security_group_exists(char *name)
{
	if (!strcmp(name, "unknown-users") || find_security_group(name))
		return 1;
	return 0;
}

/** Add a new security-group and add it to the list, but search for existing one first.
 * @param name	The name of the security group
 * @returns A SecurityGroup struct (already added to the 'securitygroups' linked list)
 */
SecurityGroup *add_security_group(char *name, int priority)
{
	SecurityGroup *s = find_security_group(name);

	/* Existing? */
	if (s)
		return s;

	/* Otherwise, create a new entry */
	s = safe_alloc(sizeof(SecurityGroup));
	strlcpy(s->name, name, sizeof(s->name));
	s->priority = priority;
	AddListItemPrio(s, securitygroups, priority);
	return s;
}

/** Free a SecurityGroup struct */
void free_security_group(SecurityGroup *s)
{
	/* atm there is nothing else to free,
	 * but who knows this may change in the future
	 */
	safe_free(s);
}

/** Initialize the default security-group blocks */
void set_security_group_defaults(void)
{
	SecurityGroup *s, *s_next;

	/* First free all security groups */
	for (s = securitygroups; s; s = s_next)
	{
		s_next = s->next;
		free_security_group(s);
	}
	securitygroups = NULL;

	/* Default group: known-users */
	s = add_security_group("known-users", 100);
	s->identified = 1;
	s->reputation_score = 25;
	s->webirc = 0;

	/* Default group: tls-and-known-users */
	s = add_security_group("tls-and-known-users", 200);
	s->identified = 1;
	s->reputation_score = 25;
	s->webirc = 0;
	s->tls = 1;

	/* Default group: tls-users */
	s = add_security_group("tls-users", 300);
	s->tls = 1;
}

/** Returns 1 if the user is OK as far as the security-group is concerned.
 * @param client	The client to check
 * @param s		The security-group to check against
 * @retval 1 if user is allowed by security-group, 0 if not.
 */
int user_allowed_by_security_group(Client *client, SecurityGroup *s)
{
	if (s->identified && IsLoggedIn(client))
		return 1;
	if (s->webirc && moddata_client_get(client, "webirc"))
		return 1;
	if (s->reputation_score && (GetReputation(client) >= s->reputation_score))
		return 1;
	if (s->tls && (IsSecureConnect(client) || IsSecure(client)))
		return 1;
	return 0;
}

/** Returns 1 if the user is OK as far as the security-group is concerned - "by name" version.
 * @param client	The client to check
 * @param secgroupname	The name of the security-group to check against
 * @retval 1 if user is allowed by security-group, 0 if not.
 */
int user_allowed_by_security_group_name(Client *client, char *secgroupname)
{
	SecurityGroup *s;

	/* Handle the magical 'unknown-users' case. */
	if (!strcmp(secgroupname, "unknown-users"))
	{
		/* This is simply the inverse of 'known-users' */
		s = find_security_group("known-users");
		if (!s)
			return 0; /* that's weird!? pretty impossible. */
		return !user_allowed_by_security_group(client, s);
	}

	/* Find the group and evaluate it */
	s = find_security_group(secgroupname);
	if (!s)
		return 0; /* security group not found: no match */
	return user_allowed_by_security_group(client, s);
}

/** Return extended information about user for the "Client connecting" line.
 * @returns A string such as "[secure] [reputation: 5]", never returns NULL.
 */
char *get_connect_extinfo(Client *client)
{
	static char retbuf[512];
	char tmp[512];
	NameValuePrioList *list = NULL, *e;

	/* From modules... */
	RunHook2(HOOKTYPE_CONNECT_EXTINFO, client, &list);

	/* And some built-in: */

	/* "class": this should be first */
	if (MyUser(client) && client->local->class)
		add_nvplist(&list, -100000, "class", client->local->class->name);

	/* "secure": SSL/TLS */
	if (MyUser(client) && IsSecure(client))
		add_nvplist(&list, -1000, "secure", tls_get_cipher(client->local->ssl));
	else if (!MyUser(client) && IsSecureConnect(client))
		add_nvplist(&list, -1000, "secure", NULL);

	/* services account? */
	if (IsLoggedIn(client))
		add_nvplist(&list, -500, "account", client->user->svid);

	*retbuf = '\0';
	for (e = list; e; e = e->next)
	{
		if (e->value)
			snprintf(tmp, sizeof(tmp), "[%s: %s] ", e->name, e->value);
		else
			snprintf(tmp, sizeof(tmp), "[%s] ", e->name);
		strlcat(retbuf, tmp, sizeof(retbuf));
	}
	/* Cut off last space (unless empty string) */
	if (*buf)
		buf[strlen(buf)-1] = '\0';

	/* Free the list, as it was only used to build retbuf */
	free_nvplist(list);

	return retbuf;
}

/** Log a message that flood protection kicked in for the client.
 * This sends to the +f snomask at the moment.
 * @param client	The client to check flood for (local user)
 * @param opt		The flood option (eg FLD_AWAY)
 */
void flood_limit_exceeded_log(Client *client, char *floodname)
{
	char buf[1024];

	snprintf(buf, sizeof(buf), "Flood blocked (%s) from %s!%s@%s [%s]",
		floodname,
		client->name,
		client->user->username,
		client->user->realhost,
		GetIP(client));
	ircd_log(LOG_FLOOD, "%s", buf);
	sendto_snomask_global(SNO_FLOOD, "%s", buf);
}

/** Is the flood limit exceeded for an option? eg for away-flood.
 * @param client	The client to check flood for (local user)
 * @param opt		The flood option (eg FLD_AWAY)
 * @note This increments the flood counter as well.
 * @returns 1 if exceeded, 0 if not.
 */
int flood_limit_exceeded(Client *client, FloodOption opt)
{
	FloodSettings *f;

	if (!MyUser(client))
		return 0;

	f = get_floodsettings_for_user(client, opt);
	if (f->limit[opt] <= 0)
		return 0; /* No limit set or unlimited */

	ircd_log(LOG_ERROR, "Checking flood_limit_exceeded() for '%s', type %d with max %d:%ld...",
		client->name, (int)opt, (int)f->limit[opt], (long)f->period[opt]);

	/* Ok, let's do the flood check */
	if ((client->local->flood[opt].t + f->period[opt]) <= timeofday)
	{
		/* Time exceeded, reset */
		client->local->flood[opt].count = 0;
		client->local->flood[opt].t = timeofday;
	}
	if (client->local->flood[opt].count <= f->limit[opt])
		client->local->flood[opt].count++;
	if (client->local->flood[opt].count > f->limit[opt])
	{
		flood_limit_exceeded_log(client, floodoption_names[opt]);
		return 1; /* Flood limit hit! */
	}

	return 0;
}

/** Get the appropriate anti-flood settings block for this user.
 * @param client	The client, should be locally connected.
 * @param opt		The flood option we are interested in
 * @returns The FloodSettings for this user, never returns NULL.
 */
FloodSettings *get_floodsettings_for_user(Client *client, FloodOption opt)
{
	SecurityGroup *s;
	FloodSettings *f;

	/* Go through all security groups by order of priority
	 * (eg: first "known-users", then "unknown-users").
	 * For each of these:
	 * - Check if a set::anti-flood::xxxx block exists for this group
	 * - Check if the limit is non-zero (eg there is any limit set)
	 * If any of these are false then we continue with next block
	 * that matches.
	 */

	// XXX: alternatively, instead of this double loop,
	//      do a post-conf thing and sort iConf.floodsettings
	//      according to the security-group { } order.
	for (s = securitygroups; s; s = s->next)
	{
		if (user_allowed_by_security_group(client, s) &&
		    ((f = find_floodsettings_block(s->name))) &&
		    f->limit[opt])
		{
			return f;
		}
	}

	/* Return default settings block (which may have a zero limit set) */
	f = find_floodsettings_block("unknown-users");
	if (!f)
		abort(); /* impossible */

	return f;
}

MODVAR char *floodoption_names[] = {
	"nick-flood",
	"join-flood",
	"away-flood",
	"invite-flood",
	"knock-flood",
	"max-concurrent-conversations",
	NULL
};
