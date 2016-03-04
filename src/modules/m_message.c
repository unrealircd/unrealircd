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

#include "unrealircd.h"

int _is_silenced(aClient *, aClient *);
char *_StripColors(unsigned char *text);
char *_StripControlCodes(unsigned char *text);

int	ban_version(aClient *sptr, char *text);

CMD_FUNC(m_private);
CMD_FUNC(m_notice);
int m_message(aClient *cptr, aClient *sptr, int parc, char *parv[], int notice);

/* Place includes here */
#define MSG_PRIVATE     "PRIVMSG"       /* PRIV */
#define MSG_NOTICE      "NOTICE"        /* NOTI */

ModuleHeader MOD_HEADER(m_message)
  = {
	"message",	/* Name of module */
	"4.0", /* Version */
	"private message and notice", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

MOD_TEST(m_message)
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAddPChar(modinfo->handle, EFUNC_STRIPCOLORS, _StripColors);
	EfunctionAddPChar(modinfo->handle, EFUNC_STRIPCONTROLCODES, _StripControlCodes);
	EfunctionAdd(modinfo->handle, EFUNC_IS_SILENCED, _is_silenced);
	return MOD_SUCCESS;
}

/* This is called on module init, before Server Ready */
MOD_INIT(m_message)
{
	CommandAdd(modinfo->handle, MSG_PRIVATE, m_private, 2, M_USER|M_SERVER|M_RESETIDLE|M_VIRUS);
	CommandAdd(modinfo->handle, MSG_NOTICE, m_notice, 2, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;	
}

/* Is first run when server is 100% ready */
MOD_LOAD(m_message)
{
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
MOD_UNLOAD(m_message)
{
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
int ret;

	if (IsVirus(sptr))
	{
		sendnotice(sptr, "You are only allowed to talk in '%s'", SPAMFILTER_VIRUSCHAN);
		return CANPRIVMSG_CONTINUE;
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
		Hook *tmphook;
		
		if (!notice && MyConnect(sptr) &&
		    acptr->user && acptr->user->away)
			sendto_one(sptr, rpl_str(RPL_AWAY),
			    me.name, sptr->name, acptr->name,
			    acptr->user->away);

		if (MyClient(sptr))
		{
			ret = dospamfilter(sptr, *text, (notice ? SPAMF_USERNOTICE : SPAMF_USERMSG), acptr->name, 0, NULL);
			if (ret < 0)
				return ret;
		}

		for (tmphook = Hooks[HOOKTYPE_PRE_USERMSG]; tmphook; tmphook = tmphook->next) {
			*text = (*(tmphook->func.pcharfunc))(sptr, acptr, *text, notice);
			if (!*text)
				break;
		}
		if (!*text)
			return CANPRIVMSG_CONTINUE;
		
		RunHook4(HOOKTYPE_USERMSG, sptr, acptr, *text, notice);
		
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
**	parv[1] = receiver list
**	parv[2] = message text
**
** massive cleanup
** rev argv 6/91
**
*/
int m_message(aClient *cptr, aClient *sptr, int parc, char *parv[], int notice)
{
	aClient *acptr, *srvptr;
	char *s;
	aChannel *chptr;
	char *nick, *server, *p, *cmd, *p2, *pc, *text, *newcmd;
	int  cansend = 0;
	int  prefix = 0;
	char pfixchan[CHANNELLEN + 4];
	int ret;

	/*
	 * Reasons why someone can't send to a channel
	 * Note: a few have been moved to modules now, but don't just blindly delete them!! as we use the array index ;p
	 */
	static char *err_cantsend[] = {
		"You need voice (+v)",
		"No external channel messages",
		"Color is not permitted in this channel <<NOLONGERUSED>>",
		"You are banned",
		"CTCPs are not permitted in this channel <<NOLONGERUSED>>",
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
		    me.name, sptr->name, cmd);
		return -1;
	}

	if (parc < 3 || *parv[2] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NOTEXTTOSEND), me.name, sptr->name);
		return -1;
	}

	if (MyConnect(sptr))
		parv[1] = (char *)canonize(parv[1]);
		
	for (p = NULL, nick = strtoken(&p, parv[1], ","); nick; nick = strtoken(&p, NULL, ","))
	{
		/* The nicks "ircd" and "irc" are special (and reserved) */

		if (!strcasecmp(nick, "ircd") && MyClient(sptr))
			return 0;

		if (!strcasecmp(nick, "irc") && MyClient(sptr))
		{
			/* When ban version { } is enabled the IRCd sends a CTCP VERSION request
			 * from the "IRC" nick. So we need to handle CTCP VERSION replies to "IRC".
			 */
			if (!strncmp(parv[2], "\1VERSION ", 9))
				return ban_version(sptr, parv[2] + 9);
			if (!strncmp(parv[2], "\1SCRIPT ", 8))
				return ban_version(sptr, parv[2] + 8);
			return 0;
		}
		
		p2 = strchr(nick, '#');
		prefix = 0;
		
		/* Message to channel */
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
					if (MyClient(sptr) && !op_can_override("override:message:prefix",sptr,chptr,NULL))
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

				sendto_channelprefix_butone(cptr,
				    sptr, chptr,
				    prefix,
				    notice ? ":%s NOTICE %s :%s" : ":%s PRIVMSG %s :%s",
				    sptr->name, nick, text);

				sendanyways = 0;
				RunHook4(HOOKTYPE_CHANMSG, sptr, chptr, text, notice);
				continue;
			}
			else
			if (MyClient(sptr))
			{
				if (!notice || (cansend == 8)) /* privmsg or 'cannot send notice'... */
					sendto_one(sptr, err_str(ERR_CANNOTSENDTOCHAN),
					    me.name, sptr->name, chptr->chname,
					    err_cantsend[cansend - 1], p2);
			}
			continue;
		}
		else if (p2)
		{
			sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name,
			    sptr->name, p2);
			continue;
		}


		/* Message to $servermask */
		if (*nick == '$')
		{
			if (!ValidatePermissionsForPath("notice:global", sptr, NULL, NULL, NULL))
			{
				/* Apparently no other IRCd does this, but I think it's confusing not to
				 * send an error message, especially with our new privilege system.
				 * Error message could be more descriptive perhaps.
				 */
				sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
				continue;
			}
			sendto_match_butone(IsServer(cptr) ? cptr : NULL,
			    sptr, nick + 1,
			    (*nick == '#') ? MATCH_HOST :
			    MATCH_SERVER,
			    ":%s %s %s :%s", sptr->name, cmd, nick, parv[2]);
			continue;
		}

		/* nickname addressed? */
		acptr = hash_find_nickserver(nick, NULL);
		if (acptr)
		{
			text = parv[2];
			newcmd = cmd;
			ret = can_privmsg(cptr, sptr, acptr, notice, &text, &newcmd);
			if (ret == CANPRIVMSG_SEND)
			{
				sendto_message_one(acptr, sptr, sptr->name, newcmd, (MyClient(acptr) ? acptr->name : nick), text);
				continue;
			} else
			if (ret == CANPRIVMSG_CONTINUE)
				continue;
			else
				return ret;
		}

		/* If nick@server -and- the @server portion was set::services-server then send a special message */
		if (!acptr)
		{
			char *server = index(nick, '@');
			if (server && strncasecmp(server + 1, SERVICES_NAME, strlen(SERVICES_NAME)) == 0)
			{
				sendto_one(sptr, err_str(ERR_SERVICESDOWN), me.name, sptr->name, nick);
				continue;
			}
		}

		/* nothing, nada, not anything found */
		sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, sptr->name, nick);
		continue;
	}
	return 0;
}

