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

aCommand	*CommandHash[256]; 

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
	else if (me.receiveB > 1023)
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
	add_Command(MSG_MODE, TOK_MODE, m_mode, MAXPARA);
	add_Command(MSG_OPERMOTD, TOK_OPERMOTD, m_opermotd, MAXPARA);
	add_CommandX(MSG_NICK, TOK_NICK, m_nick, MAXPARA, M_UNREGISTERED|M_USER|M_SERVER);
	add_CommandX(MSG_JOIN, TOK_JOIN, m_join, MAXPARA, M_USER);
	add_Command(MSG_ISON, TOK_ISON, m_ison, 1);
	add_CommandX(MSG_USER, TOK_USER, m_user, 4, M_UNREGISTERED|M_USER);
	add_CommandX(MSG_PART, TOK_PART, m_part, 2, M_USER);
	add_Command(MSG_WATCH, TOK_WATCH, m_watch, 1);
	add_Command(MSG_USERHOST, TOK_USERHOST, m_userhost, 1);
	add_Command(MSG_LUSERS, TOK_LUSERS, m_lusers, MAXPARA);
	add_Command(MSG_TOPIC, TOK_TOPIC, m_topic, 4);
	add_Command(MSG_INVITE, TOK_INVITE, m_invite, MAXPARA);
	add_Command(MSG_KICK, TOK_KICK, m_kick, 3);
	add_Command(MSG_WALLOPS, TOK_WALLOPS, m_wallops, 1);
	add_CommandX(MSG_ERROR, TOK_ERROR, m_error, MAXPARA, M_UNREGISTERED|M_SERVER);
	add_CommandX(MSG_PROTOCTL, TOK_PROTOCTL, m_protoctl, MAXPARA, M_UNREGISTERED|M_SERVER|M_USER);
	add_CommandX(MSG_SERVER, TOK_SERVER, m_server, MAXPARA, M_UNREGISTERED|M_SERVER);
	add_Command(MSG_SQUIT, TOK_SQUIT, m_squit, 2);
	add_Command(MSG_WHOWAS, TOK_WHOWAS, m_whowas, MAXPARA);
	add_Command(MSG_LIST, TOK_LIST, m_list, MAXPARA);
	add_Command(MSG_NAMES, TOK_NAMES, m_names, MAXPARA);
	add_Command(MSG_TRACE, TOK_TRACE, m_trace, MAXPARA);
	add_CommandX(MSG_PASS, TOK_PASS, m_pass, 1, M_UNREGISTERED|M_USER|M_SERVER);
	add_Command(MSG_TIME, TOK_TIME, m_time, MAXPARA);
	add_Command(MSG_CONNECT, TOK_CONNECT, m_connect, MAXPARA);
	add_CommandX(MSG_VERSION, TOK_VERSION, m_version, MAXPARA, M_UNREGISTERED|M_USER|M_SERVER);
	add_Command(MSG_STATS, TOK_STATS, m_stats, MAXPARA);
	add_Command(MSG_LINKS, TOK_LINKS, m_links, MAXPARA);
	add_CommandX(MSG_ADMIN, TOK_ADMIN, m_admin, MAXPARA, M_UNREGISTERED|M_USER|M_SHUN);
	add_Command(MSG_SUMMON, NULL, m_summon, 1);
	add_Command(MSG_USERS, NULL, m_users, MAXPARA);
	add_Command(MSG_SAMODE, NULL, m_samode, MAXPARA);
	add_Command(MSG_SVSKILL, TOK_SVSKILL, m_svskill, MAXPARA);
	add_Command(MSG_HELP, TOK_HELP, m_help, 1);
	add_Command(MSG_HELPOP, TOK_HELP, m_help, 1);
	add_Command(MSG_INFO, TOK_INFO, m_info, MAXPARA);
	add_Command(MSG_MOTD, TOK_MOTD, m_motd, MAXPARA);
	add_Command(MSG_CLOSE, TOK_CLOSE, m_close, MAXPARA);
	add_Command(MSG_SILENCE, TOK_SILENCE, m_silence, MAXPARA);
	add_Command(MSG_GNOTICE, TOK_GNOTICE, m_gnotice, MAXPARA);
	add_Command(MSG_GOPER, TOK_GOPER, m_goper, MAXPARA);
	add_Command(MSG_GLOBOPS, TOK_GLOBOPS, m_globops, 1);
	add_Command(MSG_CHATOPS, TOK_CHATOPS, m_chatops, 1);
	add_Command(MSG_LOCOPS, TOK_LOCOPS, m_locops, 1);
	add_Command(MSG_DNS, TOK_DNS, m_dns, MAXPARA);
	add_Command(MSG_REHASH, TOK_REHASH, m_rehash, MAXPARA);
	add_Command(MSG_RESTART, TOK_RESTART, m_restart, MAXPARA);
	add_Command(MSG_DIE, TOK_DIE, m_die, MAXPARA);
	add_Command(MSG_RULES, TOK_RULES, m_rules, MAXPARA);
	add_Command(MSG_MAP, TOK_MAP, m_map, MAXPARA);
	add_Command(MSG_DALINFO, TOK_DALINFO, m_dalinfo, MAXPARA);
	add_Command(MSG_ADDLINE, TOK_ADDLINE, m_addline, 1);
	add_Command(MSG_KNOCK, TOK_KNOCK, m_knock, 2);
	add_Command(MSG_CREDITS, TOK_CREDITS, m_credits, MAXPARA);
	add_Command(MSG_LICENSE, TOK_LICENSE, m_license, MAXPARA);
	add_Command(MSG_NETINFO, TOK_NETINFO, m_netinfo, MAXPARA);
	add_Command(MSG_ADDMOTD, TOK_ADDMOTD, m_addmotd, 1);
	add_Command(MSG_ADDOMOTD, TOK_ADDOMOTD, m_addomotd, 1);
	add_Command(MSG_SAJOIN, TOK_SAJOIN, m_sajoin, MAXPARA);
	add_Command(MSG_SAPART, TOK_SAPART, m_sapart, MAXPARA);
	add_Command(MSG_SVSFLINE, TOK_SVSFLINE, m_svsfline, MAXPARA);
	add_Command(MSG_BOTMOTD, TOK_BOTMOTD, m_botmotd, MAXPARA);
	add_Command(MSG_SJOIN, TOK_SJOIN, m_sjoin, MAXPARA);
	add_Command(MSG_UMODE2, TOK_UMODE2, m_umode2, MAXPARA);
	add_Command(MSG_DCCDENY, TOK_DCCDENY, m_dccdeny, 2);
	add_Command(MSG_UNDCCDENY, TOK_UNDCCDENY, m_undccdeny, MAXPARA);
	add_Command(MSG_NEWJOIN, TOK_JOIN, m_join, MAXPARA);
	add_Command(MSG_MODULE, TOK_MODULE, m_module, MAXPARA);	
	add_Command(MSG_TKL, TOK_TKL, m_tkl, MAXPARA);
		
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

