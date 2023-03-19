/*
 *   Unreal Internet Relay Chat Daemon, src/modules/whois.c
 *   (C) 2000-2001 Carsten V. Munk and the UnrealIRCd Team
 *   (C) 2003-2021 Bram Matthys and the UnrealIRCd team
 *   Moved to modules by Fish (Justin Hammond) in 2001
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

/* Structs */
ModuleHeader MOD_HEADER
  = {
	"whois",	/* Name of module */
	"5.0", /* Version */
	"command /whois", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-6",
    };

typedef enum WhoisConfigUser {
	WHOIS_CONFIG_USER_EVERYONE	= 1,
	WHOIS_CONFIG_USER_SELF		= 2,
	WHOIS_CONFIG_USER_OPER		= 3,
} WhoisConfigUser;
#define HIGHEST_WHOIS_CONFIG_USER_VALUE 3 /* adjust this if you edit the enum above !! */

//this one is in include/struct.h because it needs full API exposure:
//typedef enum WhoisConfigDetails {
//	...
//} WhoisConfigDetails;
//

typedef struct WhoisConfig WhoisConfig;
struct WhoisConfig {
	WhoisConfig *prev, *next;
	char *name;
	WhoisConfigDetails permissions[HIGHEST_WHOIS_CONFIG_USER_VALUE+1];
};

/* Global variables */
WhoisConfig *whoisconfig = NULL;

/* Forward declarations */
WhoisConfigDetails _whois_get_policy(Client *client, Client *target, const char *name);
CMD_FUNC(cmd_whois);
static int whois_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
static int whois_config_run(ConfigFile *cf, ConfigEntry *ce, int type);
static void whois_config_setdefaults(void);

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAdd(modinfo->handle, EFUNC_WHOIS_GET_POLICY, TO_INTFUNC(_whois_get_policy));
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, whois_config_test);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	CommandAdd(modinfo->handle, "WHOIS", cmd_whois, MAXPARA, CMD_USER);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, whois_config_run);
	whois_config_setdefaults();
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

static WhoisConfig *find_whois_config(const char *name)
{
	WhoisConfig *w;
	for (w = whoisconfig; w; w = w->next)
		if (!strcmp(w->name, name))
			return w;
	return NULL;
}

/* Lazy helper for whois_config_setdefaults */
static void whois_config_add(const char *name, WhoisConfigUser user, WhoisConfigDetails details)
{
	WhoisConfig *w = find_whois_config(name);

	if (!w)
	{
		/* New one */
		w = safe_alloc(sizeof(WhoisConfig));
		safe_strdup(w->name, name);
		AddListItem(w, whoisconfig);
	}
	w->permissions[user] = details;
}

