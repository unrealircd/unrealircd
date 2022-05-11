/*
 * timedban - Timed bans that are automatically unset.
 * (C) Copyright 2009-2017 Bram Matthys (Syzop) and the UnrealIRCd team.
 * License: GPLv2 or later
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

ModuleHeader MOD_HEADER
  = {
	"extbans/timedban",
	"1.0",
	"ExtBan ~t: automatically removed timed bans",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Forward declarations */
const char *timedban_extban_conv_param(BanContext *b, Extban *extban);
int timedban_extban_is_ok(BanContext *b);
int timedban_is_banned(BanContext *b);
void add_send_mode_param(Channel *channel, Client *from, char what, char mode, char *param);
char *timedban_chanmsg(Client *, Client *, Channel *, char *, int);

EVENT(timedban_timeout);

MOD_TEST()
{
	return MOD_SUCCESS;
}

MOD_INIT()
{
	ExtbanInfo extban;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&extban, 0, sizeof(ExtbanInfo));
	extban.letter = 't';
	extban.name = "time";
	extban.options |= EXTBOPT_ACTMODIFIER; /* not really, but ours shouldn't be stacked from group 1 */
	extban.options |= EXTBOPT_INVEX; /* also permit timed invite-only exceptions (+I) */
	extban.conv_param = timedban_extban_conv_param;
	extban.is_ok = timedban_extban_is_ok;
	extban.is_banned = timedban_is_banned;
	extban.is_banned_events = BANCHK_ALL;

	if (!ExtbanAdd(modinfo->handle, extban))
	{
		config_error("timedban: unable to register 't' extban type!!");
		return MOD_FAILED;
	}
                
	EventAdd(modinfo->handle, "timedban_timeout", timedban_timeout, NULL, TIMEDBAN_TIMER*1000, 0);

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

/** Generic helper for our conv_param extban function.
 * Mostly copied from clean_ban_mask()
 * FIXME: Figure out why we have this one at all and not use conv_param? ;)
 */
const char *generic_clean_ban_mask(BanContext *b, Extban *extban)
{
	char *cp, *x;
	char *user;
	char *host;
	static char maskbuf[512];
	char *mask;

	/* Work on a copy */
	strlcpy(maskbuf, b->banstr, sizeof(maskbuf));
	mask = maskbuf;

	cp = strchr(mask, ' ');
	if (cp)
		*cp = '\0';

	/* Strip any ':' at beginning since that would cause a desync */
	for (; (*mask && (*mask == ':')); mask++);
	if (!*mask)
		return NULL;

	/* Forbid ASCII <= 32 in all bans */
	for (x = mask; *x; x++)
		if (*x <= ' ')
			return NULL;

	/* Extended ban? */
	if (is_extended_ban(mask))
	{
		const char *nextbanstr;
		Extban *extban = findmod_by_bantype(mask, &nextbanstr);
		if (!extban)
			return NULL; /* reject unknown extban */
		if (extban->conv_param)
		{
			const char *ret;
			static char retbuf[512];
			BanContext *newb = safe_alloc(sizeof(BanContext));
			newb->banstr = nextbanstr;
			newb->conv_options = b->conv_options;
			ret = extban->conv_param(newb, extban);
			ret = prefix_with_extban(ret, newb, extban, retbuf, sizeof(retbuf));
			safe_free(newb);
			return ret;
		}
		/* else, do some basic sanity checks and cut it off at 80 bytes */
		if ((mask[1] != ':') || (mask[2] == '\0'))
		    return NULL; /* require a ":<char>" after extban type */
		if (strlen(mask) > 80)
			mask[80] = '\0';
		return mask;
	}

	if ((*mask == '~') && !strchr(mask, '@'))
		return NULL; /* not an extended ban and not a ~user@host ban either. */

	if ((user = strchr((cp = mask), '!')))
		*user++ = '\0';
	if ((host = strrchr(user ? user : cp, '@')))
	{
		*host++ = '\0';

		if (!user)
			return make_nick_user_host(NULL, trim_str(cp,USERLEN), 
				trim_str(host,HOSTLEN));
	}
	else if (!user && strchr(cp, '.'))
		return make_nick_user_host(NULL, NULL, trim_str(cp,HOSTLEN));
	return make_nick_user_host(trim_str(cp,NICKLEN), trim_str(user,USERLEN), 
		trim_str(host,HOSTLEN));
}

