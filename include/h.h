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
extern char *extraflags;
extern int tainted;
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
extern TS check_pings(TS now);
extern TS TS2ts(char *s);
extern time_t timeofday;
/* newconf */
#define get_sendq(x) ((x)->class ? (x)->class->sendq : MAXSENDQLENGTH) 
/* get_recvq is only called in send.c for local connections */
#define get_recvq(x) ((x)->class->recvq ? (x)->class->recvq : CLIENT_FLOOD) 

#define CMD_FUNC(x) int (x) (aClient *cptr, aClient *sptr, int parc, char *parv[])

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
extern ConfigItem_class		*default_class;
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
extern ConfigItem_badword	*conf_badword_quit;
extern ConfigItem_deny_dcc	*conf_deny_dcc;
extern ConfigItem_deny_channel  *conf_deny_channel;
extern ConfigItem_deny_link	*conf_deny_link;
extern ConfigItem_allow_channel *conf_allow_channel;
extern ConfigItem_deny_version	*conf_deny_version;
extern ConfigItem_log		*conf_log;
extern ConfigItem_alias		*conf_alias;
extern ConfigItem_include	*conf_include;
extern ConfigItem_help		*conf_help;
extern int		completed_connection(aClient *);
extern void clear_unknown();
extern EVENT(tkl_check_expire);
extern EVENT(e_unload_module_delayed);
#ifdef THROTTLING
extern EVENT(e_clean_out_throttling_buckets);
#endif

extern void  module_loadall(int module_load);
extern long set_usermode(char *umode);
extern char *get_modestr(long umodes);
extern void tkl_stats(aClient *cptr);
extern void                    config_error(char *format, ...);
extern int			config_verbose;
extern void config_progress(char *format, ...);
extern void       ipport_seperate(char *string, char **ip, char **port);
ConfigItem_class	*Find_class(char *name);
ConfigItem_deny_dcc	*Find_deny_dcc(char *name);
ConfigItem_oper		*Find_oper(char *name);
ConfigItem_listen	*Find_listen(char *ipmask, int port);
ConfigItem_ulines	*Find_uline(char *host);
ConfigItem_except	*Find_except(char *host, short type);
ConfigItem_tld		*Find_tld(aClient *cptr, char *host);
ConfigItem_link		*Find_link(char *username, char *hostname, char *ip, char *servername);
ConfigItem_ban 		*Find_ban(char *host, short type);
ConfigItem_ban 		*Find_banEx(char *host, short type, short type2);
ConfigItem_vhost	*Find_vhost(char *name);
ConfigItem_deny_channel *Find_channel_allowed(char *name);
ConfigItem_alias	*Find_alias(char *name);
ConfigItem_help 	*Find_Help(char *command);
int			AllowClient(aClient *cptr, struct hostent *hp, char *sockhost);
int parse_netmask(const char *text, struct IN_ADDR *addr, int *b);
int match_ipv4(struct IN_ADDR *addr, struct IN_ADDR *mask, int b);
#ifdef INET6
int match_ipv6(struct IN_ADDR *addr, struct IN_ADDR *mask, int b);
#endif
aMotd *read_motd(char *filename);
aMotd *read_rules(char *filename);
extern struct tm *motd_tm;
extern Link	*Servers;
void add_ListItem(ListStruct *, ListStruct **);
ListStruct *del_ListItem(ListStruct *, ListStruct **);
/* Remmed out for win32 compatibility.. as stated of 467leaf win32 port.. */
extern aClient *find_match_server(char *mask);
extern LoopStruct loop;
extern int del_banid(aChannel *chptr, char *banid);
extern int del_exbanid(aChannel *chptr, char *banid);
#ifdef SHOWCONNECTINFO


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

extern int R_do_dns, R_fin_dns, R_fin_dnsc, R_fail_dns,
    R_do_id, R_fin_id, R_fail_id;

#endif
extern inline aCommand *find_Command(char *cmd, short token, int flags);
extern aCommand *find_Command_simple(char *cmd);
extern aChannel *find_channel(char *, aChannel *);
extern Membership *find_membership_link(Membership *lp, aChannel *ptr);
extern Member *find_member_link(Member *, aClient *);
extern void remove_user_from_channel(aClient *, aChannel *);
extern char *base64enc(long);
extern long base64dec(char *);
extern void add_server_to_table(aClient *);
extern void remove_server_from_table(aClient *);
extern void iNAH_host(aClient *sptr, char *host);
extern void set_snomask(aClient *sptr, char *snomask);
extern char *get_sno_str(aClient *sptr);
/* for services */
extern void del_invite(aClient *, aChannel *);
extern int add_silence(aClient *, char *, int);
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

