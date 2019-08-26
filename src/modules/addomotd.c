/*
 *   IRC - Internet Relay Chat, src/modules/out.c
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

CMD_FUNC(m_addomotd);

#define MSG_ADDOMOTD 	"ADDOMOTD"	

ModuleHeader MOD_HEADER(addomotd)
  = {
	"addomotd",
	"5.0",
	"command /addomotd", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_INIT(addomotd)
{
	CommandAdd(modinfo->handle, MSG_ADDOMOTD, m_addomotd, 1, M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(addomotd)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(addomotd)
{
	return MOD_SUCCESS;
}

/*
** m_addomotd (write a line to opermotd)
**
** De-Potvinized by codemastr
*/
CMD_FUNC(m_addomotd)
{
	FILE *conf;
	char *text;

	text = parc > 1 ? parv[1] : NULL;

	if (!MyConnect(sptr))
		return 0;

	if (!ValidatePermissionsForPath("server:addomotd",sptr,NULL,NULL,NULL))
	{
		sendnumeric(sptr, ERR_NOPRIVILEGES);
		return 0;
	}
	if (parc < 2)
	{
		sendnumeric(sptr, ERR_NEEDMOREPARAMS, "ADDOMOTD");
		return 0;
	}
	conf = fopen(conf_files->opermotd_file, "a");
	if (conf == NULL)
	{
		return 0;
	}
	sendnotice(sptr, "*** Wrote (%s) to OperMotd", text);
	/*      for (i=1 ; i<parc ; i++)
	   {
	   if (i!=parc-1)
	   fprintf (conf,"%s ",parv[i]);
	   else
	   fprintf (conf,"%s\n",parv[i]);
	   } */
	fprintf(conf, "%s\n", text);

	fclose(conf);
	return 1;
}
