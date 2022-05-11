/* cmd_whox.c / WHOX.
 * based on code from charybdis and ircu.
 * was originally made for tircd and modified to work with u4.
 * - 2018 i <ircd@servx.org>
 * - 2019 Bram Matthys (Syzop) <syzop@vulnscan.org>
 * License: GPLv2 or later
 */

#include "unrealircd.h"

/* Module header */
ModuleHeader MOD_HEADER
  = {
	"whox",
	"5.0",
	"command /who",
	"UnrealIRCd Team",
	"unrealircd-6",
    };


/* Defines */
#define FIELD_CHANNEL	0x0001
#define FIELD_HOP	0x0002
#define FIELD_FLAGS	0x0004
#define FIELD_HOST	0x0008
#define FIELD_IP	0x0010
#define FIELD_IDLE	0x0020
#define FIELD_NICK	0x0040
#define FIELD_INFO	0x0080
#define FIELD_SERVER	0x0100
#define FIELD_QUERYTYPE	0x0200 /* cookie for client */
#define FIELD_USER	0x0400
#define FIELD_ACCOUNT	0x0800
#define FIELD_OPLEVEL	0x1000 /* meaningless and stupid, but whatever */
#define FIELD_REALHOST	0x2000
#define FIELD_MODES	0x4000
#define FIELD_REPUTATION	0x8000

#define WMATCH_NICK	0x0001
#define WMATCH_USER	0x0002
#define WMATCH_OPER	0x0004
#define WMATCH_HOST	0x0008
#define WMATCH_INFO	0x0010
#define WMATCH_SERVER	0x0020
#define WMATCH_ACCOUNT	0x0040
#define WMATCH_IP	0x0080
#define WMATCH_MODES	0x0100
#define WMATCH_CONTIME	0x0200

#define RPL_WHOSPCRPL	354

#define WHO_ADD 1
#define WHO_DEL 0

#define HasField(x, y) ((x)->fields & (y))
#define IsMatch(x, y) ((x)->matchsel & (y))

#define IsMarked(x)           (moddata_client(x, whox_md).l)
#define SetMark(x)            do { moddata_client(x, whox_md).l = 1; } while(0)
#define ClearMark(x)          do { moddata_client(x, whox_md).l = 0; } while(0)

/* Structs */
struct who_format
{
	int fields;
	int matchsel;
	int umodes;
	int noumodes;
	const char *querytype;
	int show_realhost;
	int show_ip;
	time_t contimemin;
	time_t contimemax;
};

/* Global variables */
ModDataInfo *whox_md = NULL;

/* Forward declarations */
CMD_FUNC(cmd_whox);
static void who_global(Client *client, char *mask, int operspy, struct who_format *fmt);
static void do_who(Client *client, Client *acptr, Channel *channel, struct who_format *fmt);
static void do_who_on_channel(Client *client, Channel *channel,
                              int member, int operspy, struct who_format *fmt);
static int convert_classical_who_request(Client *client, int *parc, const char *parv[], const char **orig_mask, struct who_format *fmt);
const char *whox_md_serialize(ModData *m);
void whox_md_unserialize(const char *str, ModData *m);
void whox_md_free(ModData *md);
static void append_format(char *buf, size_t bufsize, size_t *pos, const char *fmt, ...) __attribute__((format(printf,4,5)));

