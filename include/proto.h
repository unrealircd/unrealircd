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
/* lusers.c */
extern void init_irccounts(void);

/* match.c */
extern char *collapse(char *pattern);

/* scache.c */
extern void clear_scache_hash_table(void);

/* send.c */
extern void sendto_one(Client *, MessageTag *mtags, FORMAT_STRING(const char *), ...) __attribute__((format(printf,3,4)));
extern void sendto_realops(FORMAT_STRING(const char *pattern), ...) __attribute__((format(printf,1,2)));

/* ircd.c */
extern EVENT(garbage_collect);
extern EVENT(loop_event);
extern EVENT(check_pings);
extern EVENT(handshake_timeout);
extern EVENT(check_deadsockets);
extern EVENT(try_connections);
/* support.c */
extern char *my_itoa(int i);

/* s_serv.c */
extern void load_tunefile(void);
extern EVENT(save_tunefile);
extern void read_motd(const char *filename, MOTDFile *motd);

/* s_user.c */
extern int target_limit_exceeded(Client *client, void *target, const char *name);
extern void make_umodestr(void);
extern char *get_usermode_string(Client *acptr);

/* s_misc.c */
extern char *convert_time(time_t ltime);

/* whowas.c */
extern void initwhowas(void);

/* uid.c */
extern void uid_init(void);
extern const char *uid_get(void);

#endif /* proto_h */
