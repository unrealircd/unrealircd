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
#ifndef NO_FDLIST
#include "fdlist.h"
#endif
extern MODVAR char *extraflags;
extern MODVAR int tainted;
/* for the new s_err.c */
extern char *getreply(int);
#define rpl_str(x) getreply(x)
#define err_str(x) getreply(x)
extern MODVAR Member *freemember;
extern MODVAR Membership *freemembership;
extern MODVAR MembershipL *freemembershipL;
extern MODVAR TS nextconnect, nextdnscheck, nextping;
extern MODVAR aClient *client, me, *local[];
extern MODVAR aChannel *channel;
extern MODVAR struct stats *ircstp;
extern MODVAR int bootopt;
extern MODVAR time_t TSoffset;
/* Prototype added to force errors -- Barubary */
extern TS check_pings(TS now);
extern TS TS2ts(char *s);
extern MODVAR time_t timeofday;
/* newconf */
#define get_sendq(x) ((x)->class ? (x)->class->sendq : MAXSENDQLENGTH) 
/* get_recvq is only called in send.c for local connections */
#define get_recvq(x) ((x)->class->recvq ? (x)->class->recvq : CLIENT_FLOOD) 

#define CMD_FUNC(x) int (x) (aClient *cptr, aClient *sptr, int parc, char *parv[])

#ifndef NO_FDLIST
extern MODVAR float currentrate;
extern MODVAR float currentrate2;		/* outgoing */
extern MODVAR float highest_rate;
extern MODVAR float highest_rate2;
extern MODVAR int  lifesux;
extern MODVAR int  LRV;
extern MODVAR time_t   LCF;
extern MODVAR int  currlife;
extern MODVAR int  HTMLOCK;
extern MODVAR int  noisy_htm;
extern MODVAR long lastsendK, lastrecvK;
#endif

/*
 * Configuration linked lists
*/
extern MODVAR ConfigItem_me		*conf_me;
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
extern MODVAR ConfigItem_badword	*conf_badword_channel;
extern MODVAR ConfigItem_badword       *conf_badword_message;
extern MODVAR ConfigItem_badword	*conf_badword_quit;
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
extern int		completed_connection(aClient *);
extern void clear_unknown();
extern EVENT(e_unload_module_delayed);
extern EVENT(e_clean_out_throttling_buckets);

extern void  module_loadall(int module_load);
extern long set_usermode(char *umode);
extern char *get_modestr(long umodes);
extern void                    config_error(char *format, ...) __attribute__((format(printf,1,2)));
extern MODVAR int config_verbose;
extern void config_progress(char *format, ...) __attribute__((format(printf,1,2)));
extern void       ipport_seperate(char *string, char **ip, char **port);
ConfigItem_class	*Find_class(char *name);
ConfigItem_deny_dcc	*Find_deny_dcc(char *name);
ConfigItem_oper		*Find_oper(char *name);
ConfigItem_listen	*Find_listen(char *ipmask, int port);
ConfigItem_ulines	*Find_uline(char *host);
ConfigItem_except	*Find_except(aClient *, char *host, short type);
ConfigItem_tld		*Find_tld(aClient *cptr, char *host);
ConfigItem_link		*Find_link(char *username, char *hostname, char *ip, char *servername);
ConfigItem_cgiirc *Find_cgiirc(char *username, char *hostname, char *ip, CGIIRCType type);
ConfigItem_ban 		*Find_ban(aClient *, char *host, short type);
ConfigItem_ban 		*Find_banEx(aClient *,char *host, short type, short type2);
ConfigItem_vhost	*Find_vhost(char *name);
ConfigItem_deny_channel *Find_channel_allowed(char *name);
ConfigItem_alias	*Find_alias(char *name);
ConfigItem_help 	*Find_Help(char *command);
int			AllowClient(aClient *cptr, struct hostent *hp, char *sockhost, char *username);
int parse_netmask(const char *text, struct irc_netmask *netmask);
int match_ip(struct IN_ADDR addr, char *uhost, char *mask, struct irc_netmask *netmask);
ConfigItem_ban  *Find_ban_ip(aClient *sptr);
extern MODVAR struct tm motd_tm, smotd_tm;
extern MODVAR Link	*Servers;
void add_ListItem(ListStruct *, ListStruct **);
ListStruct *del_ListItem(ListStruct *, ListStruct **);
/* Remmed out for win32 compatibility.. as stated of 467leaf win32 port.. */
extern aClient *find_match_server(char *mask);
extern MODVAR LoopStruct loop;
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

