
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

/*
 * The tokens are in the ascii character range of 33-127, and we start
 * from 33 and just move up.  It would be nice to match then up so they
 * are slightly related to their string counterpart, but that makes it
 * too confusing when we want to add another one and need to make sure
 * we're not using one already used. -Cabal95
 *
 * As long as the #defines are kept statically placed, it will be fine.
 * We don't care/worry about the msgtab[] since it can be dynamic, but
 * the tokens it uses will still be static according to the messages
 * they represent.  In other words leave the #defines in order, if you're
 * going to add something, PUT IT AT THE END.  Do not even look for an
 * open spot somewhere, as that may lead to one type of message being
 * sent by server A to server B, but server B thinks its something else.
 * Remember, skip the : since its got a special use, and I skip the \ too
 * since it _may_ cause problems, but not sure.  -Cabal95
 * I'm skipping A and a as well, because some clients and scripts use
 * these to test if the server has already processed whole queue.
 * Since the client could request this protocol withhout the script
 * knowing it, I'm considering that reserved, and TRACE/A is now 'b'.
 * The normal msgtab should probably process this as special. -Donwulff
 */

/*	12/05/1999 - I was wrong - I didnt see the token[2] in struct Message
	okie 60*60 commands more :P - Sowwy!!! -sts
	
 */

