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
extern MODVAR Client me;
extern MODVAR Channel *channels;
extern MODVAR ModData local_variable_moddata[MODDATA_MAX_LOCAL_VARIABLE];
extern MODVAR ModData global_variable_moddata[MODDATA_MAX_GLOBAL_VARIABLE];
extern MODVAR IRCStatistics ircstats;
extern MODVAR int bootopt;
extern MODVAR time_t timeofday;
extern MODVAR struct timeval timeofday_tv;
extern MODVAR char cmodestring[512];
extern MODVAR char umodestring[UMODETABLESZ+1];
/* newconf */
#define get_sendq(x) ((x)->local->class ? (x)->local->class->sendq : DEFAULT_SENDQ)
/* get_recvq is only called in send.c for local connections */
#define get_recvq(x) ((x)->local->class->recvq ? (x)->local->class->recvq : DEFAULT_RECVQ)

/* Configuration preprocessor */
extern PreprocessorItem parse_preprocessor_item(char *start, char *end, char *filename, int linenumber, ConditionalConfig **cc);
extern void preprocessor_cc_duplicate_list(ConditionalConfig *r, ConditionalConfig **out);
extern void preprocessor_cc_free_level(ConditionalConfig **cc_list, int level);
extern void preprocessor_cc_free_list(ConditionalConfig *cc);
extern void preprocessor_resolve_conditionals_ce(ConfigEntry **ce_list, PreprocessorPhase phase);
extern void preprocessor_resolve_conditionals_all(PreprocessorPhase phase);
extern void free_config_defines(void);
extern void preprocessor_replace_defines(char **item, ConfigEntry *ce);

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
extern MODVAR ConfigItem_sni		*conf_sni;
extern MODVAR ConfigItem_ban		*conf_ban;
extern MODVAR ConfigItem_deny_channel  *conf_deny_channel;
extern MODVAR ConfigItem_deny_link	*conf_deny_link;
extern MODVAR ConfigItem_allow_channel *conf_allow_channel;
extern MODVAR ConfigItem_deny_version	*conf_deny_version;
extern MODVAR ConfigItem_log		*conf_log;
extern MODVAR ConfigItem_alias		*conf_alias;
extern MODVAR ConfigItem_include	*conf_include;
extern MODVAR ConfigItem_help		*conf_help;
extern MODVAR ConfigItem_offchans	*conf_offchans;
extern MODVAR SecurityGroup		*securitygroups;
extern void		completed_connection(int, int, void *);
extern void clear_unknown();
extern EVENT(e_unload_module_delayed);
extern EVENT(throttling_check_expire);

extern void  module_loadall(void);
extern long set_usermode(char *umode);
extern char *get_usermode_string_raw(long umodes);
extern ConfigFile *config_parse(char *filename, char *confdata);
extern ConfigFile *config_parse_with_offset(char *filename, char *confdata, unsigned int line_offset);
extern void config_error(FORMAT_STRING(const char *format), ...) __attribute__((format(printf,1,2)));
extern void config_warn(FORMAT_STRING(const char *format), ...) __attribute__((format(printf,1,2)));
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
extern void config_entry_free(ConfigEntry *ce);
extern void config_entry_free_all(ConfigEntry *ce);
extern ConfigFile *config_load(char *filename, char *displayname);
extern void config_free(ConfigFile *cfptr);
extern void ipport_seperate(char *string, char **ip, char **port);
extern ConfigItem_class	*find_class(char *name);
extern ConfigItem_deny_dcc	*find_deny_dcc(char *name);
extern ConfigItem_oper		*find_oper(char *name);
extern ConfigItem_operclass	*find_operclass(char *name);
extern ConfigItem_listen *find_listen(char *ipmask, int port, int ipv6);
extern ConfigItem_sni *find_sni(char *name);
extern ConfigItem_ulines	*find_uline(char *host);
extern ConfigItem_except	*find_except(Client *, short type);
extern ConfigItem_tld		*find_tld(Client *cptr);
extern ConfigItem_link		*find_link(char *servername, Client *acptr);
extern ConfigItem_ban 		*find_ban(Client *, char *host, short type);
extern ConfigItem_ban 		*find_banEx(Client *,char *host, short type, short type2);
extern ConfigItem_vhost	*find_vhost(char *name);
extern ConfigItem_deny_channel *find_channel_allowed(Client *cptr, char *name);
extern ConfigItem_alias	*find_alias(char *name);
extern ConfigItem_help 	*find_Help(char *command);

extern OperPermission ValidatePermissionsForPath(char *path, Client *client, Client *victim, Channel *channel, void *extra);
extern void OperClassValidatorDel(OperClassValidator *validator);

extern ConfigItem_ban  *find_ban_ip(Client *client);
extern void add_ListItem(ListStruct *, ListStruct **);
extern void append_ListItem(ListStruct *item, ListStruct **list);
extern void add_ListItemPrio(ListStructPrio *, ListStructPrio **, int);
extern void del_ListItem(ListStruct *, ListStruct **);
extern MODVAR LoopStruct loop;
extern int del_banid(Channel *channel, char *banid);
extern int del_exbanid(Channel *channel, char *banid);
#define REPORT_DO_DNS	"NOTICE * :*** Looking up your hostname...\r\n"
#define REPORT_FIN_DNS	"NOTICE * :*** Found your hostname\r\n"
#define REPORT_FIN_DNSC "NOTICE * :*** Found your hostname (cached)\r\n"
#define REPORT_FAIL_DNS "NOTICE * :*** Couldn't resolve your hostname; using your IP address instead\r\n"
#define REPORT_DO_ID	"NOTICE * :*** Checking ident...\r\n"
#define REPORT_FIN_ID	"NOTICE * :*** Received identd response\r\n"
#define REPORT_FAIL_ID	"NOTICE * :*** No ident response; username prefixed with ~\r\n"
extern MODVAR int R_do_dns, R_fin_dns, R_fin_dnsc, R_fail_dns, R_do_id, R_fin_id, R_fail_id;
extern MODVAR struct list_head client_list;
extern MODVAR struct list_head lclient_list;
extern MODVAR struct list_head server_list;
extern MODVAR struct list_head oper_list;
extern MODVAR struct list_head unknown_list;
extern MODVAR struct list_head global_server_list;
extern MODVAR struct list_head dead_list;
extern RealCommand *find_command(char *cmd, int flags);
extern RealCommand *find_command_simple(char *cmd);
extern Membership *find_membership_link(Membership *lp, Channel *ptr);
extern Member *find_member_link(Member *, Client *);
extern int remove_user_from_channel(Client *, Channel *);
extern void add_server_to_table(Client *);
extern void remove_server_from_table(Client *);
extern void iNAH_host(Client *client, char *host);
extern void set_snomask(Client *client, char *snomask);
extern char *get_snomask_string(Client *client);
extern int check_tkls(Client *cptr);
/* for services */
extern void send_user_joins(Client *, Client *);
extern int valid_channelname(const char *);
extern int valid_server_name(char *name);
extern long get_access(Client *, Channel *);
extern int ban_check_mask(Client *, Channel *, char *, int, char **, char **, int);
extern int extban_is_ok_nuh_extban(Client *, Channel *, char *, int, int, int);
extern char *extban_conv_param_nuh_or_extban(char *);
extern char *extban_conv_param_nuh(char *);
extern Ban *is_banned(Client *, Channel *, int, char **, char **);
extern Ban *is_banned_with_nick(Client *, Channel *, int, char *, char **, char **);