MOD_INIT()
{
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	if (!CommandAdd(modinfo->handle, "WHO", cmd_whox, MAXPARA, CMD_USER))
	{
		config_warn("You cannot load both cmd_whox and cmd_who. You should ONLY load the cmd_whox module.");
		return MOD_FAILED;
	}

	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "whox";
	mreq.type = MODDATATYPE_CLIENT;
	mreq.serialize = whox_md_serialize;
	mreq.unserialize = whox_md_unserialize;
	mreq.free = whox_md_free;
	mreq.sync = 0;
	whox_md = ModDataAdd(modinfo->handle, mreq);
	if (!whox_md)
	{
		config_error("could not register whox moddata");
		return MOD_FAILED;
	}

	ISupportAdd(modinfo->handle, "WHOX", NULL);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

/** whox module data operations: serialize (rare) */
const char *whox_md_serialize(ModData *m)
{
	static char buf[32];
	if (m->i == 0)
		return NULL; /* not set */
	snprintf(buf, sizeof(buf), "%d", m->i);
	return buf;
}

/** whox module data operations: unserialize (rare) */
void whox_md_unserialize(const char *str, ModData *m)
{
	m->i = atoi(str);
}

/** whox module data operations: free */
void whox_md_free(ModData *md)
{
	/* we have nothing to free actually, but we must set to zero */
	md->l = 0;
}

/** cmd_whox: standardized "extended" version of WHO.
 * The good thing about WHOX is that it allows the client to define what
 * output they want to see. Another good thing is that it is standardized
 * (ok, actually it isn't... but hey!).
 * The bad thing is that it's a lot less flexible than the original WHO
 * we had in UnrealIRCd in 3.2.x and 4.0.x and that the WHOX spec defines
 * that there are two ways to specify a mask (neither one is logical):
 * WHO <mask> <flags>
 * WHO <mask> <flags> <mask2>
 * In case the latter is present then mask2 overrides the mask
 * (and the original 'mask' is ignored)
 * Yes, this indeed damn ugly. It shows (yet again) that they should never
 * have put the mask before the flags.. what kind of logic was that?
 * I wonder if the person who invented this also writes C functions like:
 * int (char *param)func
 * or types:
 * cp mode,time=--preserve
 * or creates configuration files like:
 * Syzop oper {
 * 5 maxlogins;
 * I mean... really...? -- Syzop
 * So, we will try to abide to the WHOX spec as much as possible but
 * try to warn our users in case they use the 'original' UnrealIRCd
 * WHO syntax which was WHO [+-]<flags> <mask>.
 * Note that we won't catch all cases, but we do our best.
 * When in doubt, we will assume WHOX so to not break the spec
 * (which again.. doesn't exist... but hey..)
 */
CMD_FUNC(cmd_whox)
{
	char *mask;
	const char *orig_mask;
	char ch; /* Scratch char register */
	const char *p; /* Scratch char pointer */
	int member;
	int operspy = 0;
	struct who_format fmt;
	const char *s;
	char maskcopy[BUFSIZE];
	Membership *lp;
	Client *acptr;

	memset(&fmt, 0, sizeof(fmt));

	if (!MyUser(client))
		return;

	if ((parc < 2))
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "WHO");
		return;
	}

	if ((parc > 3) && parv[3])
		orig_mask = parv[3];
	else
		orig_mask = parv[1];

	if (!convert_classical_who_request(client, &parc, parv, &orig_mask, &fmt))
		return;

	/* Evaluate the flags now, we consider the second parameter
	 * as "matchFlags%fieldsToInclude,querytype" */

	if ((parc > 2) && parv[2] && *parv[2])
	{
		p = parv[2];
		while (((ch = *(p++))) && (ch != '%') && (ch != ','))
		{
			switch (ch)
			{
				case 'o': fmt.matchsel |= WMATCH_OPER; continue;
				case 'n': fmt.matchsel |= WMATCH_NICK; continue;
				case 'u': fmt.matchsel |= WMATCH_USER; continue;
				case 'h': fmt.matchsel |= WMATCH_HOST; continue;
				case 'i': fmt.matchsel |= WMATCH_IP; continue;
				case 'r': fmt.matchsel |= WMATCH_INFO; continue;
				case 's': fmt.matchsel |= WMATCH_SERVER; continue;
				case 'a': fmt.matchsel |= WMATCH_ACCOUNT; continue;
				case 'm': fmt.matchsel |= WMATCH_MODES; continue;
				case 't': fmt.matchsel |= WMATCH_CONTIME; continue;
				case 'R':
					if (IsOper(client))
						fmt.show_realhost = 1;
					continue;
				case 'I':
					if (IsOper(client))
						fmt.show_ip = 1;
					continue;
			}
		}
	}

	if ((parc > 2) && (s = strchr(parv[2], '%')) != NULL)
	{
		s++;
		for (; *s != '\0'; s++)
		{
			switch (*s)
			{
				case 'c': fmt.fields |= FIELD_CHANNEL; break;
				case 'd': fmt.fields |= FIELD_HOP; break;
				case 'f': fmt.fields |= FIELD_FLAGS; break;
				case 'h': fmt.fields |= FIELD_HOST; break;
				case 'H': fmt.fields |= FIELD_REALHOST; break;
				case 'i': fmt.fields |= FIELD_IP; break;
				case 'l': fmt.fields |= FIELD_IDLE; break;
				case 'n': fmt.fields |= FIELD_NICK; break;
				case 'r': fmt.fields |= FIELD_INFO; break;
				case 's': fmt.fields |= FIELD_SERVER; break;
				case 't': fmt.fields |= FIELD_QUERYTYPE; break;
				case 'u': fmt.fields |= FIELD_USER; break;
				case 'a': fmt.fields |= FIELD_ACCOUNT; break;
				case 'm': fmt.fields |= FIELD_MODES; break;
				case 'o': fmt.fields |= FIELD_OPLEVEL; break;
				case 'R': fmt.fields |= FIELD_REPUTATION; break;
				case ',':
					s++;
					fmt.querytype = s;
					s += strlen(s);
					s--;
					break;
			}
		}
		if (BadPtr(fmt.querytype) || (strlen(fmt.querytype) > 3))
			fmt.querytype = "0";
	}

	strlcpy(maskcopy, orig_mask, sizeof maskcopy);
	mask = maskcopy;

	collapse(mask);

	if ((ValidatePermissionsForPath("channel:see:who:secret",client,NULL,NULL,NULL) &&
	     ValidatePermissionsForPath("channel:see:whois",client,NULL,NULL,NULL)))
	{
		operspy = 1;
	}

	if (fmt.matchsel & WMATCH_MODES)
	{
		char *s = mask;
		int *umodes;
		int what = WHO_ADD;
	
		while (*s)
		{
			Umode *um;

			switch (*s)
			{
				case '+':
					what = WHO_ADD;
					s++;
					break;
				case '-':
					what = WHO_DEL;
					s++;
					break;
			}

			if (!*s)
				break;

			if (what == WHO_ADD)
				umodes = &fmt.umodes;
			else
				umodes = &fmt.noumodes;

			for (um = usermodes; um; um = um->next)
			{
				if (um->letter == *s)
				{
					*umodes |= um->mode;
					break;
				}
			}
			s++;
		}

		if (!IsOper(client))
		{
			/* these are usermodes regular users may search for. just oper now. */
			fmt.umodes &= UMODE_OPER;
			fmt.noumodes &= UMODE_OPER;
		}
	}

	/* match connect time */
	if (fmt.matchsel & WMATCH_CONTIME)
	{
		char *s = mask;
		time_t currenttime = TStime();

		fmt.contimemin = 0;
		fmt.contimemax = 0;

		switch (*s)
		{
			case '<':
				if (*s++)
					fmt.contimemin = currenttime - config_checkval(s, CFG_TIME);
				break;
			case '>':
				if (*s++)
					fmt.contimemax = currenttime - config_checkval(s, CFG_TIME);
				break;
		}
	}

	/* '/who #some_channel' */
	if (IsChannelName(mask))
	{
		Channel *channel = NULL;

		/* List all users on a given channel */
		if ((channel = find_channel(orig_mask)) != NULL)
		{
			if (IsMember(client, channel) || operspy)
				do_who_on_channel(client, channel, 1, operspy, &fmt);
			else if (!SecretChannel(channel))
				do_who_on_channel(client, channel, 0, operspy, &fmt);
		}

		sendnumeric(client, RPL_ENDOFWHO, mask);
		return;
	}

	if (ValidatePermissionsForPath("channel:see:who:secret",client,NULL,NULL,NULL) ||
                ValidatePermissionsForPath("channel:see:whois",client,NULL,NULL,NULL))
	{
		operspy = 1;
	}

	/* '/who 0' for a global list.  this forces clients to actually
	 * request a full list.  I presume its because of too many typos
	 * with "/who" ;) --fl
	 */
	if (!strcmp(mask, "0"))
		who_global(client, NULL, 0, &fmt);
	else
		who_global(client, mask, operspy, &fmt);

	sendnumeric(client, RPL_ENDOFWHO, mask);
}