/** Convert ban to an acceptable format (or return NULL to fully reject it) */
const char *timedban_extban_conv_param(BanContext *b, Extban *extban)
{
	static char retbuf[MAX_LENGTH+1];
	char para[MAX_LENGTH+1];
	char tmpmask[MAX_LENGTH+1];
	char *durationstr; /**< Duration, such as '5' */
	int duration;
	char *matchby; /**< Matching method, such as 'n!u@h' */
	const char *newmask; /**< Cleaned matching method, such as 'n!u@h' */
	static int timedban_extban_conv_param_recursion = 0;
	
	if (timedban_extban_conv_param_recursion)
		return NULL; /* reject: recursion detected! */

	strlcpy(para, b->banstr, sizeof(para)); /* work on a copy (and truncate it) */
	
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
	b->banstr = matchby; // this was previously 'tmpmask' but then it's a copy-copy-copy.. :D
	newmask = generic_clean_ban_mask(b, extban);
	timedban_extban_conv_param_recursion--;
	if (!newmask || (strlen(newmask) <= 1))
		return NULL;

	//snprintf(retbuf, sizeof(retbuf), "~t:%d:%s", duration, newmask);
	snprintf(retbuf, sizeof(retbuf), "%d:%s", duration, newmask);
	return retbuf;
}

int timedban_extban_syntax(Client *client, int checkt, char *reason)
{
	if (MyUser(client) && (checkt == EXBCHK_PARAM))
	{
		sendnotice(client, "Error when setting timed ban: %s", reason);
		sendnotice(client, " Syntax: +b ~t:duration:mask");
		sendnotice(client, "Example: +b ~t:5:nick!user@host");
		sendnotice(client, "Duration is the time in minutes after which the ban is removed (1-9999)");
		sendnotice(client, "Valid masks are: nick!user@host or another extban type such as ~a, ~c, ~S, ..");
	}
	return 0; /* FAIL: ban rejected */
}

/** Generic helper for sub-bans, used by our "is this ban ok?" function */
int generic_ban_is_ok(BanContext *b)
{
	if ((b->banstr[0] == '~') && MyUser(b->client))
	{
		Extban *extban;
		const char *nextbanstr;

		/* This portion is copied from clean_ban_mask() */
		if (is_extended_ban(b->banstr) && MyUser(b->client))
		{
			if (RESTRICT_EXTENDEDBANS && !ValidatePermissionsForPath("immune:restrict-extendedbans",b->client,NULL,NULL,NULL))
			{
				if (!strcmp(RESTRICT_EXTENDEDBANS, "*"))
				{
					if (b->is_ok_check == EXBCHK_ACCESS_ERR)
						sendnotice(b->client, "Setting/removing of extended bans has been disabled");
					return 0; /* REJECT */
				}
				if (strchr(RESTRICT_EXTENDEDBANS, b->banstr[1]))
				{
					if (b->is_ok_check == EXBCHK_ACCESS_ERR)
						sendnotice(b->client, "Setting/removing of extended bantypes '%s' has been disabled", RESTRICT_EXTENDEDBANS);
					return 0; /* REJECT */
				}
			}
			/* And next is inspired by cmd_mode */
			extban = findmod_by_bantype(b->banstr, &nextbanstr);
			if (extban && extban->is_ok)
			{
				b->banstr = nextbanstr;
				if ((b->is_ok_check == EXBCHK_ACCESS) || (b->is_ok_check == EXBCHK_ACCESS_ERR))
				{
					if (!extban->is_ok(b) &&
					    !ValidatePermissionsForPath("channel:override:mode:extban",b->client,NULL,b->channel,NULL))
					{
						return 0; /* REJECT */
					}
				} else
				if (b->is_ok_check == EXBCHK_PARAM)
				{
					if (!extban->is_ok(b))
					{
						return 0; /* REJECT */
					}
				}
			}
		}
	}
	
	/* ACCEPT:
	 * - not an extban; OR
	 * - extban with NULL is_ok; OR
	 * - non-existing extban character (handled by conv_param?)
	 */
	return 1;
}

/** Validate ban ("is this ban ok?") */
int timedban_extban_is_ok(BanContext *b)
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
	if (b->what == MODE_DEL)
		return 1;

	if (timedban_extban_is_ok_recursion)
		return 0; /* Recursion detected (~t:1:~t:....) */

	strlcpy(para, b->banstr, sizeof(para)); /* work on a copy (and truncate it) */
	
	/* ~t:duration:n!u@h   for direct matching
	 * ~t:duration:~x:.... when calling another bantype
	 */

	durationstr = para;
	matchby = strchr(para, ':');
	if (!matchby || !matchby[1])
		return timedban_extban_syntax(b->client, b->is_ok_check, "Invalid syntax");
	*matchby++ = '\0';

	duration = atoi(durationstr);

	if ((duration <= 0) || (duration > TIMEDBAN_MAX_TIME))
		return timedban_extban_syntax(b->client, b->is_ok_check, "Invalid duration time");

	strlcpy(tmpmask, matchby, sizeof(tmpmask));
	timedban_extban_is_ok_recursion++;
	//res = extban_is_ok_nuh_extban(b->client, b->channel, tmpmask, b->is_ok_check, b->what, b->ban_type);
	b->banstr = tmpmask;
	res = generic_ban_is_ok(b);
	timedban_extban_is_ok_recursion--;
	if (res == 0)
	{
		/* This could be anything ranging from:
		 * invalid n!u@h syntax, unknown (sub)extbantype,
		 * disabled extban type in conf, too much recursion, etc.
		 */
		return timedban_extban_syntax(b->client, b->is_ok_check, "Invalid matcher");
	}

	return 1; /* OK */
}