#if !defined(HAVE_SNPRINT) || !defined(HAVE_VSNPRINTF)
#ifndef _WIN32
extern int snprintf (char *str, size_t count, const char *fmt, ...);
extern int vsnprintf (char *str, size_t count, const char *fmt, va_list arg);
#endif
#endif

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


extern void restart(char *);
extern void send_channel_modes(aClient *, aChannel *);
extern void server_reboot(char *);
extern void terminate(), write_pidfile();
extern void *MyMallocEx(size_t size);
extern int advanced_check(char *userhost, int ipstat);
extern int send_queued(aClient *);
/* i know this is naughty but :P --stskeeps */
extern void send_channel_modes_sjoin(aClient *cptr, aChannel *chptr);
extern void send_channel_modes_sjoin3(aClient *cptr, aChannel *chptr);
extern void sendto_locfailops(char *pattern, ...);
extern void sendto_connectnotice(char *nick, anUser *user, aClient *sptr, int disconnect, char *comment);
extern void sendto_serv_butone_nickcmd(aClient *one, aClient *sptr, char *nick, int hopcount,
int lastnick, char *username, char *realhost, char *server, long servicestamp, char *info, char *umodes,
char *virthost);
extern void    sendto_message_one(aClient *to, aClient *from, char *sender,
    char *cmd, char *nick, char *msg);
#define PREFIX_ALL		0
#define PREFIX_HALFOP	0x1
#define PREFIX_VOICE	0x2
#define PREFIX_OP		0x4
extern void sendto_channelprefix_butone(aClient *one, aClient *from, aChannel *chptr,
    int prefix, char *pattern, ...);
extern void sendto_channelprefix_butone_tok(aClient *one, aClient *from, aChannel *chptr,
    int prefix, char *cmd, char *tok, char *nick, char *text);
extern void sendto_channel_butone(aClient *, aClient *, aChannel *, char *,
    ...);
extern void sendto_channel_butserv_butone(aChannel *chptr, aClient *from, aClient *one, char *pattern, ...);
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
extern void sendto_snomask(int snomask, char *pattern, ...);
extern int writecalls, writeb[];
extern int deliver_it(aClient *, char *, int);
extern int  check_for_chan_flood(aClient *cptr, aClient *sptr, aChannel *chptr);
extern int  check_for_target_limit(aClient *sptr, void *target, const char *name);
extern char *stripbadwords_message(char *str, int *);
extern char *stripbadwords_channel(char *str, int *);
extern char *stripbadwords_quit(char *str, int *);
extern char *stripbadwords(char *, ConfigItem_badword *, int *);
extern unsigned char *StripColors(unsigned char *);
extern const char *StripControlCodes(unsigned char *text);
extern char *canonize(char *buffer);
extern int webtv_parse(aClient *sptr, char *string);
extern ConfigItem_deny_dcc *dcc_isforbidden(aClient *cptr, aClient *sptr, aClient *target, char *filename);
extern int check_registered(aClient *);
extern int check_registered_user(aClient *);
extern char *get_client_name(aClient *, int);
extern char *get_client_host(aClient *);
extern char *myctime(time_t), *date(time_t);
extern int exit_client(aClient *, aClient *, aClient *, char *);
extern void initstats(), tstats(aClient *, char *);
extern char *check_string(char *);
extern char *make_nick_user_host(char *, char *, char *);
extern char *make_user_host(char *, char *);
extern int parse(aClient *, char *, char *);
extern int do_numeric(int, aClient *, aClient *, int, char **);
extern int hunt_server(aClient *, aClient *, char *, int, int, char **);
extern int hunt_server_token(aClient *, aClient *, char *, char *, char *, int, int, char **);
extern int hunt_server_token_quiet(aClient *, aClient *, char *, char *, char *, int, int, char **);
extern aClient *next_client(aClient *, char *);
extern int m_umode(aClient *, aClient *, int, char **);
extern int m_names(aClient *, aClient *, int, char **);
extern int m_server_estab(aClient *);
extern void umode_init(void);
extern long umode_get(char, int);
#define umode_lget(x) umode_get(x, 0);
#define umode_gget(x) umode_get(x, 1);
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
#ifndef _WIN32
extern struct hostent *get_res(char *);
#else
extern struct hostent *get_res(char *, long);
#endif
extern struct hostent *gethost_byaddr(char *, Link *);
extern struct hostent *gethost_byname(char *, Link *);
extern void flush_cache();
extern int init_resolver(int);
extern time_t timeout_query_list(time_t);
extern time_t expire_cache(time_t);
extern void del_queries(char *);