/* do_match
 * inputs	- pointer to client requesting who
 *		- pointer to client to do who on
 *		- char * mask to match
 *		- format options
 * output	- 1 if match, 0 if no match
 * side effects	- NONE
 */
static int do_match(Client *client, Client *acptr, char *mask, struct who_format *fmt)
{
	if (mask == NULL)
		return 1;

	/* default */
	if (fmt->matchsel == 0 && (match_simple(mask, acptr->name) ||
		match_simple(mask, acptr->user->username) ||
		match_simple(mask, GetHost(acptr)) ||
		(IsOper(client) &&
		(match_simple(mask, acptr->user->realhost) ||
		(acptr->ip &&
		match_simple(mask, acptr->ip))))))
	{
		return 1;
	}

	/* match nick */
	if (IsMatch(fmt, WMATCH_NICK) && match_simple(mask, acptr->name))
		return 1;

	/* match username */
	if (IsMatch(fmt, WMATCH_USER) && match_simple(mask, acptr->user->username))
		return 1;

	/* match server */
	if (IsMatch(fmt, WMATCH_SERVER) && IsOper(client) && match_simple(mask, acptr->user->server))
		return 1;

	/* match hostname */
	if (IsMatch(fmt, WMATCH_HOST) && (match_simple(mask, GetHost(acptr)) ||
		(IsOper(client) && (match_simple(mask, acptr->user->realhost) ||
		(acptr->ip && match_simple(mask, acptr->ip))))))
	{
		return 1;
	}

	/* match realname */
	if (IsMatch(fmt, WMATCH_INFO) && match_simple(mask, acptr->info))
		return 1;

	/* match ip address */
	if (IsMatch(fmt, WMATCH_IP) && IsOper(client) && acptr->ip &&
		match_user(mask, acptr, MATCH_CHECK_IP))
		return 1;

	/* match account */
	if (IsMatch(fmt, WMATCH_ACCOUNT) && IsLoggedIn(acptr) && match_simple(mask, acptr->user->account))
	{
		return 1;
	}

	/* match usermodes */
	if (IsMatch(fmt, WMATCH_MODES) && (fmt->umodes || fmt->noumodes))
	{
		long umodes = acptr->umodes;
		if ((acptr->umodes & UMODE_HIDEOPER) && !IsOper(client))
			umodes &= ~UMODE_OPER; /* pretend -o if +H */
		/* Now check 'umodes' (not acptr->umodes),
		 * If multiple conditions are specified it is an
		 * AND condition and not an OR.
		 */
		if (((umodes & fmt->umodes) == fmt->umodes) &&
		    ((umodes & fmt->noumodes) == 0))
		{
			return 1;
		}
	}

	/* match connect time */
	if (IsMatch(fmt, WMATCH_CONTIME) && MyConnect(acptr) && (fmt->contimemin || fmt->contimemax))
	{
		if (fmt->contimemin && (acptr->local->creationtime > fmt->contimemin))
			return 1;

		if (fmt->contimemax && (acptr->local->creationtime < fmt->contimemax))
			return 1;
	}

	return 0;
}