#define MSG_PRIVATE	"PRIVMSG"	/* PRIV */
#define TOK_PRIVATE	"!"	/* 33 */
#define MSG_WHO		"WHO"	/* WHO  -> WHOC */
#define TOK_WHO		"\""	/* 34 */
#define MSG_WHOIS	"WHOIS"	/* WHOI */
#define TOK_WHOIS	"#"	/* 35 */
#define MSG_WHOWAS	"WHOWAS"	/* WHOW */
#define TOK_WHOWAS	"$"	/* 36 */
#define MSG_USER	"USER"	/* USER */
#define TOK_USER	"%"	/* 37 */
#define MSG_NICK	"NICK"	/* NICK */
#define TOK_NICK	"&"	/* 38 */
#define MSG_SERVER	"SERVER"	/* SERV */
#define TOK_SERVER	"'"	/* 39 */
#define MSG_LIST	"LIST"	/* LIST */
#define TOK_LIST	"("	/* 40 */
#define MSG_TOPIC	"TOPIC"	/* TOPI */
#define TOK_TOPIC	")"	/* 41 */
#define MSG_INVITE	"INVITE"	/* INVI */
#define TOK_INVITE	"*"	/* 42 */
#define MSG_VERSION	"VERSION"	/* VERS */
#define TOK_VERSION	"+"	/* 43 */
#define MSG_QUIT	"QUIT"	/* QUIT */
#define TOK_QUIT	","	/* 44 */
#define MSG_SQUIT	"SQUIT"	/* SQUI */
#define TOK_SQUIT	"-"	/* 45 */
#define MSG_KILL	"KILL"	/* KILL */
#define TOK_KILL	"."	/* 46 */
#define MSG_INFO	"INFO"	/* INFO */
#define TOK_INFO	"/"	/* 47 */
#define MSG_LINKS	"LINKS"	/* LINK */
#define TOK_LINKS	"0"	/* 48 */
#define MSG_SUMMON	"SUMMON"	/* SUMM */
#define TOK_SUMMON	"1"	/* 49 */
#define MSG_STATS	"STATS"	/* STAT */
#define TOK_STATS	"2"	/* 50 */
#define MSG_USERS	"USERS"	/* USER -> USRS */
#define TOK_USERS	"3"	/* 51 */
#define MSG_HELP	"HELP"	/* HELP */
#define MSG_HELPOP	"HELPOP"	/* HELP */
#define TOK_HELP	"4"	/* 52 */
#define MSG_ERROR	"ERROR"	/* ERRO */
#define TOK_ERROR	"5"	/* 53 */
#define MSG_AWAY	"AWAY"	/* AWAY */
#define TOK_AWAY	"6"	/* 54 */
#define MSG_CONNECT	"CONNECT"	/* CONN */
#define TOK_CONNECT	"7"	/* 55 */
#define MSG_PING	"PING"	/* PING */
#define TOK_PING	"8"	/* 56 */
#define MSG_PONG	"PONG"	/* PONG */
#define TOK_PONG	"9"	/* 57 */
#define MSG_OPER	"OPER"	/* OPER */
#define TOK_OPER	";"	/* 59 */
#define MSG_PASS	"PASS"	/* PASS */
#define TOK_PASS	"<"	/* 60 */
#define MSG_WALLOPS	"WALLOPS"	/* WALL */
#define TOK_WALLOPS	"="	/* 61 */
#define MSG_TIME	"TIME"	/* TIME */
#define TOK_TIME	">"	/* 62 */
#define MSG_NAMES	"NAMES"	/* NAME */
#define TOK_NAMES	"?"	/* 63 */
#define MSG_ADMIN	"ADMIN"	/* ADMI */
#define TOK_ADMIN	"@"	/* 64 */
#define MSG_NOTICE	"NOTICE"	/* NOTI */
#define TOK_NOTICE	"B"	/* 66 */
#define MSG_JOIN	"JOIN"	/* JOIN */
#define TOK_JOIN	"C"	/* 67 */
#define MSG_PART	"PART"	/* PART */
#define TOK_PART	"D"	/* 68 */
#define MSG_LUSERS	"LUSERS"	/* LUSE */
#define TOK_LUSERS	"E"	/* 69 */
#define MSG_MOTD	"MOTD"	/* MOTD */
#define TOK_MOTD	"F"	/* 70 */
#define MSG_MODE	"MODE"	/* MODE */
#define TOK_MODE	"G"	/* 71 */
#define MSG_KICK	"KICK"	/* KICK */
#define TOK_KICK	"H"	/* 72 */
#define MSG_SERVICE	"SERVICE"	/* SERV -> SRVI */
#define TOK_SERVICE	"I"	/* 73 */
#define MSG_USERHOST	"USERHOST"	/* USER -> USRH */
#define TOK_USERHOST	"J"	/* 74 */
#define MSG_ISON	"ISON"	/* ISON */
#define TOK_ISON	"K"	/* 75 */
#define	MSG_REHASH	"REHASH"	/* REHA */
#define TOK_REHASH	"O"	/* 79 */
#define	MSG_RESTART	"RESTART"	/* REST */
#define TOK_RESTART	"P"	/* 80 */
#define	MSG_CLOSE	"CLOSE"	/* CLOS */
#define TOK_CLOSE	"Q"	/* 81 */
#define	MSG_DIE		"DIE"	/* DIE */
#define TOK_DIE		"R"	/* 82 */
#define	MSG_HASH	"HASH"	/* HASH */
#define TOK_HASH	"S"	/* 83 */
#define	MSG_DNS		"DNS"	/* DNS  -> DNSS */
#define TOK_DNS		"T"	/* 84 */
#define MSG_SILENCE	"SILENCE"	/* SILE */
#define TOK_SILENCE	"U"	/* 85 */
#define MSG_AKILL	"AKILL"	/* AKILL */
#define TOK_AKILL	"V"	/* 86 */
#define MSG_KLINE	"KLINE"	/* KLINE */
#define TOK_KLINE	"W"	/* 87 */
#define MSG_UNKLINE     "UNKLINE"	/* UNKLINE */
#define TOK_UNKLINE	"X"	/* 88 */
#define MSG_RAKILL	"RAKILL"	/* RAKILL */
#define TOK_RAKILL	"Y"	/* 89 */
#define MSG_GNOTICE	"GNOTICE"	/* GNOTICE */
#define TOK_GNOTICE	"Z"	/* 90 */
#define MSG_GOPER	"GOPER"	/* GOPER */
#define TOK_GOPER	"["	/* 91 */
#define MSG_GLOBOPS	"GLOBOPS"	/* GLOBOPS */
#define TOK_GLOBOPS	"]"	/* 93 */
#define MSG_LOCOPS	"LOCOPS"	/* LOCOPS */
#define TOK_LOCOPS	"^"	/* 94 */
#define MSG_PROTOCTL	"PROTOCTL"	/* PROTOCTL */
#define TOK_PROTOCTL	"_"	/* 95 */
#define MSG_WATCH	"WATCH"	/* WATCH */
#define TOK_WATCH	"`"	/* 96 */
#define MSG_TRACE	"TRACE"	/* TRAC */
#define TOK_TRACE	"b"	/* 97 */
#define MSG_SQLINE	"SQLINE"	/* SQLINE */
#define TOK_SQLINE	"c"	/* 98 */
#define MSG_UNSQLINE	"UNSQLINE"	/* UNSQLINE */
#define TOK_UNSQLINE	"d"	/* 99 */
#define MSG_SVSNICK	"SVSNICK"	/* SVSNICK */
#define TOK_SVSNICK	"e"	/* 100 */
#define MSG_SVSNOOP	"SVSNOOP"	/* SVSNOOP */
#define TOK_SVSNOOP	"f"	/* 101 */
#define MSG_IDENTIFY	"IDENTIFY"	/* IDENTIFY */
#define TOK_IDENTIFY	"g"	/* 102 */
#define MSG_SVSKILL	"SVSKILL"	/* SVSKILL */
#define TOK_SVSKILL	"h"	/* 103 */
#define MSG_NICKSERV	"NICKSERV"	/* NICKSERV */
#define MSG_NS		"NS"
#define TOK_NICKSERV	"i"	/* 104 */
#define MSG_CHANSERV	"CHANSERV"	/* CHANSERV */
#define MSG_CS		"CS"
#define TOK_CHANSERV	"j"	/* 105 */
#define MSG_OPERSERV	"OPERSERV"	/* OPERSERV */
#define MSG_OS		"OS"
#define TOK_OPERSERV	"k"	/* 106 */
#define MSG_MEMOSERV	"MEMOSERV"	/* MEMOSERV */
#define MSG_MS		"MS"
#define TOK_MEMOSERV	"l"	/* 107 */
#define MSG_SERVICES	"SERVICES"	/* SERVICES */
#define TOK_SERVICES	"m"	/* 108 */
#define MSG_SVSMODE	"SVSMODE"	/* SVSMODE */
#define TOK_SVSMODE	"n"	/* 109 */
#define MSG_SAMODE	"SAMODE"	/* SAMODE */
#define TOK_SAMODE	"o"	/* 110 */
#define MSG_CHATOPS	"CHATOPS"	/* CHATOPS */
#define TOK_CHATOPS	"p"	/* 111 */
#define MSG_ZLINE    	"ZLINE"	/* ZLINE */
#define TOK_ZLINE	"q"	/* 112 */
#define MSG_UNZLINE  	"UNZLINE"	/* UNZLINE */
#define TOK_UNZLINE	"r"	/* 113 */
#define MSG_HELPSERV    "HELPSERV"	/* HELPSERV */
#define MSG_HS		"HS"
#define TOK_HELPSERV    "s"	/* 114 */
#define MSG_RULES       "RULES"	/* RULES */
#define TOK_RULES       "t"	/* 115 */
#define MSG_MAP         "MAP"	/* MAP */
#define TOK_MAP         "u"	/* 117 */
#define MSG_SVS2MODE    "SVS2MODE"	/* SVS2MODE */
#define TOK_SVS2MODE	"v"	/* 118 */
#define MSG_DALINFO     "DALINFO"	/* dalinfo */
#define TOK_DALINFO     "w"	/* 119 */
#define MSG_ADMINCHAT   "ADCHAT"	/* Admin chat */
#define TOK_ADMINCHAT   "x"	/* 120 */
#define MSG_MKPASSWD	"MKPASSWD"	/* MKPASSWD */
#define TOK_MKPASSWD	"y"	/* 121 */
#define MSG_ADDLINE     "ADDLINE"	/* ADDLINE */
#define TOK_ADDLINE     "z"	/* 122 */
#define MSG_GLINE	"GLINE"	/* The awesome g-line */
#define TOK_GLINE	"}"	/* 125 */
#define MSG_SJOIN	"SJOIN"
#define TOK_SJOIN	"~"
#define MSG_SETHOST 	"SETHOST"	/* sethost */
#define TOK_SETHOST 	"AA"	/* 127 4ever !;) */
#define MSG_TECHAT 	"TECHAT"	/* techadmin chat */
#define TOK_TECHAT  	"AB"	/* questionmark? */
#define MSG_NACHAT  	"NACHAT"	/* netadmin chat */
#define TOK_NACHAT  	"AC"	/* *beep* */
#define MSG_SETIDENT 	"SETIDENT"	/* set ident */
#define	TOK_SETIDENT	"AD"	/* good old BASIC ;P */
#define MSG_SETNAME	"SETNAME"	/* set GECOS */
#define TOK_SETNAME	"AE"	/* its almost unreeaaall... */
#define MSG_LAG		"LAG"	/* Lag detect */
#define TOK_LAG		"AF"	/* a or ? */
#define MSG_SDESC       "SDESC"	/* set description */
#define TOK_SDESC       "AG"
#define MSG_STATSERV	"STATSERV"	/* alias */
#define TOK_STATSERV	"AH"
#define MSG_KNOCK	"KNOCK"
#define TOK_KNOCK	"AI"
#define MSG_CREDITS 	"CREDITS"
#define TOK_CREDITS 	"AJ"
#define MSG_LICENSE 	"LICENSE"
#define TOK_LICENSE 	"AK"
#define MSG_CHGHOST 	"CHGHOST"
#define TOK_CHGHOST 	"AL"
#define MSG_RPING   	"RPING"
#define TOK_RPING	"AM"
#define MSG_RPONG   	"RPONG"
#define TOK_RPONG	"AN"
#define MSG_NETINFO 	"NETINFO"
#define TOK_NETINFO 	"AO"
#define MSG_SENDUMODE 	"SENDUMODE"
#define TOK_SENDUMODE 	"AP"
#define MSG_ADDMOTD 	"ADDMOTD"
#define TOK_ADDMOTD	"AQ"
#define MSG_ADDOMOTD	"ADDOMOTD"
#define TOK_ADDOMOTD	"AR"
#define MSG_SVSMOTD	"SVSMOTD"
#define TOK_SVSMOTD	"AS"
#define MSG_SMO 	"SMO"
#define TOK_SMO 	"AU"
#define MSG_OPERMOTD 	"OPERMOTD"
#define TOK_OPERMOTD 	"AV"
#define MSG_TSCTL 	"TSCTL"
#define TOK_TSCTL 	"AW"
#define MSG_SVSJOIN 	"SVSJOIN"
#define TOK_SVSJOIN 	"AX"
#define MSG_SAJOIN 	"SAJOIN"
#define TOK_SAJOIN 	"AY"
#define MSG_SVSPART 	"SVSPART"
#define TOK_SVSPART 	"AX"
#define MSG_SAPART 	"SAPART"
#define TOK_SAPART 	"AY"
#define MSG_CHGIDENT 	"CHGIDENT"
#define TOK_CHGIDENT 	"AZ"
#define MSG_SWHOIS 	"SWHOIS"
#define TOK_SWHOIS 	"BA"
#define MSG_SVSO 	"SVSO"
#define TOK_SVSO 	"BB"
#define MSG_SVSFLINE 	"SVSFLINE"
#define TOK_SVSFLINE 	"BC"
#define MSG_TKL		"TKL"
#define TOK_TKL 	"BD"
#define MSG_VHOST 	"VHOST"
#define TOK_VHOST 	"BE"
#define MSG_BOTMOTD 	"BOTMOTD"
#define TOK_BOTMOTD 	"BF"
#define MSG_REMGLINE	"REMGLINE"	/* remove g-line */
#define TOK_REMGLINE	"BG"
#define MSG_HTM		"HTM"
#define TOK_HTM		"BH"
#define MSG_UMODE2	"UMODE2"
#define TOK_UMODE2	"|"
#define MSG_DCCDENY	"DCCDENY"
#define TOK_DCCDENY	"BI"
#define MSG_UNDCCDENY   "UNDCCDENY"
#define TOK_UNDCCDENY   "BJ"
#define MSG_CHGNAME	"CHGNAME"
#define MSG_SVSNAME	"SVSNAME"
#define TOK_CHGNAME	"BK"
#define MSG_SHUN	"SHUN"
#define TOK_SHUN	"BL"
#ifdef CRYPTOIRCD
#define MSG_CRYPTO	"CRYPTO"
#define TOK_CRYPTO	"BM"
#endif
#define MSG_NEWJOIN 	"NEWJOIN"	/* For CR Java Chat */
#define MSG_POST	"POST"
#define TOK_POST	"BN"
#define MSG_INFOSERV 	"INFOSERV"
#define MSG_IS		"IS"
#define TOK_INFOSERV	"BO"
#define MAXPARA    	15

