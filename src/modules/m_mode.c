/*
 *   IRC - Internet Relay Chat, src/modules/m_mode.c
 *   (C) 2005 The UnrealIRCd Team
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers.
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
#include "proto.h"
#include "channel.h"
#include "macros.h"
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
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

/* Forward declarations */
DLLFUNC CMD_FUNC(m_mode);
DLLFUNC void _do_mode(aChannel *chptr, aClient *cptr, aClient *sptr, int parc, char *parv[], time_t sendts, int samode);
DLLFUNC void _set_mode(aChannel *chptr, aClient *cptr, int parc, char *parv[], u_int *pcount,
    char pvar[MAXMODEPARAMS][MODEBUFLEN + 3], int bounce);
DLLFUNC CMD_FUNC(_m_umode);

/* local: */
static void bounce_mode(aChannel *, aClient *, int, char **);
int do_mode_char(aChannel *chptr, long modetype, char modechar, char *param,
    u_int what, aClient *cptr,
     u_int *pcount, char pvar[MAXMODEPARAMS][MODEBUFLEN + 3], char bounce, long my_access);
int do_extmode_char(aChannel *chptr, int modeindex, char *param, u_int what,
                    aClient *cptr, u_int *pcount, char pvar[MAXMODEPARAMS][MODEBUFLEN + 3],
                    char bounce);
#ifdef EXTCMODE
void make_mode_str(aChannel *chptr, long oldm, Cmode_t oldem, long oldl, int pcount,
    char pvar[MAXMODEPARAMS][MODEBUFLEN + 3], char *mode_buf, char *para_buf, char bounce);
#else
void make_mode_str(aChannel *chptr, long oldm, long oldl, int pcount,
    char pvar[MAXMODEPARAMS][MODEBUFLEN + 3], char *mode_buf, char *para_buf, char bounce);
#endif
static void mode_cutoff(char *s);
static void mode_cutoff2(aClient *sptr, aChannel *chptr, int *parc_out, char *parv[]);

static int samode_in_progress = 0;

#define MSG_MODE 	"MODE"	
#define TOK_MODE 	"G"	

ModuleHeader MOD_HEADER(m_mode)
  = {
	"m_mode",
	"$Id$",
	"command /mode", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_TEST(m_mode)(ModuleInfo *modinfo)
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAddVoid(modinfo->handle, EFUNC_DO_MODE, _do_mode);
	EfunctionAddVoid(modinfo->handle, EFUNC_SET_MODE, _set_mode);
	EfunctionAdd(modinfo->handle, EFUNC_M_UMODE, _m_umode);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_INIT(m_mode)(ModuleInfo *modinfo)
{
	CommandAdd(modinfo->handle, MSG_MODE, TOK_MODE, m_mode, MAXPARA, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_mode)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_mode)(int module_unload)
{
	return MOD_SUCCESS;
}

/*
 * m_mode -- written by binary (garryb@binary.islesfan.net)
 *	Completely rewrote it.  The old mode command was 820 lines of ICKY
 * coding, which is a complete waste, because I wrote it in 570 lines of
 * *decent* coding.  This is also easier to read, change, and fine-tune.  Plus,
 * everything isn't scattered; everything's grouped where it should be.
 *
 * parv[0] - sender
 * parv[1] - channel
 */
CMD_FUNC(m_mode)
{
	long unsigned sendts = 0;
	Ban *ban;
	aChannel *chptr;


	/* Now, try to find the channel in question */
	if (parc > 1)
	{
		if (*parv[1] == '#')
		{
			chptr = find_channel(parv[1], NullChn);
			if (chptr == NullChn)
			{
				return m_umode(cptr, sptr, parc, parv);
			}
		}
		else
			return m_umode(cptr, sptr, parc, parv);
	}
	else
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "MODE");
		return 0;
	}

	if (MyConnect(sptr))
		clean_channelname(parv[1]);
	if (check_channelmask(sptr, cptr, parv[1]))
		return 0;

	if (parc < 3)
	{
		*modebuf = *parabuf = '\0';
		
		modebuf[1] = '\0';
		channel_modes(sptr, modebuf, parabuf, chptr);
		sendto_one(sptr, rpl_str(RPL_CHANNELMODEIS), me.name, parv[0],
		    chptr->chname, modebuf, parabuf);
		sendto_one(sptr, rpl_str(RPL_CREATIONTIME), me.name, parv[0],
		    chptr->chname, chptr->creationtime);
		return 0;
	}

	if (IsPerson(sptr) && parc < 4 && ((*parv[2] == 'b'
	    && parv[2][1] == '\0') || (parv[2][1] == 'b' && parv[2][2] == '\0'
	    && (*parv[2] == '+' || *parv[2] == '-'))))
	{
		if (!IsMember(sptr, chptr) && !IsAnOper(sptr))
			return 0;
		/* send ban list */
		for (ban = chptr->banlist; ban; ban = ban->next)
			sendto_one(sptr, rpl_str(RPL_BANLIST), me.name,
			    sptr->name, chptr->chname, ban->banstr,
			    ban->who, ban->when);
		sendto_one(cptr,
		    rpl_str(RPL_ENDOFBANLIST), me.name, sptr->name,
		    chptr->chname);
		return 0;
	}

	if (IsPerson(sptr) && parc < 4 && ((*parv[2] == 'e'
	    && parv[2][1] == '\0') || (parv[2][1] == 'e' && parv[2][2] == '\0'
	    && (*parv[2] == '+' || *parv[2] == '-'))))
	{
		if (!IsMember(sptr, chptr) && !IsAnOper(sptr))
			return 0;
		/* send exban list */
		for (ban = chptr->exlist; ban; ban = ban->next)
			sendto_one(sptr, rpl_str(RPL_EXLIST), me.name,
			    sptr->name, chptr->chname, ban->banstr,
			    ban->who, ban->when);
		sendto_one(cptr,
		    rpl_str(RPL_ENDOFEXLIST), me.name, sptr->name,
		    chptr->chname);
		return 0;
	}

	if (IsPerson(sptr) && parc < 4 && ((*parv[2] == 'q'
	    && parv[2][1] == '\0') || (parv[2][1] == 'q' && parv[2][2] == '\0'
	    && (*parv[2] == '+' || *parv[2] == '-'))))
	{
		if (!IsMember(sptr, chptr) && !IsAnOper(sptr))
			return 0;
		{
			Member *member;
			/* send chanowner list */
			/* [Whole story about bad loops removed, sorry ;)]
			 * Now rewritten so it works (was: bad logic) -- Syzop
			 */
			for (member = chptr->members; member; member = member->next)
			{
				if (is_chanowner(member->cptr, chptr))
					sendto_one(sptr, rpl_str(RPL_QLIST),
					    me.name, sptr->name, chptr->chname,
					    member->cptr->name);
			}
			sendto_one(cptr,
			    rpl_str(RPL_ENDOFQLIST), me.name, sptr->name,
			    chptr->chname);
			return 0;
		}
	}

	if (IsPerson(sptr) && parc < 4 && ((*parv[2] == 'a'
	    && parv[2][1] == '\0') || (parv[2][1] == 'a' && parv[2][2] == '\0'
	    && (*parv[2] == '+' || *parv[2] == '-'))))
	{
		if (!IsMember(sptr, chptr) && !IsAnOper(sptr))
			return 0;
		{
			Member *member;
			/* send chanowner list */
			/* [Whole story about bad loops removed, sorry ;)]
			 * Now rewritten so it works (was: bad logic) -- Syzop
			 */
			for (member = chptr->members; member; member = member->next)
			{
				if (is_chanprot(member->cptr, chptr))
					sendto_one(sptr, rpl_str(RPL_ALIST),
					    me.name, sptr->name, chptr->chname,
					    member->cptr->name);
			}
			sendto_one(cptr,
			    rpl_str(RPL_ENDOFALIST), me.name, sptr->name,
			    chptr->chname);
			return 0;
		}
	}


	if (IsPerson(sptr) && parc < 4 && ((*parv[2] == 'I'
	    && parv[2][1] == '\0') || (parv[2][1] == 'I' && parv[2][2] == '\0'
	    && (*parv[2] == '+' || *parv[2] == '-'))))
	{
		if (!IsMember(sptr, chptr) && !IsAnOper(sptr))
			return 0;
		for (ban = chptr->invexlist; ban; ban = ban->next)
			sendto_one(sptr, rpl_str(RPL_INVEXLIST), me.name,
			    sptr->name, chptr->chname, ban->banstr,
			    ban->who, ban->when);
		sendto_one(sptr, rpl_str(RPL_ENDOFINVEXLIST), me.name,
		    sptr->name, chptr->chname);
		return 0;
	}
	opermode = 0;

#ifndef NO_OPEROVERRIDE
        if (IsPerson(sptr) && !IsULine(sptr) && !is_chan_op(sptr, chptr)
            && !is_half_op(sptr, chptr) && (MyClient(sptr) ? (IsOper(sptr) &&
	    OPCanOverride(sptr)) : IsOper(sptr)))
        {
                sendts = 0;
                opermode = 1;
                goto aftercheck;
        }

        if (IsPerson(sptr) && !IsULine(sptr) && !is_chan_op(sptr, chptr)
            && is_half_op(sptr, chptr) && (MyClient(sptr) ? (IsOper(sptr) &&
	    OPCanOverride(sptr)) : IsOper(sptr)))
        {
                opermode = 2;
                goto aftercheck;
        }
#endif

	if (IsPerson(sptr) && !IsULine(sptr) && !is_chan_op(sptr, chptr)
	    && !is_half_op(sptr, chptr)
	    && (cptr == sptr || !IsSAdmin(sptr) || !IsOper(sptr)))
	{
		if (cptr == sptr)
		{
			sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
			    me.name, parv[0], chptr->chname);
			return 0;
		}
		sendto_one(cptr, ":%s MODE %s -oh %s %s 0",
		    me.name, chptr->chname, parv[0], parv[0]);
		/* Tell the other server that the user is
		 * de-opped.  Fix op desyncs. */
		bounce_mode(chptr, cptr, parc - 2, parv + 2);
		return 0;
	}

	if (IsServer(sptr) && (sendts = TS2ts(parv[parc - 1]))
	    && !IsULine(sptr) && chptr->creationtime
	    && sendts > chptr->creationtime)
	{
		if (!(*parv[2] == '&'))	/* & denotes a bounce */
		{
			/* !!! */
			sendto_snomask(SNO_EYES,
			    "*** TS bounce for %s - %lu(ours) %lu(theirs)",
			    chptr->chname, chptr->creationtime, sendts);
			bounce_mode(chptr, cptr, parc - 2, parv + 2);
		}
		return 0;
	}
	if (IsServer(sptr) && !sendts && *parv[parc - 1] != '0')
		sendts = -1;
	if (IsServer(sptr) && sendts != -1)
		parc--;		/* server supplied a time stamp, remove it now */

      aftercheck:
/*	if (IsPerson(sptr) && IsOper(sptr)) {
		if (!is_chan_op(sptr, chptr)) {
			if (MyClient(sptr) && !IsULine(cptr) && mode_buf[1])
				sendto_snomask(SNO_EYES, "*** OperMode [IRCop: %s] - [Channel: %s] - [Mode: %s %s]",
        	 		   sptr->name, chptr->chname, mode_buf, parabuf);
			sendts = 0;
		}
	}	
*/

	/* This is to prevent excess +<whatever> modes. -- Syzop */
	if (MyClient(sptr) && parv[2])
	{
		mode_cutoff(parv[2]);
		mode_cutoff2(sptr, chptr, &parc, parv);
	}

	/* Filter out the unprivileged FIRST. *
	 * Now, we can actually do the mode.  */

	(void)do_mode(chptr, cptr, sptr, parc - 2, parv + 2, sendts, 0);
	opermode = 0; /* Important since sometimes forgotten. -- Syzop */
	return 0;
}

/** Cut off mode string (eg: +abcdfjkdsgfgs) at MAXMODEPARAMS modes.
 * @param s The mode string (modes only, no parameters)
 * @notes Should only used on local clients
 * @author Syzop
 */
static void mode_cutoff(char *s)
{
unsigned short modesleft = MAXMODEPARAMS * 2; /* be generous... */

	for (; *s && modesleft; s++)
		if ((*s != '-') && (*s != '+'))
			modesleft--;
	*s = '\0';
}

/** Another mode cutoff routine - this one for the server-side
 * amplification/enlargement problem that happens with bans/exempts/invex
 * as explained in #2837. -- Syzop
 */
static void mode_cutoff2(aClient *sptr, aChannel *chptr, int *parc_out, char *parv[])
{
int modes = 0;
char *s;
int len, i;
int parc = *parc_out;

	if (parc-2 <= 3)
		return; /* Less than 3 mode parameters? Then we don't even have to check */

	/* Calculate length of MODE if it would go through fully as-is */
	/* :nick!user@host MODE #channel +something param1 param2 etc... */
	len = strlen(sptr->name) + strlen(sptr->user->username) + strlen(GetHost(sptr)) +
	      strlen(chptr->chname) + 11;
	
	len += strlen(parv[2]);

	if (*parv[2] != '+' && *parv[2] != '-')
		len++;
	
	for (i = 3; parv[i]; i++)
	{
		len += strlen(parv[i]) + 1; /* (+1 for the space character) */
		/* +4 is another potential amplification (per-param).
		 * If we were smart we would only check this for b/e/I and only for
		 * relevant cases (not for all extended), but this routine is dumb,
		 * so we just +4 for any case where the full mask is missing.
		 * It's better than assuming +4 for all cases, though...
		 */
		if (match("*!*@*", parv[i]))
			len += 4;
	}

	/* Now check if the result is acceptable... */	
	if (len < 510)
		return; /* Ok, no problem there... */

	/* Ok, we have a potential problem...
	 * we just dump the last parameter... check how much space we saved...
	 * and try again if that did not help
	 */
	for (i = parc-1; parv[i] && (i > 3); i--)
	{
		len -= strlen(parv[i]);
		if (match("*!*@*", parv[i]))
			len -= 4; /* must adjust accordingly.. */
		parv[i] = NULL;
		*parc_out--;
		if (len < 510)
			break;
	}
	/* This may be reached if like the first parameter is really insane long..
	 * which is no problem, as other layers (eg: ban) takes care of that.
	 * We're done...
	 */
}

/* bounce_mode -- written by binary
 *	User or server is NOT authorized to change the mode.  This takes care
 * of making the bounce string and bounce it.  Because of the 1 for the bounce
 * param (last param) of the calls to set_mode and make_mode_str, it will not
 * set the mode, but create the bounce string.
 */
static void bounce_mode(aChannel *chptr, aClient *cptr, int parc, char *parv[])
{
	char pvar[MAXMODEPARAMS][MODEBUFLEN + 3];
	int  pcount;

	set_mode(chptr, cptr, parc, parv, &pcount, pvar, 1);

	if (chptr->creationtime)
		sendto_one(cptr, ":%s MODE %s &%s %s %lu", me.name,
		    chptr->chname, modebuf, parabuf, chptr->creationtime);
	else
		sendto_one(cptr, ":%s MODE %s &%s %s", me.name, chptr->chname,
		    modebuf, parabuf);

	/* the '&' denotes a bounce so servers won't bounce a bounce */
}

/* do_mode -- written by binary
 *	User or server is authorized to do the mode.  This takes care of
 * setting the mode and relaying it to other users and servers.
 */
DLLFUNC void _do_mode(aChannel *chptr, aClient *cptr, aClient *sptr, int parc, char *parv[], time_t sendts, int samode)
{
	char pvar[MAXMODEPARAMS][MODEBUFLEN + 3];
	int  pcount;
	char tschange = 0, isbounce = 0;	/* fwd'ing bounce */

	if (**parv == '&')
		isbounce = 1;

	/* Please keep the next 3 lines next to each other */
	samode_in_progress = samode;
	set_mode(chptr, sptr, parc, parv, &pcount, pvar, 0);
	samode_in_progress = 0; 

	if (IsServer(sptr))
	{
		if (sendts > 0)
		{
			if (!chptr->creationtime
			    || sendts < chptr->creationtime)
			{
				tschange = 1;
/*
				if (chptr->creationtime != 0)
					sendto_snomask(SNO_EYES, "*** TS fix for %s - %lu(ours) %lu(theirs)",
					chptr->chname, chptr->creationtime, sendts);			
					*/
				chptr->creationtime = sendts;
				if (sendts < 750000)
				{
					sendto_realops(
						"Warning! Possible desynch: MODE for channel %s ('%s %s') has fishy timestamp (%ld) (from %s/%s)",
						chptr->chname, modebuf, parabuf, sendts, cptr->name, sptr->name);
					ircd_log(LOG_ERROR, "Possible desynch: MODE for channel %s ('%s %s') has fishy timestamp (%ld) (from %s/%s)",
						chptr->chname, modebuf, parabuf, sendts, cptr->name, sptr->name);
				}
				/* new chan or our timestamp is wrong */
				/* now works for double-bounce prevention */

			}
			if (sendts > chptr->creationtime && chptr->creationtime)
			{
				/* theirs is wrong but we let it pass anyway */
				sendts = chptr->creationtime;
				sendto_one(cptr, ":%s MODE %s + %lu", me.name,
				    chptr->chname, chptr->creationtime);
			}
		}
		if (sendts == -1 && chptr->creationtime)
			sendts = chptr->creationtime;
	}
	if (*modebuf == '\0' || (*(modebuf + 1) == '\0' && (*modebuf == '+'
	    || *modebuf == '-')))
	{
		if (tschange || isbounce) {	/* relay bounce time changes */
			if (chptr->creationtime)
				sendto_serv_butone_token(cptr, me.name,
				    MSG_MODE, TOK_MODE, "%s %s+ %lu",
				    chptr->chname, isbounce ? "&" : "",
				    chptr->creationtime);
			else
				sendto_serv_butone_token(cptr, me.name,
				    MSG_MODE, TOK_MODE, "%s %s+ ",
				    chptr->chname, isbounce ? "&" : "");
		return;		/* nothing to send */
		}
	}
	/* opermode for twimodesystem --sts */
#ifndef NO_OPEROVERRIDE
	if (opermode == 1)
	{
		if (modebuf[1])
			sendto_snomask(SNO_EYES,
			    "*** OperOverride -- %s (%s@%s) MODE %s %s %s",
			    sptr->name, sptr->user->username, sptr->user->realhost,
			    chptr->chname, modebuf, parabuf);

			/* Logging Implementation added by XeRXeS */
			ircd_log(LOG_OVERRIDE,"OVERRIDE: %s (%s@%s) MODE %s %s %s",
				sptr->name, sptr->user->username, sptr->user->realhost,
				chptr->chname, modebuf, parabuf);

		sendts = 0;
	}
#endif

	/* Should stop null modes */
	if (*(modebuf + 1) == '\0')
		return;
	if (IsPerson(sptr) && samode && MyClient(sptr))
	{
		sendto_serv_butone_token(NULL, me.name, MSG_GLOBOPS,
		    TOK_GLOBOPS, ":%s used SAMODE %s (%s%s%s)", sptr->name,
		    chptr->chname, modebuf, *parabuf ? " " : "", parabuf);
		sendto_failops_whoare_opers
		    ("from %s: %s used SAMODE %s (%s%s%s)", me.name, sptr->name,
		    chptr->chname, modebuf, *parabuf ? " " : "", parabuf);
		sptr = &me;
		sendts = 0;
	}

	
	sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s",
	    sptr->name, chptr->chname, modebuf, parabuf);
	if (IsServer(sptr) && sendts != -1)
		sendto_serv_butone_token(cptr, sptr->name, MSG_MODE, TOK_MODE,
		    "%s %s%s %s %lu", chptr->chname, isbounce ? "&" : "",
		    modebuf, parabuf, sendts);
	else if (samode && IsMe(sptr)) /* SAMODE is a special case: always send a TS of 0 (omitting TS==desynch) */
		sendto_serv_butone_token(cptr, sptr->name, MSG_MODE, TOK_MODE,
		    "%s %s %s 0", chptr->chname, modebuf, parabuf);
	else
		sendto_serv_butone_token(cptr, sptr->name, MSG_MODE, TOK_MODE,
		    "%s %s%s %s", chptr->chname, isbounce ? "&" : "",
		    modebuf, parabuf);
	/* tell them it's not a timestamp, in case the last param
	   ** is a number. */

	if (MyConnect(sptr))
		RunHook7(HOOKTYPE_LOCAL_CHANMODE, cptr, sptr, chptr, modebuf, parabuf, sendts, samode);
	else
		RunHook7(HOOKTYPE_REMOTE_CHANMODE, cptr, sptr, chptr, modebuf, parabuf, sendts, samode);
}
/* make_mode_str -- written by binary
 *	Reconstructs the mode string, to make it look clean.  mode_buf will
 *  contain the +x-y stuff, and the parabuf will contain the parameters.
 *  If bounce is set to 1, it will make the string it needs for a bounce.
 */
#ifdef EXTCMODE
void make_mode_str(aChannel *chptr, long oldm, Cmode_t oldem, long oldl, int pcount, 
	char pvar[MAXMODEPARAMS][MODEBUFLEN + 3], char *mode_buf, char *para_buf, char bounce)
#else
void make_mode_str(aChannel *chptr, long oldm, long oldl, int pcount, 
	char pvar[MAXMODEPARAMS][MODEBUFLEN + 3], char *mode_buf, char *para_buf, char bounce)