extern MODVAR char REPORT_DO_DNS[256], REPORT_FIN_DNS[256], REPORT_FIN_DNSC[256],
    REPORT_FAIL_DNS[256], REPORT_DO_ID[256], REPORT_FIN_ID[256],
    REPORT_FAIL_ID[256];

extern MODVAR int R_do_dns, R_fin_dns, R_fin_dnsc, R_fail_dns,
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
extern int do_remote_nick_name(char *);
extern int can_send(aClient *, aChannel *, char *, int);
extern long get_access(aClient *, aChannel *);
extern int is_chan_op(aClient *, aChannel *);
extern int has_voice(aClient *, aChannel *);
extern int is_chanowner(aClient *, aChannel *);
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
extern void get_sockhost(aClient *, char *);
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
extern MODVAR int dbufalloc, dbufblocks, debuglevel;
#else
extern int dbufalloc, dbufblocks, debuglevel, errno, h_errno;
#endif

extern MODVAR short LastSlot; /* last used index in local client array */
extern MODVAR int OpenFiles;  /* number of files currently open */
extern MODVAR int debuglevel, portnum, debugtty, maxusersperchannel;
extern MODVAR int readcalls, udpfd, resfd;

#ifndef NEW_IO
extern aClient *add_connection(aClient *, int);
extern int add_listener(aConfItem *);
extern int check_client(aClient *, char *);
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
#else /* ifndef NEW_IO */
#endif /* ifndef NEW_IO */
extern int setup_ping();

extern void start_auth(aClient *);
extern void read_authports(aClient *);
extern void send_authports(aClient *);


extern void restart(char *);
extern void server_reboot(char *);
extern void terminate(), write_pidfile();
extern void *MyMallocEx(size_t size);
extern int advanced_check(char *userhost, int ipstat);
extern int send_queued(aClient *);
/* i know this is naughty but :P --stskeeps */
extern void sendto_locfailops(char *pattern, ...) __attribute__((format(printf,1,2)));
extern void sendto_connectnotice(char *nick, anUser *user, aClient *sptr, int disconnect, char *comment);
extern void sendto_serv_butone_nickcmd(aClient *one, aClient *sptr, char *nick, int hopcount,
long lastnick, char *username, char *realhost, char *server, long servicestamp, char *info, char *umodes,
char *virthost);
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
extern void sendto_channelprefix_butone_tok(aClient *one, aClient *from, aChannel *chptr,
    int prefix, char *cmd, char *tok, char *nick, char *text, char do_send_check);
extern void sendto_channel_butone(aClient *, aClient *, aChannel *,
                                  char *, ...) __attribute__((format(printf,4,5)));
extern void sendto_channel_butserv_butone(aChannel *chptr, aClient *from, aClient *one,
                                          char *pattern, ...) __attribute__((format(printf,4,5)));
extern void sendto_serv_butone(aClient *, char *, ...) __attribute__((format(printf,2,3)));
extern void sendto_serv_butone_quit(aClient *, char *, ...) __attribute__((format(printf,2,3)));
extern void sendto_serv_butone_sjoin(aClient *, char *, ...) __attribute__((format(printf,2,3)));
extern void sendto_serv_sjoin(aClient *, char *, ...) __attribute__((format(printf,2,3)));
extern void sendto_common_channels(aClient *, char *, ...) __attribute__((format(printf,2,3)));
extern void sendto_channel_butserv(aChannel *, aClient *, char *, ...) __attribute__((format(printf,3,4)));
extern void sendto_match_servs(aChannel *, aClient *, char *, ...) __attribute__((format(printf,3,4)));
extern void sendto_match_butone(aClient *, aClient *, char *, int,
    char *pattern, ...) __attribute__((format(printf,5,6)));
