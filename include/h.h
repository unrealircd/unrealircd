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

extern Member *freemember;
extern Membership *freemembership;
extern MembershipL *freemembershipL;
extern TS nextconnect, nextdnscheck, nextping;
extern aClient *client, me, *local[];
extern aChannel *channel;
extern struct stats *ircstp;
extern int bootopt;
extern time_t TSoffset;
/* Prototype added to force errors -- Barubary */
extern TS check_pings(TS now, int check_kills);
extern TS TS2ts(char *s);
extern time_t timeofday;
/* newconf */
#define get_sendq(x) ((x)->class ? (x)->class->sendq : MAXSENDQLENGTH) 


#ifndef NO_FDLIST
extern float currentrate;
extern float currentrate2;		/* outgoing */
extern float highest_rate;
extern float highest_rate2;
extern int  lifesux;
extern int  LRV;
extern time_t   LCF;
extern int  currlife;
extern int  HTMLOCK;
extern int  noisy_htm;
extern long lastsendK, lastrecvK;
#endif

/*
 * Configuration linked lists
*/
extern ConfigItem_me		*conf_me;
extern ConfigItem_class 	*conf_class;
extern ConfigItem_admin 	*conf_admin;
extern ConfigItem_admin		*conf_admin_tail;
extern ConfigItem_drpass	*conf_drpass;
extern ConfigItem_ulines	*conf_ulines;
extern ConfigItem_tld		*conf_tld;
extern ConfigItem_oper		*conf_oper;
extern ConfigItem_listen	*conf_listen;
extern ConfigItem_allow		*conf_allow;
extern ConfigItem_except	*conf_except;
extern ConfigItem_vhost		*conf_vhost;
extern ConfigItem_link		*conf_link;
extern ConfigItem_ban		*conf_ban;
extern ConfigItem_badword	*conf_badword_channel;
extern ConfigItem_badword       *conf_badword_message;
extern ConfigItem_deny_dcc	*conf_deny_dcc;
extern ConfigItem_deny_channel  *conf_deny_channel;
extern ConfigItem_deny_link	*conf_deny_link;
extern ConfigItem_allow_channel *conf_allow_channel;
extern ConfigItem_deny_version	*conf_deny_version;
extern ConfigItem_log		*conf_log;

EVENT(tkl_check_expire);

ConfigItem_class	*Find_class(char *name);
ConfigItem_deny_dcc	*Find_deny_dcc(char *name);
ConfigItem_oper		*Find_oper(char *name);
ConfigItem_listen	*Find_listen(char *ipmask, int port);
ConfigItem_ulines	*Find_uline(char *host);
ConfigItem_except	*Find_except(char *host, short type);
ConfigItem_tld		*Find_tld(char *host);
ConfigItem_link		*Find_link(char *username, char *hostname, char *ip, char *servername);
ConfigItem_ban 		*Find_ban(char *host, short type);
ConfigItem_ban 		*Find_banEx(char *host, short type, short type2);
ConfigItem_vhost	*Find_vhost(char *name);
ConfigItem_deny_channel *Find_channel_allowed(char *name);
int			AllowClient(aClient *cptr, struct hostent *hp, char *sockhost);


aMotd *read_motd(char *filename);
aMotd *read_rules(char *filename);
extern struct tm *motd_tm;
extern Link	*Servers;

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
extern inline aCommand *find_Command(char *cmd, short token, int flags);
extern aChannel *find_channel(char *, aChannel *);
extern Member *find_member_link(Member *, aClient *);
extern void remove_user_from_channel(aClient *, aChannel *);
extern char *base64enc(long);
extern long base64dec(char *);
extern void add_server_to_table(aClient *);
extern void remove_server_from_tabel(aClient *);

/* for services */
extern void del_invite(aClient *, aChannel *);
extern int del_silence(aClient *, char *);
extern void send_user_joins(aClient *, aClient *);
extern void clean_channelname(char *);
extern int do_nick_name(char *);
extern int can_send(aClient *, aChannel *, char *);
extern int is_chan_op(aClient *, aChannel *);
extern int has_voice(aClient *, aChannel *);
extern int is_chanowner(aClient *, aChannel *);
extern Ban *is_banned(aClient *, aClient *, aChannel *);
extern int parse_help(aClient *, char *, char *);

