/* src/modules/history.c - (C) 2020 Bram Matthys (Syzop) & The UnrealIRCd Team
 *
 * Simple HISTORY command to fetch channel history.
 * This is one of the interfaces to the channel history backend.
 * The other, most prominent, being history-on-join at the moment.
 * In the future there will likely be an IRCv3 standard which
 * will allow clients to fetch history automatically, which will
 * be implemented under a different command and not really meant
 * for end users. However, it will take a while until such a spec is
 * finalized, let alone when major clients will finally support it.
 *
 * See doc/Authors and git history for additional names of the programmers.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"history",
	"5.0",
	"Simple history command for end-users",
	"UnrealIRCd Team",
	"unrealircd-6",
	};

#define HISTORY_LINES_DEFAULT 100
#define HISTORY_LINES_MAX 100

CMD_FUNC(cmd_history);

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	CommandAdd(modinfo->handle, "HISTORY", cmd_history, MAXPARA, CMD_USER);
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

void history_usage(Client *client)
{
	sendnotice(client, " Use: /HISTORY #channel [lines-to-display]");
	sendnotice(client, "  Ex: /HISTORY #lobby");
	sendnotice(client, "  Ex: /HISTORY #lobby 50");
	sendnotice(client, "The lines-to-display value must be 1-%d, the default is %d",
		HISTORY_LINES_MAX, HISTORY_LINES_DEFAULT);
	sendnotice(client, "Naturally, the line count and time limits in channel mode +H are obeyed");
}

CMD_FUNC(cmd_history)
{
	HistoryFilter filter;
	HistoryResult *r;
	Channel *channel;
	int lines = HISTORY_LINES_DEFAULT;

	if (!MyUser(client))
		return;

	if ((parc < 2) || BadPtr(parv[1]))
	{
		history_usage(client);
		return;
	}

	channel = find_channel(parv[1]);
	if (!channel)
	{
		sendnumeric(client, ERR_NOSUCHCHANNEL, parv[1]);
		return;
	}

	if (!IsMember(client, channel))
	{
		sendnumeric(client, ERR_NOTONCHANNEL, channel->name);
		return;
	}

	if (!has_channel_mode(channel, 'H'))
	{
		sendnotice(client, "Channel %s does not have channel mode +H set", channel->name);
		return;
	}

	if (parv[2])
	{
		lines = atoi(parv[2]);
		if (lines < 1)
		{
			history_usage(client);
			return;
		}
		if (lines > HISTORY_LINES_MAX)
			lines = HISTORY_LINES_MAX;
	}

	if (!HasCapability(client, "server-time"))
	{
		sendnotice(client, "Your IRC client does not support the 'server-time' capability");
		sendnotice(client, "https://ircv3.net/specs/extensions/server-time");
		sendnotice(client, "History request refused.");
		return;
	}

	memset(&filter, 0, sizeof(filter));
	filter.cmd = HFC_SIMPLE;
	filter.last_lines = lines;

	if ((r = history_request(channel->name, &filter)))
	{
		history_send_result(client, r);
		free_history_result(r);
	}
}