aCommand *add_Command_backend(char *cmd, int (*func)(), unsigned char parameters, unsigned char token, int flags)
{
	aCommand	*newcmd = (aCommand *) MyMalloc(sizeof(aCommand));
	
	bzero(newcmd, sizeof(aCommand));
	
	newcmd->cmd = (char *) strdup(cmd);
	newcmd->parameters = parameters;
	newcmd->token = token;
	newcmd->func = func;
	newcmd->flags = flags;
	
	/* Add in hash with hash value = first byte */
	AddListItem(newcmd, CommandHash[toupper(*cmd)]);
	return newcmd;
}

int CommandExists(char *name)
{
	aCommand *p;
	
	for (p = CommandHash[toupper(*name)]; p; p = p->next)
	{
		if (!stricmp(p->cmd, name))
			return 1;
	}
	return 0;
}

Command *CommandAdd(Module *module, char *cmd, char *tok, int (*func)(), unsigned char params, int flags) {
	Command *command = MyMallocEx(sizeof(Command));
	command->cmd = add_Command_backend(cmd,func,params, 0, flags);
	command->tok = NULL;
	command->cmd->owner = module;
	if (tok) {
		command->tok = add_Command_backend(tok,func,params,1,flags);
		command->tok->owner = module;
	}
	if (module) {
		ModuleObject *cmdobj = (ModuleObject *)MyMallocEx(sizeof(ModuleObject));
		cmdobj->object.command = command;
		cmdobj->type = MOBJ_COMMAND;
		AddListItem(cmdobj, module->objects);
	}
	return command;
}


void CommandDel(Command *command) {
	DelListItem(command->cmd, CommandHash[toupper(*command->cmd->cmd)]);
	if (command->tok)
		DelListItem(command->tok, CommandHash[toupper(*command->tok->cmd)]);
	if (command->cmd->owner) {
		ModuleObject *cmdobj;
		for (cmdobj = command->cmd->owner->objects; cmdobj; cmdobj = (ModuleObject *)cmdobj->next) {
			if (cmdobj->type == MOBJ_COMMAND && cmdobj->object.command == command) {
				DelListItem(cmdobj,command->cmd->owner->objects);
				MyFree(cmdobj);
				break;
			}
		}
	}
	MyFree(command->cmd->cmd);
	MyFree(command->cmd);
	if (command->tok) {
		MyFree(command->tok->cmd);
		MyFree(command->tok);
	}
	MyFree(command);
}

void	add_Command(char *cmd, char *token, int (*func)(), unsigned char parameters)
{
	add_Command_backend(cmd, func, parameters, 0, 0);
	if (token)
		add_Command_backend(token, func, parameters, 1, 0);
}

void    add_CommandX(char *cmd, char *token, int (*func)(), unsigned char parameters, int flags) 
{
	add_Command_backend(cmd, func, parameters, 0, flags);
	if (token != NULL)
		add_Command_backend(token, func, parameters, 1, flags);
}

inline aCommand *find_CommandEx(char *cmd, int (*func)(), int token)
{
	aCommand *p;
	
	for (p = CommandHash[toupper(*cmd)]; p; p = p->next)
		if (p->token && token)
		{
			if (!strcmp(p->cmd, cmd))
				if (p->func == func)
					return (p);
		}
		else
			if (!stricmp(p->cmd, cmd))
				if (p->func == func)
					return (p);
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
		DelListItem(p, CommandHash[toupper(*cmd)]);
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
			DelListItem(p, CommandHash[toupper(*token)]);
			if (p->cmd)
				MyFree(p->cmd);
			MyFree(p);
		}
	}
	return i;	

}

inline aCommand *find_Command(char *cmd, short token, int flags)
{
	aCommand	*p;
	
	Debug((DEBUG_NOTICE, "FindCommand %s", cmd));

	for (p = CommandHash[toupper(*cmd)]; p; p = p->next) {
		if ((flags & M_UNREGISTERED) && !(p->flags & M_UNREGISTERED))
			continue;
		if ((flags & M_SHUN) && !(p->flags & M_SHUN))
			continue;
		if ((flags & M_ALIAS) && !(p->flags & M_ALIAS))
			continue;
		if (p->token && token)
		{
			if (!strcmp(p->cmd, cmd))
				return (p);
		}
		else if (!p->token)
			if (!stricmp(p->cmd, cmd))
				return (p);
	}
	return NULL;
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
