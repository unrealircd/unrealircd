/*
 *   Unreal Internet Relay Chat Daemon, src/s_user.c
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

#include "macros.h"
#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"
#include "proto.h"

#ifdef _WIN32
#include "version.h"
#endif

void send_umode_out(aClient *, aClient *, long);
void send_umode(aClient *, aClient *, long, long, char *);
void set_snomask(aClient *, char *);
void create_snomask(aClient *, anUser *, char *);
extern int short_motd(aClient *sptr);
extern aChannel *get_channel(aClient *cptr, char *chname, int flag);
/* static  Link    *is_banned(aClient *, aChannel *); */
int  dontspread = 0;
extern char *me_hash;
extern char backupbuf[];
static char buf[BUFSIZE];

void iNAH_host(aClient *sptr, char *host)
{
	if (!sptr->user)
		return;

	userhost_save_current(sptr);

	if (sptr->user->virthost)
	{
		MyFree(sptr->user->virthost);
		sptr->user->virthost = NULL;
	}
	sptr->user->virthost = strdup(host);
	if (MyConnect(sptr))
		sendto_server(&me, 0, 0, NULL, ":%s SETHOST :%s", sptr->name, sptr->user->virthost);
	sptr->umodes |= UMODE_SETHOST;

	userhost_changed(sptr);

	sendnumeric(sptr, RPL_HOSTHIDDEN, sptr->user->virthost);
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
** m_functions execute protocol messages on this server:
**
**	cptr	is always NON-NULL, pointing to a *LOCAL* client
**		structure (with an open socket connected!). This
**		identifies the physical socket where the message
**		originated (or which caused the m_function to be
**		executed--some m_functions may call others...).
**
**	sptr	is the source of the message, defined by the
**		prefix part of the message if present. If not
**		or prefix not found, then sptr==cptr.
**
**		(!IsServer(cptr)) => (cptr == sptr), because
**		prefixes are taken *only* from servers...
**
**		(IsServer(cptr))
**			(sptr == cptr) => the message didn't
**			have the prefix.
**
**			(sptr != cptr && IsServer(sptr) means
**			the prefix specified servername. (?)
**
**			(sptr != cptr && !IsServer(sptr) means
**			that message originated from a remote
**			user (not local).
**
**		combining
**
**		(!IsServer(sptr)) means that, sptr can safely
**		taken as defining the target structure of the
**		message in this server.
**
**	*Always* true (if 'parse' and others are working correct):
**
**	1)	sptr->from == cptr  (note: cptr->from == cptr)
**
**	2)	MyConnect(sptr) <=> sptr == cptr (e.g. sptr
**		*cannot* be a local connection, unless it's
**		actually cptr!). [MyConnect(x) should probably
**		be defined as (x == x->from) --msa ]
**
**	parc	number of variable parameter strings (if zero,
**		parv is allowed to be NULL)
**
**	parv	a NULL terminated list of parameter pointers,
**
**			parv[1]...parv[parc-1]
**				pointers to additional parameters
**			parv[parc] == NULL, *always*
**
**		note:	it is guaranteed that parv[1]..parv[parc-1] are all
**			non-NULL pointers.
*/

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
int hunt_server(aClient *cptr, aClient *sptr, MessageTag *mtags, char *command, int server, int parc, char *parv[])
{
	aClient *acptr;
	char *saved;

	/* This would be strange and bad. Previous version assumed "it's for me". Hmm.. okay. */
	if (parc <= server || BadPtr(parv[server]))
		return HUNTED_ISME;

	acptr = find_client(parv[server], NULL);

	/* find_client() may find a variety of clients. Only servers/persons please, no 'unknowns'. */
	if (acptr && MyConnect(acptr) && !IsMe(acptr) && !IsPerson(acptr) && !IsServer(acptr))
		acptr = NULL;

	if (!acptr)
	{
		sendnumeric(sptr, ERR_NOSUCHSERVER, parv[server]);
		return HUNTED_NOSUCH;
	}
	
	if (IsMe(acptr) || MyClient(acptr))
		return HUNTED_ISME;

	/* Never send the message back from where it came from */
	if (acptr->from == sptr->from)
	{
		sendnumeric(sptr, ERR_NOSUCHSERVER, parv[server]);
		return HUNTED_NOSUCH;
	}

	/* Replace "server" part with actual servername (eg: 'User' -> 'x.y.net')
	 * Ugly. Previous version didn't even restore the state, now we do.
	 */
	saved = parv[server];
	parv[server] = acptr->name;

	if (mtags)
	{
		char *str = mtags_to_string(mtags, acptr);
		char newcommand[512];
		/* Update the format string */
		snprintf(newcommand, sizeof(newcommand), "@%%s %s", command);
		/* And do the sendto_one() */
		sendto_one(acptr, mtags, newcommand, str, sptr->name,
		    parv[1], parv[2], parv[3], parv[4],
		    parv[5], parv[6], parv[7], parv[8]);
	} else {
		sendto_one(acptr, mtags, command, sptr->name,
		    parv[1], parv[2], parv[3], parv[4],
		    parv[5], parv[6], parv[7], parv[8]);
	}

	parv[server] = saved;

	return HUNTED_PASS;
}

/** Convert a target pointer to an 8 bit hash, used for target limiting. */
unsigned char hash_target(void *target)
{
	unsigned int v = (unsigned long)target;
	/* ircu does >> 16 and 8 but since our sizeof(aClient) is
	 * towards 512 (and hence the alignment), that bit is useless.
	 * So we do >> 17 and 9.
	 */
	return (unsigned char)((v >> 17) ^ (v >> 9));
}

/** check_for_target_limit
 * @param sptr   The client.
 * @param target The target client
 * @param name   The name of the target client (used in the error message)
 * @retval Returns 1 if too many targets were addressed (do not send!), 0 if ok to send.
 */
int check_for_target_limit(aClient *sptr, void *target, const char *name)
{
	u_char *p;
	u_char hash = hash_target(target);
	int i;

	if (ValidatePermissionsForPath("immune:target-limit",sptr,NULL,NULL,NULL))
		return 0;
	if (sptr->local->targets[0] == hash)
		return 0;

	for (i = 1; i < iConf.max_concurrent_conversations_users; i++)
	{
		if (sptr->local->targets[i] == hash)
		{
			/* Move this target hash to the first position */
			memmove(&sptr->local->targets[1], &sptr->local->targets[0], i);
			sptr->local->targets[0] = hash;
			return 0;
		}
	}

	if (TStime() < sptr->local->nexttarget)
	{
		/* Target limit reached */
		sptr->local->nexttarget += 2; /* punish them some more */
		sptr->local->since += 2; /* lag them up as well */

		sendnumeric(sptr, ERR_TARGETTOOFAST,
			name, sptr->local->nexttarget - TStime());

		return 1;
	}

	/* If not set yet or in the very past, then adjust it.
	 * This is so sptr->local->nexttarget=0 will become sptr->local->nexttarget=currenttime-...
	 */
	if (TStime() > sptr->local->nexttarget +
	    (iConf.max_concurrent_conversations_users * iConf.max_concurrent_conversations_new_user_every))
	{
		sptr->local->nexttarget = TStime() - ((iConf.max_concurrent_conversations_users-1) * iConf.max_concurrent_conversations_new_user_every);
	}

	sptr->local->nexttarget += iConf.max_concurrent_conversations_new_user_every;

	/* Add the new target (first move the rest, then add us at position 0 */
	memmove(&sptr->local->targets[1], &sptr->local->targets[0], iConf.max_concurrent_conversations_users - 1);
	sptr->local->targets[0] = hash;

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
char *get_sno_str(aClient *sptr) {
	int i;
	char *m;

	m = buf;

	*m++ = '+';
	for (i = 0; i <= Snomask_highest && (m - buf < BUFSIZE - 4); i++)
		if (Snomask_Table[i].flag && sptr->user->snomask & Snomask_Table[i].mode)
			*m++ = Snomask_Table[i].flag;
	*m = 0;
	return buf;
}

char *get_mode_str(aClient *acptr)
{
	int  i;
	char *m;

	m = buf;
	*m++ = '+';
	for (i = 0; (i <= Usermode_highest) && (m - buf < BUFSIZE - 4); i++)
		if (Usermode_Table[i].flag && (acptr->umodes & Usermode_Table[i].mode))
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


void set_snomask(aClient *sptr, char *snomask) {
	int what = MODE_ADD; /* keep this an int. -- Syzop */
	char *p;
	int i;
	if (snomask == NULL) {
		sptr->user->snomask = 0;
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
					if (Snomask_Table[i].allowed && !Snomask_Table[i].allowed(sptr,what))
						continue;
		 	 		if (what == MODE_ADD)
			 	 		sptr->user->snomask |= Snomask_Table[i].mode;
			 	 	else
			 	 		sptr->user->snomask &= ~Snomask_Table[i].mode;
		 	 	}
		 	 }				
		}
	}
}

