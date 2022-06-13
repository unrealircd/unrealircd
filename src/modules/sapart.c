/*
 *   IRC - Internet Relay Chat, src/modules/sapart.c
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

CMD_FUNC(cmd_sapart);

#define MSG_SAPART 	"SAPART"	

ModuleHeader MOD_HEADER
  = {
	"sapart",
	"5.0",
	"command /sapart", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_SAPART, cmd_sapart, 3, CMD_USER|CMD_SERVER);
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

static void log_sapart(Client *client, Client *target, const char *channels, const char *comment)
{
	if (comment)
	{
		unreal_log(ULOG_INFO, "sacmds", "SAPART_COMMAND", client, "SAPART: $client used SAPART to make $target part $channels ($reason)",
			   log_data_client("target", target),
			   log_data_string("channels", channels),
			   log_data_string("reason", comment));
	}
	else
	{
		unreal_log(ULOG_INFO, "sacmds", "SAPART_COMMAND", client, "SAPART: $client used SAPART to make $target part $channels",
			   log_data_client("target", target),
			   log_data_string("channels", channels));
	}
}


/* cmd_sapart() - Lamego - Wed Jul 21 20:04:48 1999
   Copied off PTlink IRCd (C) PTlink coders team.
   Coded for Sadmin by Stskeeps
   also Modified by NiQuiL (niquil@programmer.net)
	parv[1] - nick to make part
	parv[2] - channel(s) to part
	parv[3] - comment
*/

CMD_FUNC(cmd_sapart)
{
	Client *target;
	Channel *channel;
	Membership *lp;
	char *name, *p = NULL;
	int i;
	const char *comment = (parc > 3 && parv[3] ? parv[3] : NULL);
	char commentx[512];
	char request[BUFSIZE];
	char jbuf[BUFSIZE];
	int ntargets = 0;
	int maxtargets = max_targets_for_command("SAPART");

	if ((parc < 3) || BadPtr(parv[2]))
        {
                sendnumeric(client, ERR_NEEDMOREPARAMS, "SAPART");
                return;
        }

        if (!(target = find_user(parv[1], NULL)))
        {
                sendnumeric(client, ERR_NOSUCHNICK, parv[1]);
                return;
        }

	/* See if we can operate on this vicim/this command */
	if (!ValidatePermissionsForPath("sacmd:sapart",client,target,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	/* Broadcast so other servers can log it appropriately as an SAPART */
	if (parv[3])
		sendto_server(client, 0, 0, recv_mtags, ":%s SAPART %s %s :%s", client->id, target->id, parv[2], comment);
	else
		sendto_server(client, 0, 0, recv_mtags, ":%s SAPART %s %s", client->id, target->id, parv[2]);

	if (!MyUser(target))
	{
		log_sapart(client, target, parv[2], comment);
		return;
	}

	/* 'target' is our client... */

	*jbuf = 0;
	strlcpy(request, parv[2], sizeof(request));
	for (i = 0, name = strtoken(&p, request, ","); name; name = strtoken(&p, NULL, ","))
	{
		if (++ntargets > maxtargets)
		{
			sendnumeric(client, ERR_TOOMANYTARGETS, name, maxtargets, "SAPART");
			break;
		}

		if (!(channel = find_channel(name)))
		{
			sendnumeric(client, ERR_NOSUCHCHANNEL, name);
			continue;
		}

		/* Validate oper can do this on chan/victim */
		if (!IsULine(client) && !ValidatePermissionsForPath("sacmd:sapart",client,target,channel,NULL))
		{
			sendnumeric(client, ERR_NOPRIVILEGES);
			continue;
		}

		if (!(lp = find_membership_link(target->user->channel, channel)))
		{
			sendnumeric(client, ERR_USERNOTINCHANNEL, name, target->name);
			continue;
		}
		if (*jbuf)
			strlcat(jbuf, ",", sizeof jbuf);
		strlncat(jbuf, name, sizeof jbuf, sizeof(jbuf) - i - 1);
		i += strlen(name) + 1;
	}

	if (!*jbuf)
		return;

	strlcpy(request, jbuf, sizeof(request));

	log_sapart(client, target, request, comment);

	if (comment)
	{
		snprintf(commentx, sizeof(commentx), "SAPart: %s", comment);
		sendnotice(target, "*** You were forced to part %s (%s)", request, commentx);
	} else {
		sendnotice(target, "*** You were forced to part %s", request);
	}

	parv[0] = target->name; // nick
	parv[1] = request; // chan
	parv[2] = comment ? commentx : NULL; // comment

	/* Now, do the actual parting: */
	do_cmd(target, NULL, "PART", comment ? 3 : 2, parv);

	/* NOTE: target may be killed now due to the part reason @ spamfilter */
}
