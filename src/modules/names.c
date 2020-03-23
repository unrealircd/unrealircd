/*
 *   IRC - Internet Relay Chat, src/modules/names.c
 *   (C) 2006 The UnrealIRCd Team
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

CMD_FUNC(cmd_names);

#define MSG_NAMES 	"NAMES"

ModuleHeader MOD_HEADER
  = {
	"names",
	"5.0",
	"command /names", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_NAMES, cmd_names, MAXPARA, CMD_USER|CMD_SERVER);
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

/************************************************************************
 * cmd_names() - Added by Jto 27 Apr 1989
 * 12 Feb 2000 - geesh, time for a rewrite -lucas
 ************************************************************************/

static char buf[BUFSIZE];

/*
** cmd_names
**	parv[1] = channel
*/
#define TRUNCATED_NAMES 64
CMD_FUNC(cmd_names)
{
	int multiprefix = (MyConnect(client) && HasCapability(client, "multi-prefix"));
	int uhnames = (MyConnect(client) && HasCapability(client, "userhost-in-names")); // cache UHNAMES support
	int bufLen = NICKLEN + (!uhnames ? 0 : (1 + USERLEN + 1 + HOSTLEN));
	int mlen = strlen(me.name) + bufLen + 7;
	Channel *channel;
	Client *acptr;
	int member;
	Member *cm;
	int idx, flag = 1, spos;
	char *s, *para = parv[1];
	char nuhBuffer[NICKLEN+USERLEN+HOSTLEN+3];

	if (parc < 2 || !MyConnect(client))
	{
		sendnumeric(client, RPL_ENDOFNAMES, "*");
		return;
	}

	for (s = para; *s; s++)
	{
		if (*s == ',')
		{
			if (strlen(para) > TRUNCATED_NAMES)
				para[TRUNCATED_NAMES] = '\0';
			sendto_realops("names abuser %s %s",
			    get_client_name(client, FALSE), para);
			sendnumeric(client, ERR_TOOMANYTARGETS, s+1, 1, "NAMES");
			return;
		}
	}

	channel = find_channel(para, NULL);

	if (!channel || (!ShowChannel(client, channel) && !ValidatePermissionsForPath("channel:see:names:secret",client,NULL,channel,NULL)))
	{
		sendnumeric(client, RPL_ENDOFNAMES, para);
		return;
	}

	/* cache whether this user is a member of this channel or not */
	member = IsMember(client, channel);

	if (PubChannel(channel))
		buf[0] = '=';
	else if (SecretChannel(channel))
		buf[0] = '@';
	else
		buf[0] = '*';

	idx = 1;
	buf[idx++] = ' ';
	for (s = channel->chname; *s; s++)
		buf[idx++] = *s;
	buf[idx++] = ' ';
	buf[idx++] = ':';

	/* If we go through the following loop and never add anything,
	   we need this to be empty, otherwise spurious things from the
	   LAST /names call get stuck in there.. - lucas */
	buf[idx] = '\0';

	spos = idx;		/* starting point in buffer for names! */

	for (cm = channel->members; cm; cm = cm->next)
	{
		acptr = cm->client;
		if (IsInvisible(acptr) && !member && !ValidatePermissionsForPath("channel:see:names:invisible",client,acptr,channel,NULL))
			continue;

		if (!user_can_see_member(client, acptr, channel))
			continue; /* invisible (eg: due to delayjoin) */

		if (!multiprefix)
		{
			/* Standard NAMES reply */
#ifdef PREFIX_AQ
			if (cm->flags & CHFL_CHANOWNER)
				buf[idx++] = '~';
			else if (cm->flags & CHFL_CHANADMIN)
				buf[idx++] = '&';
			else
#endif
			if (cm->flags & CHFL_CHANOP)
				buf[idx++] = '@';
			else if (cm->flags & CHFL_HALFOP)
				buf[idx++] = '%';
			else if (cm->flags & CHFL_VOICE)
				buf[idx++] = '+';
		} else {
			/* NAMES reply with all rights included (multi-prefix / NAMESX) */
#ifdef PREFIX_AQ
			if (cm->flags & CHFL_CHANOWNER)
				buf[idx++] = '~';
			if (cm->flags & CHFL_CHANADMIN)
				buf[idx++] = '&';
#endif
			if (cm->flags & CHFL_CHANOP)
				buf[idx++] = '@';
			if (cm->flags & CHFL_HALFOP)
				buf[idx++] = '%';
			if (cm->flags & CHFL_VOICE)
				buf[idx++] = '+';
		}

		if (!uhnames) {
			s = acptr->name;
		} else {
			strlcpy(nuhBuffer,
			        make_nick_user_host(acptr->name, acptr->user->username, GetHost(acptr)),
				bufLen + 1);
			s = nuhBuffer;
		}
		/* 's' is intialized above to point to either acptr->name (normal),
		 * or to nuhBuffer (for UHNAMES).
		 */
		for (; *s; s++)
			buf[idx++] = *s;
		if (cm->next)
			buf[idx++] = ' ';
		buf[idx] = '\0';
		flag = 1;
		if (mlen + idx + bufLen > BUFSIZE - 7)
		{
			sendnumeric(client, RPL_NAMREPLY, buf);
			idx = spos;
			flag = 0;
		}
	}

	if (flag)
		sendnumeric(client, RPL_NAMREPLY, buf);

	sendnumeric(client, RPL_ENDOFNAMES, para);
}