#endif
{

	char tmpbuf[MODEBUFLEN+3], *tmpstr;
	aCtab *tab = &cFlagTab[0];
	char *x = mode_buf;
	int  what, cnt, z;
#ifdef EXTCMODE
	int i;
#endif
	char *m;
	what = 0;

	*tmpbuf = '\0';
	*mode_buf = '\0';
	*para_buf = '\0';
	what = 0;
	/* + param-less modes */
	tab = &cFlagTab[0];
	while (tab->mode != 0x0)
	{
		if (chptr->mode.mode & tab->mode)
		{
			if (!(oldm & tab->mode))
			{
				if (what != MODE_ADD)
				{
					*x++ = bounce ? '-' : '+';
					what = MODE_ADD;
				}
				*x++ = tab->flag;
			}
		}
		tab++;
	}
#ifdef EXTCMODE
	/* + paramless extmodes... */
	for (i=0; i <= Channelmode_highest; i++)
	{
		if (!Channelmode_Table[i].flag || Channelmode_Table[i].paracount)
			continue;
		/* have it now and didn't have it before? */
		if ((chptr->mode.extmode & Channelmode_Table[i].mode) &&
		    !(oldem & Channelmode_Table[i].mode))
		{
			if (what != MODE_ADD)
			{
				*x++ = bounce ? '-' : '+';
				what = MODE_ADD;
			}
			*x++ = Channelmode_Table[i].flag;
		}
	}
#endif

	*x = '\0';
	/* - param-less modes */
	tab = &cFlagTab[0];
	while (tab->mode != 0x0)
	{
		if (!(chptr->mode.mode & tab->mode))
		{
			if (oldm & tab->mode)
			{
				if (what != MODE_DEL)
				{
					*x++ = bounce ? '+' : '-';
					what = MODE_DEL;
				}
				*x++ = tab->flag;
			}
		}
		tab++;
	}

#ifdef EXTCMODE
	/* - extmodes (both "param modes" and paramless don't have
	 * any params when unsetting...
	 */
	for (i=0; i <= Channelmode_highest; i++)
	{
		if (!Channelmode_Table[i].flag /* || Channelmode_Table[i].paracount */)
			continue;
		/* don't have it now and did have it before */
		if (!(chptr->mode.extmode & Channelmode_Table[i].mode) &&
		    (oldem & Channelmode_Table[i].mode))
		{
			if (what != MODE_DEL)
			{
				*x++ = bounce ? '+' : '-';
				what = MODE_DEL;
			}
			*x++ = Channelmode_Table[i].flag;
		}
	}
#endif

	*x = '\0';
	/* user limit */
	if (chptr->mode.limit != oldl)
	{
		if ((!bounce && chptr->mode.limit == 0) ||
		    (bounce && chptr->mode.limit != 0))
		{
			if (what != MODE_DEL)
			{
				*x++ = '-';
				what = MODE_DEL;
			}
			if (bounce)
				chptr->mode.limit = 0;	/* set it back */
			*x++ = 'l';
		}
		else
		{
			if (what != MODE_ADD)
			{
				*x++ = '+';
				what = MODE_ADD;
			}
			*x++ = 'l';
			if (bounce)
				chptr->mode.limit = oldl;	/* set it back */
			ircsprintf(para_buf, "%s%d ", para_buf, chptr->mode.limit);
		}
	}
	/* reconstruct bkov chain */
	for (cnt = 0; cnt < pcount; cnt++)
	{
		if ((*(pvar[cnt]) == '+') && what != MODE_ADD)
		{
			*x++ = bounce ? '-' : '+';
			what = MODE_ADD;
		}
		if ((*(pvar[cnt]) == '-') && what != MODE_DEL)
		{
			*x++ = bounce ? '+' : '-';
			what = MODE_DEL;
		}
		*x++ = *(pvar[cnt] + 1);
		tmpstr = &pvar[cnt][2];
		z = (MODEBUFLEN * MAXMODEPARAMS);
		m = para_buf;
		while ((*m)) { m++; }
		while ((*tmpstr) && ((m-para_buf) < z))
		{
			*m = *tmpstr; 
			m++;
			tmpstr++;
		}
		*m++ = ' ';
		*m = '\0';
	}
	if (bounce)
	{
		chptr->mode.mode = oldm;
#ifdef EXTCMODE
		chptr->mode.extmode = oldem;
#endif
	}
	z = strlen(para_buf);
	if (para_buf[z - 1] == ' ')
		para_buf[z - 1] = '\0';
	*x = '\0';
	if (*mode_buf == '\0')
	{
		*mode_buf = '+';
		mode_buf++;
		*mode_buf = '\0';
		/* Don't send empty lines. */
	}
	return;
}


/* do_mode_char
 *  processes one mode character
 *  returns 1 if it ate up a param, otherwise 0
 *	written by binary
 *  modified for Unreal by stskeeps..
 */

#define REQUIRE_PARAMETER() { if (!param || *pcount >= MAXMODEPARAMS) { retval = 0; break; } retval = 1; }

#ifdef PREFIX_AQ
#define is_xchanop(x) ((x & (CHFL_CHANOP|CHFL_CHANPROT|CHFL_CHANOWNER)))
#else
#define is_xchanop(x) ((x & CHFL_CHANOP))
#endif

