/************************************************************************
 *   Unreal Internet Relay Chat Daemon, src/packet.c
 *   Copyright (C) 1988-1990 Jarkko Oikarinen and
 *                 University of Oulu, Computing Center
 *   Copyright (C) 1999-present UnrealIRCd team
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

RealCommand *CommandHash[256]; /* one per letter */

/*
** dopacket
**	client - pointer to client structure for which the buffer data
**	       applies.
**	buffer - pointr to the buffer containing the newly read data
**	length - number of valid bytes of data in the buffer
**
** Note:
**	It is implicitly assumed that dopacket is called only
**	with client of "local" variation, which contains all the
**	necessary fields (buffer etc..)
**
** Rewritten for linebufs, 19th May 2013. --kaniini
*/
void dopacket(Client *client, char *buffer, int length)
{
	me.local->receiveB += length;	/* Update bytes received */
	client->local->receiveB += length;
	if (client->local->receiveB > 1023)
	{
		client->local->receiveK += (client->local->receiveB >> 10);
		client->local->receiveB &= 0x03ff;	/* 2^10 = 1024, 3ff = 1023 */
	}
	if (me.local->receiveB > 1023)
	{
		me.local->receiveK += (me.local->receiveB >> 10);
		me.local->receiveB &= 0x03ff;
	}

	me.local->receiveM += 1;	/* Update messages received */
	client->local->receiveM += 1;

	parse(client, buffer, length);
}

void	init_CommandHash(void)
{
#ifdef DEVELOP_DEBUG
	RealCommand	 *p;
	int		 i;
	long		chainlength;
#endif
	
	memset(CommandHash, 0, sizeof(CommandHash));
	CommandAdd(NULL, MSG_ERROR, cmd_error, MAXPARA, CMD_UNREGISTERED|CMD_SERVER);
	CommandAdd(NULL, MSG_VERSION, cmd_version, MAXPARA, CMD_UNREGISTERED|CMD_USER|CMD_SERVER);
	CommandAdd(NULL, MSG_INFO, cmd_info, MAXPARA, CMD_USER);
	CommandAdd(NULL, MSG_DNS, cmd_dns, MAXPARA, CMD_USER);
	CommandAdd(NULL, MSG_REHASH, cmd_rehash, MAXPARA, CMD_USER|CMD_SERVER);
	CommandAdd(NULL, MSG_RESTART, cmd_restart, 2, CMD_USER);
	CommandAdd(NULL, MSG_DIE, cmd_die, MAXPARA, CMD_USER);
	CommandAdd(NULL, MSG_DALINFO, cmd_dalinfo, MAXPARA, CMD_USER);
	CommandAdd(NULL, MSG_CREDITS, cmd_credits, MAXPARA, CMD_USER);
	CommandAdd(NULL, MSG_LICENSE, cmd_license, MAXPARA, CMD_USER);
	CommandAdd(NULL, MSG_MODULE, cmd_module, MAXPARA, CMD_USER);
		
#ifdef DEVELOP_DEBUG
	for (i = 0; i <= 255; i++)
	{
		chainlength = 0;
		for (p = CommandHash[i]; p; p = p->next)
			chainlength++;
		if (chainlength)
			fprintf(stderr, "%c chainlength = %i\r\n",
					i, chainlength);
	}				
#endif
}

RealCommand *add_Command_backend(char *cmd)
{
	RealCommand *c = safe_alloc(sizeof(RealCommand));

	safe_strdup(c->cmd, cmd);

	/* Add in hash with hash value = first byte */
	AddListItem(c, CommandHash[toupper(*cmd)]);

	return c;
}

static inline RealCommand *find_Cmd(char *cmd, int flags)
{
	RealCommand *p;
	for (p = CommandHash[toupper(*cmd)]; p; p = p->next) {
		if ((flags & CMD_UNREGISTERED) && !(p->flags & CMD_UNREGISTERED))
			continue;
		if ((flags & CMD_SHUN) && !(p->flags & CMD_SHUN))
			continue;
		if ((flags & CMD_VIRUS) && !(p->flags & CMD_VIRUS))
			continue;
		if ((flags & CMD_ALIAS) && !(p->flags & CMD_ALIAS))
			continue;
		if (!strcasecmp(p->cmd, cmd))
			return p;
	}
	return NULL;
}

RealCommand *find_Command(char *cmd, short token, int flags)
{
	Debug((DEBUG_NOTICE, "FindCommand %s", cmd));

	return find_Cmd(cmd, flags);
}

RealCommand *find_Command_simple(char *cmd)
{
	RealCommand	*p;
	
	for (p = CommandHash[toupper(*cmd)]; p; p = p->next) {
		if (!strcasecmp(p->cmd, cmd))
				return (p);
	}

	return NULL;
}

/** Calls the specified command for the user, as if it was received
 * that way on IRC.
 * @param client		Client that is the source.
 * @param mtags		Message tags for this command.
 * @param cmd		Command to run, eg "JOIN".
 * @param parc		Parameter count plus 1.
 * @param parv		Parameter array.
 * @note Make sure you terminate the last parv[] parameter with NULL,
 *       this can easily be forgotten, but certain functions depend on it,
 *       you risk crashes otherwise.
 * @note Once do_cmd() has returned, be sure to check IsDead(client) to
 *       see if the client has been killed. This may happen due to various
 *       reasons, including spamfilter kicking in or some other security
 *       measure.
 * @note Do not pass insane parameters. The combined size of all parameters
 *       should not exceed 510 bytes, since that is what all code expects.
 *       Similarly, you should not exceed MAXPARA for parc.
 */
void do_cmd(Client *client, MessageTag *mtags, char *cmd, int parc, char *parv[])
{
	RealCommand *cmptr;

	cmptr = find_Command_simple(cmd);
	if (cmptr)
		(*cmptr->func) (client, mtags, parc, parv);
}