/* who_common_channel
 * inputs		- pointer to client requesting who
 *			- pointer to channel.
 *			- char * mask to match
 *			- int if oper on a server or not
 *			- pointer to int maxmatches
 *			- format options
 * output		- NONE
 * side effects 	- lists matching clients on specified channel,
 *			  marks matched clients.
 *
 * NOTE: only call this from who_global() due to client marking!
 */

static void who_common_channel(Client *client, Channel *channel,
	char *mask, int *maxmatches, struct who_format *fmt)
{
	Member *cm = channel->members;
	Client *acptr;
	Hook *h;
	int i = 0;

	for (cm = channel->members; cm; cm = cm->next)
	{
		acptr = cm->client;

		if (IsMarked(acptr))
			continue;

		if (IsMatch(fmt, WMATCH_OPER) && !IsOper(acptr))
			continue;

		for (h = Hooks[HOOKTYPE_VISIBLE_IN_CHANNEL]; h; h = h->next)
		{
			i = (*(h->func.intfunc))(acptr,channel);
			if (i != 0)
				break;
		}

		if (i != 0 && !(check_channel_access(client, channel, "hoaq")) && !(check_channel_access(acptr, channel, "hoaq") || check_channel_access(acptr,channel, "v")))
			continue;

		SetMark(acptr);

		if (*maxmatches > 0)
		{
			if (do_match(client, acptr, mask, fmt))
			{
				do_who(client, acptr, NULL, fmt);
				--(*maxmatches);
			}
		}
	}
}

