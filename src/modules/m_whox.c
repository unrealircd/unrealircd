/* m_whox.c / WHOX.
 * based on code from charybdis and ircu.
 * was originally made for tircd and modified to work with u4.
 * - 2018 i <ircd@servx.org>
 * GPLv2
 */

#include "unrealircd.h"

#define MSG_WHO 	"WHO"

#define FLAGS_MARK	0x400000 /* marked client (was hybnotice) */

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

#define WMATCH_NICK	0x0001
#define WMATCH_USER	0x0002
#define WMATCH_OPER	0x0004
#define WMATCH_HOST	0x0008
#define WMATCH_INFO	0x0010
#define WMATCH_SERVER	0x0020
#define WMATCH_ACCOUNT	0x0040
#define WMATCH_IP	0x0080
#define WMATCH_MODES	0x0100

#define RPL_WHOSPCRPL	354

#define WHO_ADD 1
#define WHO_DEL 2

#define SetMark(x) ((x)->flags |= FLAGS_MARK)
#define ClearMark(x) ((x)->flags &= ~FLAGS_MARK)
#define IsMarked(x) ((x)->flags & FLAGS_MARK)

#define HasField(x, y) ((x)->fields & (y))
#define IsMatch(x, y) ((x)->matchsel & (y))

struct who_format
{
	int fields;
	int matchsel;
	int umodes;
	int noumodes;
	const char *querytype;
};

CMD_FUNC(m_whox);
static void who_global(aClient *sptr, char *mask, int operspy, struct who_format *fmt);
static void do_who(aClient *sptr, aClient *acptr, aChannel *chptr, struct who_format *fmt);
static void do_who_on_channel(aClient *sptr, aChannel *chptr,
                              int member, int operspy, struct who_format *fmt);
static int override_who(Cmdoverride *ovr, aClient *cptr, aClient *sptr, int parc, char *parv[]);

ModuleHeader MOD_HEADER(m_whox)
  = {
	"m_whox",
	"4.0",
	"command /who",
	"3.2-b8-1",
	NULL
    };

MOD_INIT(m_whox)
{
	//CommandAdd(modinfo->handle, MSG_WHO, m_whox, MAXPARA, M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	IsupportAdd(NULL, "WHOX", NULL);
	return MOD_SUCCESS;
}

MOD_LOAD(m_whox)
{
	CmdoverrideAddEx(modinfo->handle, "WHO", 9999999, override_who);
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_whox)
{
	Isupport *hunted = IsupportFind("WHOX");

	if (hunted)
		IsupportDel(hunted);

	return MOD_SUCCESS;
}

/* Temporary glue until this module becomes the new m_who */
static int override_who(Cmdoverride *ovr, aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	/* We are always last (and we need to be), thanks to our cmdoverride priority */
	return m_whox(cptr, sptr, parc, parv);
}