int  do_mode_char(aChannel *chptr, long modetype, char modechar, char *param, 
	u_int what, aClient *cptr,
	 u_int *pcount, char pvar[MAXMODEPARAMS][MODEBUFLEN + 3], char bounce, long my_access)
{
	aCtab *tab = &cFlagTab[0];


	int  retval = 0;
	Member *member = NULL;
	Membership *membership = NULL;
	aClient *who;
	unsigned int tmp = 0;
	char tmpbuf[512], *tmpstr;
	char tc = ' ';		/* */
	int  chasing, x;
	int xxi, xyi, xzi, hascolon;
	char *xp;
	int  notsecure;
	chasing = 0;

	if ((my_access & CHFL_HALFOP) && !is_xchanop(my_access) && !IsULine(cptr)
	    && !op_can_override(cptr) && !samode_in_progress)
	{
		if (MyClient(cptr) && (modetype == MODE_HALFOP) && (what == MODE_DEL) &&
		    param && (find_client(param, NULL) == cptr))
		{
			/* halfop -h'ing him/herself */
			/* ALLOW */
		} else
		{
			/* Ugly halfop hack --sts 
			   - this allows halfops to do +b +e +v and so on */
			/* (Syzop/20040413: Allow remote halfop modes */
			if ((Halfop_mode(modetype) == FALSE) && MyClient(cptr))
			{
				int eaten = 0;
				while (tab->mode != 0x0)
				{
					if (tab->mode == modetype)
					{
						sendto_one(cptr,
						    err_str(ERR_NOTFORHALFOPS), me.name,
						    cptr->name, tab->flag);
						eaten = tab->parameters;
						break;
					}
					tab++;
				}
				return eaten;
			}
		} /* not -h self */
	}
	switch (modetype)
	{
	  case MODE_AUDITORIUM:
		  if (IsULine(cptr) || IsServer(cptr) || samode_in_progress)
			  goto auditorium_ok;
		  if (MyClient(cptr) && !is_chanowner(cptr, chptr) && !op_can_override(cptr))
		  {
			sendto_one(cptr, err_str(ERR_CHANOWNPRIVNEEDED), me.name, cptr->name,
				   chptr->chname);
			break;
		  }
		  if (op_can_override(cptr) && !is_chanowner(cptr, chptr))
		  {
		  	opermode = 1;
		  }

		auditorium_ok:
		  goto setthephuckingmode;
	  case MODE_OPERONLY:
		  if (MyClient(cptr) && !IsAnOper(cptr))
		  {
			sendto_one(cptr, err_str(ERR_NOPRIVILEGES), me.name, cptr->name);
			break;
		  }
		  goto setthephuckingmode;
	  case MODE_ADMONLY:
		  if (!IsSkoAdmin(cptr) && !IsServer(cptr)
		      && !IsULine(cptr))
		  {
			sendto_one(cptr, err_str(ERR_NOPRIVILEGES), me.name, cptr->name);
			break;
		  }
		  goto setthephuckingmode;
	  case MODE_RGSTR:
		  if (!IsServer(cptr) && !IsULine(cptr))
		  {
			sendto_one(cptr, err_str(ERR_ONLYSERVERSCANCHANGE), me.name, cptr->name,
				   chptr->chname);
			break;
		  }
		  goto setthephuckingmode;
	  case MODE_SECRET:
	  case MODE_PRIVATE:
	  case MODE_MODERATED:
	  case MODE_TOPICLIMIT:
	  case MODE_NOPRIVMSGS:
	  case MODE_RGSTRONLY:
	  case MODE_MODREG:
	  case MODE_NOCOLOR:
	  case MODE_NOKICKS:
	  case MODE_STRIP:
	  	goto setthephuckingmode;

	  case MODE_INVITEONLY:
		if (what == MODE_DEL && modetype == MODE_INVITEONLY && (chptr->mode.mode & MODE_NOKNOCK))
			chptr->mode.mode &= ~MODE_NOKNOCK;
		goto setthephuckingmode;
	  case MODE_NOKNOCK:
		if (what == MODE_ADD && modetype == MODE_NOKNOCK && !(chptr->mode.mode & MODE_INVITEONLY))
		{
			sendto_one(cptr, err_str(ERR_CANNOTCHANGECHANMODE), 
				me.name, cptr->name, 'K', "+i must be set");
			break;
		}
		goto setthephuckingmode;
	  case MODE_ONLYSECURE:
	  case MODE_NOCTCP:
	  case MODE_NONICKCHANGE:
	  case MODE_NOINVITE:
		setthephuckingmode:
		  retval = 0;
		  if (what == MODE_ADD) {
			  /* +sp bugfix.. (by JK/Luke)*/
		 	 if (modetype == MODE_SECRET
			      && (chptr->mode.mode & MODE_PRIVATE))
				  chptr->mode.mode &= ~MODE_PRIVATE;
			  if (modetype == MODE_PRIVATE
			      && (chptr->mode.mode & MODE_SECRET))
				  chptr->mode.mode &= ~MODE_SECRET;
			  if (modetype == MODE_NOCOLOR
			      && (chptr->mode.mode & MODE_STRIP))
				  chptr->mode.mode &= ~MODE_STRIP;
			  if (modetype == MODE_STRIP
			      && (chptr->mode.mode & MODE_NOCOLOR))
				  chptr->mode.mode &= ~MODE_NOCOLOR;
			  chptr->mode.mode |= modetype;
		  }
		  else
		  {
			  chptr->mode.mode &= ~modetype;
#ifdef NEWCHFLOODPROT
			  /* reset joinflood on -i, reset msgflood on -m, etc.. */
			  if (chptr->mode.floodprot)
			  {
				switch(modetype)
				{
				case MODE_NOCTCP:
					chptr->mode.floodprot->c[FLD_CTCP] = 0;
					chanfloodtimer_del(chptr, 'C', MODE_NOCTCP);
					break;
				case MODE_NONICKCHANGE:
					chptr->mode.floodprot->c[FLD_NICK] = 0;
					chanfloodtimer_del(chptr, 'N', MODE_NONICKCHANGE);
					break;
				case MODE_MODERATED:
					chptr->mode.floodprot->c[FLD_MSG] = 0;
					chptr->mode.floodprot->c[FLD_CTCP] = 0;
					chanfloodtimer_del(chptr, 'm', MODE_MODERATED);
					break;
				case MODE_NOKNOCK:
					chptr->mode.floodprot->c[FLD_KNOCK] = 0;
					chanfloodtimer_del(chptr, 'K', MODE_NOKNOCK);
					break;
				case MODE_INVITEONLY:
					chptr->mode.floodprot->c[FLD_JOIN] = 0;
					chanfloodtimer_del(chptr, 'i', MODE_INVITEONLY);
					break;
				case MODE_MODREG:
					chptr->mode.floodprot->c[FLD_MSG] = 0;
					chptr->mode.floodprot->c[FLD_CTCP] = 0;
					chanfloodtimer_del(chptr, 'M', MODE_MODREG);
					break;
				case MODE_RGSTRONLY:
					chptr->mode.floodprot->c[FLD_JOIN] = 0;
					chanfloodtimer_del(chptr, 'R', MODE_RGSTRONLY);
					break;
				default:
					break;
				}
			  }
#endif
		  }
		  break;

/* do pro-opping here (popping) */
	  case MODE_CHANOWNER:
		  REQUIRE_PARAMETER()
		  if (!IsULine(cptr) && !IsServer(cptr) && !is_chanowner(cptr, chptr) && !samode_in_progress)
		  {
		  	  if (MyClient(cptr) && !op_can_override(cptr))
		  	  {
		  	  	sendto_one(cptr, err_str(ERR_CHANOWNPRIVNEEDED), me.name, cptr->name, chptr->chname);
		  	  	break;
		  	  }
			  if (IsOper(cptr))
			  {
				if (!is_halfop(cptr, chptr)) /* htrig will take care of halfop override notices */
				   opermode = 1;
			  }
		  }
	  case MODE_CHANPROT:
		  REQUIRE_PARAMETER()
		  /* not uline, not server, not chanowner, not an samode, not -a'ing yourself... */
		  if (!IsULine(cptr) && !IsServer(cptr) && !is_chanowner(cptr, chptr) && !samode_in_progress &&
		      !(param && (what == MODE_DEL) && (find_client(param, NULL) == cptr)))
		  {
		  	  if (MyClient(cptr) && !op_can_override(cptr))
		  	  {
		  	  	sendto_one(cptr, err_str(ERR_CHANOWNPRIVNEEDED), me.name, cptr->name, chptr->chname);
		  	  	break;
		  	  }
			  if (IsOper(cptr))
			  {
				if (!is_halfop(cptr, chptr)) /* htrig will take care of halfop override notices */
				   opermode = 1;
			  }
		  }
		 

	  case MODE_HALFOP:
	  case MODE_CHANOP:
	  case MODE_VOICE:
		  REQUIRE_PARAMETER()
		  if (!(who = find_chasing(cptr, param, &chasing)))
			  break;
		  if (!who->user)
		  	break;
   		  /* codemastr: your patch is a good idea here, but look at the
   		     member->flags stuff longer down. this caused segfaults */
   		  if (!(membership = find_membership_link(who->user->channel, chptr)))
		  {
			  sendto_one(cptr, err_str(ERR_USERNOTINCHANNEL),
			      me.name, cptr->name, who->name, chptr->chname);
			  break;
		  }
		  member = find_member_link(chptr->members, who);
		  if (!member)
		  {
		  	/* should never happen */
		  	sendto_realops("crap! find_membership_link && !find_member_link !!. Report to unreal team");
		  	break;
		  }
		  /* we make the rules, we bend the rules */
		  if (IsServer(cptr) || IsULine(cptr))
			  goto breaktherules;
		
		  /* Services are special! */
		  if (IsServices(member->cptr) && MyClient(cptr) && !IsNetAdmin(cptr) && (what == MODE_DEL))
		  {
			char errbuf[NICKLEN+50];
			ircsprintf(errbuf, "%s is a network service", member->cptr->name);
			sendto_one(cptr, err_str(ERR_CANNOTCHANGECHANMODE), me.name, cptr->name,
				   modechar, errbuf);
			break;
		  }

		  /* This check not only prevents unprivileged users from doing a -q on chanowners,
		   * it also protects against -o/-h/-v on them.
		   */
		  if (is_chanowner(member->cptr, chptr)
		      && member->cptr != cptr
		      && !is_chanowner(cptr, chptr) && !IsServer(cptr)
		      && !IsULine(cptr) && !opermode && !samode_in_progress && (what == MODE_DEL))
		  {
			  if (MyClient(cptr))
			  {
			  	/* Need this !op_can_override() here again, even with the !opermode
			  	 * check a few lines up, all due to halfops. -- Syzop
			  	 */
				if (!op_can_override(cptr))
				{
					char errbuf[NICKLEN+30];
					ircsprintf(errbuf, "%s is a channel owner", member->cptr->name);
					sendto_one(cptr, err_str(ERR_CANNOTCHANGECHANMODE), me.name, cptr->name,
					   modechar, errbuf);
					break;
				}
			  } else
			  if (IsOper(cptr))
			      opermode = 1;
		  }

		  /* This check not only prevents unprivileged users from doing a -a on chanadmins,
		   * it also protects against -o/-h/-v on them.
		   */
		  if (is_chanprot(member->cptr, chptr)
		      && member->cptr != cptr
		      && !is_chanowner(cptr, chptr) && !IsServer(cptr) && !opermode && !samode_in_progress
		      && modetype != MODE_CHANOWNER && (what == MODE_DEL))
		  {
			  if (MyClient(cptr))
			  {
			  	/* Need this !op_can_override() here again, even with the !opermode
			  	 * check a few lines up, all due to halfops. -- Syzop
			  	 */
			  	if (!op_can_override(cptr))
			  	{
					char errbuf[NICKLEN+30];
					ircsprintf(errbuf, "%s is a channel admin", member->cptr->name);
					sendto_one(cptr, err_str(ERR_CANNOTCHANGECHANMODE), me.name, cptr->name,
					   modechar, errbuf);
					break;
				}
			  } else
			  if (IsOper(cptr))
			      opermode = 1;
		  }
		breaktherules:
		  tmp = member->flags;
		  if (what == MODE_ADD)
			  member->flags |= modetype;
		  else
			  member->flags &= ~modetype;
		  if ((tmp == member->flags) && (bounce || !IsULine(cptr)))
			  break;
		  /* It's easier to undo the mode here instead of later
		   * when you call make_mode_str for a bounce string.
		   * Why set it if it will be instantly removed?
		   * Besides, pvar keeps a log of it. */
		  if (bounce)
			  member->flags = tmp;
		  if (modetype == MODE_CHANOWNER)
			  tc = 'q';
		  if (modetype == MODE_CHANPROT)
			  tc = 'a';
		  if (modetype == MODE_CHANOP)
			  tc = 'o';
		  if (modetype == MODE_HALFOP)
			  tc = 'h';
		  if (modetype == MODE_VOICE)
			  tc = 'v';
		  /* Make sure membership->flags and member->flags is the same */
		  membership->flags = member->flags;
		  (void)ircsprintf(pvar[*pcount], "%c%c%s",
		      what == MODE_ADD ? '+' : '-', tc, who->name);
		  (*pcount)++;
		  break;
	  case MODE_LIMIT:
		  if (what == MODE_ADD)
		  {
			  if (!param)
			  {
				  retval = 0;
				  break;
			  }
			  retval = 1;
			  tmp = atoi(param);
			  if (chptr->mode.limit == tmp)
				  break;
			  chptr->mode.limit = tmp;
		  }
		  else
		  {
			  retval = 0;
			  if (!chptr->mode.limit)
				  break;
			  chptr->mode.limit = 0;
		  }
		  break;
	  case MODE_KEY:
		  if (!param || *pcount >= MAXMODEPARAMS)
		  {
			  retval = 0;
			  break;
		  }
		  retval = 1;
		  for (x = 0; x < *pcount; x++)
		  {
			  if (pvar[x][1] == 'k')
			  {	/* don't allow user to change key
				 * more than once per command. */
				  retval = 0;
				  break;
			  }
		  }
		  if (retval == 0)	/* you can't break a case from loop */
			  break;
		  if (what == MODE_ADD)
		  {
			  if (!bounce) {	/* don't do the mode at all. */
				  char *tmp;
				  if ((tmp = strchr(param, ' ')))
					*tmp = '\0';
				  if ((tmp = strchr(param, ':')))
					*tmp = '\0';
				  if ((tmp = strchr(param, ',')))
					*tmp = '\0';
				  if (*param == '\0')
					break;
				  if (strlen(param) > KEYLEN)
				    param[KEYLEN] = '\0';
				  if (!strcmp(chptr->mode.key, param))
					break;
				  strncpyzt(chptr->mode.key, param,
				      sizeof(chptr->mode.key));
			  }
			  tmpstr = param;
		  }
		  else
		  {
			  if (!*chptr->mode.key)
				  break;	/* no change */
			  strncpyzt(tmpbuf, chptr->mode.key, sizeof(tmpbuf));
			  tmpstr = tmpbuf;
			  if (!bounce)
				  strcpy(chptr->mode.key, "");
		  }
		  retval = 1;

		  (void)ircsprintf(pvar[*pcount], "%ck%s",
		      what == MODE_ADD ? '+' : '-', tmpstr);
		  (*pcount)++;
		  break;

	  case MODE_BAN:
		  if (!param || *pcount >= MAXMODEPARAMS)
		  {
			  retval = 0;
			  break;
		  }
		  retval = 1;
		  tmpstr = clean_ban_mask(param, what, cptr);
		  if (BadPtr(tmpstr))
		      break; /* ignore ban, but eat param */
		  if ((tmpstr[0] == '~') && MyClient(cptr) && !bounce)
		  {
		      /* extban: check access if needed */
		      Extban *p = findmod_by_bantype(tmpstr[1]);
		      if (p && p->is_ok)
		      {
			if (!p->is_ok(cptr, chptr, tmpstr, EXBCHK_ACCESS, what, EXBTYPE_BAN))
		        {
		            if (IsAnOper(cptr))
		            {
		                /* TODO: send operoverride notice */
  		            } else {
		                p->is_ok(cptr, chptr, tmpstr, EXBCHK_ACCESS_ERR, what, EXBTYPE_BAN);
		                break;
		            }
		        }
			if (!p->is_ok(cptr, chptr, tmpstr, EXBCHK_PARAM, what, EXBTYPE_BAN))
		            break;
		     }
		  }
		  /* For bounce, we don't really need to worry whether
		   * or not it exists on our server.  We'll just always
		   * bounce it. */
		  if (!bounce &&
		      ((what == MODE_ADD && add_listmode(&chptr->banlist, cptr, chptr, tmpstr))
		      || (what == MODE_DEL && del_listmode(&chptr->banlist, chptr, tmpstr))))
			  break;	/* already exists */
		  (void)ircsprintf(pvar[*pcount], "%cb%s",
		      what == MODE_ADD ? '+' : '-', tmpstr);
		  (*pcount)++;
		  break;
	  case MODE_EXCEPT:
		  if (!param || *pcount >= MAXMODEPARAMS)
		  {
			  retval = 0;
			  break;
		  }
		  retval = 1;
		  tmpstr = clean_ban_mask(param, what, cptr);
		  if (BadPtr(tmpstr))
		     break; /* ignore except, but eat param */
		  if ((tmpstr[0] == '~') && MyClient(cptr) && !bounce)
		  {
		      /* extban: check access if needed */
		      Extban *p = findmod_by_bantype(tmpstr[1]);
		      if (p && p->is_ok)
       		      {
			 if (!p->is_ok(cptr, chptr, tmpstr, EXBCHK_ACCESS, what, EXBTYPE_EXCEPT))
		         {
		            if (IsAnOper(cptr))
		            {
		                /* TODO: send operoverride notice */
		            } else {
		                p->is_ok(cptr, chptr, tmpstr, EXBCHK_ACCESS_ERR, what, EXBTYPE_EXCEPT);
		                break;
		            }
		        }
			if (!p->is_ok(cptr, chptr, tmpstr, EXBCHK_PARAM, what, EXBTYPE_EXCEPT))
		            break;
		     }
		  }
		  /* For bounce, we don't really need to worry whether
		   * or not it exists on our server.  We'll just always
		   * bounce it. */
		  if (!bounce &&
		      ((what == MODE_ADD && add_listmode(&chptr->exlist, cptr, chptr, tmpstr))
		      || (what == MODE_DEL && del_listmode(&chptr->exlist, chptr, tmpstr))))
			  break;	/* already exists */
		  (void)ircsprintf(pvar[*pcount], "%ce%s",
		      what == MODE_ADD ? '+' : '-', tmpstr);
		  (*pcount)++;
		  break;
	  case MODE_INVEX:
		  if (!param || *pcount >= MAXMODEPARAMS)
		  {
			  retval = 0;
			  break;
		  }
		  retval = 1;
		  tmpstr = clean_ban_mask(param, what, cptr);
		  if (BadPtr(tmpstr))
		     break; /* ignore except, but eat param */
		  if ((tmpstr[0] == '~') && MyClient(cptr) && !bounce)
		  {
		      /* extban: check access if needed */
		      Extban *p = findmod_by_bantype(tmpstr[1]);
		      if (p)
       		      {
       		        if (!(p->options & EXTBOPT_INVEX))
				break; /* this extended ban type does not support INVEX */
       		        
			if (p->is_ok && !p->is_ok(cptr, chptr, tmpstr, EXBCHK_ACCESS, what, EXBTYPE_EXCEPT))
		        {
		            if (IsAnOper(cptr))
		            {
		                /* TODO: send operoverride notice */
		            } else {
		                p->is_ok(cptr, chptr, tmpstr, EXBCHK_ACCESS_ERR, what, EXBTYPE_EXCEPT);
		                break;
		            }
		        }
			if (p->is_ok && !p->is_ok(cptr, chptr, tmpstr, EXBCHK_PARAM, what, EXBTYPE_EXCEPT))
		            break;
		     }
		  }
		  /* For bounce, we don't really need to worry whether
		   * or not it exists on our server.  We'll just always
		   * bounce it. */
		  if (!bounce &&
		      ((what == MODE_ADD && add_listmode(&chptr->invexlist, cptr, chptr, tmpstr))
		      || (what == MODE_DEL && del_listmode(&chptr->invexlist, chptr, tmpstr))))
			  break;	/* already exists */
		  (void)ircsprintf(pvar[*pcount], "%cI%s",
		      what == MODE_ADD ? '+' : '-', tmpstr);
		  (*pcount)++;
		  break;
	  case MODE_LINK:
		  if (IsULine(cptr) || IsServer(cptr))
		  {
			  goto linkok;
		  }

		  if (MyClient(cptr) && !is_chanowner(cptr, chptr) && !op_can_override(cptr) && !samode_in_progress)
		  {
			sendto_one(cptr, err_str(ERR_CHANOWNPRIVNEEDED), me.name, cptr->name,
				   chptr->chname);
			break;
		  }

		linkok:
		  retval = 1;
		  for (x = 0; x < *pcount; x++)
		  {
			  if (pvar[x][1] == 'L')
			  {	/* don't allow user to change link
				 * more than once per command. */
				  retval = 0;
				  break;
			  }
		  }
		  if (retval == 0)	/* you can't break a case from loop */
			  break;
		  if (what == MODE_ADD)
		  {
		      char *tmp;
			  if (!param || *pcount >= MAXMODEPARAMS)
			  {
				  retval = 0;
				  break;
			  }
			  if (strchr(param, ','))
				  break;
			  if (!IsChannelName(param))
			  {
				  if (MyClient(cptr))
					  sendto_one(cptr,
					      err_str(ERR_NOSUCHCHANNEL),
					      me.name, cptr->name, param);
				  break;
			  }
			  /* Now make it a clean channelname.. This has to be done before all checking
			   * because it could have been changed later to something disallowed (like
			   * self-linking). -- Syzop
			   */
			  strlcpy(tmpbuf, param, CHANNELLEN+1);
			  clean_channelname(tmpbuf);
			  /* don't allow linking to local chans either.. */
			  if ((tmp = strchr(tmpbuf, ':')))
				*tmp = '\0';

			  if (!stricmp(tmpbuf, chptr->mode.link))
				break;
			  if (!stricmp(tmpbuf, chptr->chname))
			  {
				if (MyClient(cptr))
					sendto_one(cptr, err_str(ERR_CANNOTCHANGECHANMODE), 
						   me.name, cptr->name, 'L', 
					    	   "a channel cannot be linked to itself");
				break;
			  }
			  if (!bounce)	/* don't do the mode at all. */
			  {
				  strncpyzt(chptr->mode.link, tmpbuf,
				      sizeof(chptr->mode.link));
			      tmpstr = tmpbuf;
			  } else
			      tmpstr = param; /* Use the original value if bounce?? -- Syzop */
		  }
		  else
		  {
			  if (!*chptr->mode.link)
				  break;	/* no change */
			  strncpyzt(tmpbuf, chptr->mode.link, sizeof(tmpbuf));
			  tmpstr = tmpbuf;
			  if (!bounce)
			  {
				  strcpy(chptr->mode.link, "");
			  }
		  }
		  if (!IsULine(cptr) && IsPerson(cptr) && op_can_override(cptr) && !is_chanowner(cptr, chptr))
		  {
		  	opermode = 1;
		  }
		  retval = 1;

		  (void)ircsprintf(pvar[*pcount], "%cL%s",
		      what == MODE_ADD ? '+' : '-', tmpstr);
		  (*pcount)++;
		  break;
	  case MODE_FLOODLIMIT:
		  retval = 1;
		  for (x = 0; x < *pcount; x++)
		  {
			  if (pvar[x][1] == 'f')
			  {	/* don't allow user to change flood
				 * more than once per command. */
				  retval = 0;
				  break;
			  }
		  }
		  if (retval == 0)	/* you can't break a case from loop */
			  break;
#ifndef NEWCHFLOODPROT
		  if (what == MODE_ADD)
		  {
			  if (!bounce)	/* don't do the mode at all. */
			  {
				  if (!param || *pcount >= MAXMODEPARAMS)
				  {
					  retval = 0;
					  break;
				  }

				  /* like 1:1 and if its less than 3 chars then ahem.. */
				  if (strlen(param) < 3)
				  {
					  break;
				  }
				  /* may not contain other chars 
				     than 0123456789: & NULL */
				  hascolon = 0;
				  for (xp = param; *xp; xp++)
				  {
					  if (*xp == ':')
						hascolon++;
					  /* fast alpha check */
					  if (((*xp < '0') || (*xp > '9'))
					      && (*xp != ':')
					      && (*xp != '*'))
						goto break_flood;
					  /* uh oh, not the first char */
					  if (*xp == '*' && (xp != param))
						goto break_flood;
				  }
				  /* We can avoid 2 strchr() and a strrchr() like this
				   * it should be much faster. -- codemastr
				   */
				  if (hascolon != 1)
					break;
				  if (*param == '*')
				  {
					  xzi = 1;
					  //                      chptr->mode.kmode = 1;
				  }
				  else
				  {
					  xzi = 0;

					  //                   chptr->mode.kmode = 0;
				  }
				  xp = index(param, ':');
				  *xp = '\0';
				  xxi =
				      atoi((*param ==
				      '*' ? (param + 1) : param));
				  xp++;
				  xyi = atoi(xp);
				  if (xxi > 500 || xyi > 500)
					break;
				  xp--;
				  *xp = ':';
				  if ((xxi == 0) || (xyi == 0))
					  break;
				  if ((chptr->mode.msgs == xxi)
				      && (chptr->mode.per == xyi)
				      && (chptr->mode.kmode == xzi))
					  break;
				  chptr->mode.msgs = xxi;
				  chptr->mode.per = xyi;
				  chptr->mode.kmode = xzi;
			  }
			  tmpstr = param;
			  retval = 1;
		  }
		  else
		  {
			  if (!chptr->mode.msgs || !chptr->mode.per)
				  break;	/* no change */
			  ircsprintf(tmpbuf,
			      (chptr->mode.kmode > 0 ? "*%i:%i" : "%i:%i"),
			      chptr->mode.msgs, chptr->mode.per);
			  tmpstr = tmpbuf;
			  if (!bounce)
			  {
				  chptr->mode.msgs = chptr->mode.per =
				      chptr->mode.kmode = 0;
			  }
			  retval = 0;
		  }
#else
		/* NEW */
		if (what == MODE_ADD)
		{
			if (!bounce)	/* don't do the mode at all. */
			{
				ChanFloodProt newf;
				memset(&newf, 0, sizeof(newf));

				if (!param || *pcount >= MAXMODEPARAMS)
				{
					retval = 0;
					break;
				}

				/* old +f was like +f 10:5 or +f *10:5
				 * new is +f [5c,30j,10t#b]:15
				 * +f 10:5  --> +f [10t]:5
				 * +f *10:5 --> +f [10t#b]:5
				 */
				if (param[0] != '[')
				{
					/* <<OLD +f>> */
				  /* like 1:1 and if its less than 3 chars then ahem.. */
				  if (strlen(param) < 3)
				  {
					  break;
				  }
				  /* may not contain other chars 
				     than 0123456789: & NULL */
				  hascolon = 0;
				  for (xp = param; *xp; xp++)
				  {
					  if (*xp == ':')
						hascolon++;
					  /* fast alpha check */
					  if (((*xp < '0') || (*xp > '9'))
					      && (*xp != ':')
					      && (*xp != '*'))
						goto break_flood;
					  /* uh oh, not the first char */
					  if (*xp == '*' && (xp != param))
						goto break_flood;
				  }
				  /* We can avoid 2 strchr() and a strrchr() like this
				   * it should be much faster. -- codemastr
				   */
				  if (hascolon != 1)
					break;
				  if (*param == '*')
				  {
					  xzi = 1;
					  //                      chptr->mode.kmode = 1;
				  }
				  else
				  {
					  xzi = 0;

					  //                   chptr->mode.kmode = 0;
				  }
				  xp = index(param, ':');
				  *xp = '\0';
				  xxi =
				      atoi((*param ==
				      '*' ? (param + 1) : param));
				  xp++;
				  xyi = atoi(xp);
				  if (xxi > 500 || xyi > 500)
					break;
				  xp--;
				  *xp = ':';
				  if ((xxi == 0) || (xyi == 0))
					  break;

				  /* ok, we passed */
				  newf.l[FLD_TEXT] = xxi;
				  newf.per = xyi;
				  if (xzi == 1)
				      newf.a[FLD_TEXT] = 'b';
				} else {
					/* NEW +F */
					char xbuf[256], c, a, *p, *p2, *x = xbuf+1;
					int v;
					unsigned short warnings = 0, breakit;
					unsigned char r;

					/* '['<number><1 letter>[optional: '#'+1 letter],[next..]']'':'<number> */
					strlcpy(xbuf, param, sizeof(xbuf));
					p2 = strchr(xbuf+1, ']');
					if (!p2)
						break;
					*p2 = '\0';
					if (*(p2+1) != ':')
						break;
					breakit = 0;
					for (x = strtok(xbuf+1, ","); x; x = strtok(NULL, ","))
					{
						/* <number><1 letter>[optional: '#'+1 letter] */
						p = x;
						while(isdigit(*p)) { p++; }
						if ((*p == '\0') ||
						    !((*p == 'c') || (*p == 'j') || (*p == 'k') ||
						      (*p == 'm') || (*p == 'n') || (*p == 't')))
						{
							if (MyClient(cptr) && *p && (warnings++ < 3))
								sendto_one(cptr, ":%s NOTICE %s :warning: channelmode +f: floodtype '%c' unknown, ignored.",
									me.name, cptr->name, *p);
							continue; /* continue instead of break for forward compatability. */
						}
						c = *p;
						*p = '\0';
						v = atoi(x);
						if ((v < 1) || (v > 999)) /* out of range... */
						{
							if (MyClient(cptr))
							{
								sendto_one(cptr, err_str(ERR_CANNOTCHANGECHANMODE),
									   me.name, cptr->name, 
									   'f', "value should be from 1-999");
								breakit = 1;
								break;
							} else
								continue; /* just ignore for remote servers */
						}
						p++;
						a = '\0';
						r = MyClient(cptr) ? MODEF_DEFAULT_UNSETTIME : 0;
						if (*p != '\0')
						{
							if (*p == '#')
							{
								p++;
								a = *p;
								p++;
								if (*p != '\0')
								{
									int tv;
									tv = atoi(p);
									if (tv <= 0)
										tv = 0; /* (ignored) */
									if (tv > (MyClient(cptr) ? MODEF_MAX_UNSETTIME : 255))
										tv = (MyClient(cptr) ? MODEF_MAX_UNSETTIME : 255); /* set to max */
									r = (unsigned char)tv;
								}
							}
						}

						switch(c)
						{
							case 'c':
								newf.l[FLD_CTCP] = v;
								if ((a == 'm') || (a == 'M'))
									newf.a[FLD_CTCP] = a;
								else
									newf.a[FLD_CTCP] = 'C';
								newf.r[FLD_CTCP] = r;
								break;
							case 'j':
								newf.l[FLD_JOIN] = v;
								if (a == 'R')
									newf.a[FLD_JOIN] = a;
								else
									newf.a[FLD_JOIN] = 'i';
								newf.r[FLD_JOIN] = r;
								break;
							case 'k':
								newf.l[FLD_KNOCK] = v;
								newf.a[FLD_KNOCK] = 'K';
								newf.r[FLD_KNOCK] = r;
								break;
							case 'm':
								newf.l[FLD_MSG] = v;
								if (a == 'M')
									newf.a[FLD_MSG] = a;
								else
									newf.a[FLD_MSG] = 'm';
								newf.r[FLD_MSG] = r;
								break;
							case 'n':
								newf.l[FLD_NICK] = v;
								newf.a[FLD_NICK] = 'N';
								newf.r[FLD_NICK] = r;
								break;
							case 't':
								newf.l[FLD_TEXT] = v;
								if (a == 'b')
									newf.a[FLD_TEXT] = a;
								/** newf.r[FLD_TEXT] ** not supported */
								break;
							default:
								breakit=1;
								break;
						}
						if (breakit)
							break;
					} /* for */
					if (breakit)
						break;
					/* parse 'per' */
					p2++;
					if (*p2 != ':')
						break;
					p2++;
					if (!*p2)
						break;
					v = atoi(p2);
					if ((v < 1) || (v > 999)) /* 'per' out of range */
					{
						if (MyClient(cptr))
							sendto_one(cptr, err_str(ERR_CANNOTCHANGECHANMODE), 
								   me.name, cptr->name, 'f', 
								   "time range should be 1-999");
						break;
					}
					newf.per = v;
					
					/* Is anything turned on? (to stop things like '+f []:15' */
					breakit = 1;
					for (v=0; v < NUMFLD; v++)
						if (newf.l[v])
							breakit=0;
					if (breakit)
						break;
					
				} /* if param[0] == '[' */ 

				if (chptr->mode.floodprot &&
				    !memcmp(chptr->mode.floodprot, &newf, sizeof(ChanFloodProt)))
					break; /* They are identical */

				/* Good.. store the mode (and alloc if needed) */
				if (!chptr->mode.floodprot)
					chptr->mode.floodprot = MyMalloc(sizeof(ChanFloodProt));
				memcpy(chptr->mode.floodprot, &newf, sizeof(ChanFloodProt));
				strcpy(tmpbuf, channel_modef_string(chptr->mode.floodprot));
				tmpstr = tmpbuf;
			} else {
				/* bounce... */
				tmpstr = param;
			}
			retval = 1;
		} else
		{ /* MODE_DEL */
			if (!chptr->mode.floodprot)
				break; /* no change */
			if (!bounce)
			{
				strcpy(tmpbuf, channel_modef_string(chptr->mode.floodprot));
				tmpstr = tmpbuf;
				free(chptr->mode.floodprot);
				chptr->mode.floodprot = NULL;
				chanfloodtimer_stopchantimers(chptr);
			} else {
				/* bounce.. */
				tmpstr = param;
			}
			retval = 1;
		}
#endif

		  (void)ircsprintf(pvar[*pcount], "%cf%s",
		      what == MODE_ADD ? '+' : '-', tmpstr);
		  (*pcount)++;
		  break_flood:
		  break;
	}
	return retval;
}

