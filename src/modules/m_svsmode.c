/*
 *   IRC - Internet Relay Chat, src/modules/m_svsmode.c
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
#include "proto.h"
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

DLLFUNC int m_svsmode(aClient *cptr, aClient *sptr, int parc, char *parv[]);
DLLFUNC int m_svs2mode(aClient *cptr, aClient *sptr, int parc, char *parv[]);

extern ircstats IRCstats;
#define MSG_SVSMODE 	"SVSMODE"	
#define TOK_SVSMODE 	"n"	
#define MSG_SVS2MODE    "SVS2MODE"
#define TOK_SVS2MODE	"v"

#ifndef DYNAMIC_LINKING
ModuleHeader m_svsmode_Header
#else
#define m_svsmode_Header Mod_Header
ModuleHeader Mod_Header
#endif
  = {
	"m_svsmode",
	"$Id$",
	"command /svsmode and svs2mode", 
	"3.2-b5",
	NULL 
    };

#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Init(int module_load)
#else
int    m_svsmode_Init(int module_load)
#endif
{
	add_Command(MSG_SVSMODE, TOK_SVSMODE, m_svsmode, MAXPARA);
	add_Command(MSG_SVS2MODE, TOK_SVS2MODE, m_svs2mode, MAXPARA);
	return MOD_SUCCESS;
}

#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Load(int module_load)
#else
int m_svsmode_Load(int module_load)
#endif
{
	return MOD_SUCCESS;
}

#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Unload(int module_unload)
#else
int	m_svsmode_Unload(int module_unload)
#endif
{
	if (del_Command(MSG_SVSMODE, TOK_SVSMODE, m_svsmode) < 0 || del_Command(MSG_SVS2MODE, TOK_SVS2MODE, m_svs2mode) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_svsmode_Header.name);
	}
	return MOD_SUCCESS;
}

extern void add_send_mode_param(aChannel *chptr, aClient *from, char what, char mode, char *param);
extern char modebuf[MODEBUFLEN], parabuf[MODEBUFLEN];
int channel_svsmode(aClient *cptr, aClient *sptr, int parc, char *parv[]) 
{
	aChannel *chptr;
	time_t ts;
	aClient *acptr;
	char *m;
	int what;
	
	int i = 4;


	modebuf[0] = 0;
	if(!(chptr = find_channel(parv[1], NULL)))
		return 0;
/*	if (parc >= 4) {
			return 0;
		if (parc > 4) {
			ts = TS2ts(parv[4]);
			if (acptr->since != ts)
				return 0;
		}
	}*/
	ts = TS2ts(parv[parc-1]);
	for(m = parv[2]; *m; m++) {
		switch (*m) {
			case '+':
				what = MODE_ADD;
				break;
			case '-':
				what = MODE_DEL;
				break;
			case 'q': {
				Member *cm;
				for (cm = chptr->members; cm; cm = cm->next) {
					if (cm->flags & CHFL_CHANOWNER) {
						add_send_mode_param(chptr, sptr, '-', 'q', cm->cptr->name);
						cm->flags &= ~CHFL_CHANOWNER;
					}
				}
			}
			break;
			case 'a': {
				Member *cm;
				for (cm = chptr->members; cm; cm = cm->next) {
					if (cm->flags & CHFL_CHANPROT) {
						add_send_mode_param(chptr, sptr, '-', 'a', cm->cptr->name);
						cm->flags &= ~CHFL_CHANPROT;
					}
				}
			}
			break;
			case 'o': {
				Member *cm;
				for (cm = chptr->members; cm; cm = cm->next) {
					if (cm->flags & CHFL_CHANOP) {
						add_send_mode_param(chptr, sptr, '-', 'o', cm->cptr->name);
						cm->flags &= ~CHFL_CHANOP;
					}
				}
			}
			break;
			case 'h': {
				Member *cm;
				for (cm = chptr->members; cm; cm = cm->next) {
					if (cm->flags & CHFL_HALFOP) {
						add_send_mode_param(chptr, sptr, '-', 'h', cm->cptr->name);
						cm->flags &= ~CHFL_HALFOP;
					}
				}
			}
			break;
			case 'v': {
				Member *cm;
				for (cm = chptr->members; cm; cm = cm->next) {
					if (cm->flags & CHFL_VOICE) {
						add_send_mode_param(chptr, sptr, '-', 'v', cm->cptr->name);
						cm->flags &= ~CHFL_VOICE;
					}
				}
			}
			break;
			case 'b': {
				Ban *ban, *bnext;
				if (parc >= i) {
					char uhost[NICKLEN+USERLEN+HOSTLEN+6], vhost[NICKLEN+USERLEN+HOSTLEN+6];
					if (!(acptr = find_person(parv[i-1], NULL))) {
						i++;
						break;
					}
					if (ts && ts != acptr->since) {
						i++;
						break;
					}
					i++;

					strcpy(uhost, make_nick_user_host(acptr->name, 
						acptr->user->username, acptr->user->realhost));
					strcpy(vhost, make_nick_user_host(acptr->name,
						acptr->user->username, acptr->user->virthost));
					ban = chptr->banlist;
					while (ban) {
						bnext = ban->next;
						if (!match(ban->banstr, uhost) || !match(ban->banstr, vhost)) {
							add_send_mode_param(chptr, sptr, '-',  'b', 
								ban->banstr);
							del_banid(chptr, ban->banstr);
						}
						ban = bnext;
					}
				}
				else {
					ban = chptr->banlist;
					while (ban) {
						bnext = ban->next;
						add_send_mode_param(chptr, sptr, '-',  'b', ban->banstr);
						del_banid(chptr, ban->banstr);
						ban = bnext;
					}
				}
			}
			break;
			case 'e': {
				Ban *ban, *bnext;
				if (parc >= i) {
					char uhost[NICKLEN+USERLEN+HOSTLEN+6], vhost[NICKLEN+USERLEN+HOSTLEN+6];
					if (!(acptr = find_person(parv[i-1], NULL))) {
						i++;
						break;
					}
					if (ts && ts != acptr->since) {
						i++;
						break;
					}
					i++;

					strcpy(uhost, make_nick_user_host(acptr->name, 
						acptr->user->username, acptr->user->realhost));
					strcpy(vhost, make_nick_user_host(acptr->name,
						acptr->user->username, acptr->user->virthost));
					ban = chptr->exlist;
					while (ban) {
						bnext = ban->next;
						if (!match(ban->banstr, uhost) || !match(ban->banstr, vhost)) {
							add_send_mode_param(chptr, sptr, '-',  'e', 
								ban->banstr);
							del_exbanid(chptr, ban->banstr);
						}
						ban = bnext;
					}
				}
				else {
					ban = chptr->exlist;
					while (ban) {
						bnext = ban->next;
						add_send_mode_param(chptr, sptr, '-',  'e', ban->banstr);
						del_exbanid(chptr, ban->banstr);
						ban = bnext;
					}
				}
			}
			break;
		}
	}
	if (*parabuf) {
		sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s", sptr->name, chptr->chname, 
			modebuf, parabuf);
		sendto_serv_butone(NULL, ":%s MODE %s %s %s", sptr->name, chptr->chname, modebuf, parabuf);
		*parabuf = 0;
	}
	return 0;
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
	int i;
        char **p, *m;
        aClient *acptr;
        int  what, setflags;

        if (!IsULine(sptr))
                return 0;

        what = MODE_ADD;

        if (parc < 3)
                return 0;

	if (parv[1][0] == '#') 
		return channel_svsmode(cptr, sptr, parc, parv);

        if (!(acptr = find_person(parv[1], NULL)))
                return 0;
        setflags = 0;
        for (i = 0; i <= Usermode_highest; i++)
                if (Usermode_Table[i].flag && (acptr->umodes & Usermode_Table[i].mode))
                        setflags |= Usermode_Table[i].mode;
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
                                  for (i = 0; i <= Usermode_highest; i++)
                                  {
                                  	  if (!Usermode_Table[i].flag)
                                  	  	continue;
                                          if (*m == Usermode_Table[i].flag)
                                          {
                                                  if (what == MODE_ADD)
                                                          acptr->umodes |= Usermode_Table[i].mode;
                                                  else
                                                          acptr->umodes &=
                                                              ~Usermode_Table[i].mode;
                                                  break;
                                          }
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
        int  i;
        char **p, *m;
        aClient *acptr;
        int  what, setflags;
	char buf[BUFSIZE];


        if (!IsULine(sptr))
                return 0;

        what = MODE_ADD;

        if (parc < 3)
                return 0;

	if (parv[1][0] == '#') 
		return channel_svsmode(cptr, sptr, parc, parv);

        if (!(acptr = find_person(parv[1], NULL)))
                return 0;

        setflags = 0;
        for (i = 0; i <= Usermode_highest; i++)
                if (Usermode_Table[i].flag && (acptr->umodes & Usermode_Table[i].mode))
                        setflags |= Usermode_Table[i].mode;
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
                                  for (i = 0; i <= Usermode_highest; i++)
                                  {
                                  	  if (!Usermode_Table[i].flag)
                                  	  	continue;
                                          if (*m == Usermode_Table[i].flag)
                                          {
                                                  if (what == MODE_ADD)
                                                          acptr->umodes |= Usermode_Table[i].mode;
                                                  else
                                                          acptr->umodes &=
                                                              ~Usermode_Table[i].mode;
                                                  break;
                                            
                                          }
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
