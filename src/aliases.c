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

/* m_alias is a special type of command, it has an extra argument 'cmd'. */
static int recursive_alias = 0;
int m_alias(Client *cptr, Client *sptr, MessageTag *mtags, int parc, char *parv[], char *cmd)
{
	ConfigItem_alias *alias;
	Client *acptr;
	int ret;

	if (!(alias = Find_alias(cmd))) 
	{
		sendto_one(sptr, NULL, ":%s %d %s %s :Unknown command",
			me.name, ERR_UNKNOWNCOMMAND, sptr->name, cmd);
		return 0;
	}
	
	/* If it isn't an ALIAS_COMMAND, we require a paramter ... We check ALIAS_COMMAND LATER */
	if (alias->type != ALIAS_COMMAND && (parc < 2 || *parv[1] == '\0'))
	{
		sendnumeric(sptr, ERR_NOTEXTTOSEND);
		return -1;
	}

	if (alias->type == ALIAS_SERVICES) 
	{
		if (SERVICES_NAME && (acptr = find_person(alias->nick, NULL)))
		{
			if (alias->spamfilter && (ret = run_spamfilter(sptr, parv[1], SPAMF_USERMSG, alias->nick, 0, NULL)) < 0)
				return ret;
			sendto_one(acptr, NULL, ":%s PRIVMSG %s@%s :%s", sptr->name,
				alias->nick, SERVICES_NAME, parv[1]);
		}
		else
			sendnumeric(sptr, ERR_SERVICESDOWN, alias->nick);
	}
	else if (alias->type == ALIAS_STATS) 
	{
		if (STATS_SERVER && (acptr = find_person(alias->nick, NULL)))
		{
			if (alias->spamfilter && (ret = run_spamfilter(sptr, parv[1], SPAMF_USERMSG, alias->nick, 0, NULL)) < 0)
				return ret;
			sendto_one(acptr, NULL, ":%s PRIVMSG %s@%s :%s", sptr->name,
				alias->nick, STATS_SERVER, parv[1]);
		}
		else
			sendnumeric(sptr, ERR_SERVICESDOWN, alias->nick);
	}
	else if (alias->type == ALIAS_NORMAL) 
	{
		if ((acptr = find_person(alias->nick, NULL))) 
		{
			if (alias->spamfilter && (ret = run_spamfilter(sptr, parv[1], SPAMF_USERMSG, alias->nick, 0, NULL)) < 0)
				return ret;
			if (MyClient(acptr))
				sendto_one(acptr, NULL, ":%s!%s@%s PRIVMSG %s :%s", sptr->name, 
					sptr->user->username, GetHost(sptr),
					alias->nick, parv[1]);
			else
				sendto_one(acptr, NULL, ":%s PRIVMSG %s :%s", sptr->name,
					alias->nick, parv[1]);
		}
		else
			sendnumeric(sptr, ERR_NOSUCHNICK, alias->nick);
	}
	else if (alias->type == ALIAS_CHANNEL)
	{
		Channel *chptr;
		if ((chptr = find_channel(alias->nick, NULL)))
		{
			char *msg = parv[1];
			char *errmsg = NULL;
			if (!can_send(sptr, chptr, &msg, &errmsg, 0))
			{
				if (alias->spamfilter && (ret = run_spamfilter(sptr, parv[1], SPAMF_CHANMSG, chptr->chname, 0, NULL)) < 0)
					return ret;
				new_message(sptr, NULL, &mtags);
				sendto_channel(chptr, sptr, sptr,
				               PREFIX_ALL, 0, SEND_ALL|SKIP_DEAF, mtags,
				               ":%s PRIVMSG %s :%s",
				               sptr->name, chptr->chname, parv[1]);
				free_message_tags(mtags);
				return 0;
			}
		}
		sendnumeric(sptr, ERR_CANNOTDOCOMMAND,
				cmd, "You may not use this command at this time");
	}
	else if (alias->type == ALIAS_COMMAND) 
	{
		ConfigItem_alias_format *format;
		char *ptr = "";

		if (!(parc < 2 || *parv[1] == '\0'))
			ptr = parv[1]; 

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
							strlcat(output, sptr->name, sizeof output);
							j += strlen(sptr->name);
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
					sendnumeric(sptr, ERR_NEEDMOREPARAMS, cmd);
					return -1;
				}
				
				if (format->type == ALIAS_SERVICES) 
				{
					if (SERVICES_NAME && (acptr = find_person(format->nick, NULL)))
					{
						if (alias->spamfilter && (ret = run_spamfilter(sptr, output, SPAMF_USERMSG, format->nick, 0, NULL)) < 0)
							return ret;
						sendto_one(acptr, NULL, ":%s PRIVMSG %s@%s :%s", sptr->name,
							format->nick, SERVICES_NAME, output);
					} else
						sendnumeric(sptr, ERR_SERVICESDOWN, format->nick);
				}
				else if (format->type == ALIAS_STATS) 
				{
					if (STATS_SERVER && (acptr = find_person(format->nick, NULL)))
					{
						if (alias->spamfilter && (ret = run_spamfilter(sptr, output, SPAMF_USERMSG, format->nick, 0, NULL)) < 0)
							return ret;
						sendto_one(acptr, NULL, ":%s PRIVMSG %s@%s :%s", sptr->name,
							format->nick, STATS_SERVER, output);
					} else
						sendnumeric(sptr, ERR_SERVICESDOWN, format->nick);
				}
				else if (format->type == ALIAS_NORMAL) 
				{
					if ((acptr = find_person(format->nick, NULL))) 
					{
						if (alias->spamfilter && (ret = run_spamfilter(sptr, output, SPAMF_USERMSG, format->nick, 0, NULL)) < 0)
							return ret;
						if (MyClient(acptr))
							sendto_one(acptr, NULL, ":%s!%s@%s PRIVMSG %s :%s", sptr->name, 
							sptr->user->username, IsHidden(sptr) ? sptr->user->virthost : sptr->user->realhost,
							format->nick, output);
						else
							sendto_one(acptr, NULL, ":%s PRIVMSG %s :%s", sptr->name,
								format->nick, output);
					}
					else
						sendnumeric(sptr, ERR_NOSUCHNICK, format->nick);
				}
				else if (format->type == ALIAS_CHANNEL)
				{
					Channel *chptr;
					if ((chptr = find_channel(format->nick, NULL)))
					{
						char *msg = output;
						char *errmsg = NULL;
						if (!can_send(sptr, chptr, &msg, &errmsg, 0))
						{
							if (alias->spamfilter && (ret = run_spamfilter(sptr, output, SPAMF_CHANMSG, chptr->chname, 0, NULL)) < 0)
								return ret;
							new_message(sptr, NULL, &mtags);
							sendto_channel(chptr, sptr, sptr,
							               PREFIX_ALL, 0, SEND_ALL|SKIP_DEAF, mtags,
							               ":%s PRIVMSG %s :%s",
							               sptr->name, chptr->chname, parv[1]);
							free_message_tags(mtags);
							return 0;
						}
					}
					sendnumeric(sptr, ERR_CANNOTDOCOMMAND, cmd, 
						"You may not use this command at this time");
				}
				else if (format->type == ALIAS_REAL)
				{
					int ret;
					char mybuf[500];
					
					snprintf(mybuf, sizeof(mybuf), "%s %s", format->nick, output);

					if (recursive_alias)
					{
						sendnumeric(sptr, ERR_CANNOTDOCOMMAND, cmd, "You may not use this command at this time -- recursion");
						return -1;
					}

					recursive_alias = 1;
					ret = parse(sptr, mybuf, strlen(mybuf));
					recursive_alias = 0;

					return ret;
				}
				break;
			}
		}
		return 0;
	}
	return 0;
}
