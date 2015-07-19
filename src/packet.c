/************************************************************************
 *   Unreal Internet Relay Chat Daemon, src/packet.c
 *   Copyright (C) 1990  Jarkko Oikarinen and
 *                       University of Oulu, Computing Center
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
#include "msg.h"
#include "h.h"
#include <string.h>
ID_Copyright
    ("(C) 1988 University of Oulu, Computing Center and Jarkko Oikarinen");
ID_Notes("2.12 1/30/94");

aCommand	*CommandHash[256]; /* one per letter */

/*
** dopacket
**	cptr - pointer to client structure for which the buffer data
**	       applies.
**	buffer - pointr to the buffer containing the newly read data
**	length - number of valid bytes of data in the buffer
**
** Note:
**	It is implicitly assumed that dopacket is called only
**	with cptr of "local" variation, which contains all the
**	necessary fields (buffer etc..)
**
** Rewritten for linebufs, 19th May 2013. --kaniini
*/
int  dopacket(aClient *cptr, char *buffer, int length)
{
	me.local->receiveB += length;	/* Update bytes received */
	cptr->local->receiveB += length;
	if (cptr->local->receiveB > 1023)
	{
		cptr->local->receiveK += (cptr->local->receiveB >> 10);
		cptr->local->receiveB &= 0x03ff;	/* 2^10 = 1024, 3ff = 1023 */
	}
	if (me.local->receiveB > 1023)
	{
		me.local->receiveK += (me.local->receiveB >> 10);
		me.local->receiveB &= 0x03ff;
	}

	me.local->receiveM += 1;	/* Update messages received */
	cptr->local->receiveM += 1;

	return parse(cptr, buffer, buffer + length);
}

void	init_CommandHash(void)
{
#ifdef DEVELOP_DEBUG
	aCommand	 *p;
	int		 i;
	long		chainlength;
#endif
	
	bzero(CommandHash, sizeof(CommandHash));
	CommandAdd(NULL, MSG_ERROR, m_error, MAXPARA, M_UNREGISTERED|M_SERVER);
	CommandAdd(NULL, MSG_VERSION, m_version, MAXPARA, M_UNREGISTERED|M_USER|M_SERVER);
	CommandAdd(NULL, MSG_SUMMON, m_summon, 1, M_USER);
	CommandAdd(NULL, MSG_USERS, m_users, MAXPARA, M_USER);
	CommandAdd(NULL, MSG_INFO, m_info, MAXPARA, M_USER);
	CommandAdd(NULL, MSG_DNS, m_dns, MAXPARA, M_USER);
	CommandAdd(NULL, MSG_REHASH, m_rehash, MAXPARA, M_USER|M_SERVER);
	CommandAdd(NULL, MSG_RESTART, m_restart, 2, M_USER);
	CommandAdd(NULL, MSG_DIE, m_die, MAXPARA, M_USER);
	CommandAdd(NULL, MSG_DALINFO, m_dalinfo, MAXPARA, M_USER);
	CommandAdd(NULL, MSG_CREDITS, m_credits, MAXPARA, M_USER);
	CommandAdd(NULL, MSG_LICENSE, m_license, MAXPARA, M_USER);
	CommandAdd(NULL, MSG_MODULE, m_module, MAXPARA, M_USER);
		
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

aCommand *add_Command_backend(char *cmd, int (*func)(), unsigned char parameters, int flags)
{
	aCommand *newcmd = MyMallocEx(sizeof(aCommand));
	
	newcmd->cmd = (char *) strdup(cmd);
	newcmd->parameters = (parameters > MAXPARA) ? MAXPARA : parameters;
	newcmd->func = func;
	newcmd->flags = flags;
	
	/* Add in hash with hash value = first byte */
	AddListItem(newcmd, CommandHash[toupper(*cmd)]);

	return newcmd;
}

inline aCommand *find_CommandEx(char *cmd, int (*func)(), int token)
{
	aCommand *p;
	
	for (p = CommandHash[toupper(*cmd)]; p; p = p->next)
		if (!stricmp(p->cmd, cmd) && p->func == func)
			return p;

	return NULL;
	
}

int del_Command(char *cmd, int (*func)())
{
	aCommand *p;
	int	i = 0;
	p = find_CommandEx(cmd, func, 0);
	if (!p)
		i--;
	else
	{
		Cmdoverride *ovr, *ovrnext;
		DelListItem(p, CommandHash[toupper(*cmd)]);
		for (ovr = p->overriders; ovr; ovr = ovrnext)
		{
			ovrnext = ovr->next;
			CmdoverrideDel(ovr);
		}

		if (p->cmd)
			MyFree(p->cmd);
		MyFree(p);
	}
	return i;	

}

static inline aCommand *find_Cmd(char *cmd, int flags)
{
	aCommand *p;
	for (p = CommandHash[toupper(*cmd)]; p; p = p->next) {
		if ((flags & M_UNREGISTERED) && !(p->flags & M_UNREGISTERED))
			continue;
		if ((flags & M_SHUN) && !(p->flags & M_SHUN))
			continue;
		if ((flags & M_VIRUS) && !(p->flags & M_VIRUS))
			continue;
		if ((flags & M_ALIAS) && !(p->flags & M_ALIAS))
			continue;
		if (!stricmp(p->cmd, cmd))
			return p;
	}
	return NULL;
}

aCommand *find_Command(char *cmd, short token, int flags)
{
	aCommand *p;
	
	Debug((DEBUG_NOTICE, "FindCommand %s", cmd));

	return find_Cmd(cmd, flags);
}

aCommand *find_Command_simple(char *cmd)
{
	aCommand	*p;
	
	for (p = CommandHash[toupper(*cmd)]; p; p = p->next) {
		if (!stricmp(p->cmd, cmd))
				return (p);
	}

	return NULL;
}

/** Calls the specified command.
 * PURPOSE:
 *  This function is especially meant for calling modulized commands,
 *  both from the core and from (eg:) module A to a command in module B.
 *  An alternative to this is MOD_Dep, but this requires a lot more
 *  effort, is more error phrone and is not a general solution
 *  (but it is slightly faster).
 * PARAMETERS:
 *  Parameters are clear.. the usual cptr, sptr, parc, parv stuff.
 *  'cmd' is the command string, eg: "JOIN"
 * RETURN VALUE:
 *  The value returned by the command function, or -99 if command not found.
 * IMPORTANT NOTES:
 *  - make sure you terminate the last parv[] parameter with NULL,
 *    this can easily be forgotten, but certain functions depend on it,
 *    you risk crashes otherwise.
 *  - be sure to check for FLUSH_BUFFER (-5) return value, especially
 *    if you are calling functions that might cause an immediate kill
 *    (eg: due to spamfilter).
 *  - obvious, but... do not stuff in insane parameters, like a parameter
 *    of 1024 bytes, most of the ircd code depends on the max size of the
 *    total command being less than 512 bytes. Same for parc < MAXPARA.
 */
int do_cmd(aClient *cptr, aClient *sptr, char *cmd, int parc, char *parv[])
{
aCommand *cmptr;

	cmptr = find_Command_simple(cmd);
	if (!cmptr)
		return -99;
	return (*cmptr->func) (cptr, sptr, parc, parv);
}