/*
** m_whox
**	parv[1] = nickname mask list
**	parv[2] = additional selection flag and format options
*/
CMD_FUNC(m_whox)
{
	static time_t last_used = 0;
	char *mask;
	char ch; /* Scratch char register */
	char *p; /* Scratch char pointer */
	int member;
	int operspy = 0;
	struct who_format fmt;
	const char *s;
	char maskcopy[BUFSIZE];
	Membership *lp;
	aClient *acptr;

	if (!MyClient(sptr))
		return 0;

	if ((parc < 2) && (IsPerson(sptr)))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			me.name, sptr->name, "WHO");
		return 0;
	}

	fmt.fields = 0;
	fmt.matchsel = 0;
	fmt.umodes = 0;
	fmt.noumodes = 0;
	fmt.querytype = NULL;

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

	strlcpy(maskcopy, parv[1], sizeof maskcopy);
	mask = maskcopy;

	collapse(mask);

	/* '/who *' */
	if ((*(mask + 1) == '\0') && (*mask == '*'))
	{
		if(sptr->user == NULL)
			return 0;

		if ((lp = sptr->user->channel) != NULL)
			do_who_on_channel(sptr, lp->chptr, 1, 0, &fmt);

		sendto_one(sptr, getreply(RPL_ENDOFWHO), me.name, sptr->name, "*");
		return 0;
	}

	if ((ValidatePermissionsForPath("override:see:who:secret",sptr,NULL,NULL,NULL) ||
	     ValidatePermissionsForPath("override:see:whois",sptr,NULL,NULL,NULL)) && (*mask == '!'))
	{
		mask++;
		operspy = 1;

		if (BadPtr(mask))
		{
			sendto_one(sptr, getreply(RPL_ENDOFWHO), me.name, sptr->name, parv[1]);
			return 0;
		}
	}

	if (fmt.matchsel & WMATCH_MODES)
	{
		char *s = mask;
		int *umodes;
		int what = WHO_ADD;
	
		while (*s)
		{
			int i;

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

			for (i = 0; i <= Usermode_highest; i++)
			{
				if (*s == Usermode_Table[i].flag)
				{
					*umodes |= Usermode_Table[i].mode;
					break;
				}
			}
			s++;
		}

		if (!IsOper(sptr))
		{
			/* these are usermodes regular users may search for. just oper now. */
			fmt.umodes &= UMODE_OPER;
			fmt.noumodes &= UMODE_OPER;
		}
	}

	/* '/who #some_channel' */
	if (IsChannelName(mask))
	{
		aChannel *chptr = NULL;

		/* List all users on a given channel */
		if ((chptr = find_channel(parv[1] + operspy, NULL)) != NULL)
		{
			if (operspy)
			{
				sendto_snomask_global(SNO_EYES, "*** %s (%s@%s) did a /who on %s",
					sptr->name, sptr->user->username, GetHost(sptr),
					chptr->chname);
			}

			if (IsMember(sptr, chptr) || operspy)
				do_who_on_channel(sptr, chptr, 1, operspy, &fmt);
			else if (!SecretChannel(chptr))
				do_who_on_channel(sptr, chptr, 0, operspy, &fmt);
		}

		sendto_one(sptr, getreply(RPL_ENDOFWHO), me.name, sptr->name, mask);
		return 0;
	}

	/* '/who nick' */
	if (((acptr = find_person(mask, NULL)) != NULL) &&
		(!(fmt.matchsel & WMATCH_MODES)) &&
		(!(fmt.matchsel & WMATCH_OPER) || IsOper(acptr)))
	{
		int isinvis = 0;
		int i = 0;
		Hook *h;

		isinvis = IsInvisible(acptr);
		for (lp = acptr->user->channel; lp; lp = lp->next)
		{
			member = IsMember(sptr, lp->chptr);

			if (isinvis && !member)
				continue;

			for (h = Hooks[HOOKTYPE_VISIBLE_IN_CHANNEL]; h; h = h->next)
			{
				i = (*(h->func.intfunc))(acptr,lp->chptr);
				if (i != 0)
					break;
			}

			if (i != 0 && !(is_skochanop(sptr, lp->chptr)) && !(is_skochanop(acptr, lp->chptr) || has_voice(acptr,lp->chptr)))
				continue;

			if (member || (!isinvis && PubChannel(lp->chptr)))
				break;
		}
		if (lp != NULL)
			do_who(sptr, acptr, lp->chptr, &fmt);
		else
			do_who(sptr, acptr, NULL, &fmt);

		sendto_one(sptr, getreply(RPL_ENDOFWHO), me.name, sptr->name, parv[1]);
		return 0;
	}

	if (ValidatePermissionsForPath("override:see:who:secret",sptr,NULL,NULL,NULL) ||
                ValidatePermissionsForPath("override:see:whois",sptr,NULL,NULL,NULL))
	{
		operspy = 1;
	}

	/* '/who 0' for a global list.  this forces clients to actually
	 * request a full list.  I presume its because of too many typos
	 * with "/who" ;) --fl
	 */
	if ((*(mask + 1) == '\0') && (*mask == '0'))
		who_global(sptr, NULL, 0, &fmt);
	else
		who_global(sptr, mask, operspy, &fmt);

	sendto_one(sptr, getreply(RPL_ENDOFWHO), me.name, sptr->name, mask);

	return 0;
}