/*
** m_private
**	parv[1] = receiver list
**	parv[2] = message text
*/
CMD_FUNC(m_private)
{
	return m_message(cptr, sptr, parc, parv, 0);
}

/*
** m_notice
**	parv[1] = receiver list
**	parv[2] = notice text
*/
CMD_FUNC(m_notice)
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
	static char sender[HOSTLEN + NICKLEN + USERLEN + 5];
	
	if (!acptr->user || !sptr->user || !(lp = acptr->user->silence))
		return 0;

	ircsnprintf(sender, sizeof(sender), "%s!%s@%s", sptr->name, sptr->user->username, GetHost(sptr));

	for (; lp; lp = lp->next)
	{
		if (!match(lp->value.cp, sender))
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

	if ((*text != 1) || ValidatePermissionsForPath("immune:dcc",sptr,targetcli,NULL,NULL) || (targetcli && ValidatePermissionsForPath("self:getbaddcc",targetcli,NULL,NULL,NULL)))
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
		sptr->flags |= FLAGS_DCCBLOCK;

		RunHook5(HOOKTYPE_DCC_DENIED, sptr, target, realfile, displayfile, fl);

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

	if ((*text != 1) || ValidatePermissionsForPath("immune:dcc",from,to,NULL,NULL)|| ValidatePermissionsForPath("self:getbaddcc",to,NULL,NULL,NULL))
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
	
			return 0;
		}
	}

	return 1; /* allowed */
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
			else if (*text != '\026') /* (strip reverse too) */
			{
				new_str[i] = *text;
				i++;
			}
		}
		text++;
		len--;
	}
	new_str[i] = 0;
	if (new_str[0] == '\0')
		return NULL;
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

int	ban_version(aClient *sptr, char *text)
{	
	int len;
	ConfigItem_ban *ban;

	len = strlen(text);
	if (!len)
		return 0;

	if (text[len-1] == '\1')
		text[len-1] = '\0'; /* remove CTCP REPLY terminator (ASCII 1) */

	if ((ban = Find_ban(NULL, text, CONF_BAN_VERSION)))
		return place_host_ban(sptr, ban->action, ban->reason, BAN_VERSION_TKL_TIME);

	return 0;
}