extern void ircd_log(int, FORMAT_STRING(const char *), ...) __attribute__((format(printf,2,3)));
extern Client *find_client(char *, Client *);
extern Client *find_name(char *, Client *);
extern Client *find_nickserv(char *, Client *);
extern Client *find_person(char *, Client *);
extern Client *find_server(char *, Client *);
extern Client *find_service(char *, Client *);
#define find_server_quick(x) find_server(x, NULL)
extern char *find_or_add(char *);
extern void inittoken();
extern void reset_help();

extern MODVAR char *debugmode, *configfile, *sbrk0;
extern char *getfield(char *);
extern void set_sockhost(Client *, char *);
#ifdef _WIN32
extern MODFUNC char *sock_strerror(int);
#endif
extern int dgets(int, char *, int);

#ifdef _WIN32
extern MODVAR int debuglevel;
#else
extern int debuglevel, errno, h_errno;
#endif
extern MODVAR int OpenFiles;  /* number of files currently open */
extern MODVAR int debuglevel, portnum, debugtty, maxusersperchannel;
extern MODVAR int readcalls, udpfd, resfd;
extern Client *add_connection(ConfigItem_listen *, int);
extern void add_local_domain(char *, int);
extern int check_server_init(Client *);
extern void close_connection(Client *);
extern void close_unbound_listeners();
extern int connect_server(ConfigItem_link *, Client *, struct hostent *);
extern void get_my_name(Client *, char *, int);
extern int get_sockerr(Client *);
extern int inetport(ConfigItem_listen *, char *, int, int);
extern void init_sys();
extern void check_user_limit(void);
extern void init_modef();
extern int verify_hostname(char *name);

extern void report_error(char *, Client *);
extern int setup_ping();

extern void set_channel_mlock(Client *, Channel *, const char *, int);

extern void restart(char *);
extern void server_reboot(char *);
extern void terminate(), write_pidfile();
extern void *safe_alloc(size_t size);
extern void set_socket_buffers(int fd, int rcvbuf, int sndbuf);
extern int send_queued(Client *);
extern void send_queued_cb(int fd, int revents, void *data);
extern void sendto_connectnotice(Client *client, int disconnect, char *comment);
extern void sendto_serv_butone_nickcmd(Client *one, Client *client, char *umodes);
extern void    sendto_message_one(Client *to, Client *from, char *sender,
    char *cmd, char *nick, char *msg);
#define PREFIX_ALL		0
#define PREFIX_HALFOP	0x1
#define PREFIX_VOICE	0x2
#define PREFIX_OP	0x4
#define PREFIX_ADMIN	0x08
#define PREFIX_OWNER	0x10
extern void sendto_channel(Channel *channel, Client *from, Client *skip,
                           int prefix, long clicap, int sendflags,
                           MessageTag *mtags,
                           FORMAT_STRING(const char *pattern), ...) __attribute__((format(printf,8,9)));
extern void sendto_local_common_channels(Client *user, Client *skip,
                                         long clicap, MessageTag *mtags,
                                         FORMAT_STRING(const char *pattern), ...) __attribute__((format(printf,5,6)));
extern void sendto_match_servs(Channel *, Client *, FORMAT_STRING(const char *), ...) __attribute__((format(printf,3,4)));
extern void sendto_match_butone(Client *, Client *, char *, int, MessageTag *,
    FORMAT_STRING(const char *pattern), ...) __attribute__((format(printf,6,7)));
extern void sendto_all_butone(Client *, Client *, FORMAT_STRING(const char *), ...) __attribute__((format(printf,3,4)));
extern void sendto_ops(FORMAT_STRING(const char *), ...) __attribute__((format(printf,1,2)));
extern void sendto_ops_butone(Client *, Client *, FORMAT_STRING(const char *), ...) __attribute__((format(printf,3,4)));
extern void sendto_prefix_one(Client *, Client *, MessageTag *, FORMAT_STRING(const char *), ...) __attribute__((format(printf,4,5)));
extern void sendto_opers(FORMAT_STRING(const char *), ...) __attribute__((format(printf,1,2)));
extern void sendto_umode(int, FORMAT_STRING(const char *), ...) __attribute__((format(printf,2,3)));
extern void sendto_umode_global(int, FORMAT_STRING(const char *), ...) __attribute__((format(printf,2,3)));
extern void sendto_snomask(int snomask, FORMAT_STRING(const char *pattern), ...) __attribute__((format(printf,2,3)));
extern void sendto_snomask_global(int snomask, FORMAT_STRING(const char *pattern), ...) __attribute__((format(printf,2,3)));
extern void sendnotice(Client *to, FORMAT_STRING(const char *pattern), ...) __attribute__((format(printf,2,3)));
extern void sendnumeric(Client *to, int numeric, ...);
extern void sendnumericfmt(Client *to, int numeric, FORMAT_STRING(const char *pattern), ...) __attribute__((format(printf,3,4)));
extern void sendto_server(Client *one, unsigned long caps, unsigned long nocaps, MessageTag *mtags, FORMAT_STRING(const char *format), ...) __attribute__((format(printf, 5, 6)));
extern void sendto_ops_and_log(FORMAT_STRING(const char *pattern), ...) __attribute__((format(printf,1,2)));

extern MODVAR int writecalls, writeb[];
extern int deliver_it(Client *cptr, char *str, int len, int *want_read);
extern int target_limit_exceeded(Client *client, void *target, const char *name);
extern char *canonize(char *buffer);
extern int check_registered(Client *);
extern int check_registered_user(Client *);
extern char *get_client_name(Client *, int);
extern char *get_client_host(Client *);
extern char *myctime(time_t);
extern char *short_date(time_t, char *buf);
extern char *long_date(time_t);
extern void exit_client(Client *client, MessageTag *recv_mtags, char *comment);
extern void exit_client_ex(Client *client, Client *origin, MessageTag *recv_mtags, char *comment);
extern void initstats(), tstats(Client *, char *);
extern char *check_string(char *);
extern char *make_nick_user_host(char *, char *, char *);
extern char *make_nick_user_host_r(char *namebuf, char *nick, char *name, char *host);
extern char *make_user_host(char *, char *);
extern void parse(Client *cptr, char *buffer, int length);
extern int hunt_server(Client *, MessageTag *, char *, int, int, char **);
extern int cmd_server_estab(Client *);
extern void umode_init(void);
#define UMODE_GLOBAL 1
#define UMODE_LOCAL 0
extern int umode_allow_all(Client *client, int what);
extern int umode_allow_unset(Client *client, int what);
extern int umode_allow_opers(Client *client, int what);
extern int umode_allow_none(Client *client, int what);
extern int umode_delete(char ch, long val);
extern void build_umode_string(Client *client, long old, long sendmask, char *umode_buf);
extern void send_umode_out(Client *client, int show_to_user, long old);

