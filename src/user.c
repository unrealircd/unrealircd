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

/* s_user.c 2.74 2/8/94 (C) 1988 University of Oulu, Computing Center and Jarkko Oikarinen */

#include "unrealircd.h"

int dontspread = 0;
static char buf[BUFSIZE];

MODVAR int labeled_response_inhibit = 0;

/** Set to 1 if an UTF8 incompatible nick character set is in use */
MODVAR int non_utf8_nick_chars_in_use = 0;

void iNAH_host(Client *client, char *host)
{
	if (!client->user)
		return;

	userhost_save_current(client);

	safe_strdup(client->user->virthost, host);
	if (MyConnect(client))
		sendto_server(&me, 0, 0, NULL, ":%s SETHOST :%s", client->name, client->user->virthost);
	client->umodes |= UMODE_SETHOST;

	userhost_changed(client);

	sendnumeric(client, RPL_HOSTHIDDEN, client->user->virthost);
}

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

/*
** hunt_server
**
**	Do the basic thing in delivering the message (command)
**	across the relays to the specific server (server) for
**	actions.
**
**	Note:	The command is a format string and *MUST* be
**		of prefixed style (e.g. ":%s COMMAND %s ...").
**		Command can have only max 8 parameters.
**
**	server	parv[server] is the parameter identifying the
**		target server.
**
**	*WARNING*
**		parv[server] is replaced with the pointer to the
**		real servername from the matched client (I'm lazy
**		now --msa).
**
**	returns: (see #defines)
**
** Rewritten by Syzop / Oct 2015. This function was rather
** complex and no longer understandable. It also was responsible
** for mysterious issues and crashes. Hence rewritten.
*/
int hunt_server(Client *client, MessageTag *mtags, char *command, int server, int parc, char *parv[])
{
	Client *acptr;
	char *saved;

	/* This would be strange and bad. Previous version assumed "it's for me". Hmm.. okay. */
	if (parc <= server || BadPtr(parv[server]))
		return HUNTED_ISME;

	acptr = find_client(parv[server], NULL);

	/* find_client() may find a variety of clients. Only servers/persons please, no 'unknowns'. */
	if (acptr && MyConnect(acptr) && !IsMe(acptr) && !IsUser(acptr) && !IsServer(acptr))
		acptr = NULL;

	if (!acptr)
	{
		sendnumeric(client, ERR_NOSUCHSERVER, parv[server]);
		return HUNTED_NOSUCH;
	}
	
	if (IsMe(acptr) || MyUser(acptr))
		return HUNTED_ISME;

	/* Never send the message back from where it came from */
	if (acptr->direction == client->direction)
	{
		sendnumeric(client, ERR_NOSUCHSERVER, parv[server]);
		return HUNTED_NOSUCH;
	}

	/* Replace "server" part with actual servername (eg: 'User' -> 'x.y.net')
	 * Ugly. Previous version didn't even restore the state, now we do.
	 */
	saved = parv[server];
	parv[server] = acptr->name;

	sendto_one(acptr, mtags, command, client->name,
	    parv[1], parv[2], parv[3], parv[4],
	    parv[5], parv[6], parv[7], parv[8]);

	parv[server] = saved;

	return HUNTED_PASS;
}

/** Convert a target pointer to an 8 bit hash, used for target limiting. */
unsigned char hash_target(void *target)
{
	unsigned long long v = (unsigned long long)target;
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

	if (ValidatePermissionsForPath("immune:target-limit",client,NULL,NULL,NULL))
		return 0;

	if (client->local->targets[0] == hash)
		return 0;

	for (i = 1; i < iConf.max_concurrent_conversations_users; i++)
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

		sendnumeric(client, ERR_TARGETTOOFAST, name, client->local->nexttarget - TStime());

		return 1;
	}

	/* If not set yet or in the very past, then adjust it.
	 * This is so client->local->nexttarget=0 will become client->local->nexttarget=currenttime-...
	 */
	if (TStime() > client->local->nexttarget +
	    (iConf.max_concurrent_conversations_users * iConf.max_concurrent_conversations_new_user_every))
	{
		client->local->nexttarget = TStime() - ((iConf.max_concurrent_conversations_users-1) * iConf.max_concurrent_conversations_new_user_every);
	}

	client->local->nexttarget += iConf.max_concurrent_conversations_new_user_every;

	/* Add the new target (first move the rest, then add us at position 0 */
	memmove(&client->local->targets[1], &client->local->targets[0], iConf.max_concurrent_conversations_users - 1);
	client->local->targets[0] = hash;

	return 0;
}

/*
** canonize
**
** reduce a string of duplicate list entries to contain only the unique
** items.  Unavoidably O(n^2).
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
			(void)strcpy(cp, s);
			if (p)
				cp += (p - s);
		}
		else if (p2)
			p2[-1] = ',';
	}
	return cbuf;
}

/*
** get_mode_str
** by vmlinuz
** returns an ascii string of modes
*/
char *get_sno_str(Client *client) {
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

char *get_mode_str(Client *client)
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


char *get_modestr(long umodes)
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

char *get_snostr(long sno) {
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


void set_snomask(Client *client, char *snomask) {
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
 * Originally by avalon.
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
