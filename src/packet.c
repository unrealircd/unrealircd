
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

inline void 	add_CommandX(char *cmd, int (*func)(), int count, int parameters,
				char *tok, long a)
{
	add_Command(cmd, tok, func, parameters);
}

void	init_CommandHash(void)
{
#ifdef DEVELOP_DEBUG
	aCommand	 *p;
	int		 i;
#endif
	long		chainlength;
	
	bzero(CommandHash, sizeof(CommandHash));
	add_CommandX(MSG_PRIVATE, m_private, 0, MAXPARA, TOK_PRIVATE, 0L);
	add_CommandX(MSG_NOTICE, m_notice, 0, MAXPARA, TOK_NOTICE, 0L);
	add_CommandX(MSG_MODE, m_mode, 0, MAXPARA, TOK_MODE, 0L);
	add_CommandX(MSG_NICK, m_nick, 0, MAXPARA, TOK_NICK, 0L);
	add_CommandX(MSG_JOIN, m_join, 0, MAXPARA, TOK_JOIN, 0L);
	add_CommandX(MSG_PING, m_ping, 0, MAXPARA, TOK_PING, 0L);
	add_CommandX(MSG_WHOIS, m_whois, 0, MAXPARA, TOK_WHOIS, 0L);
	add_CommandX(MSG_ISON, m_ison, 0, 1, TOK_ISON, 0L);
	add_CommandX(MSG_USER, m_user, 0, MAXPARA, TOK_USER, 0L);
	add_CommandX(MSG_PONG, m_pong, 0, MAXPARA, TOK_PONG, 0L);
	add_CommandX(MSG_PART, m_part, 0, MAXPARA, TOK_PART, 0L);
	add_CommandX(MSG_QUIT, m_quit, 0, MAXPARA, TOK_QUIT, 0L);
	add_CommandX(MSG_WATCH, m_watch, 0, 1, TOK_WATCH, 0L);
	add_CommandX(MSG_USERHOST, m_userhost, 0, 1, TOK_USERHOST, 0L);
	add_CommandX(MSG_SVSNICK, m_svsnick, 0, MAXPARA, TOK_SVSNICK, 0L);
	add_CommandX(MSG_SVSMODE, m_svsmode, 0, MAXPARA, TOK_SVSMODE, 0L);
	add_CommandX(MSG_LUSERS, m_lusers, 0, MAXPARA, TOK_LUSERS, 0L);
	add_CommandX(MSG_IDENTIFY, m_identify, 0, 1, TOK_IDENTIFY, 0L);
	add_CommandX(MSG_CHANSERV, m_chanserv, 0, 1, TOK_CHANSERV, 0L);
	add_CommandX(MSG_TOPIC, m_topic, 0, MAXPARA, TOK_TOPIC, 0L);
	add_CommandX(MSG_INVITE, m_invite, 0, MAXPARA, TOK_INVITE, 0L);
	add_CommandX(MSG_KICK, m_kick, 0, MAXPARA, TOK_KICK, 0L);
	add_CommandX(MSG_WALLOPS, m_wallops, 0, 1, TOK_WALLOPS, 0L);
	add_CommandX(MSG_ERROR, m_error, 0, MAXPARA, TOK_ERROR, 0L);
	add_CommandX(MSG_KILL, m_kill, 0, MAXPARA, TOK_KILL, 0L);
	add_CommandX(MSG_PROTOCTL, m_protoctl, 0, MAXPARA, TOK_PROTOCTL, 0L);
	add_CommandX(MSG_AWAY, m_away, 0, MAXPARA, TOK_AWAY, 0L);
	add_CommandX(MSG_SERVER, m_server, 0, MAXPARA, TOK_SERVER, 0L);
	add_CommandX(MSG_SQUIT, m_squit, 0, MAXPARA, TOK_SQUIT, 0L);
	add_CommandX(MSG_WHO, m_who, 0, MAXPARA, TOK_WHO, 0L);
	add_CommandX(MSG_WHOWAS, m_whowas, 0, MAXPARA, TOK_WHOWAS, 0L);
	add_CommandX(MSG_LIST, m_list, 0, MAXPARA, TOK_LIST, 0L);
	add_CommandX(MSG_NAMES, m_names, 0, MAXPARA, TOK_NAMES, 0L);
	add_CommandX(MSG_TRACE, m_trace, 0, MAXPARA, TOK_TRACE, 0L);
	add_CommandX(MSG_PASS, m_pass, 0, MAXPARA, TOK_PASS, 0L);
	add_CommandX(MSG_TIME, m_time, 0, MAXPARA, TOK_TIME, 0L);
	add_CommandX(MSG_OPER, m_oper, 0, MAXPARA, TOK_OPER, 0L);
	add_CommandX(MSG_CONNECT, m_connect, 0, MAXPARA, TOK_CONNECT, 0L);
	add_CommandX(MSG_VERSION, m_version, 0, MAXPARA, TOK_VERSION, 0L);
	add_CommandX(MSG_STATS, m_stats, 0, MAXPARA, TOK_STATS, 0L);
	add_CommandX(MSG_LINKS, m_links, 0, MAXPARA, TOK_LINKS, 0L);
	add_CommandX(MSG_ADMIN, m_admin, 0, MAXPARA, TOK_ADMIN, 0L);
	add_CommandX(MSG_SUMMON, m_summon, 0, 1, TOK_SUMMON, 0L);
	add_CommandX(MSG_USERS, m_users, 0, MAXPARA, TOK_USERS, 0L);
	add_CommandX(MSG_SAMODE, m_samode, 0, MAXPARA, TOK_SAMODE, 0L);
	add_CommandX(MSG_SVSKILL, m_svskill, 0, MAXPARA, TOK_SVSKILL, 0L);
	add_CommandX(MSG_SVSNOOP, m_svsnoop, 0, MAXPARA, TOK_SVSNOOP, 0L);
	add_CommandX(MSG_CS, m_chanserv, 0, 1, TOK_CHANSERV, 0L);
	add_CommandX(MSG_NICKSERV, m_nickserv, 0, 1, TOK_NICKSERV, 0L);
	add_CommandX(MSG_NS, m_nickserv, 0, 1, TOK_NICKSERV, 0L);
	add_CommandX(MSG_INFOSERV, m_infoserv, 0, 1, TOK_INFOSERV, 0L);
	add_CommandX(MSG_IS, m_infoserv, 0, 1, TOK_INFOSERV, 0L);
	add_CommandX(MSG_OPERSERV, m_operserv, 0, 1, TOK_OPERSERV, 0L);
	add_CommandX(MSG_OS, m_operserv, 0, 1, TOK_OPERSERV, 0L);
	add_CommandX(MSG_MEMOSERV, m_memoserv, 0, 1, TOK_MEMOSERV, 0L);
	add_CommandX(MSG_MS, m_memoserv, 0, 1, TOK_MEMOSERV, 0L);
	add_CommandX(MSG_HELPSERV, m_helpserv, 0, 1, TOK_HELPSERV, 0L);
	add_CommandX(MSG_HS, m_helpserv, 0, 1, TOK_HELPSERV, 0L);
	add_CommandX(MSG_SERVICES, m_services, 0, 1, TOK_SERVICES, 0L);
	add_CommandX(MSG_HELP, m_help, 0, 1, TOK_HELP, 0L);
	add_CommandX(MSG_HELPOP, m_help, 0, 1, TOK_HELP, 0L);
	add_CommandX(MSG_INFO, m_info, 0, MAXPARA, TOK_INFO, 0L);
	add_CommandX(MSG_MOTD, m_motd, 0, MAXPARA, TOK_MOTD, 0L);
	add_CommandX(MSG_CLOSE, m_close, 0, MAXPARA, TOK_CLOSE, 0L);
	add_CommandX(MSG_SILENCE, m_silence, 0, MAXPARA, TOK_SILENCE, 0L);
	add_CommandX(MSG_AKILL, m_akill, 0, MAXPARA, TOK_AKILL, 0L);
	add_CommandX(MSG_SQLINE, m_sqline, 0, MAXPARA, TOK_SQLINE, 0L);
	add_CommandX(MSG_UNSQLINE, m_unsqline, 0, MAXPARA, TOK_UNSQLINE, 0L);
	add_CommandX(MSG_KLINE, m_kline, 0, MAXPARA, TOK_KLINE, 0L);
	add_CommandX(MSG_UNKLINE, m_unkline, 0, MAXPARA, TOK_UNKLINE, 0L);
	add_CommandX(MSG_ZLINE, m_zline, 0, MAXPARA, TOK_ZLINE, 0L);
	add_CommandX(MSG_UNZLINE, m_unzline, 0, MAXPARA, TOK_UNZLINE, 0L);
	add_CommandX(MSG_RAKILL, m_rakill, 0, MAXPARA, TOK_RAKILL, 0L);
	add_CommandX(MSG_GNOTICE, m_gnotice, 0, MAXPARA, TOK_GNOTICE, 0L);
	add_CommandX(MSG_GOPER, m_goper, 0, MAXPARA, TOK_GOPER, 0L);
	add_CommandX(MSG_GLOBOPS, m_globops, 0, MAXPARA, TOK_GLOBOPS, 0L);
	add_CommandX(MSG_CHATOPS, m_chatops, 0, 1, TOK_CHATOPS, 0L);
	add_CommandX(MSG_LOCOPS, m_locops, 0, 1, TOK_LOCOPS, 0L);
	add_CommandX(MSG_HASH, m_hash, 0, MAXPARA, TOK_HASH, 0L);
	add_CommandX(MSG_DNS, m_dns, 0, MAXPARA, TOK_DNS, 0L);
	add_CommandX(MSG_REHASH, m_rehash, 0, MAXPARA, TOK_REHASH, 0L);
	add_CommandX(MSG_RESTART, m_restart, 0, MAXPARA, TOK_RESTART, 0L);
	add_CommandX(MSG_DIE, m_die, 0, MAXPARA, TOK_DIE, 0L);
	add_CommandX(MSG_RULES, m_rules, 0, MAXPARA, TOK_RULES, 0L);
	add_CommandX(MSG_MAP, m_map, 0, MAXPARA, TOK_MAP, 0L);
	add_CommandX(MSG_GLINE, m_gline, 0, MAXPARA, TOK_GLINE, 0L);
	add_CommandX(MSG_REMGLINE, m_remgline, 0, MAXPARA, TOK_REMGLINE, 0L);
	add_CommandX(MSG_DALINFO, m_dalinfo, 0, MAXPARA, TOK_DALINFO, 0L);
	add_CommandX(MSG_SVS2MODE, m_svs2mode, 0, MAXPARA, TOK_SVS2MODE, 0L);
	add_CommandX(MSG_MKPASSWD, m_mkpasswd, 0, MAXPARA, TOK_MKPASSWD, 0L);
	add_CommandX(MSG_ADDLINE, m_addline, 0, 1, TOK_ADDLINE, 0L);
	add_CommandX(MSG_ADMINCHAT, m_admins, 0, 1, TOK_ADMINCHAT, 0L);
	add_CommandX(MSG_SETHOST, m_sethost, 0, MAXPARA, TOK_SETHOST, 0L);
	add_CommandX(MSG_TECHAT, m_techat, 0, 1, TOK_TECHAT, 0L);
	add_CommandX(MSG_NACHAT, m_nachat, 0, 1, TOK_NACHAT, 0L);
	add_CommandX(MSG_SETIDENT, m_setident, 0, MAXPARA, TOK_SETIDENT, 0L);
	add_CommandX(MSG_SETNAME, m_setname, 0, 1, TOK_SETNAME, 0L);
	add_CommandX(MSG_LAG, m_lag, 0, MAXPARA, TOK_LAG, 0L);
	add_CommandX(MSG_SDESC, m_sdesc, 0, 1, TOK_SDESC, 0L);
	add_CommandX(MSG_STATSERV, m_statserv, 0, 1, TOK_STATSERV, 0L);
	add_CommandX(MSG_KNOCK, m_knock, 0, 2, TOK_KNOCK, 0L);
	add_CommandX(MSG_CREDITS, m_credits, 0, MAXPARA, TOK_CREDITS, 0L);
	add_CommandX(MSG_LICENSE, m_license, 0, MAXPARA, TOK_LICENSE, 0L);
	add_CommandX(MSG_CHGHOST, m_chghost, 0, MAXPARA, TOK_CHGHOST, 0L);
	add_CommandX(MSG_RPING, m_rping, 0, MAXPARA, TOK_RPING, 0L);
	add_CommandX(MSG_RPONG, m_rpong, 0, MAXPARA, TOK_RPONG, 0L);
	add_CommandX(MSG_NETINFO, m_netinfo, 0, MAXPARA, TOK_NETINFO, 0L);
	add_CommandX(MSG_SENDUMODE, m_sendumode, 0, MAXPARA, TOK_SENDUMODE, 0L);
	add_CommandX(MSG_SMO, m_sendumode, 0, MAXPARA, TOK_SMO, 0L);
	add_CommandX(MSG_ADDMOTD, m_addmotd, 0, 1, TOK_ADDMOTD, 0L);
	add_CommandX(MSG_ADDOMOTD, m_addomotd, 0, 1, TOK_ADDOMOTD, 0L);
	add_CommandX(MSG_SVSMOTD, m_svsmotd, 0, MAXPARA, TOK_SVSMOTD, 0L);
	add_CommandX(MSG_OPERMOTD, m_opermotd, 0, MAXPARA, TOK_OPERMOTD, 0L);
	add_CommandX(MSG_TSCTL, m_tsctl, 0, MAXPARA, TOK_TSCTL, 0L);
	add_CommandX(MSG_SVSJOIN, m_svsjoin, 0, MAXPARA, TOK_SVSJOIN, 0L);
	add_CommandX(MSG_SAJOIN, m_sajoin, 0, MAXPARA, TOK_SAJOIN, 0L);
	add_CommandX(MSG_SVSPART, m_svspart, 0, MAXPARA, TOK_SVSPART, 0L);
	add_CommandX(MSG_SAPART, m_sapart, 0, MAXPARA, TOK_SAPART, 0L);
	add_CommandX(MSG_CHGIDENT, m_chgident, 0, MAXPARA, TOK_CHGIDENT, 0L);
	add_CommandX(MSG_SWHOIS, m_swhois, 0, MAXPARA, TOK_SWHOIS, 0L);
	add_CommandX(MSG_SVSO, m_svso, 0, MAXPARA, TOK_SVSO, 0L);
	add_CommandX(MSG_SVSFLINE, m_svsfline, 0, MAXPARA, TOK_SVSFLINE, 0L);
	add_CommandX(MSG_TKL, m_tkl, 0, MAXPARA, TOK_TKL, 0L);
	add_CommandX(MSG_VHOST, m_vhost, 0, MAXPARA, TOK_VHOST, 0L);
	add_CommandX(MSG_BOTMOTD, m_botmotd, 0, MAXPARA, TOK_BOTMOTD, 0L);
	add_CommandX(MSG_SJOIN, m_sjoin, 0, MAXPARA, TOK_SJOIN, 0L);
	add_CommandX(MSG_HTM, m_htm, 0, MAXPARA, TOK_HTM, 0L);
	add_CommandX(MSG_UMODE2, m_umode2, 0, MAXPARA, TOK_UMODE2, 0L);
	add_CommandX(MSG_DCCDENY, m_dccdeny, 0, 2, TOK_DCCDENY, 0L);
	add_CommandX(MSG_UNDCCDENY, m_undccdeny, 0, MAXPARA, TOK_UNDCCDENY, 0L);
	add_CommandX(MSG_CHGNAME, m_chgname, 0, MAXPARA, TOK_CHGNAME, 0L);
	add_CommandX(MSG_SVSNAME, m_chgname, 0, MAXPARA, TOK_CHGNAME, 0L);
	add_CommandX(MSG_SHUN, m_shun, 0, MAXPARA, TOK_SHUN, 0L);
	add_CommandX(MSG_NEWJOIN, m_join, 0, MAXPARA, TOK_JOIN, 0L);
	add_CommandX(MSG_BOTSERV, m_botserv, 0, 1, TOK_BOTSERV,0L);
	add_CommandX(TOK_BOTSERV, m_botserv, 0, 1, TOK_BOTSERV,0L);
	
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
