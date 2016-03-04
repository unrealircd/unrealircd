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

#include "unrealircd.h"

/* Forward declarations */
CMD_FUNC(m_mode);
CMD_FUNC(m_mlock);
DLLFUNC void _do_mode(aChannel *chptr, aClient *cptr, aClient *sptr, int parc, char *parv[], time_t sendts, int samode);
DLLFUNC void _set_mode(aChannel *chptr, aClient *cptr, int parc, char *parv[], u_int *pcount,
    char pvar[MAXMODEPARAMS][MODEBUFLEN + 3], int bounce);
CMD_FUNC(_m_umode);

/* local: */
static void bounce_mode(aChannel *, aClient *, int, char **);
int do_mode_char(aChannel *chptr, long modetype, char modechar, char *param,
    u_int what, aClient *cptr,
     u_int *pcount, char pvar[MAXMODEPARAMS][MODEBUFLEN + 3], char bounce, long my_access);
int do_extmode_char(aChannel *chptr, Cmode *handler, char *param, u_int what,
                    aClient *cptr, u_int *pcount, char pvar[MAXMODEPARAMS][MODEBUFLEN + 3],
                    char bounce);
void make_mode_str(aChannel *chptr, long oldm, Cmode_t oldem, long oldl, int pcount,
    char pvar[MAXMODEPARAMS][MODEBUFLEN + 3], char *mode_buf, char *para_buf,
    size_t mode_buf_size, size_t para_buf_size, char bounce);

static void mode_cutoff(char *s);
static void mode_cutoff2(aClient *sptr, aChannel *chptr, int *parc_out, char *parv[]);

static int samode_in_progress = 0;

#define MSG_MODE 	"MODE"

ModuleHeader MOD_HEADER(m_mode)
  = {
	"m_mode",
	"4.0",
	"command /mode",
	"3.2-b8-1",
	NULL
    };

MOD_TEST(m_mode)
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAddVoid(modinfo->handle, EFUNC_DO_MODE, _do_mode);
	EfunctionAddVoid(modinfo->handle, EFUNC_SET_MODE, _set_mode);
	EfunctionAdd(modinfo->handle, EFUNC_M_UMODE, _m_umode);
	return MOD_SUCCESS;
}

