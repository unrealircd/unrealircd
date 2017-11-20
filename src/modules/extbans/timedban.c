/*
 * timedban - Timed bans that are automatically unset.
 * (C) Copyright 2009-2017 Bram Matthys (Syzop) and the UnrealIRCd team.
 * License: GPLv2
 *
 * This module adds an extended ban ~t:time:mask
 * Where 'time' is the time in minutes after which the ban will be removed.
 * Where 'mask' is any banmask that is normally valid.
 *
 * Note that this extended ban is rather special in the sense that
 * it permits (crazy) triple-extbans to be set, such as:
 * +b ~t:1:~q:~a:Account
 * (=a temporary 1min ban to mute a user with services account Account)
 * +e ~t:1440:~m:moderated:*!*@host
 * (=user with *!*@host may speak through +m for the next 1440m / 24h)
 *
 * The triple-extbans / double-stacking requires special routines that
 * are based on parts of the core and special recursion checks.
 * If you are looking for inspiration of coding your own extended ban
 * then look at another extended ban * module as this module is not a
 * good starting point ;)
 */
   
#include "unrealircd.h"

#define TIMEDBAN_VERSION "v1.0"

/* Maximum time (in minutes) for a ban */
#define TIMEDBAN_MAX_TIME	9999

/* Maximum length of a ban */
#define MAX_LENGTH 128

/* Split timeout event in <this> amount of iterations */
#define TIMEDBAN_TIMER_ITERATION_SPLIT 4

/* Call timeout event every <this> seconds.
 * NOTE: until all channels are processed it takes
 *       TIMEDBAN_TIMER_ITERATION_SPLIT * TIMEDBAN_TIMER.
 */
#define TIMEDBAN_TIMER	2

/* We allow a ban to (potentially) expire slightly before the deadline.
 * For example with TIMEDBAN_TIMER_ITERATION_SPLIT=4 and TIMEDBAN_TIMER=2
 * a 1 minute ban would expire at 56-63 seconds, rather than 60-67 seconds.
 * This is usually preferred.
 */
#define TIMEDBAN_TIMER_DELTA ((TIMEDBAN_TIMER_ITERATION_SPLIT*TIMEDBAN_TIMER)/2)

ModuleHeader MOD_HEADER(timedban)
  = {
	"timedban",
	TIMEDBAN_VERSION,
	"ExtBan ~t: automatically removed timed bans",
	"3.2-b8-1",
	NULL 
    };

/* Forward declarations */
char *timedban_extban_conv_param(char *para_in);
int timedban_extban_is_ok(aClient* sptr, aChannel* chptr, char* para_in, int checkt, int what, int what2);
int timedban_is_banned(aClient *sptr, aChannel *chptr, char *ban, int chktype);
void add_send_mode_param(aChannel *chptr, aClient *from, char what, char mode, char *param);
char *timedban_chanmsg(aClient *, aClient *, aChannel *, char *, int);

EVENT(timedban_timeout);

MOD_TEST(timedban)
{
	return MOD_SUCCESS;
}

MOD_INIT(timedban)
{
ExtbanInfo extban;

	memset(&extban, 0, sizeof(ExtbanInfo));
	extban.flag = 't';
	extban.options |= EXTBOPT_ACTMODIFIER; /* not really, but ours shouldn't be stacked from group 1 */
	extban.options |= EXTBOPT_CHSVSMODE; /* so "SVSMODE -nick" will unset affected ~t extbans */
	extban.options |= EXTBOPT_INVEX; /* also permit timed invite-only exceptions (+I) */
	extban.conv_param = timedban_extban_conv_param;
	extban.is_ok = timedban_extban_is_ok;
	extban.is_banned = timedban_is_banned;

	if (!ExtbanAdd(modinfo->handle, extban))
	{
		config_error("timedban: unable to register 't' extban type!!");
		return MOD_FAILED;
	}
                
	EventAddEx(modinfo->handle, "timedban_timeout", TIMEDBAN_TIMER, 0, timedban_timeout, NULL);

	return MOD_SUCCESS;
}

MOD_LOAD(timedban)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(timedban)
{
	return MOD_SUCCESS;
}

/** Generic helper for our conv_param extban function.
 * Mostly copied from clean_ban_mask()
 */
