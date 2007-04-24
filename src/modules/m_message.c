/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_message.c
 *   (C) 2000-2001 Carsten V. Munk and the UnrealIRCd Team
 *   Moved to modules by Fish (Justin Hammond)
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

#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"
#include "proto.h"
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif
#include "badwords.h"

int _is_silenced(aClient *, aClient *);
char *_stripbadwords(char *str, ConfigItem_badword *start_bw, int *blocked);
char *stripbadwords_message(char *str, int *blocked);
char *_StripColors(unsigned char *text);
char *_StripControlCodes(unsigned char *text);

DLLFUNC int m_message(aClient *cptr, aClient *sptr, int parc, char *parv[], int notice);
DLLFUNC int  m_notice(aClient *cptr, aClient *sptr, int parc, char *parv[]);
DLLFUNC int  m_private(aClient *cptr, aClient *sptr, int parc, char *parv[]);

extern int webtv_parse(aClient *sptr, char *string);

/* Place includes here */
#define MSG_PRIVATE     "PRIVMSG"       /* PRIV */
#define TOK_PRIVATE     "!"     /* 33 */
#define MSG_NOTICE      "NOTICE"        /* NOTI */
#define TOK_NOTICE      "B"     /* 66 */

ModuleHeader MOD_HEADER(m_message)
  = {
	"message",	/* Name of module */
	"$Id$", /* Version */
	"private message and notice", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_TEST(m_message)(ModuleInfo *modinfo)
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAddPChar(modinfo->handle, EFUNC_STRIPBADWORDS, _stripbadwords);
	EfunctionAddPChar(modinfo->handle, EFUNC_STRIPCOLORS, _StripColors);
	EfunctionAddPChar(modinfo->handle, EFUNC_STRIPCONTROLCODES, _StripControlCodes);
	EfunctionAdd(modinfo->handle, EFUNC_IS_SILENCED, _is_silenced);
	return MOD_SUCCESS;
}

/* This is called on module init, before Server Ready */
DLLFUNC int MOD_INIT(m_message)(ModuleInfo *modinfo)
{
	/*
	 * We call our add_Command crap here
	*/
	add_CommandX(MSG_PRIVATE, TOK_PRIVATE, m_private, 2, M_USER|M_SERVER|M_RESETIDLE|M_VIRUS);
	add_Command(MSG_NOTICE, TOK_NOTICE, m_notice, 2);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
	
}