extern void free_client(Client *);
extern void free_link(Link *);
extern void free_ban(Ban *);
extern void free_user(Client *);
extern int find_str_match_link(Link *, char *);
extern void free_str_list(Link *);
extern Link *make_link();
extern Ban *make_ban();
extern User *make_user(Client *);
extern Server *make_server();
extern Client *make_client(Client *, Client *);
extern Member *find_channel_link(Member *, Channel *);
extern char *pretty_mask(char *);
extern void add_client_to_list(Client *);
extern void remove_client_from_list(Client *);
extern void initlists();
extern struct hostent *get_res(char *);
extern struct hostent *gethost_byaddr(char *, Link *);
extern struct hostent *gethost_byname(char *, Link *);
extern void flush_cache();
extern void init_resolver(int firsttime);
extern time_t timeout_query_list(time_t);
extern time_t expire_cache(time_t);
extern void del_queries(char *);

/* Hash stuff */
#define NICK_HASH_TABLE_SIZE 32768
#define CHAN_HASH_TABLE_SIZE 32768
#define WATCH_HASH_TABLE_SIZE 32768
#define WHOWAS_HASH_TABLE_SIZE 32768
#define THROTTLING_HASH_TABLE_SIZE 8192
#define hash_find_channel find_channel
extern uint64_t siphash(const char *in, const char *k);
extern uint64_t siphash_raw(const char *in, size_t len, const char *k);
extern uint64_t siphash_nocase(const char *in, const char *k);
extern void siphash_generate_key(char *k);
extern void init_hash(void);
uint64_t hash_whowas_name(const char *name);
extern int add_to_client_hash_table(char *, Client *);
extern int del_from_client_hash_table(char *, Client *);
extern int add_to_id_hash_table(char *, Client *);
extern int del_from_id_hash_table(char *, Client *);
extern int add_to_channel_hash_table(char *, Channel *);
extern void del_from_channel_hash_table(char *, Channel *);
extern int add_to_watch_hash_table(char *, Client *, int);
extern int del_from_watch_hash_table(char *, Client *);
extern int hash_check_watch(Client *, int);
extern int hash_del_watch_list(Client *);
extern void count_watch_memory(int *, u_long *);
extern Watch *hash_get_watch(char *);
extern Channel *hash_get_chan_bucket(uint64_t);
extern Client *hash_find_client(const char *, Client *);
extern Client *hash_find_id(const char *, Client *);
extern Client *hash_find_nickatserver(const char *, Client *);
extern Channel *find_channel(char *name, Channel *channel);
extern Client *hash_find_server(const char *, Client *);
extern struct MODVAR ThrottlingBucket *ThrottlingHash[THROTTLING_HASH_TABLE_SIZE];

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
extern size_t strlcpy(char *dst, const char *src, size_t size);
#endif
#ifndef HAVE_STRLCAT
extern size_t strlcat(char *dst, const char *src, size_t size);
#endif
#ifndef HAVE_STRLNCAT
extern size_t strlncat(char *dst, const char *src, size_t size, size_t n);
#endif
extern char *strldup(const char *src, size_t n);

extern void dopacket(Client *, char *, int);

extern void debug(int, FORMAT_STRING(const char *), ...) __attribute__((format(printf,2,3)));
#if defined(DEBUGMODE)
extern void send_usage(Client *, char *);
extern void count_memory(Client *, char *);
extern int checkprotoflags(Client *, int, char *, int);
#endif

extern char *inetntop(int af, const void *in, char *local_dummy, size_t the_size);

/* Internal command stuff - not for modules */
extern MODVAR RealCommand *CommandHash[256];
extern void init_CommandHash(void);

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

extern AuthenticationType Auth_FindType(char *hash, char *type);
extern AuthConfig	*AuthBlockToAuthConfig(ConfigEntry *ce);
extern void		Auth_FreeAuthConfig(AuthConfig *as);
extern int		Auth_Check(Client *cptr, AuthConfig *as, char *para);
extern char   		*Auth_Hash(int type, char *para);
extern int   		Auth_CheckError(ConfigEntry *ce);
extern int              Auth_AutoDetectHashType(char *hash);

extern void make_cloakedhost(Client *client, char *curr, char *buf, size_t buflen);
extern int  channel_canjoin(Client *client, char *name);
extern char *collapse(char *pattern);
extern void dcc_sync(Client *client);
extern void report_flines(Client *client);
extern void report_network(Client *client);
extern void report_dynconf(Client *client);
extern void count_memory(Client *cptr, char *nick);
extern void list_scache(Client *client);
extern char *oflagstr(long oflag);
extern int rehash(Client *client, int sig);
extern void s_die();
extern int match_simple(const char *mask, const char *name);
extern int match_esc(const char *mask, const char *name);
extern int add_listener(ConfigItem_listen *conf);
extern void link_cleanup(ConfigItem_link *link_ptr);
extern void       listen_cleanup();
extern int  numeric_collides(long numeric);
extern u_long cres_mem(Client *client, char *nick);
extern void      flag_add(char ch);
extern void      flag_del(char ch);
extern void init_dynconf(void);
extern char *pretty_time_val(long);
extern char *pretty_date(time_t t);
extern int        init_conf(char *filename, int rehash);
extern void       validate_configuration(void);
extern void       run_configuration(void);
extern void rehash_motdrules();
extern void read_motd(const char *filename, MOTDFile *motd); /* s_serv.c */
extern void send_proto(Client *, ConfigItem_link *);
extern void unload_all_modules(void);
extern void set_sock_opts(int fd, Client *cptr, int ipv6);
extern void stripcrlf(char *line);
extern time_t rfc2time(char *s);
extern char *rfctime(time_t t, char *buf);
extern int strnatcmp(char const *a, char const *b);
extern int strnatcasecmp(char const *a, char const *b);
extern void outofmemory(size_t bytes);

/** Memory allocation and deallocation functions and macros that should be used in UnrealIRCd.
 * Use these instead of malloc/calloc/free.
 * @defgroup MemoryRoutines Memory allocation and deallocation
 * @{
 */
extern void *safe_alloc(size_t size);
/** Free previously allocate memory pointer.
 * This also sets the pointer to NULL, since that would otherwise be common to forget.
 */
