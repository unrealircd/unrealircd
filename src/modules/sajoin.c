/*
 *   IRC - Internet Relay Chat, src/modules/m_sajoin.c
 *   (C) 2004 The UnrealIRCd Team
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

#include "unrealircd.h"

CMD_FUNC(m_sajoin);

#define MSG_SAJOIN 	"SAJOIN"	

ModuleHeader MOD_HEADER(sajoin)
  = {
	"sajoin",
	"5.0",
	"command /sajoin", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(sajoin)
{
	CommandAdd(modinfo->handle, MSG_SAJOIN, m_sajoin, MAXPARA, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(sajoin)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(sajoin)
{
	return MOD_SUCCESS;
}

/* m_sajoin() - Lamego - Wed Jul 21 20:04:48 1999
   Copied off PTlink IRCd (C) PTlink coders team.
   Coded for Sadmin by Stskeeps
   also Modified by NiQuiL (niquil@programmer.net)
	parv[1] - nick to make join
	parv[2] - channel(s) to join
*/
CMD_FUNC(m_sajoin)
{
	aClient *acptr;
	char jbuf[BUFSIZE];
	char mode = '\0';
	char sjmode = '\0';
	char *mode_args[3];
	int did_anything = 0;
	int ntargets = 0;
	int maxtargets = max_targets_for_command("SAJOIN");

	if (parc < 3)
	{
		sendnumeric(sptr, ERR_NEEDMOREPARAMS, "SAJOIN");
		return 0;
	}

	if (!(acptr = find_person(parv[1], NULL)))
	{
		sendnumeric(sptr, ERR_NOSUCHNICK, parv[1]);
		return 0;
	}

	/* Is this user disallowed from operating on this victim at all? */
	if (!IsULine(sptr) && !ValidatePermissionsForPath("sacmd:sajoin",sptr,acptr,NULL,NULL))
	{
		sendnumeric(sptr, ERR_NOPRIVILEGES);
		return 0;
	}

	/* If it's not for our client, then simply pass on the message... */
	if (!MyClient(acptr))
	{
		sendto_one(acptr, NULL, ":%s SAJOIN %s %s", sptr->name, parv[1], parv[2]);

		/* Logging function added by XeRXeS */
		ircd_log(LOG_SACMDS,"SAJOIN: %s used SAJOIN to make %s join %s",
			sptr->name, parv[1], parv[2]);

		return 0;
	}

	/* Can't this just use do_join() or something with a parameter to bypass some checks?
	 * This duplicate code is damn ugly. Ah well..
	 */
	{
		char *name, *p = NULL;
		int i, parted = 0;
	
		*jbuf = 0;

		/* Now works like m_join */
		for (i = 0, name = strtoken(&p, parv[2], ","); name; name = strtoken(&p, NULL, ","))
		{
			aChannel *chptr;
			Membership *lp;

			if (++ntargets > maxtargets)
			{
				sendnumeric(sptr, ERR_TOOMANYTARGETS, name, maxtargets, "SAJOIN");
				break;
			}

			switch (name[0])
			{
#ifdef PREFIX_AQ
				case '~':
					mode = 'q';
					sjmode = '~';
					++name;
					break;
				case '&':
					mode = 'a';
					sjmode = '&';
					++name;
					break;
#endif
				case '@':
					mode = 'o';
					sjmode = '@';
					++name;
					break;
				case '%':
					mode = 'h';
					sjmode = '%';
					++name;
					break;
				case '+':
					mode = 'v';
					sjmode = '+';
					++name;
					break;
				default:
					mode = sjmode = '\0'; /* make sure sjmode is 0. */
					break;
			}

			if (strlen(name) > CHANNELLEN)
				name[CHANNELLEN] = 0;
			clean_channelname(name);
			if (*name == '0' && !atoi(name) && !sjmode)
			{
				(void)strcpy(jbuf, "0");
				i = 1;
				parted = 1;
				continue;
			}
			if (*name == '0' || !IsChannelName(name))
			{
				sendnumeric(sptr, ERR_NOSUCHCHANNEL, name);
				continue;
			}

			chptr = get_channel(acptr, name, 0);

			/* If this _specific_ channel is not permitted, skip it */
			if (!IsULine(sptr) && !ValidatePermissionsForPath("sacmd:sajoin",sptr,acptr,chptr,NULL))
			{
				sendnumeric(sptr, ERR_NOPRIVILEGES);
				continue;
			}

			if (!parted && chptr && (lp = find_membership_link(acptr->user->channel, chptr)))
			{
				sendnumeric(sptr, ERR_USERONCHANNEL, parv[1], name);
				continue;
			}
			if (*jbuf)
				(void)strlcat(jbuf, ",", sizeof jbuf);
			(void)strlncat(jbuf, name, sizeof jbuf, sizeof(jbuf) - i - 1);
			i += strlen(name) + 1;
		}
		if (!*jbuf)
			return -1;
		i = 0;
		strcpy(parv[2], jbuf);
		*jbuf = 0;
		for (name = strtoken(&p, parv[2], ","); name; name = strtoken(&p, NULL, ","))
		{
			MessageTag *mtags = NULL;
			int flags;
			aChannel *chptr;
			Membership *lp;
			Hook *h;
			int i = 0;

			if (*name == '0' && !atoi(name) && !sjmode)
			{
				/* Rewritten so to generate a PART for each channel to servers,
				 * so the same msgid is used for each part on all servers. -- Syzop
				 */
				did_anything = 1;
				while ((lp = acptr->user->channel))
				{
					MessageTag *mtags = NULL;
					chptr = lp->chptr;

					new_message(acptr, NULL, &mtags);
					sendto_channel(chptr, acptr, NULL, 0, 0, SEND_LOCAL, NULL,
					               ":%s PART %s :%s",
					               acptr->name, chptr->chname, "Left all channels");
					sendto_server(cptr, 0, 0, mtags, ":%s PART %s :Left all channels", acptr->name, chptr->chname);
					free_message_tags(mtags);
					if (MyConnect(acptr))
						RunHook4(HOOKTYPE_LOCAL_PART, acptr, acptr, chptr, "Left all channels");
					remove_user_from_channel(acptr, chptr);
				}
				strcpy(jbuf, "0");
				continue;
			}
			flags = (ChannelExists(name)) ? CHFL_DEOPPED : LEVEL_ON_JOIN;
			chptr = get_channel(acptr, name, CREATE);
			if (chptr && (lp = find_membership_link(acptr->user->channel, chptr)))
				continue;

			i = HOOK_CONTINUE;
			for (h = Hooks[HOOKTYPE_CAN_SAJOIN]; h; h = h->next)
			{
				i = (*(h->func.intfunc))(acptr,chptr,sptr);
				if (i != HOOK_CONTINUE)
					break;
			}

			if (i == HOOK_DENY)
				continue; /* process next channel */

			/* Generate a new message without inheritance.
			 * We can do this because we are the server that
			 * will send a JOIN for each channel due to this loop.
			 * Each with their own unique msgid.
			 */
			new_message(acptr, NULL, &mtags);
			join_channel(chptr, acptr, acptr, mtags, flags);
			if (sjmode)
			{
				opermode = 0;
				sajoinmode = 1;
				mode_args[0] = (char*)malloc(2);
				mode_args[0][0] = mode;
				mode_args[0][1] = '\0';
				mode_args[1] = acptr->name;
				mode_args[2] = 0;
				(void)do_mode(chptr, &me, acptr, NULL, 3, mode_args, 0, 1);
				sajoinmode = 0;
				free(mode_args[0]);
			}
			free_message_tags(mtags);
			did_anything = 1;
			if (*jbuf)
				strlcat(jbuf, ",", sizeof jbuf);
			strlcat(jbuf, name, sizeof jbuf);
		}
		
		if (did_anything)
		{
			if (!sjmode)
			{
				sendnotice(acptr, "*** You were forced to join %s", jbuf);
				sendto_realops("%s used SAJOIN to make %s join %s", sptr->name, acptr->name, jbuf);
				sendto_server(&me, 0, 0, NULL, ":%s GLOBOPS :%s used SAJOIN to make %s join %s",
					me.name, sptr->name, acptr->name, jbuf);
				/* Logging function added by XeRXeS */
				ircd_log(LOG_SACMDS,"SAJOIN: %s used SAJOIN to make %s join %s",
					sptr->name, parv[1], jbuf);
			}
			else
			{
				sendnotice(acptr, "*** You were forced to join %s with '%c'", jbuf, sjmode);
				sendto_realops("%s used SAJOIN to make %s join %c%s", sptr->name, acptr->name, sjmode, jbuf);
				sendto_server(&me, 0, 0, NULL, ":%s GLOBOPS :%s used SAJOIN to make %s join %c%s",
					me.name, sptr->name, acptr->name, sjmode, jbuf);
				ircd_log(LOG_SACMDS,"SAJOIN: %s used SAJOIN to make %s join %c%s",
					sptr->name, parv[1], sjmode, jbuf);
			}
		}
	}

	return 0;
}