extern void ircd_log(int, char *, ...);
extern aClient *find_client(char *, aClient *);
extern aClient *find_name(char *, aClient *);
extern aClient *find_nickserv(char *, aClient *);
extern aClient *find_person(char *, aClient *);
extern aClient *find_server(char *, aClient *);
extern aClient *find_server_quickx(char *, aClient *);
extern aClient *find_service(char *, aClient *);
#define find_server_quick(x) find_server_quickx(x, NULL)
extern char *find_or_add(char *);
extern int attach_conf(aClient *, aConfItem *);
extern void inittoken();
extern void reset_help();

extern char *debugmode, *configfile, *sbrk0;
extern char *getfield(char *);
extern void get_sockhost(aClient *, char *);
extern char *strerror(int);
extern int dgets(int, char *, int);
extern char *inetntoa(char *);

#ifdef _WIN32
extern int dbufalloc, dbufblocks, debuglevel;
#else
extern int dbufalloc, dbufblocks, debuglevel, errno, h_errno;
#endif
extern short LastSlot; /* last used index in local client array */
extern int OpenFiles;  /* number of files currently open */
extern int debuglevel, portnum, debugtty, maxusersperchannel;
extern int readcalls, udpfd, resfd;
extern aClient *add_connection(aClient *, int);
extern int add_listener(aConfItem *);
extern void add_local_domain(char *, int);
extern int check_client(aClient *);
extern int check_server(aClient *, struct hostent *, aConfItem *,
    aConfItem *, int);
extern int check_server_init(aClient *);
extern void close_connection(aClient *);
extern void close_listeners();
extern int connect_server(ConfigItem_link *, aClient *, struct hostent *);
extern void get_my_name(aClient *, char *, int);
extern int get_sockerr(aClient *);
extern int inetport(aClient *, char *, int);
extern void init_sys();

#ifdef NO_FDLIST
extern int read_message(time_t);
#else
extern int read_message(time_t, fdlist *);
#endif

extern void report_error(char *, aClient *);
extern void set_non_blocking(int, aClient *);
extern int setup_ping();

extern void start_auth(aClient *);
extern void read_authports(aClient *);
extern void send_authports(aClient *);

#ifdef SOCKSPORT
extern void init_socks(aClient *);
extern void start_socks(aClient *);
extern void send_socksquery(aClient *);
extern void read_socks(aClient *);
#endif

extern void restart(char *);
extern void send_channel_modes(aClient *, aChannel *);
extern void server_reboot(char *);
extern void terminate(), write_pidfile();

extern int send_queued(aClient *);
/* i know this is naughty but :P --stskeeps */
extern void sendto_channel_butone(aClient *, aClient *, aChannel *, char *,
    ...);
extern void sendto_channelops_butone(aClient *, aClient *, aChannel *,
    char *, ...);
extern void sendto_channelvoice_butone(aClient *, aClient *, aChannel *,
    char *, ...);
extern void sendto_serv_butone(aClient *, char *, ...);
extern void sendto_serv_butone_quit(aClient *, char *, ...);
extern void sendto_serv_butone_sjoin(aClient *, char *, ...);
extern void sendto_serv_sjoin(aClient *, char *, ...);
extern void sendto_common_channels(aClient *, char *, ...);
extern void sendto_channel_butserv(aChannel *, aClient *, char *, ...);
extern void sendto_match_servs(aChannel *, aClient *, char *, ...);
extern void sendto_match_butone(aClient *, aClient *, char *, int,
    char *pattern, ...);
extern void sendto_all_butone(aClient *, aClient *, char *, ...);
extern void sendto_ops(char *, ...);
extern void sendto_ops_butone(aClient *, aClient *, char *, ...);
extern void sendto_ops_butme(aClient *, char *, ...);
extern void sendto_prefix_one(aClient *, aClient *, const char *, ...);
extern void sendto_failops_whoare_opers(char *, ...);
extern void sendto_failops(char *, ...);
extern void sendto_opers(char *, ...);
extern void sendto_umode(int, char *, ...);
extern void sendto_conn_hcn(char *, ...);
extern int writecalls, writeb[];
extern int deliver_it(aClient *, char *, int);