extern void clear_channel_hash_table();
extern void clear_client_hash_table();
extern void clear_watch_hash_table();
extern int add_to_client_hash_table(char *, aClient *);
extern int del_from_client_hash_table(char *, aClient *);
extern int add_to_channel_hash_table(char *, aChannel *);
extern int del_from_channel_hash_table(char *, aChannel *);
extern int add_to_watch_hash_table(char *, aClient *);
extern int del_from_watch_hash_table(char *, aClient *);
extern int hash_check_watch(aClient *, int);
extern int hash_del_watch_list(aClient *);
extern void count_watch_memory(int *, u_long *);
extern aWatch *hash_get_watch(char *);
extern aChannel *hash_get_chan_bucket(unsigned int);
extern aClient *hash_find_client(char *, aClient *);
extern aClient *hash_find_nickserver(char *, aClient *);
extern aClient *hash_find_server(char *, aClient *);
extern char *find_by_aln(char *);
extern char *convert2aln(int);
extern int convertfromaln(char *);
extern char *find_server_aln(char *);
extern time_t atime(char *xtime);


/* Mode externs
*/
extern long UMODE_INVISIBLE; /*  0x0001	 makes user invisible */
extern long UMODE_OPER;      /*  0x0002	 Operator */
extern long UMODE_WALLOP;    /*  0x0004	 send wallops to them */
extern long UMODE_FAILOP;    /*  0x0008	 Shows some global messages */
extern long UMODE_HELPOP;    /*  0x0010	 Help system operator */
extern long UMODE_REGNICK;   /*  0x0020	 Nick set by services as registered */
extern long UMODE_SADMIN;    /*  0x0040	 Services Admin */
extern long UMODE_ADMIN;     /*  0x0080	 Admin */
extern long UMODE_SERVNOTICE;/* 0x0100	 server notices such as kill */
extern long UMODE_LOCOP;     /* 0x0200	 Local operator -- SRB */
extern long UMODE_RGSTRONLY; /* 0x0400  Only reg nick message */
extern long UMODE_WEBTV;     /* 0x0800  WebTV Client */
extern long UMODE_SERVICES;  /* 0x4000	 services */
extern long UMODE_HIDE;	     /* 0x8000	 Hide from Nukes */
extern long UMODE_NETADMIN;  /* 0x10000	 Network Admin */
extern long UMODE_COADMIN;   /* 0x80000	 Co Admin */
extern long UMODE_WHOIS;     /* 0x100000	 gets notice on /whois */
extern long UMODE_KIX;       /* 0x200000	 usermode +q */
extern long UMODE_BOT;       /* 0x400000	 User is a bot */
extern long UMODE_SECURE;    /*	0x800000	 User is a secure connect */
extern long UMODE_VICTIM;    /* 0x8000000	 Intentional Victim */
extern long UMODE_DEAF;      /* 0x10000000       Deaf */
extern long UMODE_HIDEOPER;  /* 0x20000000	 Hide oper mode */
extern long UMODE_SETHOST;   /* 0x40000000	 used sethost */
extern long UMODE_STRIPBADWORDS; /* 0x80000000	 */
extern long UMODE_HIDEWHOIS; /* hides channels in /whois */
extern long AllUmodes, SendUmodes;

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
extern void send_listinfo(aClient *, char *);
extern void count_memory(aClient *, char *);
#endif

#ifdef INET6
extern char *inetntop(int af, const void *in, char *local_dummy,
    size_t the_size);
#endif

/*
 * socket.c
*/

