/*   src/modules/who_old.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers.
 *
 *   This program is free softwmare; you can redistribute it and/or modify
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

/* rewritten 06/02 by larne, the old one was unreadable. */
/* changed indentation + some parts rewritten by Syzop. */

#include "unrealircd.h"

CMD_FUNC(cmd_who);

/* Place includes here */
#define MSG_WHO 	"WHO"

ModuleHeader MOD_HEADER
  = {
	"who_old",	/* Name of module */
	"5.0", /* Version */
	"command /who (old version)", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* This is called on module init, before Server Ready */
MOD_INIT()
{
	if (!CommandAdd(modinfo->handle, MSG_WHO, cmd_who, MAXPARA, CMD_USER))
	{
		config_warn("You cannot load both the cmd_whox and cmd_who module. You should ONLY load the cmd_whox module.");
		return MOD_FAILED;
	}
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
MOD_LOAD()
{
	return MOD_SUCCESS;
}


/* Called when module is unloaded */
MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

static void do_channel_who(Client *client, Channel *channel, const char *mask);
static void make_who_status(Client *, Client *, Channel *, Member *, char *, int);
static void do_other_who(Client *client, const char *mask);
static void send_who_reply(Client *, Client *, const char *, const char *, const char *);
static const char *first_visible_channel(Client *, Client *, int *);
static int parse_who_options(Client *, int, const char **);
static void who_sendhelp(Client *);

#define WF_OPERONLY  0x01 /**< only show opers */
#define WF_ONCHANNEL 0x02 /**< we're on the channel we're /who'ing */
#define WF_WILDCARD  0x04 /**< a wildcard /who */
#define WF_REALHOST  0x08 /**< want real hostnames */
#define WF_IP	     0x10 /**< want IP addresses */

static int who_flags;

#define WHO_CANTSEE 0x01 /**< set if we can't see them */
#define WHO_CANSEE  0x02 /**< set if we can */
#define WHO_OPERSEE 0x04 /**< set if we only saw them because we're an oper */

#define FVC_HIDDEN  0x01

#define WHO_WANT 1
#define WHO_DONTWANT 2
#define WHO_DONTCARE 0

struct {
	int want_away;
	int want_channel;
	const char *channel; /**< if they want one */
	int want_gecos;
	const char *gecos;
	int want_server;
	const char *server;
	int want_host;
	const char *host;
	int want_nick;
	const char *nick;
	int want_user;
	const char *user;
	int want_ip;
	const char *ip;
	int want_port;
	int port;
	int want_umode;
	int umodes_dontwant;
	int umodes_want;
	int common_channels_only;
} wfl;

/** The /who command: retrieves information from users. */
CMD_FUNC(cmd_who)
{
	Channel *target_channel;
	const char *mask = parv[1];
	char maskbuf[512];
	int i = 0;

	if (!MyUser(client))
		return;

	who_flags = 0;
	memset(&wfl, 0, sizeof(wfl));

	if (parc > 1)
	{
		i = parse_who_options(client, parc - 1, parv + 1);
		if (i < 0)
		{
			sendnumeric(client, RPL_ENDOFWHO, mask);
			return;
		}
	}

	if (parc-i < 2 || strcmp(parv[1 + i], "0") == 0)
		mask = "*";
	else
		mask = parv[1 + i];

	if (!i && parc > 2 && *parv[2] == 'o')
		who_flags |= WF_OPERONLY;

	/* Pfff... collapse... hate it! */
	strlcpy(maskbuf, mask, sizeof(maskbuf));
	collapse(maskbuf);
	mask = maskbuf;

	if (*mask == '\0')
	{
		/* no mask given */
		sendnumeric(client, RPL_ENDOFWHO, "*");
		return;
	}

	if ((target_channel = find_channel(mask)) != NULL)
	{
		do_channel_who(client, target_channel, mask);
		sendnumeric(client, RPL_ENDOFWHO, mask);
		return;
	}

	if (wfl.channel && wfl.want_channel == WHO_WANT && 
	    (target_channel = find_channel(wfl.channel)) != NULL)
	{
		do_channel_who(client, target_channel, mask);
		sendnumeric(client, RPL_ENDOFWHO, mask);
		return;
	}
	else
	{
		do_other_who(client, mask);
		sendnumeric(client, RPL_ENDOFWHO, mask);
		return;
	}

	return;
}

static void who_sendhelp(Client *client)
{
  char *who_help[] = {
    "/WHO [+|-][achmnsuM] [args]",
    "Flags are specified like channel modes, the flags chmnsu all have arguments",
    "Flags are set to a positive check by +, a negative check by -",
    "The flags work as follows:",
    "Flag a: user is away",
    "Flag c <channel>:       user is on <channel>,",
    "                        no wildcards accepted",
    "Flag h <host>:          user has string <host> in their hostname,",
    "                        wildcards accepted",
    "Flag m <usermodes>:     user has <usermodes> set, only",
    "                        O/o/C/A/a/N/B are allowed",
    "Flag n <nick>:          user has string <nick> in their nickname,",
    "                        wildcards accepted",
    "Flag s <server>:        user is on server <server>,",
    "                        wildcards not accepted",
    "Flag u <user>:          user has string <user> in their username,",
    "                        wildcards accepted",
    "Behavior flags:",
    "Flag M: check for user in channels I am a member of",
    NULL
  };

  char *who_oper_help[] = {
    "/WHO [+|-][acghimnsuMRI] [args]",
    "Flags are specified like channel modes, the flags chigmnsu all have arguments",
    "Flags are set to a positive check by +, a negative check by -",
    "The flags work as follows:",
    "Flag a: user is away",
    "Flag c <channel>:       user is on <channel>,",
    "                        no wildcards accepted",
    "Flag g <gcos/realname>: user has string <gcos> in their GCOS,",
    "                        wildcards accepted",
    "Flag h <host>:          user has string <host> in their hostname,",
    "                        wildcards accepted",
    "Flag i <ip>:            user has string <ip> in their IP address,",
    "                        wildcards accepted",
    "Flag p <port>:          user is connecting on port <port>,",
    "                        local connections only",
    "Flag m <usermodes>:     user has <usermodes> set",
    "Flag n <nick>:          user has string <nick> in their nickname,",
    "                        wildcards accepted",
    "Flag s <server>:        user is on server <server>,",
    "                        wildcards not accepted",
    "Flag u <user>:          user has string <user> in their username,",
    "                        wildcards accepted",
    "Behavior flags:",
    "Flag M: check for user in channels I am a member of",
    "Flag R: show users' real hostnames",
    "Flag I: show users' IP addresses",
    NULL
  };
  char **s;

	if (IsOper(client))
		s = who_oper_help;
	else
		s = who_help;

	for (; *s; s++)
		sendnumeric(client, RPL_LISTSYNTAX, *s);
}

#define WHO_ADD 1
#define WHO_DEL 2

static int parse_who_options(Client *client, int argc, const char **argv)
{
	const char *s = argv[0];
	int what = WHO_ADD;
	int i = 1;

/* A few helper macro's because this is used a lot, added during recode by Syzop. */

/** function requiress a parameter: check if there's one, if not: return -1. */
#define REQUIRE_PARAM() { if (i >= argc) { \
                           who_sendhelp(client); \
                           return -1; \
                      } } while(0);
/** set option 'x' depending on 'what' (add/want or del/dontwant) */
#define SET_OPTION(x) { if (what == WHO_ADD) \
                           x = WHO_WANT; \
                      else \
                           x = WHO_DONTWANT; \
                      } while(0);
