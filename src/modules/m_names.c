/*
 *   IRC - Internet Relay Chat, src/modules/m_names.c
 *   (C) 2006 The UnrealIRCd Team
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

CMD_FUNC(m_names);

#define MSG_NAMES 	"NAMES"

ModuleHeader MOD_HEADER(m_names)
  = {
	"m_names",
	"4.0",
	"command /names", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_names)
{
	CommandAdd(modinfo->handle, MSG_NAMES, m_names, MAXPARA, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_names)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_names)
{
	return MOD_SUCCESS;
}

/************************************************************************
 * m_names() - Added by Jto 27 Apr 1989
 * 12 Feb 2000 - geesh, time for a rewrite -lucas
 ************************************************************************/

static char buf[BUFSIZE];

/*
** m_names
**	parv[1] = channel
*/
#define TRUNCATED_NAMES 64
CMD_FUNC(m_names)
{
	int uhnames = (MyConnect(sptr) && SupportUHNAMES(sptr)); // cache UHNAMES support
	int bufLen = NICKLEN + (!uhnames ? 0 : (1 + USERLEN + 1 + HOSTLEN));
	int  mlen = strlen(me.name) + bufLen + 7;
	aChannel *chptr;
	aClient *acptr;
	int  member;
	int i = 0;
	Hook *h;
	Member *cm;
	int  idx, flag = 1, spos;
	char *s, *para = parv[1];
	char nuhBuffer[NICKLEN+USERLEN+HOSTLEN+3];


	if (parc < 2 || !MyConnect(sptr))
	{
		sendto_one(sptr, rpl_str(RPL_ENDOFNAMES), me.name,
		    sptr->name, "*");
		return 0;
	}

	for (s = para; *s; s++)
	{
		if (*s == ',')
		{
			if (strlen(para) > TRUNCATED_NAMES)
				para[TRUNCATED_NAMES] = '\0';
			sendto_realops("names abuser %s %s",
			    get_client_name(sptr, FALSE), para);
			sendto_one(sptr, err_str(ERR_TOOMANYTARGETS),
			    me.name, sptr->name, "NAMES");
			return 0;
		}
	}

	chptr = find_channel(para, (aChannel *)NULL);

	if (!chptr || (!ShowChannel(sptr, chptr) && !ValidatePermissionsForPath("override:see:names:secret",sptr,NULL,chptr,NULL)))
	{
		sendto_one(sptr, rpl_str(RPL_ENDOFNAMES), me.name,
		    sptr->name, para);
		return 0;
	}

	/* cache whether this user is a member of this channel or not */
	member = IsMember(sptr, chptr);

	if (PubChannel(chptr))
		buf[0] = '=';
	else if (SecretChannel(chptr))
		buf[0] = '@';
	else
		buf[0] = '*';

	idx = 1;
	buf[idx++] = ' ';
	for (s = chptr->chname; *s; s++)
		buf[idx++] = *s;
	buf[idx++] = ' ';
	buf[idx++] = ':';

	/* If we go through the following loop and never add anything,
	   we need this to be empty, otherwise spurious things from the
	   LAST /names call get stuck in there.. - lucas */
	buf[idx] = '\0';

	spos = idx;		/* starting point in buffer for names! */

	for (cm = chptr->members; cm; cm = cm->next)
	{
		acptr = cm->cptr;
		if (IsInvisible(acptr) && !member && !ValidatePermissionsForPath("override:see:names:invisible",sptr,acptr,chptr,NULL))
			continue;

		for (h = Hooks[HOOKTYPE_VISIBLE_IN_CHANNEL]; h; h = h->next)
		{
			i = (*(h->func.intfunc))(cm->cptr,chptr);
			if (i != 0)
				break;
		}

		if (i != 0)
			if (!is_skochanop(sptr, chptr)
			    && !has_voice(sptr, chptr))
				if (!(cm->
				    flags & (CHFL_CHANOP | CHFL_CHANPROT |
				    CHFL_CHANOWNER | CHFL_HALFOP)) && acptr != sptr)
					continue;

		if (!SupportNAMESX(sptr))
		{
			/* Standard NAMES reply */
#ifdef PREFIX_AQ
			if (cm->flags & CHFL_CHANOWNER)
				buf[idx++] = '~';
			else if (cm->flags & CHFL_CHANPROT)
				buf[idx++] = '&';
			else
#endif
			if (cm->flags & CHFL_CHANOP)
				buf[idx++] = '@';
			else if (cm->flags & CHFL_HALFOP)
				buf[idx++] = '%';
			else if (cm->flags & CHFL_VOICE)
				buf[idx++] = '+';
		} else {
			/* NAMES reply with all rights included (NAMESX) */
#ifdef PREFIX_AQ
			if (cm->flags & CHFL_CHANOWNER)
				buf[idx++] = '~';
			if (cm->flags & CHFL_CHANPROT)
				buf[idx++] = '&';
#endif
			if (cm->flags & CHFL_CHANOP)
				buf[idx++] = '@';
			if (cm->flags & CHFL_HALFOP)
				buf[idx++] = '%';
			if (cm->flags & CHFL_VOICE)
				buf[idx++] = '+';
		}

		if (!uhnames) {
			s = acptr->name;
		} else {
			strlcpy(nuhBuffer,
			        make_nick_user_host(acptr->name, acptr->user->username, GetHost(acptr)),
				bufLen + 1);
			s = nuhBuffer;
		}
		/* 's' is intialized above to point to either acptr->name (normal),
		 * or to nuhBuffer (for UHNAMES).
		 */
		for (; *s; s++)
			buf[idx++] = *s;
		buf[idx++] = ' ';
		buf[idx] = '\0';
		flag = 1;
		if (mlen + idx + bufLen > BUFSIZE - 7)
		{
			sendto_one(sptr, rpl_str(RPL_NAMREPLY), me.name,
			    sptr->name, buf);
			idx = spos;
			flag = 0;
		}
	}

	if (flag)
		sendto_one(sptr, rpl_str(RPL_NAMREPLY), me.name, sptr->name, buf);

	sendto_one(sptr, rpl_str(RPL_ENDOFNAMES), me.name, sptr->name, para);

	return 0;

}