char	*Inet_si2p(struct SOCKADDR_IN *sin);
char	*Inet_si2pB(struct SOCKADDR_IN *sin, char *buf, int sz);
char	*Inet_ia2p(struct IN_ADDR *ia);
char	*Inet_ia2pNB(struct IN_ADDR *ia, int compressed);

/*
 * CommandHash -Stskeeps
*/
extern aCommand *CommandHash[256];
extern void	init_CommandHash(void);
extern aCommand	*add_Command_backend(char *cmd, int (*func)(), unsigned char parameters, unsigned char token, int flags);
extern void	add_Command(char *cmd, char *token, int (*func)(), unsigned char parameters);
extern void	add_Command_to_list(aCommand *item, aCommand **list);
extern aCommand *del_Command_from_list(aCommand *item, aCommand **list);
extern int	del_Command(char *cmd, char *token, int (*func)());
extern void    add_CommandX(char *cmd, char *token, int (*func)(), unsigned char parameters, int flags);

/* CRULE */
char *crule_parse(char *);
int crule_test(char *);
char *crule_errstring(int);
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

extern int b64_encode(unsigned char const *src, size_t srclength, char *target, size_t targsize);
extern int b64_decode(char const *src, unsigned char *target, size_t targsize);

extern int		Auth_FindType(char *type);
extern anAuthStruct	*Auth_ConvertConf2AuthStruct(ConfigEntry *ce);
extern void		Auth_DeleteAuthStruct(anAuthStruct *as);
extern int		Auth_Check(aClient *cptr, anAuthStruct *as, char *para);
extern char   		*Auth_Make(short type, char *para);
extern int   		Auth_CheckError(ConfigEntry *ce);

extern long xbase64dec(char *b64);
extern aClient *find_server_b64_or_real(char *name);
extern aClient *find_server_by_base64(char *b64);
extern int is_chanownprotop(aClient *cptr, aChannel *chptr);
extern char *make_virthost(char *curr, char *new, int mode);
extern int  channel_canjoin(aClient *sptr, char *name);
extern char *collapse(char *pattern);
extern void send_list(aClient *cptr, int numsend);
extern int  find_tkline_match_zap(aClient *cptr);
extern int  find_shun(aClient *cptr);
extern void tkl_synch(aClient *sptr);
extern void dcc_sync(aClient *sptr);
extern void report_flines(aClient *sptr);
extern void report_network(aClient *sptr);
extern void report_dynconf(aClient *sptr);
extern void count_memory(aClient *cptr, char *nick);
extern void list_scache(aClient *sptr);
extern void ns_stats(aClient *cptr);
extern char *oflagstr(long oflag);
extern int rehash(aClient *cptr, aClient *sptr, int sig);
extern int _match(char *mask, char *name);
extern void outofmemory(void);
extern unsigned long our_crc32(const unsigned char *s, unsigned int len);
extern int add_listener2(ConfigItem_listen *conf);
extern void link_cleanup(ConfigItem_link *link_ptr);
extern void       listen_cleanup();
extern int  numeric_collides(long numeric);
extern u_long cres_mem(aClient *sptr, char *nick);
extern void      flag_add(char *ch);
extern void      flag_del(char ch);
extern void init_dynconf(void);
extern char *pretty_time_val(long);
extern int        init_conf(char *filename, int rehash);
extern void       validate_configuration(void);
extern void       run_configuration(void);
extern aMotd *read_file(char *filename, aMotd **list);
CMD_FUNC(m_server_remote);
extern void send_proto(aClient *, ConfigItem_link *);
extern char *xbase64enc(long i);
extern void unload_all_modules(void);
extern void flush_fdlist_connections(fdlist * listp);
extern int set_blocking(int fd);
extern void set_sock_opts(int fd, aClient *cptr);
extern void iCstrip(char *line);
extern time_t rfc2time(char *s);
extern char *rfctime(time_t t, char *buf);
extern void *MyMallocEx(size_t size);
#ifdef USE_SSL
char  *ssl_get_cipher(SSL *ssl);
#endif
long config_checkval(char *value, unsigned short flags);
void config_status(char *format, ...);
void init_random();
u_int32_t getrandom32();
extern char trouble_info[1024];
#define EVENT_DRUGS BASE_VERSION
extern void rejoin_doparts(aClient *sptr);
extern void rejoin_dojoinandmode(aClient *sptr);
