/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_err.c
 *   Copyright (C) 1992 Darren Reed
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
#include "numeric.h"
#include "common.h"

#ifndef lint
static  char sccsid[] = "@(#)s_err.c	1.12 11/1/93 (C) 1992 Darren Reed";
#endif

ID_CVS("$Id$");

typedef	struct	{
	int	num_val;
	char	*num_form;
} Numeric;

static	char	*prepbuf PROTO((char *, int, char *));
static	char	numbuff[514];
static	char	numbers[] = "0123456789";

static	Numeric	local_replies[] = {
/* 000 */	0, (char *)NULL,
/* 001 */	RPL_WELCOME, ":Welcome to the %s IRC Network %s!%s@%s",
/* 002 */	RPL_YOURHOST, ":Your host is %s, running version %s",
/* 003 */	RPL_CREATED, ":This server was created %s",
/* 004 */       RPL_MYINFO, "%s %s %s %s",
/* 005 */	RPL_PROTOCTL, "%s :are available on this server",
/* 006 */       RPL_MAP, ":%s%-*s\2[Users:%5d]  [%2d%%]\2",
/* 007 */       RPL_MAPEND, ":End of /MAP",
		0, (char *)NULL
};

static	Numeric	numeric_errors[] = {
/* 401 */	ERR_NOSUCHNICK, "%s :No such nick/channel",
/* 402 */	ERR_NOSUCHSERVER, "%s :No such server",
/* 403 */	ERR_NOSUCHCHANNEL, "%s :No such channel",
/* 404 */	ERR_CANNOTSENDTOCHAN, "%s :Cannot send to channel (%s)",
/* 405 */	ERR_TOOMANYCHANNELS, "%s :You have joined too many channels",
/* 406 */	ERR_WASNOSUCHNICK, "%s :There was no such nickname",
/* 407 */	ERR_TOOMANYTARGETS,
		"%s :Duplicate recipients. No message delivered",
/* 408 */	ERR_NOSUCHSERVICE, (char *)NULL,
/* 409 */	ERR_NOORIGIN, ":No origin specified",
		0, (char *)NULL,
/* 411 */	ERR_NORECIPIENT, ":No recipient given (%s)",
/* 412 */	ERR_NOTEXTTOSEND, ":No text to send",
/* 413 */	ERR_NOTOPLEVEL, "%s :No toplevel domain specified",
/* 414 */	ERR_WILDTOPLEVEL, "%s :Wildcard in toplevel Domain",
		0, (char *)NULL, 0, (char *)NULL, 0, (char *)NULL,
		0, (char *)NULL, 0, (char *)NULL, 0, (char *)NULL,
/* 421 */       ERR_UNKNOWNCOMMAND, "%s :Unknown command",
/* 422 */	ERR_NOMOTD, ":MOTD File is missing",
/* 423 */	ERR_NOADMININFO,
		"%s :No administrative info available",
/* 424 */	ERR_FILEERROR, ":File error doing %s on %s",
/* 425*/    ERR_NOOPERMOTD, ":OPERMOTD File is missing",
		0, (char *) NULL, 0, (char *)NULL,
		0, (char *)NULL, 0, (char *)NULL, 0, (char *)NULL,
/* 431 */	ERR_NONICKNAMEGIVEN, ":No nickname given",
/* 432 */	ERR_ERRONEUSNICKNAME, "%s :Erroneous Nickname: %s",
/* 433 */	ERR_NICKNAMEINUSE, "%s :Nickname is already in use.",
/* 434 */       ERR_NORULES, ":RULES File is missing",
/* 435 */	ERR_SERVICECONFUSED, (char *)NULL,
/* 436 */	ERR_NICKCOLLISION, "%s :Nickname collision KILL",
/* 437 */	ERR_BANNICKCHANGE,
		"%s :Cannot change nickname while banned on channel",
/* 438 */	ERR_NCHANGETOOFAST, "%s :Nick change too fast. Please wait %d seconds",
/* 439 */	ERR_TARGETTOOFAST, "%s :Message target change too fast. Please wait %d seconds",
/* 440 */	ERR_SERVICESDOWN, "%s :Services are currently down. Please try again later.",
/* 441 */	ERR_USERNOTINCHANNEL, "%s %s :They aren't on that channel",
/* 442 */	ERR_NOTONCHANNEL, "%s :You're not on that channel",
/* 443 */	ERR_USERONCHANNEL, "%s %s :is already on channel",
/* 444 */	ERR_NOLOGIN, "%s :User not logged in",
#ifndef	ENABLE_SUMMON
/* 445 */	ERR_SUMMONDISABLED, ":SUMMON has been disabled",
#else
		0, (char *)NULL,
#endif
#ifndef	ENABLE_USERS
/* 446 */	ERR_USERSDISABLED, ":USERS has been disabled",
#else
		0, (char *)NULL,
#endif
		0, (char *)NULL, 0, (char *)NULL, 0, (char *)NULL,
		0, (char *)NULL,
/* 451 */	ERR_NOTREGISTERED, ":You have not registered",
		0, (char *)NULL, 0, (char *)NULL, 0, (char *)NULL,
#ifdef HOSTILENAME
/* 455 */	ERR_HOSTILENAME, ":Your username %s contained the invalid "
			"character(s) %s and has been changed to %s. "
			"Please use only the characters 0-9 a-z A-Z _ - "
			"or . in your username. Your username is the part "
			"before the @ in your email address.",
#else
		0, (char *)NULL,
#endif
		0, (char *)NULL, 0, (char *)NULL, 0, (char *)NULL,
/* 459 */       ERR_NOHIDING,	"%s :Cannot join channel (+H)",
/* 460 */   ERR_NOTFORHALFOPS, ":Halfops cannot set mode %c",
/* 461 */	ERR_NEEDMOREPARAMS, "%s :Not enough parameters",
/* 462 */	ERR_ALREADYREGISTRED, ":You may not reregister",
/* 463 */	ERR_NOPERMFORHOST, ":Your host isn't among the privileged",
/* 464 */	ERR_PASSWDMISMATCH, ":Password Incorrect",
/* 465 */	ERR_YOUREBANNEDCREEP, ":You are banned from this server.  Mail %s for more information", 
/* 466 */	ERR_YOUWILLBEBANNED, (char *)NULL, 
/* 467 */	ERR_KEYSET, "%s :Channel key already set",
/* 468 */	ERR_ONLYSERVERSCANCHANGE, "%s :Only servers can change that mode",
/* 469 */	ERR_LINKSET, "%s :Channel link already set",
/* 470 */	ERR_LINKCHANNEL, "[Link] %s has become full, so you are automatically being transferred to the linked channel %s",
/* 471 */	ERR_CHANNELISFULL, "%s :Cannot join channel (+l)",
/* 472 */	ERR_UNKNOWNMODE  , "%c :is unknown mode char to me",
/* 473 */	ERR_INVITEONLYCHAN, "%s :Cannot join channel (+i)",
/* 474 */	ERR_BANNEDFROMCHAN, "%s :Cannot join channel (+b)",
/* 475 */	ERR_BADCHANNELKEY, "%s :Cannot join channel (+k)",
/* 476 */	ERR_BADCHANMASK, "%s :Bad Channel Mask",
/* 477 */	ERR_NEEDREGGEDNICK, "%s :You need a registered nick to join that channel.",
/* 478 */	ERR_BANLISTFULL, "%s %s :Channel ban/ignore list is full",
/* 479 */	ERR_LINKFAIL, "%s :Sorry, the channel has an invalid channel link set.",
/* 480 */	ERR_CANNOTKNOCK, ":Cannot knock on %s (%s)",
/* 481 */	ERR_NOPRIVILEGES, ":Permission Denied- You do not have the correct IRC operator privileges",
/* 482 */	ERR_CHANOPRIVSNEEDED, "%s :You're not channel operator",
/* 483 */	ERR_CANTKILLSERVER, ":You cant kill a server!",
/* 484 */	ERR_ATTACKDENY, "%s :Cannot kick protected user %s.",
/* 485 */	ERR_KILLDENY, ":Cannot kill protected user %s.",
		0, (char *)NULL, 0, (char *)NULL, 0, (char *)NULL,
		0, (char *)NULL, 0, (char *)NULL,
/* 491 */	ERR_NOOPERHOST, ":No O-lines for your host",
/* 492 */	ERR_NOSERVICEHOST, (char *)NULL,
		0, (char *)NULL, 0, (char *)NULL, 0, (char *)NULL,
		0, (char *)NULL, 0, (char *)NULL, 0, (char *)NULL,
		0, (char *)NULL, 0, (char *)NULL,
/* 501 */	ERR_UMODEUNKNOWNFLAG, ":Unknown MODE flag",
/* 502 */	ERR_USERSDONTMATCH, ":Cant change mode for other users",
		0, (char *)NULL, 0, (char *)NULL, 0, (char *)NULL,
		0, (char *)NULL, 0, (char *)NULL, 0, (char *)NULL,
		0, (char *)NULL, 0, (char *)NULL,
/* 511 */	ERR_SILELISTFULL, "%s :Your silence list is full",
/* 512 */	ERR_TOOMANYWATCH, "%s :Maximum size for WATCH-list is 128 entries",
/* 513 */	ERR_NEEDPONG, ":To connect, type /QUOTE PONG %lX",
		0, (char *)NULL, 0, (char *)NULL, 0, (char *)NULL,
		0, (char *)NULL, 
		518, ":Cannot invite (+I) at channel",
        519, ":Cannot join channel (Admin only)",
		520, ":Cannot join channel (IRCops only)",
/* 521 */	ERR_LISTSYNTAX, "Bad list syntax, type /quote list ? or /raw list ?"
};

