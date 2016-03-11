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
#include "setup.h"
#include "fdlist.h"

extern MODVAR char *extraflags;
extern MODVAR int tainted;
/* for the new s_err.c */
extern char *getreply(int);
#define rpl_str(x) getreply(x)
#define err_str(x) getreply(x)
extern MODVAR Member *freemember;
extern MODVAR Membership *freemembership;
extern MODVAR MembershipL *freemembershipL;
extern MODVAR aClient me;
extern MODVAR aChannel *channel;
extern MODVAR struct stats *ircstp;
extern MODVAR int bootopt;
extern MODVAR time_t TSoffset;
extern MODVAR time_t timeofday;
/* newconf */
#define get_sendq(x) ((x)->local->class ? (x)->local->class->sendq : MAXSENDQLENGTH)
/* get_recvq is only called in send.c for local connections */
#define get_recvq(x) ((x)->local->class->recvq ? (x)->local->class->recvq : CLIENT_FLOOD)

#define CMD_FUNC(x) int __attribute__((warn_unused_result)) (x) (aClient *cptr, aClient *sptr, int parc, char *parv[])

/*
 * Configuration linked lists
*/
extern MODVAR ConfigItem_me		*conf_me;
extern MODVAR ConfigItem_files		*conf_files;
extern MODVAR ConfigItem_class 	*conf_class;
extern MODVAR ConfigItem_class		*default_class;
extern MODVAR ConfigItem_admin 	*conf_admin;
extern MODVAR ConfigItem_admin		*conf_admin_tail;
extern MODVAR ConfigItem_drpass	*conf_drpass;
extern MODVAR ConfigItem_ulines	*conf_ulines;
extern MODVAR ConfigItem_tld		*conf_tld;
extern MODVAR ConfigItem_oper		*conf_oper;
extern MODVAR ConfigItem_listen	*conf_listen;
extern MODVAR ConfigItem_allow		*conf_allow;
extern MODVAR ConfigItem_except	*conf_except;
extern MODVAR ConfigItem_vhost		*conf_vhost;
extern MODVAR ConfigItem_link		*conf_link;
extern MODVAR ConfigItem_ban		*conf_ban;
extern MODVAR ConfigItem_deny_dcc	*conf_deny_dcc;
extern MODVAR ConfigItem_deny_channel  *conf_deny_channel;
extern MODVAR ConfigItem_deny_link	*conf_deny_link;
extern MODVAR ConfigItem_allow_channel *conf_allow_channel;
extern MODVAR ConfigItem_allow_dcc *conf_allow_dcc;
extern MODVAR ConfigItem_deny_version	*conf_deny_version;
extern MODVAR ConfigItem_log		*conf_log;
extern MODVAR ConfigItem_alias		*conf_alias;
extern MODVAR ConfigItem_include	*conf_include;
extern MODVAR ConfigItem_help		*conf_help;
extern MODVAR ConfigItem_offchans	*conf_offchans;
extern void		completed_connection(int, int, void *);
extern void clear_unknown();
extern EVENT(e_unload_module_delayed);
extern EVENT(e_clean_out_throttling_buckets);

extern void  module_loadall(void);
extern long set_usermode(char *umode);
extern char *get_modestr(long umodes);
extern void                    config_error(char *format, ...) __attribute__((format(printf,1,2)));
extern void config_warn(char *format, ...) __attribute__((format(printf,1,2)));
extern void config_error_missing(const char *filename, int line, const char *entry);
extern void config_error_unknown(const char *filename, int line, const char *block, const char *entry);
extern void config_error_unknownflag(const char *filename, int line, const char *block, const char *entry);
extern void config_error_unknownopt(const char *filename, int line, const char *block, const char *entry);
extern void config_error_noname(const char *filename, int line, const char *block);
extern void config_error_blank(const char *filename, int line, const char *block);
extern void config_error_empty(const char *filename, int line, const char *block, const char *entry);
extern void config_warn_duplicate(const char *filename, int line, const char *entry);
extern int config_is_blankorempty(ConfigEntry *cep, const char *block);
extern MODVAR int config_verbose;
extern void config_progress(char *format, ...) __attribute__((format(printf,1,2)));
extern void       ipport_seperate(char *string, char **ip, char **port);
extern ConfigItem_class	*Find_class(char *name);
extern ConfigItem_deny_dcc	*Find_deny_dcc(char *name);
extern ConfigItem_oper		*Find_oper(char *name);
extern ConfigItem_operclass	*Find_operclass(char *name);
extern ConfigItem_listen *Find_listen(char *ipmask, int port, int ipv6);
extern ConfigItem_ulines	*Find_uline(char *host);
extern ConfigItem_except	*Find_except(aClient *, short type);
extern ConfigItem_tld		*Find_tld(aClient *cptr);
extern ConfigItem_link		*Find_link(char *servername, aClient *acptr);
extern ConfigItem_ban 		*Find_ban(aClient *, char *host, short type);
extern ConfigItem_ban 		*Find_banEx(aClient *,char *host, short type, short type2);
extern ConfigItem_vhost	*Find_vhost(char *name);
extern ConfigItem_deny_channel *Find_channel_allowed(aClient *cptr, char *name);
extern ConfigItem_alias	*Find_alias(char *name);
extern ConfigItem_help 	*Find_Help(char *command);