extern int check_registered(aClient *);
extern int check_registered_user(aClient *);
extern char *get_client_name(aClient *, int);
extern char *get_client_host(aClient *);
extern char *my_name_for_link(char *, aConfItem *);
extern char *myctime(time_t), *date(time_t);
extern int exit_client(aClient *, aClient *, aClient *, char *);
extern void initstats(), tstats(aClient *, char *);
extern char *check_string(char *);
extern char *make_nick_user_host(char *, char *, char *);
extern char *make_user_host(char *, char *);
extern int parse(aClient *, char *, char *);
extern int do_numeric(int, aClient *, aClient *, int, char **);
extern int hunt_server(aClient *, aClient *, char *, int, int, char **);
extern aClient *next_client(aClient *, char *);
extern int m_umode(aClient *, aClient *, int, char **);
extern int m_names(aClient *, aClient *, int, char **);
extern int m_server_estab(aClient *);
extern void send_umode(aClient *, aClient *, long, long, char *);
extern void send_umode_out(aClient *, aClient *, long);

extern void free_client(aClient *);
extern void free_link(Link *);
extern void free_ban(Ban *);
extern void free_class(aClass *);
extern void free_user(anUser *, aClient *);
extern int find_str_match_link(Link *, char *);
extern void free_str_list(Link *);
extern Link *make_link();
extern Ban *make_ban();
extern anUser *make_user(aClient *);
extern aClass *make_class();
extern aServer *make_server();
extern aClient *make_client(aClient *, aClient *);
extern Link *find_user_link(Link *, aClient *);
extern Member *find_channel_link(Member *, aChannel *);
extern char *pretty_mask(char *);
extern void add_client_to_list(aClient *);
extern void checklist();
extern void remove_client_from_list(aClient *);
extern void initlists();
extern struct hostent *get_res(char *);
extern struct hostent *gethost_byaddr(char *, Link *);
extern struct hostent *gethost_byname(char *, Link *);
extern void flush_cache();
extern int init_resolver(int);
extern time_t timeout_query_list(time_t);
extern time_t expire_cache(time_t);
extern void del_queries(char *);

extern void clear_channel_hash_table();
extern void clear_client_hash_table();
extern void clear_notify_hash_table();
extern int add_to_client_hash_table(char *, aClient *);
extern int del_from_client_hash_table(char *, aClient *);
extern int add_to_channel_hash_table(char *, aChannel *);
extern int del_from_channel_hash_table(char *, aChannel *);
extern int add_to_notify_hash_table(char *, aClient *);
extern int del_from_notify_hash_table(char *, aClient *);
extern int hash_check_notify(aClient *, int);
extern int hash_del_notify_list(aClient *);
extern void count_watch_memory(int *, u_long *);
extern aNotify *hash_get_notify(char *);
extern aChannel *hash_get_chan_bucket(int);
extern aClient *hash_find_client(char *, aClient *);
extern aClient *hash_find_nickserver(char *, aClient *);
extern aClient *hash_find_server(char *, aClient *);
extern char *find_by_aln(char *);
extern char *convert2aln(int);
extern int convertfromaln(char *);
extern char *find_server_aln(char *);
extern atime(char *xtime);


extern int dopacket(aClient *, char *, int);

extern void debug(int, char *, ...);
#if defined(DEBUGMODE)
extern void send_usage(aClient *, char *);
extern void send_listinfo(aClient *, char *);
extern void count_memory(aClient *, char *);
#endif

#ifdef INET6
extern char *inetntop(int af, const void *in, char *local_dummy,
    size_t the_size);
#endif

/*
 * CommandHash -Stskeeps
*/
extern aCommand *CommandHash[256];
void	init_CommandHash(void);
void	add_Command_backend(char *cmd, int (*func)(), unsigned char parameters, unsigned char token, int flags);
void	add_Command(char *cmd, char *token, int (*func)(), unsigned char parameters);
void	add_Command_to_list(aCommand *item, aCommand **list);
aCommand *del_Command_from_list(aCommand *item, aCommand **list);
int	del_Command(char *cmd, char *token, int (*func)());

/* CRULE */
char *crule_parse(char *);
int crule_eval(char *);
void crule_free(char **);

/* Add clients to LocalClients array */
extern void add_local_client(aClient* cptr);
/* Remove clients from LocalClients array */
extern void remove_local_client(aClient* cptr);
/*
 * Close all local socket connections, invalidate client fd's
 * WIN32 cleanup winsock lib
 */
extern void close_connections(void);
extern void flush_connections(aClient *cptr);

#define HASH_CHECK BASE_VERSION