static	Numeric	numeric_replies[] = {
/* 300 */	RPL_NONE, (char *)NULL,
/* 301 */	RPL_AWAY, "%s :%s",
/* 302 */	RPL_USERHOST, ":",
/* 303 */	RPL_ISON, ":",
/* 304 */	RPL_TEXT, (char *)NULL,
/* 305 */	RPL_UNAWAY, ":You are no longer marked as being away",
/* 306 */	RPL_NOWAWAY, ":You have been marked as being away",
/* 307 */	RPL_WHOISREGNICK, "%s :is a registered nick",
/* 308 */	RPL_RULESSTART, ":- %s Server Rules - ",
/* 309 */       RPL_ENDOFRULES, ":End of RULES command.",
/* 310 */	RPL_WHOISHELPOP, "%s :is available for help.",
/* 311 */	RPL_WHOISUSER, "%s %s %s * :%s",
/* 312 */	RPL_WHOISSERVER, "%s %s :%s",
/* 313 */	RPL_WHOISOPERATOR, "%s :is %s on %s",
/* 314 */	RPL_WHOWASUSER, "%s %s %s * :%s",
/* 315 */	RPL_ENDOFWHO, "%s :End of /WHO list.",
/* 316 */	RPL_WHOISCHANOP, (char *)NULL,
/* 317 */	RPL_WHOISIDLE, "%s %ld %ld :seconds idle, signon time",
/* 318 */	RPL_ENDOFWHOIS, "%s :End of /WHOIS list.",
/* 319 */	RPL_WHOISCHANNELS, "%s :%s",
                RPL_WHOISSPECIAL, "%s :%s",
/* 321 */	RPL_LISTSTART, "Channel :Users  Name",
/* 322 */	RPL_LIST, "%s %d :%s",
/* 323 */	RPL_LISTEND, ":End of /LIST",
/* 324 */	RPL_CHANNELMODEIS, "%s %s %s",
		0, (char *)NULL, 0, (char *)NULL,
		0, (char *)NULL, 0, (char *)NULL,
/* 329 */ RPL_CREATIONTIME, "%s %lu",
		0, (char *)NULL,
/* 331 */	RPL_NOTOPIC, "%s :No topic is set.",
/* 332 */	RPL_TOPIC, "%s :%s",
/* 333 */       RPL_TOPICWHOTIME, "%s %s %lu",
/* 334 */	RPL_LISTSYNTAX, ":%s",
/* 335 */       RPL_WHOISBOT, "%s :is a \2Bot\2 on %s",
		 0, (char *)NULL, 0, (char *)NULL,
                0, (char *)NULL, 0, (char *)NULL, 0, (char *)NULL,
/* 341 */	RPL_INVITING, "%s %s",
/* 342 */	RPL_SUMMONING, "%s :User summoned to irc",
		0, (char *)NULL, 0, (char *)NULL,
		0, (char *)NULL, 
		RPL_INVITELIST, "%s %s",
		RPL_ENDOFINVITELIST, "%s :End of Channel Invite List",
		RPL_EXLIST, "%s %s %s %lu",
		RPL_ENDOFEXLIST, "%s :End of Channel Exception List",
		0, (char *)NULL,
/* 351 */	RPL_VERSION, "%s(%s).%s %s :%s [%s=%li%s]",
/* 352 */	RPL_WHOREPLY, "%s %s %s %s %s %s :%d %s",
/* 353 */	RPL_NAMREPLY, "%s",
		354, ":Reserved for Undernet",
		
		0, (char *)NULL, 0, (char *)NULL,
		0, (char *)NULL, 0, (char *)NULL, 
		0, (char *)NULL, 0, (char *)NULL, 
/* 361 */	RPL_KILLDONE, (char *)NULL,
/* 362 */	RPL_CLOSING, "%s :Closed. Status = %d",
/* 363 */	RPL_CLOSEEND, "%d: Connections Closed",
/* 364 */	RPL_LINKS, "%s %s :%d %s",
/* 365 */	RPL_ENDOFLINKS, "%s :End of /LINKS list.",
/* 366 */	RPL_ENDOFNAMES, "%s :End of /NAMES list.",
/* 367 */	RPL_BANLIST, "%s %s %s %lu",
/* 368 */	RPL_ENDOFBANLIST, "%s :End of Channel Ban List",
/* 369 */	RPL_ENDOFWHOWAS, "%s :End of WHOWAS",
		0, (char *)NULL,
/* 371 */	RPL_INFO, ":%s",
/* 372 */	RPL_MOTD, ":- %s",
/* 373 */	RPL_INFOSTART, ":Server INFO",
/* 374 */	RPL_ENDOFINFO, ":End of /INFO list.",
/* 375 */	RPL_MOTDSTART, ":- %s Message of the Day - ",
/* 376 */	RPL_ENDOFMOTD, ":End of /MOTD command.",
		0, (char *)NULL,
/* 378 */       RPL_WHOISHOST, "%s :is connecting from *@%s",
/* 379 */       RPL_WHOISMODES, "%s is using modes %s",
		0, (char *)NULL,
/* 381 */	RPL_YOUREOPER, ":You are now an IRC Operator",
/* 382 */	RPL_REHASHING, "%s :Rehashing",
/* 383 */	RPL_YOURESERVICE, (char *)NULL,
/* 384 */	RPL_MYPORTIS, "%d :Port to local server is\r\n",
/* 385 */	RPL_NOTOPERANYMORE, (char *)NULL,
		0, (char *)NULL, 0, (char *)NULL, 0, (char *)NULL,
		0, (char *)NULL, 0, (char *)NULL,
/* 391 */	RPL_TIME, "%s :%s",
#ifdef	ENABLE_USERS
/* 392 */	RPL_USERSSTART, ":UserID   Terminal  Host",
/* 393 */	RPL_USERS, ":%-8s %-9s %-8s",
/* 394 */	RPL_ENDOFUSERS, ":End of Users",
/* 395 */	RPL_NOUSERS, ":Nobody logged in.",
#else
		0, (char *)NULL, 0, (char *)NULL, 0, (char *)NULL,
		0, (char *)NULL,
#endif
		0, (char *)NULL, 0, (char *)NULL, 0, (char *)NULL,
		0, (char *)NULL,
/* 200 */	RPL_TRACELINK, "Link %s%s %s %s",
/* 201 */	RPL_TRACECONNECTING, "Attempt %d %s",
/* 202 */	RPL_TRACEHANDSHAKE, "Handshaking %d %s",
/* 203 */	RPL_TRACEUNKNOWN, "???? %d %s",
/* 204 */	RPL_TRACEOPERATOR, "Operator %d %s [%s] %ld",
/* 205 */	RPL_TRACEUSER, "User %d %s [%s] %ld",
/* 206 */	RPL_TRACESERVER, "Server %d %dS %dC %s %s!%s@%s %ld",
/* 207 */	RPL_TRACESERVICE, "Service %d %s",
/* 208 */	RPL_TRACENEWTYPE, "<newtype> 0 %s",
/* 209 */	RPL_TRACECLASS, "Class %d %d",
		0, (char *)NULL,
/* 211 */	RPL_STATSLINKINFO, (char *)NULL,
#ifdef DEBUGMODE
/* 212 */	RPL_STATSCOMMANDS, "%s %u %u %u %u %u %u",
#else
/* 212 */	RPL_STATSCOMMANDS, "%s %u %u",
#endif
/* 213 */	RPL_STATSCLINE, "%c %s * %s %d %d",
/* 214 */	RPL_STATSNLINE, "%c %s * %s %d %d",
/* 215 */	RPL_STATSILINE, "%c %s * %s %d %d",
/* 216 */	RPL_STATSKLINE, "%c %s %s %s %d %d",
/* 217 */	RPL_STATSQLINE, "%c %s %s %s %d %d",
/* 218 */	RPL_STATSYLINE, "%c %d %d %d %d %ld",
/* 219 */	RPL_ENDOFSTATS, "%c :End of /STATS report",
/* 220 */	RPL_STATSBLINE, "%c %s %s %s %d %d",
/* 221 */	RPL_UMODEIS, "%s",
/* 222 */	RPL_SQLINE_NICK, "%s :%s",
/* 223 */	RPL_STATSGLINE, "%c %s@%s %li %li %s :%s",
/* 224 */	RPL_STATSTLINE, "T %s %s %s",
		0, (char *)NULL, 0, (char *)NULL, 0, (char *)NULL,
		0, (char *)NULL, 0, (char *)NULL, 0, (char *)NULL,
/* 231 */	RPL_SERVICEINFO, (char *)NULL,
/* 232 */	RPL_RULES, ":- %s",
/* 233 */	RPL_SERVICE, (char *)NULL,
/* 234 */	RPL_SERVLIST, (char *)NULL,
/* 235 */	RPL_SERVLISTEND, (char *)NULL,
		0, (char *)NULL, 0, (char *)NULL, 0, (char *)NULL,
		0, (char *)NULL, 0, (char *)NULL,
/* 241 */	RPL_STATSLLINE, "%c %s * %s %d %d",
/* 242 */	RPL_STATSUPTIME, ":Server Up %d days, %d:%02d:%02d",
/* 243 */	RPL_STATSOLINE, "%c %s * %s %s %d",
/* 244 */	RPL_STATSHLINE, "%c %s * %s %d %d", 
/* 245 */	RPL_STATSSLINE, "%c %s * %s %d %d", 
		0, (char *)NULL, 
/* 247 */	RPL_STATSXLINE, "X %s %d", 
/* 248 */	RPL_STATSULINE, "%c %s * %s %d %d", 
		0, (char *)NULL, 
/* 250 */       RPL_STATSCONN,
                ":Highest connection count: %d (%d clients)",
/* 251 */	RPL_LUSERCLIENT,
		":There are %d users and %d invisible on %d servers",
/* 252 */	RPL_LUSEROP, "%d :operator(s) online",
/* 253 */	RPL_LUSERUNKNOWN, "%d :unknown connection(s)",
/* 254 */	RPL_LUSERCHANNELS, "%d :channels formed",
/* 255 */	RPL_LUSERME, ":I have %d clients and %d servers",
/* 256 */	RPL_ADMINME, ":Administrative info about %s",
/* 257 */	RPL_ADMINLOC1, ":%s",
/* 258 */	RPL_ADMINLOC2, ":%s",
/* 259 */	RPL_ADMINEMAIL, ":%s",
		0, (char *)NULL,
/* 261 */	RPL_TRACELOG, "File %s %d",
		0, (char *)NULL, 0, (char *)NULL, 0, (char *)NULL,
/* 265 */	RPL_LOCALUSERS, ":Current Local Users: %d  Max: %d",
/* 266 */	RPL_GLOBALUSERS, ":Current Global Users: %d  Max: %d",
		0, (char *)NULL,
		0, (char *)NULL, 0, (char *)NULL, 0, (char *)NULL,
/* 271 */	RPL_SILELIST, "%s %s",
/* 272 */	RPL_ENDOFSILELIST, ":End of Silence List",
		0, (char *)NULL, 0, (char *)NULL,
/* 275 */	RPL_STATSDLINE, "%c %s %s",
		0, (char *)NULL, 0, (char *)NULL, 0, (char *)NULL,
		0, (char *)NULL, 0, (char *)NULL, 0, (char *)NULL,
		0, (char *)NULL, 0, (char *)NULL, 0, (char *)NULL,
		0, (char *)NULL, 0, (char *)NULL, 0, (char *)NULL,
		0, (char *)NULL, 0, (char *)NULL, 0, (char *)NULL,
		0, (char *)NULL, 0, (char *)NULL, 0, (char *)NULL,
/* 294 */	RPL_HELPFWD, ":Your help-request has been forwarded to Help Operators",
/* 295 */	RPL_HELPIGN, ":Your address has been ignored from forwarding"
};