/** Eat a param, set the param in memory and set the option to want or dontwant */
#define DOIT(x,y) { REQUIRE_PARAM(); x = argv[i]; SET_OPTION(y); i++; } while(0);

	if (*s != '-' && *s != '+')
		return 0;

	while (*s)
 	{
		switch (*s)
		{
			case '+':
	  			what = WHO_ADD;
	  			break;
			case '-':
				what = WHO_DEL;
				break;
			case 'a':
				SET_OPTION(wfl.want_away);
				break;
			case 'c':
				DOIT(wfl.channel, wfl.want_channel);
				break;
			case 'g':
				REQUIRE_PARAM()
				if (!IsOper(client))
					break; /* oper-only */
				wfl.gecos = argv[i];
				SET_OPTION(wfl.want_gecos);
				i++;
				break;
			case 's':
				DOIT(wfl.server, wfl.want_server);
				break;
			case 'h':
				DOIT(wfl.host, wfl.want_host);
				break;
			case 'i':
				REQUIRE_PARAM()
				if (!IsOper(client))
					break; /* oper-only */
				wfl.ip = argv[i];
				SET_OPTION(wfl.want_ip);
				i++;
				break;
			case 'n':
				DOIT(wfl.nick, wfl.want_nick);
				break;
			case 'u':
				DOIT(wfl.user, wfl.want_user);
				break;
			case 'm':
				REQUIRE_PARAM()
				{
					const char *s = argv[i];
					int *umodes;

					if (what == WHO_ADD)
						umodes = &wfl.umodes_want;
					else
						umodes = &wfl.umodes_dontwant;

					*umodes = set_usermode(s);

					if (!IsOper(client))
						*umodes = *umodes & UMODE_OPER; /* these are usermodes regular users may search for. just oper now. */
					if (*umodes == 0)
						return -1;
				}
				i++;
				break;
			case 'p':
				REQUIRE_PARAM()
				if (!IsOper(client))
					break; /* oper-only */
				wfl.port = atoi(argv[i]);
				SET_OPTION(wfl.want_port);
				i++;
				break;
			case 'M':
				SET_OPTION(wfl.common_channels_only);
				break;
			case 'R':
				if (!IsOper(client))
					break;
				if (what == WHO_ADD)
					who_flags |= WF_REALHOST;
				else
					who_flags &= ~WF_REALHOST;
				break;
			case 'I':
				if (!IsOper(client))
					break;
				if (what == WHO_ADD)
					who_flags |= WF_IP;
				
				else
					who_flags &= ~WF_IP;
				break;
			default:
				who_sendhelp(client);
				return -1;
		}
		s++;
    }

  return i;