extern void sendto_all_butone(aClient *, aClient *, char *, ...) __attribute__((format(printf,3,4)));
extern void sendto_ops(char *, ...) __attribute__((format(printf,1,2)));
extern void sendto_ops_butone(aClient *, aClient *, char *, ...) __attribute__((format(printf,3,4)));
extern void sendto_ops_butme(aClient *, char *, ...) __attribute__((format(printf,2,3)));
extern void sendto_prefix_one(aClient *, aClient *, const char *, ...) __attribute__((format(printf,3,4)));
extern void sendto_failops_whoare_opers(char *, ...) __attribute__((format(printf,1,2)));
extern void sendto_failops(char *, ...) __attribute__((format(printf,1,2)));
extern void sendto_opers(char *, ...) __attribute__((format(printf,1,2)));
extern void sendto_umode(int, char *, ...) __attribute__((format(printf,2,3)));
extern void sendto_umode_raw(int, char *, ...) __attribute__((format(printf,2,3)));
extern void sendto_snomask(int snomask, char *pattern, ...) __attribute__((format(printf,2,3)));
extern void sendto_snomask_global(int snomask, char *pattern, ...) __attribute__((format(printf,2,3)));
extern void sendto_snomask_normal(int snomask, char *pattern, ...) __attribute__((format(printf,2,3)));
extern void sendto_snomask_normal_global(int snomask, char *pattern, ...) __attribute__((format(printf,2,3)));
extern void sendnotice(aClient *to, char *pattern, ...) __attribute__((format(printf,2,3)));
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
extern inline char *make_nick_user_host_r(char *namebuf, char *nick, char *name, char *host);
extern char *make_user_host(char *, char *);
extern int parse(aClient *, char *, char *);
extern int do_numeric(int, aClient *, aClient *, int, char **);
extern int hunt_server(aClient *, aClient *, char *, int, int, char **);
extern int hunt_server_token(aClient *, aClient *, char *, char *, char *, int, int, char **);
extern int hunt_server_token_quiet(aClient *, aClient *, char *, char *, char *, int, int, char **);
extern aClient *next_client(aClient *, char *);
extern int m_server_estab(aClient *);
extern void umode_init(void);
extern long umode_get(char, int, int (*)(aClient *, int));
#define UMODE_GLOBAL 1
#define UMODE_LOCAL 0
#define umode_lget(x) umode_get(x, 0, 0);
#define umode_gget(x) umode_get(x, 1, 0);
extern int umode_allow_all(aClient *sptr, int what);
extern int umode_allow_opers(aClient *sptr, int what);
extern int  umode_delete(char ch, long val);
extern void send_umode(aClient *, aClient *, long, long, char *);
extern void send_umode_out(aClient *, aClient *, long);