/*
 * NOTE: Unlike the others, this one goes strait through, 600-799
 */
static	Numeric	numeric_replies2[] = {
/* 600 */	RPL_LOGON, "%s %s %s %d :logged online",
/* 601 */	RPL_LOGOFF, "%s %s %s %d :logged offline",
/* 602 */	RPL_WATCHOFF, "%s %s %s %d :stopped watching",
/* 603 */	RPL_WATCHSTAT, ":You have %d and are on %d WATCH entries",
/* 604 */	RPL_NOWON, "%s %s %s %d :is online",
/* 605 */	RPL_NOWOFF, "%s %s %s %d :is offline",
/* 606 */	RPL_WATCHLIST, ":%s",
/* 607 */	RPL_ENDOFWATCHLIST, ":End of WATCH %c",

/* 610 */       RPL_MAPMORE, ":%s%-*s --> *more*",
/* 611 */		0, (char *)NULL,
/* 612 */		0, (char *)NULL,
/* 613 */		0, (char *)NULL,
/* 614 */		0, (char *)NULL,
/* 615 */		0, (char *)NULL,
/* 616 */		0, (char *)NULL,
/* 617 */		0, (char *)NULL,
/* 618 */		0, (char *)NULL,
/* 619 */		0, (char *)NULL,
/* 620 */		0,(char *) NULL,
/* 621 */		0,(char *) NULL,
/* 622 */		0,(char *) NULL,
			0,(char *) NULL,
			0,(char *) NULL,
			0,(char *) NULL,
			0,(char *) NULL,
			0,(char *) NULL,
			0,(char *) NULL,
			0,(char *) NULL,
			0,(char *) NULL,
			0,(char *) NULL,
			0,(char *) NULL,
			0,(char *) NULL,
			0,(char *) NULL,
			0,(char *) NULL,
			0,(char *) NULL,
			0,(char *) NULL,
			0,(char *) NULL,
			0,(char *) NULL,
/* 640 */		RPL_DUMPING, ":Dumping clients matching %s",
/* 641 */		RPL_DUMPRPL, "%s %s %s %s %s%s%s%s",
/* 642 */		RPL_EODUMP,	 ":End of /dusers - %i",
		0, (char *) NULL,
		0, (char *)NULL
};

