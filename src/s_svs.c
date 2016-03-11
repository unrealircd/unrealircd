/*
 *   Unreal Internet Relay Chat Daemon, src/s_svs.c
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
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "channel.h"
#include <fcntl.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <sys/wait.h>
#else
#include <io.h>
#endif
#include <sys/stat.h>
#ifdef __hpux
#include "inet.h"
#endif
#if defined(PCS) || defined(AIX) || defined(SVR3)
#include <time.h>
#endif
#include <string.h>

#include "h.h"
#include "proto.h"

extern ircstats IRCstats;

aConfiguration iConf;

/* ok, given a mask, our job is to determine
 * wether or not it's a safe mask to banish...
 *
 * userhost= mask to verify
 * ipstat= TRUE  == it's an ip
 *         FALSE == it's a hostname
 *         UNSURE == we need to find out
 * return value
 *         TRUE  == mask is ok
 *         FALSE == mask is not ok
 *        UNSURE == [unused] something went wrong
 */

int advanced_check(char *userhost, int ipstat)
{
	register int retval = TRUE;
	char *up = NULL, *p, *thisseg;
	int  numdots = 0, segno = 0, numseg, i = 0;
	char *ipseg[10 + 2];
	char safebuffer[512] = "";	/* buffer strtoken() can mess up to its heart's content...;> */

	strlcpy(safebuffer, userhost, sizeof safebuffer);

#define userhost safebuffer
#define IP_WILDS_OK(x) ((x)<2? 0 : 1)

	if (ipstat == UNSURE)
	{
		ipstat = TRUE;
		for (; *up; up++)
		{
			if (*up == '.')
				numdots++;
			if (!isdigit(*up) && !ispunct(*up))
			{
				ipstat = FALSE;
				continue;
			}
		}
		if (numdots != 3)
			ipstat = FALSE;
		if (numdots < 1 || numdots > 9)
			return (0);
	}

	/* fill in the segment set */
	{
		int  l = 0;
		for (segno = 0, i = 0, thisseg = strtoken(&p, userhost, ".");
		    thisseg; thisseg = strtoken(&p, NULL, "."), i++)
		{

			l = strlen(thisseg) + 2;
			ipseg[segno] = calloc(1, l);
			strncpy(ipseg[segno++], thisseg, l);
		}
	}
	if (segno < 2 && ipstat == TRUE)
		retval = FALSE;
	numseg = segno;
	if (ipstat == TRUE)
		for (i = 0; i < numseg; i++)
		{
			if (!IP_WILDS_OK(i) && (index(ipseg[i], '*')
			    || index(ipseg[i], '?')))
				retval = FALSE;
			/* The person who wrote this function was braindead --Stskeeps */
			/* MyFree(ipseg[i]); */
		}
	else
	{
		int  wildsok = 0;

		for (i = 0; i < numseg; i++)
		{
			/* for hosts, let the mask extent all the way to 
			   the second-level domain... */
			wildsok = 1;
			if (i == numseg || (i + 1) == numseg)
				wildsok = 0;
			if (wildsok == 0 && (index(ipseg[i], '*')
			    || index(ipseg[i], '?')))
			{
				retval = FALSE;
			}
			/* MyFree(ipseg[i]); */
		}


	}

	return (retval);
#undef userhost
#undef IP_WILDS_OK

}

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
int m_alias(aClient *cptr, aClient *sptr, int parc, char *parv[], char *cmd)
{
ConfigItem_alias *alias;
aClient *acptr;
int ret;

	if (!(alias = Find_alias(cmd))) 
	{
		sendto_one(sptr, ":%s %d %s %s :Unknown command",
			me.name, ERR_UNKNOWNCOMMAND, sptr->name, cmd);
		return 0;
	}
	
	/* If it isn't an ALIAS_COMMAND, we require a paramter ... We check ALIAS_COMMAND LATER */
	if (alias->type != ALIAS_COMMAND && (parc < 2 || *parv[1] == '\0'))
	{
		sendto_one(sptr, err_str(ERR_NOTEXTTOSEND), me.name, sptr->name);
		return -1;
	}

	if (alias->type == ALIAS_SERVICES) 
	{
		if (SERVICES_NAME && (acptr = find_person(alias->nick, NULL)))
		{
			if (alias->spamfilter && (ret = dospamfilter(sptr, parv[1], SPAMF_USERMSG, alias->nick, 0, NULL)) < 0)
				return ret;
			sendto_one(acptr, ":%s PRIVMSG %s@%s :%s", sptr->name,
				alias->nick, SERVICES_NAME, parv[1]);
		}
		else
			sendto_one(sptr, err_str(ERR_SERVICESDOWN), me.name,
				sptr->name, alias->nick);
	}
	else if (alias->type == ALIAS_STATS) 
	{
		if (STATS_SERVER && (acptr = find_person(alias->nick, NULL)))
		{
			if (alias->spamfilter && (ret = dospamfilter(sptr, parv[1], SPAMF_USERMSG, alias->nick, 0, NULL)) < 0)
				return ret;
			sendto_one(acptr, ":%s PRIVMSG %s@%s :%s", sptr->name,
				alias->nick, STATS_SERVER, parv[1]);
		}
		else
			sendto_one(sptr, err_str(ERR_SERVICESDOWN), me.name,
				sptr->name, alias->nick);
	}
	else if (alias->type == ALIAS_NORMAL) 
	{
		if ((acptr = find_person(alias->nick, NULL))) 
		{
			if (alias->spamfilter && (ret = dospamfilter(sptr, parv[1], SPAMF_USERMSG, alias->nick, 0, NULL)) < 0)
				return ret;
			if (MyClient(acptr))
				sendto_one(acptr, ":%s!%s@%s PRIVMSG %s :%s", sptr->name, 
					sptr->user->username, GetHost(sptr),
					alias->nick, parv[1]);
			else
				sendto_one(acptr, ":%s PRIVMSG %s :%s", sptr->name,
					alias->nick, parv[1]);
		}
		else
			sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name,
				sptr->name, alias->nick);
	}
	else if (alias->type == ALIAS_CHANNEL)
	{
		aChannel *chptr;
		if ((chptr = find_channel(alias->nick, NULL)))
		{
			if (!can_send(sptr, chptr, parv[1], 0))
			{
				if (alias->spamfilter && (ret = dospamfilter(sptr, parv[1], SPAMF_CHANMSG, chptr->chname, 0, NULL)) < 0)
					return ret;
				sendto_channelprefix_butone(sptr,
				    sptr, chptr, PREFIX_ALL,
                                    ":%s PRIVMSG %s :%s", sptr->name,
				    chptr->chname, parv[1]);
				return 0;
			}
		}
		sendto_one(sptr, err_str(ERR_CANNOTDOCOMMAND), me.name, sptr->name,
				cmd, "You may not use this command at this time");
	}
	else if (alias->type == ALIAS_COMMAND) 
	{
		ConfigItem_alias_format *format;
		char *ptr = "";
		if (!(parc < 2 || *parv[1] == '\0'))
			ptr = parv[1]; 
		for (format = alias->format; format; format = (ConfigItem_alias_format *)format->next) 
		{
			if (unreal_match(format->expr, ptr))
			{
				/* Parse the parameters */
				int i = 0, j = 0, k = 1;
				char output[1024], current[1024];
				char nums[4];

				bzero(current, sizeof current);
				bzero(output, sizeof output);

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
					sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, sptr->name, cmd);
					return -1;
				}
				
				if (format->type == ALIAS_SERVICES) 
				{
					if (SERVICES_NAME && (acptr = find_person(format->nick, NULL)))
					{
						if (alias->spamfilter && (ret = dospamfilter(sptr, output, SPAMF_USERMSG, format->nick, 0, NULL)) < 0)
							return ret;
						sendto_one(acptr, ":%s PRIVMSG %s@%s :%s", sptr->name,
							format->nick, SERVICES_NAME, output);
					} else
						sendto_one(sptr, err_str(ERR_SERVICESDOWN), me.name,
							sptr->name, format->nick);
				}
				else if (format->type == ALIAS_STATS) 
				{
					if (STATS_SERVER && (acptr = find_person(format->nick, NULL)))
					{
						if (alias->spamfilter && (ret = dospamfilter(sptr, output, SPAMF_USERMSG, format->nick, 0, NULL)) < 0)
							return ret;
						sendto_one(acptr, ":%s PRIVMSG %s@%s :%s", sptr->name,
							format->nick, STATS_SERVER, output);
					} else
						sendto_one(sptr, err_str(ERR_SERVICESDOWN), me.name,
							sptr->name, format->nick);
				}
				else if (format->type == ALIAS_NORMAL) 
				{
					if ((acptr = find_person(format->nick, NULL))) 
					{
						if (alias->spamfilter && (ret = dospamfilter(sptr, output, SPAMF_USERMSG, format->nick, 0, NULL)) < 0)
							return ret;
						if (MyClient(acptr))
							sendto_one(acptr, ":%s!%s@%s PRIVMSG %s :%s", sptr->name, 
							sptr->user->username, IsHidden(sptr) ? sptr->user->virthost : sptr->user->realhost,
							format->nick, output);
						else
							sendto_one(acptr, ":%s PRIVMSG %s :%s", sptr->name,
								format->nick, output);
					}
					else
						sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name,
							sptr->name, format->nick);
				}
				else if (format->type == ALIAS_CHANNEL)
				{
					aChannel *chptr;
					if ((chptr = find_channel(format->nick, NULL)))
					{
						if (!can_send(sptr, chptr, output, 0))
						{
							if (alias->spamfilter && (ret = dospamfilter(sptr, output, SPAMF_CHANMSG, chptr->chname, 0, NULL)) < 0)
								return ret;
							sendto_channelprefix_butone(sptr,
							    sptr, chptr, PREFIX_ALL,
			                                    ":%s PRIVMSG %s :%s", sptr->name,
							    chptr->chname, parv[1]);
							return 0;
						}
					}
					sendto_one(sptr, err_str(ERR_CANNOTDOCOMMAND), me.name,
						 sptr->name, cmd, 
						"You may not use this command at this time");
				}
				else if (format->type == ALIAS_REAL)
				{
					int ret;
					char mybuf[500];
					
					snprintf(mybuf, sizeof(mybuf), "%s %s", format->nick, output);

					if (recursive_alias)
					{
						sendto_one(sptr, err_str(ERR_CANNOTDOCOMMAND), me.name, sptr->name, cmd, "You may not use this command at this time -- recursion");
						return -1;
					}

					recursive_alias = 1;
					ret = parse(sptr, mybuf, mybuf+strlen(mybuf));
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
