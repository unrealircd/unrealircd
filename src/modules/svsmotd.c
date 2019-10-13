/*
 *   IRC - Internet Relay Chat, src/modules/svsmotd.c
 *   (C) 2001 The UnrealIRCd Team
 *
 *   SVSMOTD command
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

CMD_FUNC(cmd_svsmotd);

#define MSG_SVSMOTD 	"SVSMOTD"	

ModuleHeader MOD_HEADER
  = {
	"svsmotd",
	"5.0",
	"command /svsmotd", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_SVSMOTD, cmd_svsmotd, MAXPARA, CMD_SERVER);
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
** cmd_svsmotd
**
*/
CMD_FUNC(cmd_svsmotd)
{
        FILE *conf = NULL;

        if (!IsULine(client))
        {
                sendnumeric(client, ERR_NOPRIVILEGES);
                return;
        }
        if (parc < 2)
        {
                sendnumeric(client, ERR_NEEDMOREPARAMS, "SVSMOTD");
                return;
        }

        if ((*parv[1] != '!') && parc < 3)
        {
                sendnumeric(client, ERR_NEEDMOREPARAMS, "SVSMOTD");
                return;
        }

        switch (*parv[1])
        {
          case '#':
                  conf = fopen(conf_files->svsmotd_file, "a");
                  sendto_ops("Added '%s' to services motd", parv[2]);
                  break;
          case '!':
          {
                  remove(conf_files->svsmotd_file);
                  free_motd(&svsmotd);
                  sendto_ops("Wiped out services motd data");
                  break;
          }
          default:
                  return;
        }
        if (parv[2])
                sendto_server(client, 0, 0, NULL, ":%s SVSMOTD %s :%s", client->id, parv[1], parv[2]);
        else
                sendto_server(client, 0, 0, NULL, ":%s SVSMOTD %s", client->id, parv[1]);

        if (conf == NULL)
                return;

        if (parc < 3 && (*parv[1] == '!'))
        {
                fclose(conf);
                return;
        }
        fprintf(conf, "%s\n", parv[2]);
        if (*parv[1] == '!')
                sendto_ops("Added '%s' to services motd", parv[2]);

        fclose(conf);
        /* We editted it, so rehash it -- codemastr */
        read_motd(conf_files->svsmotd_file, &svsmotd);
}
