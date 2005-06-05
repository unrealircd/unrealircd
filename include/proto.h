/************************************************************************
 *   Unreal Internet Relay Chat Daemon, include/proto.h
 *      (C) Dominick Meglio <codemastr@unrealircd.com> 2000
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

#ifndef proto_h
#define proto_h
/* channel.c */
int  sendmodeto_one(aClient *cptr, char *from, char *name, char *mode, char *param, TS creationtime);
void make_cmodestr(void);

/* lusers.c */
void init_ircstats(void);

/* match.c */
char *collapse(char *pattern);

/* scache.c */
void clear_scache_hash_table(void);

/* send.c */
void sendto_one(aClient *, char *, ...) __attribute__((format(printf,2,3)));
void sendto_chanops_butone(aClient *one, aChannel *chptr, char *pattern, ...) __attribute__((format(printf,3,4)));
void sendto_realops(char *pattern, ...) __attribute__((format(printf,1,2)));
void sendto_serv_butone_token(aClient *one, char *prefix, char *command, 
                              char *token, char *pattern, ...) __attribute__((format(printf,5,6)));
void sendto_serv_butone_token_opt(aClient *one, int opt, char *prefix, 
                                  char *command, char *token, char *pattern, ...) __attribute__((format(printf,6,7)));
void sendto_channel_ntadmins(aClient *from, aChannel *chptr, char *pattern, ...) __attribute__((format(printf,3,4))); 

/* fdlist.c */
EVENT(lcf_check);
EVENT(htm_calc);
/* ircd.c */
EVENT(e_check_fdlists);
EVENT(garbage_collect);
EVENT(loop_event);
/* support.c */
char *my_itoa(int i);

/* s_serv.c */
void load_tunefile(void);
extern EVENT(save_tunefile);
aMotd *read_rules(char *filename);
aMotd *read_motd(char *filename);

/* s_user.c */
int  check_for_target_limit(aClient *sptr, void *target, const char *name);
void make_umodestr(void);

/* webtv.c */
int  is_halfop(aClient *cptr, aChannel *chptr);
int  is_chanprot(aClient *cptr, aChannel *chptr);
char *convert_time(time_t ltime);
char *get_mode_str(aClient *acptr);

/* whowas.c */
void initwhowas(void);
#endif /* proto_h */
