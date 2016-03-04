/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_whois.c
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

CMD_FUNC(m_whois);

#define MSG_WHOIS       "WHOIS"

ModuleHeader MOD_HEADER(m_whois)
  = {
	"whois",	/* Name of module */
	"4.0", /* Version */
	"command /whois", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

/* This is called on module init, before Server Ready */
MOD_INIT(m_whois)
{
	CommandAdd(modinfo->handle, MSG_WHOIS, m_whois, MAXPARA, M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
MOD_LOAD(m_whois)
{
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
MOD_UNLOAD(m_whois)
{
	return MOD_SUCCESS;
}


/*
** m_whois
**	parv[1] = nickname masklist
*/
CMD_FUNC(m_whois)
{
	Membership *lp;
	aClient *acptr;
	aChannel *chptr;
	char *nick, *tmp, *name;
	char *p = NULL;
	int  found, len, mlen, cnt = 0;
	char querybuf[BUFSIZE];

	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NONICKNAMEGIVEN),
		    me.name, sptr->name);
		return 0;
	}

	if (parc > 2)
	{
		if (hunt_server(cptr, sptr, ":%s WHOIS %s :%s", 1, parc, parv) != HUNTED_ISME)
			return 0;
		parv[1] = parv[2];
	}

	strlcpy(querybuf, parv[1], sizeof(querybuf));

	for (tmp = canonize(parv[1]); (nick = strtoken(&p, tmp, ",")); tmp = NULL)
	{
		unsigned char invis, showchannel, member, wilds, hideoper; /* <- these are all boolean-alike */

		if (++cnt > MAXTARGETS)
			break;

		found = 0;
		/* We do not support "WHOIS *" */
		wilds = (index(nick, '?') || index(nick, '*'));
		if (wilds)
			continue;

		if ((acptr = find_client(nick, NULL)))
		{
			if (IsServer(acptr))
				continue;
			/*
			 * I'm always last :-) and acptr->next == NULL!!
			 */
			if (IsMe(acptr))
				break;
			/*
			 * 'Rules' established for sending a WHOIS reply:
			 * - only send replies about common or public channels
			 *   the target user(s) are on;
			 */

			if (!IsPerson(acptr))
				continue;

			name = (!*acptr->name) ? "?" : acptr->name;

			invis = acptr != sptr && IsInvisible(acptr);
			member = (acptr->user->channel) ? 1 : 0;

			hideoper = 0;
			if (IsHideOper(acptr) && (acptr != sptr) && !IsOper(sptr))
				hideoper = 1;

			sendto_one(sptr, rpl_str(RPL_WHOISUSER), me.name,
			    sptr->name, name,
			    acptr->user->username,
			    IsHidden(acptr) ? acptr->user->virthost : acptr->user->realhost,
			    acptr->info);

			if (IsOper(sptr) || acptr == sptr)
			{
				char sno[128];
				strlcpy(sno, get_sno_str(acptr), sizeof(sno));
				
				/* send the target user's modes */
				sendto_one(sptr, rpl_str(RPL_WHOISMODES),
				    me.name, sptr->name, name,
				    get_mode_str(acptr), sno[1] == 0 ? "" : sno);
			}
			if ((acptr == sptr) || IsOper(sptr))
			{
				sendto_one(sptr, rpl_str(RPL_WHOISHOST),
				    me.name, sptr->name, acptr->name,
					(MyConnect(acptr) && strcmp(acptr->username, "unknown")) ? acptr->username : "*",
					acptr->user->realhost, acptr->ip ? acptr->ip : "");
			}

			if (IsARegNick(acptr))
				sendto_one(sptr, rpl_str(RPL_WHOISREGNICK), me.name, sptr->name, name);
			
			found = 1;
			mlen = strlen(me.name) + strlen(sptr->name) + 10 + strlen(name);
			for (len = 0, *buf = '\0', lp = acptr->user->channel; lp; lp = lp->next)
			{
				Hook *h;
				int ret = EX_ALLOW;
				int operoverride = 0;
				
				chptr = lp->chptr;
				showchannel = 0;

				if (ShowChannel(sptr, chptr))
					showchannel = 1;

				for (h = Hooks[HOOKTYPE_SEE_CHANNEL_IN_WHOIS]; h; h = h->next)
				{
					int n = (*(h->func.intfunc))(sptr, acptr, chptr);
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
				
				if (!showchannel && (ValidatePermissionsForPath("override:see:whois",sptr,NULL,chptr,NULL)))
				{
					showchannel = 1; /* OperOverride */
					operoverride = 1;
				}
				
				if ((ret == EX_ALWAYS_DENY) && (acptr != sptr))
					continue; /* a module asked us to really not expose this channel, so we don't (except target==ourselves). */

				if (acptr == sptr)
					showchannel = 1;

				if (showchannel)
				{
					long access;
					if (len + strlen(chptr->chname) > (size_t)BUFSIZE - 4 - mlen)
					{
						sendto_one(sptr,
						    ":%s %d %s %s :%s",
						    me.name,
						    RPL_WHOISCHANNELS,
						    sptr->name, name, buf);
						*buf = '\0';
						len = 0;
					}

					if (operoverride)
					{
						/* '?' and '!' both mean we can see the channel in /WHOIS and normally wouldn't,
						 * but there's still a slight difference between the two...
						 */
						if (!PubChannel(chptr))
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

					access = get_access(acptr, chptr);
					if (!MyClient(sptr) || !SupportNAMESX(sptr))
					{
#ifdef PREFIX_AQ
						if (access & CHFL_CHANOWNER)
							*(buf + len++) = '~';
						else if (access & CHFL_CHANPROT)
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
						if (access & CHFL_CHANPROT)
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
					(void)strcpy(buf + len, chptr->chname);
					len += strlen(chptr->chname);
					(void)strcat(buf + len, " ");
					len++;
				}
			}

			if (buf[0] != '\0')
				sendto_one(sptr, rpl_str(RPL_WHOISCHANNELS), me.name, sptr->name, name, buf); 

                        if (!(IsULine(acptr) && !IsOper(sptr) && HIDE_ULINES))
				sendto_one(sptr, rpl_str(RPL_WHOISSERVER),
				    me.name, sptr->name, name, acptr->user->server,
				    acptr->srvptr ? acptr->srvptr->info : "*Not On This Net*");

			if (acptr->user->away)
				sendto_one(sptr, rpl_str(RPL_AWAY), me.name,
				    sptr->name, name, acptr->user->away);

			if (IsOper(acptr) && !hideoper)
			{
				buf[0] = '\0';
				if (IsOper(acptr))
					strlcat(buf, "an IRC Operator", sizeof buf);

				else
					strlcat(buf, "a Local IRC Operator", sizeof buf);
				if (buf[0])
				{
					if (IsOper(sptr) && MyClient(acptr))
					{
						char *operclass = "???";
						ConfigItem_oper *oper = Find_oper(acptr->user->operlogin);
						if (oper && oper->operclass)
							operclass = oper->operclass;
						sendto_one(sptr,
						    ":%s 313 %s %s :is %s (%s) [%s]", me.name,
						    sptr->name, name, buf,
						    acptr->user->operlogin ? acptr->user->operlogin : "unknown",
						    operclass);
					}
					else
						sendto_one(sptr,
						    rpl_str(RPL_WHOISOPERATOR), me.name,
						    sptr->name, name, buf);
				}
			}

			if (acptr->umodes & UMODE_SECURE)
				sendto_one(sptr, rpl_str(RPL_WHOISSECURE), me.name, sptr->name, name,
					"is using a Secure Connection");
			
			RunHook2(HOOKTYPE_WHOIS, sptr, acptr);

			if (acptr->user->swhois && !hideoper)
			{
				SWhois *s;
				
				for (s = acptr->user->swhois; s; s = s->next)
					sendto_one(sptr, ":%s %d %s %s :%s",
					    me.name, RPL_WHOISSPECIAL, sptr->name,
					    name, s->line);
			}

			/*
			 * display services account name if it's actually a services account name and
			 * not a legacy timestamp.  --nenolod
			 */
			if (!isdigit(*acptr->user->svid))
				sendto_one(sptr, rpl_str(RPL_WHOISLOGGEDIN), me.name, sptr->name, name, acptr->user->svid);

			/*
			 * Umode +I hides an oper's idle time from regular users.
			 * -Nath.
			 */
			if (MyConnect(acptr) && (IsOper(sptr) || !(acptr->umodes & UMODE_HIDLE)))
			{
				sendto_one(sptr, rpl_str(RPL_WHOISIDLE),
				    me.name, sptr->name, name,
				    TStime() - acptr->local->last, acptr->local->firsttime);
			}
		}
		if (!found)
			sendto_one(sptr, err_str(ERR_NOSUCHNICK),
			    me.name, sptr->name, nick);
	}
	sendto_one(sptr, rpl_str(RPL_ENDOFWHOIS), me.name, sptr->name, querybuf);

	return 0;
}