/* Is first run when server is 100% ready */
DLLFUNC int MOD_LOAD(m_message)(int module_load)
{
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
DLLFUNC int MOD_UNLOAD(m_message)(int module_unload)
{
	if (del_Command(MSG_PRIVATE, TOK_PRIVATE, m_private) < 0)
	{
		sendto_realops("Failed to delete command privmsg when unloading %s",
				MOD_HEADER(m_message).name);
	}
	if (del_Command(MSG_NOTICE, TOK_NOTICE, m_notice) < 0)
	{
		sendto_realops("Failed to delete command notice when unloading %s",
				MOD_HEADER(m_message).name);
	}
	return MOD_SUCCESS;
}

static int check_dcc(aClient *sptr, char *target, aClient *targetcli, char *text);
static int check_dcc_soft(aClient *from, aClient *to, char *text);

#define CANPRIVMSG_CONTINUE		100
#define CANPRIVMSG_SEND			101
/** Check if PRIVMSG's are permitted from a person to another person.
 * cptr:	..
 * sptr:	..
 * acptr:	target client
 * notice:	1 if notice, 0 if privmsg
 * text:	Pointer to a pointer to a text [in, out]
 * cmd:		Pointer to a pointer which contains the command to use [in, out]
 *
 * RETURN VALUES:
 * CANPRIVMSG_CONTINUE: issue a 'continue' in target nickname list (aka: skip further processing this target)
 * CANPRIVMSG_SEND: send the message (use text/newcmd!)
 * Other: return with this value (can be anything like 0, -1, FLUSH_BUFFER, etc)
 */
static int can_privmsg(aClient *cptr, aClient *sptr, aClient *acptr, int notice, char **text, char **cmd)
{
char *ctcp;
int ret;

	if (IsVirus(sptr))
	{
		sendnotice(sptr, "You are only allowed to talk in '%s'", SPAMFILTER_VIRUSCHAN);
		return CANPRIVMSG_CONTINUE;
	}
	/* Umode +R (idea from Bahamut) */
	if (IsRegNickMsg(acptr) && !IsRegNick(sptr) && !IsULine(sptr) && !IsOper(sptr) && !IsServer(sptr)) {
		sendto_one(sptr, err_str(ERR_NONONREG), me.name, sptr->name,
			acptr->name);
		return 0;
	}
	if (IsNoCTCP(acptr) && !IsOper(sptr) && **text == 1 && acptr != sptr)
	{
		ctcp = *text + 1; /* &*text[1]; */
		if (myncmp(ctcp, "ACTION ", 7) && myncmp(ctcp, "DCC ", 4))
		{
			sendto_one(sptr, err_str(ERR_NOCTCP), me.name, sptr->name, acptr->name);
			return 0;
		}
	}

	if (MyClient(sptr) && !strncasecmp(*text, "\001DCC", 4))
	{
		ret = check_dcc(sptr, acptr->name, acptr, *text);
		if (ret < 0)
			return ret;
		if (ret == 0)
			return CANPRIVMSG_CONTINUE;
	}
	if (MyClient(acptr) && !strncasecmp(*text, "\001DCC", 4) &&
	    !check_dcc_soft(sptr, acptr, *text))
		return CANPRIVMSG_CONTINUE;

	if (MyClient(sptr) && check_for_target_limit(sptr, acptr, acptr->name))
		return CANPRIVMSG_CONTINUE;

	if (!is_silenced(sptr, acptr))
	{
#ifdef STRIPBADWORDS
		int blocked = 0;
#endif
		Hook *tmphook;
		
		if (notice && IsWebTV(acptr) && **text != '\1')
			*cmd = MSG_PRIVATE;
		if (!notice && MyConnect(sptr) &&
		    acptr->user && acptr->user->away)
			sendto_one(sptr, rpl_str(RPL_AWAY),
			    me.name, sptr->name, acptr->name,
			    acptr->user->away);

#ifdef STRIPBADWORDS
		if (MyClient(sptr) && !IsULine(acptr) && IsFilteringWords(acptr))
		{
			*text = stripbadwords_message(*text, &blocked);
			if (blocked)
			{
				if (!notice && MyClient(sptr))
					sendto_one(sptr, rpl_str(ERR_NOSWEAR),
						me.name, sptr->name, acptr->name);
				return CANPRIVMSG_CONTINUE;
			}
		}
#endif

		if (MyClient(sptr))
		{
			ret = dospamfilter(sptr, *text, (notice ? SPAMF_USERNOTICE : SPAMF_USERMSG), acptr->name, 0, NULL);
			if (ret < 0)
				return ret;
		}

		for (tmphook = Hooks[HOOKTYPE_USERMSG]; tmphook; tmphook = tmphook->next) {
			*text = (*(tmphook->func.pcharfunc))(cptr, sptr, acptr, *text, notice);
			if (!*text)
				break;
		}
		if (!*text)
			return CANPRIVMSG_CONTINUE;
		
		return CANPRIVMSG_SEND;
	} else {
		/* Silenced */
		RunHook4(HOOKTYPE_SILENCED, cptr, sptr, acptr, notice);
	}
	return CANPRIVMSG_CONTINUE;
}

/*
** m_message (used in m_private() and m_notice())
** the general function to deliver MSG's between users/channels
**
**	parv[0] = sender prefix
**	parv[1] = receiver list
**	parv[2] = message text
**
** massive cleanup
** rev argv 6/91
**
*/
static int recursive_webtv = 0;
DLLFUNC int m_message(aClient *cptr, aClient *sptr, int parc, char *parv[], int notice)
{
	aClient *acptr, *srvptr;
	char *s;
	aChannel *chptr;
	char *nick, *server, *p, *cmd, *ctcp, *p2, *pc, *text, *newcmd;
	int  cansend = 0;
	int  prefix = 0;
	char pfixchan[CHANNELLEN + 4];
	int ret;

	/*
	 * Reasons why someone can't send to a channel
	 */
	static char *err_cantsend[] = {
		"You need voice (+v)",
		"No external channel messages",
		"Color is not permitted in this channel",
		"You are banned",
		"CTCPs are not permitted in this channel",
		"You must have a registered nick (+r) to talk on this channel",
		"Swearing is not permitted in this channel",
		"NOTICEs are not permitted in this channel",
		NULL
	};

	if (IsHandshake(sptr))
		return 0;

	cmd = notice ? MSG_NOTICE : MSG_PRIVATE;
	if (parc < 2 || *parv[1] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NORECIPIENT),
		    me.name, parv[0], cmd);
		return -1;
	}

	if (parc < 3 || *parv[2] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NOTEXTTOSEND), me.name, parv[0]);
		return -1;
	}

	if (MyConnect(sptr))
		parv[1] = (char *)canonize(parv[1]);
		
	for (p = NULL, nick = strtoken(&p, parv[1], ","); nick;
	    nick = strtoken(&p, NULL, ","))
	{
		if (IsVirus(sptr) && (!strcasecmp(nick, "ircd") || !strcasecmp(nick, "irc")))
		{
			sendnotice(sptr, "IRC command(s) unavailable because you are suspected to have a virus");
			continue;
		}
		/*
		   ** nickname addressed?
		 */
		if (!strcasecmp(nick, "ircd") && MyClient(sptr))
		{
			ret = 0;
			if (!recursive_webtv)
			{
				recursive_webtv = 1;
				ret = parse(sptr, parv[2], (parv[2] + strlen(parv[2])));
				recursive_webtv = 0;
			}
			return ret;
		}
		if (!strcasecmp(nick, "irc") && MyClient(sptr))
		{
			if (!recursive_webtv)
			{
				recursive_webtv = 1;
				ret = webtv_parse(sptr, parv[2]);
				if (ret == -99)
				{
					ret = parse(sptr, parv[2], (parv[2] + strlen(parv[2])));
				}
				recursive_webtv = 0;
				return ret;
			}
		}
		
		if (*nick != '#' && (acptr = find_person(nick, NULL)))
		{
			text = parv[2];
			newcmd = cmd;
			ret = can_privmsg(cptr, sptr, acptr, notice, &text, &newcmd);
			if (ret == CANPRIVMSG_SEND)
			{
				sendto_message_one(acptr, sptr, parv[0], newcmd, nick, text);
				continue;
			} else
			if (ret == CANPRIVMSG_CONTINUE)
				continue;
			else
				return ret;
		}

		p2 = (char *)strchr(nick, '#');
		prefix = 0;
		if (p2 && (chptr = find_channel(p2, NullChn)))
		{
			if (p2 != nick)
			{
				for (pc = nick; pc != p2; pc++)
				{
#ifdef PREFIX_AQ
 #define PREFIX_REST (PREFIX_ADMIN|PREFIX_OWNER)
#else
 #define PREFIX_REST (0)
#endif
					switch (*pc)
					{
					  case '+':
						  prefix |= PREFIX_VOICE | PREFIX_HALFOP | PREFIX_OP | PREFIX_REST;
						  break;
					  case '%':
						  prefix |= PREFIX_HALFOP | PREFIX_OP | PREFIX_REST;
						  break;
					  case '@':
						  prefix |= PREFIX_OP | PREFIX_REST;
						  break;
#ifdef PREFIX_AQ
					  case '&':
						  prefix |= PREFIX_ADMIN | PREFIX_OWNER;
					  	  break;
					  case '~':
						  prefix |= PREFIX_OWNER;
						  break;
#else
					  case '&':
						  prefix |= PREFIX_OP | PREFIX_REST;
					  	  break;
					  case '~':
						  prefix |= PREFIX_OP | PREFIX_REST;
						  break;
#endif
					  default:
						  break;	/* ignore it :P */
					}
				}
				
				if (prefix)
				{
					if (MyClient(sptr) && !op_can_override(sptr))
					{
						Membership *lp = find_membership_link(sptr->user->channel, chptr);
						/* Check if user is allowed to send. RULES:
						 * Need at least voice (+) in order to send to +,% or @
						 * Need at least ops (@) in order to send to & or ~
						 */
						if (!lp || !(lp->flags & (CHFL_VOICE|CHFL_HALFOP|CHFL_CHANOP|CHFL_CHANOWNER|CHFL_CHANPROT)))
						{
							sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
								me.name, sptr->name, chptr->chname);
							return 0;
						}
						if (!(prefix & PREFIX_OP) && ((prefix & PREFIX_OWNER) || (prefix & PREFIX_ADMIN)) &&
						    !(lp->flags & (CHFL_CHANOP|CHFL_CHANOWNER|CHFL_CHANPROT)))
						{
							sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
								me.name, sptr->name, chptr->chname);
							return 0;
						}
					}
					/* Now find out the lowest prefix and use that.. (so @&~#chan becomes @#chan) */
					if (prefix & PREFIX_VOICE)
						pfixchan[0] = '+';
					else if (prefix & PREFIX_HALFOP)
						pfixchan[0] = '%';
					else if (prefix & PREFIX_OP)
						pfixchan[0] = '@';
#ifdef PREFIX_AQ
					else if (prefix & PREFIX_ADMIN)
						pfixchan[0] = '&';
					else if (prefix & PREFIX_OWNER)
						pfixchan[0] = '~';
#endif
					else
						abort();
					strlcpy(pfixchan+1, p2, sizeof(pfixchan)-1);
					nick = pfixchan;
				} else {
					strlcpy(pfixchan, p2, sizeof(pfixchan));
					nick = pfixchan;
				}
			}
			
			if (MyClient(sptr) && (*parv[2] == 1))
			{
				ret = check_dcc(sptr, chptr->chname, NULL, parv[2]);
				if (ret < 0)
					return ret;
				if (ret == 0)
					continue;
			}
			if (IsVirus(sptr) && strcasecmp(chptr->chname, SPAMFILTER_VIRUSCHAN))
			{
				sendnotice(sptr, "You are only allowed to talk in '%s'", SPAMFILTER_VIRUSCHAN);
				continue;
			}
			
			cansend =
			    !IsULine(sptr) ? can_send(sptr, chptr, parv[2], notice) : 0;
			if (!cansend)
			{
				Hook *tmphook;

				sendanyways = (strchr(CHANCMDPFX,parv[2][0]) ? 1 : 0);
				text = parv[2];
				if (MyClient(sptr) && (chptr->mode.mode & MODE_STRIP))
					text = StripColors(parv[2]);

				if (MyClient(sptr))
				{
					ret = dospamfilter(sptr, text, notice ? SPAMF_CHANNOTICE : SPAMF_CHANMSG, chptr->chname, 0, NULL);
					if (ret < 0)
						return ret;
				}

				for (tmphook = Hooks[HOOKTYPE_PRE_CHANMSG]; tmphook; tmphook = tmphook->next) {
					text = (*(tmphook->func.pcharfunc))(sptr, chptr, text, notice);
					if (!text)
						break;
				}
				
				if (!text)
					continue;

				sendto_channelprefix_butone_tok(cptr,
				    sptr, chptr,
				    prefix,
				    notice ? MSG_NOTICE : MSG_PRIVATE,
				    notice ? TOK_NOTICE : TOK_PRIVATE,
				    nick, text, 1);

				RunHook4(HOOKTYPE_CHANMSG, sptr, chptr, text, notice);
				sendanyways = 0;
				continue;
			}
			else
			if (MyClient(sptr))
			{
				if (!notice || (cansend == 8)) /* privmsg or 'cannot send notice'... */
					sendto_one(sptr, err_str(ERR_CANNOTSENDTOCHAN),
					    me.name, parv[0], chptr->chname,
					    err_cantsend[cansend - 1], p2);
			}
			continue;
		}
		else if (p2)
		{
			sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name,
			    parv[0], p2);
			continue;
		}


		/*
		   ** the following two cases allow masks in NOTICEs
		   ** (for OPERs only)
		   **
		   ** Armin, 8Jun90 (gruner@informatik.tu-muenchen.de)
		 */
		if ((*nick == '$' || *nick == '#') && (IsAnOper(sptr)
		    || IsULine(sptr)))
		{
			sendto_match_butone(IsServer(cptr) ? cptr : NULL,
			    sptr, nick + 1,
			    (*nick == '#') ? MATCH_HOST :
			    MATCH_SERVER,
			    ":%s %s %s :%s", parv[0], cmd, nick, parv[2]);
			continue;
		}

		/*
		   ** user[%host]@server addressed?
		 */
		server = index(nick, '@');
		if (server)	/* meaning there is a @ */
		{
			/* There is always a \0 if its a string */
			if (*(server + 1) != '\0')
			{
				char fulltarget[NICKLEN + HOSTLEN + 1];
				
				strlcpy(fulltarget, nick, sizeof(fulltarget)); /* lame.. I know.. */
				
				srvptr = find_server_quick(server + 1);
				if (srvptr)
				{
					acptr = find_nickserv(nick, NULL);
					if (acptr && (acptr->srvptr == srvptr))
					{
						text = parv[2];
						newcmd = cmd;
						ret = can_privmsg(cptr, sptr, acptr, notice, &text, &newcmd);
						if (ret == CANPRIVMSG_CONTINUE)
							continue;
						else if (ret != CANPRIVMSG_SEND)
							return ret;
						/* If we end up here, we have to actually send it... */

						if (IsMe(acptr))
							sendto_prefix_one(acptr, sptr, ":%s %s %s :%s", sptr->name, newcmd, acptr->name, text);
						else
							sendto_message_one(acptr, sptr, sptr->name, newcmd, fulltarget, text);
						continue;
					}
				}
				/* NICK@SERVER NOT FOUND: */
				if (server && strncasecmp(server + 1, SERVICES_NAME, strlen(SERVICES_NAME)) == 0)
					sendto_one(sptr, err_str(ERR_SERVICESDOWN), me.name, parv[0], nick);
				else
					sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, parv[0], fulltarget);

				continue;
			}

		}
		/* nothing, nada, not anything found */
		sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, parv[0],
		    nick);
		continue;
	}
	return 0;
}