extern int m_private(), m_topic(), m_join(), m_part(), m_mode(), m_svsmode();
extern int m_ping(), m_pong(), m_wallops(), m_kick(), m_svsnick();
extern int m_nick(), m_error(), m_notice(), m_samode(), m_svsnoop();
extern int m_invite(), m_quit(), m_kill(), m_svskill(), m_identify();
extern int m_akill(), m_kline(), m_unkline(), m_rakill(), m_sqline();
extern int m_zline(), m_unzline();
extern int m_gnotice(), m_goper(), m_globops(), m_locops(), m_unsqline(),
m_chatops();
extern int m_protoctl();
extern int m_motd(), m_who(), m_whois(), m_user(), m_list();
extern int m_server(), m_info(), m_links(), m_summon(), m_stats();
extern int m_users(), m_version(), m_help();
extern int m_nickserv(), m_operserv(), m_chanserv(), m_memoserv(),
m_infoserv(), m_helpserv(), m_services(), m_identify();
extern int m_statserv();
extern int m_squit(), m_away(), m_connect();
extern int m_oper(), m_pass(), m_trace();
extern int m_time(), m_names(), m_admin();
extern int m_lusers(), m_umode(), m_close();
extern int m_motd(), m_whowas(), m_silence();
extern int m_service(), m_userhost(), m_ison(), m_watch();
extern int m_service(), m_servset(), m_servlist(), m_squery();
extern int m_rehash(), m_restart(), m_die(), m_dns(), m_hash();
/*extern int m_noshortn(),m_noshortc(),m_noshortm(),m_noshorto(),m_noshorth();*/