#undef REQUIRE_PARAM
#undef SET_OPTION
#undef DOIT
}

static int can_see(Client *requester, Client *target, Channel *channel)
{
	int ret = 0;
	char has_common_chan = 0;

	do {
		/* can only see people */
		if (!IsUser(target))
			return WHO_CANTSEE;

		/* can only see opers if thats what they want */
		if (who_flags & WF_OPERONLY)
		{
			if (!IsOper(target))
				return ret | WHO_CANTSEE;
			if (IsHideOper(target)) {
				if (IsOper(requester))
					ret |= WHO_OPERSEE;
				else
					return ret | WHO_CANTSEE;
			}
		}

		/* if they only want people who are away */
		if ((wfl.want_away == WHO_WANT && !target->user->away) ||
		    (wfl.want_away == WHO_DONTWANT && target->user->away))
			return WHO_CANTSEE;

		/* if they only want people on a certain channel. */
		if (wfl.want_channel != WHO_DONTCARE)
 		{
			Channel *chan = find_channel(wfl.channel);
			if (!chan && wfl.want_channel == WHO_WANT)
				return WHO_CANTSEE;
			if ((wfl.want_channel == WHO_WANT) && !IsMember(target, chan))
				return WHO_CANTSEE;
			if ((wfl.want_channel == WHO_DONTWANT) && IsMember(target, chan))
				return WHO_CANTSEE;
		}

		/* if they only want people with a certain gecos */
		if (wfl.want_gecos != WHO_DONTCARE)
		{
			if (((wfl.want_gecos == WHO_WANT) && !match_simple(wfl.gecos, target->info)) ||
			    ((wfl.want_gecos == WHO_DONTWANT) && match_simple(wfl.gecos, target->info)))
			{
				return WHO_CANTSEE;
			}
		}

		/* if they only want people with a certain server */
		if (wfl.want_server != WHO_DONTCARE)
		{
			if (((wfl.want_server == WHO_WANT) && strcasecmp(wfl.server, target->user->server)) ||
			    ((wfl.want_server == WHO_DONTWANT) && !strcasecmp(wfl.server, target->user->server)))
			{
				return WHO_CANTSEE;
			}
		}

		/* if they only want people with a certain host */
		if (wfl.want_host != WHO_DONTCARE)
		{
			char *host;

			if (IsOper(requester))
				host = target->user->realhost;
			else
				host = GetHost(target);

			if (((wfl.want_host == WHO_WANT) && !match_simple(wfl.host, host)) ||
			    ((wfl.want_host == WHO_DONTWANT) && match_simple(wfl.host, host)))
			{
				return WHO_CANTSEE;
			}
		}

		/* if they only want people with a certain IP */
		if (wfl.want_ip != WHO_DONTCARE)
		{
			char *ip;

			ip = target->ip;
			if (!ip)
				return WHO_CANTSEE;

			if (((wfl.want_ip == WHO_WANT) && !match_simple(wfl.ip, ip)) ||
			    ((wfl.want_ip == WHO_DONTWANT) && match_simple(wfl.ip, ip)))
			{
				return WHO_CANTSEE;
			}
		}

		/* if they only want people connecting on a certain port */
		if (wfl.want_port != WHO_DONTCARE)
		{
			int port;
			
			if (!MyUser(target))
				return WHO_CANTSEE;

			port = target->local->listener->port;

			if (((wfl.want_port == WHO_WANT) && wfl.port != port) ||
			    ((wfl.want_port == WHO_DONTWANT) && wfl.port == port))
			{
				return WHO_CANTSEE;
			}
		}

		/* if they only want people with a certain nick.. */
		if (wfl.want_nick != WHO_DONTCARE)
		{
			if (((wfl.want_nick == WHO_WANT) && !match_simple(wfl.nick, target->name)) ||
			    ((wfl.want_nick == WHO_DONTWANT) && match_simple(wfl.nick, target->name)))
			{
				return WHO_CANTSEE;
			}
		}

		/* if they only want people with a certain username */
		if (wfl.want_user != WHO_DONTCARE)
		{
			if (((wfl.want_user == WHO_WANT) && !match_simple(wfl.user, target->user->username)) ||
			    ((wfl.want_user == WHO_DONTWANT) && match_simple(wfl.user, target->user->username)))
			{
				return WHO_CANTSEE;
			}
		}

		/* if they only want people with a certain umode */
		if (wfl.umodes_want)
		{
			if (!(target->umodes & wfl.umodes_want) || (!IsOper(requester) && (target->umodes & UMODE_HIDEOPER)))
				return WHO_CANTSEE;
		}

		if (wfl.umodes_dontwant)
		{
			if ((target->umodes & wfl.umodes_dontwant) && (!(target->umodes & UMODE_HIDEOPER) || IsOper(requester)))
				return WHO_CANTSEE;
		}

		/* if they only want common channels */
		if (wfl.common_channels_only)
		{
			if (!has_common_channels(requester, target))
				return WHO_CANTSEE;
			has_common_chan = 1;
		}

		if (channel)
		{
			int member = who_flags & WF_ONCHANNEL;

			if (SecretChannel(channel) || HiddenChannel(channel))
			{
				/* if they aren't on it.. they can't see it */
				if (!(who_flags & WF_ONCHANNEL))
					break;
			}
			if (IsInvisible(target) && !member)
				break;

			if (!user_can_see_member(requester, target, channel))
				break; /* invisible (eg: due to delayjoin) */
		}
		else
		{
			/* a user/mask who */

			/* If the common channel info hasn't been set, set it now */
			if (!wfl.common_channels_only)
				has_common_chan = has_common_channels(requester, target);

			if (IsInvisible(target) && !has_common_chan)
			{
				/* don't show them unless it's an exact match 
				   or it is the user requesting the /who */
				if ((who_flags & WF_WILDCARD) && requester != target)
					break;
			}
		}

		/* phew.. show them. */
		return WHO_CANSEE;
	} while (0);

	/* if we get here, it's oper-dependant. */
	if (IsOper(requester))
		return ret | WHO_OPERSEE | WHO_CANSEE;
	else
	{
		if (requester == target)
			return ret | WHO_CANSEE;
		else
			return ret | WHO_CANTSEE;
	}
}