void create_snomask(aClient *sptr, anUser *user, char *snomask) {
	int what = MODE_ADD; /* keep this an int. -- Syzop */
	char *p;
	int i;
	if (snomask == NULL) {
		user->snomask = 0;
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
					if (Snomask_Table[i].allowed && !Snomask_Table[i].allowed(sptr,what))
						continue;
		 	 		if (what == MODE_ADD)
			 	 		user->snomask |= Snomask_Table[i].mode;
			 	 	else
			 	 		user->snomask &= ~Snomask_Table[i].mode;
		 	 	}
		 	 }				
		}
	}
}

/*
 * send the MODE string for user (user) to connection cptr
 * -avalon
 */
void send_umode(aClient *cptr, aClient *sptr, long old, long sendmask, char *umode_buf)
{
	int i;
	long flag;
	char *m;
	int  what = MODE_NULL;

	/*
	 * build a string in umode_buf to represent the change in the user's
	 * mode between the new (sptr->flag) and 'old'.
	 */
	m = umode_buf;
	*m = '\0';
	for (i = 0; i <= Usermode_highest; i++)
	{
		if (!Usermode_Table[i].flag)
			continue;
		flag = Usermode_Table[i].mode;
		if (MyClient(sptr) && !(flag & sendmask))
			continue;
		if ((flag & old) && !(sptr->umodes & flag))
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
		else if (!(flag & old) && (sptr->umodes & flag))
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
	if (*umode_buf && cptr)
		sendto_one(cptr, NULL, ":%s MODE %s :%s", sptr->name,
		    sptr->name, umode_buf);
}

/*
 * added Sat Jul 25 07:30:42 EST 1992
 */
void send_umode_out(aClient *cptr, aClient *sptr, long old)
{
	aClient *acptr;

	send_umode(NULL, sptr, old, SEND_UMODES, buf);

	list_for_each_entry(acptr, &server_list, special_node)
	{
		if ((acptr != cptr) && (acptr != sptr) && *buf)
		{
			sendto_one(acptr, NULL, ":%s UMODE2 %s",
				    sptr->name,
				    buf);
		}
	}

	if (cptr && MyClient(cptr))
		send_umode(cptr, sptr, old, ALL_UMODES, buf);
}

int  del_silence(aClient *sptr, char *mask)
{
	Link **lp;
	Link *tmp;

	for (lp = &(sptr->user->silence); *lp; lp = &((*lp)->next))
		if (mycmp(mask, (*lp)->value.cp) == 0)
		{
			tmp = *lp;
			*lp = tmp->next;
			MyFree(tmp->value.cp);
			free_link(tmp);
			return 0;
		}
	return -1;
}

int add_silence(aClient *sptr, char *mask, int senderr)
{
	Link *lp;
	int  cnt = 0;

	for (lp = sptr->user->silence; lp; lp = lp->next)
	{
		if (MyClient(sptr))
			if ((strlen(lp->value.cp) > MAXSILELENGTH) || (++cnt >= SILENCE_LIMIT))
			{
				if (senderr)
					sendnumeric(sptr, ERR_SILELISTFULL, mask);
				return -1;
			}
			else
			{
				if (!match(lp->value.cp, mask))
					return -1;
			}
		else if (!mycmp(lp->value.cp, mask))
			return -1;
	}
	lp = make_link();
	bzero((char *)lp, sizeof(Link));
	lp->next = sptr->user->silence;
	lp->value.cp = strdup(mask);
	sptr->user->silence = lp;
	return 0;
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
		m = MyMallocEx(sizeof(MaxTarget));
		m->cmd = strdup(cmdupper);
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
		safefree(m->cmd);
		MyFree(m);
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
	IsupportSet(NULL, "TARGMAX", buf);
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
int is_handshake_finished(aClient *sptr)
{
	Hook *h;
	int n;

	for (h = Hooks[HOOKTYPE_IS_HANDSHAKE_FINISHED]; h; h = h->next)
	{
		n = (*(h->func.intfunc))(sptr);
		if (n == 0)
			return 0; /* We can stop already */
	}

	/* I figured these can be here, in the core: */
	if (sptr->user && *sptr->user->username && sptr->name[0] && IsNotSpoof(sptr))
		return 1;

	return 0;
}