/*
** m_private
**	parv[0] = sender prefix
**	parv[1] = receiver list
**	parv[2] = message text
*/

DLLFUNC int  m_private(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	return m_message(cptr, sptr, parc, parv, 0);
}

/*
** m_notice
**	parv[0] = sender prefix
**	parv[1] = receiver list
**	parv[2] = notice text
*/

DLLFUNC int  m_notice(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	return m_message(cptr, sptr, parc, parv, 1);
}
/***********************************************************************
 * m_silence() - Added 19 May 1994 by Run.
 *
 ***********************************************************************/

/*
 * is_silenced : Does the actual check wether sptr is allowed
 *               to send a message to acptr.
 *               Both must be registered persons.
 * If sptr is silenced by acptr, his message should not be propagated,
 * but more over, if this is detected on a server not local to sptr
 * the SILENCE mask is sent upstream.
 */
int _is_silenced(aClient *sptr, aClient *acptr)
{
	Link *lp;
	anUser *user;
	static char sender[HOSTLEN + NICKLEN + USERLEN + 5];
	static char senderx[HOSTLEN + NICKLEN + USERLEN + 5];
	char checkv = 0;
	
	if (!(acptr->user) || !(lp = acptr->user->silence) ||
	    !(user = sptr->user)) return 0;

	ircsprintf(sender, "%s!%s@%s", sptr->name, user->username,
	    user->realhost);
	/* We also check for matches against sptr->user->virthost if present,
	 * this is checked regardless of mode +x so you can't do tricks like:
	 * evil has +x and msgs, victim places silence on +x host, evil does -x
	 * and can msg again. -- Syzop
	 */
	if (sptr->user->virthost)
	{
		ircsprintf(senderx, "%s!%s@%s", sptr->name, user->username,
		    sptr->user->virthost);
		checkv = 1;
	}

	for (; lp; lp = lp->next)
	{
		if (!match(lp->value.cp, sender) || (checkv && !match(lp->value.cp, senderx)))
		{
			if (!MyConnect(sptr))
			{
				sendto_one(sptr->from, ":%s SILENCE %s :%s",
				    acptr->name, sptr->name, lp->value.cp);
				lp->flags = 1;
			}
			return 1;
		}
	}
	return 0;
}