extern void free_client(aClient *);
extern void free_link(Link *);
extern void free_ban(Ban *);
extern void free_user(anUser *, aClient *);
extern int find_str_match_link(Link *, char *);
extern void free_str_list(Link *);
extern Link *make_link();
extern Ban *make_ban();
extern anUser *make_user(aClient *);
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
extern int add_to_channel_hash_table(char *, aChannel *);
extern int del_from_channel_hash_table(char *, aChannel *);
extern int add_to_watch_hash_table(char *, aClient *, int);
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
extern MODVAR long UMODE_INVISIBLE; /*  0x0001	 makes user invisible */
extern MODVAR long UMODE_OPER;      /*  0x0002	 Operator */
extern MODVAR long UMODE_WALLOP;    /*  0x0004	 send wallops to them */
extern MODVAR long UMODE_FAILOP;    /*  0x0008	 Shows some global messages */
extern MODVAR long UMODE_HELPOP;    /*  0x0010	 Help system operator */
extern MODVAR long UMODE_REGNICK;   /*  0x0020	 Nick set by services as registered */
extern MODVAR long UMODE_SADMIN;    /*  0x0040	 Services Admin */
extern MODVAR long UMODE_ADMIN;     /*  0x0080	 Admin */
extern MODVAR long UMODE_SERVNOTICE;/* 0x0100	 server notices such as kill */
extern MODVAR long UMODE_LOCOP;     /* 0x0200	 Local operator -- SRB */
extern MODVAR long UMODE_RGSTRONLY; /* 0x0400  Only reg nick message */
extern MODVAR long UMODE_WEBTV;     /* 0x0800  WebTV Client */
extern MODVAR long UMODE_SERVICES;  /* 0x4000	 services */
extern MODVAR long UMODE_HIDE;	     /* 0x8000	 Hide from Nukes */
extern MODVAR long UMODE_NETADMIN;  /* 0x10000	 Network Admin */
extern MODVAR long UMODE_COADMIN;   /* 0x80000	 Co Admin */
extern MODVAR long UMODE_WHOIS;     /* 0x100000	 gets notice on /whois */
extern MODVAR long UMODE_KIX;       /* 0x200000	 usermode +q */
extern MODVAR long UMODE_BOT;       /* 0x400000	 User is a bot */
extern MODVAR long UMODE_SECURE;    /*	0x800000	 User is a secure connect */
extern MODVAR long UMODE_VICTIM;    /* 0x8000000	 Intentional Victim */
extern MODVAR long UMODE_DEAF;      /* 0x10000000       Deaf */
extern MODVAR long UMODE_HIDEOPER;  /* 0x20000000	 Hide oper mode */
extern MODVAR long UMODE_SETHOST;   /* 0x40000000	 used sethost */
extern MODVAR long UMODE_STRIPBADWORDS; /* 0x80000000	 */
extern MODVAR long UMODE_HIDEWHOIS; /* hides channels in /whois */
extern MODVAR long UMODE_NOCTCP;    /* blocks all ctcp (except dcc and action) */
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

#ifdef EXTCMODE
/* Extended chanmodes... */
extern MODVAR Cmode_t EXTMODE_NONOTICE;
#ifdef STRIPBADWORDS
extern MODVAR Cmode_t EXTMODE_STRIPBADWORDS;
#endif
extern MODVAR Cmode_t EXTMODE_JOINTHROTTLE;
#endif

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
extern MODVAR aCommand *CommandHash[256];
extern MODVAR aCommand *TokenHash[256];
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
extern void      flag_add(char ch);
extern void      flag_del(char ch);
extern void init_dynconf(void);
extern char *pretty_time_val(long);
extern int        init_conf(char *filename, int rehash);
extern int global_test();
extern void       validate_configuration(void);
extern void       run_configuration(void);
extern void rehash_motdrules();
extern aMotd *read_file(char *filename, aMotd **list);
extern aMotd *read_file_ex(char *filename, aMotd **list, struct tm *);
extern CMD_FUNC(m_server_remote);
extern void send_proto(aClient *, ConfigItem_link *);
extern char *xbase64enc(long i);
extern void unload_all_modules(void);
#ifndef NEW_IO
extern void flush_fdlist_connections(fdlist * listp);
#else /* ifndef NEW_IO */
#endif /* ifndef NEW_IO */
extern void set_sock_opts(int fd, aClient *cptr);
extern void iCstrip(char *line);
extern time_t rfc2time(char *s);
extern char *rfctime(time_t t, char *buf);
extern void *MyMallocEx(size_t size);
#ifdef USE_SSL
extern MODFUNC char  *ssl_get_cipher(SSL *ssl);
#endif
extern long config_checkval(char *value, unsigned short flags);
extern void config_status(char *format, ...) __attribute__((format(printf,1,2)));
extern void init_random();
extern u_char getrandom8();
extern u_int16_t getrandom16();
extern u_int32_t getrandom32();
extern MODVAR char trouble_info[1024];
#define EVENT_DRUGS BASE_VERSION
extern void rejoin_doparts(aClient *sptr, char did_parts[]);
extern void rejoin_dojoinandmode(aClient *sptr, char did_parts[]);
extern void ident_failed(aClient *cptr);