static void do_channel_who(Client *client, Channel *channel, const char *mask)
{
	Member *cm = channel->members;
	if (IsMember(client, channel) || ValidatePermissionsForPath("channel:see:who:onchannel",client,NULL,channel,NULL))
		who_flags |= WF_ONCHANNEL;

	for (cm = channel->members; cm; cm = cm->next)
	{
		Client *acptr = cm->client;
		char status[32];
		int cansee;
		if ((cansee = can_see(client, acptr, channel)) & WHO_CANTSEE)
			continue;

		make_who_status(client, acptr, channel, cm, status, cansee);
		send_who_reply(client, acptr, channel->name, status, "");
    }
}

static void make_who_status(Client *client, Client *acptr, Channel *channel, 
			    Member *cm, char *status, int cansee)
{
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
		int ret = (*(h->func.intfunc))(client, acptr, channel, cm, status, cansee);
		if (ret != 0)
			status[i++] = (char)ret;
	}
	
	if (IsOper(acptr) && (!IsHideOper(acptr) || client == acptr || IsOper(client)))
		status[i++] = '*';

	if (IsOper(acptr) && (IsHideOper(acptr) && client != acptr && IsOper(client)))
		status[i++] = '!';
  
	if (cansee & WHO_OPERSEE)
		status[i++] = '?';

	if (cm)
	{
		if (HasCapability(client, "multi-prefix"))
		{
			/* Standard NAMES reply (single character) */
			char c = mode_to_prefix(*cm->member_modes);
			if (c)
				status[i++] = c;
		}
		else
		{
			/* NAMES reply with all rights included (multi-prefix / NAMESX) */
			strcpy(&status[i], modes_to_prefix(cm->member_modes));
			i += strlen(&status[i]);
		}
	}

	status[i] = '\0';
}