extern OperPermission ValidatePermissionsForPath(char *path, aClient *sptr, aClient *victim, aChannel *channel, void *extra);
extern void OperClassValidatorDel(OperClassValidator *validator);

extern int AllowClient(aClient *cptr, struct hostent *hp, char *sockhost, char *username);
extern int match_user(char *rmask, aClient *acptr, int options);
extern ConfigItem_ban  *Find_ban_ip(aClient *sptr);
extern void add_ListItem(ListStruct *, ListStruct **);
extern void add_ListItemPrio(ListStructPrio *, ListStructPrio **, int);
extern ListStruct *del_ListItem(ListStruct *, ListStruct **);
extern aClient *find_match_server(char *mask);
extern MODVAR LoopStruct loop;
extern int del_banid(aChannel *chptr, char *banid);
extern int del_exbanid(aChannel *chptr, char *banid);
#ifdef SHOWCONNECTINFO


#define BREPORT_DO_DNS	"NOTICE * :*** Looking up your hostname...\r\n"
#define BREPORT_FIN_DNS	"NOTICE * :*** Found your hostname\r\n"
#define BREPORT_FIN_DNSC "NOTICE * :*** Found your hostname (cached)\r\n"
#define BREPORT_FAIL_DNS "NOTICE * :*** Couldn't resolve your hostname; using your IP address instead\r\n"
#define BREPORT_DO_ID	"NOTICE * :*** Checking ident...\r\n"
#define BREPORT_FIN_ID	"NOTICE * :*** Received identd response\r\n"
#define BREPORT_FAIL_ID	"NOTICE * :*** No ident response; username prefixed with ~\r\n"

extern MODVAR char REPORT_DO_DNS[256], REPORT_FIN_DNS[256], REPORT_FIN_DNSC[256],
    REPORT_FAIL_DNS[256], REPORT_DO_ID[256], REPORT_FIN_ID[256],
    REPORT_FAIL_ID[256];

extern MODVAR int R_do_dns, R_fin_dns, R_fin_dnsc, R_fail_dns,
    R_do_id, R_fin_id, R_fail_id;

#endif
extern MODVAR struct list_head client_list, lclient_list, server_list, oper_list, unknown_list, global_server_list;
extern aCommand *find_Command(char *cmd, short token, int flags);
extern aCommand *find_Command_simple(char *cmd);
extern aChannel *find_channel(char *, aChannel *);
extern Membership *find_membership_link(Membership *lp, aChannel *ptr);
extern Member *find_member_link(Member *, aClient *);
extern void remove_user_from_channel(aClient *, aChannel *);
extern void add_server_to_table(aClient *);
extern void remove_server_from_table(aClient *);
extern void iNAH_host(aClient *sptr, char *host);
extern void set_snomask(aClient *sptr, char *snomask);
extern char *get_sno_str(aClient *sptr);
extern int check_tkls(aClient *cptr);
/* for services */
extern void del_invite(aClient *, aChannel *);
extern int add_silence(aClient *, char *, int);
extern int del_silence(aClient *, char *);
extern void send_user_joins(aClient *, aClient *);
extern void clean_channelname(char *);
extern int do_nick_name(char *);
extern int do_remote_nick_name(char *);
extern int can_send(aClient *, aChannel *, char *, int);
extern long get_access(aClient *, aChannel *);
extern int is_chan_op(aClient *, aChannel *);
extern int has_voice(aClient *, aChannel *);
extern int is_chanowner(aClient *, aChannel *);
extern int ban_check_mask(aClient *, aChannel *, char *, int, int);
extern int extban_is_ok_nuh_extban(aClient *, aChannel *, char *, int, int, int);
extern char* extban_conv_param_nuh_or_extban(char *);
extern Ban *is_banned(aClient *, aChannel *, int);
extern Ban *is_banned_with_nick(aClient *, aChannel *, int, char *);
extern int parse_help(aClient *, char *, char *);

extern void ircd_log(int, char *, ...) __attribute__((format(printf,2,3)));
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

extern MODVAR char *debugmode, *configfile, *sbrk0;
extern char *getfield(char *);
extern void set_sockhost(aClient *, char *);
#ifndef _WIN32
extern char *strerror(int);
#else
extern MODFUNC char *sock_strerror(int);
#endif
extern int dgets(int, char *, int);
extern char *inetntoa(char *);

#ifndef HAVE_SNPRINTF
extern int snprintf (char *str, size_t count, const char *fmt, ...);
#endif
#ifndef HAVE_VSNPRINTF
extern int vsnprintf (char *str, size_t count, const char *fmt, va_list arg);
#endif

