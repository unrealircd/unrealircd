
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
int  dopacket(cptr, buffer, length)
	aClient *cptr;
	char *buffer;
	int  length;
{
	char *ch1;
	char *ch2;
	aClient *acpt = cptr->listener;

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
					    "Dead Socket");
				ch1 = cptr->buffer;
			}
			else if (ch1 <
			    cptr->buffer + (sizeof(cptr->buffer) - 1))
				ch1++;	/* There is always room for the null */
		}
	cptr->count = ch1 - cptr->buffer;
	return 0;
}

void	init_CommandHash(void)
{
#ifdef DEVELOP_DEBUG
	aCommand	 *p;
	int		 i;
#endif
	long		chainlength;
	
	bzero(CommandHash, sizeof(CommandHash));
	add_Command(MSG_PRIVATE, TOK_PRIVATE, m_private, MAXPARA);
	add_Command(MSG_NOTICE, TOK_NOTICE, m_notice, MAXPARA);
	add_Command(MSG_MODE, TOK_MODE, m_mode, MAXPARA);
	add_Command(MSG_NICK, TOK_NICK, m_nick, MAXPARA);
	add_Command(MSG_JOIN, TOK_JOIN, m_join, MAXPARA);
	add_Command(MSG_PING, TOK_PING, m_ping, MAXPARA);
	add_Command(MSG_WHOIS, TOK_WHOIS, m_whois, MAXPARA);
	add_Command(MSG_ISON, TOK_ISON, m_ison, 1);
	add_Command(MSG_USER, TOK_USER, m_user, MAXPARA);
	add_Command(MSG_PONG, TOK_PONG, m_pong, MAXPARA);
	add_Command(MSG_PART, TOK_PART, m_part, MAXPARA);
	add_Command(MSG_QUIT, TOK_QUIT, m_quit, MAXPARA);
	add_Command(MSG_WATCH, TOK_WATCH, m_watch, 1);
	add_Command(MSG_USERHOST, TOK_USERHOST, m_userhost, 1);
	add_Command(MSG_SVSNICK, TOK_SVSNICK, m_svsnick, MAXPARA);
	add_Command(MSG_SVSMODE, TOK_SVSMODE, m_svsmode, MAXPARA);
	add_Command(MSG_LUSERS, TOK_LUSERS, m_lusers, MAXPARA);
	add_Command(MSG_IDENTIFY, TOK_IDENTIFY, m_identify, 1);
	add_Command(MSG_CHANSERV, TOK_CHANSERV, m_chanserv, 1);
	add_Command(MSG_TOPIC, TOK_TOPIC, m_topic, MAXPARA);
	add_Command(MSG_INVITE, TOK_INVITE, m_invite, MAXPARA);
	add_Command(MSG_KICK, TOK_KICK, m_kick, MAXPARA);
	add_Command(MSG_WALLOPS, TOK_WALLOPS, m_wallops, 1);
	add_Command(MSG_ERROR, TOK_ERROR, m_error, MAXPARA);
	add_Command(MSG_KILL, TOK_KILL, m_kill, MAXPARA);
	add_Command(MSG_PROTOCTL, TOK_PROTOCTL, m_protoctl, MAXPARA);
	add_Command(MSG_AWAY, TOK_AWAY, m_away, MAXPARA);
	add_Command(MSG_SERVER, TOK_SERVER, m_server, MAXPARA);
	add_Command(MSG_SQUIT, TOK_SQUIT, m_squit, MAXPARA);
	add_Command(MSG_WHO, TOK_WHO, m_who, MAXPARA);
	add_Command(MSG_WHOWAS, TOK_WHOWAS, m_whowas, MAXPARA);
	add_Command(MSG_LIST, TOK_LIST, m_list, MAXPARA);
	add_Command(MSG_NAMES, TOK_NAMES, m_names, MAXPARA);
	add_Command(MSG_TRACE, TOK_TRACE, m_trace, MAXPARA);
	add_Command(MSG_PASS, TOK_PASS, m_pass, MAXPARA);
	add_Command(MSG_TIME, TOK_TIME, m_time, MAXPARA);
	add_Command(MSG_OPER, TOK_OPER, m_oper, MAXPARA);
	add_Command(MSG_CONNECT, TOK_CONNECT, m_connect, MAXPARA);
	add_Command(MSG_VERSION, TOK_VERSION, m_version, MAXPARA);
	add_Command(MSG_STATS, TOK_STATS, m_stats, MAXPARA);
	add_Command(MSG_LINKS, TOK_LINKS, m_links, MAXPARA);
	add_Command(MSG_ADMIN, TOK_ADMIN, m_admin, MAXPARA);
	add_Command(MSG_SUMMON, TOK_SUMMON, m_summon, 1);
	add_Command(MSG_USERS, TOK_USERS, m_users, MAXPARA);
	add_Command(MSG_SAMODE, TOK_SAMODE, m_samode, MAXPARA);
	add_Command(MSG_SVSKILL, TOK_SVSKILL, m_svskill, MAXPARA);
	add_Command(MSG_SVSNOOP, TOK_SVSNOOP, m_svsnoop, MAXPARA);
	add_Command(MSG_CS, TOK_CHANSERV, m_chanserv, 1);
	add_Command(MSG_NICKSERV, TOK_NICKSERV, m_nickserv, 1);
	add_Command(MSG_NS, TOK_NICKSERV, m_nickserv, 1);
	add_Command(MSG_INFOSERV, TOK_INFOSERV, m_infoserv, 1);
	add_Command(MSG_IS, TOK_INFOSERV, m_infoserv, 1);
	add_Command(MSG_OPERSERV, TOK_OPERSERV, m_operserv, 1);
	add_Command(MSG_OS, TOK_OPERSERV, m_operserv, 1);
	add_Command(MSG_MEMOSERV, TOK_MEMOSERV, m_memoserv, 1);
	add_Command(MSG_MS, TOK_MEMOSERV, m_memoserv, 1);
	add_Command(MSG_HELPSERV, TOK_HELPSERV, m_helpserv, 1);
	add_Command(MSG_HS, TOK_HELPSERV, m_helpserv, 1);
	add_Command(MSG_SERVICES, TOK_SERVICES, m_services, 1);
	add_Command(MSG_HELP, TOK_HELP, m_help, 1);
	add_Command(MSG_HELPOP, TOK_HELP, m_help, 1);
	add_Command(MSG_INFO, TOK_INFO, m_info, MAXPARA);
	add_Command(MSG_MOTD, TOK_MOTD, m_motd, MAXPARA);
	add_Command(MSG_CLOSE, TOK_CLOSE, m_close, MAXPARA);
	add_Command(MSG_SILENCE, TOK_SILENCE, m_silence, MAXPARA);
	add_Command(MSG_AKILL, TOK_AKILL, m_akill, MAXPARA);
	add_Command(MSG_SQLINE, TOK_SQLINE, m_sqline, MAXPARA);
	add_Command(MSG_UNSQLINE, TOK_UNSQLINE, m_unsqline, MAXPARA);
	add_Command(MSG_KLINE, TOK_KLINE, m_kline, MAXPARA);
	add_Command(MSG_UNKLINE, TOK_UNKLINE, m_unkline, MAXPARA);
	add_Command(MSG_ZLINE, TOK_ZLINE, m_zline, MAXPARA);
	add_Command(MSG_UNZLINE, TOK_UNZLINE, m_unzline, MAXPARA);
	add_Command(MSG_RAKILL, TOK_RAKILL, m_rakill, MAXPARA);
	add_Command(MSG_GNOTICE, TOK_GNOTICE, m_gnotice, MAXPARA);
	add_Command(MSG_GOPER, TOK_GOPER, m_goper, MAXPARA);
	add_Command(MSG_GLOBOPS, TOK_GLOBOPS, m_globops, MAXPARA);
	add_Command(MSG_CHATOPS, TOK_CHATOPS, m_chatops, 1);
	add_Command(MSG_LOCOPS, TOK_LOCOPS, m_locops, 1);
	add_Command(MSG_HASH, TOK_HASH, m_hash, MAXPARA);
	add_Command(MSG_DNS, TOK_DNS, m_dns, MAXPARA);
	add_Command(MSG_REHASH, TOK_REHASH, m_rehash, MAXPARA);
	add_Command(MSG_RESTART, TOK_RESTART, m_restart, MAXPARA);
	add_Command(MSG_DIE, TOK_DIE, m_die, MAXPARA);
	add_Command(MSG_RULES, TOK_RULES, m_rules, MAXPARA);
	add_Command(MSG_MAP, TOK_MAP, m_map, MAXPARA);
	add_Command(MSG_GLINE, TOK_GLINE, m_gline, MAXPARA);
	add_Command(MSG_REMGLINE, TOK_REMGLINE, m_remgline, MAXPARA);
	add_Command(MSG_DALINFO, TOK_DALINFO, m_dalinfo, MAXPARA);
	add_Command(MSG_SVS2MODE, TOK_SVS2MODE, m_svs2mode, MAXPARA);
	add_Command(MSG_MKPASSWD, TOK_MKPASSWD, m_mkpasswd, MAXPARA);
	add_Command(MSG_ADDLINE, TOK_ADDLINE, m_addline, 1);
	add_Command(MSG_ADMINCHAT, TOK_ADMINCHAT, m_admins, 1);
	add_Command(MSG_TECHAT, TOK_TECHAT, m_techat, 1);
	add_Command(MSG_NACHAT, TOK_NACHAT, m_nachat, 1);
	add_Command(MSG_SETIDENT, TOK_SETIDENT, m_setident, MAXPARA);
	add_Command(MSG_LAG, TOK_LAG, m_lag, MAXPARA);
	add_Command(MSG_SDESC, TOK_SDESC, m_sdesc, 1);
	add_Command(MSG_STATSERV, TOK_STATSERV, m_statserv, 1);
	add_Command(MSG_KNOCK, TOK_KNOCK, m_knock, 2);
	add_Command(MSG_CREDITS, TOK_CREDITS, m_credits, MAXPARA);
	add_Command(MSG_LICENSE, TOK_LICENSE, m_license, MAXPARA);
	add_Command(MSG_RPING, TOK_RPING, m_rping, MAXPARA);
	add_Command(MSG_RPONG, TOK_RPONG, m_rpong, MAXPARA);
	add_Command(MSG_NETINFO, TOK_NETINFO, m_netinfo, MAXPARA);
	add_Command(MSG_SENDUMODE, TOK_SENDUMODE, m_sendumode, MAXPARA);
	add_Command(MSG_SMO, TOK_SMO, m_sendumode, MAXPARA);
	add_Command(MSG_ADDMOTD, TOK_ADDMOTD, m_addmotd, 1);
	add_Command(MSG_ADDOMOTD, TOK_ADDOMOTD, m_addomotd, 1);
	add_Command(MSG_SVSMOTD, TOK_SVSMOTD, m_svsmotd, MAXPARA);
	add_Command(MSG_OPERMOTD, TOK_OPERMOTD, m_opermotd, MAXPARA);
	add_Command(MSG_TSCTL, TOK_TSCTL, m_tsctl, MAXPARA);
	add_Command(MSG_SVSJOIN, TOK_SVSJOIN, m_svsjoin, MAXPARA);
	add_Command(MSG_SAJOIN, TOK_SAJOIN, m_sajoin, MAXPARA);
	add_Command(MSG_SVSPART, TOK_SVSPART, m_svspart, MAXPARA);
	add_Command(MSG_SAPART, TOK_SAPART, m_sapart, MAXPARA);
	add_Command(MSG_CHGIDENT, TOK_CHGIDENT, m_chgident, MAXPARA);
	add_Command(MSG_SWHOIS, TOK_SWHOIS, m_swhois, MAXPARA);
	add_Command(MSG_SVSO, TOK_SVSO, m_svso, MAXPARA);
	add_Command(MSG_SVSFLINE, TOK_SVSFLINE, m_svsfline, MAXPARA);
	add_Command(MSG_TKL, TOK_TKL, m_tkl, MAXPARA);
	add_Command(MSG_VHOST, TOK_VHOST, m_vhost, MAXPARA);
	add_Command(MSG_BOTMOTD, TOK_BOTMOTD, m_botmotd, MAXPARA);
	add_Command(MSG_SJOIN, TOK_SJOIN, m_sjoin, MAXPARA);
	add_Command(MSG_HTM, TOK_HTM, m_htm, MAXPARA);
	add_Command(MSG_UMODE2, TOK_UMODE2, m_umode2, MAXPARA);
	add_Command(MSG_DCCDENY, TOK_DCCDENY, m_dccdeny, 2);
	add_Command(MSG_UNDCCDENY, TOK_UNDCCDENY, m_undccdeny, MAXPARA);
	add_Command(MSG_CHGNAME, TOK_CHGNAME, m_chgname, MAXPARA);
	add_Command(MSG_SVSNAME, TOK_CHGNAME, m_chgname, MAXPARA);
	add_Command(MSG_SHUN, TOK_SHUN, m_shun, MAXPARA);
	add_Command(MSG_NEWJOIN, TOK_JOIN, m_join, MAXPARA);
	add_Command(MSG_BOTSERV, TOK_BOTSERV, m_botserv, 1);
	add_Command(TOK_BOTSERV, TOK_BOTSERV, m_botserv, 1);
	add_Command(MSG_CYCLE, TOK_CYCLE, m_cycle, MAXPARA);	
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
#endif
}