static void do_other_who(Client *client, const char *mask)
{
int oper = IsOper(client);

	if (strchr(mask, '*') || strchr(mask, '?'))
	{
		int i = 0;
		/* go through all users.. */
		Client *acptr;
		who_flags |= WF_WILDCARD;

		list_for_each_entry(acptr, &client_list, client_node)
		{
		int cansee;
		char status[20];
		const char *channel;
		int flg;

			if (!IsUser(acptr))
				continue;
			if (!oper) {
				/* non-opers can only search on nick here */
				if (!match_simple(mask, acptr->name))
					continue;
			} else {
				/* opers can search on name, ident, virthost, ip and realhost.
				 * Yes, I like readable if's -- Syzop.
				 */
				if (match_simple(mask, acptr->name) || match_simple(mask, acptr->user->realhost) ||
				    match_simple(mask, acptr->user->username))
					goto matchok;
				if (IsHidden(acptr) && match_simple(mask, acptr->user->virthost))
					goto matchok;
				if (acptr->ip && match_simple(mask, acptr->ip))
					goto matchok;
				/* nothing matched... */
				continue;
			}
matchok:
			if ((cansee = can_see(client, acptr, NULL)) & WHO_CANTSEE)
				continue;
			if (WHOLIMIT && !IsOper(client) && ++i > WHOLIMIT)
			{
				sendnumeric(client, ERR_WHOLIMEXCEED, WHOLIMIT);
				return;
			}

			channel = first_visible_channel(client, acptr, &flg);
			make_who_status(client, acptr, NULL, NULL, status, cansee);
			send_who_reply(client, acptr, channel, status, (flg & FVC_HIDDEN) ? "~" : "");
		}
	}
	else
	{
		/* just a single client (no wildcards detected) */
		Client *acptr = find_client(mask, NULL);
		int cansee;
		char status[20];
		const char *channel;
		int flg;

		if (!acptr)
			return;

		if ((cansee = can_see(client, acptr, NULL)) == WHO_CANTSEE)
			return;

		channel = first_visible_channel(client, acptr, &flg);
		make_who_status(client, acptr, NULL, NULL, status, cansee);
		send_who_reply(client, acptr, channel, status, (flg & FVC_HIDDEN) ? "~" : "");
	}
}

