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
aCommand	*TokenHash[256]; 

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
*/
void    add_CommandX(char *cmd, char *token, int (*func)(), unsigned char parameters, int flags) ;

int  dopacket(aClient *cptr, char *buffer, int length)
{
	char *ch1;
	char *ch2;
	aClient *acpt = cptr->listener;
#ifdef ZIP_LINKS
	int zipped = 0;
	int done_unzip = 0;
#endif

	me.receiveB += length;	/* Update bytes received */
	cptr->receiveB += length;
	if (cptr->receiveB > 1023)
	{
		cptr->receiveK += (cptr->receiveB >> 10);
		cptr->receiveB &= 0x03ff;	/* 2^10 = 1024, 3ff = 1023 */
	}
	if (acpt != &me)
	{
		acpt->receiveB += length;
		if (acpt->receiveB > 1023)
		{
			acpt->receiveK += (acpt->receiveB >> 10);
			acpt->receiveB &= 0x03ff;
		}
	}
	if (me.receiveB > 1023)
	{
		me.receiveK += (me.receiveB >> 10);
		me.receiveB &= 0x03ff;
	}
	ch1 = cptr->buffer + cptr->count;
	ch2 = buffer;
#ifdef ZIP_LINKS
	if (IsZipStart(cptr))
	{
		 if (*ch2 == '\n' || *ch2 == '\r')
		 {
		 	ch2++;
		 	length--;
		 }
		 cptr->zip->first = 0;
	} else {
		done_unzip = 1;
	}

	if (IsZipped(cptr))
	{
		/* uncompressed buffer first */
		zipped = length;
		cptr->zip->inbuf[0] = '\0';    /* unnecessary but nicer for debugging */
		cptr->zip->incount = 0;
		ch2 = unzip_packet(cptr, ch2, &zipped);
		length = zipped;
		zipped = 1;
		if (length == -1)
			return exit_client(cptr, cptr, &me,
				"fatal error in unzip_packet(1)");
	}

	/* While there is "stuff" in the compressed input to deal with,
	 * keep loop parsing it. I have to go through this loop at least once.
	 * -Dianora
	 */
	do
	{
#endif
		while (--length >= 0)
		{
			char g = (*ch1 = *ch2++);
			/*
			 * Yuck.  Stuck.  To make sure we stay backward compatible,
			 * we must assume that either CR or LF terminates the message
			 * and not CR-LF.  By allowing CR or LF (alone) into the body
			 * of messages, backward compatibility is lost and major
			 * problems will arise. - Avalon
			 */
			if (g < '\16' && (g == '\n' || g == '\r'))
			{
				if (ch1 == cptr->buffer)
					continue;	/* Skip extra LF/CR's */
				*ch1 = '\0';
				me.receiveM += 1;	/* Update messages received */
				cptr->receiveM += 1;
				if (cptr->listener != &me)
					cptr->listener->receiveM += 1;
				cptr->count = 0;	/* ...just in case parse returns with
							   ** FLUSH_BUFFER without removing the
							   ** structure pointed by cptr... --msa
							 */
				if (parse(cptr, cptr->buffer, ch1) ==
				    FLUSH_BUFFER)
					/*
					   ** FLUSH_BUFFER means actually that cptr
					   ** structure *does* not exist anymore!!! --msa
					 */
					return FLUSH_BUFFER;
				/*
				 ** Socket is dead so exit (which always returns with
				 ** FLUSH_BUFFER here).  - avalon
				 */
				if (cptr->flags & FLAGS_DEADSOCKET)
					return exit_client(cptr, cptr, &me,
					    cptr->error_str ? cptr->error_str : "Dead socket");
#ifdef ZIP_LINKS
				if ((IsZipped(cptr)) && (zipped == 0) && (length > 0))
				{
					/*
					** beginning of server connection, the buffer
					** contained PASS/CAPAB/SERVER and is now
					** zipped!
					** Ignore the '\n' that should be here.
					*/
					/* Checked RFC1950: \r or \n can't start a
					** zlib stream  -orabidoo
					*/
					zipped = length;
					if (zipped > 0 && (*ch2 == '\n' || *ch2 == '\r'))
					{
						ch2++;
						zipped--;
					}
					cptr->zip->first = 0;
					ch2 = unzip_packet(cptr, ch2, &zipped);
					length = zipped;
					zipped = 1;
					if (length == -1)
						return exit_client(cptr, cptr, &me,
							"fatal error in unzip_packet(2)");
				}
#endif
				ch1 = cptr->buffer;
			}
			else if (ch1 <
			    cptr->buffer + (sizeof(cptr->buffer) - 1))
				ch1++;	/* There is always room for the null */
		}
#ifdef ZIP_LINKS
		 /* Now see if anything is left uncompressed in the input
		  * If so, uncompress it and continue to parse
		  * -Dianora
		  */
		if ((IsZipped(cptr)) && cptr->zip->incount)
		{
			/* This call simply finishes unzipping whats left
			 * second parameter is not used. -Dianora
			 */
			ch2 = unzip_packet(cptr, (char *)NULL, &zipped);
			length = zipped;
			zipped = 1;
			if (length == -1)
				return exit_client(cptr, cptr, &me,
					"fatal error in unzip_packet(3)");
			ch1 = ch2 + length;
			done_unzip = 0;
		} else {
			done_unzip = 1;
		}

	} while(!done_unzip);
#endif
	cptr->count = ch1 - cptr->buffer;
	return 0;
}