char *generic_clean_ban_mask(char *mask)
{
	char *cp, *x;
	char *user;
	char *host;
	Extban *p;
	static char maskbuf[512];

	/* Work on a copy */
	strlcpy(maskbuf, mask, sizeof(maskbuf));
	mask = maskbuf;

	cp = index(mask, ' ');
	if (cp)
		*cp = '\0';

	/* Strip any ':' at beginning since that would cause a desynch */
	for (; (*mask && (*mask == ':')); mask++);
	if (!*mask)
		return NULL;

	/* Forbid ASCII <= 32 in all bans */
	for (x = mask; *x; x++)
		if (*x <= ' ')
			return NULL;

	/* Extended ban? */
	if ((*mask == '~') && mask[1] && (mask[2] == ':'))
	{
		p = findmod_by_bantype(mask[1]);
		if (!p)
			return NULL; /* reject unknown extban */
		if (p->conv_param)
			return p->conv_param(mask);
		/* else, do some basic sanity checks and cut it off at 80 bytes */
		if ((mask[1] != ':') || (mask[2] == '\0'))
		    return NULL; /* require a ":<char>" after extban type */
		if (strlen(mask) > 80)
			mask[80] = '\0';
		return mask;
	}

	if ((*mask == '~') && !strchr(mask, '@'))
		return NULL; /* not an extended ban and not a ~user@host ban either. */

	if ((user = index((cp = mask), '!')))
		*user++ = '\0';
	if ((host = rindex(user ? user : cp, '@')))
	{
		*host++ = '\0';

		if (!user)
			return make_nick_user_host(NULL, trim_str(cp,USERLEN), 
				trim_str(host,HOSTLEN));
	}
	else if (!user && index(cp, '.'))
		return make_nick_user_host(NULL, NULL, trim_str(cp,HOSTLEN));
	return make_nick_user_host(trim_str(cp,NICKLEN), trim_str(user,USERLEN), 
		trim_str(host,HOSTLEN));
}

/** Convert ban to an acceptable format (or return NULL to fully reject it) */
char *timedban_extban_conv_param(char *para_in)
{
	static char retbuf[MAX_LENGTH+1];
	char para[MAX_LENGTH+1];
	char tmpmask[MAX_LENGTH+1];
	char *durationstr; /**< Duration, such as '5' */
	int duration;
	char *matchby; /**< Matching method, such as 'n!u@h' */
	char *newmask; /**< Cleaned matching method, such as 'n!u@h' */
	static int timedban_extban_conv_param_recursion = 0;
	
	if (timedban_extban_conv_param_recursion)
		return NULL; /* reject: recursion detected! */

	strlcpy(para, para_in+3, sizeof(para)); /* work on a copy (and truncate it) */
	
	/* ~t:duration:n!u@h   for direct matching
	 * ~t:duration:~x:.... when calling another bantype
	 */

	durationstr = para;
	matchby = strchr(para, ':');
	if (!matchby || !matchby[1])
		return NULL;
	*matchby++ = '\0';
	
	duration = atoi(durationstr);

	if ((duration <= 0) || (duration > TIMEDBAN_MAX_TIME))
		return NULL;

	strlcpy(tmpmask, matchby, sizeof(tmpmask));
	timedban_extban_conv_param_recursion++;
	//newmask = extban_conv_param_nuh_or_extban(tmpmask);
	newmask = generic_clean_ban_mask(tmpmask);
	timedban_extban_conv_param_recursion--;
	if (!newmask || (strlen(newmask) <= 1))
		return NULL;

	snprintf(retbuf, sizeof(retbuf), "~t:%d:%s", duration, newmask);
	return retbuf;
}

int timedban_extban_syntax(aClient *sptr, int checkt, char *reason)
{
	if (MyClient(sptr) && (checkt == EXBCHK_PARAM))
	{
		sendnotice(sptr, "Error when setting timed ban: %s", reason);
		sendnotice(sptr, " Syntax: +b ~t:duration:mask");
		sendnotice(sptr, "Example: +b ~t:5:nick!user@host");
		sendnotice(sptr, "Duration is the time in minutes after which the ban is removed (1-9999)");
		sendnotice(sptr, "Valid masks are: nick!user@host or another extban type such as ~a, ~c, ~S, ..");
	}
	return 0; /* FAIL: ban rejected */
}