/*
 * who_global
 *
 * inputs		- pointer to client requesting who
 *			- char * mask to match
 *			- int if oper on a server or not
 *			- int if operspy or not
 *			- format options
 * output		- NONE
 * side effects		- do a global scan of all clients looking for match
 *			  this is slightly expensive on EFnet ...
 *			  marks assumed cleared for all clients initially
 *			  and will be left cleared on return
 */

static void who_global(Client *client, char *mask, int operspy, struct who_format *fmt)
{
	Client *hunted = NULL;
	Client *acptr;
	int maxmatches = IsOper(client) ? INT_MAX : WHOLIMIT;

	/* If searching for a nick explicitly, then include it later on in the result: */
	if (mask && ((fmt->matchsel & WMATCH_NICK) || (fmt->matchsel == 0)))
		hunted = find_user(mask, NULL);

	/* Initialize the markers to zero */
	list_for_each_entry(acptr, &client_list, client_node)
		ClearMark(acptr);

	/* First, if not operspy, then list all matching clients on common channels */
	if (!operspy)
	{
		Membership *lp;

		for (lp = client->user->channel; lp; lp = lp->next)
			who_common_channel(client, lp->channel, mask, &maxmatches, fmt);
	}

	/* Second, list all matching visible clients. */
	list_for_each_entry(acptr, &client_list, client_node)
	{
		if (!IsUser(acptr))
			continue;

		if (IsInvisible(acptr) && !operspy && (client != acptr) && (acptr != hunted))
			continue;

		if (IsMarked(acptr))
			continue;

		if (IsMatch(fmt, WMATCH_OPER) && !IsOper(acptr))
			continue;

		if (maxmatches > 0)
		{
			if (do_match(client, acptr, mask, fmt))
 			{
				do_who(client, acptr, NULL, fmt);
				--maxmatches;
 			}
 		}
	}

	if (maxmatches <= 0)
		sendnumeric(client, ERR_TOOMANYMATCHES, "WHO", "output too large, truncated");
}

/*
 * do_who_on_channel
 *
 * inputs		- pointer to client requesting who
 *			- pointer to channel to do who on
 *			- The "real name" of this channel
 *			- int if client is a server oper or not
 *			- int if client is member or not
 *			- format options
 * output		- NONE
 * side effects		- do a who on given channel
 */

static void do_who_on_channel(Client *client, Channel *channel,
	int member, int operspy, struct who_format *fmt)
{
	Member *cm = channel->members;
	Hook *h;
	int i = 0;

	for (cm = channel->members; cm; cm = cm->next)
	{
		Client *acptr = cm->client;

		if (IsMatch(fmt, WMATCH_OPER) && !IsOper(acptr))
			continue;

		for (h = Hooks[HOOKTYPE_VISIBLE_IN_CHANNEL]; h; h = h->next)
		{
			i = (*(h->func.intfunc))(acptr,channel);
			if (i != 0)
				break;
		}

		if (!operspy && (acptr != client) && i != 0 && !(check_channel_access(client, channel, "hoaq")) && !(check_channel_access(acptr, channel, "hoaq") || check_channel_access(acptr,channel, "v")))
			continue;

		if (member || !IsInvisible(acptr))
			do_who(client, acptr, channel, fmt);
	}
}