void	add_Command_backend(char *cmd, int (*func)(), unsigned char parameters, unsigned char token)
{
	aCommand	*newcmd = (aCommand *) MyMalloc(sizeof(aCommand));
	
	bzero(newcmd, sizeof(aCommand));
	
	newcmd->cmd = (char *) strdup(cmd);
	newcmd->parameters = parameters;
	newcmd->token = token;
	newcmd->func = func;
	
	/* Add in hash with hash value = first byte */
	add_Command_to_list(newcmd, &CommandHash[toupper(*cmd)]);
}

void	add_Command(char *cmd, char *token, int (*func)(), unsigned char parameters)
{
	add_Command_backend(cmd, func, parameters, 0);
	add_Command_backend(token, func, parameters, 1);
}

void	add_Command_to_list(aCommand *item, aCommand **list)
{
	item->next = *list;
	item->prev = NULL;
	if (*list)
		(*list)->prev = item;
	*list = item;
}

aCommand *del_Command_from_list(aCommand *item, aCommand **list)
{
	aCommand *p, *q;
	
	for (p = *list; p; p = p->next)
	{
		if (p == item)
		{
			q = p->next;
			if (p->prev)
				p->prev->next = p->next;
			else
				*list = p->next;
				
			if (p->next)
				p->next->prev = p->prev;
			return q;		
		}
	}
	return NULL;
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
			if (!match(p->cmd, cmd))
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
		del_Command_from_list(p, &CommandHash[toupper(*cmd)]);
		if (p->cmd)
			MyFree(p->cmd);
		MyFree(p);
	}
	p = find_CommandEx(token, func, 1);
	if (!p)
		i--;
	else
	{
		del_Command_from_list(p, &CommandHash[toupper(*token)]);
		if (p->cmd)
			MyFree(p->cmd);
		MyFree(p);
	}
	return i;	
}

inline aCommand *find_Command(char *cmd, int token)
{
	aCommand	*p;
	
	for (p = CommandHash[toupper(*cmd)]; p; p = p->next)
		if (p->token && token)
		{
			if (!strcmp(p->cmd, cmd))
				return (p);
		}
		else
			if (!match(p->cmd, cmd))
				return (p);
	return NULL;
}
