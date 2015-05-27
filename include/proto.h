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
extern int  sendmodeto_one(aClient *cptr, char *from, char *name, char *mode, char *param, TS creationtime);
extern void make_cmodestr(void);
extern int  is_halfop(aClient *cptr, aChannel *chptr);
extern int  is_chanprot(aClient *cptr, aChannel *chptr);

/* lusers.c */
extern void init_ircstats(void);

/* match.c */
extern char *collapse(char *pattern);

/* scache.c */
extern void clear_scache_hash_table(void);

/* send.c */
extern void sendto_one(aClient *, char *, ...) __attribute__((format(printf,2,3)));
extern void sendto_chanops_butone(aClient *one, aChannel *chptr, char *pattern, ...) __attribute__((format(printf,3,4)));
extern void sendto_realops(char *pattern, ...) __attribute__((format(printf,1,2)));
extern void sendto_channel_ntadmins(aClient *from, aChannel *chptr, char *pattern, ...) __attribute__((format(printf,3,4))); 

/* fdlist.c */
extern EVENT(lcf_check);
extern EVENT(htm_calc);
/* ircd.c */
extern EVENT(garbage_collect);
extern EVENT(loop_event);
extern EVENT(check_pings);
extern EVENT(check_unknowns);
extern EVENT(check_deadsockets);
extern EVENT(try_connections);
/* support.c */
extern char *my_itoa(int i);

/* s_serv.c */
extern void load_tunefile(void);
extern EVENT(save_tunefile);
extern void read_motd(const char *filename, aMotdFile *motd);

/* s_user.c */
extern int  check_for_target_limit(aClient *sptr, void *target, const char *name);
extern void make_umodestr(void);
extern char *get_mode_str(aClient *acptr);

/* s_misc.c */
extern char *convert_time(time_t ltime);

/* whowas.c */
extern void initwhowas(void);

/* uid.c */
extern void uid_init(void);
extern const char *uid_get(void);

#endif /* proto_h */