#ifdef _WIN32
extern MODVAR int debuglevel;
#else
extern int debuglevel, errno, h_errno;
#endif
extern MODVAR int OpenFiles;  /* number of files currently open */
extern MODVAR int debuglevel, portnum, debugtty, maxusersperchannel;
extern MODVAR int readcalls, udpfd, resfd;
extern aClient *add_connection(ConfigItem_listen *, int);
extern int add_listener(aConfItem *);
extern void add_local_domain(char *, int);
extern int check_client(aClient *, char *);
extern int check_server(aClient *, struct hostent *, aConfItem *,
    aConfItem *, int);
extern int check_server_init(aClient *);
extern void close_connection(aClient *);
extern void close_listeners();
extern int connect_server(ConfigItem_link *, aClient *, struct hostent *);
extern void get_my_name(aClient *, char *, int);
extern int get_sockerr(aClient *);
extern int inetport(ConfigItem_listen *, char *, int, int);
extern void init_sys();
extern void init_modef();
extern int verify_hostname(char *name);

extern void report_error(char *, aClient *);
extern void set_non_blocking(int, aClient *);
extern int setup_ping();

extern void start_auth(aClient *);

extern void set_channel_mlock(aClient *, aClient *, aChannel *, const char *, int);

extern void restart(char *);
extern void server_reboot(char *);
extern void terminate(), write_pidfile();
extern void *MyMallocEx(size_t size);
extern int advanced_check(char *userhost, int ipstat);
extern int send_queued(aClient *);
extern void sendto_connectnotice(aClient *sptr, int disconnect, char *comment);
extern void sendto_serv_butone_nickcmd(aClient *one, aClient *sptr, char *umodes);
extern void    sendto_message_one(aClient *to, aClient *from, char *sender,
    char *cmd, char *nick, char *msg);
#define PREFIX_ALL		0
#define PREFIX_HALFOP	0x1
#define PREFIX_VOICE	0x2
#define PREFIX_OP	0x4
#define PREFIX_ADMIN	0x08
#define PREFIX_OWNER	0x10
extern void sendto_channelprefix_butone(aClient *one, aClient *from, aChannel *chptr,
    int prefix, char *pattern, ...) __attribute__((format(printf,5,6)));
extern void sendto_channel_butone(aClient *, aClient *, aChannel *,
                                  char *, ...) __attribute__((format(printf,4,5)));
void sendto_channel_butone_with_capability(aClient *one, unsigned int cap,
        aClient *from, aChannel *chptr, char *pattern, ...) __attribute__((format(printf,5,6)));
extern void sendto_channel_butserv_butone(aChannel *chptr, aClient *from, aClient *one,
                                          char *pattern, ...) __attribute__((format(printf,4,5)));
extern void sendto_common_channels(aClient *, char *, ...) __attribute__((format(printf,2,3)));
extern void sendto_common_channels_local_butone(aClient *, int, char *, ...) __attribute__((format(printf,3,4)));
extern void sendto_channel_butserv(aChannel *, aClient *, char *, ...) __attribute__((format(printf,3,4)));
extern void sendto_match_servs(aChannel *, aClient *, char *, ...) __attribute__((format(printf,3,4)));
extern void sendto_match_butone(aClient *, aClient *, char *, int,
    char *pattern, ...) __attribute__((format(printf,5,6)));
extern void sendto_all_butone(aClient *, aClient *, char *, ...) __attribute__((format(printf,3,4)));
extern void sendto_ops(char *, ...) __attribute__((format(printf,1,2)));
extern void sendto_ops_butone(aClient *, aClient *, char *, ...) __attribute__((format(printf,3,4)));
extern void sendto_ops_butme(aClient *, char *, ...) __attribute__((format(printf,2,3)));
extern void sendto_prefix_one(aClient *, aClient *, const char *, ...) __attribute__((format(printf,3,4)));
extern void sendto_opers(char *, ...) __attribute__((format(printf,1,2)));
extern void sendto_umode(int, char *, ...) __attribute__((format(printf,2,3)));
extern void sendto_umode_global(int, char *, ...) __attribute__((format(printf,2,3)));
extern void sendto_umode_raw(int, char *, ...) __attribute__((format(printf,2,3)));
extern void sendto_snomask(int snomask, char *pattern, ...) __attribute__((format(printf,2,3)));
extern void sendto_snomask_global(int snomask, char *pattern, ...) __attribute__((format(printf,2,3)));
extern void sendto_snomask_normal(int snomask, char *pattern, ...) __attribute__((format(printf,2,3)));
extern void sendto_snomask_normal_global(int snomask, char *pattern, ...) __attribute__((format(printf,2,3)));
extern void sendnotice(aClient *to, char *pattern, ...) __attribute__((format(printf,2,3)));
extern void sendto_server(aClient *one, unsigned long caps, unsigned long nocaps, const char *format, ...) __attribute__((format(printf, 4,5)));

