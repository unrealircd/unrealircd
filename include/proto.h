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
/* badwords.c */
int  loadbadwords_message PROTO((char *wordfile));
int  loadbadwords_channel PROTO((char *wordfile));

/* channel.c */
int  sendmodeto_one PROTO((aClient *cptr, char *from, char *name, char *mode, char *param, TS creationtime));
void make_cmodestr PROTO((void));

/* dynconf.c */
int  load_conf PROTO((char *filename, int type));
int  load_conf2 PROTO((FILE * conf, char *filename, int type));
int  load_conf3 PROTO((FILE * conf, char *filename, int type));
void init_dynconf PROTO((void));
void doneconf PROTO((int type));

/* lusers.c */
void init_ircstats PROTO((void));

/* match.c */
char *collapse PROTO((char *pattern));

/* scache.c */
void clear_scache_hash_table PROTO((void));

/* send.c */
void sendto_one PROTO((aClient *, char *, ...));
void sendto_chanops_butone PROTO((aClient *one, aChannel *chptr, char *pattern, ...));
void sendto_realops PROTO((char *pattern, ...));
void sendto_serv_butone_token PROTO((aClient *one, char *prefix, char *command, char *token, char *pattern, ...));
void sendto_serv_butone_token_opt PROTO((aClient *one, int opt, char *prefix, char *command, char *token, char *pattern, ...));
void sendto_channel_ntadmins PROTO((aClient *from, aChannel *chptr, char *pattern, ...)); 

/* support.c */
char *my_itoa PROTO((int i));

/* s_conf.c */
int  find_nline PROTO((aClient *cptr));

/* s_extra.c */
int  channel_canjoin PROTO((aClient *sptr, char *name));
int  dcc_loadconf PROTO((void));
int  cr_loadconf PROTO((void));
int  vhost_loadconf PROTO((void));

/* s_kline.c */
int  find_tkline_match PROTO((aClient *cptr, int xx));
void tkl_check_expire PROTO((void));
int  tkl_sweep PROTO((void));

/* s_serv.c */
void load_tunefile PROTO((void));
void save_tunefile PROTO((void));
aMotd *read_botmotd PROTO((char *filename));
aMotd *read_rules PROTO((char *filename));
aMotd *read_opermotd PROTO((char *filename));
aMotd *read_motd PROTO((char *filename));
aMotd *read_svsmotd PROTO((char *filename));
void read_tlines PROTO((void));

/* s_unreal.c */
void unrealmanual PROTO((void));

/* s_user.c */
int  check_for_target_limit PROTO((aClient *sptr, void *target, const char *name));
void make_umodestr PROTO((void));

/* webtv.c */
int  is_halfop PROTO((aClient *cptr, aChannel *chptr));
int  is_chanprot PROTO((aClient *cptr, aChannel *chptr));
char *convert_time PROTO((time_t ltime));
char *get_mode_str PROTO((aClient *acptr));

/* whowas.c */
void initwhowas PROTO((void));
#endif /* proto_h */