extern MODVAR char extchmstr[4][64];
extern MODVAR char extbanstr[EXTBANTABLESZ+1];
#ifdef EXTCMODE
extern int extcmode_default_requirechop(aClient *, aChannel *, char *, int, int);
extern int extcmode_default_requirehalfop(aClient *, aChannel *, char *, int, int);
extern Cmode_t extcmode_get(Cmode *);
extern void extcmode_init(void);
extern void *extcmode_get_struct(aChannel *, char);
extern void make_extcmodestr();
extern void extcmode_duplicate_paramlist(void **xi, void **xo);
extern void extcmode_free_paramlist(void **ar);
#endif
extern void chmode_str(struct ChMode, char *, char *);
extern char *get_cptr_status(aClient *);
extern char *get_snostr(long);
#ifdef _WIN32
extern void InitDebug(void);
extern int InitwIRCD(int argc, char **);
#endif
extern void SocketLoop(void *);
#ifdef STATIC_LINKING
extern int l_commands_Init(ModuleInfo *);
extern int l_commands_Test(ModuleInfo *);
extern int l_commands_Load(int);
#endif
extern void sendto_chmodemucrap(aClient *, aChannel *, char *);
extern void verify_opercount(aClient *, char *);
extern int valid_host(char *host);
extern int count_oper_sessions(char *);
extern int file_exists(char* file);
extern char *unreal_mktemp(char *dir, char *suffix);
extern char *unreal_getpathname(char *filepath, char *path);
extern char *unreal_getfilename(char *path);
extern int unreal_copyfile(char *src, char *dest);
extern int unreal_copyfileex(char *src, char *dest, int tryhardlink);
extern time_t unreal_getfilemodtime(char *filename);
extern void unreal_setfilemodtime(char *filename, time_t mtime);
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
extern char *spamfilter_inttostring_long(int v);
extern aChannel *get_channel(aClient *cptr, char *chname, int flag);
extern MODVAR char backupbuf[];
extern void add_invite(aClient *, aChannel *);
extern void channel_modes(aClient *, char *, char *, aChannel *);
extern MODVAR char modebuf[BUFSIZE], parabuf[BUFSIZE];
extern int op_can_override(aClient *sptr);
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
extern MODVAR aMotd *rules;
#ifndef NEW_IO
extern MODVAR fdlist default_fdlist, busycli_fdlist, serv_fdlist, oper_fdlist;
#else /* ifndef NEW_IO */
#endif /* ifndef NEW_IO */
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
extern void sendto_fconnectnotice(char *nick, anUser *user, aClient *sptr, int disconnect, char *comment);
extern void sendto_one_nickcmd(aClient *cptr, aClient *sptr, char *umodes);
extern int on_dccallow_list(aClient *to, aClient *from);
extern int add_dccallow(aClient *sptr, aClient *optr);
extern int del_dccallow(aClient *sptr, aClient *optr);
extern void delete_linkblock(ConfigItem_link *link_ptr);
extern void delete_classblock(ConfigItem_class *class_ptr);
extern void del_async_connects(void);
extern void make_extbanstr(void);
extern void isupport_init(void);
extern int do_cmd(aClient *cptr, aClient *sptr, char *cmd, int parc, char *parv[]);
extern void create_snomask(aClient *sptr, anUser *user, char *snomask);
extern MODVAR char *me_hash;
extern MODVAR int dontspread;
/* Efuncs */
extern MODVAR int (*do_join)(aClient *, aClient *, int, char **);
extern MODVAR void (*join_channel)(aChannel *chptr, aClient *cptr, aClient *sptr, int flags);
extern MODVAR int (*can_join)(aClient *cptr, aClient *sptr, aChannel *chptr, char *key, char *link, char *parv[]);
extern MODVAR void (*do_mode)(aChannel *chptr, aClient *cptr, aClient *sptr, int parc, char *parv[], time_t sendts, int samode);
extern MODVAR void (*set_mode)(aChannel *chptr, aClient *cptr, int parc, char *parv[], u_int *pcount,
    char pvar[MAXMODEPARAMS][MODEBUFLEN + 3], int bounce);