/** Make a viewable dcc filename.
 * This is to protect a bit against tricks like 'flood-it-off-the-buffer'
 * and color 1,1 etc...
 */
char *dcc_displayfile(char *f)
{
static char buf[512];
char *i, *o = buf;
size_t n = strlen(f);

	if (n < 300)
	{
		for (i = f; *i; i++)
			if (*i < 32)
				*o++ = '?';
			else
				*o++ = *i;
		*o = '\0';
		return buf;
	}

	/* Else, we show it as: [first 256 chars]+"[..TRUNCATED..]"+[last 20 chars] */
	for (i = f; i < f+256; i++)
		if (*i < 32)
			*o++ = '?';
		else
			*o++ = *i;
	strcpy(o, "[..TRUNCATED..]");
	o += sizeof("[..TRUNCATED..]");
	for (i = f+n-20; *i; i++)
		if (*i < 32)
			*o++ = '?';
		else
			*o++ = *i;
	*o = '\0';
	return buf;
	
}

/** Checks if a DCC is allowed.
 * PARAMETERS:
 * sptr:		the client to check for
 * target:		the target (eg a user or a channel)
 * targetcli:	the target client, NULL in case of a channel
 * text:		the whole msg
 * RETURNS:
 * 1:			allowed (no dcc, etc)
 * 0:			block
 * <0:			immediately return with this value (could be FLUSH_BUFFER)
 * HISTORY:
 * F:Line stuff by _Jozeph_ added by Stskeeps with comments.
 * moved and various improvements by Syzop.
 */