MOD_INIT(m_mode)
{
	CommandAdd(modinfo->handle, MSG_MODE, m_mode, MAXPARA, M_USER|M_SERVER);
	CommandAdd(modinfo->handle, MSG_MLOCK, m_mlock, MAXPARA, M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_mode)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_mode)
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
		    me.name, sptr->name, "MODE");
		return 0;
	}

	if (MyConnect(sptr))
		clean_channelname(parv[1]);

	if (parc < 3)
	{
		*modebuf = *parabuf = '\0';

		modebuf[1] = '\0';
		channel_modes(sptr, modebuf, parabuf, sizeof(modebuf), sizeof(parabuf), chptr);
		sendto_one(sptr, rpl_str(RPL_CHANNELMODEIS), me.name, sptr->name,
		    chptr->chname, modebuf, parabuf);
		sendto_one(sptr, rpl_str(RPL_CREATIONTIME), me.name, sptr->name,
		    chptr->chname, chptr->creationtime);
		return 0;
	}

	if (IsPerson(sptr) && strstr(parv[2], "b") && BadPtr(parv[3]))
	{
		if (!IsMember(sptr, chptr) && !ValidatePermissionsForPath("channel:remotebanlist",sptr,NULL,chptr,NULL))
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

	if (IsPerson(sptr) && strstr(parv[2], "e") && BadPtr(parv[3]))
	{
		if (!IsMember(sptr, chptr) && !ValidatePermissionsForPath("channel:remotebanlist",sptr,NULL,chptr,NULL))
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

	if (IsPerson(sptr) && strstr(parv[2], "I") && BadPtr(parv[3]))
	{
		if (!IsMember(sptr, chptr) && !ValidatePermissionsForPath("channel:remoteinvexlist",sptr,NULL,chptr,NULL))
			return 0;
		for (ban = chptr->invexlist; ban; ban = ban->next)
			sendto_one(sptr, rpl_str(RPL_INVEXLIST), me.name,
			    sptr->name, chptr->chname, ban->banstr,
			    ban->who, ban->when);
		sendto_one(sptr, rpl_str(RPL_ENDOFINVEXLIST), me.name,
		    sptr->name, chptr->chname);
		return 0;
	}

	if (IsPerson(sptr) && strstr(parv[2], "q") && BadPtr(parv[3]))
	{
		if (!IsMember(sptr, chptr) && !ValidatePermissionsForPath("channel:remoteownerlist",sptr,NULL,chptr,NULL))
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

	if (IsPerson(sptr) && strstr(parv[2], "a") && BadPtr(parv[3]))
	{
		if (!IsMember(sptr, chptr) && !ValidatePermissionsForPath("channel:remoteownerlist",sptr,NULL,chptr,NULL))
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

	opermode = 0;

#ifndef NO_OPEROVERRIDE
        if (IsPerson(sptr) && !IsULine(sptr) && !is_chan_op(sptr, chptr)
            && !is_half_op(sptr, chptr) && ValidatePermissionsForPath("override:mode",sptr,NULL,chptr,NULL))
        {
                sendts = 0;
                opermode = 1;
                goto aftercheck;
        }

        if (IsPerson(sptr) && !IsULine(sptr) && !is_chan_op(sptr, chptr)
            && is_half_op(sptr, chptr) && ValidatePermissionsForPath("override:mode",sptr,NULL,chptr,NULL))
        {
                opermode = 2;
                goto aftercheck;
        }
#endif

	if (IsPerson(sptr) && !IsULine(sptr) && !is_chan_op(sptr, chptr)
	    && !is_half_op(sptr, chptr)
	    && (cptr == sptr || !ValidatePermissionsForPath("override:mode",sptr,NULL,chptr,NULL)))
	{
		if (cptr == sptr)
		{
			sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
			    me.name, sptr->name, chptr->chname);
			return 0;
		}
		sendto_one(cptr, ":%s MODE %s -oh %s %s 0",
		    me.name, chptr->chname, sptr->name, sptr->name);
		/* Tell the other server that the user is
		 * de-opped.  Fix op desyncs. */
		bounce_mode(chptr, cptr, parc - 2, parv + 2);
		return 0;
	}

	if (IsServer(sptr) && (sendts = atol(parv[parc - 1]))
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

	/* This is to prevent excess +<whatever> modes. -- Syzop */
	if (MyClient(sptr) && parv[2])
	{
		mode_cutoff(parv[2]);
		mode_cutoff2(sptr, chptr, &parc, parv);
	}

	/* Filter out the unprivileged FIRST. *
	 * Now, we can actually do the mode.  */

	(void)do_mode(chptr, cptr, sptr, parc - 2, parv + 2, sendts, 0);
	/* After this don't touch 'chptr' anymore, as permanent module may have destroyed the channel */
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
		(*parc_out)--;
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

	if (MyConnect(sptr))
			RunHook7(HOOKTYPE_PRE_LOCAL_CHANMODE, cptr, sptr, chptr, modebuf, parabuf, sendts, samode);
		else
			RunHook7(HOOKTYPE_PRE_REMOTE_CHANMODE, cptr, sptr, chptr, modebuf, parabuf, sendts, samode);


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
				sendto_server(cptr, 0, 0, ":%s MODE %s %s+ %lu",
				    me.name, chptr->chname, isbounce ? "&" : "",
				    chptr->creationtime);
			else
				sendto_server(cptr, 0, 0, ":%s MODE %s %s+",
				    me.name, chptr->chname, isbounce ? "&" : "");
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
		sendto_umode_global(UMODE_OPER, "%s used SAMODE %s (%s%s%s)",
			sptr->name, chptr->chname, modebuf, *parabuf ? " " : "", parabuf);
		sptr = &me;
		sendts = 0;
	}

	sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s",
	    sptr->name, chptr->chname, modebuf, parabuf);

	if (IsServer(sptr) && sendts != -1)
	{
		sendto_server(cptr, 0, 0,
		              ":%s MODE %s %s%s %s %lu",
		              sptr->name, chptr->chname, isbounce ? "&" : "", modebuf, parabuf, sendts);
	} else
	if (samode && IsMe(sptr))
	{
		/* SAMODE is a special case: always send a TS of 0 (omitting TS==desynch) */
		sendto_server(cptr, 0, 0,
		              ":%s MODE %s %s %s 0",
		              sptr->name, chptr->chname, modebuf, parabuf);
	} else
	{
		sendto_server(cptr, 0, 0,
		              ":%s MODE %s %s%s %s",
		              sptr->name, chptr->chname, isbounce ? "&" : "", modebuf, parabuf);
		/* tell them it's not a timestamp, in case the last param
		   ** is a number. */
	}

	if (MyConnect(sptr))
		RunHook7(HOOKTYPE_LOCAL_CHANMODE, cptr, sptr, chptr, modebuf, parabuf, sendts, samode);
	else
		RunHook7(HOOKTYPE_REMOTE_CHANMODE, cptr, sptr, chptr, modebuf, parabuf, sendts, samode);
	/* After this, don't touch 'chptr' anymore! As permanent module may destroy the channel. */
}
/* make_mode_str -- written by binary
 *	Reconstructs the mode string, to make it look clean.  mode_buf will
 *  contain the +x-y stuff, and the parabuf will contain the parameters.
 *  If bounce is set to 1, it will make the string it needs for a bounce.
 */
void make_mode_str(aChannel *chptr, long oldm, Cmode_t oldem, long oldl, int pcount,
    char pvar[MAXMODEPARAMS][MODEBUFLEN + 3], char *mode_buf, char *para_buf,
    size_t mode_buf_size, size_t para_buf_size, char bounce) {

	char tmpbuf[MODEBUFLEN+3], *tmpstr;
	aCtab *tab = &cFlagTab[0];
	char *x = mode_buf;
	int  what, cnt, z;
	int i;
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

	/* - extmodes (both "param modes" and paramless don't have
	 * any params when unsetting... well, except one special type, that is (we skip those here)
	 */
	for (i=0; i <= Channelmode_highest; i++)
	{
		if (!Channelmode_Table[i].flag || Channelmode_Table[i].unset_with_param)
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
			ircsnprintf(para_buf, para_buf_size, "%s%d ", para_buf, chptr->mode.limit);
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
		chptr->mode.extmode = oldem;
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
	int  chasing = 0, x;
	int xxi, xyi, xzi, hascolon;
	char *xp;
	int  notsecure;
	Hook *h;

	if ((my_access & CHFL_HALFOP) && !is_xchanop(my_access) && !IsULine(cptr)
	    && !op_can_override("override:mode",cptr,chptr,&modetype) && !samode_in_progress)
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
	  case MODE_RGSTR:
		  if (!IsServer(cptr) && !IsULine(cptr))
		  {
			sendto_one(cptr, err_str(ERR_ONLYSERVERSCANCHANGE), me.name, cptr->name,
				   chptr->chname);
			break;
		  }
		  goto setmode;
	  case MODE_SECRET:
	  case MODE_PRIVATE:
	  case MODE_MODERATED:
	  case MODE_TOPICLIMIT:
	  case MODE_NOPRIVMSGS:
	  case MODE_INVITEONLY:
	  	goto setmode;
		setmode:
		  retval = 0;
		  if (what == MODE_ADD) {
			  /* +sp bugfix.. (by JK/Luke)*/
		 	 if (modetype == MODE_SECRET
			      && (chptr->mode.mode & MODE_PRIVATE))
				  chptr->mode.mode &= ~MODE_PRIVATE;
			  if (modetype == MODE_PRIVATE
			      && (chptr->mode.mode & MODE_SECRET))
				  chptr->mode.mode &= ~MODE_SECRET;
			  chptr->mode.mode |= modetype;
		  }
		  else
		  {
			  chptr->mode.mode &= ~modetype;
			  RunHook2(HOOKTYPE_MODECHAR_DEL, chptr, (int)modechar);
		  }
		  break;

/* do pro-opping here (popping) */
	  case MODE_CHANOWNER:
		  REQUIRE_PARAMETER()
		  if (!IsULine(cptr) && !IsServer(cptr) && !is_chanowner(cptr, chptr) && !samode_in_progress)
		  {
		  	  if (MyClient(cptr) && !op_can_override("override:mode",cptr,chptr,&modetype))
		  	  {
		  	  	sendto_one(cptr, err_str(ERR_CHANOWNPRIVNEEDED), me.name, cptr->name, chptr->chname);
		  	  	break;
		  	  }
			    if (!is_halfop(cptr, chptr)) /* htrig will take care of halfop override notices */
			        opermode = 1;

		  }
	  case MODE_CHANPROT:
		  REQUIRE_PARAMETER()
		  /* not uline, not server, not chanowner, not an samode, not -a'ing yourself... */
		  if (!IsULine(cptr) && !IsServer(cptr) && !is_chanowner(cptr, chptr) && !samode_in_progress &&
		      !(param && (what == MODE_DEL) && (find_client(param, NULL) == cptr)))
		  {
		  	  if (MyClient(cptr) && !op_can_override("override:mode",cptr,chptr,&modetype))
		  	  {
		  	  	sendto_one(cptr, err_str(ERR_CHANOWNPRIVNEEDED), me.name, cptr->name, chptr->chname);
		  	  	break;
		  	  }
  				if (!is_halfop(cptr, chptr)) /* htrig will take care of halfop override notices */
  				   opermode = 1;
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

		  if (what == MODE_DEL)
		  {
		  	int ret = EX_ALLOW;
		  	char *badmode = NULL;

		  	for (h = Hooks[HOOKTYPE_MODE_DEOP]; h; h = h->next)
		  	{
		  		int n = (*(h->func.intfunc))(cptr, member->cptr, chptr, what, modechar, my_access, &badmode);
		  		if (n == EX_DENY)
		  			ret = n;
				else if (n == EX_ALWAYS_DENY)
				{
					ret = n;
					break;
				}
		  	}

		  	if (ret == EX_ALWAYS_DENY)
		  	{
		  		if (MyClient(cptr) && badmode)
		  			sendto_one(cptr, "%s", badmode); /* send error message, if any */

				if (MyClient(cptr))
					break; /* stop processing this mode */
		  	}

		  	/* This probably should work but is completely untested (the operoverride stuff, I mean): */
		  	if (ret == EX_DENY)
		  	{
		  		if (!op_can_override("override:mode:del",cptr,chptr,&modetype))
		  		{
  					if (badmode)
  						sendto_one(cptr, "%s", badmode); /* send error message, if any */
  						break; /* stop processing this mode */
  				} else {
  						opermode = 1;
  				}
		  	}
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
  				if (!op_can_override("override:mode:del",cptr,chptr,&modetype))
  				{
  					char errbuf[NICKLEN+30];
  					ircsnprintf(errbuf, sizeof(errbuf), "%s is a channel owner", member->cptr->name);
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
			  	if (!op_can_override("override:mode:del",cptr,chptr,&modetype))
			  	{
					char errbuf[NICKLEN+30];
					ircsnprintf(errbuf, sizeof(errbuf), "%s is a channel admin", member->cptr->name);
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
		  ircsnprintf(pvar[*pcount], MODEBUFLEN + 3, "%c%c%s",
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
			  RunHook2(HOOKTYPE_MODECHAR_DEL, chptr, (int)modechar);
		  }
		  break;
	  case MODE_KEY:
		  REQUIRE_PARAMETER()
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
				  strlcpy(chptr->mode.key, param,
				      sizeof(chptr->mode.key));
			  }
			  tmpstr = param;
		  }
		  else
		  {
			  if (!*chptr->mode.key)
				  break;	/* no change */
			  strlcpy(tmpbuf, chptr->mode.key, sizeof(tmpbuf));
			  tmpstr = tmpbuf;
			  if (!bounce)
				  strcpy(chptr->mode.key, "");
			  RunHook2(HOOKTYPE_MODECHAR_DEL, chptr, (int)modechar);
		  }
		  retval = 1;

		  ircsnprintf(pvar[*pcount], MODEBUFLEN + 3, "%ck%s",
		      what == MODE_ADD ? '+' : '-', tmpstr);
		  (*pcount)++;
		  break;

	  case MODE_BAN:
		  REQUIRE_PARAMETER()
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
		            if (ValidatePermissionsForPath("override:extban",cptr,NULL,chptr,NULL))
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
		  ircsnprintf(pvar[*pcount], MODEBUFLEN + 3, "%cb%s",
		      what == MODE_ADD ? '+' : '-', tmpstr);
		  (*pcount)++;
		  break;
	  case MODE_EXCEPT:
		  REQUIRE_PARAMETER()
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
		            if (ValidatePermissionsForPath("override:extban",cptr,NULL,chptr,NULL))
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
		  ircsnprintf(pvar[*pcount], MODEBUFLEN + 3, "%ce%s",
		      what == MODE_ADD ? '+' : '-', tmpstr);
		  (*pcount)++;
		  break;
	  case MODE_INVEX:
		  REQUIRE_PARAMETER()
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
		            if (ValidatePermissionsForPath("override:extban",cptr,NULL,chptr,NULL))
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
		  ircsnprintf(pvar[*pcount], MODEBUFLEN + 3, "%cI%s",
		      what == MODE_ADD ? '+' : '-', tmpstr);
		  (*pcount)++;
		  break;
	}
	return retval;
}

/** Check access and if granted, set the extended chanmode to the requested value in memory.
  * note: if bounce is requested then the mode will not be set.
  * @returns amount of params eaten (0 or 1)
  */
int do_extmode_char(aChannel *chptr, Cmode *handler, char *param, u_int what,
                    aClient *cptr, u_int *pcount, char pvar[MAXMODEPARAMS][MODEBUFLEN + 3],
                    char bounce)
{
int paracnt = (what == MODE_ADD) ? handler->paracount : 0;
char mode = handler->flag;
int x;
char *morphed;

	if ((what == MODE_DEL) && handler->unset_with_param)
		paracnt = 1; /* there's always an exception! */

	/* Expected a param and it isn't there? */
	if (paracnt && (!param || (*pcount >= MAXMODEPARAMS)))
		return 0;

	/* Prevent remote users from setting local channel modes */
	if (handler->local && !MyClient(cptr))
		return paracnt;

	if (MyClient(cptr))
	{
		x = handler->is_ok(cptr, chptr, mode, param, EXCHK_ACCESS, what);
		if ((x == EX_ALWAYS_DENY) ||
		    ((x == EX_DENY) && !op_can_override("override:mode:del",cptr,chptr,handler) && !samode_in_progress))
		{
			handler->is_ok(cptr, chptr, mode, param, EXCHK_ACCESS_ERR, what);
			return paracnt; /* Denied & error msg sent */
		}
		if (x == EX_DENY)
			opermode = 1; /* override in progress... */
	} else {
		/* remote user: we only need to check if we need to generate an operoverride msg */
		if (!IsULine(cptr) && IsPerson(cptr) && op_can_override("override:mode:del",cptr,chptr,handler) &&
		    (handler->is_ok(cptr, chptr, mode, param, EXCHK_ACCESS, what) != EX_ALLOW))
			opermode = 1; /* override in progress... */
	}

	/* Check for multiple changes in 1 command (like +y-y+y 1 2, or +yy 1 2). */
	for (x = 0; x < *pcount; x++)
	{
		if (pvar[x][1] == handler->flag)
		{
			/* this is different than the old chanmode system, coz:
			 * "mode #chan +kkL #a #b #c" will get "+kL #a #b" which is wrong :p.
			 * we do eat the parameter. -- Syzop
			 */
			return paracnt;
		}
	}

	/* w00t... a parameter mode */
	if (handler->paracount)
	{
		if (what == MODE_DEL)
		{
			if (!(chptr->mode.extmode & handler->mode))
				return paracnt; /* There's nothing to remove! */
			if (handler->unset_with_param)
			{
				/* Special extended channel mode requiring a parameter on unset.
				 * Any provided parameter is ok, the current one (that is set) will be used.
				 */
				ircsnprintf(pvar[*pcount], MODEBUFLEN + 3, "-%c%s",
					handler->flag, cm_getparameter(chptr, handler->flag));
				(*pcount)++;
			} else
			{
				/* Normal extended channel mode: deleteing is just -X, no parameter */
				ircsnprintf(pvar[*pcount], MODEBUFLEN + 3, "-%c", handler->flag);
			}
		} else {
			/* add: is the parameter ok? */
			if (handler->is_ok(cptr, chptr, mode, param, EXCHK_PARAM, what) == FALSE)
				return paracnt;

			morphed =  handler->conv_param(param, cptr);

			/* is it already set at the same value? if so, ignore it. */
			if (chptr->mode.extmode & handler->mode)
			{
				char *now, *requested;
				char flag = handler->flag;
				now = cm_getparameter(chptr, flag);
				requested = handler->conv_param(param, cptr);
				if (now && requested && !strcmp(now, requested))
					return paracnt; /* ignore... */
			}
				ircsnprintf(pvar[*pcount], MODEBUFLEN + 3, "+%c%s",
					handler->flag, handler->conv_param(param, cptr));
			(*pcount)++;
			param = morphed; /* set param to converted parameter. */
		}
	}

	if (bounce) /* bounce here means: only check access and return return value */
		return paracnt;

	if (what == MODE_ADD)
	{	/* + */
		chptr->mode.extmode |= handler->mode;
		if (handler->paracount)
			cm_putparameter(chptr, handler->flag, param);
	} else
	{	/* - */
		chptr->mode.extmode &= ~(handler->mode);
		RunHook2(HOOKTYPE_MODECHAR_DEL, chptr, (int)mode);
		if (handler->paracount)
			cm_freeparameter(chptr, handler->flag);
	}
	return paracnt;
}

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

/** In 2003 I introduced PROTOCTL CHANMODES= so remote servers (and services)
 * could deal with unknown "parameter eating" channel modes, minimizing desynchs.
 * Now, in 2015, I finally added the code to deal with this. -- Syzop
 */
int paracount_for_chanmode_from_server(aClient *acptr, u_int what, char mode)
{
	if (MyClient(acptr))
		return 0; /* no server, we have no idea, assume 0 paracount */

	if (!acptr->serv)
	{
		/* If it's from a remote client then figure out from which "uplink" we
		 * received this MODE. The uplink is the directly-connected-server to us
		 * and may differ from the server the user is actually on. This is correct.
		 */
		if (!acptr->from || !acptr->from->serv)
			return 0;
		acptr = acptr->from;
	}

	if (acptr->serv->features.chanmodes[0] && strchr(acptr->serv->features.chanmodes[0], mode))
		return 1; /* 1 parameter for set, 1 parameter for unset */

	if (acptr->serv->features.chanmodes[1] && strchr(acptr->serv->features.chanmodes[1], mode))
		return 1; /* 1 parameter for set, 1 parameter for unset */

	if (acptr->serv->features.chanmodes[2] && strchr(acptr->serv->features.chanmodes[2], mode))
		return (what == MODE_ADD) ? 1 : 0; /* 1 parameter for set, no parameter for unset */

	if (acptr->serv->features.chanmodes[3] && strchr(acptr->serv->features.chanmodes[3], mode))
		return 0; /* no parameter for set, no parameter for unset */

	/* If we end up here it means we have no idea if it is a parameter-eating or paramless
	 * channel mode. That's actually pretty bad. This shouldn't happen since CHANMODES=
	 * is sent since 2003 and the (often also required) EAUTH PROTOCTL is in there since 2010.
	 */
	sendto_realops("Unknown channel mode %c%c from server %s!",
		(what == MODE_ADD) ? '+' : '-',
		mode,
		acptr->name);

	/* Some additional backward compatability for +j for really old servers.. Hmm.. too nice */
	if ((what == MODE_ADD) && (mode == 'j'))
		return 1;

	return 0;
}

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
	int  sent_mlock_warning = 0;
	unsigned int htrig = 0;
	long oldm, oldl;
	int checkrestr = 0, warnrestr = 1;
	int extm = 1000000; /* (default value not used but stops gcc from complaining) */
	Cmode_t oldem;
	long my_access;
	paracount = 1;
	*pcount = 0;

	oldm = chptr->mode.mode;
	oldl = chptr->mode.limit;
	oldem = chptr->mode.extmode;
	if (RESTRICT_CHANNELMODES && !ValidatePermissionsForPath("channel:restrictedmodes",cptr,NULL,chptr,NULL)) /* "cache" this */
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
			  if (MyClient(cptr) && chptr->mode_lock && strchr(chptr->mode_lock, *curchr) != NULL)
			  {
				  if (!sent_mlock_warning)
				  {
					  sendto_one(cptr, err_str(ERR_MLOCKRESTRICTED), me.name, cptr->name, chptr->chname, *curchr, chptr->mode_lock);
					  sent_mlock_warning++;
				  }
				  continue;
			  }
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
					/* Maybe in extmodes */
					for (extm=0; extm <= Channelmode_highest; extm++)
					{
						if (Channelmode_Table[extm].flag == *curchr)
						{
							found = 2;
							break;
						}
					}
			  }
			  if (found == 0) /* Mode char unknown */
			  {
				  if (!MyClient(cptr))
				      paracount += paracount_for_chanmode_from_server(cptr, what, *curchr);
				  else
					  sendto_one(cptr, err_str(ERR_UNKNOWNMODE),
					     me.name, cptr->name, *curchr);
				  break;
			  }

			  if (checkrestr && strchr(RESTRICT_CHANNELMODES, *curchr))
			  {
				  if (warnrestr)
				  {
					sendnotice(cptr, "Setting/removing of channelmode(s) '%s' has been disabled.",
						RESTRICT_CHANNELMODES);
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
				else if (found == 2) {
					/* Extended mode: all override stuff is in do_extmode_char which will set
					 * opermode if appropriate. -- Syzop
					 */
				}
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
			else if (found == 2)
			{
				paracount += do_extmode_char(chptr, &Channelmode_Table[extm], parv[paracount],
				                             what, cptr, pcount, pvar, bounce);
			}
			  break;
		}
	}

	make_mode_str(chptr, oldm, oldem, oldl, *pcount, pvar, modebuf, parabuf, sizeof(modebuf), sizeof(parabuf), bounce);

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
 * parv[1] - username to change mode for
 * parv[2] - modes to change
 */
CMD_FUNC(_m_umode)
{
	int  i;
	char **p, *m;
	aClient *acptr;
	int what, setsnomask = 0;
	long setflags = 0;
	/* (small note: keep 'what' as an int. -- Syzop). */
	short rpterror = 0, umode_restrict_err = 0, chk_restrict = 0, modex_err = 0;

	what = MODE_ADD;

	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, sptr->name, "MODE");
		return 0;
	}

	if (!(acptr = find_person(parv[1], NULL)))
	{
		if (MyConnect(sptr))
			sendto_one(sptr, err_str(ERR_NOSUCHNICK),
			    me.name, sptr->name, parv[1]);
		return 0;
	}
	if (acptr != sptr)
		return 0;

	if (parc < 3)
	{
		sendto_one(sptr, rpl_str(RPL_UMODEIS),
		    me.name, sptr->name, get_mode_str(sptr));
		if (sptr->user->snomask)
			sendto_one(sptr, rpl_str(RPL_SNOMASK),
				me.name, sptr->name, get_sno_str(sptr));
		return 0;
	}

	/* find flags already set for user */
	for (i = 0; i <= Usermode_highest; i++)
		if ((sptr->umodes & Usermode_Table[i].mode))
			setflags |= Usermode_Table[i].mode;

	if (RESTRICT_USERMODES && MyClient(sptr) && !ValidatePermissionsForPath("self:restrictedumodes",sptr,NULL,NULL,NULL))
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
				sendnotice(sptr, "Setting/removing of usermode(s) '%s' has been disabled.",
					RESTRICT_USERMODES);
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
					set_snomask(sptr, IsOper(sptr) ? SNO_DEFOPER : SNO_DEFUSER);
				else
					set_snomask(sptr, parv[3]);
				goto def;
			  }
		  case 'o':
		  case 'O':
			  if(sptr->from->flags & FLAGS_QUARANTINE)
			  {
				sendto_realops("QUARANTINE: Oper %s on server %s killed, due to quarantine", sptr->name, sptr->srvptr->name);
			    sendto_server(NULL, 0, 0, ":%s KILL %s :%s (Quarantined: no oper privileges allowed)", me.name, sptr->name, me.name);
			    return exit_client(cptr, sptr, &me, "Quarantined: no oper privileges allowed");
			  }
			  /* A local user trying to set himself +o/+O is denied here.
			   * A while later (outside this loop) it is handled as well (and +C, +N, etc too)
			   * but we need to take care here too because it might cause problems
			   * that's just asking for bugs! -- Syzop.
			   */
			  if (MyClient(sptr) && (what == MODE_ADD)) /* Someone setting himself +o? Deny it. */
			    break;
			  goto def;
		  case 't':
		  case 'x':
			  switch (UHOST_ALLOWED)
			  {
				case UHALLOW_ALWAYS:
					goto def;
				case UHALLOW_NEVER:
					if (MyClient(sptr))
					{
						if (!modex_err)
						{
							sendnotice(sptr, "*** Setting %c%c is disabled",
								what == MODE_ADD ? '+' : '-', *m);
							modex_err = 1;
						}
						break;
					}
					goto def;
				case UHALLOW_NOCHANS:
					if (MyClient(sptr) && sptr->user->joined)
					{
						if (!modex_err)
						{
							sendnotice(sptr, "*** Setting %c%c can not be done while you are on channels",
								what == MODE_ADD ? '+' : '-', *m);
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
				      		me.name, sptr->name);
					  rpterror = 1;
				  }
			  }
			  break;
		} /* switch */
	} /* for */

	/* Don't let non-ircops set ircop-only modes or snomasks */
	if (!ValidatePermissionsForPath("self:restrictedumodes",sptr,NULL,NULL,NULL))
	{
		remove_oper_privileges(sptr, 0);
	}

	if (MyClient(sptr) && !ValidatePermissionsForPath("override:secure",sptr,NULL,NULL,NULL) &&  (sptr->umodes & UMODE_SECURE)
			&& !IsSecure(sptr))
  {
			sptr->umodes &= ~UMODE_SECURE;
	}


	/* -x translates to -xt (if applicable) */
	if ((setflags & UMODE_HIDE) && !IsHidden(sptr))
		sptr->umodes &= ~UMODE_SETHOST;

	/* Vhost unset = unset some other data as well */
	if ((setflags & UMODE_SETHOST) && !IsSetHost(sptr))
	{
		swhois_delete(sptr, "vhost", "*", &me, NULL);
	}

	/* +x or -t+x */
	if ((IsHidden(sptr) && !(setflags & UMODE_HIDE)) ||
	    ((setflags & UMODE_SETHOST) && !IsSetHost(sptr) && IsHidden(sptr)))
	{
		safefree(sptr->user->virthost);
		sptr->user->virthost = strdup(sptr->user->cloakedhost);
		if (!dontspread)
			sendto_server(cptr, PROTO_VHP, 0, ":%s SETHOST :%s",
				sptr->name, sptr->user->virthost);
		if (UHOST_ALLOWED == UHALLOW_REJOIN)
		{
			/* LOL, this is ugly ;) */
			sptr->umodes &= ~UMODE_HIDE;
			rejoin_leave(sptr);
			sptr->umodes |= UMODE_HIDE;
			rejoin_joinandmode(sptr);
			if (MyClient(sptr))
				sptr->local->since += 7; /* Add fake lag */
		}
		if (MyClient(sptr))
			sendto_one(sptr, err_str(RPL_HOSTHIDDEN), me.name, sptr->name, sptr->user->virthost);
	}

	/* -x */
	if (!IsHidden(sptr) && (setflags & UMODE_HIDE))
	{
		if (UHOST_ALLOWED == UHALLOW_REJOIN)
		{
			/* LOL, this is ugly ;) */
			sptr->umodes |= UMODE_HIDE;
			rejoin_leave(sptr);
			sptr->umodes &= ~UMODE_HIDE;
			rejoin_joinandmode(sptr);
			if (MyClient(sptr))
				sptr->local->since += 7; /* Add fake lag */
		}
		/* (Re)create the cloaked virthost, because it will be used
		 * for ban-checking... free+recreate here because it could have
		 * been a vhost for example. -- Syzop
		 */
		safefree(sptr->user->virthost);
		sptr->user->virthost = strdup(sptr->user->cloakedhost);
		if (MyClient(sptr))
			sendto_one(sptr, err_str(RPL_HOSTHIDDEN), me.name, sptr->name, sptr->user->realhost);
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
	if ((setflags & UMODE_OPER) && !IsOper(sptr) && MyConnect(sptr))
	{
		list_del(&sptr->special_node);
		remove_oper_privileges(sptr, 0);
		RunHook2(HOOKTYPE_LOCAL_OPER, sptr, 0);
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
		IRCstats.operators--;
		VERIFY_OPERCOUNT(sptr, "umode2");
	}
	/* end of dealing with opercounts */

	if ((setflags & UMODE_HIDEOPER) && !IsHideOper(sptr))
	{
		IRCstats.operators++;
	}
	if (!(setflags & UMODE_INVISIBLE) && IsInvisible(sptr))
		IRCstats.invisible++;
	if ((setflags & UMODE_INVISIBLE) && !IsInvisible(sptr))
		IRCstats.invisible--;

	if (MyConnect(sptr) && !IsOper(sptr))
		remove_oper_privileges(sptr, 0);

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
			me.name, sptr->name, get_sno_str(sptr));

	return 0;
}

CMD_FUNC(m_mlock)
{
	aChannel *chptr = NULL;
	TS chants;

	if ((parc < 3) || BadPtr(parv[2]))
		return 0;

	chants = (TS) atol(parv[1]);

	/* Now, try to find the channel in question */
	chptr = find_channel(parv[2], NullChn);
	if (chptr == NULL)
		return 0;

	/* Senders' Channel TS is higher, drop it. */
	if (chants > chptr->creationtime)
		return 0;

	if (IsServer(sptr))
		set_channel_mlock(cptr, sptr, chptr, parv[3], TRUE);

	return 0;
}
