/*
 *   IRC - Internet Relay Chat, src/modules/m_svsmotd.c
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

CMD_FUNC(m_svsmotd);

#define MSG_SVSMOTD 	"SVSMOTD"	

ModuleHeader MOD_HEADER(m_svsmotd)
  = {
	"m_svsmotd",
	"4.0",
	"command /svsmotd", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_svsmotd)
{
	CommandAdd(modinfo->handle, MSG_SVSMOTD, m_svsmotd, MAXPARA, M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_svsmotd)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_svsmotd)
{
	return MOD_SUCCESS;
}

/*
** m_svsmotd
**
*/
CMD_FUNC(m_svsmotd)
{
        FILE *conf = NULL;

        if (!IsULine(sptr))
        {
                sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
                return 0;
        }
        if (parc < 2)
        {
                sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
                    me.name, sptr->name, "SVSMOTD");
                return 0;
        }

        if ((*parv[1] != '!') && parc < 3)
        {
                sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
                    me.name, sptr->name, "SVSMOTD");
                return 0;
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
                  return 0;
        }
        if (parv[2])
                sendto_server(cptr, 0, 0, ":%s SVSMOTD %s :%s", sptr->name, parv[1], parv[2]);
        else
                sendto_server(cptr, 0, 0, ":%s SVSMOTD %s", sptr->name, parv[1]);

        if (conf == NULL)
        {
                return 0;
        }

        if (parc < 3 && (*parv[1] == '!'))
        {
                fclose(conf);
                return 1;
        }
        fprintf(conf, "%s\n", parv[2]);
        if (*parv[1] == '!')
                sendto_ops("Added '%s' to services motd", parv[2]);

        fclose(conf);
        /* We editted it, so rehash it -- codemastr */
        read_motd(conf_files->svsmotd_file, &svsmotd);
        return 1;
}