#ifdef EXTCMODE
/** Check access and if granted, set the extended chanmode to the requested value in memory.
  * note: if bounce is requested then the mode will not be set.
  * @returns amount of params eaten (0 or 1)
  */
int do_extmode_char(aChannel *chptr, int modeindex, char *param, u_int what,
                    aClient *cptr, u_int *pcount, char pvar[MAXMODEPARAMS][MODEBUFLEN + 3],
                    char bounce)
{
int paracnt = (what == MODE_ADD) ? Channelmode_Table[modeindex].paracount : 0;
int x;

	/* Expected a param and it isn't there? */
	if (paracnt && (!param || (*pcount >= MAXMODEPARAMS)))
		return 0;

	/* Prevent remote users from setting local channel modes */
	if ((Channelmode_Table[modeindex].local) && !MyClient(cptr))
		return paracnt;

	if (MyClient(cptr))
	{
		x = Channelmode_Table[modeindex].is_ok(cptr, chptr, param, EXCHK_ACCESS, what);
		if ((x == EX_ALWAYS_DENY) ||
		    ((x == EX_DENY) && !op_can_override(cptr) && !samode_in_progress))
		{
			Channelmode_Table[modeindex].is_ok(cptr, chptr, param, EXCHK_ACCESS_ERR, what);
			return paracnt; /* Denied & error msg sent */
		}
		if (x == EX_DENY)
			opermode = 1; /* override in progress... */
	} else {
		/* remote user: we only need to check if we need to generate an operoverride msg */
		if (!IsULine(cptr) && IsPerson(cptr) && op_can_override(cptr) &&
		    (Channelmode_Table[modeindex].is_ok(cptr, chptr, param, EXCHK_ACCESS, what) != EX_ALLOW))
			opermode = 1; /* override in progress... */
	}

	/* Check for multiple changes in 1 command (like +y-y+y 1 2, or +yy 1 2). */
	for (x = 0; x < *pcount; x++)
	{
		if (pvar[x][1] == Channelmode_Table[modeindex].flag)
		{
			/* this is different than the old chanmode system, coz:
			 * "mode #chan +kkL #a #b #c" will get "+kL #a #b" which is wrong :p.
			 * we do eat the parameter. -- Syzop
			 */
			return paracnt;
		}
	}

	/* w00t... a parameter mode */
	if (Channelmode_Table[modeindex].paracount)
	{
		if (what == MODE_DEL)
		{
			if (!(chptr->mode.extmode & Channelmode_Table[modeindex].mode))
				return paracnt; /* There's nothing to remove! */
			/* del means any parameter is ok, the one-who-is-set will be used */
			ircsprintf(pvar[*pcount], "-%c", Channelmode_Table[modeindex].flag);
		} else {
			/* add: is the parameter ok? */
			if (Channelmode_Table[modeindex].is_ok(cptr, chptr, param, EXCHK_PARAM, what) == FALSE)
				return paracnt;
			/* is it already set at the same value? if so, ignore it. */
			if (chptr->mode.extmode & Channelmode_Table[modeindex].mode)
			{
				char *p, *p2;
				p = Channelmode_Table[modeindex].get_param(extcmode_get_struct(chptr->mode.extmodeparam,Channelmode_Table[modeindex].flag));
				p2 = Channelmode_Table[modeindex].conv_param(param);
				if (p && p2 && !strcmp(p, p2))
					return paracnt; /* ignore... */
			}
				ircsprintf(pvar[*pcount], "+%c%s",
					Channelmode_Table[modeindex].flag, Channelmode_Table[modeindex].conv_param(param));
			(*pcount)++;
		}
	}

	if (bounce) /* bounce here means: only check access and return return value */
		return paracnt;
	
	if (what == MODE_ADD)
	{	/* + */
		chptr->mode.extmode |= Channelmode_Table[modeindex].mode;
		if (Channelmode_Table[modeindex].paracount)
		{
			CmodeParam *p = extcmode_get_struct(chptr->mode.extmodeparam, Channelmode_Table[modeindex].flag);
			CmodeParam *r;
			r = Channelmode_Table[modeindex].put_param(p, param);
			if (r != p)
				AddListItem(r, chptr->mode.extmodeparam);
		}
	} else
	{	/* - */
		chptr->mode.extmode &= ~(Channelmode_Table[modeindex].mode);
		if (Channelmode_Table[modeindex].paracount)
		{
			CmodeParam *p = extcmode_get_struct(chptr->mode.extmodeparam, Channelmode_Table[modeindex].flag);
			if (p)
			{
				DelListItem(p, chptr->mode.extmodeparam);
				Channelmode_Table[modeindex].free_param(p);
			}
		}
	}
	return paracnt;
}
#endif /* EXTCMODE */