/*
 * append_format
 *
 * inputs		- pointer to buffer
 *			- size of buffer
 *			- pointer to position
 *			- format string
 *			- arguments for format
 * output		- NONE
 * side effects 	- position incremented, possibly beyond size of buffer
 *			  this allows detecting overflow
 */

static void append_format(char *buf, size_t bufsize, size_t *pos, const char *fmt, ...)
{
	size_t max, result;
	va_list ap;

	max = *pos >= bufsize ? 0 : bufsize - *pos;
	va_start(ap, fmt);
	result = vsnprintf(buf + *pos, max, fmt, ap);
	va_end(ap);
	*pos += result;
}

/*
 * show_ip()		- asks if the true IP should be shown when source is
 *			  asking for info about target
 *
 * Inputs		- client who is asking
 *			- acptr who do we want the info on
 * Output		- returns 1 if clear IP can be shown, otherwise 0
 * Side Effects		- none
 */

static int show_ip(Client *client, Client *acptr)
{
	if (IsServer(acptr))
		return 0;
	else if ((client != NULL) && (MyConnect(client) && !IsOper(client)) && (client == acptr))
		return 1;
	else if (IsHidden(acptr) && ((client != NULL) && !IsOper(client)))
		return 0;
	else
		return 1;
}

/*
 * do_who
 *
 * inputs	- pointer to client requesting who
 *		- pointer to client to do who on
 *		- channel or NULL
 *		- format options
 * output	- NONE
 * side effects - do a who on given person
 */