static void whois_config_setdefaults(void)
{
	whois_config_add("basic", WHOIS_CONFIG_USER_EVERYONE, WHOIS_CONFIG_DETAILS_FULL);

	whois_config_add("modes", WHOIS_CONFIG_USER_SELF, WHOIS_CONFIG_DETAILS_FULL);
	whois_config_add("modes", WHOIS_CONFIG_USER_OPER, WHOIS_CONFIG_DETAILS_FULL);

	whois_config_add("realhost", WHOIS_CONFIG_USER_SELF, WHOIS_CONFIG_DETAILS_FULL);
	whois_config_add("realhost", WHOIS_CONFIG_USER_OPER, WHOIS_CONFIG_DETAILS_FULL);

	whois_config_add("registered-nick", WHOIS_CONFIG_USER_EVERYONE, WHOIS_CONFIG_DETAILS_FULL);

	whois_config_add("channels", WHOIS_CONFIG_USER_EVERYONE, WHOIS_CONFIG_DETAILS_LIMITED);
	whois_config_add("channels", WHOIS_CONFIG_USER_SELF, WHOIS_CONFIG_DETAILS_FULL);
	whois_config_add("channels", WHOIS_CONFIG_USER_OPER, WHOIS_CONFIG_DETAILS_FULL);

	whois_config_add("server", WHOIS_CONFIG_USER_EVERYONE, WHOIS_CONFIG_DETAILS_FULL);

	whois_config_add("away", WHOIS_CONFIG_USER_EVERYONE, WHOIS_CONFIG_DETAILS_FULL);

	whois_config_add("oper", WHOIS_CONFIG_USER_EVERYONE, WHOIS_CONFIG_DETAILS_LIMITED);
	whois_config_add("oper", WHOIS_CONFIG_USER_SELF, WHOIS_CONFIG_DETAILS_FULL);
	whois_config_add("oper", WHOIS_CONFIG_USER_OPER, WHOIS_CONFIG_DETAILS_FULL);

	whois_config_add("secure", WHOIS_CONFIG_USER_EVERYONE, WHOIS_CONFIG_DETAILS_LIMITED);
	whois_config_add("secure", WHOIS_CONFIG_USER_SELF, WHOIS_CONFIG_DETAILS_FULL);
	whois_config_add("secure", WHOIS_CONFIG_USER_OPER, WHOIS_CONFIG_DETAILS_FULL);

	whois_config_add("bot", WHOIS_CONFIG_USER_EVERYONE, WHOIS_CONFIG_DETAILS_FULL);

	whois_config_add("services", WHOIS_CONFIG_USER_EVERYONE, WHOIS_CONFIG_DETAILS_FULL);

	whois_config_add("reputation", WHOIS_CONFIG_USER_OPER, WHOIS_CONFIG_DETAILS_FULL);

	whois_config_add("security-groups", WHOIS_CONFIG_USER_OPER, WHOIS_CONFIG_DETAILS_FULL);

	whois_config_add("geo", WHOIS_CONFIG_USER_OPER, WHOIS_CONFIG_DETAILS_FULL);

	whois_config_add("certfp", WHOIS_CONFIG_USER_EVERYONE, WHOIS_CONFIG_DETAILS_FULL);

	whois_config_add("shunned", WHOIS_CONFIG_USER_OPER, WHOIS_CONFIG_DETAILS_FULL);

	whois_config_add("account", WHOIS_CONFIG_USER_EVERYONE, WHOIS_CONFIG_DETAILS_FULL);

	whois_config_add("swhois", WHOIS_CONFIG_USER_EVERYONE, WHOIS_CONFIG_DETAILS_FULL);

	whois_config_add("idle", WHOIS_CONFIG_USER_EVERYONE, WHOIS_CONFIG_DETAILS_LIMITED);
	whois_config_add("idle", WHOIS_CONFIG_USER_SELF, WHOIS_CONFIG_DETAILS_FULL);
	whois_config_add("idle", WHOIS_CONFIG_USER_OPER, WHOIS_CONFIG_DETAILS_FULL);
}

static void whois_free_config(void)
{
}

static WhoisConfigUser whois_config_user_strtovalue(const char *str)
{
	if (!strcmp(str, "everyone"))
		return WHOIS_CONFIG_USER_EVERYONE;
	if (!strcmp(str, "self"))
		return WHOIS_CONFIG_USER_SELF;
	if (!strcmp(str, "oper"))
		return WHOIS_CONFIG_USER_OPER;
	return 0;
}

static WhoisConfigDetails whois_config_details_strtovalue(const char *str)
{
	if (!strcmp(str, "full"))
		return WHOIS_CONFIG_DETAILS_FULL;
	if (!strcmp(str, "limited"))
		return WHOIS_CONFIG_DETAILS_LIMITED;
	if (!strcmp(str, "none"))
		return WHOIS_CONFIG_DETAILS_NONE;
	return 0;
}

