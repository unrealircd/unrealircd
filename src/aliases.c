/*
 *   Unreal Internet Relay Chat Daemon, src/aliases.c
 *   (C) 2000-2001 Carsten V. Munk and the UnrealIRCd Team
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

/* Function to return a group of tokens -- codemastr */
void strrangetok(char *in, char *out, char tok, short first, short last) {
	int i = 0, tokcount = 0, j = 0;
	first--;
	last--;
	while(in[i]) {
		if (in[i] == tok) {
			tokcount++;
			if (tokcount == first)
				i++;
		}
		if (tokcount >= first && (tokcount <= last || last == -1)) {
			out[j] = in[i];
			j++;
		}
		i++;
	}
	out[j] = 0;
}			

/* cmd_alias is a special type of command, it has an extra argument 'cmd'. */
static int recursive_alias = 0;

void cmd_alias(Client *client, MessageTag *mtags, int parc, const char *parv[], const char *cmd)
{
	ConfigItem_alias *alias;
	Client *acptr;
	int ret;
	char request[BUFSIZE];

	if (!(alias = find_alias(cmd))) 
	{
		sendto_one(client, NULL, ":%s %d %s %s :Unknown command",
			me.name, ERR_UNKNOWNCOMMAND, client->name, cmd);
		return;
	}
	
	/* If it isn't an ALIAS_COMMAND, we require a paramter ... We check ALIAS_COMMAND LATER */
	if (alias->type != ALIAS_COMMAND && (parc < 2 || *parv[1] == '\0'))
	{
		sendnumeric(client, ERR_NOTEXTTOSEND);
		return;
	}

	if (alias->type == ALIAS_SERVICES) 
	{
		if (SERVICES_NAME && (acptr = find_user(alias->nick, NULL)))
		{
			if (alias->spamfilter && match_spamfilter(client, parv[1], SPAMF_USERMSG, cmd, alias->nick, 0, NULL))
				return;
			sendto_one(acptr, NULL, ":%s PRIVMSG %s@%s :%s", client->name,
				alias->nick, SERVICES_NAME, parv[1]);
		}
		else
			sendnumeric(client, ERR_SERVICESDOWN, alias->nick);
	}
	else if (alias->type == ALIAS_STATS) 
	{
		if (STATS_SERVER && (acptr = find_user(alias->nick, NULL)))
		{
			if (alias->spamfilter && match_spamfilter(client, parv[1], SPAMF_USERMSG, cmd, alias->nick, 0, NULL))
				return;
			sendto_one(acptr, NULL, ":%s PRIVMSG %s@%s :%s", client->name,
				alias->nick, STATS_SERVER, parv[1]);
		}
		else
			sendnumeric(client, ERR_SERVICESDOWN, alias->nick);
	}
	else if (alias->type == ALIAS_NORMAL) 
	{
		if ((acptr = find_user(alias->nick, NULL))) 
		{
			if (alias->spamfilter && match_spamfilter(client, parv[1], SPAMF_USERMSG, cmd, alias->nick, 0, NULL))
				return;
			if (MyUser(acptr))
				sendto_one(acptr, NULL, ":%s!%s@%s PRIVMSG %s :%s", client->name, 
					client->user->username, GetHost(client),
					alias->nick, parv[1]);
			else
				sendto_one(acptr, NULL, ":%s PRIVMSG %s :%s", client->name,
					alias->nick, parv[1]);
		}
		else
			sendnumeric(client, ERR_NOSUCHNICK, alias->nick);
	}
	else if (alias->type == ALIAS_CHANNEL)
	{
		Channel *channel;
		if ((channel = find_channel(alias->nick)))
		{
			const char *msg = parv[1];
			const char *errmsg = NULL;
			if (can_send_to_channel(client, channel, &msg, &errmsg, 0))
			{
				if (alias->spamfilter && match_spamfilter(client, parv[1], SPAMF_CHANMSG, cmd, channel->name, 0, NULL))
					return;
				new_message(client, NULL, &mtags);
				sendto_channel(channel, client, client->direction,
				               NULL, 0, SEND_ALL|SKIP_DEAF, mtags,
				               ":%s PRIVMSG %s :%s",
				               client->name, channel->name, parv[1]);
				free_message_tags(mtags);
				return;
			}
		}
		sendnumeric(client, ERR_CANNOTDOCOMMAND,
				cmd, "You may not use this command at this time");
	}
	else if (alias->type == ALIAS_COMMAND) 
	{
		ConfigItem_alias_format *format;
		char *ptr = "";

		if (!(parc < 2 || *parv[1] == '\0'))
		{
			strlcpy(request, parv[1], sizeof(request));
			ptr = request;
		}

		for (format = alias->format; format; format = format->next)
		{
			if (unreal_match(format->expr, ptr))
			{
				/* Parse the parameters */
				int i = 0, j = 0, k = 1;
				char output[1024], current[1024];
				char nums[4];

				memset(current, 0, sizeof(current));
				memset(output, 0, sizeof(output));

				while(format->parameters[i] && j < 500) 
				{
					k = 0;
					if (format->parameters[i] == '%') 
					{
						i++;
						if (format->parameters[i] == '%') 
							output[j++] = '%';
						else if (isdigit(format->parameters[i])) 
						{
							for(; isdigit(format->parameters[i]) && k < 2; i++, k++) {
								nums[k] = format->parameters[i];
							}
							nums[k] = 0;
							i--;
							if (format->parameters[i+1] == '-') {
								strrangetok(ptr, current, ' ', atoi(nums),0);
								i++;
							}
							else 
								strrangetok(ptr, current, ' ', atoi(nums), atoi(nums));
							if (!*current)
								continue;
							if (j + strlen(current)+1 >= 500)
								break;
							strlcat(output, current, sizeof output);
							j += strlen(current);
							
						}
						else if (format->parameters[i] == 'n' ||
							 format->parameters[i] == 'N')
						{
							strlcat(output, client->name, sizeof output);
							j += strlen(client->name);
						}
						else 
						{
							output[j++] = '%';
							output[j++] = format->parameters[i];
						}
						i++;
						continue;
					}
					output[j++] = format->parameters[i++];
				}
				output[j] = 0;
				/* Now check to make sure we have something to send */
				if (strlen(output) == 0)
				{
					sendnumeric(client, ERR_NEEDMOREPARAMS, cmd);
					return;
				}
				
				if (format->type == ALIAS_SERVICES) 
				{
					if (SERVICES_NAME && (acptr = find_user(format->nick, NULL)))
					{
						if (alias->spamfilter && match_spamfilter(client, output, SPAMF_USERMSG, cmd, format->nick, 0, NULL))
							return;
						sendto_one(acptr, NULL, ":%s PRIVMSG %s@%s :%s", client->name,
							format->nick, SERVICES_NAME, output);
					} else
						sendnumeric(client, ERR_SERVICESDOWN, format->nick);
				}
				else if (format->type == ALIAS_STATS) 
				{
					if (STATS_SERVER && (acptr = find_user(format->nick, NULL)))
					{
						if (alias->spamfilter && match_spamfilter(client, output, SPAMF_USERMSG, cmd, format->nick, 0, NULL))
							return;
						sendto_one(acptr, NULL, ":%s PRIVMSG %s@%s :%s", client->name,
							format->nick, STATS_SERVER, output);
					} else
						sendnumeric(client, ERR_SERVICESDOWN, format->nick);
				}
				else if (format->type == ALIAS_NORMAL) 
				{
					if ((acptr = find_user(format->nick, NULL))) 
					{
						if (alias->spamfilter && match_spamfilter(client, output, SPAMF_USERMSG, cmd, format->nick, 0, NULL))
							return;
						if (MyUser(acptr))
							sendto_one(acptr, NULL, ":%s!%s@%s PRIVMSG %s :%s", client->name, 
							client->user->username, IsHidden(client) ? client->user->virthost : client->user->realhost,
							format->nick, output);
						else
							sendto_one(acptr, NULL, ":%s PRIVMSG %s :%s", client->name,
								format->nick, output);
					}
					else
						sendnumeric(client, ERR_NOSUCHNICK, format->nick);
				}
				else if (format->type == ALIAS_CHANNEL)
				{
					Channel *channel;
					if ((channel = find_channel(format->nick)))
					{
						const char *msg = output;
						const char *errmsg = NULL;
						if (!can_send_to_channel(client, channel, &msg, &errmsg, 0))
						{
							if (alias->spamfilter && match_spamfilter(client, output, SPAMF_CHANMSG, cmd, channel->name, 0, NULL))
								return;
							new_message(client, NULL, &mtags);
							sendto_channel(channel, client, client->direction,
							               NULL, 0, SEND_ALL|SKIP_DEAF, mtags,
							               ":%s PRIVMSG %s :%s",
							               client->name, channel->name, parv[1]);
							free_message_tags(mtags);
							return;
						}
					}
					sendnumeric(client, ERR_CANNOTDOCOMMAND, cmd, 
						"You may not use this command at this time");
				}
				else if (format->type == ALIAS_REAL)
				{
					int ret;
					char mybuf[500];
					
					snprintf(mybuf, sizeof(mybuf), "%s %s", format->nick, output);

					if (recursive_alias)
					{
						sendnumeric(client, ERR_CANNOTDOCOMMAND, cmd, "You may not use this command at this time -- recursion");
						return;
					}

					recursive_alias = 1;
					parse(client, mybuf, strlen(mybuf));
					recursive_alias = 0;

					return;
				}
				break;
			}
		}
	}
}
