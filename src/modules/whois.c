/*
 *   Unreal Internet Relay Chat Daemon, src/modules/whois.c
 *   (C) 2000-2001 Carsten V. Munk and the UnrealIRCd Team
 *   Moved to modules by Fish (Justin Hammond)
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

static char buf[BUFSIZE];

CMD_FUNC(cmd_whois);

#define MSG_WHOIS       "WHOIS"

ModuleHeader MOD_HEADER
  = {
	"whois",	/* Name of module */
	"5.0", /* Version */
	"command /whois", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-5",
    };

/* This is called on module init, before Server Ready */
MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_WHOIS, cmd_whois, MAXPARA, CMD_USER);
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


/*
** cmd_whois
**	parv[1] = nickname masklist
*/
CMD_FUNC(cmd_whois)
{
	Membership *lp;
	Client *target;
	Channel *channel;
	char *nick, *tmp, *name;
	char *p = NULL;
	int  found, len, mlen;
	char querybuf[BUFSIZE];
	int ntargets = 0;
	int maxtargets = max_targets_for_command("WHOIS");

	if (parc < 2)
	{
		sendnumeric(client, ERR_NONICKNAMEGIVEN);
		return;
	}

	if (parc > 2)
	{
		if (hunt_server(client, recv_mtags, ":%s WHOIS %s :%s", 1, parc, parv) != HUNTED_ISME)
			return;
		parv[1] = parv[2];
	}

	strlcpy(querybuf, parv[1], sizeof(querybuf));

	for (tmp = canonize(parv[1]); (nick = strtoken(&p, tmp, ",")); tmp = NULL)
	{
		unsigned char showchannel, wilds, hideoper; /* <- these are all boolean-alike */

		if (MyUser(client) && (++ntargets > maxtargets))
		{
			sendnumeric(client, ERR_TOOMANYTARGETS, nick, maxtargets, "WHOIS");
			break;
		}

		found = 0;
		/* We do not support "WHOIS *" */
		wilds = (strchr(nick, '?') || strchr(nick, '*'));
		if (wilds)
			continue;

		if ((target = find_person(nick, NULL)))
		{
			/*
			 * 'Rules' established for sending a WHOIS reply:
			 * - only send replies about common or public channels
			 *   the target user(s) are on;
			 */

			if (!IsUser(target))
				continue;

			name = (!*target->name) ? "?" : target->name;

			hideoper = 0;
			if (IsHideOper(target) && (target != client) && !IsOper(client))
				hideoper = 1;

			sendnumeric(client, RPL_WHOISUSER, name,
			    target->user->username,
			    IsHidden(target) ? target->user->virthost : target->user->realhost,
			    target->info);

			if (IsOper(client) || target == client)
			{
				char sno[128];
				strlcpy(sno, get_snomask_string(target), sizeof(sno));
				
				/* send the target user's modes */
				sendnumeric(client, RPL_WHOISMODES, name,
				    get_usermode_string(target), sno[1] == 0 ? "" : sno);
			}
			if ((target == client) || IsOper(client))
			{
				sendnumeric(client, RPL_WHOISHOST, target->name,
					(MyConnect(target) && strcmp(target->ident, "unknown")) ? target->ident : "*",
					target->user->realhost, target->ip ? target->ip : "");
			}

			if (IsRegNick(target))
				sendnumeric(client, RPL_WHOISREGNICK, name);
			
			found = 1;
			mlen = strlen(me.name) + strlen(client->name) + 10 + strlen(name);
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
				
				if (!showchannel && (ValidatePermissionsForPath("channel:see:whois",client,NULL,channel,NULL)))
				{
					showchannel = 1; /* OperOverride */
					operoverride = 1;
				}
				
				if ((ret == EX_ALWAYS_DENY) && (target != client))
					continue; /* a module asked us to really not expose this channel, so we don't (except target==ourselves). */

				if (target == client)
					showchannel = 1;

				if (showchannel)
				{
					long access;
					if (len + strlen(channel->chname) > (size_t)BUFSIZE - 4 - mlen)
					{
						sendto_one(client, NULL,
						    ":%s %d %s %s :%s",
						    me.name,
						    RPL_WHOISCHANNELS,
						    client->name, name, buf);
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

					access = get_access(target, channel);
					if (!MyUser(client) || !HasCapability(client, "multi-prefix"))
					{
#ifdef PREFIX_AQ
						if (access & CHFL_CHANOWNER)
							*(buf + len++) = '~';
						else if (access & CHFL_CHANADMIN)
							*(buf + len++) = '&';
						else
#endif
						if (access & CHFL_CHANOP)
							*(buf + len++) = '@';
						else if (access & CHFL_HALFOP)
							*(buf + len++) = '%';
						else if (access & CHFL_VOICE)
							*(buf + len++) = '+';
					}
					else
					{
#ifdef PREFIX_AQ
						if (access & CHFL_CHANOWNER)
							*(buf + len++) = '~';
						if (access & CHFL_CHANADMIN)
							*(buf + len++) = '&';
#endif
						if (access & CHFL_CHANOP)
							*(buf + len++) = '@';
						if (access & CHFL_HALFOP)
							*(buf + len++) = '%';
						if (access & CHFL_VOICE)
							*(buf + len++) = '+';
					}
					if (len)
						*(buf + len) = '\0';
					strcpy(buf + len, channel->chname);
					len += strlen(channel->chname);
					strcat(buf + len, " ");
					len++;
				}
			}

			if (buf[0] != '\0')
				sendnumeric(client, RPL_WHOISCHANNELS, name, buf); 

                        if (!(IsULine(target) && !IsOper(client) && HIDE_ULINES))
				sendnumeric(client, RPL_WHOISSERVER, name, target->user->server,
				    target->srvptr ? target->srvptr->info : "*Not On This Net*");

			if (target->user->away)
				sendnumeric(client, RPL_AWAY, name, target->user->away);

			if (IsOper(target) && !hideoper)
			{
				buf[0] = '\0';
				if (IsOper(target))
					strlcat(buf, "an IRC Operator", sizeof buf);

				else
					strlcat(buf, "a Local IRC Operator", sizeof buf);
				if (buf[0])
				{
					if (IsOper(client) && MyUser(target))
					{
						char *operclass = "???";
						ConfigItem_oper *oper = find_oper(target->user->operlogin);
						if (oper && oper->operclass)
							operclass = oper->operclass;
						sendto_one(client, NULL,
						    ":%s 313 %s %s :is %s (%s) [%s]", me.name,
						    client->name, name, buf,
						    target->user->operlogin ? target->user->operlogin : "unknown",
						    operclass);
					}
					else
						sendnumeric(client, RPL_WHOISOPERATOR, name, buf);
				}
			}

			if (target->umodes & UMODE_SECURE)
				sendnumeric(client, RPL_WHOISSECURE, name,
					"is using a Secure Connection");
			
			RunHook2(HOOKTYPE_WHOIS, client, target);

			if (IsOper(client) && MyUser(target) && IsShunned(target))
			{
				sendto_one(client, NULL, ":%s %d %s %s :is shunned",
				           me.name, RPL_WHOISSPECIAL, client->name, target->name);
			}

			if (target->user->swhois && !hideoper)
			{
				SWhois *s;
				
				for (s = target->user->swhois; s; s = s->next)
					sendto_one(client, NULL, ":%s %d %s %s :%s",
					    me.name, RPL_WHOISSPECIAL, client->name,
					    name, s->line);
			}

			/*
			 * display services account name if it's actually a services account name and
			 * not a legacy timestamp.  --nenolod
			 */
			if (IsLoggedIn(target))
				sendnumeric(client, RPL_WHOISLOGGEDIN, name, target->user->svid);

			/*
			 * Umode +I hides an oper's idle time from regular users.
			 * -Nath.
			 */
			if (MyConnect(target) && !hide_idle_time(client, target))
			{
				sendnumeric(client, RPL_WHOISIDLE, name,
				    TStime() - target->local->last, target->local->firsttime);
			}
		}
		if (!found)
			sendnumeric(client, ERR_NOSUCHNICK, nick);
	}
	sendnumeric(client, RPL_ENDOFWHOIS, querybuf);
}
