
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
#define TOK_PRIVATE	"!"	/* 33 */
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
#define MSG_NACHAT  	"NACHAT"	/* netadmin chat */
#define TOK_NACHAT  	"AC"	/* *beep* */
#define MSG_SETIDENT    "SETIDENT"
#define TOK_SETIDENT    "AD"
#define MSG_SETNAME	"SETNAME"	/* set GECOS */
#define TOK_SETNAME	"AE"	/* its almost unreeaaall... */
#define MSG_LAG		"LAG"	/* Lag detect */
#define TOK_LAG		"AF"	/* a or ? */
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
#define TOK_SVSJOIN 	"BR"
#define MSG_SAJOIN 	"SAJOIN"
#define TOK_SAJOIN 	"AX"
#define MSG_SVSPART 	"SVSPART"
#define TOK_SVSPART 	"BT"
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
#define MSG_NEWJOIN 	"NEWJOIN"	/* For CR Java Chat */
#define MSG_POST	"POST"
#define TOK_POST	"BN"
#define MSG_INFOSERV 	"INFOSERV"
#define MSG_IS		"IS"
#define TOK_INFOSERV	"BO"

#define MSG_BOTSERV	"BOTSERV"
#define TOK_BOTSERV	"BS"

#define MSG_CYCLE	"CYCLE"
#define TOK_CYCLE	"BP"

#define MSG_MODULE	"MODULE"
#define TOK_MODULE	"BQ"
/* BR and BT are in use */

#define MSG_SENDSNO	"SENDSNO"
#define TOK_SENDSNO	"Ss"

#define MSG_EOS		"EOS"
#define TOK_EOS		"ES"

#define MAXPARA    	15

extern int m_join(), m_part(), m_mode();
extern int m_wallops();
extern int m_nick(), m_error(), m_samode();
extern int m_svskill();
extern int m_chatops(), m_dns();
extern int m_gnotice(), m_goper(), m_globops(), m_locops();
extern int m_protoctl(), m_tkl();
extern int m_motd(), m_user();
extern int m_server(), m_info(), m_links(), m_summon(), m_stats();
extern int m_users(), m_version(), m_help();
extern int m_squit(), m_connect();
extern int m_pass(), m_trace();
extern int m_time(), m_names(), m_admin();
extern int m_lusers(), m_umode(), m_close();
extern int m_motd(), m_whowas(), m_silence();
extern int m_service(), m_userhost(), m_ison(), m_watch();
extern int m_map(), m_dalinfo();
extern int m_addline(), m_rules();
extern int m_knock(),m_credits();
extern int m_license();
extern int m_netinfo(), m_addmotd(), m_addomotd();
extern int m_svsfline();
extern int m_botmotd(), m_sjoin();
extern int m_umode2(), m_dccdeny(), m_undccdeny();
extern int m_opermotd();
extern int m_module(), m_alias(), m_tkl(), m_opermotd();
extern int m_rehash(), m_die(), m_restart();
#endif

