/*
 *   IRC - Internet Relay Chat, src/modules/userip.c
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

CMD_FUNC(cmd_userip);

#define MSG_USERIP 	"USERIP"	

ModuleHeader MOD_HEADER
  = {
	"userip",
	"5.0",
	"command /userip", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_USERIP, cmd_userip, 1, CMD_USER);
	ISupportAdd(modinfo->handle, "USERIP", NULL);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

/*
 * cmd_userip is based on cmd_userhost
 * cmd_userhost added by Darren Reed 13/8/91 to aid clients and reduce
 * the need for complicated requests like WHOIS. It returns user/host
 * information only (no spurious AWAY labels or channels).
 * Re-written by Dianora 1999
 */
/* Keep this at 5!!!! */
#define MAXUSERHOSTREPLIES 5
CMD_FUNC(cmd_userip)
{

	char *p;		/* scratch end pointer */
	char *cn;		/* current name */
	char *ip, ipbuf[HOSTLEN+1];
	Client *acptr;
	char request[BUFSIZE];
	char response[MAXUSERHOSTREPLIES][NICKLEN * 2 + CHANNELLEN + USERLEN + HOSTLEN + 30];
	int  i;			/* loop counter */
	int w;

	if (!MyUser(client))
		return;
		
	if (parc < 2)
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "USERIP");
		return;
	}

	/* The idea is to build up the response string out of pieces
	 * none of this strlen() nonsense.
	 * MAXUSERHOSTREPLIES * (NICKLEN*2+CHANNELLEN+USERLEN+HOSTLEN+30) is still << sizeof(buf)
	 * and our ircsnprintf() truncates it to fit anyway. There is
	 * no danger of an overflow here. -Dianora
	 */
	response[0][0] = response[1][0] = response[2][0] = response[3][0] = response[4][0] = '\0';

	strlcpy(request, parv[1], sizeof(request));
	cn = request;

	for (w = 0, i = 0; (i < MAXUSERHOSTREPLIES) && cn; i++)
	{
		if ((p = strchr(cn, ' ')))
			*p = '\0';

		if ((acptr = find_user(cn, NULL)))
		{
			if (!(ip = GetIP(acptr)))
				ip = "<unknown>";
			if (client != acptr && !ValidatePermissionsForPath("client:see:ip",client,acptr,NULL,NULL) && IsHidden(acptr))
			{
				make_cloakedhost(acptr, GetIP(acptr), ipbuf, sizeof(ipbuf));
				ip = ipbuf;
			}

			ircsnprintf(response[w], NICKLEN * 2 + CHANNELLEN + USERLEN + HOSTLEN + 30, "%s%s=%c%s@%s",
			    acptr->name,
			    (IsOper(acptr) && (!IsHideOper(acptr) || client == acptr || IsOper(client)))
				? "*" : "",
			    (acptr->user->away) ? '-' : '+',
			    acptr->user->username, ip);
			w++;
		}
		if (p)
			p++;
		cn = p;
	}

	sendnumeric(client, RPL_USERIP, response[0], response[1], response[2], response[3], response[4]);
}