static int check_dcc(aClient *sptr, char *target, aClient *targetcli, char *text)
{
char *ctcp;
ConfigItem_deny_dcc *fl;
char *end, realfile[BUFSIZE];
int size_string, ret;

	if ((*text != 1) || !MyClient(sptr) || IsOper(sptr) || (targetcli && IsAnOper(targetcli)))
		return 1;

	ctcp = &text[1];
	/* Most likely a DCC send .. */
	if (!myncmp(ctcp, "DCC SEND ", 9))
		ctcp = text + 10;
	else if (!myncmp(ctcp, "DCC RESUME ", 11))
		ctcp = text + 12;
	else
		return 1; /* something else, allow */

	if (sptr->flags & FLAGS_DCCBLOCK)
	{
		sendto_one(sptr, ":%s NOTICE %s :*** You are blocked from sending files as you have tried to "
		                 "send a forbidden file - reconnect to regain ability to send",
			me.name, sptr->name);
		return 0;
	}
	for (; (*ctcp == ' '); ctcp++); /* skip leading spaces */

	if (*ctcp == '"' && *(ctcp+1))
		end = index(ctcp+1, '"');
	else
		end = index(ctcp, ' ');

	/* check if it was fake.. just pass it along then .. */
	if (!end || (end < ctcp))
		return 1; /* allow */

	size_string = (int)(end - ctcp);

	if (!size_string || (size_string > (BUFSIZE - 1)))
		return 1; /* allow */

	strlcpy(realfile, ctcp, size_string+1);

	if ((ret = dospamfilter(sptr, realfile, SPAMF_DCC, target, 0, NULL)) < 0)
		return ret;

	if ((fl = dcc_isforbidden(sptr, realfile)))
	{
		char *displayfile = dcc_displayfile(realfile);
		sendto_one(sptr,
		    ":%s %d %s :*** Cannot DCC SEND file %s to %s (%s)",
		    me.name, RPL_TEXT,
		    sptr->name, displayfile, target, fl->reason);
		sendto_one(sptr, ":%s NOTICE %s :*** You have been blocked from sending files, reconnect to regain permission to send files",
			me.name, sptr->name);

		sendto_umode(UMODE_VICTIM,
		    "%s tried to send forbidden file %s (%s) to %s (is blocked now)",
		    sptr->name, displayfile, fl->reason, target);
		sendto_serv_butone_token(NULL, me.name, MSG_SMO, TOK_SMO, "v :%s tried to send forbidden file %s (%s) to %s (is blocked now)",
			sptr->name, displayfile, fl->reason, target);
		sptr->flags |= FLAGS_DCCBLOCK;
		return 0; /* block */
	}

	/* Channel dcc (???) and discouraged? just block */
	if (!targetcli && ((fl = dcc_isdiscouraged(sptr, realfile))))
	{
		char *displayfile = dcc_displayfile(realfile);
		sendto_one(sptr,
		    ":%s %d %s :*** Cannot DCC SEND file %s to %s (%s)",
		    me.name, RPL_TEXT,
		    sptr->name, displayfile, target, fl->reason);
		return 0; /* block */
	}
	return 1; /* allowed */
}