/** Generic helper for sub-bans, used by our "is this ban ok?" function */
int generic_ban_is_ok(aClient *sptr, aChannel *chptr, char *mask, int checkt, int what, int what2)
{
	if ((mask[0] == '~') && MyClient(sptr))
	{
		Extban *p;

		/* This portion is copied from clean_ban_mask() */
		if (mask[1] && (mask[2] == ':') &&
		    RESTRICT_EXTENDEDBANS && MyClient(sptr) &&
		    !ValidatePermissionsForPath("channel:extbans",sptr,NULL,NULL,NULL))
		{
			if (!strcmp(RESTRICT_EXTENDEDBANS, "*"))
			{
				if (checkt == EXBCHK_ACCESS_ERR)
					sendnotice(sptr, "Setting/removing of extended bans has been disabled");
				return 0; /* REJECT */
			}
			if (strchr(RESTRICT_EXTENDEDBANS, mask[1]))
			{
				if (checkt == EXBCHK_ACCESS_ERR)
					sendnotice(sptr, "Setting/removing of extended bantypes '%s' has been disabled", RESTRICT_EXTENDEDBANS);
				return 0; /* REJECT */
			}
		}
		/* End of portion */

		/* This portion is inspired by m_mode */
		p = findmod_by_bantype(mask[1]);
		if (checkt == EXBCHK_ACCESS)
		{
			if (p && p->is_ok && !p->is_ok(sptr, chptr, mask, EXBCHK_ACCESS, what, what2) &&
			    !ValidatePermissionsForPath("override:extban",sptr,NULL,chptr,NULL))
			{
				return 0; /* REJECT */
			}
		} else
		if (checkt == EXBCHK_ACCESS_ERR)
		{
			if (p && p->is_ok && !p->is_ok(sptr, chptr, mask, EXBCHK_ACCESS, what, what2) &&
			    !ValidatePermissionsForPath("override:extban",sptr,NULL,chptr,NULL))
			{
				p->is_ok(sptr, chptr, mask, EXBCHK_ACCESS_ERR, what, what2);
				return 0; /* REJECT */
			}
		} else
		if (checkt == EXBCHK_PARAM)
		{
			if (p && p->is_ok && !p->is_ok(sptr, chptr, mask, EXBCHK_PARAM, what, what2))
			{
				return 0; /* REJECT */
			}
		}
		/* End of portion */
	}
	
	/* ACCEPT:
	 * - not an extban; OR
	 * - extban with NULL is_ok; OR
	 * - non-existing extban character (handled by conv_param?)
	 */
	return 1;
}

/** Validate ban ("is this ban ok?") */
int timedban_extban_is_ok(aClient* sptr, aChannel* chptr, char* para_in, int checkt, int what, int what2)
{
	char para[MAX_LENGTH+1];
	char tmpmask[MAX_LENGTH+1];
	char *durationstr; /**< Duration, such as '5' */
	int duration;
	char *matchby; /**< Matching method, such as 'n!u@h' */
	char *newmask; /**< Cleaned matching method, such as 'n!u@h' */
	static int timedban_extban_is_ok_recursion = 0;
	int res;

	/* Always permit deletion */
	if (what == MODE_DEL)
		return 1;

	if (timedban_extban_is_ok_recursion)
		return 0; /* Recursion detected (~t:1:~t:....) */

	strlcpy(para, para_in+3, sizeof(para)); /* work on a copy (and truncate it) */
	
	/* ~t:duration:n!u@h   for direct matching
	 * ~t:duration:~x:.... when calling another bantype
	 */

	durationstr = para;
	matchby = strchr(para, ':');
	if (!matchby || !matchby[1])
		return timedban_extban_syntax(sptr, checkt, "Invalid syntax");
	*matchby++ = '\0';

	duration = atoi(durationstr);

	if ((duration <= 0) || (duration > TIMEDBAN_MAX_TIME))
		return timedban_extban_syntax(sptr, checkt, "Invalid duration time");

	strlcpy(tmpmask, matchby, sizeof(tmpmask));
	timedban_extban_is_ok_recursion++;
	//res = extban_is_ok_nuh_extban(sptr, chptr, tmpmask, checkt, what, what2);
	res = generic_ban_is_ok(sptr, chptr, tmpmask, checkt, what, what2);
	timedban_extban_is_ok_recursion--;
	if (res == 0)
	{
		/* This could be anything ranging from:
		 * invalid n!u@h syntax, unknown (sub)extbantype,
		 * disabled extban type in conf, too much recursion, etc.
		 */
		return timedban_extban_syntax(sptr, checkt, "Invalid matcher");
	}

	return 1; /* OK */
}

/** Check if the user is currently banned */
int timedban_is_banned(aClient *sptr, aChannel *chptr, char *ban, int chktype)
{
	if (strncmp(ban, "~t:", 3))
		return 0; /* not for us */
	ban = strchr(ban+3, ':'); /* skip time argument */
	if (!ban)
		return 0; /* invalid fmt */
	ban++;

	return ban_check_mask(sptr, chptr, ban, chktype, 0);
}