extern MODVAR int (*m_umode)(aClient *, aClient *, int, char **);
extern MODVAR int (*register_user)(aClient *cptr, aClient *sptr, char *nick, char *username, char *umode, char *virthost, char *ip);
extern MODVAR int (*tkl_hash)(unsigned int c);
extern MODVAR char (*tkl_typetochar)(int type);
extern MODVAR aTKline *(*tkl_add_line)(int type, char *usermask, char *hostmask, char *reason, char *setby,
                  TS expire_at, TS set_at, TS spamf_tkl_duration, char *spamf_tkl_reason);
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
extern MODVAR void (*send_list)(aClient *cptr, int numsend);
extern MODVAR char *(*stripbadwords)(char *str, ConfigItem_badword *start_bw, int *blocked);
extern MODVAR unsigned char *(*StripColors)(unsigned char *text);
extern MODVAR const char *(*StripControlCodes)(unsigned char *text);
extern MODVAR void (*spamfilter_build_user_string)(char *buf, char *nick, aClient *acptr);
extern MODVAR int (*is_silenced)(aClient *sptr, aClient *acptr);
/* /Efuncs */
extern MODVAR aMotd *opermotd, *svsmotd, *motd, *botmotd, *smotd;
extern MODVAR int max_connection_count;
extern int add_listmode(Ban **list, aClient *cptr, aChannel *chptr, char *banid);
extern int del_listmode(Ban **list, aChannel *chptr, char *banid);
extern int Halfop_mode(long mode);
extern char *clean_ban_mask(char *, int, aClient *);
extern int find_invex(aChannel *chptr, aClient *sptr);
extern void DoMD5(unsigned char *mdout, unsigned char *src, unsigned long n);
#ifdef JOINTHROTTLE
aJFlood *cmodej_addentry(aClient *cptr, aChannel *chptr);
void cmodej_delentry(aJFlood *e);
void cmodej_deluserentries(aClient *cptr);
void cmodej_delchannelentries(aChannel *chptr);
#endif
extern void charsys_reset(void);
extern void charsys_addmultibyterange(char s1, char e1, char s2, char e2);
extern void charsys_addallowed(char *s);
extern void charsys_reset(void);
extern MODVAR char langsinuse[4096];
extern MODVAR aTKline *tklines[TKLISTLEN];
extern char *cmdname_by_spamftarget(int target);
extern int isipv6(struct IN_ADDR *addr);
extern void inet4_to_inet6(const void *src_in, void *dst_in);
extern void unrealdns_delreq_bycptr(aClient *cptr);
extern void inet6_to_inet4(const void *src, void *dst);
extern void sendtxtnumeric(aClient *to, char *pattern, ...) __attribute__((format(printf,2,3)));;
extern void unrealdns_gethostbyname_link(char *name, ConfigItem_link *conf);
extern void unrealdns_delasyncconnects(void);
extern int is_autojoin_chan(char *chname);
extern void unreal_free_hostent(struct hostent *he);
extern int match_esc(const char *mask, const char *name);
extern int iplist_onlist(IPList *iplist, char *ip);
extern struct hostent *unreal_create_hostent(char *name, struct IN_ADDR *addr);
extern char *unreal_time_synch_error(void);
extern int unreal_time_synch(int timeout);
extern int extban_is_banned_helper(char *buf);
extern char *getcloak(aClient *sptr);
extern unsigned char param_to_slot_mapping[256];
extern char *cm_getparameter(aChannel *chptr, char mode);
extern void cm_putparameter(aChannel *chptr, char mode, char *str);
extern void cm_freeparameter(aChannel *chptr, char mode);
extern char *cm_getparameter_ex(void **p, char mode);
extern void cm_putparameter_ex(void **p, char mode, char *str);
extern void cm_freeparameter_ex(void **p, char mode, char *str);
extern void kick_insecure_users(aChannel *);
extern void free_motd(aMotd *m);
