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

static int is_silenced(aClient *, aClient *);

DLLFUNC int m_message(aClient *cptr, aClient *sptr, int parc, char *parv[], int notice);
DLLFUNC int  m_notice(aClient *cptr, aClient *sptr, int parc, char *parv[]);
DLLFUNC int  m_private(aClient *cptr, aClient *sptr, int parc, char *parv[]);

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

/* This is called on module init, before Server Ready */
DLLFUNC int MOD_INIT(m_message)(ModuleInfo *modinfo)
{
	/*
	 * We call our add_Command crap here
	*/
	add_CommandX(MSG_PRIVATE, TOK_PRIVATE, m_private, MAXPARA, M_USER|M_SERVER|M_RESETIDLE);
	add_Command(MSG_NOTICE, TOK_NOTICE, m_notice, MAXPARA);
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
	aClient *acptr;
	char *s;
	aChannel *chptr;
	char *nick, *server, *p, *cmd, *ctcp, *p2, *pc, *text;
	int  cansend = 0;
	int  prefix = 0;
	char pfixchan[CHANNELLEN + 32];

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
		/*
		   ** nickname addressed?
		 */
		if (!strcasecmp(nick, "ircd") && MyClient(sptr))
		{
			int ret = 0;
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
				int ret;
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
			/* Umode +R (idea from Bahamut) */
			if (IsRegNickMsg(acptr) && !IsRegNick(sptr) && !IsULine(sptr) && !IsOper(sptr)) {
				sendto_one(sptr, err_str(ERR_NONONREG), me.name, parv[0],
					acptr->name);
				return 0;
			}
			if (IsNoCTCP(acptr) && !IsOper(sptr) && *parv[2] == 1 && acptr != sptr)
			{
				ctcp = &parv[2][1];
				if (myncmp(ctcp, "ACTION ", 7) && myncmp(ctcp, "DCC ", 4))
				{
					sendto_one(sptr, err_str(ERR_NOCTCP), me.name, parv[0],
						acptr->name);
					return 0;
				}
			}
			/* F:Line stuff by _Jozeph_ added by Stskeeps with comments */
			if (*parv[2] == 1 && MyClient(sptr) && !IsOper(sptr))
				/* somekinda ctcp thing
				   and i don't want to waste cpu on what others already checked..
				   (should this be checked ??) --Sts
				 */
			{
				ctcp = &parv[2][1];
				/* Most likely a DCC send .. */
				if (!myncmp(ctcp, "DCC SEND ", 9))
				{
					ConfigItem_deny_dcc *fl;
					char *end, file[BUFSIZE];
					int  size_string = 0;

					if (sptr->flags & FLAGS_DCCBLOCK)
					{
						sendto_one(sptr, ":%s NOTICE %s :*** You are blocked from sending files as you have tried to send a forbidden file - reconnect to regain ability to send",
							me.name, sptr->name);
						continue;
					}
					ctcp = &parv[2][10];
					if (*ctcp == '"' && *(ctcp+1))
						end = index(ctcp+1, '"');
					else
						end = index(ctcp, ' ');
					/* check if it was fake.. just pass it along then .. */
					if (!end || (end < ctcp))
						goto dcc_was_ok;

					size_string = (int)(end - ctcp);

					if (!size_string
					    || (size_string > (BUFSIZE - 1)))
						goto dcc_was_ok;

					strncpy(file, ctcp, size_string);
					file[size_string] = '\0';

					if ((fl =
					    (ConfigItem_deny_dcc *)
					    dcc_isforbidden(cptr, sptr, acptr,
					    file)))
					{
						sendto_one(sptr,
						    ":%s %d %s :*** Cannot DCC SEND file %s to %s (%s)",
						    me.name, RPL_TEXT,
						    sptr->name, file,
						    acptr->name,
						    fl->reason ? fl->reason :
						    "Possible infected virus file");
						sendto_one(sptr, ":%s NOTICE %s :*** You have been blocked from sending files, reconnect to regain permission to send files",
							me.name, sptr->name);

						sendto_umode(UMODE_VICTIM,
						    "%s tried to send forbidden file %s (%s) to %s (is blocked now)",
						    sptr->name, file,
						    fl->reason, acptr->name);
						sptr->flags |= FLAGS_DCCBLOCK;
						continue;
					}
					/* if it went here it was a legal send .. */
				}
			}
		      dcc_was_ok:

			if (MyClient(sptr)
			    && check_for_target_limit(sptr, acptr, acptr->name))
				continue;

			if (!is_silenced(sptr, acptr))
			{
#ifdef STRIPBADWORDS
				int blocked = 0;
#endif
				char *newcmd = cmd, *text;
				Hook *tmphook;
				
				if (notice && IsWebTV(acptr) && *parv[2] != '\1')
					newcmd = MSG_PRIVATE;
				if (!notice && MyConnect(sptr) &&
				    acptr->user && acptr->user->away)
					sendto_one(sptr, rpl_str(RPL_AWAY),
					    me.name, parv[0], acptr->name,
					    acptr->user->away);

#ifdef STRIPBADWORDS
				if (MyClient(sptr) && !IsULine(acptr) && IsFilteringWords(acptr))
				{
					text = stripbadwords_message(parv[2], &blocked);
					if (blocked)
					{
						if (!notice && MyClient(sptr))
							sendto_one(sptr, rpl_str(ERR_NOSWEAR),
								me.name, parv[0], acptr->name);
						continue;
					}
				}
				else
#endif
					text = parv[2];

				for (tmphook = Hooks[HOOKTYPE_USERMSG]; tmphook; tmphook = tmphook->next) {
					text = (*(tmphook->func.pcharfunc))(cptr, sptr, acptr, text, (int)(newcmd == MSG_NOTICE ? 1 : 0) );
					if (!text)
						break;
				}
				if (!text)
					continue;
					
				sendto_message_one(acptr,
				    sptr, parv[0], newcmd, nick, text);
			}
			continue;
		}

		p2 = (char *)strchr(nick, '#');
		prefix = 0;
		if (p2 && (chptr = find_channel(p2, NullChn)))
		{
			if (p2 != nick)
			{
				int len = 0;
				for (pc = nick; pc != p2; pc++)
				{
					switch (*pc)
					{
					  case '+':
						  if (!(prefix & PREFIX_VOICE))
						  	pfixchan[len++] = '+';
						  prefix |= PREFIX_VOICE;
						  break;
					  case '%':
						  if (!(prefix & PREFIX_HALFOP))
						  	pfixchan[len++] = '%';
						  prefix |= PREFIX_HALFOP;
						  break;
					  case '@':
						  if (!(prefix & PREFIX_OP))
						  	pfixchan[len++] = '@';
						  prefix |= PREFIX_OP;
						  break;
#ifdef PREFIX_AQ
					  case '&':
						  if (!(prefix & PREFIX_ADMIN))
							pfixchan[len++] = '&';
						  prefix |= PREFIX_ADMIN;
					  	  break;
					  case '~':
						  if (!(prefix & PREFIX_OWNER))
							pfixchan[len++] = '~';
						  prefix |= PREFIX_OWNER;
						  break;
#endif
					  default:
						  break;	/* ignore it :P */
					}
				}
				pfixchan[len] = '\0';
				strlcat(pfixchan, p2, sizeof(pfixchan));
				nick = pfixchan;
			}
			
			cansend =
			    !IsULine(sptr) ? can_send(sptr, chptr, parv[2], notice) : 0;
			if (!cansend)
			{
#ifdef STRIPBADWORDS
				int blocked = 0;
#endif
				Hook *tmphook;
#ifdef NEWCHFLOODPROT
				if (chptr->mode.floodprot && chptr->mode.floodprot->l[FLD_TEXT])
#else
				if (chptr->mode.per)
#endif
					if (check_for_chan_flood(cptr, sptr, chptr) == 1)
						continue;

				if (!CHANCMDPFX)
					sendanyways = (parv[2][0] == '`' ? 1 : 0);
				else
					sendanyways = (strchr(CHANCMDPFX,parv[2][0]) ? 1 : 0);
				text = parv[2];
				if (MyClient(sptr) && (chptr->mode.mode & MODE_STRIP))
					text = StripColors(parv[2]);
#ifdef STRIPBADWORDS
				if (MyClient(sptr))
				{
 #ifdef STRIPBADWORDS_CHAN_ALWAYS
					text = stripbadwords_channel(text,& blocked);
					if (blocked)
					{
						if (!notice)
							sendto_one(sptr, err_str(ERR_CANNOTSENDTOCHAN),
							    me.name, parv[0], parv[0],
							    err_cantsend[6], p2);
						continue;
					}
 #else
					if (chptr->mode.mode & MODE_STRIPBADWORDS)
					{
						text = stripbadwords_channel(text, &blocked);
						if (blocked)
						{
							if (!notice)
								sendto_one(sptr, err_str(ERR_CANNOTSENDTOCHAN),
								    me.name, parv[0], parv[0],
								    err_cantsend[6], p2);
							continue;
						}
					}
 #endif
				}
#endif
				for (tmphook = Hooks[HOOKTYPE_CHANMSG]; tmphook; tmphook = tmphook->next) {
					text = (*(tmphook->func.pcharfunc))(cptr, sptr, chptr, text, notice);
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

#ifdef NEWCHFLOODPROT
				if (chptr->mode.floodprot && !is_skochanop(sptr, chptr) &&
				    !IsULine(sptr) && do_chanflood(chptr->mode.floodprot, FLD_MSG) &&
				    MyClient(sptr))
				{
					do_chanflood_action(chptr, FLD_MSG, "msg/notice");
				}
				
				if (chptr->mode.floodprot && !is_skochanop(sptr, chptr) &&
				    (text[0] == '\001') && strncmp(text+1, "ACTION ", 7) &&
				    do_chanflood(chptr->mode.floodprot, FLD_CTCP) && MyClient(sptr))
				{
					do_chanflood_action(chptr, FLD_CTCP, "CTCP");
				}
#endif
				sendanyways = 0;
				continue;
			}
			else
			if (MyClient(sptr))
			{
				if (!notice || (cansend == 8)) /* privmsg or 'cannot send notice'... */
					sendto_one(sptr, err_str(ERR_CANNOTSENDTOCHAN),
					    me.name, parv[0], parv[0],
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
			if (IsULine(sptr))
				goto itsokay;
			if (!(s = (char *)rindex(nick, '.')))
			{
				sendto_one(sptr, err_str(ERR_NOTOPLEVEL),
				    me.name, parv[0], nick);
				continue;
			}
			while (*++s)
				if (*s == '.' || *s == '*' || *s == '?')
					break;
			if (*s == '*' || *s == '?')
			{
				sendto_one(sptr, err_str(ERR_WILDTOPLEVEL),
				    me.name, parv[0], nick);
				continue;
			}
		      itsokay:
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
				acptr = find_server_quick(server + 1);
				if (acptr)
				{
					/*
					   ** Not destined for a user on me :-(
					 */
					if (!IsMe(acptr))
					{
						if (IsToken(acptr))
							sendto_one(acptr,
							    ":%s %s %s :%s", parv[0],
							    notice ? TOK_NOTICE : TOK_PRIVATE,
							    nick, parv[2]);
						else
							sendto_one(acptr,
							    ":%s %s %s :%s", parv[0],
							    cmd, nick, parv[2]);
						continue;
					}

					/* Find the nick@server using hash. */
					acptr =
					    find_nickserv(nick,
					    (aClient *)NULL);
					if (acptr)
					{
						sendto_prefix_one(acptr, sptr,
						    ":%s %s %s :%s",
						    parv[0], cmd,
						    acptr->name, parv[2]);
						continue;
					}
				}
				if (server
				    && strncasecmp(server + 1, SERVICES_NAME,
				    strlen(SERVICES_NAME)) == 0)
					sendto_one(sptr,
					    err_str(ERR_SERVICESDOWN), me.name,
					    parv[0], nick);
				else
					sendto_one(sptr,
					    err_str(ERR_NOSUCHNICK), me.name,
					    parv[0], nick);

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
static int is_silenced(aClient *sptr, aClient *acptr)
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