/** Checks if a DCC is allowed by DCCALLOW rules (only SOFT bans are checked).
 * PARAMETERS:
 * from:		the sender client (possibly remote)
 * to:			the target client (always local)
 * text:		the whole msg
 * RETURNS:
 * 1:			allowed
 * 0:			block
 */
static int check_dcc_soft(aClient *from, aClient *to, char *text)
{
char *ctcp;
ConfigItem_deny_dcc *fl;
char *end, realfile[BUFSIZE];
int size_string;

	if ((*text != 1) || IsOper(from) || IsOper(to))
		return 1;

	ctcp = &text[1];
	/* Most likely a DCC send .. */
	if (!myncmp(ctcp, "DCC SEND ", 9))
		ctcp = text + 10;
	else
		return 1; /* something else, allow */

	if (*ctcp == '"' && *(ctcp+1))
		end = index(ctcp+1, '"');
	else
		end = index(ctcp, ' ');

	/* check if it was fake.. just pass it along then .. */
	if (!end || (end < ctcp))
		return 1; /* allow */

	size_string = (int)(end - ctcp);

	if (!size_string || (size_string > (BUFSIZE - 1)))
		return 1; /* allow */

	strlcpy(realfile, ctcp, size_string+1);

	if ((fl = dcc_isdiscouraged(from, realfile)))
	{
		if (!on_dccallow_list(to, from))
		{
			char *displayfile = dcc_displayfile(realfile);
			sendto_one(from,
				":%s %d %s :*** Cannot DCC SEND file %s to %s (%s)",
				me.name, RPL_TEXT, from->name, displayfile, to->name, fl->reason);
			sendnotice(from, "User %s is currently not accepting DCC SENDs with such a filename/filetype from you. "
				"Your file %s was not sent.", to->name, displayfile);
			sendnotice(to, "%s (%s@%s) tried to DCC SEND you a file named '%s', the request has been blocked.",
				from->name, from->user->username, GetHost(from), displayfile);
			if (!IsDCCNotice(to))
			{
				SetDCCNotice(to);
				sendnotice(to, "Files like these might contain malicious content (viruses, trojans). "
					"Therefore, you must explicitly allow anyone that tries to send you such files.");
				sendnotice(to, "If you trust %s, and want him/her to send you this file, you may obtain "
					"more information on using the dccallow system by typing '/DCCALLOW HELP'", from->name);
			}
	
			/* if (SHOW_ALLDENIEDDCCS) */
			if (0)
			{
				sendto_umode(UMODE_VICTIM,
					"[DCCALLOW] %s tried to send forbidden file %s (%s) to %s",
					from->name, displayfile, fl->reason, to->name);
			}
			return 0;
		}
	}

	return 1; /* allowed */
}

/* This was modified a bit in order to use newconf. The loading functions
 * have been trashed and integrated into the config parser. The striping
 * function now only uses REPLACEWORD if no word is specifically defined
 * for the word found. Also the freeing function has been ditched. -- codemastr
 */

/*
 * our own strcasestr implementation because strcasestr is often not
 * available or is not working correctly (??).
 */
char *our_strcasestr(char *haystack, char *needle) {
int i;
int nlength = strlen (needle);
int hlength = strlen (haystack);

	if (nlength > hlength) return NULL;
	if (hlength <= 0) return NULL;
	if (nlength <= 0) return haystack;
	for (i = 0; i <= (hlength - nlength); i++) {
		if (strncasecmp (haystack + i, needle, nlength) == 0)
			return haystack + i;
	}
  return NULL; /* not found */
}
inline int fast_badword_match(ConfigItem_badword *badword, char *line)
{
 	char *p;
	int bwlen = strlen(badword->word);
	if ((badword->type & BADW_TYPE_FAST_L) && (badword->type & BADW_TYPE_FAST_R))
		return (our_strcasestr(line, badword->word) ? 1 : 0);

	p = line;
	while((p = our_strcasestr(p, badword->word)))
	{
		if (!(badword->type & BADW_TYPE_FAST_L))
		{
			if ((p != line) && !iswseperator(*(p - 1))) /* aaBLA but no *BLA */
				goto next;
		}
		if (!(badword->type & BADW_TYPE_FAST_R))
		{
			if (!iswseperator(*(p + bwlen)))  /* BLAaa but no BLA* */
				goto next;
		}
		/* Looks like it matched */
		return 1;
next:
		p += bwlen;
	}
	return 0;
}
/* fast_badword_replace:
 * a fast replace routine written by Syzop used for replacing badwords.
 * searches in line for huntw and replaces it with replacew,
 * buf is used for the result and max is sizeof(buf).
 * (Internal assumptions: max > 0 AND max > strlen(line)+1)
 */