/** Check if the user is currently banned */
int timedban_is_banned(BanContext *b)
{
	b->banstr = strchr(b->banstr, ':'); /* skip time argument */
	if (!b->banstr)
		return 0; /* invalid fmt */
	b->banstr++; /* skip over final semicolon */

	return ban_check_mask(b);
}

/** Helper to check if the ban has been expired.
 */
int timedban_has_ban_expired(Ban *ban)
{
	char *banstr = ban->banstr;
	char *p1, *p2;
	int t;
	time_t expire_on;

	/* The caller has only performed a very light check (string starting
	 * with ~t, in the interest of performance), so we don't know yet if
	 * it REALLY is a timed ban. We check that first here...
	 */
	if (!strncmp(banstr, "~t:", 3))
		p1 = banstr + 3;
	else if (!strncmp(banstr, "~time:", 6))
		p1 = banstr + 6;
	else
		return 0; /* not for us */
	p2 = strchr(p1+1, ':'); /* skip time argument */
	if (!p2)
		return 0; /* invalid fmt */
	*p2 = '\0'; /* danger.. must restore!! */
	t = atoi(p1);
	*p2 = ':'; /* restored.. */
	
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
	Channel *channel;
	Ban *ban, *nextban;
	static int current_iteration = 0;

	if (++current_iteration >= TIMEDBAN_TIMER_ITERATION_SPLIT)
		current_iteration = 0;

	for (channel = channels; channel; channel = channel->nextch)
	{
		/* This is a very quick check, at the cost of it being
		 * biased since there's always a tendency of more channel
		 * names to start with one specific letter. But hashing
		 * is too costly. So we stick with this. It should be
		 * good enough. Alternative would be some channel->id value.
		 */
		if (((unsigned int)channel->name[1] % TIMEDBAN_TIMER_ITERATION_SPLIT) != current_iteration)
			continue; /* not this time, maybe next */

		*mbuf = *pbuf = '\0';
		for (ban = channel->banlist; ban; ban=nextban)
		{
			nextban = ban->next;
			if (!strncmp(ban->banstr, "~t", 2) && timedban_has_ban_expired(ban))
			{
				add_send_mode_param(channel, &me, '-',  'b', ban->banstr);
				del_listmode(&channel->banlist, channel, ban->banstr);
			}
		}
		for (ban = channel->exlist; ban; ban=nextban)
		{
			nextban = ban->next;
			if (!strncmp(ban->banstr, "~t", 2) && timedban_has_ban_expired(ban))
			{
				add_send_mode_param(channel, &me, '-',  'e', ban->banstr);
				del_listmode(&channel->exlist, channel, ban->banstr);
			}
		}
		for (ban = channel->invexlist; ban; ban=nextban)
		{
			nextban = ban->next;
			if (!strncmp(ban->banstr, "~t", 2) && timedban_has_ban_expired(ban))
			{
				add_send_mode_param(channel, &me, '-',  'I', ban->banstr);
				del_listmode(&channel->invexlist, channel, ban->banstr);
			}
		}
		if (*pbuf)
		{
			MessageTag *mtags = NULL;
			new_message(&me, NULL, &mtags);
			sendto_channel(channel, &me, NULL, 0, 0, SEND_LOCAL, mtags, ":%s MODE %s %s %s", me.name, channel->name, mbuf, pbuf);
			sendto_server(NULL, 0, 0, mtags, ":%s MODE %s %s %s 0", me.id, channel->name, mbuf, pbuf);
			free_message_tags(mtags);
			*pbuf = 0;
		}
	}
}

#if MODEBUFLEN > 512
 #error "add_send_mode_param() is not made for MODEBUFLEN > 512"
#endif

void add_send_mode_param(Channel *channel, Client *from, char what, char mode, char *param) {
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

	if (send)
	{
		MessageTag *mtags = NULL;

		new_message(&me, NULL, &mtags);
		sendto_channel(channel, &me, NULL, 0, 0, SEND_LOCAL, mtags, ":%s MODE %s %s %s", me.name, channel->name, mbuf, pbuf);
		sendto_server(NULL, 0, 0, mtags, ":%s MODE %s %s %s 0", me.id, channel->name, mbuf, pbuf);
		free_message_tags(mtags);
		send = 0;
		*pbuf = 0;
		modes = mbuf;
		*modes++ = what;
		lastwhat = what;
		if (count != MAXMODEPARAMS)
		{
			strlcpy(pbuf, param, sizeof(pbuf));
			*modes++ = mode;
			count = 1;
		} else {
			count = 0;
		}
		*modes = 0;
	}
}