static void send_who_reply(Client *client, Client *acptr, 
			   const char *channel, const char *status, const char *xstat)
{
	char *stat;
	const char *host;
	int flat = (FLAT_MAP && !IsOper(client)) ? 1 : 0;

	stat = safe_alloc(strlen(status) + strlen(xstat) + 1);
	sprintf(stat, "%s%s", status, xstat);

	if (IsOper(client))
	{
		if (who_flags & WF_REALHOST)
			host = acptr->user->realhost;
		else if (who_flags & WF_IP)
			host = (acptr->ip ? acptr->ip : acptr->user->realhost);
		else
			host = GetHost(acptr);
	}
	else
		host = GetHost(acptr);
					

	if (IsULine(acptr) && !IsOper(client) && !ValidatePermissionsForPath("server:info:map:ulines",client,acptr,NULL,NULL) && HIDE_ULINES)
	{
	        sendnumeric(client, RPL_WHOREPLY,
        	     channel,       /* channel name */
	             acptr->user->username, /* user name */
        	     host,		    /* hostname */
	             "hidden",              /* let's hide the server from normal users if the server is a uline and HIDE_ULINES is on */
        	     acptr->name,           /* nick */
	             stat,                  /* status */
        	     0,                     /* hops (hidden) */
	             acptr->info            /* realname */
             	);

	} else {
		sendnumeric(client, RPL_WHOREPLY,
		     channel,       /* channel name */
		     acptr->user->username,      /* user name */
		     host,		         /* hostname */
		     acptr->user->server,        /* server name */
		     acptr->name,                /* nick */
		     stat,                       /* status */
		     flat ? 0 : acptr->hopcount, /* hops */ 
		     acptr->info                 /* realname */
		     );
	}
	safe_free(stat);
}

static const char *first_visible_channel(Client *client, Client *acptr, int *flg)
{
	Membership *lp;

	*flg = 0;

	for (lp = acptr->user->channel; lp; lp = lp->next)
	{
		Channel *channel = lp->channel;
		Hook *h;
		int ret = EX_ALLOW;
		int operoverride = 0;
		int showchannel = 0;
		
		/* Note that the code below is almost identical to the one in /WHOIS */

		if (ShowChannel(client, channel))
			showchannel = 1;

		for (h = Hooks[HOOKTYPE_SEE_CHANNEL_IN_WHOIS]; h; h = h->next)
		{
			int n = (*(h->func.intfunc))(client, acptr, channel);
			/* Hook return values:
			 * EX_ALLOW means 'yes is ok, as far as modules are concerned'
			 * EX_DENY means 'hide this channel, unless oper overriding'
			 * EX_ALWAYS_DENY means 'hide this channel, always'
			 * ... with the exception that we always show the channel if you /WHOIS yourself
			 */
			if (n == EX_DENY)
			{
				ret = EX_DENY;
			}
			else if (n == EX_ALWAYS_DENY)
			{
				ret = EX_ALWAYS_DENY;
				break;
			}
		}
		
		if (ret == EX_DENY)
			showchannel = 0;
		
		if (!showchannel && (ValidatePermissionsForPath("channel:see:who:secret",client,NULL,channel,NULL) || ValidatePermissionsForPath("channel:see:whois",client,NULL,channel,NULL)))
		{
			showchannel = 1; /* OperOverride */
			operoverride = 1;
		}
		
		if ((ret == EX_ALWAYS_DENY) && (acptr != client))
			continue; /* a module asked us to really not expose this channel, so we don't (except target==ourselves). */

		if (acptr == client)
			showchannel = 1;

		if (operoverride)
			*flg |= FVC_HIDDEN;

		if (showchannel)
			return channel->name;
	}

	/* no channels that they can see */
	return "*";
}
