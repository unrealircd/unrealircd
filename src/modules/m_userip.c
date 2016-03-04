/*
 *   IRC - Internet Relay Chat, src/modules/m_userip.c
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

CMD_FUNC(m_userip);

#define MSG_USERIP 	"USERIP"	

ModuleHeader MOD_HEADER(m_userip)
  = {
	"m_userip",
	"4.0",
	"command /userip", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_userip)
{
	CommandAdd(modinfo->handle, MSG_USERIP, m_userip, 1, M_USER|M_ANNOUNCE);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_userip)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_userip)
{
	return MOD_SUCCESS;
}

/*
 * m_userip is based on m_userhost
 * m_userhost added by Darren Reed 13/8/91 to aid clients and reduce
 * the need for complicated requests like WHOIS. It returns user/host
 * information only (no spurious AWAY labels or channels).
 * Re-written by Dianora 1999
 */
CMD_FUNC(m_userip)
{

	char *p;		/* scratch end pointer */
	char *cn;		/* current name */
	char *ip, ipbuf[HOSTLEN+1];
	struct Client *acptr;
	char response[5][NICKLEN * 2 + CHANNELLEN + USERLEN + HOSTLEN + 30];
	int  i;			/* loop counter */

	if (!MyClient(sptr))
		return -1;
		
	if (parc < 2)
	{
		sendto_one(sptr, rpl_str(ERR_NEEDMOREPARAMS),
		    me.name, sptr->name, "USERIP");
		return 0;
	}

	/* The idea is to build up the response string out of pieces
	 * none of this strlen() nonsense.
	 * 5 * (NICKLEN*2+CHANNELLEN+USERLEN+HOSTLEN+30) is still << sizeof(buf)
	 * and our ircsnprintf() truncates it to fit anyway. There is
	 * no danger of an overflow here. -Dianora
	 */
	response[0][0] = response[1][0] = response[2][0] =
	    response[3][0] = response[4][0] = '\0';

	cn = parv[1];

	for (i = 0; (i < 5) && cn; i++)
	{
		if ((p = strchr(cn, ' ')))
			*p = '\0';

		if ((acptr = find_person(cn, NULL)))
		{
			if (!(ip = GetIP(acptr)))
				ip = "<unknown>";
			if (sptr != acptr && !ValidatePermissionsForPath("client:ip",sptr,acptr,NULL,NULL) && IsHidden(acptr))
			{
				make_virthost(acptr, GetIP(acptr), ipbuf, 0);
				ip = ipbuf;
			}

			ircsnprintf(response[i], NICKLEN * 2 + CHANNELLEN + USERLEN + HOSTLEN + 30, "%s%s=%c%s@%s",
			    acptr->name,
			    (IsOper(acptr) && (!IsHideOper(acptr) || sptr == acptr || IsOper(sptr)))
				? "*" : "",
			    (acptr->user->away) ? '-' : '+',
			    acptr->user->username, ip);
			/* add extra fakelag (penalty) because of all the work we need to do: 1s per entry: */
			sptr->local->since += 1;
		}
		if (p)
			p++;
		cn = p;
	}

	sendto_one(sptr, rpl_str(RPL_USERIP), me.name, sptr->name,
	    response[0], response[1], response[2], response[3], response[4]);

	return 0;
}
