/************************************************************************
 *   Unreal Internet Relay Chat Daemon, include/h.h
 *   Copyright (C) 1992 Darren Reed
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
 *
 *   $Id$
 */

/*
 * "h.h". - Headers file.
 *
 * Most of the externs and prototypes thrown in here to 'cleanup' things.
 * -avalon
 */
#ifndef NO_FDLIST
#include "fdlist.h"
#endif

/* for the new s_err.c */
extern char *getreply(int);
#define rpl_str(x) getreply(x)
#define err_str(x) getreply(x)

extern TS nextconnect, nextdnscheck, nextping;
extern aClient *client, me, *local[];
extern aChannel *channel;
extern struct stats *ircstp;
extern int bootopt;
extern TS TSoffset;
/* Prototype added to force errors -- Barubary */
extern TS check_pings(TS now, int check_kills);
extern TS TS2ts(char *s);
/* newconf */
/*
 * Configuration linked lists
*/
extern ConfigItem_me		*conf_me;
extern ConfigItem_class 	*conf_class;
extern ConfigItem_admin 	*conf_admin;
extern ConfigItem_drpass	*conf_drpass;
extern ConfigItem_ulines	*conf_ulines;
extern ConfigItem_tld		*conf_tld;
extern ConfigItem_oper		*conf_oper;
extern ConfigItem_listen	*conf_listen;
extern ConfigItem_allow	*conf_allow;
extern ConfigItem_except	*conf_except;
extern ConfigItem_vhost	*conf_vhost;
extern ConfigItem_link		*conf_link;

ConfigItem_class	*Find_class(char *name);
ConfigItem_oper		*Find_oper(char *name);
ConfigItem_listen	*Find_listen(char *ipmask, int port);

/* Remmed out for win32 compatibility.. as stated of 467leaf win32 port.. */

extern LoopStruct loop;

#ifdef SHOWCONNECTINFO

#ifdef SOCKSPORT
#define BREPORT_DO_SOCKS "NOTICE AUTH :*** Checking for open socks server...\r\n"
#define BREPORT_GOOD_SOCKS "NOTICE AUTH :*** Secure socks found (good!)...\r\n"
#define BREPORT_NO_SOCKS "NOTICE AUTH :*** No socks server found (good!)...\r\n"
#endif

#define BREPORT_DO_DNS	"NOTICE AUTH :*** Looking up your hostname...\r\n"
#define BREPORT_FIN_DNS	"NOTICE AUTH :*** Found your hostname\r\n"
#define BREPORT_FIN_DNSC "NOTICE AUTH :*** Found your hostname (cached)\r\n"
#define BREPORT_FAIL_DNS "NOTICE AUTH :*** Couldn't resolve your hostname; using your IP address instead\r\n"
#define BREPORT_DO_ID	"NOTICE AUTH :*** Checking ident...\r\n"
#define BREPORT_FIN_ID	"NOTICE AUTH :*** Received identd response\r\n"
#define BREPORT_FAIL_ID	"NOTICE AUTH :*** No ident response; username prefixed with ~\r\n"

extern char REPORT_DO_DNS[128], REPORT_FIN_DNS[128], REPORT_FIN_DNSC[128],
    REPORT_FAIL_DNS[128], REPORT_DO_ID[128], REPORT_FIN_ID[128],
    REPORT_FAIL_ID[128];
#ifdef SOCKSPORT
extern char REPORT_DO_SOCKS[128], REPORT_GOOD_SOCKS[128], REPORT_NO_SOCKS[128];
#endif

extern int R_do_dns, R_fin_dns, R_fin_dnsc, R_fail_dns,
    R_do_id, R_fin_id, R_fail_id;
#ifdef SOCKSPORT
extern int R_do_socks, R_good_socks, R_no_socks;
#endif

#endif
extern aChannel *find_channel PROTO((char *, aChannel *));
extern void remove_user_from_channel PROTO((aClient *, aChannel *));
extern char *base64enc PROTO((long));
extern long base64dec PROTO((char *));
extern void add_server_to_table PROTO((aClient *));
extern void remove_server_from_tabel PROTO((aClient *));