/* do_match
 * inputs	- pointer to client requesting who
 *		- pointer to client to do who on
 *		- char * mask to match
 *		- format options
 * output	- 1 if match, 0 if no match
 * side effects	- NONE
 */
static int do_match(aClient *sptr, aClient *acptr, char *mask, struct who_format *fmt)
{
	if (mask == NULL)
		return 1;

	/* default */
	if (fmt->matchsel == 0 && (!match(mask, acptr->name) ||
		!match(mask, acptr->user->username) ||
		!match(mask, GetHost(acptr)) ||
		(IsOper(sptr) &&
		(!match(mask, acptr->user->realhost) ||
		(acptr->ip &&
		!match(mask, acptr->ip))))))
	{
		return 1;
	}

	/* match nick */
	if (IsMatch(fmt, WMATCH_NICK) && !match(mask, acptr->name))
		return 1;

	/* match username */
	if (IsMatch(fmt, WMATCH_USER) && !match(mask, acptr->user->username))
		return 1;

	/* match server */
	if (IsMatch(fmt, WMATCH_SERVER) && IsOper(sptr) && !match(mask, acptr->user->server))
		return 1;

	/* match hostname */
	if (IsMatch(fmt, WMATCH_HOST) && (!match(mask, GetHost(acptr)) ||
		(IsOper(sptr) && (!match(mask, acptr->user->realhost) ||
		(acptr->ip && !match(mask, acptr->ip))))))
	{
		return 1;
	}

	/* match realname */
	if (IsMatch(fmt, WMATCH_INFO) && !match(mask, acptr->info))
		return 1;

	/* match ip address */
	if (IsMatch(fmt, WMATCH_IP) && IsOper(sptr) && acptr->ip && !match(mask, acptr->ip))
		return 1;

	/* match account */
	if (IsMatch(fmt, WMATCH_ACCOUNT) && !BadPtr(acptr->user->svid) &&
		!isdigit(*acptr->user->svid) && !match(mask, acptr->user->svid))
	{
		return 1;
	}

	/* match usermodes */
	if (IsMatch(fmt, WMATCH_MODES) &&
		((acptr->umodes & fmt->umodes) &&
		!(acptr->umodes & fmt->noumodes) &&
		(!(acptr->umodes & UMODE_HIDEOPER) || IsOper(sptr))))
	{
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
 * side effects 	- lists matching invisible clients on specified channel,
 *			  marks matched clients.
 */

static void who_common_channel(aClient *sptr, aChannel *chptr,
	char *mask, int *maxmatches, struct who_format *fmt)
{
	Member *cm = chptr->members;
	aClient *acptr;
	Hook *h;
	int i = 0;

	for (cm = chptr->members; cm; cm = cm->next)
	{
		acptr = cm->cptr;

		if (!IsInvisible(acptr) || IsMarked(acptr))
			continue;

		if(IsMatch(fmt, WMATCH_OPER) && !IsOper(acptr))
			continue;

		for (h = Hooks[HOOKTYPE_VISIBLE_IN_CHANNEL]; h; h = h->next)
		{
			i = (*(h->func.intfunc))(acptr,chptr);
			if (i != 0)
				break;
		}

		if (i != 0 && !(is_skochanop(sptr, chptr)) && !(is_skochanop(acptr, chptr) || has_voice(acptr,chptr)))
			continue;

		SetMark(acptr);

		if(*maxmatches > 0)
		{
			if (do_match(sptr, acptr, mask, fmt))
			{
				do_who(sptr, acptr, NULL, fmt);
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

static void who_global(aClient *sptr, char *mask, int operspy, struct who_format *fmt)
{
	aClient *acptr;
	int maxmatches = WHOLIMIT ? WHOLIMIT : 100;

	/* first, list all matching INvisible clients on common channels
	 * if this is not an operspy who
	 */
	if(!operspy)
	{
		Membership *lp;

		for (lp = sptr->user->channel; lp; lp = lp->next)
			who_common_channel(sptr, lp->chptr, mask, &maxmatches, fmt);
	}

	/* second, list all matching visible clients and clear all marks
	 * on invisible clients
	 * if this is an operspy who, list all matching clients, no need
	 * to clear marks
	 */
	list_for_each_entry(acptr, &client_list, client_node)
	{
		if(!IsPerson(acptr))
			continue;

		if(IsInvisible(acptr) && !operspy)
 		{
			ClearMark(acptr);
			continue;
		}

		if(IsMatch(fmt, WMATCH_OPER) && !IsOper(acptr))
			continue;

		if(maxmatches > 0)
		{
			if (do_match(sptr, acptr, mask, fmt))
 			{
				do_who(sptr, acptr, NULL, fmt);
				--maxmatches;
 			}
 		}
	}

	if (maxmatches <= 0)
		sendto_one(sptr, ":%s 416 %s %s :output too large, truncated", me.name, sptr->name, "WHO"); /* ERR_TOOMANYMATCHES */
}

/*
 * do_who_on_channel
 *
 * inputs		- pointer to client requesting who
 *			- pointer to channel to do who on
 *			- The "real name" of this channel
 *			- int if sptr is a server oper or not
 *			- int if client is member or not
 *			- format options
 * output		- NONE
 * side effects		- do a who on given channel
 */

static void do_who_on_channel(aClient *sptr, aChannel *chptr,
	int member, int operspy, struct who_format *fmt)
{
	Member *cm = chptr->members;
	Hook *h;
	int i = 0;

	for (cm = chptr->members; cm; cm = cm->next)
	{
		aClient *acptr = cm->cptr;

		if (IsMatch(fmt, WMATCH_OPER) && !IsOper(acptr))
			continue;

		for (h = Hooks[HOOKTYPE_VISIBLE_IN_CHANNEL]; h; h = h->next)
		{
			i = (*(h->func.intfunc))(acptr,chptr);
			if (i != 0)
				break;
		}

		if (!operspy && (acptr != sptr) && i != 0 && !(is_skochanop(sptr, chptr)) && !(is_skochanop(acptr, chptr) || has_voice(acptr,chptr)))
			continue;

		if(member || !IsInvisible(acptr))
			do_who(sptr, acptr, chptr, fmt);
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
 * Inputs		- sptr who is asking
 *			- acptr who do we want the info on
 * Output		- returns 1 if clear IP can be shown, otherwise 0
 * Side Effects		- none
 */

static int show_ip(aClient *sptr, aClient *acptr)
{
	if (IsServer(acptr))
		return 0;
	else if ((sptr != NULL) && (MyConnect(sptr) && !IsOper(sptr)) && (sptr == acptr))
		return 1;
	else if (IsHidden(acptr) && ((sptr != NULL) && !IsOper(sptr)))
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

static void do_who(aClient *sptr, aClient *acptr, aChannel *chptr, struct who_format *fmt)
{
	char status[20];
	char str[510 + 1];
	size_t pos;
	int hide = (FLAT_MAP && !IsOper(sptr)) ? 1 : 0;
	int i = 0;
	Hook *h;

	if (acptr->user->away)
 		status[i++] = 'G';
 	else
 		status[i++] = 'H';

	if (IsARegNick(acptr))
		status[i++] = 'r';
 
	for (h = Hooks[HOOKTYPE_WHO_STATUS]; h; h = h->next)
	{
		int ret = (*(h->func.intfunc))(sptr, acptr, NULL, NULL, status, 0);
		if (ret != 0)
			status[i++] = (char)ret;
	}

	if (IsOper(acptr) && (!IsHideOper(acptr) || sptr == acptr || IsOper(sptr)))
		status[i++] = '*';

	if (IsOper(acptr) && (IsHideOper(acptr) && sptr != acptr && IsOper(sptr)))
		status[i++] = '!';

	if (chptr)
	{
		Membership *lp;

		if ((lp = find_membership_link(acptr->user->channel, chptr)))
		{
			if (!(fmt->fields || SupportNAMESX(sptr)))
			{
				/* Standard NAMES reply */
#ifdef PREFIX_AQ
				if (lp->flags & CHFL_CHANOWNER)
					status[i++] = '~';
				else if (lp->flags & CHFL_CHANPROT)
					status[i++] = '&';
				else
#endif
				if (lp->flags & CHFL_CHANOP)
					status[i++] = '@';
				else if (lp->flags & CHFL_HALFOP)
					status[i++] = '%';
				else if (lp->flags & CHFL_VOICE)
					status[i++] = '+';
			}
			else
			{
				/* NAMES reply with all rights included (NAMESX) */
#ifdef PREFIX_AQ
				if (lp->flags & CHFL_CHANOWNER)
					status[i++] = '~';
				if (lp->flags & CHFL_CHANPROT)
					status[i++] = '&';
#endif
				if (lp->flags & CHFL_CHANOP)
					status[i++] = '@';
				if (lp->flags & CHFL_HALFOP)
					status[i++] = '%';
				if (lp->flags & CHFL_VOICE)
					status[i++] = '+';
			}
		}
	}

	status[i] = '\0';
 
	if (fmt->fields == 0)
	{
		sendto_one(sptr, getreply(RPL_WHOREPLY), me.name,
			sptr->name, chptr ? chptr->chname : "*",
			acptr->user->username, GetHost(acptr),
			hide ? "*" : acptr->user->server,
			acptr->name, status, hide ? 0 : acptr->hopcount, acptr->info);
	} else
	{
		str[0] = '\0';
		pos = 0;
		append_format(str, sizeof str, &pos, ":%s %d %s", me.name, RPL_WHOSPCRPL, sptr->name);
		if (HasField(fmt, FIELD_QUERYTYPE))
			append_format(str, sizeof str, &pos, " %s", fmt->querytype);
		if (HasField(fmt, FIELD_CHANNEL))
			append_format(str, sizeof str, &pos, " %s", chptr ? chptr->chname : "*");
		if (HasField(fmt, FIELD_USER))
			append_format(str, sizeof str, &pos, " %s", acptr->user->username);
		if (HasField(fmt, FIELD_IP))
		{
			if (show_ip(sptr, acptr) && acptr->ip)
				append_format(str, sizeof str, &pos, " %s", acptr->ip);
			else
				append_format(str, sizeof str, &pos, " %s", "255.255.255.255");
		}

		if (HasField(fmt, FIELD_HOST) || HasField(fmt, FIELD_REALHOST))
		{
			if (IsOper(sptr) && HasField(fmt, FIELD_REALHOST))
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
			if (IsOper(sptr))
				append_format(str, sizeof str, &pos, " %s", strtok(get_mode_str(acptr), "+"));
			else
				append_format(str, sizeof str, &pos, " %s", "*");
		}
		if (HasField(fmt, FIELD_HOP))
			append_format(str, sizeof str, &pos, " %d", hide ? 0 : acptr->hopcount);
		if (HasField(fmt, FIELD_IDLE))
			append_format(str, sizeof str, &pos, " %d", (int)(MyClient(acptr) &&
				(!(acptr->umodes & UMODE_HIDLE) || IsOper(sptr) ||
				(sptr == acptr)) ? TStime() - acptr->local->last : 0));
		if (HasField(fmt, FIELD_ACCOUNT))
			append_format(str, sizeof str, &pos, " %s", (!isdigit(*acptr->user->svid)) ? acptr->user->svid : "0");
		if (HasField(fmt, FIELD_OPLEVEL))
			append_format(str, sizeof str, &pos, " %s", (chptr && is_skochanop(acptr, chptr)) ? "999" : "n/a");
		if (HasField(fmt, FIELD_INFO))
			append_format(str, sizeof str, &pos, " :%s", acptr->info);

		if (pos >= sizeof str)
		{
			static int warned = 0;
			if (!warned)
				sendto_snomask(SNO_JUNK, "*** WHOX overflow while sending information about %s to %s", acptr->name, sptr->name);
			warned = 1;
 		}
		sendto_one(sptr, "%s", str);
	}
}