/*
 * ListBits(bitvalue, bitlength);
 * written by Stskeeps
*/
#ifdef DEVELOP
char *ListBits(long bits, long length)
{
	char *bitstr, *p;
	long l, y;
	y = 1;
	bitstr = (char *)MyMalloc(length + 1);
	p = bitstr;
	for (l = 1; l <= length; l++)
	{
		if (bits & y)
			*p = '1';
		else
			*p = '0';
		p++;
		y = y + y;
	}
	*p = '\0';
	return (bitstr);
}
#endif


/* set_mode
 *	written by binary
 */
DLLFUNC void _set_mode(aChannel *chptr, aClient *cptr, int parc, char *parv[], u_int *pcount, 
	char pvar[MAXMODEPARAMS][MODEBUFLEN + 3], int bounce)
{
	char *curchr;
	u_int what = MODE_ADD;
	long modetype = 0;
	int  paracount = 1;
#ifdef DEVELOP
	char *tmpo = NULL;
#endif
	aCtab *tab = &cFlagTab[0];
	aCtab foundat;
	int  found = 0;
	unsigned int htrig = 0;
	long oldm, oldl;
	int checkrestr = 0, warnrestr = 1;
#ifdef EXTCMODE
	int extm = 1000000; /* (default value not used but stops gcc from complaining) */
	Cmode_t oldem;
#endif
	long my_access;
	paracount = 1;
	*pcount = 0;

	oldm = chptr->mode.mode;
	oldl = chptr->mode.limit;
#ifdef EXTCMODE
	oldem = chptr->mode.extmode;
#endif
	if (RESTRICT_CHANNELMODES && MyClient(cptr) && !IsAnOper(cptr) && !IsServer(cptr)) /* "cache" this */
		checkrestr = 1;

	/* Set access to the status we have */
	my_access = IsPerson(cptr) ? get_access(cptr, chptr) : 0;

	for (curchr = parv[0]; *curchr; curchr++)
	{
		switch (*curchr)
		{
		  case '+':
			  what = MODE_ADD;
			  break;
		  case '-':
			  what = MODE_DEL;
			  break;
#ifdef DEVELOP
		  case '^':
			  tmpo = (char *)ListBits(chptr->mode.mode, 64);
			  sendto_one(cptr,
			      ":%s NOTICE %s :*** %s mode is %li (0x%lx) [%s]",
			      me.name, cptr->name, chptr->chname,
			      chptr->mode.mode, chptr->mode.mode, tmpo);
			  MyFree(tmpo);
			  break;
#endif
		  default:
			  found = 0;
			  tab = &cFlagTab[0];
			  while ((tab->mode != 0x0) && found == 0)
			  {
				  if (tab->flag == *curchr)
				  {
					  found = 1;
					  foundat = *tab;
				  }
				  tab++;
			  }
			  if (found == 1)
			  {
				  modetype = foundat.mode;
			  } else {
#ifdef EXTCMODE
					/* Maybe in extmodes */
					for (extm=0; extm <= Channelmode_highest; extm++)
					{
						if (Channelmode_Table[extm].flag == *curchr)
						{
							found = 2;
							break;
						}
					}
#endif
			  }
			  if (found == 0) /* Mode char unknown */
			  {
			      /* temporary hack: eat parameters of certain future chanmodes.. */
			      if (*curchr == 'I')
				      paracount++;
				  if ((*curchr == 'j') && (what == MODE_ADD))
					  paracount++;

				  if (MyClient(cptr))
					  sendto_one(cptr, err_str(ERR_UNKNOWNMODE),
					     me.name, cptr->name, *curchr);
				  break;
			  }

			  if (checkrestr && strchr(RESTRICT_CHANNELMODES, *curchr))
			  {
				  if (warnrestr)
				  {
					sendto_one(cptr, ":%s %s %s :Setting/removing of channelmode(s) '%s' has been disabled.",
						me.name, IsWebTV(cptr) ? "PRIVMSG" : "NOTICE", cptr->name, RESTRICT_CHANNELMODES);
					warnrestr = 0;
				  }
				  paracount += foundat.parameters;
				  break;
			  }

#ifndef NO_OPEROVERRIDE
				if (found == 1)
				{
                          if ((Halfop_mode(modetype) == FALSE) && opermode == 2 && htrig != 1)
                          {
                          	/* YUCK! */
				if ((foundat.flag == 'h') && !(parc <= paracount) && parv[paracount] &&
				    (find_person(parv[paracount], NULL) == cptr))
				{
					/* ircop with halfop doing a -h on himself. no warning. */
				} else {
					opermode = 0;
					htrig = 1;
				}
                          }
				}
#ifdef EXTCMODE
				else if (found == 2) {
					/* Extended mode: all override stuff is in do_extmode_char which will set
					 * opermode if appropriate. -- Syzop
					 */
				}
#endif /* EXTCMODE */
#endif /* !NO_OPEROVERRIDE */

			  /* We can afford to send off a param */
			  if (parc <= paracount)
			  	parv[paracount] = NULL;
			  if (parv[paracount] &&
			      strlen(parv[paracount]) >= MODEBUFLEN)
			        parv[paracount][MODEBUFLEN-1] = '\0';
			if (found == 1)
			{
			  paracount +=
			      do_mode_char(chptr, modetype, *curchr,
			      parv[paracount], what, cptr, pcount, pvar,
			      bounce, my_access);
			}
#ifdef EXTCMODE
			else if (found == 2)
			{
				paracount += do_extmode_char(chptr, extm, parv[paracount],
				                             what, cptr, pcount, pvar, bounce);
			}
#endif /* EXTCMODE */
			  break;
		}
	}

#ifdef EXTCMODE
	make_mode_str(chptr, oldm, oldem, oldl, *pcount, pvar, modebuf, parabuf, bounce);
#else
	make_mode_str(chptr, oldm, oldl, *pcount, pvar, modebuf, parabuf, bounce);
#endif

#ifndef NO_OPEROVERRIDE
        if (htrig == 1)
        {
                /* This is horrible. Just horrible. */
                if (!((modebuf[0] == '+' || modebuf[0] == '-') && modebuf[1] == '\0'))
                sendto_snomask(SNO_EYES, "*** OperOverride -- %s (%s@%s) MODE %s %s %s",
                      cptr->name, cptr->user->username, cptr->user->realhost,
                      chptr->chname, modebuf, parabuf);

		/* Logging Implementation added by XeRXeS */
		ircd_log(LOG_OVERRIDE,"OVERRIDE: %s (%s@%s) MODE %s %s %s",
			cptr->name, cptr->user->username, cptr->user->realhost,
			chptr->chname, modebuf, parabuf);		

                htrig = 0;
                opermode = 0; /* stop double override notices... but is this ok??? -- Syzop */
        }
#endif

}