#define safe_free(x) do { if (x) free(x); x = NULL; } while(0)
/** Free previously allocated memory pointer.
 * Raw version which does not touch the pointer itself. You most likely don't
 * need this, as it's only used in 1 place in UnrealIRCd.
 */
#define safe_free_raw(x) free(x)

/** Free previous memory (if any) and then save a duplicate of the specified string.
 * @param dst   The current pointer and the pointer where a new copy of the string will be stored.
 * @param str   The string you want to copy
 */
#define safe_strdup(dst,str) do { if (dst) free(dst); if (!(str)) dst = NULL; else dst = our_strdup(str); } while(0)

/** Return a copy of the string. Do not free any existing memory.
 * @param str   The string to duplicate
 * @returns A pointer to the new copy.
 * @note
 * Generally you need to use safe_strdup() instead(!). But when clearly initializing
 * a variable that does not have a previous value, then raw_strdup() usage is fine, eg:
 * int somefunc()
 * {
 *     char *somevar = raw_strdup("IRC");
 * And, similarly if you want to return a duplicate (there is no destination variable):
 * return raw_strdup(something);
 */
#define raw_strdup(str) strdup(str)

/** Free previous memory (if any) and then save a duplicate of the specified string with a length limit.
 * @param dst   The current pointer and the pointer where a new copy of the string will be stored.
 * @param str   The string you want to copy
 * @param sz    Length limit including the NUL byte, usually sizeof(dst)
 */
#define safe_strldup(dst,str,sz) do { if (dst) free(dst); if (!str) dst = NULL; else dst = our_strldup(str,sz); } while(0)

/** Return a duplicate of the specified string with a length limit. Do not free any existing memory.
 * @param str   The string you want to copy
 * @param sz    Length limit including the NUL byte, usually sizeof(dst)
 * @returns A pointer to the new copy.
 * @note
 * Generally you need to use safe_strldup() instead(!). But when clearly initializing
 * a variable that does not have a previous value, then raw_strldup() usage is fine, eg:
 * int somefunc(char *str)
 * {
 *     char *somevar = raw_strldup(str, 16);
 * And, similarly if you want to return a duplicate (there is no destination variable):
 * return raw_strldup(something);
 */
#define raw_strldup(str, max) our_strldup(str, max)

extern void *safe_alloc_sensitive(size_t size);

/** Free previously allocate memory pointer - this is the sensitive version which
 * may ONLY be called on allocations returned by safe_alloc_sensitive() / safe_strdup_sensitive().
 * This will set the memory to all zeroes before actually deallocating.
 * It also sets the pointer to NULL, since that would otherwise be common to forget.
 * @note If you call this function on normally allocated memory (non-sensitive) then we will crash.
 */
#define safe_free_sensitive(x) do { if (x) sodium_free(x); x = NULL; } while(0)

/** Free previous memory (if any) and then save a duplicate of the specified string -
 * This is the 'sensitive' version which should only be used for HIGHLY sensitive data,
 * as it wastes about 8000 bytes even if you only duplicate a string of 32 bytes (this is by design).
 * @param dst   The current pointer and the pointer where a new copy of the string will be stored.
 * @param str   The string you want to copy
 */
#define safe_strdup_sensitive(dst,str) do { if (dst) sodium_free(dst); if (!(str)) dst = NULL; else dst = our_strdup_sensitive(str); } while(0)

/** Safely destroy a string in memory (but do not free!) */
#define destroy_string(str) sodium_memzero(str, strlen(str))

/** @} */
extern char *our_strdup(const char *str);
extern char *our_strldup(const char *str, size_t max);
extern char *our_strdup_sensitive(const char *str);

extern long config_checkval(char *value, unsigned short flags);
extern void config_status(FORMAT_STRING(const char *format), ...) __attribute__((format(printf,1,2)));
extern void init_random();
extern u_char getrandom8();
extern uint16_t getrandom16();
extern uint32_t getrandom32();
extern void gen_random_alnum(char *buf, int numbytes);

extern MODVAR char extchmstr[4][64];

extern int extcmode_default_requirechop(Client *, Channel *, char, char *, int, int);
extern int extcmode_default_requirehalfop(Client *, Channel *, char, char *, int, int);
extern Cmode_t extcmode_get(Cmode *);
extern void extcmode_init(void);
extern void make_extcmodestr();
extern void extcmode_duplicate_paramlist(void **xi, void **xo);
extern void extcmode_free_paramlist(void **ar);

extern void chmode_str(struct ChMode *, char *, char *, size_t, size_t);
extern char *get_client_status(Client *);
extern char *get_snomask_string_raw(long);
extern void SocketLoop(void *);
#ifdef _WIN32
extern void InitDebug(void);
extern int InitUnrealIRCd(int argc, char **);
extern void win_error();
extern void win_log(FORMAT_STRING(const char *format), ...);
extern int GetOSName(char *pszOS);
extern void CleanUp(void);
extern int CountRTFSize(unsigned char *buffer);
extern void IRCToRTF(unsigned char *buffer, unsigned char *string);
#endif
extern void sendto_chmodemucrap(Client *, Channel *, char *);
extern void verify_opercount(Client *, char *);
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
extern BanAction banact_stringtoval(char *s);
extern char *banact_valtostring(BanAction val);
extern BanAction banact_chartoval(char c);
extern char banact_valtochar(BanAction val);
extern int spamfilter_gettargets(char *s, Client *client);
extern char *spamfilter_target_inttostring(int v);
extern Spamfilter *unreal_buildspamfilter(char *s);
extern char *our_strcasestr(char *haystack, char *needle);
extern int spamfilter_getconftargets(char *s);
extern void remove_oper_snomasks(Client *client);
extern void remove_oper_modes(Client *client);
extern char *spamfilter_inttostring_long(int v);
extern Channel *get_channel(Client *cptr, char *chname, int flag);
extern MODVAR char backupbuf[];
extern void add_invite(Client *, Client *, Channel *, MessageTag *);
extern void del_invite(Client *, Channel *);
extern int is_invited(Client *client, Channel *channel);
extern void channel_modes(Client *client, char *mbuf, char *pbuf, size_t mbuf_size, size_t pbuf_size, Channel *channel, int hide_local_modes);
extern MODVAR char modebuf[BUFSIZE], parabuf[BUFSIZE];
extern int op_can_override(char *acl, Client *client,Channel *channel,void* extra);
extern Client *find_chasing(Client *client, char *user, int *chasing);
extern MODVAR long opermode;
extern MODVAR long sajoinmode;
extern void add_user_to_channel(Channel *channel, Client *who, int flags);
extern int add_banid(Client *, Channel *, char *);
extern int add_exbanid(Client *cptr, Channel *channel, char *banid);
extern int sub1_from_channel(Channel *);
extern MODVAR CoreChannelModeTable corechannelmodetable[];
extern char *unreal_encodespace(char *s);
extern char *unreal_decodespace(char *s);
extern MODVAR Link *helpign;
extern void reread_motdsandrules();
extern MODVAR int SVSNOOP;
extern int callbacks_check(void);
extern void callbacks_switchover(void);
extern int efunctions_check(void);
extern void efunctions_switchover(void);
extern char *encode_ip(char *);
extern char *decode_ip(char *);
extern void sendto_fconnectnotice(Client *client, int disconnect, char *comment);
extern void sendto_one_nickcmd(Client *server, Client *client, char *umodes);
extern int on_dccallow_list(Client *to, Client *from);
extern int add_dccallow(Client *client, Client *optr);
extern int del_dccallow(Client *client, Client *optr);
extern void delete_linkblock(ConfigItem_link *link_ptr);
extern void delete_classblock(ConfigItem_class *class_ptr);
extern void del_async_connects(void);
extern void isupport_init(void);
extern void clicap_init(void);
extern void efunctions_init(void);
extern void do_cmd(Client *client, MessageTag *mtags, char *cmd, int parc, char *parv[]);
extern MODVAR char *me_hash;
extern MODVAR int dontspread;
extern MODVAR int labeled_response_inhibit;
extern MODVAR int labeled_response_inhibit_end;
extern MODVAR int labeled_response_force;