/* for services */
extern void del_invite PROTO((aClient *, aChannel *));
extern int del_silence PROTO((aClient *, char *));
extern void send_user_joins PROTO((aClient *, aClient *));
extern void clean_channelname PROTO((char *));
extern int do_nick_name PROTO((char *));
extern int can_send PROTO((aClient *, aChannel *, char *));
extern int is_chan_op PROTO((aClient *, aChannel *));
extern int has_voice PROTO((aClient *, aChannel *));
extern int is_chanowner PROTO((aClient *, aChannel *));
extern Ban *is_banned PROTO((aClient *, aClient *, aChannel *));
extern int parse_help PROTO((aClient *, char *, char *));

extern void ircd_log PROTO((char *, ...));
extern aClient *find_client PROTO((char *, aClient *));
extern aClient *find_name PROTO((char *, aClient *));
extern aClient *find_nickserv PROTO((char *, aClient *));
extern aClient *find_person PROTO((char *, aClient *));
extern aClient *find_server PROTO((char *, aClient *));
extern aClient *find_server_quickx PROTO((char *, aClient *));
extern aClient *find_service PROTO((char *, aClient *));
#define find_server_quick(x) find_server_quickx(x, NULL)
extern char *find_or_add PROTO((char *));
extern int attach_conf PROTO((aClient *, aConfItem *));
extern aConfItem *attach_confs PROTO((aClient *, char *, int));
extern aConfItem *attach_confs_host PROTO((aClient *, char *, int));
extern int attach_Iline PROTO((aClient *, struct hostent *, char *));
extern aConfItem *conf, *find_me PROTO(()), *find_admin PROTO(());
extern aConfItem *count_cnlines PROTO((Link *));
extern aSqlineItem *sqline;
extern void det_confs_butmask PROTO((aClient *, int));
extern int detach_conf PROTO((aClient *, aConfItem *));
extern aSqlineItem *find_sqline_nick PROTO((char *));
extern aSqlineItem *find_sqline_match PROTO((char *));
extern aConfItem *det_confs_butone PROTO((aClient *, aConfItem *));
extern char *find_diepass();
extern char *find_restartpass();
extern aConfItem *find_conf PROTO((Link *, char *, int));
extern aConfItem *find_conf_exact PROTO((char *, char *, char *, int));
extern aConfItem *find_conf_host PROTO((Link *, char *, int));
extern aConfItem *find_conf_ip PROTO((Link *, char *, char *, int));
extern aConfItem *find_conf_name PROTO((char *, int));
extern aConfItem *find_temp_conf_entry PROTO((aConfItem *, u_int));
extern aConfItem *find_conf_servern PROTO((char *));
extern int find_kill PROTO((aClient *));
extern char *find_zap PROTO((aClient *, int));
extern int find_restrict PROTO((aClient *));
extern int rehash PROTO((aClient *, aClient *, int));
extern int initconf PROTO((int));
extern void add_temp_conf();
extern void inittoken PROTO(());
extern void reset_help PROTO(());
extern int find_exception(char *);	/* hidden host */

#ifndef DMALLOC
extern char *MyMalloc PROTO((int)), *MyRealloc PROTO((char *, int));
#endif
extern char *debugmode, *configfile, *sbrk0;
extern char *getfield PROTO((char *));
extern void get_sockhost PROTO((aClient *, char *));
extern char *strerror PROTO((int));
extern int dgets PROTO((int, char *, int));
extern char *inetntoa PROTO((char *));

#ifdef _WIN32
extern int dbufalloc, dbufblocks, debuglevel;
#else
extern int dbufalloc, dbufblocks, debuglevel, errno, h_errno;
#endif
extern int highest_fd, debuglevel, portnum, debugtty, maxusersperchannel;
extern int readcalls, udpfd, resfd;
extern aClient *add_connection PROTO((aClient *, int));
extern int add_listener PROTO((aConfItem *));
extern void add_local_domain PROTO((char *, int));
extern int check_client PROTO((aClient *));
extern int check_server PROTO((aClient *, struct hostent *, aConfItem *,
    aConfItem *, int));