inline int fast_badword_replace(ConfigItem_badword *badword, char *line, char *buf, int max)
{
/* Some aliases ;P */
char *replacew = badword->replace ? badword->replace : REPLACEWORD;
char *pold = line, *pnew = buf; /* Pointers to old string and new string */
char *poldx = line;
int replacen = -1; /* Only calculated if needed. w00t! saves us a few nanosecs? lol */
int searchn = -1;
char *startw, *endw;
char *c_eol = buf + max - 1; /* Cached end of (new) line */
int run = 1;
int cleaned = 0;

	Debug((DEBUG_NOTICE, "replacing %s -> %s in '%s'", badword->word, replacew, line));

	while(run) {
		pold = our_strcasestr(pold, badword->word);
		if (!pold)
			break;
		if (replacen == -1)
			replacen = strlen(replacew);
		if (searchn == -1)
			searchn = strlen(badword->word);
		/* Hunt for start of word */
 		if (pold > line) {
			for (startw = pold; (!iswseperator(*startw) && (startw != line)); startw--);
			if (iswseperator(*startw))
				startw++; /* Don't point at the space/seperator but at the word! */
		} else {
			startw = pold;
		}

		if (!(badword->type & BADW_TYPE_FAST_L) && (pold != startw)) {
			/* not matched */
			pold++;
			continue;
		}

		/* Hunt for end of word */
		for (endw = pold; ((*endw != '\0') && (!iswseperator(*endw))); endw++);

		if (!(badword->type & BADW_TYPE_FAST_R) && (pold+searchn != endw)) {
			/* not matched */
			pold++;
			continue;
		}

		cleaned = 1; /* still too soon? Syzop/20050227 */

		/* Do we have any not-copied-yet data? */
		if (poldx != startw) {
			int tmp_n = startw - poldx;
			if (pnew + tmp_n >= c_eol) {
				/* Partial copy and return... */
				memcpy(pnew, poldx, c_eol - pnew);
				*c_eol = '\0';
				return 1;
			}

			memcpy(pnew, poldx, tmp_n);
			pnew += tmp_n;
		}
		/* Now update the word in buf (pnew is now something like startw-in-new-buffer */

		if (replacen) {
			if ((pnew + replacen) >= c_eol) {
				/* Partial copy and return... */
				memcpy(pnew, replacew, c_eol - pnew);
				*c_eol = '\0';
				return 1;
			}
			memcpy(pnew, replacew, replacen);
			pnew += replacen;
		}
		poldx = pold = endw;
	}
	/* Copy the last part */
	if (*poldx) {
		strncpy(pnew, poldx, c_eol - pnew);
		*(c_eol) = '\0';
	} else {
		*pnew = '\0';
	}
	return cleaned;
}

/*
 * Returns a string, which has been filtered by the words loaded via
 * the loadbadwords() function.  It's primary use is to filter swearing
 * in both private and public messages
 */