/* Efuncs */
extern MODVAR void (*do_join)(Client *, int, char **);
extern MODVAR void (*join_channel)(Channel *channel, Client *client, MessageTag *mtags, int flags);
extern MODVAR int (*can_join)(Client *client, Channel *channel, char *key, char *parv[]);
extern MODVAR void (*do_mode)(Channel *channel, Client *client, MessageTag *mtags, int parc, char *parv[], time_t sendts, int samode);
extern MODVAR void (*set_mode)(Channel *channel, Client *cptr, int parc, char *parv[], u_int *pcount,
    char pvar[MAXMODEPARAMS][MODEBUFLEN + 3], int bounce);
extern MODVAR void (*cmd_umode)(Client *, MessageTag *, int, char **);
extern MODVAR int (*register_user)(Client *client, char *nick, char *username, char *umode, char *virthost, char *ip);
extern MODVAR int (*tkl_hash)(unsigned int c);
extern MODVAR char (*tkl_typetochar)(int type);
extern MODVAR int (*tkl_chartotype)(char c);
extern MODVAR char *(*tkl_type_string)(TKL *tk);
extern MODVAR TKL *(*tkl_add_serverban)(int type, char *usermask, char *hostmask, char *reason, char *setby,
                                            time_t expire_at, time_t set_at, int soft, int flags);
extern MODVAR TKL *(*tkl_add_banexception)(int type, char *usermask, char *hostmask, char *reason, char *set_by,
                                               time_t expire_at, time_t set_at, int soft, char *bantypes, int flags);
extern MODVAR TKL *(*tkl_add_nameban)(int type, char *name, int hold, char *reason, char *setby,
                                          time_t expire_at, time_t set_at, int flags);
extern MODVAR TKL *(*tkl_add_spamfilter)(int type, unsigned short target, unsigned short action, Match *match, char *setby,
                                             time_t expire_at, time_t set_at,
                                             time_t spamf_tkl_duration, char *spamf_tkl_reason,
                                             int flags);
extern MODVAR TKL *(*find_tkl_serverban)(int type, char *usermask, char *hostmask, int softban);
extern MODVAR TKL *(*find_tkl_banexception)(int type, char *usermask, char *hostmask, int softban);
extern MODVAR TKL *(*find_tkl_nameban)(int type, char *name, int hold);
extern MODVAR TKL *(*find_tkl_spamfilter)(int type, char *match_string, unsigned short action, unsigned short target);
extern MODVAR void (*sendnotice_tkl_del)(char *removed_by, TKL *tkl);
extern MODVAR void (*sendnotice_tkl_add)(TKL *tkl);
extern MODVAR void (*free_tkl)(TKL *tkl);
extern MODVAR TKL *(*tkl_del_line)(TKL *tkl);
extern MODVAR void (*tkl_check_local_remove_shun)(TKL *tmp);
extern MODVAR int (*find_tkline_match)(Client *cptr, int skip_soft);
extern MODVAR int (*find_shun)(Client *cptr);
extern MODVAR int (*find_spamfilter_user)(Client *client, int flags);
extern MODVAR TKL *(*find_qline)(Client *cptr, char *nick, int *ishold);
extern MODVAR TKL *(*find_tkline_match_zap)(Client *cptr);
extern MODVAR void (*tkl_stats)(Client *cptr, int type, char *para, int *cnt);
extern MODVAR void (*tkl_sync)(Client *client);
extern MODVAR void (*cmd_tkl)(Client *client, MessageTag *recv_mtags, int parc, char *parv[]);
extern MODVAR int (*place_host_ban)(Client *client, BanAction action, char *reason, long duration);
extern MODVAR int (*match_spamfilter)(Client *client, char *str_in, int type, char *cmd, char *target, int flags, TKL **rettk);
extern MODVAR int (*match_spamfilter_mtags)(Client *client, MessageTag *mtags, char *cmd);
extern MODVAR int (*join_viruschan)(Client *client, TKL *tk, int type);
extern MODVAR unsigned char *(*StripColors)(unsigned char *text);
extern MODVAR const char *(*StripControlCodes)(unsigned char *text);
extern MODVAR void (*spamfilter_build_user_string)(char *buf, char *nick, Client *acptr);
extern MODVAR void (*send_protoctl_servers)(Client *client, int response);
extern MODVAR int (*verify_link)(Client *client, char *servername, ConfigItem_link **link_out);
extern MODVAR void (*send_server_message)(Client *client);
extern MODVAR void (*broadcast_md_client)(ModDataInfo *mdi, Client *acptr, ModData *md);
extern MODVAR void (*broadcast_md_channel)(ModDataInfo *mdi, Channel *channel, ModData *md);
extern MODVAR void (*broadcast_md_member)(ModDataInfo *mdi, Channel *channel, Member *m, ModData *md);
extern MODVAR void (*broadcast_md_membership)(ModDataInfo *mdi, Client *acptr, Membership *m, ModData *md);
extern MODVAR void (*broadcast_md_client_cmd)(Client *except, Client *sender, Client *acptr, char *varname, char *value);
extern MODVAR void (*broadcast_md_channel_cmd)(Client *except, Client *sender, Channel *channel, char *varname, char *value);
extern MODVAR void (*broadcast_md_member_cmd)(Client *except, Client *sender, Channel *channel, Client *acptr, char *varname, char *value);
extern MODVAR void (*broadcast_md_membership_cmd)(Client *except, Client *sender, Client *acptr, Channel *channel, char *varname, char *value);
extern MODVAR void (*send_moddata_client)(Client *srv, Client *acptr);
extern MODVAR void (*send_moddata_channel)(Client *srv, Channel *channel);
extern MODVAR void (*send_moddata_members)(Client *srv);
extern MODVAR void (*broadcast_moddata_client)(Client *acptr);
extern MODVAR int (*check_banned)(Client *cptr, int exitflags);
extern MODVAR void (*introduce_user)(Client *to, Client *acptr);
extern MODVAR int (*check_deny_version)(Client *cptr, char *software, int protocol, char *flags);
extern MODVAR int (*match_user)(char *rmask, Client *acptr, int options);
extern MODVAR void (*userhost_save_current)(Client *client);
extern MODVAR void (*userhost_changed)(Client *client);
extern MODVAR void (*send_join_to_local_users)(Client *client, Channel *channel, MessageTag *mtags);
extern MODVAR int (*do_nick_name)(char *nick);
extern MODVAR int (*do_remote_nick_name)(char *nick);
extern MODVAR char *(*charsys_get_current_languages)(void);
extern MODVAR void (*broadcast_sinfo)(Client *acptr, Client *to, Client *except);
extern MODVAR void (*parse_message_tags)(Client *cptr, char **str, MessageTag **mtag_list);
extern MODVAR char *(*mtags_to_string)(MessageTag *m, Client *acptr);
extern MODVAR int (*can_send_to_channel)(Client *cptr, Channel *channel, char **msgtext, char **errmsg, int notice);
extern MODVAR void (*broadcast_md_globalvar)(ModDataInfo *mdi, ModData *md);
extern MODVAR void (*broadcast_md_globalvar_cmd)(Client *except, Client *sender, char *varname, char *value);
extern MODVAR int (*tkl_ip_hash)(char *ip);
extern MODVAR int (*tkl_ip_hash_type)(int type);
extern MODVAR int (*find_tkl_exception)(int ban_type, Client *cptr);
extern MODVAR int (*del_silence)(Client *client, const char *mask);
extern MODVAR int (*add_silence)(Client *client, const char *mask, int senderr);
extern MODVAR int (*is_silenced)(Client *client, Client *acptr);
extern MODVAR void *(*labeled_response_save_context)(void);
extern MODVAR void (*labeled_response_set_context)(void *ctx);
extern MODVAR void (*labeled_response_force_end)(void);
extern MODVAR void (*kick_user)(MessageTag *mtags, Channel *channel, Client *client, Client *victim, char *comment);
/* /Efuncs */