static int whois_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep, *cepp;

	if (type != CONFIG_SET)
		return 0;

	/* We are only interrested in set::whois-details.. */
	if (!ce || strcmp(ce->name, "whois-details"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (cep->value)
		{
			config_error("%s:%i: set::whois-details::%s item has a value, which is unexpected. Check your syntax!",
				cep->file->filename, cep->line_number, cep->name);
			errors++;
			continue;
		}
		for (cepp = cep->items; cepp; cepp = cepp->next)
		{
			if (!whois_config_user_strtovalue(cepp->name))
			{
				config_error("%s:%i: set::whois-details::%s contains unknown user category called '%s', must be one of: everyone, self, ircop",
					cepp->file->filename, cepp->line_number, cep->name, cepp->name);
				errors++;
				continue;
			} else
			if (!cepp->value || !whois_config_details_strtovalue(cepp->value))
			{
				config_error("%s:%i: set::whois-details::%s contains unknown details type '%s', must be one of: full, limited, none",
					cepp->file->filename, cepp->line_number, cep->name, cepp->name);
				errors++;
				continue;
			} /* else it is good */
		}
	}

	*errs = errors;
	return errors ? -1 : 1;
}

static int whois_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep, *cepp;

	if (type != CONFIG_SET)
		return 0;

	/* We are only interrested in set::whois-details.. */
	if (!ce || strcmp(ce->name, "whois-details"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		WhoisConfig *w = find_whois_config(cep->name);
		if (!w)
		{
			/* New one */
			w = safe_alloc(sizeof(WhoisConfig));
			safe_strdup(w->name, cep->name);
			AddListItem(w, whoisconfig);
		}
		for (cepp = cep->items; cepp; cepp = cepp->next)
		{
			WhoisConfigUser user = whois_config_user_strtovalue(cepp->name);
			WhoisConfigDetails details = whois_config_details_strtovalue(cepp->value);
			w->permissions[user] = details;
		}
	}
	return 1;
}

/** Get set::whois-details policy for an item.
 * @param client		The client doing the /WHOIS
 * @param target		The client being whoised, so the one to show all details for
 * @param name			The name of the whois item (eg "modes")
 */
WhoisConfigDetails _whois_get_policy(Client *client, Client *target, const char *name)
{
	WhoisConfig *w = find_whois_config(name);
	if (!w)
		return WHOIS_CONFIG_DETAILS_DEFAULT;
	if ((client == target) && (w->permissions[WHOIS_CONFIG_USER_SELF] > 0))
		return w->permissions[WHOIS_CONFIG_USER_SELF];
	if (IsOper(client) && (w->permissions[WHOIS_CONFIG_USER_OPER] > 0))
		return w->permissions[WHOIS_CONFIG_USER_OPER];
	if (w->permissions[WHOIS_CONFIG_USER_EVERYONE] > 0)
		return w->permissions[WHOIS_CONFIG_USER_EVERYONE];
	return WHOIS_CONFIG_DETAILS_NONE;
}

/* WHOIS command.
 * parv[1] = list of nicks (comma separated)
 */