extern int m_gline(), m_remgline(), m_map(), m_svs2mode(), m_admins(),
m_dalinfo();
extern int m_addline(), m_rules(), m_mkpasswd();
extern int m_sethost(), m_nachat(), m_techat(), m_setident(), m_setname();
extern int m_lag(), m_sdesc(), m_knock(), m_credits();
extern int m_license(), m_chghost(), m_rping(), m_rpong();
extern int m_netinfo(), m_sendumode(), m_addmotd(), m_addomotd();
extern int m_svsmotd(), m_opermotd(), m_tsctl();
extern int m_svsjoin(), m_sajoin(), m_svspart(), m_sapart();
extern int m_chgident(), m_swhois(), m_svso(), m_svsfline();
extern int m_tkl(), m_vhost(), m_botmotd(), m_sjoin(), m_htm();
extern int m_umode2(), m_dccdeny(), m_undccdeny();
extern int m_chgname(), m_shun(), m_post();
#ifdef CRYPTOIRCD
extern int m_crypto();
#endif
#ifdef GUEST
extern int m_guest();
#endif

#ifdef MSGTAB
struct Message *msgmap[256];
struct Message msgtab[] = {
	{MSG_PRIVATE, m_private, 0, MAXPARA, TOK_PRIVATE, 0L},
	{MSG_NOTICE, m_notice, 0, MAXPARA, TOK_NOTICE, 0L},
	{MSG_MODE, m_mode, 0, MAXPARA, TOK_MODE, 0L},
	{MSG_NICK, m_nick, 0, MAXPARA, TOK_NICK, 0L},
	{MSG_JOIN, m_join, 0, MAXPARA, TOK_JOIN, 0L},
	{MSG_PING, m_ping, 0, MAXPARA, TOK_PING, 0L},
	{MSG_WHOIS, m_whois, 0, MAXPARA, TOK_WHOIS, 0L},
	{MSG_ISON, m_ison, 0, 1, TOK_ISON, 0L},
	{MSG_USER, m_user, 0, MAXPARA, TOK_USER, 0L},
	{MSG_PONG, m_pong, 0, MAXPARA, TOK_PONG, 0L},
	{MSG_PART, m_part, 0, MAXPARA, TOK_PART, 0L},
	{MSG_QUIT, m_quit, 0, MAXPARA, TOK_QUIT, 0L},
	{MSG_WATCH, m_watch, 0, 1, TOK_WATCH, 0L},
	{MSG_USERHOST, m_userhost, 0, 1, TOK_USERHOST, 0L},
	{MSG_SVSNICK, m_svsnick, 0, MAXPARA, TOK_SVSNICK, 0L},
	{MSG_SVSMODE, m_svsmode, 0, MAXPARA, TOK_SVSMODE, 0L},
	{MSG_LUSERS, m_lusers, 0, MAXPARA, TOK_LUSERS, 0L},
	{MSG_IDENTIFY, m_identify, 0, 1, TOK_IDENTIFY, 0L},
	{MSG_CHANSERV, m_chanserv, 0, 1, TOK_CHANSERV, 0L},
	{MSG_TOPIC, m_topic, 0, MAXPARA, TOK_TOPIC, 0L},
	{MSG_INVITE, m_invite, 0, MAXPARA, TOK_INVITE, 0L},
	{MSG_KICK, m_kick, 0, MAXPARA, TOK_KICK, 0L},
	{MSG_WALLOPS, m_wallops, 0, 1, TOK_WALLOPS, 0L},
	{MSG_ERROR, m_error, 0, MAXPARA, TOK_ERROR, 0L},
	{MSG_KILL, m_kill, 0, MAXPARA, TOK_KILL, 0L},
	{MSG_PROTOCTL, m_protoctl, 0, MAXPARA, TOK_PROTOCTL, 0L},
	{MSG_AWAY, m_away, 0, MAXPARA, TOK_AWAY, 0L},
	{MSG_SERVER, m_server, 0, MAXPARA, TOK_SERVER, 0L},
	{MSG_SQUIT, m_squit, 0, MAXPARA, TOK_SQUIT, 0L},
	{MSG_WHO, m_who, 0, MAXPARA, TOK_WHO, 0L},
	{MSG_WHOWAS, m_whowas, 0, MAXPARA, TOK_WHOWAS, 0L},
	{MSG_LIST, m_list, 0, MAXPARA, TOK_LIST, 0L},
	{MSG_NAMES, m_names, 0, MAXPARA, TOK_NAMES, 0L},
	{MSG_TRACE, m_trace, 0, MAXPARA, TOK_TRACE, 0L},
	{MSG_PASS, m_pass, 0, MAXPARA, TOK_PASS, 0L},
	{MSG_TIME, m_time, 0, MAXPARA, TOK_TIME, 0L},
	{MSG_OPER, m_oper, 0, MAXPARA, TOK_OPER, 0L},
	{MSG_CONNECT, m_connect, 0, MAXPARA, TOK_CONNECT, 0L},
	{MSG_VERSION, m_version, 0, MAXPARA, TOK_VERSION, 0L},
	{MSG_STATS, m_stats, 0, MAXPARA, TOK_STATS, 0L},
	{MSG_LINKS, m_links, 0, MAXPARA, TOK_LINKS, 0L},
	{MSG_ADMIN, m_admin, 0, MAXPARA, TOK_ADMIN, 0L},
	{MSG_SUMMON, m_summon, 0, 1, TOK_SUMMON, 0L},
	{MSG_USERS, m_users, 0, MAXPARA, TOK_USERS, 0L},
	{MSG_SAMODE, m_samode, 0, MAXPARA, TOK_SAMODE, 0L},
	{MSG_SVSKILL, m_svskill, 0, MAXPARA, TOK_SVSKILL, 0L},
	{MSG_SVSNOOP, m_svsnoop, 0, MAXPARA, TOK_SVSNOOP, 0L},
	{MSG_CS, m_chanserv, 0, 1, TOK_CHANSERV, 0L},
	{MSG_NICKSERV, m_nickserv, 0, 1, TOK_NICKSERV, 0L},
	{MSG_NS, m_nickserv, 0, 1, TOK_NICKSERV, 0L},
	{MSG_INFOSERV, m_infoserv, 0, 1, TOK_INFOSERV, 0L},
	{MSG_IS, m_infoserv, 0, 1, TOK_INFOSERV, 0L},
	{MSG_OPERSERV, m_operserv, 0, 1, TOK_OPERSERV, 0L},
	{MSG_OS, m_operserv, 0, 1, TOK_OPERSERV, 0L},
	{MSG_MEMOSERV, m_memoserv, 0, 1, TOK_MEMOSERV, 0L},
	{MSG_MS, m_memoserv, 0, 1, TOK_MEMOSERV, 0L},
	{MSG_HELPSERV, m_helpserv, 0, 1, TOK_HELPSERV, 0L},
	{MSG_HS, m_helpserv, 0, 1, TOK_HELPSERV, 0L},
	{MSG_SERVICES, m_services, 0, 1, TOK_SERVICES, 0L},
	{MSG_HELP, m_help, 0, 1, TOK_HELP, 0L},
	{MSG_HELPOP, m_help, 0, 1, TOK_HELP, 0L},
	{MSG_INFO, m_info, 0, MAXPARA, TOK_INFO, 0L},
	{MSG_MOTD, m_motd, 0, MAXPARA, TOK_MOTD, 0L},
	{MSG_CLOSE, m_close, 0, MAXPARA, TOK_CLOSE, 0L},
	{MSG_SILENCE, m_silence, 0, MAXPARA, TOK_SILENCE, 0L},
	{MSG_AKILL, m_akill, 0, MAXPARA, TOK_AKILL, 0L},
	{MSG_SQLINE, m_sqline, 0, MAXPARA, TOK_SQLINE, 0L},
	{MSG_UNSQLINE, m_unsqline, 0, MAXPARA, TOK_UNSQLINE, 0L},
	{MSG_KLINE, m_kline, 0, MAXPARA, TOK_KLINE, 0L},
	{MSG_UNKLINE, m_unkline, 0, MAXPARA, TOK_UNKLINE, 0L},
	{MSG_ZLINE, m_zline, 0, MAXPARA, TOK_ZLINE, 0L},
	{MSG_UNZLINE, m_unzline, 0, MAXPARA, TOK_UNZLINE, 0L},
	{MSG_RAKILL, m_rakill, 0, MAXPARA, TOK_RAKILL, 0L},
	{MSG_GNOTICE, m_gnotice, 0, MAXPARA, TOK_GNOTICE, 0L},
	{MSG_GOPER, m_goper, 0, MAXPARA, TOK_GOPER, 0L},
	{MSG_GLOBOPS, m_globops, 0, MAXPARA, TOK_GLOBOPS, 0L},
	{MSG_CHATOPS, m_chatops, 0, 1, TOK_CHATOPS, 0L},
	{MSG_LOCOPS, m_locops, 0, 1, TOK_LOCOPS, 0L},
	{MSG_HASH, m_hash, 0, MAXPARA, TOK_HASH, 0L},
	{MSG_DNS, m_dns, 0, MAXPARA, TOK_DNS, 0L},
	{MSG_REHASH, m_rehash, 0, MAXPARA, TOK_REHASH, 0L},
	{MSG_RESTART, m_restart, 0, MAXPARA, TOK_RESTART, 0L},
	{MSG_DIE, m_die, 0, MAXPARA, TOK_DIE, 0L},
	{MSG_RULES, m_rules, 0, MAXPARA, TOK_RULES, 0L},
	{MSG_MAP, m_map, 0, MAXPARA, TOK_MAP, 0L},
	{MSG_GLINE, m_gline, 0, MAXPARA, TOK_GLINE, 0L},
	{MSG_REMGLINE, m_remgline, 0, MAXPARA, TOK_REMGLINE, 0L},
	{MSG_DALINFO, m_dalinfo, 0, MAXPARA, TOK_DALINFO, 0L},
	{MSG_SVS2MODE, m_svs2mode, 0, MAXPARA, TOK_SVS2MODE, 0L},
	{MSG_MKPASSWD, m_mkpasswd, 0, MAXPARA, TOK_MKPASSWD, 0L},
	{MSG_ADDLINE, m_addline, 0, 1, TOK_ADDLINE, 0L},
	{MSG_ADMINCHAT, m_admins, 0, 1, TOK_ADMINCHAT, 0L},
	{MSG_SETHOST, m_sethost, 0, MAXPARA, TOK_SETHOST, 0L},
	{MSG_TECHAT, m_techat, 0, 1, TOK_TECHAT, 0L},
	{MSG_NACHAT, m_nachat, 0, 1, TOK_NACHAT, 0L},
	{MSG_SETIDENT, m_setident, 0, MAXPARA, TOK_SETIDENT, 0L},
	{MSG_SETNAME, m_setname, 0, 1, TOK_SETNAME, 0L},
	{MSG_LAG, m_lag, 0, MAXPARA, TOK_LAG, 0L},
	{MSG_SDESC, m_sdesc, 0, 1, TOK_SDESC, 0L},
	{MSG_STATSERV, m_statserv, 0, 1, TOK_STATSERV, 0L},
	{MSG_KNOCK, m_knock, 0, 2, TOK_KNOCK, 0L},
	{MSG_CREDITS, m_credits, 0, MAXPARA, TOK_CREDITS, 0L},
	{MSG_LICENSE, m_license, 0, MAXPARA, TOK_LICENSE, 0L},
	{MSG_CHGHOST, m_chghost, 0, MAXPARA, TOK_CHGHOST, 0L},
	{MSG_RPING, m_rping, 0, MAXPARA, TOK_RPING, 0L},
	{MSG_RPONG, m_rpong, 0, MAXPARA, TOK_RPONG, 0L},
	{MSG_NETINFO, m_netinfo, 0, MAXPARA, TOK_NETINFO, 0L},
	{MSG_SENDUMODE, m_sendumode, 0, MAXPARA, TOK_SENDUMODE, 0L},
	{MSG_SMO, m_sendumode, 0, MAXPARA, TOK_SMO, 0L},
	{MSG_ADDMOTD, m_addmotd, 0, 1, TOK_ADDMOTD, 0L},
	{MSG_ADDOMOTD, m_addomotd, 0, 1, TOK_ADDOMOTD, 0L},
	{MSG_SVSMOTD, m_svsmotd, 0, MAXPARA, TOK_SVSMOTD, 0L},
	{MSG_OPERMOTD, m_opermotd, 0, MAXPARA, TOK_OPERMOTD, 0L},
	{MSG_TSCTL, m_tsctl, 0, MAXPARA, TOK_TSCTL, 0L},
	{MSG_SVSJOIN, m_svsjoin, 0, MAXPARA, TOK_SVSJOIN, 0L},
	{MSG_SAJOIN, m_sajoin, 0, MAXPARA, TOK_SAJOIN, 0L},
	{MSG_SVSPART, m_svspart, 0, MAXPARA, TOK_SVSPART, 0L},
	{MSG_SAPART, m_sapart, 0, MAXPARA, TOK_SAPART, 0L},
	{MSG_CHGIDENT, m_chgident, 0, MAXPARA, TOK_CHGIDENT, 0L},
	{MSG_SWHOIS, m_swhois, 0, MAXPARA, TOK_SWHOIS, 0L},
	{MSG_SVSO, m_svso, 0, MAXPARA, TOK_SVSO, 0L},
	{MSG_SVSFLINE, m_svsfline, 0, MAXPARA, TOK_SVSFLINE, 0L},
	{MSG_TKL, m_tkl, 0, MAXPARA, TOK_TKL, 0L},
	{MSG_VHOST, m_vhost, 0, MAXPARA, TOK_VHOST, 0L},
	{MSG_BOTMOTD, m_botmotd, 0, MAXPARA, TOK_BOTMOTD, 0L},
	{MSG_SJOIN, m_sjoin, 0, MAXPARA, TOK_SJOIN, 0L},
	{MSG_HTM, m_htm, 0, MAXPARA, TOK_HTM, 0L},
	{MSG_UMODE2, m_umode2, 0, MAXPARA, TOK_UMODE2, 0L},
	{MSG_DCCDENY, m_dccdeny, 0, 2, TOK_DCCDENY, 0L},
	{MSG_UNDCCDENY, m_undccdeny, 0, MAXPARA, TOK_UNDCCDENY, 0L},
	{MSG_CHGNAME, m_chgname, 0, MAXPARA, TOK_CHGNAME, 0L},
	{MSG_SVSNAME, m_chgname, 0, MAXPARA, TOK_CHGNAME, 0L},
	{MSG_SHUN, m_shun, 0, MAXPARA, TOK_SHUN, 0L},
#ifdef CRYPTOIRCD
	{MSG_CRYPTO, m_crypto, 0, MAXPARA, TOK_CRYPTO, 0L},
#endif
	{MSG_NEWJOIN, m_join, 0, MAXPARA, TOK_JOIN, 0L},
	{(char *)0, (int (*)())0, 0, 0, 0, 0L}
};

#else
extern struct Message msgtab[];
extern struct Message *msgmap[256];
#endif
#endif /* __msg_include__ */