static void do_who(Client *client, Client *acptr, Channel *channel, struct who_format *fmt)
{
	char status[20];
	char str[510 + 1];
	size_t pos;
	int hide = (FLAT_MAP && !IsOper(client)) ? 1 : 0;
	int i = 0;
	Hook *h;

	if (acptr->user->away)
 		status[i++] = 'G';
 	else
 		status[i++] = 'H';

	if (IsRegNick(acptr))
		status[i++] = 'r';

	if (IsSecureConnect(acptr))
		status[i++] = 's';

	for (h = Hooks[HOOKTYPE_WHO_STATUS]; h; h = h->next)
	{
		int ret = (*(h->func.intfunc))(client, acptr, NULL, NULL, status, 0);
		if (ret != 0)
			status[i++] = (char)ret;
	}

	if (IsOper(acptr) && (!IsHideOper(acptr) || client == acptr || IsOper(client)))
		status[i++] = '*';

	if (IsOper(acptr) && (IsHideOper(acptr) && client != acptr && IsOper(client)))
		status[i++] = '!';

	if (channel)
	{
		Membership *lp;

		if ((lp = find_membership_link(acptr->user->channel, channel)))
		{
			if (!(fmt->fields || HasCapability(client, "multi-prefix")))
			{
				/* Standard NAMES reply (single character) */
				char c = mode_to_prefix(*lp->member_modes);
				if (c)
					status[i++] = c;
			}
			else
			{
				/* NAMES reply with all rights included (multi-prefix / NAMESX) */
				strcpy(&status[i], modes_to_prefix(lp->member_modes));
				i += strlen(&status[i]);
			}
		}
	}

	status[i] = '\0';
 
	if (fmt->fields == 0)
	{
		char *host;
		if (fmt->show_realhost)
			host = acptr->user->realhost;
		else if (fmt->show_ip)
			host = GetIP(acptr);
		else
			host = GetHost(acptr);
		sendnumeric(client, RPL_WHOREPLY,
			channel ? channel->name : "*",
			acptr->user->username, host,
			hide ? "*" : acptr->user->server,
			acptr->name, status, hide ? 0 : acptr->hopcount, acptr->info);
	} else
	{
		str[0] = '\0';
		pos = 0;
		append_format(str, sizeof str, &pos, ":%s %d %s", me.name, RPL_WHOSPCRPL, client->name);
		if (HasField(fmt, FIELD_QUERYTYPE))
			append_format(str, sizeof str, &pos, " %s", fmt->querytype);
		if (HasField(fmt, FIELD_CHANNEL))
			append_format(str, sizeof str, &pos, " %s", channel ? channel->name : "*");
		if (HasField(fmt, FIELD_USER))
			append_format(str, sizeof str, &pos, " %s", acptr->user->username);
		if (HasField(fmt, FIELD_IP))
		{
			if (show_ip(client, acptr) && acptr->ip)
				append_format(str, sizeof str, &pos, " %s", acptr->ip);
			else
				append_format(str, sizeof str, &pos, " %s", "255.255.255.255");
		}

		if (HasField(fmt, FIELD_HOST) || HasField(fmt, FIELD_REALHOST))
		{
			if (IsOper(client) && HasField(fmt, FIELD_REALHOST))
				append_format(str, sizeof str, &pos, " %s", acptr->user->realhost);
			else
				append_format(str, sizeof str, &pos, " %s", GetHost(acptr));
		}

		if (HasField(fmt, FIELD_SERVER))
			append_format(str, sizeof str, &pos, " %s", hide ? "*" : acptr->user->server);
		if (HasField(fmt, FIELD_NICK))
			append_format(str, sizeof str, &pos, " %s", acptr->name);
		if (HasField(fmt, FIELD_FLAGS))
			append_format(str, sizeof str, &pos, " %s", status);
		if (HasField(fmt, FIELD_MODES))
		{
			if (IsOper(client))
			{
				const char *umodes = get_usermode_string(acptr);
				if (*umodes == '+')
					umodes++;
				append_format(str, sizeof str, &pos, " %s", umodes);
			} else {
				append_format(str, sizeof str, &pos, " %s", "*");
			}
		}
		if (HasField(fmt, FIELD_HOP))
			append_format(str, sizeof str, &pos, " %d", hide ? 0 : acptr->hopcount);
		if (HasField(fmt, FIELD_IDLE))
		{
			append_format(str, sizeof str, &pos, " %d",
				(int)((MyUser(acptr) && !hide_idle_time(client, acptr)) ? (TStime() - acptr->local->idle_since) : 0));
		}
		if (HasField(fmt, FIELD_ACCOUNT))
			append_format(str, sizeof str, &pos, " %s", IsLoggedIn(acptr) ? acptr->user->account : "0");
		if (HasField(fmt, FIELD_OPLEVEL))
			append_format(str, sizeof str, &pos, " %s", (channel && check_channel_access(acptr, channel, "hoaq")) ? "999" : "n/a");
		if (HasField(fmt, FIELD_REPUTATION))
		{
			if (IsOper(client))
				append_format(str, sizeof str, &pos, " %d", GetReputation(acptr));
			else
				append_format(str, sizeof str, &pos, " %s", "*");
		}
		if (HasField(fmt, FIELD_INFO))
			append_format(str, sizeof str, &pos, " :%s", acptr->info);

		sendto_one(client, NULL, "%s", str);
	}
}