char *_stripbadwords(char *str, ConfigItem_badword *start_bw, int *blocked)
{
	regmatch_t pmatch[MAX_MATCH];
	static char cleanstr[4096];
	char buf[4096];
	char *ptr;
	int  matchlen, m, stringlen, cleaned;
	ConfigItem_badword *this_word;
	
	*blocked = 0;

	if (!start_bw)
		return str;

	/*
	 * work on a copy
	 */
	stringlen = strlcpy(cleanstr, StripControlCodes(str), sizeof cleanstr);
	memset(&pmatch, 0, sizeof pmatch);
	matchlen = 0;
	buf[0] = '\0';
	cleaned = 0;

	for (this_word = start_bw; this_word; this_word = (ConfigItem_badword *)this_word->next)
	{
		if (this_word->type & BADW_TYPE_FAST)
		{
			if (this_word->action == BADWORD_BLOCK)
			{
				if (fast_badword_match(this_word, cleanstr))
				{
					*blocked = 1;
					return NULL;
				}
			}
			else
			{
				int n;
				/* fast_badword_replace() does size checking so we can use 512 here instead of 4096 */
				n = fast_badword_replace(this_word, cleanstr, buf, 512);
				if (!cleaned && n)
					cleaned = n;
				strcpy(cleanstr, buf);
				memset(buf, 0, sizeof(buf)); /* regexp likes this somehow */
			}
		} else
		if (this_word->type & BADW_TYPE_REGEX)
		{
			if (this_word->action == BADWORD_BLOCK)
			{
				if (!regexec(&this_word->expr, cleanstr, 0, NULL, 0))
				{
					*blocked = 1;
					return NULL;
				}
			}
			else
			{
				ptr = cleanstr; /* set pointer to start of string */
				while (regexec(&this_word->expr, ptr, MAX_MATCH, pmatch,0) != REG_NOMATCH)
				{
					if (pmatch[0].rm_so == -1)
						break;
					m = pmatch[0].rm_eo - pmatch[0].rm_so;
					if (m == 0)
						break; /* anti-loop */
					cleaned = 1;
					matchlen += m;
					strlncat(buf, ptr, sizeof buf, pmatch[0].rm_so);
					if (this_word->replace)
						strlcat(buf, this_word->replace, sizeof buf); 
					else
						strlcat(buf, REPLACEWORD, sizeof buf);
					ptr += pmatch[0].rm_eo;	/* Set pointer after the match pos */
					memset(&pmatch, 0, sizeof(pmatch));
				}

				/* All the better to eat you with! */
				strlcat(buf, ptr, sizeof buf);	
				memcpy(cleanstr, buf, sizeof cleanstr);
				memset(buf, 0, sizeof(buf));
				if (matchlen == stringlen)
					break;
			}
		}
	}

	cleanstr[511] = '\0'; /* cutoff, just to be sure */

	return (cleaned) ? cleanstr : str;
}

char *stripbadwords_message(char *str, int *blocked)
{
#ifdef STRIPBADWORDS
	return stripbadwords(str, conf_badword_message, blocked);
#else
	return NULL;
#endif
}

/* Taken from xchat by Peter Zelezny
 * changed very slightly by codemastr
 * RGB color stripping support added -- codemastr
 */

char *_StripColors(unsigned char *text) {
	int i = 0, len = strlen(text), save_len=0;
	char nc = 0, col = 0, rgb = 0, *save_text=NULL;
	static unsigned char new_str[4096];

	while (len > 0) 
	{
		if ((col && isdigit(*text) && nc < 2) || (col && *text == ',' && nc < 3)) 
		{
			nc++;
			if (*text == ',')
				nc = 0;
		}
		/* Syntax for RGB is ^DHHHHHH where H is a hex digit.
		 * If < 6 hex digits are specified, the code is displayed
		 * as text
		 */
		else if ((rgb && isxdigit(*text) && nc < 6) || (rgb && *text == ',' && nc < 7))
		{
			nc++;
			if (*text == ',')
				nc = 0;
		}
		else 
		{
			if (col)
				col = 0;
			if (rgb)
			{
				if (nc != 6)
				{
					text = save_text+1;
					len = save_len-1;
					rgb = 0;
					continue;
				}
				rgb = 0;
			}
			if (*text == '\003') 
			{
				col = 1;
				nc = 0;
			}
			else if (*text == '\004')
			{
				save_text = text;
				save_len = len;
				rgb = 1;
				nc = 0;
			}
			else 
			{
				new_str[i] = *text;
				i++;
			}
		}
		text++;
		len--;
	}
	new_str[i] = 0;
	return new_str;
}

/* strip color, bold, underline, and reverse codes from a string */
char *_StripControlCodes(unsigned char *text) 
{
	int i = 0, len = strlen(text), save_len=0;
	char nc = 0, col = 0, rgb = 0, *save_text=NULL;
	static unsigned char new_str[4096];
	while (len > 0) 
	{
		if ( col && ((isdigit(*text) && nc < 2) || (*text == ',' && nc < 3)))
		{
			nc++;
			if (*text == ',')
				nc = 0;
		}
		/* Syntax for RGB is ^DHHHHHH where H is a hex digit.
		 * If < 6 hex digits are specified, the code is displayed
		 * as text
		 */
		else if ((rgb && isxdigit(*text) && nc < 6) || (rgb && *text == ',' && nc < 7))
		{
			nc++;
			if (*text == ',')
				nc = 0;
		}
		else 
		{
			if (col)
				col = 0;
			if (rgb)
			{
				if (nc != 6)
				{
					text = save_text+1;
					len = save_len-1;
					rgb = 0;
					continue;
				}
				rgb = 0;
			}
			switch (*text)
			{
			case 3:
				/* color */
				col = 1;
				nc = 0;
				break;
			case 4:
				/* RGB */
				save_text = text;
				save_len = len;
				rgb = 1;
				nc = 0;
				break;
			case 2:
				/* bold */
				break;
			case 31:
				/* underline */
				break;
			case 22:
				/* reverse */
				break;
			case 15:
				/* plain */
				break;
			default:
				new_str[i] = *text;
				i++;
				break;
			}
		}
		text++;
		len--;
	}
	new_str[i] = 0;
	return new_str;
}