extern MODVAR int writecalls, writeb[];
extern int deliver_it(aClient *, char *, int);
extern int  check_for_target_limit(aClient *sptr, void *target, const char *name);
extern char *canonize(char *buffer);
extern ConfigItem_deny_dcc *dcc_isforbidden(aClient *sptr, char *filename);
extern ConfigItem_deny_dcc *dcc_isdiscouraged(aClient *sptr, char *filename);
extern int check_registered(aClient *);
extern int check_registered_user(aClient *);
extern char *get_client_name(aClient *, int);
extern char *get_client_host(aClient *);
extern char *myctime(time_t), *date(time_t);
extern int exit_client(aClient *, aClient *, aClient *, char *);
extern void initstats(), tstats(aClient *, char *);
extern char *check_string(char *);
extern char *make_nick_user_host(char *, char *, char *);
extern char *make_nick_user_host_r(char *namebuf, char *nick, char *name, char *host);
extern char *make_user_host(char *, char *);
extern int parse(aClient *, char *, char *);
extern int do_numeric(int, aClient *, aClient *, int, char **);
extern int hunt_server(aClient *, aClient *, char *, int, int, char **);
extern aClient *next_client(aClient *, char *);
extern int m_server_estab(aClient *);
extern void umode_init(void);
#define UMODE_GLOBAL 1
#define UMODE_LOCAL 0
extern int umode_allow_all(aClient *sptr, int what);
extern int umode_allow_unset(aClient *sptr, int what);
extern int umode_allow_opers(aClient *sptr, int what);
extern int umode_allow_none(aClient *sptr, int what);
extern int  umode_delete(char ch, long val);
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
extern void init_resolver(int firsttime);
extern time_t timeout_query_list(time_t);
extern time_t expire_cache(time_t);
extern void del_queries(char *);

extern void clear_channel_hash_table();
extern void clear_client_hash_table();
extern void clear_watch_hash_table();
extern int add_to_client_hash_table(char *, aClient *);
extern int del_from_client_hash_table(char *, aClient *);
extern int add_to_id_hash_table(char *, aClient *);
extern int del_from_id_hash_table(char *, aClient *);
extern int add_to_channel_hash_table(char *, aChannel *);
extern int del_from_channel_hash_table(char *, aChannel *);
extern int add_to_watch_hash_table(char *, aClient *, int);
extern int del_from_watch_hash_table(char *, aClient *);
extern int hash_check_watch(aClient *, int);
extern int hash_del_watch_list(aClient *);
extern void count_watch_memory(int *, u_long *);
extern aWatch *hash_get_watch(char *);
extern aChannel *hash_get_chan_bucket(unsigned int);
extern aClient *hash_find_client(const char *, aClient *);
extern aClient *hash_find_id(const char *, aClient *);
extern aClient *hash_find_nickserver(const char *, aClient *);
extern aClient *hash_find_server(const char *, aClient *);
extern char *find_by_aln(char *);
extern char *convert2aln(int);
extern int convertfromaln(char *);
extern char *find_server_aln(char *);
extern time_t atime(char *xtime);


/* Mode externs
*/
extern MODVAR long UMODE_INVISIBLE; /*  0x0001	 makes user invisible */
extern MODVAR long UMODE_OPER;      /*  0x0002	 Operator */
extern MODVAR long UMODE_WALLOP;    /*  0x0004	 send wallops to them */
extern MODVAR long UMODE_REGNICK;   /*  0x0020	 Nick set by services as registered */
extern MODVAR long UMODE_SERVNOTICE;/* 0x0100	 server notices such as kill */
extern MODVAR long UMODE_HIDE;	     /* 0x8000	 Hide from Nukes */
extern MODVAR long UMODE_SECURE;    /*	0x800000	 User is a secure connect */
extern MODVAR long UMODE_DEAF;      /* 0x10000000       Deaf */
extern MODVAR long UMODE_HIDEOPER;  /* 0x20000000	 Hide oper mode */
extern MODVAR long UMODE_SETHOST;   /* 0x40000000	 used sethost */
extern MODVAR long UMODE_HIDLE;     /* hides oper idle times */
extern MODVAR long AllUmodes, SendUmodes;

extern MODVAR long SNO_KILLS;
extern MODVAR long SNO_CLIENT;
extern MODVAR long SNO_FLOOD;
extern MODVAR long SNO_FCLIENT;
extern MODVAR long SNO_JUNK;
extern MODVAR long SNO_VHOST;
extern MODVAR long SNO_EYES;
extern MODVAR long SNO_TKL;
extern MODVAR long SNO_NICKCHANGE;
extern MODVAR long SNO_FNICKCHANGE;
extern MODVAR long SNO_QLINE;
extern MODVAR long SNO_SNOTICE;
extern MODVAR long SNO_SPAMF;
extern MODVAR long SNO_OPER;

#ifndef HAVE_STRLCPY
size_t strlcpy(char *dst, const char *src, size_t size);
#endif
#ifndef HAVE_STRLCAT
size_t strlcat(char *dst, const char *src, size_t size);
#endif
#ifndef HAVE_STRLNCAT
size_t strlncat(char *dst, const char *src, size_t size, size_t n);
#endif


extern int dopacket(aClient *, char *, int);

extern void debug(int, char *, ...);
#if defined(DEBUGMODE)
extern void send_usage(aClient *, char *);
extern void count_memory(aClient *, char *);
extern int checkprotoflags(aClient *, int, char *, int);
#endif