/** Helper to check if the ban has been expired */
int timedban_has_ban_expired(Ban *ban)
{
char *banstr = ban->banstr;
char *p;
int t;
TS expire_on;

	if (strncmp(banstr, "~t:", 3))
		return 0; /* not for us */
	p = strchr(banstr+3, ':'); /* skip time argument */
	if (!p)
		return 0; /* invalid fmt */
	*p = '\0'; /* danger.. must restore!! */
	t = atoi(banstr+3);
	*p = ':'; /* restored.. */
	
	expire_on = ban->when + (t * 60) - TIMEDBAN_TIMER_DELTA;
	
	if (expire_on < TStime())
		return 1;
	return 0;
}

static char mbuf[512];
static char pbuf[512];

/** This removes any expired timedbans */
EVENT(timedban_timeout)
{
	aChannel *chptr;
	Ban *ban, *nextban;
	static int current_iteration = 0;

	if (++current_iteration >= TIMEDBAN_TIMER_ITERATION_SPLIT)
		current_iteration = 0;

	for (chptr = channel; chptr; chptr = chptr->nextch)
	{
		/* This is a very quick check, at the cost of it being
		 * biased since there's always a tendency of more channel
		 * names to start with one specific letter. But hashing
		 * is too costly. So we stick with this. It should be
		 * good enough. Alternative would be some chptr->id value.
		 */
		if (((unsigned int)chptr->chname[1] % TIMEDBAN_TIMER_ITERATION_SPLIT) != current_iteration)
			continue; /* not this time, maybe next */

		*mbuf = *pbuf = '\0';
		for (ban = chptr->banlist; ban; ban=nextban)
		{
			nextban = ban->next;
			if (!strncmp(ban->banstr, "~t:", 3) && timedban_has_ban_expired(ban))
			{
				add_send_mode_param(chptr, &me, '-',  'b', ban->banstr);
				del_listmode(&chptr->banlist, chptr, ban->banstr);
			}
		}
		for (ban = chptr->exlist; ban; ban=nextban)
		{
			nextban = ban->next;
			if (!strncmp(ban->banstr, "~t:", 3) && timedban_has_ban_expired(ban))
			{
				add_send_mode_param(chptr, &me, '-',  'e', ban->banstr);
				del_listmode(&chptr->exlist, chptr, ban->banstr);
			}
		}
		for (ban = chptr->invexlist; ban; ban=nextban)
		{
			nextban = ban->next;
			if (!strncmp(ban->banstr, "~t:", 3) && timedban_has_ban_expired(ban))
			{
				add_send_mode_param(chptr, &me, '-',  'I', ban->banstr);
				del_listmode(&chptr->invexlist, chptr, ban->banstr);
			}
		}
		if (*pbuf)
		{
			sendto_channel_butserv(chptr, &me, ":%s MODE %s %s %s", me.name, chptr->chname, mbuf, pbuf);
			sendto_server(NULL, 0, 0, ":%s MODE %s %s %s 0", me.name, chptr->chname, mbuf, pbuf);
			*pbuf = 0;
		}
	}
}

#if MODEBUFLEN > 512
 #error "add_send_mode_param() is not made for MODEBUFLEN > 512"
#endif

void add_send_mode_param(aChannel *chptr, aClient *from, char what, char mode, char *param) {
	static char *modes = NULL, lastwhat;
	static short count = 0;
	short send = 0;
	
	if (!modes) modes = mbuf;
	
	if (!mbuf[0]) {
		modes = mbuf;
		*modes++ = what;
		*modes = 0;
		lastwhat = what;
		*pbuf = 0;
		count = 0;
	}
	if (lastwhat != what) {
		*modes++ = what;
		*modes = 0;
		lastwhat = what;
	}
	if (strlen(pbuf) + strlen(param) + 11 < MODEBUFLEN) {
		if (*pbuf) 
			strcat(pbuf, " ");
		strcat(pbuf, param);
		*modes++ = mode;
		*modes = 0;
		count++;
	}
	else if (*pbuf) 
		send = 1;

	if (count == MAXMODEPARAMS)
		send = 1;

	if (send) {
		sendto_channel_butserv(chptr, &me, ":%s MODE %s %s %s", me.name, chptr->chname, mbuf, pbuf);
		sendto_server(NULL, 0, 0, ":%s MODE %s %s %s 0", me.name, chptr->chname, mbuf, pbuf);
		send = 0;
		*pbuf = 0;
		modes = mbuf;
		*modes++ = what;
		lastwhat = what;
		if (count != MAXMODEPARAMS) {
			strlcpy(pbuf, param, sizeof(pbuf));
			*modes++ = mode;
			count = 1;
		}
		else 
			count = 0;
		*modes = 0;
	}
}
