
/************************************************************************
 *   Unreal Internet Relay Chat Daemon, include/msg.h
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
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
 *
 *   $Id$
 */

#ifndef	__msg_include__
#define __msg_include__

#define MSG_PRIVATE	"PRIVMSG"	/* PRIV */
#define MSG_WHOIS	"WHOIS"	/* WHOI */
#define MSG_WHOWAS	"WHOWAS"	/* WHOW */
#define MSG_USER	"USER"	/* USER */
#define MSG_NICK	"NICK"	/* NICK */
#define MSG_SERVER	"SERVER"	/* SERV */
#define MSG_LIST	"LIST"	/* LIST */
#define MSG_TOPIC	"TOPIC"	/* TOPI */
#define MSG_INVITE	"INVITE"	/* INVI */
#define MSG_VERSION	"VERSION"	/* VERS */
#define MSG_QUIT	"QUIT"	/* QUIT */
#define MSG_SQUIT	"SQUIT"	/* SQUI */
#define MSG_KILL	"KILL"	/* KILL */
#define MSG_INFO	"INFO"	/* INFO */
#define MSG_LINKS	"LINKS"	/* LINK */
#define MSG_SUMMON	"SUMMON"	/* SUMM */
#define MSG_STATS	"STATS"	/* STAT */
#define MSG_USERS	"USERS"	/* USER -> USRS */
#define MSG_HELP	"HELP"	/* HELP */
#define MSG_HELPOP	"HELPOP"	/* HELP */
#define MSG_ERROR	"ERROR"	/* ERRO */
#define MSG_AWAY	"AWAY"	/* AWAY */
#define MSG_CONNECT	"CONNECT"	/* CONN */
#define MSG_PING	"PING"	/* PING */
#define MSG_PONG	"PONG"	/* PONG */
#define MSG_OPER	"OPER"	/* OPER */
#define MSG_PASS	"PASS"	/* PASS */
#define MSG_TIME	"TIME"	/* TIME */
#define MSG_NAMES	"NAMES"	/* NAME */
#define MSG_ADMIN	"ADMIN"	/* ADMI */
#define MSG_NOTICE	"NOTICE"	/* NOTI */
#define MSG_JOIN	"JOIN"	/* JOIN */
#define MSG_PART	"PART"	/* PART */
#define MSG_LUSERS	"LUSERS"	/* LUSE */
#define MSG_MOTD	"MOTD"	/* MOTD */
#define MSG_MODE	"MODE"	/* MODE */
#define MSG_KICK	"KICK"	/* KICK */
#define MSG_SERVICE	"SERVICE"	/* SERV -> SRVI */
#define MSG_USERHOST	"USERHOST"	/* USER -> USRH */
#define MSG_ISON	"ISON"	/* ISON */
#define	MSG_REHASH	"REHASH"	/* REHA */
#define	MSG_RESTART	"RESTART"	/* REST */
#define	MSG_CLOSE	"CLOSE"	/* CLOS */
#define	MSG_DIE		"DIE"	/* DIE */
#define	MSG_HASH	"HASH"	/* HASH */
#define	MSG_DNS		"DNS"	/* DNS  -> DNSS */
#define MSG_SILENCE	"SILENCE"	/* SILE */
#define MSG_AKILL	"AKILL"	/* AKILL */
#define MSG_KLINE	"KLINE"	/* KLINE */
#define MSG_UNKLINE     "UNKLINE"	/* UNKLINE */
#define MSG_RAKILL	"RAKILL"	/* RAKILL */
#define MSG_GNOTICE	"GNOTICE"	/* GNOTICE */
#define MSG_GOPER	"GOPER"	/* GOPER */
#define MSG_GLOBOPS	"GLOBOPS"	/* GLOBOPS */
#define MSG_LOCOPS	"LOCOPS"	/* LOCOPS */
#define MSG_PROTOCTL	"PROTOCTL"	/* PROTOCTL */
#define MSG_WATCH	"WATCH"	/* WATCH */
#define MSG_TRACE	"TRACE"	/* TRAC */
#define MSG_SQLINE	"SQLINE"	/* SQLINE */
#define MSG_UNSQLINE	"UNSQLINE"	/* UNSQLINE */
#define MSG_SVSNICK	"SVSNICK"	/* SVSNICK */
#define MSG_SVSNOOP	"SVSNOOP"	/* SVSNOOP */
#define MSG_IDENTIFY	"IDENTIFY"	/* IDENTIFY */
#define MSG_SVSKILL	"SVSKILL"	/* SVSKILL */
#define MSG_NICKSERV	"NICKSERV"	/* NICKSERV */
#define MSG_NS		"NS"
#define MSG_CHANSERV	"CHANSERV"	/* CHANSERV */
#define MSG_CS		"CS"
#define MSG_OPERSERV	"OPERSERV"	/* OPERSERV */
#define MSG_OS		"OS"
#define MSG_MEMOSERV	"MEMOSERV"	/* MEMOSERV */
#define MSG_MS		"MS"
#define MSG_SERVICES	"SERVICES"	/* SERVICES */
#define MSG_SVSMODE	"SVSMODE"	/* SVSMODE */
#define MSG_SAMODE	"SAMODE"	/* SAMODE */
#define MSG_CHATOPS	"CHATOPS"	/* CHATOPS */
#define MSG_ZLINE    	"ZLINE"	/* ZLINE */
#define MSG_UNZLINE  	"UNZLINE"	/* UNZLINE */
#define MSG_HELPSERV    "HELPSERV"	/* HELPSERV */
#define MSG_HS		"HS"
#define MSG_RULES       "RULES"	/* RULES */
#define MSG_MAP         "MAP"	/* MAP */
#define MSG_SVS2MODE    "SVS2MODE"	/* SVS2MODE */
#define MSG_DALINFO     "DALINFO"	/* dalinfo */
#define MSG_ADMINCHAT   "ADCHAT"	/* Admin chat */
#define MSG_MKPASSWD	"MKPASSWD"	/* MKPASSWD */
#define MSG_ADDLINE     "ADDLINE"	/* ADDLINE */
#define MSG_GLINE	"GLINE"	/* The awesome g-line */
#define MSG_SJOIN	"SJOIN"
#define MSG_SETHOST 	"SETHOST"	/* sethost */
#define MSG_NACHAT  	"NACHAT"	/* netadmin chat */
#define MSG_SETIDENT    "SETIDENT"
#define MSG_SETNAME	"SETNAME"	/* set GECOS */
#define MSG_LAG		"LAG"	/* Lag detect */
#define MSG_STATSERV	"STATSERV"	/* alias */
#define MSG_KNOCK	"KNOCK"
#define MSG_CREDITS 	"CREDITS"
#define MSG_LICENSE 	"LICENSE"
#define MSG_CHGHOST 	"CHGHOST"
#define MSG_NETINFO 	"NETINFO"
#define MSG_SENDUMODE 	"SENDUMODE"
#define MSG_ADDMOTD 	"ADDMOTD"
#define MSG_ADDOMOTD	"ADDOMOTD"
#define MSG_SVSMOTD	"SVSMOTD"
#define MSG_SMO 	"SMO"
#define MSG_OPERMOTD 	"OPERMOTD"
#define MSG_TSCTL 	"TSCTL"
#define MSG_SVSJOIN 	"SVSJOIN"
#define MSG_SAJOIN 	"SAJOIN"
#define MSG_SVSPART 	"SVSPART"
#define MSG_SAPART 	"SAPART"
#define MSG_CHGIDENT 	"CHGIDENT"
#define MSG_SWHOIS 	"SWHOIS"
#define MSG_SVSFLINE 	"SVSFLINE"
#define MSG_TKL		"TKL"
#define MSG_VHOST 	"VHOST"
#define MSG_BOTMOTD 	"BOTMOTD"
#define MSG_REMGLINE	"REMGLINE"	/* remove g-line */

#define MSG_UMODE2	"UMODE2"
#define MSG_DCCDENY	"DCCDENY"
#define MSG_UNDCCDENY   "UNDCCDENY"
#define MSG_CHGNAME	"CHGNAME"
#define MSG_SHUN	"SHUN"
#define MSG_NEWJOIN 	"NEWJOIN"	/* For CR Java Chat */
#define MSG_POST	"POST"
#define MSG_INFOSERV 	"INFOSERV"
#define MSG_IS		"IS"

#define MSG_BOTSERV	"BOTSERV"

#define MSG_CYCLE	"CYCLE"

#define MSG_MODULE	"MODULE"
/* BR and BT are in use */

#define MSG_SENDSNO	"SENDSNO"

#define MSG_EOS		"EOS"

#define MSG_MLOCK	"MLOCK"

#define MAXPARA    	15

#endif