/* SSL/TLS functions */
extern int early_init_ssl();
extern int init_ssl();
extern int ssl_handshake(Client *);   /* Handshake the accpeted con.*/
extern int ssl_client_handshake(Client *, ConfigItem_link *); /* and the initiated con.*/
extern int ircd_SSL_accept(Client *acptr, int fd);
extern int ircd_SSL_connect(Client *acptr, int fd);
extern int SSL_smart_shutdown(SSL *ssl);
extern void ircd_SSL_client_handshake(int, int, void *);
extern void SSL_set_nonblocking(SSL *s);
extern SSL_CTX *init_ctx(TLSOptions *tlsoptions, int server);
extern MODFUNC char  *tls_get_cipher(SSL *ssl);
extern TLSOptions *get_tls_options_for_client(Client *acptr);
extern int outdated_tls_client(Client *acptr);
extern char *outdated_tls_client_build_string(char *pattern, Client *acptr);
extern int check_certificate_expiry_ctx(SSL_CTX *ctx, char **errstr);
extern EVENT(tls_check_expiry);
extern MODVAR EVP_MD *sha256_function;
extern MODVAR EVP_MD *sha1_function;
extern MODVAR EVP_MD *md5_function;
/* End of SSL/TLS functions */

extern void parse_message_tags_default_handler(Client *client, char **str, MessageTag **mtag_list);
extern char *mtags_to_string_default_handler(MessageTag *m, Client *client);
extern void *labeled_response_save_context_default_handler(void);
extern void labeled_response_set_context_default_handler(void *ctx);
extern void labeled_response_force_end_default_handler(void);
extern int add_silence_default_handler(Client *client, const char *mask, int senderr);
extern int del_silence_default_handler(Client *client, const char *mask);
extern int is_silenced_default_handler(Client *client, Client *acptr);