void	init_CommandHash(void)
{
#ifdef DEVELOP_DEBUG
	aCommand	 *p;
	int		 i;
	long		chainlength;
#endif
	
	bzero(CommandHash, sizeof(CommandHash));
	bzero(TokenHash, sizeof(TokenHash));
	add_CommandX(MSG_ERROR, TOK_ERROR, m_error, MAXPARA, M_UNREGISTERED|M_SERVER);
	add_CommandX(MSG_VERSION, TOK_VERSION, m_version, MAXPARA, M_UNREGISTERED|M_USER|M_SERVER);
	add_Command(MSG_SUMMON, NULL, m_summon, 1);
	add_Command(MSG_USERS, NULL, m_users, MAXPARA);
	add_Command(MSG_INFO, TOK_INFO, m_info, MAXPARA);
	add_Command(MSG_DNS, TOK_DNS, m_dns, MAXPARA);
	add_Command(MSG_REHASH, TOK_REHASH, m_rehash, MAXPARA);
	add_Command(MSG_RESTART, TOK_RESTART, m_restart, 2);
	add_Command(MSG_DIE, TOK_DIE, m_die, MAXPARA);
	add_Command(MSG_DALINFO, TOK_DALINFO, m_dalinfo, MAXPARA);
	add_Command(MSG_CREDITS, TOK_CREDITS, m_credits, MAXPARA);
	add_Command(MSG_LICENSE, TOK_LICENSE, m_license, MAXPARA);
	add_Command(MSG_MODULE, TOK_MODULE, m_module, MAXPARA);	
		
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
	fprintf(stderr, "Tokens:\n");
	for (i = 0; i <= 255; i++)
	{
		chainlength = 0;
		for (p = TokenHash[i]; p; p = p->next)
			chainlength++;
		if (chainlength)
			fprintf(stderr, "%c chainlength = %i\r\n",
					i, chainlength);
	}				
#endif
}

aCommand *add_Command_backend(char *cmd, int (*func)(), unsigned char parameters, unsigned char token, int flags)
{
	aCommand	*newcmd = (aCommand *) MyMalloc(sizeof(aCommand));
	
	bzero(newcmd, sizeof(aCommand));
	
	newcmd->cmd = (char *) strdup(cmd);
	newcmd->parameters = (parameters > MAXPARA) ? MAXPARA : parameters;
	newcmd->func = func;
	newcmd->flags = flags;
	
	/* Add in hash with hash value = first byte */
	if (!token)
		AddListItem(newcmd, CommandHash[toupper(*cmd)]);
	else
		AddListItem(newcmd, TokenHash[*cmd]);
	return newcmd;
}

void	add_Command(char *name, char *token, int (*func)(), unsigned char parameters)
{
	aCommand *cmd, *tok;
	cmd = add_Command_backend(name, func, parameters, 0, 0);
	if (token)
	{
		tok = add_Command_backend(token, func, parameters, 1, 0);
		tok->friend = cmd;
		cmd->friend = tok;
	}
	else
		cmd->friend = NULL;
}

void    add_CommandX(char *name, char *token, int (*func)(), unsigned char parameters, int flags) 
{
	aCommand *cmd, *tok;
	cmd = add_Command_backend(name, func, parameters, 0, flags);
	if (token != NULL)
	{
		tok = add_Command_backend(token, func, parameters, 1, flags);
		tok->friend = cmd;
		cmd->friend = tok;
	}
	else
		cmd->friend = NULL;
}

inline aCommand *find_CommandEx(char *cmd, int (*func)(), int token)
{
	aCommand *p;
	
	if (!token)
	{
		for (p = CommandHash[toupper(*cmd)]; p; p = p->next)
			if (!stricmp(p->cmd, cmd) && p->func == func)
				return p;
		return NULL;
	}
	for (p = TokenHash[*cmd]; p; p = p->next)
		if (!strcmp(p->cmd, cmd) && p->func == func)
			return p;
	return NULL;
	
}

int del_Command(char *cmd, char *token, int (*func)())
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
	if (token != NULL) {
		p = find_CommandEx(token, func, 1);
		if (!p)
			i--;
		else
		{
			DelListItem(p, TokenHash[*token]);
			if (p->cmd)
				MyFree(p->cmd);
			MyFree(p);
		}
	}
	return i;	

}

static inline aCommand *find_Token(char *cmd, int flags)
{
	aCommand *p;

	for (p = TokenHash[*cmd]; p; p = p->next) {
		if ((flags & M_UNREGISTERED) && !(p->flags & M_UNREGISTERED))
			continue;
		if ((flags & M_SHUN) && !(p->flags & M_SHUN))
			continue;
		if ((flags & M_VIRUS) && !(p->flags & M_VIRUS))
			continue;
		if ((flags & M_ALIAS) && !(p->flags & M_ALIAS))
			continue;
		if (!strcmp(p->cmd, cmd))
			return p;
	}
	return NULL;
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

inline aCommand *find_Command(char *cmd, short token, int flags)
{
	aCommand *p;
	
	Debug((DEBUG_NOTICE, "FindCommand %s", cmd));

	if (token)
	{
		if (strlen(cmd) < 3)
		{
			if ((p = find_Token(cmd, flags)))
				return p;
			return find_Cmd(cmd, flags);
		}
		if ((p = find_Cmd(cmd, flags)))
			return p;
		return find_Token(cmd, flags);
	}
	return find_Cmd(cmd, flags);
}

aCommand *find_Command_simple(char *cmd)
{
	aCommand	*p;
	
	for (p = CommandHash[toupper(*cmd)]; p; p = p->next) {
		if (!stricmp(p->cmd, cmd))
				return (p);
	}

	for (p = TokenHash[*cmd]; p; p = p->next) {
		if (!strcmp(p->cmd, cmd))
				return p;
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
