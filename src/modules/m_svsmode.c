/*
 *   IRC - Internet Relay Chat, src/modules/scan.c
 *   (C) 2001 The UnrealIRCd Team
 *
 *   SVSMODE and SVS2MODE commands
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
#include "channel.h"
#include "userload.h"
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

DLLFUNC int m_svsmode(aClient *cptr, aClient *sptr, int parc, char *parv[]);
DLLFUNC int m_svs2mode(aClient *cptr, aClient *sptr, int parc, char *parv[]);

extern ircstats IRCstats;
extern int user_modes[];
#define MSG_SVSMODE 	"SVSMODE"	
#define TOK_SVSMODE 	"n"	
#define MSG_SVS2MODE    "SVS2MODE"
#define TOK_SVS2MODE	"v"

#ifndef DYNAMIC_LINKING
ModuleInfo m_svsmode_info
#else
#define m_svsmode_info mod_header
ModuleInfo mod_header
#endif
  = {
  	2,
	"test",
	"$Id$",
	"command /svsmode and svs2mode", 
	NULL,
	NULL 
    };

#ifdef DYNAMIC_LINKING
DLLFUNC int	mod_init(int module_load)
#else
int    m_svsmode_init(int module_load)
#endif
{
	add_Command(MSG_SVSMODE, TOK_SVSMODE, m_svsmode, MAXPARA);
	add_Command(MSG_SVS2MODE, TOK_SVS2MODE, m_svs2mode, MAXPARA);
}

#ifdef DYNAMIC_LINKING
DLLFUNC int	mod_load(int module_load)
#else
int m_svsmode_load(int module_load)
#endif
{
}

#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_unload(void)
#else
void	m_svsmode_unload(void)
#endif
{
	if (del_Command(MSG_SVSMODE, TOK_SVSMODE, m_svsmode) < 0 || del_Command(MSG_SVS2MODE, TOK_SVS2MODE, m_svs2mode) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_svsmode_info.name);
	}
}
/*
 * m_svsmode() added by taz
 * parv[0] - sender
 * parv[1] - username to change mode for
 * parv[2] - modes to change
 * parv[3] - Service Stamp (if mode == d)
 */
int  m_svsmode(cptr, sptr, parc, parv)
        aClient *cptr, *sptr;
        int  parc;
        char *parv[];
{
        int  flag;
        int *s;
        char **p, *m;
        aClient *acptr;
        int  what, setflags;

        if (!IsULine(sptr))
                return 0;

        what = MODE_ADD;

        if (parc < 3)
                return 0;

        if (!(acptr = find_person(parv[1], NULL)))
                return 0;
        setflags = 0;
        for (s = user_modes; (flag = *s); s += 2)
                if (acptr->umodes & flag)
                        setflags |= flag;
        /*
         * parse mode change string(s)
         */
        for (p = &parv[2]; p && *p; p++)
                for (m = *p; *m; m++)
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
                          case '\n':
                          case '\r':
                          case '\t':
                                  break;
                          case 'i':
                                  if (what == MODE_ADD
                                      && !(acptr->umodes & UMODE_INVISIBLE))
                                  {
                                          IRCstats.invisible++;
                                  }
                                  if (what == MODE_DEL
                                      && (acptr->umodes & UMODE_INVISIBLE))
                                  {

                                          IRCstats.invisible--;
                                  }
                                  goto setmodex;
                          case 'o':
                                  if (what == MODE_ADD
                                      && !(acptr->umodes & UMODE_OPER))
                                          IRCstats.operators++;
                                  if (what == MODE_DEL
                                      && (acptr->umodes & UMODE_OPER))
                                          IRCstats.operators--;
                                  goto setmodex;
                          case 'd':
                                  if (parv[3] && isdigit(*parv[3]))
                                  {
                                          acptr->user->servicestamp =
                                                strtoul(parv[3], NULL, 10);

                                          break;
                                  }
                          default:
                                setmodex:
                                  for (s = user_modes; (flag = *s); s += 2)
                                          if (*m == (char)(*(s + 1)))
                                          {
                                                  if (what == MODE_ADD)
                                                          acptr->umodes |= flag;
                                                  else
                                                          acptr->umodes &=
                                                              ~flag;
                                                  break;
                                          }
                                  break;
                        }
        if (parc > 3)
                sendto_serv_butone_token(cptr, parv[0], MSG_SVSMODE,
                    TOK_SVSMODE, "%s %s %s", parv[1], parv[2], parv[3]);
        else
                sendto_serv_butone_token(cptr, parv[0], MSG_SVSMODE,
                    TOK_SVSMODE, "%s %s", parv[1], parv[2]);

        return 0;
}

/*
 * m_svs2mode() added by Potvin
 * parv[0] - sender
 * parv[1] - username to change mode for
 * parv[2] - modes to change
 * parv[3] - Service Stamp (if mode == d)
 */
int  m_svs2mode(cptr, sptr, parc, parv)
        aClient *cptr, *sptr;
        int  parc;
        char *parv[];
{
        int  flag;
        int *s;
        char **p, *m;
        aClient *acptr;
        int  what, setflags;
	char buf[BUFSIZE];


        if (!IsULine(sptr))
                return 0;

        what = MODE_ADD;

        if (parc < 3)
                return 0;

        if (!(acptr = find_person(parv[1], NULL)))
                return 0;

        setflags = 0;
        for (s = user_modes; (flag = *s); s += 2)
                if (acptr->umodes & flag)
                        setflags |= flag;
        /*
         * parse mode change string(s)
         */
        for (p = &parv[2]; p && *p; p++)
                for (m = *p; *m; m++)
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
                          case '\n':
                          case '\r':
                          case '\t':
                                  break;
                          case 'd':
                                  if (parv[3] && (isdigit(*parv[3])))
                                  {
                                        acptr->user->servicestamp =
                                               strtoul(parv[3], NULL, 10);
                                  }
                                  break;
                          case 'i':
                                  if (what == MODE_ADD
                                      && !(acptr->umodes & UMODE_INVISIBLE))
                                  {
                                          IRCstats.invisible++;
                                  }
                                  if (what == MODE_DEL
                                      && (acptr->umodes & UMODE_INVISIBLE))
                                  {
                                          IRCstats.invisible--;
                                  }
                                  goto setmodey;
                          case 'o':
                                  if (acptr->srvptr->flags & FLAGS_QUARANTINE)
                                        break;
                                  if (what == MODE_ADD
                                      && !(acptr->umodes & UMODE_OPER))
                                          IRCstats.operators++;
                                  if (what == MODE_DEL
                                      && (acptr->umodes & UMODE_OPER))
                                          IRCstats.operators--;
                          default:
                                setmodey:
                                  for (s = user_modes; (flag = *s); s += 2)
                                          if (*m == (char)(*(s + 1)))
                                          {
                                                  if (what == MODE_ADD)
                                                          acptr->umodes |= flag;
                                                  else
                                                          acptr->umodes &=
                                                              ~flag;
                                                  break;
                                          }
                                  break;
                        }

        if (parc > 3)
                sendto_serv_butone_token(cptr, parv[0], MSG_SVS2MODE,
                    TOK_SVS2MODE, "%s %s %s", parv[1], parv[2], parv[3]);
        else
                sendto_serv_butone_token(cptr, parv[0], MSG_SVS2MODE,
                    TOK_SVS2MODE, "%s %s", parv[1], parv[2]);

        send_umode(NULL, acptr, setflags, ALL_UMODES, buf);
        if (MyClient(acptr) && buf[0] && buf[1])
                sendto_one(acptr, ":%s MODE %s :%s", parv[0], parv[1], buf);
        return 0;
}