extern char *inetntop(int af, const void *in, char *local_dummy, size_t the_size);

/*
 * CommandHash -Stskeeps
*/
extern MODVAR aCommand *CommandHash[256];
extern void	init_CommandHash(void);
extern aCommand *add_Command_backend(char *cmd, int (*func)(), unsigned char parameters, int flags);
extern void	add_Command(char *cmd, int (*func)(), unsigned char parameters);
extern void	add_Command_to_list(aCommand *item, aCommand **list);
extern aCommand *del_Command_from_list(aCommand *item, aCommand **list);
extern int	del_Command(char *cmd, int (*func)());
extern void    add_CommandX(char *cmd, int (*func)(), unsigned char parameters, int flags);

/* CRULE */
char *crule_parse(char *);
int crule_test(char *);
char *crule_errstring(int);
int crule_eval(char *);
void crule_free(char **);

/*
 * Close all local socket connections, invalidate client fd's
 * WIN32 cleanup winsock lib
 */
extern void close_connections(void);

extern int b64_encode(unsigned char const *src, size_t srclength, char *target, size_t targsize);
extern int b64_decode(char const *src, unsigned char *target, size_t targsize);

extern int Auth_FindType(char *hash, char *type);
extern anAuthStruct	*Auth_ConvertConf2AuthStruct(ConfigEntry *ce);
extern void		Auth_DeleteAuthStruct(anAuthStruct *as);
extern int		Auth_Check(aClient *cptr, anAuthStruct *as, char *para);
extern char   		*Auth_Make(short type, char *para);
extern int   		Auth_CheckError(ConfigEntry *ce);

extern int is_chanownprotop(aClient *cptr, aChannel *chptr);
extern int is_skochanop(aClient *cptr, aChannel *chptr);
extern char *make_virthost(aClient *sptr, char *curr, char *new, int mode);
extern int  channel_canjoin(aClient *sptr, char *name);
extern char *collapse(char *pattern);
extern void dcc_sync(aClient *sptr);
extern void report_flines(aClient *sptr);
extern void report_network(aClient *sptr);
extern void report_dynconf(aClient *sptr);
extern void count_memory(aClient *cptr, char *nick);
extern void list_scache(aClient *sptr);
extern char *oflagstr(long oflag);
extern int rehash(aClient *cptr, aClient *sptr, int sig);
extern int _match(const char *mask, const char *name);
extern void outofmemory(void);
extern int add_listener2(ConfigItem_listen *conf);
extern void link_cleanup(ConfigItem_link *link_ptr);
extern void       listen_cleanup();
extern int  numeric_collides(long numeric);
extern u_long cres_mem(aClient *sptr, char *nick);
extern void      flag_add(char ch);
extern void      flag_del(char ch);
extern void init_dynconf(void);
extern char *pretty_time_val(long);
extern int        init_conf(char *filename, int rehash);
extern void       validate_configuration(void);
extern void       run_configuration(void);
extern void rehash_motdrules();
extern void read_motd(const char *filename, aMotdFile *motd); /* s_serv.c */
extern void send_proto(aClient *, ConfigItem_link *);
extern void unload_all_modules(void);
extern int set_blocking(int fd);
extern void set_sock_opts(int fd, aClient *cptr, int ipv6);
extern void iCstrip(char *line);
extern time_t rfc2time(char *s);
extern char *rfctime(time_t t, char *buf);
extern void *MyMallocEx(size_t size);
extern MODFUNC char  *ssl_get_cipher(SSL *ssl);
extern long config_checkval(char *value, unsigned short flags);
extern void config_status(char *format, ...) __attribute__((format(printf,1,2)));
extern void init_random();
extern u_char getrandom8();
extern u_int16_t getrandom16();
extern u_int32_t getrandom32();
#define EVENT_DRUGS BASE_VERSION
extern void rejoin_leave(aClient *sptr);
extern void rejoin_joinandmode(aClient *sptr);
extern void ident_failed(aClient *cptr);

extern MODVAR char extchmstr[4][64];
extern MODVAR char extbanstr[EXTBANTABLESZ+1];

extern int extcmode_default_requirechop(aClient *, aChannel *, char, char *, int, int);
extern int extcmode_default_requirehalfop(aClient *, aChannel *, char, char *, int, int);
extern Cmode_t extcmode_get(Cmode *);
extern void extcmode_init(void);
extern void make_extcmodestr();
extern void extcmode_duplicate_paramlist(void **xi, void **xo);
extern void extcmode_free_paramlist(void **ar);