extern int check_server_init PROTO((aClient *));
extern void close_connection PROTO((aClient *));
extern void close_listeners PROTO(());
extern int connect_server PROTO((aConfItem *, aClient *, struct hostent *));
extern void get_my_name PROTO((aClient *, char *, int));
extern int get_sockerr PROTO((aClient *));
extern int inetport PROTO((aClient *, char *, int));
extern void init_sys PROTO(());

#ifdef NO_FDLIST
extern int read_message PROTO((TS));
#else
extern int read_message PROTO((TS, fdlist *));
#endif

extern void report_error PROTO((char *, aClient *));
extern void set_non_blocking PROTO((int, aClient *));
extern int setup_ping PROTO(());

extern void start_auth PROTO((aClient *));
extern void read_authports PROTO((aClient *));
extern void send_authports PROTO((aClient *));

#ifdef SOCKSPORT
extern void init_socks PROTO((aClient *));
extern void start_socks PROTO((aClient *));
extern void send_socksquery PROTO((aClient *));
extern void read_socks PROTO((aClient *));
#endif

extern void restart PROTO((char *));
extern void send_channel_modes PROTO((aClient *, aChannel *));
extern void server_reboot PROTO((char *));
extern void terminate PROTO(()), write_pidfile PROTO(());

extern int send_queued PROTO((aClient *));
/*VARARGS2*/
// extern       void    sendto_one(char *, ...);
/*VARARGS4*/
/* i know this is naughty but :P --stskeeps */
extern void sendto_channel_butone(aClient *, aClient *, aChannel *, char *,
    ...);
extern void sendto_channelops_butone(aClient *, aClient *, aChannel *,
    char *, ...);
extern void sendto_channelvoice_butone(aClient *, aClient *, aChannel *,
    char *, ...);
/*VARARGS2*/
extern void sendto_serv_butone(aClient *, char *, ...);
/*VARARGS2*/
extern void sendto_serv_butone_quit(aClient *, char *, ...);
extern void sendto_serv_butone_sjoin(aClient *, char *, ...);
extern void sendto_serv_sjoin(aClient *, char *, ...);
/*VARARGS2*/

extern void sendto_common_channels(aClient *, char *, ...);
/*VARARGS3*/
extern void sendto_channel_butserv(aChannel *, aClient *, char *, ...);
/*VARARGS3*/
extern void sendto_match_servs(aChannel *, aClient *, char *, ...);
/*VARARGS5*/
extern void sendto_match_butone(aClient *, aClient *, char *, int,
    char *pattern, ...);
/*VARARGS3*/
extern void sendto_all_butone(aClient *, aClient *, char *, ...);
/*VARARGS1*/
extern void sendto_ops(char *, ...);
/*VARARGS3*/
extern void sendto_ops_butone(aClient *, aClient *, char *, ...);
/*VARARGS3*/
extern void sendto_ops_butme(aClient *, char *, ...);
/*VARARGS3*/
extern void sendto_prefix_one(aClient *, aClient *, const char *, ...);
/*VARARGS3*/
extern void sendto_failops_whoare_opers(char *, ...);
/*VARARGS3*/
extern void sendto_failops(char *, ...);
/*VARARGS3*/
extern void sendto_opers(char *, ...);
/*VARARGS?*/
extern void sendto_umode(int, char *, ...);
extern void sendto_conn_hcn(char *, ...);
extern int writecalls, writeb[];
extern int deliver_it PROTO((aClient *, char *, int));

extern int check_registered PROTO((aClient *));
extern int check_registered_user PROTO((aClient *));
extern char *get_client_name PROTO((aClient *, int));
extern char *get_client_host PROTO((aClient *));
extern char *my_name_for_link PROTO((char *, aConfItem *));
extern char *myctime PROTO((TS)), *date PROTO((TS));
extern int exit_client PROTO((aClient *, aClient *, aClient *, char *));
extern void initstats PROTO(()), tstats PROTO((aClient *, char *));
extern char *check_string PROTO((char *));
extern char *make_nick_user_host PROTO((char *, char *, char *));