CMD_FUNC(cmd_whois)
{
	Membership *lp;
	Client *target;
	Channel *channel;
	char *nick, *tmp;
	char *p = NULL;
	int len, mlen;
	char querybuf[BUFSIZE];
	char buf[BUFSIZE];
	int ntargets = 0;
	int maxtargets = max_targets_for_command("WHOIS");

	if (parc < 2)
	{
		sendnumeric(client, ERR_NONICKNAMEGIVEN);
		return;
	}

	if (parc > 2)
	{
		if (hunt_server(client, recv_mtags, "WHOIS", 1, parc, parv) != HUNTED_ISME)
			return;
		parv[1] = parv[2];
	}

	strlcpy(querybuf, parv[1], sizeof(querybuf));

	for (tmp = canonize(parv[1]); (nick = strtoken(&p, tmp, ",")); tmp = NULL)
	{
		unsigned char showchannel, wilds, hideoper; /* <- these are all boolean-alike */
		NameValuePrioList *list = NULL, *e;
		int policy; /* for temporary stuff */

		if (MyUser(client) && (++ntargets > maxtargets))
		{
			sendnumeric(client, ERR_TOOMANYTARGETS, nick, maxtargets, "WHOIS");
			break;
		}

		/* We do not support "WHOIS *" */
		wilds = (strchr(nick, '?') || strchr(nick, '*'));
		if (wilds)
			continue;

		target = find_user(nick, NULL);
		if (!target)
		{
			sendnumeric(client, ERR_NOSUCHNICK, nick);
			continue;
		}

		/* Ok, from this point we are going to proceed with the WHOIS.
		 * The idea here is NOT to send any lines, so don't call sendto functions.
		 * Instead, use add_nvplist_numeric() and add_nvplist_numeric_fmt()
		 * to add items to the whois list.
		 * Then at the end of this loop we call modules who can also add/remove
		 * whois lines, and only after that we FINALLY send all the whois lines
		 * in one go.
		 */

		hideoper = 0;
		if (IsHideOper(target) && (target != client) && !IsOper(client))
			hideoper = 1;

		if (whois_get_policy(client, target, "basic") > WHOIS_CONFIG_DETAILS_NONE)
		{
			add_nvplist_numeric(&list, -1000000, "basic", client, RPL_WHOISUSER, target->name,
				target->user->username,
				IsHidden(target) ? target->user->virthost : target->user->realhost,
				target->info);
		}

		if (whois_get_policy(client, target, "modes") > WHOIS_CONFIG_DETAILS_NONE)
		{
			add_nvplist_numeric(&list, -100000, "modes", client, RPL_WHOISMODES, target->name,
				get_usermode_string(target), target->user->snomask ? target->user->snomask : "");
		}
		if (whois_get_policy(client, target, "realhost") > WHOIS_CONFIG_DETAILS_NONE)
		{
			add_nvplist_numeric(&list, -90000, "realhost", client, RPL_WHOISHOST, target->name,
				(MyConnect(target) && strcmp(target->ident, "unknown")) ? target->ident : "*",
				target->user->realhost, target->ip ? target->ip : "");
		}

		if (IsRegNick(target) && (whois_get_policy(client, target, "registered-nick") > WHOIS_CONFIG_DETAILS_NONE))
		{
			add_nvplist_numeric(&list, -80000, "registered-nick", client, RPL_WHOISREGNICK, target->name);
		}

		/* The following code deals with channels */
		policy = whois_get_policy(client, target, "channels");
		if (policy > WHOIS_CONFIG_DETAILS_NONE)
		{
			int channel_whois_lines = 0;
			mlen = strlen(me.name) + strlen(client->name) + 10 + strlen(target->name);
			for (len = 0, *buf = '\0', lp = target->user->channel; lp; lp = lp->next)
			{
				Hook *h;
				int ret = EX_ALLOW;
				int operoverride = 0;
				
				channel = lp->channel;
				showchannel = 0;

				if (ShowChannel(client, channel))
					showchannel = 1;

				for (h = Hooks[HOOKTYPE_SEE_CHANNEL_IN_WHOIS]; h; h = h->next)
				{
					int n = (*(h->func.intfunc))(client, target, channel);
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
				
				/* If the channel is normally hidden, but the user is an IRCOp,
				 * and has the channel:see:whois privilege,
				 * and set::whois-details for 'channels' has 'oper full',
				 * then show it:
				 */
				if (!showchannel && (ValidatePermissionsForPath("channel:see:whois",client,NULL,channel,NULL)) && (policy == WHOIS_CONFIG_DETAILS_FULL))
				{
					showchannel = 1; /* OperOverride */
					operoverride = 1;
				}
				
				if ((ret == EX_ALWAYS_DENY) && (target != client))
					continue; /* a module asked us to really not expose this channel, so we don't (except target==ourselves). */

				/* This deals with target==client but also for unusual set::whois-details overrides
				 * such as 'everyone full'
				 */
				if (policy == WHOIS_CONFIG_DETAILS_FULL)
					showchannel = 1;

				if (showchannel)
				{
					if (len + strlen(channel->name) > (size_t)BUFSIZE - 4 - mlen)
					{
						add_nvplist_numeric_fmt(&list, -70500-channel_whois_lines, "channels", client, RPL_WHOISCHANNELS,
						                        "%s :%s", target->name, buf);
						channel_whois_lines++;
						*buf = '\0';
						len = 0;
					}

					if (operoverride)
					{
						/* '?' and '!' both mean we can see the channel in /WHOIS and normally wouldn't,
						 * but there's still a slight difference between the two...
						 */
						if (!PubChannel(channel))
						{
							/* '?' means it's a secret/private channel (too) */
							*(buf + len++) = '?';
						}
						else
						{
							/* public channel but hidden in WHOIS (umode +p, service bot, etc) */
							*(buf + len++) = '!';
						}
					}

					if (!MyUser(client) || !HasCapability(client, "multi-prefix"))
					{
						/* Standard NAMES reply (single character) */
						char c = mode_to_prefix(*lp->member_modes);
						if (c)
							*(buf + len++) = c;
					}
					else
					{
						/* NAMES reply with all rights included (multi-prefix / NAMESX) */
						strcpy(buf + len, modes_to_prefix(lp->member_modes));
						len += strlen(buf + len);
					}
					if (len)
						*(buf + len) = '\0';
					strcpy(buf + len, channel->name);
					len += strlen(channel->name);
					strcat(buf + len, " ");
					len++;
				}
			}

			if (buf[0] != '\0')
			{
				add_nvplist_numeric_fmt(&list, -70500-channel_whois_lines, "channels", client, RPL_WHOISCHANNELS,
							"%s :%s", target->name, buf);
				channel_whois_lines++;
			}
		}

		if (!(IsULine(target) && !IsOper(client) && HIDE_ULINES) &&
		    whois_get_policy(client, target, "server") > WHOIS_CONFIG_DETAILS_NONE)
		{
			add_nvplist_numeric(&list, -60000, "server", client, RPL_WHOISSERVER,
			                    target->name, target->user->server, target->uplink->info);
		}

		if (target->user->away && (whois_get_policy(client, target, "away") > WHOIS_CONFIG_DETAILS_NONE))
		{
			add_nvplist_numeric(&list, -50000, "away", client, RPL_AWAY,
			                    target->name, target->user->away);
		}

		if (IsOper(target) && !hideoper)
		{
			policy = whois_get_policy(client, target, "oper");
			if (policy == WHOIS_CONFIG_DETAILS_FULL)
			{
				const char *operlogin = get_operlogin(target);
				const char *operclass = get_operclass(target);

				if (operlogin && operclass)
				{
					add_nvplist_numeric_fmt(&list, -40000, "oper", client, RPL_WHOISOPERATOR,
					                        "%s :is %s (%s) [%s]",
					                        target->name, "an IRC Operator", operlogin, operclass);
				} else
				if (operlogin)
				{
					add_nvplist_numeric_fmt(&list, -40000, "oper", client, RPL_WHOISOPERATOR,
					                        "%s :is %s (%s)",
					                        target->name, "an IRC Operator", operlogin);
				} else
				{
					add_nvplist_numeric(&list, -40000, "oper", client, RPL_WHOISOPERATOR,
							    target->name, "an IRC Operator");
				}
			} else
			if (policy == WHOIS_CONFIG_DETAILS_LIMITED)
			{
				add_nvplist_numeric(&list, -40000, "oper", client, RPL_WHOISOPERATOR,
				                    target->name, "an IRC Operator");
			}
		}

		if (target->umodes & UMODE_SECURE)
		{
			policy = whois_get_policy(client, target, "secure");
			if (policy == WHOIS_CONFIG_DETAILS_LIMITED)
			{
				add_nvplist_numeric(&list, -30000, "secure", client, RPL_WHOISSECURE,
				                    target->name, "is using a Secure Connection");
			} else
			if (policy == WHOIS_CONFIG_DETAILS_FULL)
			{
				const char *ciphers = tls_get_cipher(target);
				if (ciphers)
				{
					add_nvplist_numeric_fmt(&list, -30000, "secure", client, RPL_WHOISSECURE,
					                        "%s :is using a Secure Connection [%s]",
					                        target->name, ciphers);
				} else {
					add_nvplist_numeric(&list, -30000, "secure", client, RPL_WHOISSECURE,
							    target->name, "is using a Secure Connection");
				}
			}
		}

		/* The following code deals with security-groups */
		policy = whois_get_policy(client, target, "security-groups");
		if ((policy > WHOIS_CONFIG_DETAILS_NONE) && !IsULine(target))
		{
			SecurityGroup *s;
			int security_groups_whois_lines = 0;

			mlen = strlen(me.name) + strlen(client->name) + 10 + strlen(target->name) + strlen("is in security-groups: ");

			if (user_allowed_by_security_group_name(target, "known-users"))
				strlcpy(buf, "known-users,", sizeof(buf));
			else
				strlcpy(buf, "unknown-users,", sizeof(buf));
			len = strlen(buf);

			for (s = securitygroups; s; s = s->next)
			{
				if (len + strlen(s->name) > (size_t)BUFSIZE - 4 - mlen)
				{
					buf[len-1] = '\0';
					add_nvplist_numeric_fmt(&list, -15000-security_groups_whois_lines, "security-groups",
					                        target, RPL_WHOISSPECIAL,
								"%s :is in security-groups: %s", target->name, buf);
					security_groups_whois_lines++;
					*buf = '\0';
					len = 0;
				}
				if (strcmp(s->name, "known-users") && user_allowed_by_security_group(target, s))
				{
					strcpy(buf + len, s->name);
					len += strlen(buf+len);
					strcpy(buf + len, ",");
					len++;
				}
			}

			if (*buf)
			{
				buf[len-1] = '\0';
				add_nvplist_numeric_fmt(&list, -15000-security_groups_whois_lines, "security-groups",
				                        client, RPL_WHOISSPECIAL,
							"%s :is in security-groups: %s", target->name, buf);
				security_groups_whois_lines++;
			}
		}
		if (MyUser(target) && IsShunned(target) && (whois_get_policy(client, target, "shunned") > WHOIS_CONFIG_DETAILS_NONE))
		{
			add_nvplist_numeric(&list, -20000, "shunned", client, RPL_WHOISSPECIAL,
			                    target->name, "is shunned");
		}

		if (target->user->swhois && (whois_get_policy(client, target, "swhois") > WHOIS_CONFIG_DETAILS_NONE))
		{
			SWhois *s;
			int swhois_lines = 0;

			for (s = target->user->swhois; s; s = s->next)
			{
				if (hideoper && !IsOper(client) && s->setby && !strcmp(s->setby, "oper"))
					continue; /* hide oper-based swhois entries */
				add_nvplist_numeric(&list, 100000+swhois_lines, "swhois", client, RPL_WHOISSPECIAL,
				                    target->name, s->line);
				swhois_lines++;
			}
		}

		/* TODO: hmm.. this should be a bit more towards the beginning of the whois, no ? */
		if (IsLoggedIn(target) && (whois_get_policy(client, target, "account") > WHOIS_CONFIG_DETAILS_NONE))
		{
			add_nvplist_numeric(&list, 200000, "account", client, RPL_WHOISLOGGEDIN,
			                    target->name, target->user->account);
		}

		if (MyConnect(target))
		{
			policy = whois_get_policy(client, target, "idle");
			/* If the policy is 'full' then show the idle time.
			 * If the policy is 'limited then show the idle time according to the +I rules
			 */
			if ((policy == WHOIS_CONFIG_DETAILS_FULL) ||
			    ((policy == WHOIS_CONFIG_DETAILS_LIMITED) && !hide_idle_time(client, target)))
			{
				add_nvplist_numeric(&list, 500000, "idle", client, RPL_WHOISIDLE,
				                    target->name,
				                    (long long)(TStime() - target->local->idle_since),
				                    (long long)target->local->creationtime);
			}
		}

		RunHook(HOOKTYPE_WHOIS, client, target, &list);

		for (e = list; e; e = e->next)
			sendto_one(client, NULL, "%s", e->value);

		free_nvplist(list);
	}
	sendnumeric(client, RPL_ENDOFWHOIS, querybuf);
}
