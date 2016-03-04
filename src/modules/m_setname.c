/*
 *   IRC - Internet Relay Chat, src/modules/m_setname.c
 *   (c) 1999-2001 Dominick Meglio (codemastr) <codemastr@unrealircd.com>
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

CMD_FUNC(m_setname);

#define MSG_SETNAME 	"SETNAME"	/* setname */

ModuleHeader MOD_HEADER(m_setname)
  = {
	"setname",	/* Name of module */
	"4.0", /* Version */
	"command /setname", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_setname)
{
	CommandAdd(modinfo->handle, MSG_SETNAME, m_setname, 1, M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_setname)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_setname)
{
	return MOD_SUCCESS;
}

/* m_setname - 12/05/1999 - Stskeeps
 * :prefix SETNAME :gecos
 * parv[1] - gecos
 * D: This will set your gecos to be <x> (like (/setname :The lonely wanderer))
   yes it is experimental but anyways ;P
    FREEDOM TO THE USERS! ;) 
*/ 
CMD_FUNC(m_setname)
{
    int xx;
    char tmpinfo[REALLEN + 1];
    char spamfilter_user[NICKLEN + USERLEN + HOSTLEN + REALLEN + 64];

 	if ((parc < 2) || BadPtr(parv[1]))
 	{
 		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, sptr->name, "SETNAME");
 		return 0;
	}

	if (strlen(parv[1]) > REALLEN)
	{
		if (MyConnect(sptr))
		{
			sendnotice(sptr, "*** /SetName Error: \"Real names\" may maximum be %i characters of length",
				REALLEN);
		}
		return 0;
	}

    /* set temp info for spamfilter check*/
    strcpy(tmpinfo, sptr->info);
    /* set the new name before we check, but don't send to servers unless it is ok */
    strcpy(sptr->info, parv[1]);
    spamfilter_build_user_string(spamfilter_user, sptr->name, sptr);
    xx = dospamfilter(sptr, spamfilter_user, SPAMF_USER, NULL, 0, NULL);
    if (xx < 0)
    {
        if (xx != FLUSH_BUFFER)
            strcpy(sptr->info, tmpinfo); /* restore (if client wasn't killed already, that is) */
        return xx;
    }

	/* Check for n:lines here too */
	if (!ValidatePermissionsForPath("immune:namecheck",sptr,NULL,NULL,NULL) && Find_ban(NULL, sptr->info, CONF_BAN_REALNAME))
		return exit_client(cptr, sptr, &me,
		                   "Your GECOS (real name) is banned from this server");

	sendto_server(cptr, 0, 0, ":%s SETNAME :%s", sptr->name, parv[1]);

	if (MyConnect(sptr))
		sendnotice(sptr, "Your \"real name\" is now set to be %s - you have to set it manually to undo it",
			parv[1]);

	return 0;
}