extern int parse PROTO((aClient *, char *, char *, struct Message *));
extern int do_numeric PROTO((int, aClient *, aClient *, int, char **));
extern int hunt_server PROTO((aClient *, aClient *, char *, int, int, char **));
extern aClient *next_client PROTO((aClient *, char *));
extern int m_umode PROTO((aClient *, aClient *, int, char **));
extern int m_names PROTO((aClient *, aClient *, int, char **));
extern int m_server_estab PROTO((aClient *));
extern void send_umode PROTO((aClient *, aClient *, long, long, char *));
extern void send_umode_out PROTO((aClient *, aClient *, long));

extern void free_client PROTO((aClient *));
extern void free_link PROTO((Link *));
extern void free_ban PROTO((Ban *));
extern void free_conf PROTO((aConfItem *));
extern void free_class PROTO((aClass *));
extern void free_user PROTO((anUser *, aClient *));
extern int find_str_match_link PROTO((Link *, char *));
extern void free_str_list PROTO((Link *));
extern Link *make_link PROTO(());
extern Ban *make_ban PROTO(());
extern anUser *make_user PROTO((aClient *));
extern aSqlineItem *make_sqline PROTO(());
extern aConfItem *make_conf PROTO(());
extern aClass *make_class PROTO(());
extern aServer *make_server PROTO(());
extern aClient *make_client PROTO((aClient *, aClient *));
extern Link *find_user_link PROTO((Link *, aClient *));
extern Link *find_channel_link PROTO((Link *, aChannel *));
extern char *pretty_mask PROTO((char *));
extern void add_client_to_list PROTO((aClient *));
extern void checklist PROTO(());
extern void remove_client_from_list PROTO((aClient *));
extern void initlists PROTO(());

extern void add_class PROTO((int, int, int, int, long));
extern void fix_class PROTO((aConfItem *, aConfItem *));
extern long get_sendq PROTO((aClient *));
extern int get_con_freq PROTO((aClass *));
extern int get_client_ping PROTO((aClient *));
extern int get_client_class PROTO((aClient *));
extern int get_conf_class PROTO((aConfItem *));
extern void report_classes PROTO((aClient *));

extern struct hostent *get_res PROTO((char *));
extern struct hostent *gethost_byaddr PROTO((char *, Link *));
extern struct hostent *gethost_byname PROTO((char *, Link *));
extern void flush_cache PROTO(());
extern int init_resolver PROTO((int));
extern TS timeout_query_list PROTO((TS));
extern TS expire_cache PROTO((TS));
extern void del_queries PROTO((char *));

extern void clear_channel_hash_table PROTO(());
extern void clear_client_hash_table PROTO(());
extern void clear_notify_hash_table PROTO(());
extern int add_to_client_hash_table PROTO((char *, aClient *));
extern int del_from_client_hash_table PROTO((char *, aClient *));
extern int add_to_channel_hash_table PROTO((char *, aChannel *));
extern int del_from_channel_hash_table PROTO((char *, aChannel *));
extern int add_to_notify_hash_table PROTO((char *, aClient *));
extern int del_from_notify_hash_table PROTO((char *, aClient *));
extern int hash_check_notify PROTO((aClient *, int));
extern int hash_del_notify_list PROTO((aClient *));
extern void count_watch_memory PROTO((int *, u_long *));
extern aNotify *hash_get_notify PROTO((char *));
extern aChannel *hash_get_chan_bucket PROTO((int));
extern aClient *hash_find_client PROTO((char *, aClient *));
extern aClient *hash_find_nickserver PROTO((char *, aClient *));
extern aClient *hash_find_server PROTO((char *, aClient *));
extern char *find_by_aln PROTO((char *));
extern char *convert2aln PROTO((int));
extern int convertfromaln PROTO((char *));
extern char *find_server_aln PROTO((char *));
extern atime(char *xtime);


extern int dopacket PROTO((aClient *, char *, int));

/*VARARGS2*/
extern void debug();
#if defined(DEBUGMODE)
extern void send_usage PROTO((aClient *, char *));
extern void send_listinfo PROTO((aClient *, char *));
extern void count_memory PROTO((aClient *, char *));
#endif

#ifdef INET6
extern char *inetntop(int af, const void *in, char *local_dummy,
    size_t the_size);
#endif

char *crule_parse PROTO((char *));
int crule_eval PROTO((char *));
void crule_free PROTO((char **));

#define HASH_CHECK BASE_VERSION