/*
 * m_umode() added 15/10/91 By Darren Reed.
 * parv[0] - sender
 * parv[1] - username to change mode for
 * parv[2] - modes to change
 */
DLLFUNC CMD_FUNC(_m_umode)
{
	int  i;
	char **p, *m;
	aClient *acptr;
	int  what, setflags, setsnomask = 0;
	/* (small note: keep 'what' as an int. -- Syzop). */
	short rpterror = 0, umode_restrict_err = 0, chk_restrict = 0, modex_err = 0;

	what = MODE_ADD;
	
	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "MODE");
		return 0;
	}

	if (!(acptr = find_person(parv[1], NULL)))
	{
		if (MyConnect(sptr))
			sendto_one(sptr, err_str(ERR_NOSUCHNICK),
			    me.name, parv[0], parv[1]);
		return 0;
	}
	if (acptr != sptr)
		return 0;

	if (parc < 3)
	{
		sendto_one(sptr, rpl_str(RPL_UMODEIS),
		    me.name, parv[0], get_mode_str(sptr));
		if (sptr->user->snomask)
			sendto_one(sptr, rpl_str(RPL_SNOMASK),
				me.name, parv[0], get_sno_str(sptr));
		return 0;
	}

	/* find flags already set for user */
	setflags = 0;
	
	for (i = 0; i <= Usermode_highest; i++)
		if ((sptr->umodes & Usermode_Table[i].mode))
			setflags |= Usermode_Table[i].mode;

	if (RESTRICT_USERMODES && MyClient(sptr) && !IsAnOper(sptr) && !IsServer(sptr))
		chk_restrict = 1;

	if (MyConnect(sptr))
		setsnomask = sptr->user->snomask;
	/*
	 * parse mode change string(s)
	 */
	p = &parv[2];
	for (m = *p; *m; m++)
	{
		if (chk_restrict && strchr(RESTRICT_USERMODES, *m))
		{
			if (!umode_restrict_err)
			{
				sendto_one(sptr, ":%s %s %s :Setting/removing of usermode(s) '%s' has been disabled.",
					me.name, IsWebTV(sptr) ? "PRIVMSG" : "NOTICE", sptr->name, RESTRICT_USERMODES);
				umode_restrict_err = 1;
			}
			continue;
		}
		switch (*m)
		{
		  case '+':
			  what = MODE_ADD;
			  break;
		  case '-':
			  what = MODE_DEL;
			  break;
				  /* we may not get these,
			   * but they shouldnt be in default
			   */
		  case ' ':
		  case '\t':
			  break;
		  case 'r':
		  case 't':
			  if (MyClient(sptr))
				  break;
			  /* since we now use chatops define in unrealircd.conf, we have
			   * to disallow it here */
		  case 's':
			  if (what == MODE_DEL) {
				if (parc >= 4 && sptr->user->snomask) {
					set_snomask(sptr, parv[3]); 
					if (sptr->user->snomask == 0)
						goto def;
					break;
				}
				else {
					set_snomask(sptr, NULL);
					goto def;
				}
			  }
			  if (what == MODE_ADD) {
				if (parc < 4)
					set_snomask(sptr, IsAnOper(sptr) ? SNO_DEFOPER : SNO_DEFUSER);
				else
					set_snomask(sptr, parv[3]);
				goto def;
			  }
		  case 'o':
		  case 'O':
			  if(sptr->from->flags & FLAGS_QUARANTINE)
			  {
			    sendto_serv_butone(NULL, ":%s KILL %s :%s (Quarantined: no global oper privileges allowed)", me.name, sptr->name, me.name);
			    return exit_client(cptr, sptr, &me, "Quarantined: no global oper privileges allowed");
			  }
			  /* A local user trying to set himself +o/+O is denied here.
			   * A while later (outside this loop) it is handled as well (and +C, +N, etc too)
			   * but we need to take care here too because it might cause problems
			   * since otherwise all IsOper()/IsAnOper() calls cannot be trusted,
			   * that's just asking for bugs! -- Syzop.
			   */
			  if (MyClient(sptr) && (what == MODE_ADD)) /* Someone setting himself +o? Deny it. */
			    break;
			  goto def;
		  case 'x':
			  switch (UHOST_ALLOWED)
			  {
				case UHALLOW_ALWAYS:
					goto def;
				case UHALLOW_NEVER:
					if (MyClient(sptr))
					{
						if (!modex_err) {
							sendto_one(sptr, ":%s %s %s :*** Setting %cx is disabled", me.name, IsWebTV(sptr) ? "PRIVMSG" : "NOTICE", sptr->name, what == MODE_ADD ? '+' : '-');
							modex_err = 1;
						}
						break;
					}
					goto def;
				case UHALLOW_NOCHANS:
					if (MyClient(sptr) && sptr->user->joined)
					{
						if (!modex_err) {
							sendto_one(sptr, ":%s %s %s :*** Setting %cx can not be done while you are on channels", me.name, IsWebTV(sptr) ? "PRIVMSG" : "NOTICE", sptr->name, what == MODE_ADD ? '+' : '-');
							modex_err = 1;
						}
						break;
					}
					goto def;
				case UHALLOW_REJOIN:
					/* Handled later */
					goto def;
			  }
			  break;
		  default:
			def:
			  
			  for (i = 0; i <= Usermode_highest; i++)
			  {
				  if (*m == Usermode_Table[i].flag)
				  {
					  if (Usermode_Table[i].allowed)
						if (!Usermode_Table[i].allowed(sptr,what))
							break;
					  if (what == MODE_ADD)
						  sptr->umodes |= Usermode_Table[i].mode;
					  else
						  sptr->umodes &= ~Usermode_Table[i].mode;
					  break;
				  }
			  	  else if (i == Usermode_highest && MyConnect(sptr) && !rpterror)
  			  	  {
				  	sendto_one(sptr,
				      		err_str(ERR_UMODEUNKNOWNFLAG),
				      		me.name, parv[0]);
					  rpterror = 1;
				  }
			  }
			  break;
		} /* switch */
	} /* for */
	/*
	 * stop users making themselves operators too easily
	 */

	if (!(setflags & UMODE_OPER) && IsOper(sptr) && !IsServer(cptr))
		ClearOper(sptr);
	if (!(setflags & UMODE_LOCOP) && IsLocOp(sptr) && !IsServer(cptr))
		sptr->umodes &= ~UMODE_LOCOP;
	/*
	 *  Let only operators set HelpOp
	 * Helpops get all /quote help <mess> globals -Donwulff
	 */
	if (MyClient(sptr) && IsHelpOp(sptr) && !OPCanHelpOp(sptr))
		ClearHelpOp(sptr);
	/*
	 * Let only operators set FloodF, ClientF; also
	 * remove those flags if they've gone -o/-O.
	 *  FloodF sends notices about possible flooding -Cabal95
	 *  ClientF sends notices about clients connecting or exiting
	 *  Admin is for server admins
	 */
	if (!IsAnOper(sptr) && !IsServer(cptr))
	{
		sptr->umodes &= ~UMODE_WHOIS;
		ClearAdmin(sptr);
		ClearSAdmin(sptr);
		ClearNetAdmin(sptr);
		ClearHideOper(sptr);
		ClearCoAdmin(sptr);
		ClearHelpOp(sptr);
		ClearFailops(sptr);
	}

	/*
	 * New oper access flags - Only let them set certian usermodes on
	 * themselves IF they have access to set that specific mode in their
	 * O:Line.
	 */
	if (MyClient(sptr)) {
		if (IsAnOper(sptr)) {
			if (IsAdmin(sptr) && !OPIsAdmin(sptr))
				ClearAdmin(sptr);
			if (IsSAdmin(sptr) && !OPIsSAdmin(sptr))
				ClearSAdmin(sptr);
			if (IsNetAdmin(sptr) && !OPIsNetAdmin(sptr))
				ClearNetAdmin(sptr);
			if (IsCoAdmin(sptr) && !OPIsCoAdmin(sptr))
				ClearCoAdmin(sptr);
			if (MyClient(sptr) && (sptr->umodes & UMODE_SECURE)
			    && !IsSecure(sptr))
				sptr->umodes &= ~UMODE_SECURE;
		}
	/*
	   This is to remooove the kix bug.. and to protect some stuffie
	   -techie
	 */
		if (MyClient(sptr))
		{
			if ((sptr->umodes & UMODE_KIX) && (!OPCanUmodeq(sptr) || !IsAnOper(sptr)))
				sptr->umodes &= ~UMODE_KIX;
			if ((sptr->umodes & UMODE_SECURE) && !IsSecure(sptr))
				sptr->umodes &= ~UMODE_SECURE;
			if (!(sptr->umodes & UMODE_SECURE) && IsSecure(sptr))
				sptr->umodes |= UMODE_SECURE;
		}
	}
	/*
	 * For Services Protection...
	 */
	if (!IsServer(cptr) && !IsULine(sptr))
	{
		if (IsServices(sptr))
			ClearServices(sptr);
	}
	if ((setflags & UMODE_HIDE) && !IsHidden(sptr))
		sptr->umodes &= ~UMODE_SETHOST;

	if (IsHidden(sptr) && !(setflags & UMODE_HIDE))
	{
		if (sptr->user->virthost)
		{
			MyFree(sptr->user->virthost);
			sptr->user->virthost = NULL;
		}
		sptr->user->virthost = strdup(sptr->user->cloakedhost);
		if (!dontspread)
			sendto_serv_butone_token_opt(cptr, OPT_VHP, sptr->name,
				MSG_SETHOST, TOK_SETHOST, "%s", sptr->user->virthost);
		if (UHOST_ALLOWED == UHALLOW_REJOIN)
		{
			DYN_LOCAL(char, did_parts, sptr->user->joined);
			/* LOL, this is ugly ;) */
			sptr->umodes &= ~UMODE_HIDE;
			rejoin_doparts(sptr, did_parts);
			sptr->umodes |= UMODE_HIDE;
			rejoin_dojoinandmode(sptr, did_parts);
			if (MyClient(sptr))
				sptr->since += 7; /* Add fake lag */
			DYN_FREE(did_parts);
		}
	}

	if (!IsHidden(sptr) && (setflags & UMODE_HIDE))
	{
		if (UHOST_ALLOWED == UHALLOW_REJOIN)
		{
			DYN_LOCAL(char, did_parts, sptr->user->joined);
			/* LOL, this is ugly ;) */
			sptr->umodes |= UMODE_HIDE;
			rejoin_doparts(sptr, did_parts);
			sptr->umodes &= ~UMODE_HIDE;
			rejoin_dojoinandmode(sptr, did_parts);
			if (MyClient(sptr))
				sptr->since += 7; /* Add fake lag */
			DYN_FREE(did_parts);
		}
		if (sptr->user->virthost)
		{
			MyFree(sptr->user->virthost);
			sptr->user->virthost = NULL;
		}
		/* (Re)create the cloaked virthost, because it will be used
		 * for ban-checking... free+recreate here because it could have
		 * been a vhost for example. -- Syzop
		 */
		sptr->user->virthost = strdup(sptr->user->cloakedhost);
	}
	/*
	 * If I understand what this code is doing correctly...
	 *   If the user WAS an operator and has now set themselves -o/-O
	 *   then remove their access, d'oh!
	 * In order to allow opers to do stuff like go +o, +h, -o and
	 * remain +h, I moved this code below those checks. It should be
	 * O.K. The above code just does normal access flag checks. This
	 * only changes the operflag access level.  -Cabal95
	 */
	if ((setflags & (UMODE_OPER | UMODE_LOCOP)) && !IsAnOper(sptr) &&
	    MyConnect(sptr))
	{
#ifndef NO_FDLIST
		delfrom_fdlist(sptr->slot, &oper_fdlist);
#endif
		sptr->oflag = 0;
		remove_oper_snomasks(sptr);
		RunHook2(HOOKTYPE_LOCAL_OPER, sptr, 0);
	}

	if ((sptr->umodes & UMODE_BOT) && !(setflags & UMODE_BOT) && MyClient(sptr))
	{
		/* now +B */
	  do_cmd(sptr, sptr, "BOTMOTD", 1, parv);
	}

	if (!(setflags & UMODE_OPER) && IsOper(sptr))
		IRCstats.operators++;

	/* deal with opercounts and stuff */
	if ((setflags & UMODE_OPER) && !IsOper(sptr))
	{
		IRCstats.operators--;
		VERIFY_OPERCOUNT(sptr, "umode1");
	} else /* YES this 'else' must be here, otherwise we can decrease twice. fixes opercount bug. */
	if (!(setflags & UMODE_HIDEOPER) && IsHideOper(sptr))
	{
		if (IsOper(sptr)) /* decrease, but only if GLOBAL oper */
			IRCstats.operators--;
		VERIFY_OPERCOUNT(sptr, "umode2");
	}
	/* end of dealing with opercounts */

	if ((setflags & UMODE_HIDEOPER) && !IsHideOper(sptr))
	{
		if (IsOper(sptr)) /* increase, but only if GLOBAL oper */
			IRCstats.operators++;
	}
	if (!(setflags & UMODE_INVISIBLE) && IsInvisible(sptr))
		IRCstats.invisible++;
	if ((setflags & UMODE_INVISIBLE) && !IsInvisible(sptr))
		IRCstats.invisible--;
	/*
	 * compare new flags with old flags and send string which
	 * will cause servers to update correctly.
	 */
	if (setflags != sptr->umodes)
		RunHook3(HOOKTYPE_UMODE_CHANGE, sptr, setflags, sptr->umodes);
	if (dontspread == 0)
		send_umode_out(cptr, sptr, setflags);

	if (MyConnect(sptr) && setsnomask != sptr->user->snomask)
		sendto_one(sptr, rpl_str(RPL_SNOMASK),
			me.name, parv[0], get_sno_str(sptr));

	return 0;
}
