/*
 *   IRC - Internet Relay Chat, src/modules/sajoin.c
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

CMD_FUNC(cmd_sajoin);

#define MSG_SAJOIN 	"SAJOIN"	

ModuleHeader MOD_HEADER
  = {
	"sajoin",
	"5.0",
	"command /sajoin", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_SAJOIN, cmd_sajoin, MAXPARA, CMD_USER|CMD_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
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

/* cmd_sajoin() - Lamego - Wed Jul 21 20:04:48 1999
   Copied off PTlink IRCd (C) PTlink coders team.
   Coded for Sadmin by Stskeeps
   also Modified by NiQuiL (niquil@programmer.net)
	parv[1] - nick to make join
	parv[2] - channel(s) to join
*/
CMD_FUNC(cmd_sajoin)
{
	Client *target;
	char jbuf[BUFSIZE];
	char mode = '\0';
	char sjmode = '\0';
	char *mode_args[3];
	int did_anything = 0;
	int ntargets = 0;
	int maxtargets = max_targets_for_command("SAJOIN");

	if (parc < 3)
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "SAJOIN");
		return;
	}

	if (!(target = find_person(parv[1], NULL)))
	{
		sendnumeric(client, ERR_NOSUCHNICK, parv[1]);
		return;
	}

	/* Is this user disallowed from operating on this victim at all? */
	if (!IsULine(client) && !ValidatePermissionsForPath("sacmd:sajoin",client,target,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	/* If it's not for our client, then simply pass on the message... */
	if (!MyUser(target))
	{
		sendto_one(target, NULL, ":%s SAJOIN %s %s", client->id, target->id, parv[2]);

		/* Logging function added by XeRXeS */
		ircd_log(LOG_SACMDS,"SAJOIN: %s used SAJOIN to make %s join %s",
			client->name, target->name, parv[2]);

		return;
	}

	/* Can't this just use do_join() or something with a parameter to bypass some checks?
	 * This duplicate code is damn ugly. Ah well..
	 */
	{
		char *name, *p = NULL;
		int i, parted = 0;
	
		*jbuf = 0;

		/* Now works like cmd_join */
		for (i = 0, name = strtoken(&p, parv[2], ","); name; name = strtoken(&p, NULL, ","))
		{
			Channel *channel;
			Membership *lp;

			if (++ntargets > maxtargets)
			{
				sendnumeric(client, ERR_TOOMANYTARGETS, name, maxtargets, "SAJOIN");
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
			{
				sendnotice(client, "Channel name too long: %s", name);
				continue;
			}

			if (*name == '0' && !atoi(name) && !sjmode)
			{
				strcpy(jbuf, "0");
				i = 1;
				parted = 1;
				continue;
			}

			if (!valid_channelname(name))
			{
				send_invalid_channelname(client, name);
				continue;
			}

			channel = get_channel(target, name, 0);

			/* If this _specific_ channel is not permitted, skip it */
			if (!IsULine(client) && !ValidatePermissionsForPath("sacmd:sajoin",client,target,channel,NULL))
			{
				sendnumeric(client, ERR_NOPRIVILEGES);
				continue;
			}

			if (!parted && channel && (lp = find_membership_link(target->user->channel, channel)))
			{
				sendnumeric(client, ERR_USERONCHANNEL, target->name, name);
				continue;
			}
			if (*jbuf)
				strlcat(jbuf, ",", sizeof jbuf);
			strlncat(jbuf, name, sizeof jbuf, sizeof(jbuf) - i - 1);
			i += strlen(name) + 1;
		}
		if (!*jbuf)
			return;
		i = 0;
		strcpy(parv[2], jbuf);
		*jbuf = 0;
		for (name = strtoken(&p, parv[2], ","); name; name = strtoken(&p, NULL, ","))
		{
			MessageTag *mtags = NULL;
			int flags;
			Channel *channel;
			Membership *lp;
			Hook *h;
			int i = 0;

			if (*name == '0' && !atoi(name) && !sjmode)
			{
				/* Rewritten so to generate a PART for each channel to servers,
				 * so the same msgid is used for each part on all servers. -- Syzop
				 */
				did_anything = 1;
				while ((lp = target->user->channel))
				{
					MessageTag *mtags = NULL;
					channel = lp->channel;

					new_message(target, NULL, &mtags);
					sendto_channel(channel, target, NULL, 0, 0, SEND_LOCAL, mtags,
					               ":%s PART %s :%s",
					               target->name, channel->chname, "Left all channels");
					sendto_server(NULL, 0, 0, mtags, ":%s PART %s :Left all channels", target->name, channel->chname);
					if (MyConnect(target))
						RunHook4(HOOKTYPE_LOCAL_PART, target, channel, mtags, "Left all channels");
					free_message_tags(mtags);
					remove_user_from_channel(target, channel);
				}
				strcpy(jbuf, "0");
				continue;
			}
			flags = (ChannelExists(name)) ? CHFL_DEOPPED : LEVEL_ON_JOIN;
			channel = get_channel(target, name, CREATE);
			if (channel && (lp = find_membership_link(target->user->channel, channel)))
				continue;

			i = HOOK_CONTINUE;
			for (h = Hooks[HOOKTYPE_CAN_SAJOIN]; h; h = h->next)
			{
				i = (*(h->func.intfunc))(target,channel,client);
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
			new_message(target, NULL, &mtags);
			join_channel(channel, target, mtags, flags);
			if (sjmode)
			{
				opermode = 0;
				sajoinmode = 1;
				mode_args[0] = safe_alloc(2);
				mode_args[0][0] = mode;
				mode_args[0][1] = '\0';
				mode_args[1] = target->name;
				mode_args[2] = 0;
				do_mode(channel, target, NULL, 3, mode_args, 0, 1);
				sajoinmode = 0;
				safe_free(mode_args[0]);
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
				sendnotice(target, "*** You were forced to join %s", jbuf);
				sendto_umode_global(UMODE_OPER, "%s used SAJOIN to make %s join %s", client->name, target->name, jbuf);
				/* Logging function added by XeRXeS */
				ircd_log(LOG_SACMDS,"SAJOIN: %s used SAJOIN to make %s join %s",
					client->name, target->name, jbuf);
			}
			else
			{
				sendnotice(target, "*** You were forced to join %s with '%c'", jbuf, sjmode);
				sendto_umode_global(UMODE_OPER, "%s used SAJOIN to make %s join %c%s", client->name, target->name, sjmode, jbuf);
				ircd_log(LOG_SACMDS,"SAJOIN: %s used SAJOIN to make %s join %c%s",
					client->name, target->name, sjmode, jbuf);
			}
		}
	}
}