char	*err_str(numeric)
int	numeric;
{
	Reg1	Numeric	*nptr;
	Reg2	int	num = numeric;

	num -= numeric_errors[0].num_val;
	if (num < 0 || num > ERR_NEEDPONG)
		(void)sprintf(numbuff,
			":%%s %d %%s :INTERNAL ERROR: BAD NUMERIC! %d",
			numeric, num);
	else
	    {
		nptr = &numeric_errors[num];
		if (!nptr->num_form || !nptr->num_val)
			(void)sprintf(numbuff,
				":%%s %d %%s :NO ERROR FOR NUMERIC ERROR %d",
				numeric, num);
		else
			(void)prepbuf(numbuff, nptr->num_val, nptr->num_form);
	    }
	return numbuff;
}


char	*rpl_str(numeric)
int	numeric;
{
	Reg1	Numeric	*nptr;
	Reg2	int	num = numeric;

	if (num > 99)
		num -= (num > 300) ? 300 : 100;

	if ((num < 0 || num > 200) && (num < 300 || num > 499))
		(void)sprintf(numbuff,
			":%%s %d %%s :INTERNAL REPLY ERROR: BAD NUMERIC! %d",
			numeric, num);
	else
	    {
		if (numeric > 599) {
			num -= 300;
			nptr = &numeric_replies2[num];
		}
		else if (numeric > 99)
			nptr = &numeric_replies[num];
		else
			nptr = &local_replies[num];
		Debug((DEBUG_NUM, "rpl_str: numeric %d num %d nptr %x %d %x",
			numeric, num, nptr, nptr->num_val, nptr->num_form));
		if (!nptr->num_form || !nptr->num_val)
			(void)sprintf(numbuff,
				":%%s %d %%s :NO REPLY FOR NUMERIC ERROR %d",
				numeric, num);
		else
			(void)prepbuf(numbuff, nptr->num_val, nptr->num_form);
	    }
	return numbuff;
}

static	char	*prepbuf(buffer, num, tail)
char	*buffer;
Reg1	int	num;
char	*tail;
{
	Reg1	char	*s;

	(void)strcpy(buffer, ":%s ");
	s = buffer + 4;

	*s++ = numbers[num/100];
	num %= 100;
	*s++ = numbers[num/10];
	*s++ = numbers[num%10];
	(void)strcpy(s, " %s ");
	(void)strcpy(s+4, tail);
	return buffer;
}

/* this was the old RPL_MYINFO line .... 
   RPL_MYINFO, "%s %s oiwsghOkcSNfraAbexTCWq biklmnopstvRzqxOAqa", */