/* Yeah, this is fun. Thank you WHOX !!! */
static int convert_classical_who_request(Client *client, int *parc, const char *parv[], const char **orig_mask, struct who_format *fmt)
{
	const char *p;
	static char pbuf1[512], pbuf2[512];
	int points;

	/* Figure out if the user is doing a 'classical' UnrealIRCd request,
	 * which can be recognized as follows:
	 * 1) Always a + or - as 1st character for the 1st parameter.
	 * 2) Unlikely to have a % (percent sign) in the 2nd parameter
	 * 3) Unlikely to have a 3rd parameter
	 * On the other hand WHOX requests are:
	 * 4) never going to have a '*', '?' or '.' as 2nd parameter
	 * 5) never going to have a '+' or '-' as 1st character in 2nd parameter
	 * Additionally, WHOX requests are useless - and thus unlikely -
	 * to search for a mask mask starting with + or -  except when:
	 * 6) searching for 'm' (mode)
	 * 7) or 'r' (info, realname)
	 * ..for which this would be a meaningful request.
	 * The end result is that we can do quite some useful heuristics
	 * except for some corner cases.
	 */
	if (((*parv[1] == '+') || (*parv[1] == '-')) &&
	    (!parv[2] || !strchr(parv[2], '%')) &&
	    (*parc < 4))
	{
		/* Conditions 1-3 of above comment are met, now we deal
		 * with conditions 4-7.
		 */
		if (parv[2] &&
		    !strchr(parv[2], '*') && !strchr(parv[2], '?') &&
		    !strchr(parv[2], '.') &&
		    !strchr(parv[2], '+') && !strchr(parv[2], '-') &&
		    (strchr(parv[2], 'm') || strchr(parv[2], 'r')))
		{
			/* 'WHO +something m" or even
			 * 'WHO +c #something' (which includes 'm')
			 * could mean either WHO or WHOX style
			 */
		} else {
			/* If we get here then it's an classical
			 * UnrealIRCd-style WHO request which has
			 * the order: WHO <options> <mask>
			 */
			char oldrequest[256];
			snprintf(oldrequest, sizeof(oldrequest), "WHO %s%s%s",
			         parv[1], parv[2] ? " " : "", parv[2] ? parv[2] : "");
			if (parv[2])
			{
				const char *swap = parv[1];
				parv[1] = parv[2];
				parv[2] = swap;
			} else {
				/* A request like 'WHO +I' or 'WHO +R' */
				parv[2] = parv[1];
				parv[1] = "*";
				parv[3] = NULL;
				*parc = 3;
			}

			/* Ok, that was the first step, now we need to convert the
			 * flags since they have changed a little as well:
			 * Flag a: user is away                                            << no longer exists
			 * Flag c <channel>: user is on <channel>                          << no longer exists
			 * Flag g <gcos/realname>: user has string <gcos> in his/her GCOS  << now called 'r'
			 * Flag h <host>: user has string <host> in his/her hostname       << no change
			 * Flag i <ip>: user has string <ip> in his/her IP address         << no change
			 * Flag m <usermodes>: user has <usermodes> set                    << behavior change
			 * Flag n <nick>: user has string <nick> in his/her nickname       << no change
			 * Flag s <server>: user is on server <server>                     << no change
			 * Flag u <user>: user has string <user> in his/her username       << no change
			 * Behavior flags:
			 * Flag M: check for user in channels I am a member of             << no longer exists
			 * Flag R: show users' real hostnames                              << no change (re-added)
			 * Flag I: show users' IP addresses                                << no change (re-added)
			 */

			if (strchr(parv[2], 'a'))
			{
				sendnotice(client, "WHO request '%s' failed: flag 'a' no longer exists with WHOX.", oldrequest);
				return 0;
			}
			if (strchr(parv[2], 'c'))
			{
				sendnotice(client, "WHO request '%s' failed: flag 'c' no longer exists with WHOX.", oldrequest);
				return 0;
			}
			if (strchr(parv[2], 'g'))
			{
				char *w;
				strlcpy(pbuf2, parv[2], sizeof(pbuf2));
				for (w = pbuf2; *w; w++)
				{
					if (*w == 'g')
					{
						*w = 'r';
						break;
					}
				}
				parv[2] = pbuf2;
			}

			/* "WHO -m xyz" (now: xyz -m) should become "WHO -xyz m"
			 * Wow, this seems overly complex, but okay...
			 */
			points = 0;
			for (p = parv[2]; *p; p++)
			{
				if (*p == '-')
					points = 1;
				else if (*p == '+')
					points = 0;
				else if (points && (*p == 'm'))
				{
					points = 2;
					break;
				}
			}
			if (points == 2)
			{
				snprintf(pbuf1, sizeof(pbuf1), "-%s", parv[1]);
				parv[1] = pbuf1;
			}

			if ((*parv[2] == '+') || (*parv[2] == '-'))
				parv[2] = parv[2]+1; /* strip '+'/'-' prefix, which does not exist in WHOX */

			sendnotice(client, "WHO request '%s' changed to match new WHOX syntax: 'WHO %s %s'",
				oldrequest, parv[1], parv[2]);
			*orig_mask = parv[1];
		}
	}
	return 1;
}