extern void chmode_str(struct ChMode *, char *, char *, size_t, size_t);
extern char *get_cptr_status(aClient *);
extern char *get_snostr(long);
#ifdef _WIN32
extern void InitDebug(void);
extern int InitUnrealIRCd(int argc, char **);
extern void SocketLoop(void *);
#endif
extern void sendto_chmodemucrap(aClient *, aChannel *, char *);
extern void verify_opercount(aClient *, char *);
extern int valid_host(char *host);
extern int count_oper_sessions(char *);
extern char *unreal_mktemp(const char *dir, const char *suffix);
extern char *unreal_getpathname(char *filepath, char *path);
extern char *unreal_getfilename(char *path);
extern char *unreal_getmodfilename(char *path);
extern char *unreal_mkcache(const char *url);
extern int has_cached_version(const char *url);
extern int unreal_copyfile(const char *src, const char *dest);
extern int unreal_copyfileex(const char *src, const char *dest, int tryhardlink);
extern time_t unreal_getfilemodtime(const char *filename);
extern void unreal_setfilemodtime(const char *filename, time_t mtime);
extern void DeleteTempModules(void);
extern MODVAR Extban *extbaninfo;
extern Extban *findmod_by_bantype(char c);
extern Extban *ExtbanAdd(Module *reserved, ExtbanInfo req);
extern void ExtbanDel(Extban *);
extern void extban_init(void);
extern char *trim_str(char *str, int len);
extern MODVAR char *ban_realhost, *ban_virthost, *ban_ip;
extern char *unreal_checkregex(char *s, int fastsupport, int check_broadness);
extern int banact_stringtoval(char *s);
extern char *banact_valtostring(int val);
extern int banact_chartoval(char c);
extern char banact_valtochar(int val);
extern int spamfilter_gettargets(char *s, aClient *sptr);
extern char *spamfilter_target_inttostring(int v);
extern Spamfilter *unreal_buildspamfilter(char *s);
extern char *our_strcasestr(char *haystack, char *needle);
extern int spamfilter_getconftargets(char *s);
extern void remove_oper_snomasks(aClient *sptr);
extern void remove_oper_modes(aClient *sptr);
extern char *spamfilter_inttostring_long(int v);
extern aChannel *get_channel(aClient *cptr, char *chname, int flag);
extern MODVAR char backupbuf[];
extern void add_invite(aClient *, aClient *, aChannel *);
extern void channel_modes(aClient *cptr, char *mbuf, char *pbuf, size_t mbuf_size, size_t pbuf_size, aChannel *chptr);
extern MODVAR char modebuf[BUFSIZE], parabuf[BUFSIZE];
extern int op_can_override(char* acl, aClient *sptr,aChannel *channel,void* extra);
extern aClient *find_chasing(aClient *sptr, char *user, int *chasing);
extern MODVAR long opermode;
extern void add_user_to_channel(aChannel *chptr, aClient *who, int flags);
extern int add_banid(aClient *, aChannel *, char *);
extern int add_exbanid(aClient *cptr, aChannel *chptr, char *banid);
extern void sub1_from_channel(aChannel *);
extern MODVAR aCtab cFlagTab[];
extern char *unreal_encodespace(char *s);
extern char *unreal_decodespace(char *s);
extern MODVAR Link *helpign;
extern void DCCdeny_add(char *filename, char *reason, int type, int type2);
extern void DCCdeny_del(ConfigItem_deny_dcc *deny);
extern void dcc_wipe_services(void);
extern void reread_motdsandrules();
extern MODVAR int SVSNOOP;
extern int callbacks_check(void);
extern void callbacks_switchover(void);
extern int efunctions_check(void);
extern void efunctions_switchover(void);
extern char *encode_ip(u_char *);
extern char *decode_ip(char *);
extern void sendto_fconnectnotice(aClient *sptr, int disconnect, char *comment);
extern void sendto_one_nickcmd(aClient *cptr, aClient *sptr, char *umodes);
extern int on_dccallow_list(aClient *to, aClient *from);
extern int add_dccallow(aClient *sptr, aClient *optr);
extern int del_dccallow(aClient *sptr, aClient *optr);
extern void delete_linkblock(ConfigItem_link *link_ptr);
extern void delete_classblock(ConfigItem_class *class_ptr);
extern void del_async_connects(void);
extern void make_extbanstr(void);
extern void isupport_init(void);
extern void clicap_init(void);
extern int __attribute__((warn_unused_result)) do_cmd(aClient *cptr, aClient *sptr, char *cmd, int parc, char *parv[]);
extern void create_snomask(aClient *sptr, anUser *user, char *snomask);
extern MODVAR char *me_hash;
extern MODVAR int dontspread;
/* Efuncs */
extern MODVAR int (*do_join)(aClient *, aClient *, int, char **);
extern MODVAR void (*join_channel)(aChannel *chptr, aClient *cptr, aClient *sptr, int flags);
extern MODVAR int (*can_join)(aClient *cptr, aClient *sptr, aChannel *chptr, char *key, char *parv[]);
extern MODVAR void (*do_mode)(aChannel *chptr, aClient *cptr, aClient *sptr, int parc, char *parv[], time_t sendts, int samode);
extern MODVAR void (*set_mode)(aChannel *chptr, aClient *cptr, int parc, char *parv[], u_int *pcount,
    char pvar[MAXMODEPARAMS][MODEBUFLEN + 3], int bounce);
