/*
 *   IRC - Internet Relay Chat, src/modules/m_sapart.c
 *   (C) 2004 The UnrealIRCd Team
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

CMD_FUNC(m_sapart);

#define MSG_SAPART 	"SAPART"	

ModuleHeader MOD_HEADER(m_sapart)
  = {
	"m_sapart",
	"4.0",
	"command /sapart", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_sapart)
{
	CommandAdd(modinfo->handle, MSG_SAPART, m_sapart, 3, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_sapart)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_sapart)
{
	return MOD_SUCCESS;
}

/* m_sapart() - Lamego - Wed Jul 21 20:04:48 1999
   Copied off PTlink IRCd (C) PTlink coders team.
   Coded for Sadmin by Stskeeps
   also Modified by NiQuiL (niquil@programmer.net)
	parv[1] - nick to make part
	parv[2] - channel(s) to part
	parv[3] - comment
*/

CMD_FUNC(m_sapart)
{
	aClient *acptr;
	aChannel *chptr;
	Membership *lp;
	char *name, *p = NULL;
	int i;
	char *comment = (parc > 3 && parv[3] ? parv[3] : NULL);
	char commentx[512];
	char jbuf[BUFSIZE];

	if (parc < 3)
        {
                sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, sptr->name, "SAPART");
                return 0;
        }

        if (!(acptr = find_person(parv[1], NULL)))
        {
                sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, sptr->name, parv[1]);
                return 0;
        }

	/* See if we can operate on this vicim/this command */
	if (!ValidatePermissionsForPath("sapart",sptr,acptr,NULL,NULL))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
		return 0;
	}

	if (MyClient(acptr))
	{
		/* Now works like m_join */
		*jbuf = 0;

		for (i = 0, name = strtoken(&p, parv[2], ","); name; name = strtoken(&p,
			NULL, ","))
		{
			if (!(chptr = get_channel(acptr, name, 0)))
			{
				sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL), me.name, sptr->name,
					name);
				continue;
			}

			/* Validate oper can do this on chan/victim */
			if (!IsULine(sptr) && !ValidatePermissionsForPath("sapart",sptr,acptr,chptr,NULL))
        		{
                		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
				continue;
        		}
	
			if (!(lp = find_membership_link(acptr->user->channel, chptr)))
			{
				sendto_one(sptr, err_str(ERR_USERNOTINCHANNEL), me.name, sptr->name,
					parv[1], name);
				continue;
			}
			if (*jbuf)
				(void)strlcat(jbuf, ",", sizeof jbuf);
			(void)strlncat(jbuf, name, sizeof jbuf, sizeof(jbuf) - i - 1);
			i += strlen(name) + 1;
		}

		if (!*jbuf)
			return -1;

		strcpy(parv[2], jbuf);

		if (comment)
		{
			strcpy(commentx, "SAPart: ");
			strlcat(commentx, comment, 512);
		}

		parv[0] = acptr->name; // nick
		parv[1] = parv[2]; // chan
		parv[2] = comment ? commentx : NULL; // comment
		if (comment)
		{
			sendnotice(acptr,
			    "*** You were forced to part %s (%s)",
			    parv[1], commentx);
			sendto_realops("%s used SAPART to make %s part %s (%s)", sptr->name, acptr->name,
				parv[1], comment);
			sendto_server(&me, 0, 0, ":%s GLOBOPS :%s used SAPART to make %s part %s (%s)",
				me.name, sptr->name, acptr->name, parv[1], comment);
			/* Logging function added by XeRXeS */
			ircd_log(LOG_SACMDS,"SAPART: %s used SAPART to make %s part %s (%s)",
				sptr->name, acptr->name, parv[1], comment);
		}
		else
		{
			sendnotice(acptr,
			    "*** You were forced to part %s", parv[1]);
			sendto_realops("%s used SAPART to make %s part %s", sptr->name, acptr->name,
				parv[1]);
			sendto_server(&me, 0, 0, ":%s GLOBOPS :%s used SAPART to make %s part %s",
				me.name, sptr->name, acptr->name, parv[1]);
			/* Logging function added by XeRXeS */
			ircd_log(LOG_SACMDS,"SAPART: %s used SAPART to make %s part %s",
				sptr->name, acptr->name, parv[1]);
		}
		(void)do_cmd(acptr, acptr, "PART", comment ? 3 : 2, parv);
		/* acptr may be killed now due to the part reason @ spamfilter */
	}
	else
	{
		if (comment)
		{
			sendto_one(acptr, ":%s SAPART %s %s :%s", sptr->name,
			    parv[1], parv[2], comment);
			/* Logging function added by XeRXeS */
			ircd_log(LOG_SACMDS,"SAPART: %s used SAPART to make %s part %s (%s)",
				sptr->name, parv[1], parv[2], comment);
		}
		else
		{
			sendto_one(acptr, ":%s SAPART %s %s", sptr->name, parv[1],
				   parv[2]);
			/* Logging function added by XeRXeS */
			ircd_log(LOG_SACMDS,"SAPART: %s used SAPART to make %s part %s",
				sptr->name, parv[1], parv[2]);
		}
	}

	return 0;
}