extern MODVAR MOTDFile opermotd, svsmotd, motd, botmotd, smotd, rules;
extern MODVAR int max_connection_count;
extern int add_listmode(Ban **list, Client *cptr, Channel *channel, char *banid);
extern int add_listmode_ex(Ban **list, Client *cptr, Channel *channel, char *banid, char *setby, time_t seton);
extern int del_listmode(Ban **list, Channel *channel, char *banid);
extern int Halfop_mode(long mode);
extern char *clean_ban_mask(char *, int, Client *);
extern int find_invex(Channel *channel, Client *client);
extern void DoMD5(char *mdout, const char *src, unsigned long n);
extern char *md5hash(char *dst, const char *src, unsigned long n);
extern char *sha256hash(char *dst, const char *src, unsigned long n);
extern void sha256hash_binary(char *dst, const char *src, unsigned long n);
extern void sha1hash_binary(char *dst, const char *src, unsigned long n);
extern MODVAR TKL *tklines[TKLISTLEN];
extern MODVAR TKL *tklines_ip_hash[TKLIPHASHLEN1][TKLIPHASHLEN2];
extern char *cmdname_by_spamftarget(int target);
extern void unrealdns_delreq_bycptr(Client *cptr);
extern void sendtxtnumeric(Client *to, FORMAT_STRING(const char *pattern), ...) __attribute__((format(printf,2,3)));
extern void unrealdns_gethostbyname_link(char *name, ConfigItem_link *conf, int ipv4_only);
extern void unrealdns_delasyncconnects(void);
extern int is_autojoin_chan(char *chname);
extern void unreal_free_hostent(struct hostent *he);
extern struct hostent *unreal_create_hostent(char *name, char *ip);
extern char *unreal_time_sync_error(void);
extern int unreal_time_synch(int timeout);
extern char *getcloak(Client *client);
extern MODVAR unsigned char param_to_slot_mapping[256];
extern char *cm_getparameter(Channel *channel, char mode);
extern void cm_putparameter(Channel *channel, char mode, char *str);
extern void cm_freeparameter(Channel *channel, char mode);
extern char *cm_getparameter_ex(void **p, char mode);
extern void cm_putparameter_ex(void **p, char mode, char *str);
extern void cm_freeparameter_ex(void **p, char mode, char *str);
extern int file_exists(char *file);
extern time_t get_file_time(char *fname);
extern long get_file_size(char *fname);
extern void free_motd(MOTDFile *motd); /* s_serv.c */
extern void fix_timers(void);
extern char *chfl_to_sjoin_symbol(int s);
extern char chfl_to_chanmode(int s);
extern void add_pending_net(Client *client, char *str);
extern void free_pending_net(Client *client);
extern Client *find_non_pending_net_duplicates(Client *cptr);
extern PendingNet *find_pending_net_by_sid_butone(char *sid, Client *exempt);
extern Client *find_pending_net_duplicates(Client *cptr, Client **srv, char **sid);
extern MODVAR char serveropts[];
extern MODVAR char *ISupportStrings[];
extern void read_packet(int fd, int revents, void *data);
extern int process_packet(Client *cptr, char *readbuf, int length, int killsafely);
extern void sendto_realops_and_log(FORMAT_STRING(const char *fmt), ...) __attribute__((format(printf,1,2)));
extern int parse_chanmode(ParseMode *pm, char *modebuf_in, char *parabuf_in);
extern void config_report_ssl_error(void);
extern int dead_socket(Client *to, char *notice);
extern Match *unreal_create_match(MatchType type, char *str, char **error);
extern void unreal_delete_match(Match *m);
extern int unreal_match(Match *m, char *str);
extern int unreal_match_method_strtoval(char *str);
extern char *unreal_match_method_valtostr(int val);
extern int mixed_network(void);
extern void unreal_delete_masks(ConfigItem_mask *m);
extern void unreal_add_masks(ConfigItem_mask **head, ConfigEntry *ce);
extern int unreal_mask_match(Client *acptr, ConfigItem_mask *m);
extern int unreal_mask_match_string(const char *name, ConfigItem_mask *m);
extern char *our_strcasestr(char *haystack, char *needle);
extern void update_conf(void);
extern MODVAR int need_34_upgrade;
#ifdef _WIN32
extern MODVAR BOOL IsService;
#endif
extern int match_ip46(char *a, char *b);
extern void extcmodes_check_for_changes(void);
extern void umodes_check_for_changes(void);
extern int config_parse_flood(char *orig, int *times, int *period);
extern int swhois_add(Client *acptr, char *tag, int priority, char *swhois, Client *from, Client *skip);
extern int swhois_delete(Client *acptr, char *tag, char *swhois, Client *from, Client *skip);
extern void remove_oper_privileges(Client *client, int broadcast_mode_change);
extern int client_starttls(Client *acptr);
extern void start_server_handshake(Client *cptr);
extern void reject_insecure_server(Client *cptr);
extern void report_crash(void);
extern void modulemanager(int argc, char *argv[]);
extern int inet_pton4(const char *src, unsigned char *dst);
extern int inet_pton6(const char *src, unsigned char *dst);
extern int unreal_bind(int fd, char *ip, int port, int ipv6);
extern int unreal_connect(int fd, char *ip, int port, int ipv6);
extern int is_valid_ip(char *str);
extern int ipv6_capable(void);
extern MODVAR Client *remote_rehash_client;
extern MODVAR int debugfd;
extern void convert_to_absolute_path(char **path, char *reldir);
extern int has_user_mode(Client *acptr, char mode);
extern int has_channel_mode(Channel *channel, char mode);
extern Cmode_t get_extmode_bitbychar(char m);
extern long get_mode_bitbychar(char m);
extern long find_user_mode(char mode);
extern void start_listeners(void);
extern void buildvarstring(const char *inbuf, char *outbuf, size_t len, const char *name[], const char *value[]);
extern void reinit_tls(void);
extern CMD_FUNC(cmd_error);
extern CMD_FUNC(cmd_dns);
extern CMD_FUNC(cmd_info);
extern CMD_FUNC(cmd_summon);
extern CMD_FUNC(cmd_users);
extern CMD_FUNC(cmd_version);
extern CMD_FUNC(cmd_dalinfo);
extern CMD_FUNC(cmd_credits);
extern CMD_FUNC(cmd_license);
extern CMD_FUNC(cmd_module);
extern CMD_FUNC(cmd_rehash);
extern CMD_FUNC(cmd_die);
extern CMD_FUNC(cmd_restart);
extern void cmd_alias(Client *client, MessageTag *recv_mtags, int parc, char *parv[], char *cmd); /* special! */
extern char *pcre2_version(void);
extern int get_terminal_width(void);
extern int has_common_channels(Client *c1, Client *c2);
extern int user_can_see_member(Client *user, Client *target, Channel *channel);
extern int invisible_user_in_channel(Client *target, Channel *channel);
extern MODVAR int ssl_client_index;
extern TLSOptions *FindTLSOptionsForUser(Client *acptr);
extern int IsWebsocket(Client *acptr);
extern Policy policy_strtoval(char *s);
extern char *policy_valtostr(Policy policy);
extern char policy_valtochar(Policy policy);
extern int verify_certificate(SSL *ssl, char *hostname, char **errstr);
extern char *certificate_name(SSL *ssl);
extern void start_of_normal_client_handshake(Client *acptr);
extern void clicap_pre_rehash(void);
extern void clicap_post_rehash(void);
extern void unload_all_unused_mtag_handlers(void);
extern void send_cap_notify(int add, char *token);
extern void sendbufto_one(Client *to, char *msg, unsigned int quick);
extern MODVAR int current_serial;
extern char *spki_fingerprint(Client *acptr);
extern char *spki_fingerprint_ex(X509 *x509_cert);
extern int is_module_loaded(char *name);
extern void close_std_descriptors(void);
extern void banned_client(Client *acptr, char *bantype, char *reason, int global, int noexit);
extern char *mystpcpy(char *dst, const char *src);
extern size_t add_sjsby(char *buf, char *setby, time_t seton);
extern MaxTarget *findmaxtarget(char *cmd);
extern void setmaxtargets(char *cmd, int limit);
extern void freemaxtargets(void);
extern int max_targets_for_command(char *cmd);
extern void set_targmax_defaults(void);
extern void parse_chanmodes_protoctl(Client *client, char *str);
extern void concat_params(char *buf, int len, int parc, char *parv[]);
extern void charsys_check_for_changes(void);
extern int maxclients;
extern int fast_badword_match(ConfigItem_badword *badword, char *line);
extern int fast_badword_replace(ConfigItem_badword *badword, char *line, char *buf, int max);
extern char *stripbadwords(char *str, ConfigItem_badword *start_bw, int *blocked);
extern int badword_config_process(ConfigItem_badword *ca, char *str);
extern void badword_config_free(ConfigItem_badword *ca);
extern char *badword_config_check_regex(char *s, int fastsupport, int check_broadness);
extern AllowedChannelChars allowed_channelchars_strtoval(char *str);
extern char *allowed_channelchars_valtostr(AllowedChannelChars v);
extern HideIdleTimePolicy hideidletime_strtoval(char *str);
extern char *hideidletime_valtostr(HideIdleTimePolicy v);
extern long ClientCapabilityBit(const char *token);
extern int is_handshake_finished(Client *client);
extern void SetCapability(Client *acptr, const char *token);
extern void ClearCapability(Client *acptr, const char *token);
extern void new_message(Client *sender, MessageTag *recv_mtags, MessageTag **mtag_list);
extern void new_message_special(Client *sender, MessageTag *recv_mtags, MessageTag **mtag_list, FORMAT_STRING(const char *pattern), ...) __attribute__((format(printf,4,5)));
extern void generate_batch_id(char *str);
extern MessageTag *find_mtag(MessageTag *mtags, const char *token);
extern MessageTag *duplicate_mtag(MessageTag *mtag);
extern void free_message_tags(MessageTag *m);
extern time_t server_time_to_unix_time(const char *tbuf);
extern int history_set_limit(char *object, int max_lines, long max_t);
extern int history_add(char *object, MessageTag *mtags, char *line);
extern HistoryResult *history_request(char *object, HistoryFilter *filter);
extern int history_destroy(char *object);
extern int can_receive_history(Client *client);
extern void history_send_result(Client *client, HistoryResult *r);
extern void free_history_result(HistoryResult *r);
extern void free_history_filter(HistoryFilter *f);
extern void special_delayed_unloading(void);
extern int write_int64(FILE *fd, uint64_t t);
extern int write_int32(FILE *fd, uint32_t t);
extern int read_int64(FILE *fd, uint64_t *t);
extern int read_int32(FILE *fd, uint32_t *t);
extern int read_data(FILE *fd, void *buf, size_t len);
extern int write_data(FILE *fd, const void *buf, size_t len);
extern int write_str(FILE *fd, char *x);
extern int read_str(FILE *fd, char **x);
extern int char_to_channelflag(char c);
extern void _free_entire_name_list(NameList *n);
extern void _add_name_list(NameList **list, char *name);
extern void _del_name_list(NameList **list, char *name);
extern NameList *find_name_list(NameList *list, char *name);
extern NameList *find_name_list_match(NameList *list, char *name);
extern int minimum_msec_since_last_run(struct timeval *tv_old, long minimum);
extern int unrl_utf8_validate(const char *str, const char **end);
extern char *unrl_utf8_make_valid(const char *str);
extern void utf8_test(void);
extern MODVAR int non_utf8_nick_chars_in_use;
extern void short_motd(Client *client);
extern int should_show_connect_info(Client *client);
extern void send_invalid_channelname(Client *client, char *channelname);
extern int is_extended_ban(const char *str);
extern int valid_sid(char *name);
extern int valid_uid(char *name);
extern void parse_client_queued(Client *client);
extern char *sha256sum_file(const char *fname);
extern char *filename_strip_suffix(const char *fname, const char *suffix);
extern char *filename_add_suffix(const char *fname, const char *suffix);
extern int filename_has_suffix(const char *fname, const char *suffix);
extern void addmultiline(MultiLine **l, char *line);
extern void freemultiline(MultiLine *l);
#define safe_free_multiline(x) do { if (x) freemultiline(x); x = NULL; } while(0)
extern void sendnotice_multiline(Client *client, MultiLine *m);
extern void unreal_del_quotes(char *i);
extern char *unreal_add_quotes(char *str);
extern int unreal_add_quotes_r(char *i, char *o, size_t len);
extern void user_account_login(MessageTag *recv_mtags, Client *client);
extern void link_generator(void);
extern void update_throttling_timer_settings(void);
extern int hide_idle_time(Client *client, Client *target);
extern void lost_server_link(Client *serv, FORMAT_STRING(const char *fmt), ...);
extern char *sendtype_to_cmd(SendType sendtype);
extern MODVAR MessageTagHandler *mtaghandlers;
extern int security_group_valid_name(char *name);
extern int security_group_exists(char *name);
extern SecurityGroup *add_security_group(char *name, int order);
extern SecurityGroup *find_security_group(char *name);
extern void free_security_group(SecurityGroup *s);
extern void set_security_group_defaults(void);
extern int user_allowed_by_security_group(Client *client, SecurityGroup *s);
extern int user_allowed_by_security_group_name(Client *client, char *secgroupname);
extern void add_nvplist(NameValuePrioList **lst, int priority, char *name, char *value);
extern void add_fmt_nvplist(NameValuePrioList **lst, int priority, char *name, FORMAT_STRING(const char *format), ...) __attribute__((format(printf,4,5)));
extern NameValuePrioList *find_nvplist(NameValuePrioList *list, char *name);
extern void free_nvplist(NameValuePrioList *lst);
extern char *get_connect_extinfo(Client *client);
extern char *unreal_strftime(char *str);
extern void strtolower_safe(char *dst, char *src, int size);
extern int running_interactively(void);
extern void skip_whitespace(char **p);
extern void read_until(char **p, char *stopchars);
/* src/unrealdb.c start */
extern UnrealDB *unrealdb_open(const char *filename, UnrealDBMode mode, char *secret_block);
extern int unrealdb_close(UnrealDB *c);
extern char *unrealdb_test_db(const char *filename, char *secret_block);
extern int unrealdb_write_int64(UnrealDB *c, uint64_t t);
extern int unrealdb_write_int32(UnrealDB *c, uint32_t t);
extern int unrealdb_write_int16(UnrealDB *c, uint16_t t);
extern int unrealdb_write_str(UnrealDB *c, char *x);
extern int unrealdb_write_char(UnrealDB *c, char t);
extern int unrealdb_read_int64(UnrealDB *c, uint64_t *t);
extern int unrealdb_read_int32(UnrealDB *c, uint32_t *t);
extern int unrealdb_read_int16(UnrealDB *c, uint16_t *t);
extern int unrealdb_read_str(UnrealDB *c, char **x);
extern int unrealdb_read_char(UnrealDB *c, char *t);
extern char *unrealdb_test_secret(char *name);
extern UnrealDBConfig *unrealdb_copy_config(UnrealDBConfig *src);
extern UnrealDBConfig *unrealdb_get_config(UnrealDB *db);
extern void unrealdb_free_config(UnrealDBConfig *c);
extern UnrealDBError unrealdb_get_error_code(void);
extern char *unrealdb_get_error_string(void);
/* src/unrealdb.c end */
/* secret { } related stuff */
extern Secret *find_secret(char *secret_name);
extern void free_secret_cache(SecretCache *c);
extern void free_secret(Secret *s);
extern Secret *secrets;
/* end */
extern int check_password_strength(char *pass, int min_length, int strict, char **err);
extern int valid_secret_password(char *pass, char **err);
extern int flood_limit_exceeded(Client *client, FloodOption opt);
extern FloodSettings *find_floodsettings_block(const char *name);
extern FloodSettings *get_floodsettings_for_user(Client *client, FloodOption opt);
extern MODVAR char *floodoption_names[];
extern void flood_limit_exceeded_log(Client *client, char *floodname);