extern MODVAR int (*m_umode)(aClient *, aClient *, int, char **);
extern MODVAR int (*register_user)(aClient *cptr, aClient *sptr, char *nick, char *username, char *umode, char *virthost, char *ip);
extern MODVAR int (*tkl_hash)(unsigned int c);
extern MODVAR char (*tkl_typetochar)(int type);
extern MODVAR aTKline *(*tkl_add_line)(int type, char *usermask, char *hostmask, char *reason, char *setby,
                  TS expire_at, TS set_at, TS spamf_tkl_duration, char *spamf_tkl_reason, MatchType match_type);
extern MODVAR aTKline *(*tkl_del_line)(aTKline *tkl);
extern MODVAR void (*tkl_check_local_remove_shun)(aTKline *tmp);
extern MODVAR aTKline *(*tkl_expire)(aTKline * tmp);
extern MODVAR EVENT((*tkl_check_expire));
extern MODVAR int (*find_tkline_match)(aClient *cptr, int xx);
extern MODVAR int (*find_shun)(aClient *cptr);
extern MODVAR int (*find_spamfilter_user)(aClient *sptr, int flags);
extern MODVAR aTKline *(*find_qline)(aClient *cptr, char *nick, int *ishold);
extern MODVAR int  (*find_tkline_match_zap)(aClient *cptr);
extern MODVAR int  (*find_tkline_match_zap_ex)(aClient *cptr, aTKline **rettk);
extern MODVAR void (*tkl_stats)(aClient *cptr, int type, char *para);
extern MODVAR void (*tkl_synch)(aClient *sptr);
extern MODVAR int (*m_tkl)(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern MODVAR int (*place_host_ban)(aClient *sptr, int action, char *reason, long duration);
extern MODVAR int (*dospamfilter)(aClient *sptr, char *str_in, int type, char *target, int flags, aTKline **rettk);
extern MODVAR int (*dospamfilter_viruschan)(aClient *sptr, aTKline *tk, int type);
extern MODVAR void (*send_list)(aClient *cptr);
extern MODVAR unsigned char *(*StripColors)(unsigned char *text);
extern MODVAR const char *(*StripControlCodes)(unsigned char *text);
extern MODVAR void (*spamfilter_build_user_string)(char *buf, char *nick, aClient *acptr);
extern MODVAR int (*is_silenced)(aClient *sptr, aClient *acptr);
extern MODVAR void (*send_protoctl_servers)(aClient *sptr, int response);
extern MODVAR int (*verify_link)(aClient *cptr, aClient *sptr, char *servername, ConfigItem_link **link_out);
extern MODVAR void (*send_server_message)(aClient *sptr);
extern MODVAR void (*broadcast_md_client)(ModDataInfo *mdi, aClient *acptr, ModData *md);
extern MODVAR void (*broadcast_md_channel)(ModDataInfo *mdi, aChannel *chptr, ModData *md);
extern MODVAR void (*broadcast_md_member)(ModDataInfo *mdi, aChannel *chptr, Member *m, ModData *md);
extern MODVAR void (*broadcast_md_membership)(ModDataInfo *mdi, aClient *acptr, Membership *m, ModData *md);
extern MODVAR void (*broadcast_md_client_cmd)(aClient *except, aClient *sender, aClient *acptr, char *varname, char *value);
extern MODVAR void (*broadcast_md_channel_cmd)(aClient *except, aClient *sender, aChannel *chptr, char *varname, char *value);
extern MODVAR void (*broadcast_md_member_cmd)(aClient *except, aClient *sender, aChannel *chptr, aClient *acptr, char *varname, char *value);
extern MODVAR void (*broadcast_md_membership_cmd)(aClient *except, aClient *sender, aClient *acptr, aChannel *chptr, char *varname, char *value);
extern MODVAR void (*send_moddata_client)(aClient *srv, aClient *acptr);
extern MODVAR void (*send_moddata_channel)(aClient *srv, aChannel *chptr);
extern MODVAR void (*send_moddata_members)(aClient *srv);
extern MODVAR void (*broadcast_moddata_client)(aClient *acptr);
extern MODVAR int (*check_banned)(aClient *cptr);
extern MODVAR void (*introduce_user)(aClient *to, aClient *acptr);
extern MODVAR int (*check_deny_version)(aClient *cptr, char *version_string, int protocol, char *flags);

/* /Efuncs */
extern MODVAR aMotdFile opermotd, svsmotd, motd, botmotd, smotd, rules;
extern MODVAR int max_connection_count;
extern int add_listmode(Ban **list, aClient *cptr, aChannel *chptr, char *banid);
extern int del_listmode(Ban **list, aChannel *chptr, char *banid);
extern int Halfop_mode(long mode);
extern char *clean_ban_mask(char *, int, aClient *);
extern int find_invex(aChannel *chptr, aClient *sptr);
extern void DoMD5(unsigned char *mdout, const unsigned char *src, unsigned long n);
extern char *md5hash(unsigned char *dst, const unsigned char *src, unsigned long n);
extern void charsys_reset(void);
extern void charsys_addmultibyterange(char s1, char e1, char s2, char e2);
extern void charsys_addallowed(char *s);
extern void charsys_reset(void);
extern MODVAR char langsinuse[4096];
extern MODVAR char *casemapping[2];
extern MODVAR aTKline *tklines[TKLISTLEN];
extern char *cmdname_by_spamftarget(int target);
extern void unrealdns_delreq_bycptr(aClient *cptr);
extern void sendtxtnumeric(aClient *to, char *pattern, ...) __attribute__((format(printf,2,3)));;
extern void unrealdns_gethostbyname_link(char *name, ConfigItem_link *conf);
extern void unrealdns_delasyncconnects(void);
extern int is_autojoin_chan(char *chname);
extern void unreal_free_hostent(struct hostent *he);
extern int match_esc(const char *mask, const char *name);
extern struct hostent *unreal_create_hostent(char *name, char *ip);
extern char *unreal_time_synch_error(void);
extern int unreal_time_synch(int timeout);
extern char *getcloak(aClient *sptr);
extern MODVAR unsigned char param_to_slot_mapping[256];
extern char *cm_getparameter(aChannel *chptr, char mode);
extern void cm_putparameter(aChannel *chptr, char mode, char *str);
extern void cm_freeparameter(aChannel *chptr, char mode);
extern char *cm_getparameter_ex(void **p, char mode);
extern void cm_putparameter_ex(void **p, char mode, char *str);
extern void cm_freeparameter_ex(void **p, char mode, char *str);
extern int file_exists(char* file);
extern void free_motd(aMotdFile *motd); /* s_serv.c */
extern void fix_timers(void);
extern char *chfl_to_sjoin_symbol(int s);
extern char chfl_to_chanmode(int s);
extern void add_pending_net(aClient *sptr, char *str);
extern void free_pending_net(aClient *sptr);
extern aClient *find_non_pending_net_duplicates(aClient *cptr);
extern aPendingNet *find_pending_net_by_sid_butone(char *sid, aClient *exempt);
extern aClient *find_pending_net_duplicates(aClient *cptr, aClient **srv, char **sid);
extern MODVAR char serveropts[];
extern MODVAR char *IsupportStrings[];
extern void finish_auth(aClient *acptr);
extern void read_packet(int fd, int revents, void *data);
extern void sendto_realops_and_log(char *fmt, ...);
extern int parse_chanmode(ParseMode *pm, char *modebuf_in, char *parabuf_in);
extern int ssl_used_in_config_but_unavail(void);
extern void config_report_ssl_error(void);
extern int dead_link(aClient *to, char *notice);
extern aMatch *unreal_create_match(MatchType type, char *str, char **error);
extern void unreal_delete_match(aMatch *m);
extern int unreal_match(aMatch *m, char *str);
extern int unreal_match_method_strtoval(char *str);
extern char *unreal_match_method_valtostr(int val);
extern int mixed_network(void);
extern void unreal_delete_masks(ConfigItem_mask *m);
extern void unreal_add_masks(ConfigItem_mask **head, ConfigEntry *ce);
extern int unreal_mask_match(aClient *acptr, ConfigItem_mask *m);
extern char *our_strcasestr(char *haystack, char *needle);
extern void update_conf(void);
extern MODVAR int need_34_upgrade;
#ifdef _WIN32
extern MODVAR BOOL IsService;
#endif
extern int match_ip46(char *a, char *b);
extern void extcmodes_check_for_changes(void);
extern int config_parse_flood(char *orig, int *times, int *period);
extern int swhois_add(aClient *acptr, char *tag, int priority, char *swhois, aClient *from, aClient *skip);
extern int swhois_delete(aClient *acptr, char *tag, char *swhois, aClient *from, aClient *skip);
extern void remove_oper_privileges(aClient *sptr, int broadcast_mode_change);
extern int client_starttls(aClient *acptr);
extern void start_server_handshake(aClient *cptr);
extern void reject_insecure_server(aClient *cptr);
extern void ident_failed(aClient *cptr);
extern void report_crash(void);
extern int inet_pton4(const char *src, unsigned char *dst);
extern int inet_pton6(const char *src, unsigned char *dst);
extern int unreal_bind(int fd, char *ip, int port, int ipv6);
extern int unreal_connect(int fd, char *ip, int port, int ipv6);
extern int is_valid_ip(char *str);
extern int ipv6_capable(void);
extern MODVAR aClient *remote_rehash_client;
extern MODVAR int debugfd;
extern void convert_to_absolute_path(char **path, char *reldir);
extern int has_channel_mode(aChannel *chptr, char mode);
extern void start_listeners(void);
extern void buildvarstring(char *inbuf, char *outbuf, size_t len, char *name[], char *value[]);
extern void reinit_ssl(aClient *);
extern int m_error(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_dns(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_info(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_summon(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_users(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_version(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_dalinfo(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_credits(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_license(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_module(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_alias(aClient *cptr, aClient *sptr, int parc, char *parv[], char *cmd); /* special! */
extern int m_rehash(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_die(aClient *cptr, aClient *sptr, int parc, char *parv[]);
extern int m_restart(aClient *cptr, aClient *sptr, int parc, char *parv[]);
