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

extern int dorehash, dorestart, doreloadcert;
#ifndef _WIN32
extern char **myargv;
#else
extern LPCSTR cmdLine;
#endif
/* Externals */
extern MODVAR char *buildid;
extern MODVAR char backupbuf[8192];
extern EVENT(unrealdns_removeoldrecords);
extern EVENT(unrealdb_expire_secret_cache);
extern void init_glines(void);
extern void tkl_init(void);
extern void process_clients(void);
extern void unrealdb_test(void);
extern void ignore_this_signal();
extern void s_rehash();
extern void s_reloadcert();
extern void s_restart();
extern void s_die();
#ifndef _WIN32
// nix specific
extern char unreallogo[];
#else
// windows specific
extern SERVICE_STATUS_HANDLE IRCDStatusHandle;
extern SERVICE_STATUS IRCDStatus;
#endif
extern MODVAR char *extraflags;
extern MODVAR int tainted;
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
extern PreprocessorItem parse_preprocessor_item(char *start, char *end, const char *filename, int linenumber, ConditionalConfig **cc);
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
extern MODVAR ConfigItem_vhost		*conf_vhost;
extern MODVAR ConfigItem_link		*conf_link;
extern MODVAR ConfigItem_sni		*conf_sni;
extern MODVAR ConfigItem_ban		*conf_ban;
extern MODVAR ConfigItem_deny_channel  *conf_deny_channel;
extern MODVAR ConfigItem_allow_channel *conf_allow_channel;
extern MODVAR ConfigItem_deny_version	*conf_deny_version;
extern MODVAR ConfigItem_alias		*conf_alias;
extern MODVAR ConfigItem_help		*conf_help;
extern MODVAR ConfigItem_offchans	*conf_offchans;
extern MODVAR ConfigItem_proxy		*conf_proxy;
extern void		completed_connection(int, int, void *);
extern void clear_unknown();
extern EVENT(e_unload_module_delayed);
extern EVENT(throttling_check_expire);

extern void  module_loadall(void);
extern long set_usermode(const char *umode);
extern const char *get_usermode_string(Client *acptr);
extern const char *get_usermode_string_r(Client *client, char *buf, size_t buflen);
extern const char *get_usermode_string_raw(long umodes);
extern const char *get_usermode_string_raw_r(long umodes, char *buf, size_t buflen);
extern ConfigFile *config_parse(const char *filename, char *confdata);
extern ConfigFile *config_parse_with_offset(const char *filename, char *confdata, unsigned int line_offset);
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
extern ConfigFile *config_load(const char *filename, const char *displayname);
extern void config_free(ConfigFile *cfptr);
extern void ipport_seperate(const char *string, char **ip, char **port);
extern ConfigItem_class	*find_class(const char *name);
extern ConfigItem_oper		*find_oper(const char *name);
extern ConfigItem_operclass	*find_operclass(const char *name);
extern ConfigItem_listen *find_listen(const char *ipmask, int port, SocketType socket_type);
extern ConfigItem_sni *find_sni(const char *name);
extern ConfigItem_ulines	*find_uline(const char *host);
extern ConfigItem_tld		*find_tld(Client *cptr);
extern ConfigItem_link		*find_link(const char *servername);
extern ConfigItem_ban 		*find_ban(Client *, const char *host, short type);
extern ConfigItem_ban 		*find_banEx(Client *,const char *host, short type, short type2);
extern ConfigItem_vhost	*find_vhost(const char *name);
extern ConfigItem_deny_channel *find_channel_allowed(Client *cptr, const char *name);
extern ConfigItem_alias	*find_alias(const char *name);
extern ConfigItem_help 	*find_Help(const char *command);

extern OperPermission ValidatePermissionsForPath(const char *path, Client *client, Client *victim, Channel *channel, const void *extra);
extern void OperClassValidatorDel(OperClassValidator *validator);

extern ConfigItem_ban  *find_ban_ip(Client *client);
extern void add_ListItem(ListStruct *, ListStruct **);
extern void append_ListItem(ListStruct *item, ListStruct **list);
extern void add_ListItemPrio(ListStructPrio *, ListStructPrio **, int);
extern void del_ListItem(ListStruct *, ListStruct **);
extern MODVAR LoopStruct loop;
extern int del_banid(Channel *channel, const char *banid);
extern int del_exbanid(Channel *channel, const char *banid);
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
extern MODVAR struct list_head control_list;
extern MODVAR struct list_head global_server_list;
extern MODVAR struct list_head dead_list;
extern MODVAR struct list_head rpc_remote_list;
extern RealCommand *find_command(const char *cmd, int flags);
extern RealCommand *find_command_simple(const char *cmd);
extern Membership *find_membership_link(Membership *lp, Channel *ptr);
extern Member *find_member_link(Member *, Client *);
extern int remove_user_from_channel(Client *client, Channel *channel, int dont_log);
extern void add_server_to_table(Client *);
extern void remove_server_from_table(Client *);
extern void iNAH_host(Client *client, const char *host);
extern void set_snomask(Client *client, const char *snomask);
extern int check_tkls(Client *cptr);
/* for services */
extern void send_user_joins(Client *, Client *);
extern int valid_channelname(const char *);
extern int valid_server_name(const char *name);
extern Cmode *find_channel_mode_handler(char letter);
extern int valid_channel_access_mode_letter(char letter);
extern int check_channel_access(Client *client, Channel *channel, const char *modes);
extern int check_channel_access_membership(Membership *mb, const char *modes);
extern int check_channel_access_member(Member *mb, const char *modes);
extern int check_channel_access_string(const char *current_modes, const char *modes);
extern int check_channel_access_letter(const char *current_modes, const char letter);
extern const char *get_channel_access(Client *client, Channel *channel);
extern void add_member_mode_fast(Member *mb, Membership *mbs, char letter);
extern void del_member_mode_fast(Member *mb, Membership *mbs, char letter);
extern void add_member_mode(Client *client, Channel *channel, char letter);
extern void del_member_mode(Client *client, Channel *channel, char letter);
extern char sjoin_prefix_to_mode(char s);
extern char mode_to_sjoin_prefix(char s);
extern char mode_to_prefix(char s);
extern char prefix_to_mode(char s);
extern const char *modes_to_prefix(const char *modes);
extern const char *modes_to_sjoin_prefix(const char *modes);
extern char rank_to_mode(int rank);
extern int mode_to_rank(char mode);
extern char lowest_ranking_mode(const char *modes);
extern char lowest_ranking_prefix(const char *prefix);
extern void channel_member_modes_generate_equal_or_greater(const char *modes, char *buf, size_t buflen);
extern int ban_check_mask(BanContext *b);
extern int extban_is_ok_nuh_extban(BanContext *b);
extern const char *extban_conv_param_nuh_or_extban(BanContext *b, Extban *extban);
extern const char *extban_conv_param_nuh(BanContext *b, Extban *extban);
extern Ban *is_banned(Client *, Channel *, int, const char **, const char **);
extern Ban *is_banned_with_nick(Client *, Channel *, int, const char *, const char **, const char **);
extern int ban_exists(Ban *lst, const char *str);
extern int ban_exists_ignore_time(Ban *lst, const char *str);

extern Client *find_client(const char *, Client *);
extern Client *find_name(const char *, Client *);
extern Client *find_nickserv(const char *, Client *);
extern Client *find_user(const char *, Client *);
extern Client *find_server(const char *, Client *);
extern Client *find_server_by_uid(const char *uid);
extern Client *find_service(const char *, Client *);
#define find_server_quick(x) find_server(x, NULL)
extern char *find_or_add(char *);
extern void inittoken();
extern void reset_help();

extern MODVAR char *debugmode, *configfile, *sbrk0;
extern void set_sockhost(Client *, const char *);
#ifdef _WIN32
extern const char *sock_strerror(int);
#endif

#ifdef _WIN32
extern MODVAR int debuglevel;
#else
extern int debuglevel, errno, h_errno;
#endif
extern MODVAR int OpenFiles;  /* number of files currently open */
extern MODVAR int debuglevel, portnum, debugtty, maxusersperchannel;
extern MODVAR int readcalls, udpfd, resfd;
extern Client *add_connection(ConfigItem_listen *, int);
extern int check_server_init(Client *);
extern void close_connection(Client *);
extern int get_sockerr(Client *);
extern int inetport(ConfigItem_listen *, char *, int, int);
extern void init_sys();
extern void check_user_limit(void);
extern void init_modef();
extern int verify_hostname(const char *name);

extern int setup_ping();

extern void set_channel_mlock(Client *, Channel *, const char *, int);

extern void restart(const char *);
extern void server_reboot(const char *);
extern void terminate(), write_pidfile();
extern void *safe_alloc(size_t size);
extern void set_socket_buffers(int fd, int rcvbuf, int sndbuf);
extern int send_queued(Client *);
extern void send_queued_cb(int fd, int revents, void *data);
extern void sendto_serv_butone_nickcmd(Client *one, MessageTag *mtags, Client *client, const char *umodes);
extern void    sendto_message_one(Client *to, Client *from, const char *sender, const char *cmd, const char *nick, const char *msg);
extern void sendto_channel(Channel *channel, Client *from, Client *skip,
                           char *member_modes, long clicap, int sendflags,
                           MessageTag *mtags,
                           FORMAT_STRING(const char *pattern), ...) __attribute__((format(printf,8,9)));
extern void sendto_local_common_channels(Client *user, Client *skip,
                                         long clicap, MessageTag *mtags,
                                         FORMAT_STRING(const char *pattern), ...) __attribute__((format(printf,5,6)));
extern void quit_sendto_local_common_channels(Client *user, MessageTag *mtags, const char *reason);
extern void sendto_match_servs(Channel *, Client *, FORMAT_STRING(const char *), ...) __attribute__((format(printf,3,4)));
extern void sendto_match_butone(Client *, Client *, const char *, int, MessageTag *,
    FORMAT_STRING(const char *pattern), ...) __attribute__((format(printf,6,7)));
extern void sendto_all_butone(Client *, Client *, FORMAT_STRING(const char *), ...) __attribute__((format(printf,3,4)));
extern void sendto_ops(FORMAT_STRING(const char *), ...) __attribute__((format(printf,1,2)));
extern void sendto_prefix_one(Client *, Client *, MessageTag *, FORMAT_STRING(const char *), ...) __attribute__((format(printf,4,5)));
extern void vsendto_prefix_one(Client *to, Client *from, MessageTag *mtags, const char *pattern, va_list vl);
extern void sendto_opers(FORMAT_STRING(const char *), ...) __attribute__((format(printf,1,2)));
extern void sendto_umode(int, FORMAT_STRING(const char *), ...) __attribute__((format(printf,2,3)));
extern void sendto_umode_global(int, FORMAT_STRING(const char *), ...) __attribute__((format(printf,2,3)));
extern void sendnotice(Client *to, FORMAT_STRING(const char *pattern), ...) __attribute__((format(printf,2,3)));
/** Send numeric message to a client.
 * @param to		The recipient
 * @param numeric	The numeric, one of RPL_* or ERR_*, see include/numeric.h
 * @param ...		The parameters for the numeric
 * @note Be sure to provide the correct number and type of parameters that belong to the numeric. Check include/numeric.h when in doubt!
 * @section sendnumeric_examples Examples
 * @subsection sendnumeric_permission_denied Send "Permission Denied" numeric
 * This numeric has no parameter, so is simple:
 * @code
 * sendnumeric(client, ERR_NOPRIVILEGES);
 * @endcode
 * @subsection sendnumeric_notenoughparameters Send "Not enough parameters" numeric
 * This numeric requires 1 parameter: the name of the command.
 * @code
 * sendnumeric(client, ERR_NEEDMOREPARAMS, "SOMECOMMAND");
 * @endcode
 * @ingroup SendFunctions
 */
#define sendnumeric(to, numeric, ...) sendtaggednumericfmt(to, NULL, numeric, STR_ ## numeric, ##__VA_ARGS__)

/** Send numeric message to a client - format to user specific needs.
 * This will ignore the numeric definition of src/numeric.c and always send ":me.name numeric clientname "
 * followed by the pattern and format string you choose.
 * @param to		The recipient
 * @param numeric	The numeric, one of RPL_* or ERR_*, see src/numeric.c
 * @param pattern	The format string / pattern to use.
 * @param ...		Format string parameters.
 * @note Don't forget to add a colon if you need it (eg `:%%s`), this is a common mistake.
 */
#define sendnumericfmt(to, numeric, ...) sendtaggednumericfmt(to, NULL, numeric, __VA_ARGS__)

/** Send numeric message to a client - format to user specific needs.
 * This will ignore the numeric definition of src/numeric.c and always send ":me.name numeric clientname "
 * followed by the pattern and format string you choose.
 * @param to		The recipient
 * @param mtags     NULL, or NULL-terminated array of message tags
 * @param numeric	The numeric, one of RPL_* or ERR_*, see src/numeric.c
 * @param pattern	The format string / pattern to use.
 * @param ...		Format string parameters.
 * @note Don't forget to add a colon if you need it (eg `:%%s`), this is a common mistake.
 */
#define sendtaggednumeric(to, mtags, numeric, ...) sendtaggednumericfmt(to, mtags, numeric, STR_ ## numeric, ##__VA_ARGS__)

extern void sendtaggednumericfmt(Client *to, MessageTag *mtags, int numeric, FORMAT_STRING(const char *pattern), ...) __attribute__((format(printf,4,5)));

extern void sendtxtnumeric(Client *to, FORMAT_STRING(const char *pattern), ...) __attribute__((format(printf,2,3)));
/** Build numeric message so it is ready to be sent to a client - rarely used, normally you use sendnumeric() instead.
 * This function is normally only used in eg CAN_KICK and CAN_SET_TOPIC, where
 * you need to set an 'errbuf' with a full IRC protocol line to reject the request
 * (which then may or may not be sent depending on operoverride privileges).
 * @param buf		The buffer where the message should be stored to (full IRC protocol line)
 * @param buflen	The size of the buffer
 * @param to		The recipient
 * @param numeric	The numeric, one of RPL_* or ERR_*, see include/numeric.h
 * @param ...		The parameters for the numeric
 * @note Be sure to provide the correct number and type of parameters that belong to the numeric. Check include/numeric.h when in doubt!
 * @ingroup SendFunctions
 */
#define buildnumeric(buf, buflen, to, numeric, ...) buildnumericfmt(buf, buflen, to, numeric, STR_ ## numeric, ##__VA_ARGS__)
extern void buildnumericfmt(char *buf, size_t buflen, Client *to, int numeric, FORMAT_STRING(const char *pattern), ...) __attribute__((format(printf,5,6)));
extern void sendto_server(Client *one, unsigned long caps, unsigned long nocaps, MessageTag *mtags, FORMAT_STRING(const char *format), ...) __attribute__((format(printf, 5, 6)));
extern void send_raw_direct(Client *user, FORMAT_STRING(const char *pattern), ...) __attribute__((format(printf, 2, 3)));
extern MODVAR int writecalls, writeb[];
extern int deliver_it(Client *cptr, char *str, int len, int *want_read);
extern int target_limit_exceeded(Client *client, void *target, const char *name);
extern char *canonize(const char *buffer);
extern int check_registered(Client *);
extern int check_registered_user(Client *);
extern const char *get_client_name(Client *, int);
extern const char *get_client_host(Client *);
extern const char *myctime(time_t);
extern const char *short_date(time_t, char *buf);
extern const char *long_date(time_t);
extern const char *pretty_time_val(long);
extern const char *pretty_time_val_r(char *buf, size_t buflen, long timeval);
extern const char *pretty_date(time_t t);
extern time_t server_time_to_unix_time(const char *tbuf);
extern time_t rfc2616_time_to_unix_time(const char *tbuf);
extern const char *rfc2616_time(time_t clock);
extern void exit_client(Client *client, MessageTag *recv_mtags, const char *comment);
extern void exit_client_fmt(Client *client, MessageTag *recv_mtags, FORMAT_STRING(const char *pattern), ...) __attribute__((format(printf, 3, 4)));
extern void exit_client_ex(Client *client, Client *origin, MessageTag *recv_mtags, const char *comment);
extern void initstats();
extern const char *check_string(const char *);
extern char *make_nick_user_host(const char *, const char *, const char *);
extern char *make_nick_user_host_r(char *namebuf, size_t namebuflen, const char *nick, const char *name, const char *host);
extern char *make_user_host(const char *, const char *);
extern void parse(Client *cptr, char *buffer, int length);
extern int hunt_server(Client *, MessageTag *, const char *, int, int, const char **);
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
extern int link_list_length(Link *lp);
extern int find_str_match_link(Link *, const char *);
extern void free_str_list(Link *);
extern Link *make_link();
extern Ban *make_ban();
extern User *make_user(Client *);
extern Server *make_server();
extern Client *make_client(Client *, Client *);
extern Channel *make_channel(const char *name);
extern Member *find_channel_link(Member *, Channel *);
extern char *pretty_mask(const char *);
extern void add_client_to_list(Client *);
extern void remove_client_from_list(Client *);
extern void initlists(void);
extern void initlist_channels(void);
extern struct hostent *get_res(const char *);
extern struct hostent *gethost_byaddr(const char *, Link *);
extern struct hostent *gethost_byname(const char *, Link *);
extern void flush_cache();
extern void init_resolver(int firsttime);
extern time_t timeout_query_list(time_t);
extern time_t expire_cache(time_t);
extern void del_queries(const char *);

/* Hash stuff */
#define NICK_HASH_TABLE_SIZE 32768
#define CHAN_HASH_TABLE_SIZE 32768
#define WHOWAS_HASH_TABLE_SIZE 32768
#define THROTTLING_HASH_TABLE_SIZE 8192
#define IPUSERS_HASH_TABLE_SIZE 8192
extern uint64_t siphash(const char *in, const char *k);
extern uint64_t siphash_raw(const char *in, size_t len, const char *k);
extern uint64_t siphash_nocase(const char *in, const char *k);
extern void siphash_generate_key(char *k);
extern void init_hash(void);
extern void add_whowas_to_clist(WhoWas **, WhoWas *);
extern void del_whowas_from_clist(WhoWas **, WhoWas *);
extern void add_whowas_to_list(WhoWas **, WhoWas *);
extern void del_whowas_from_list(WhoWas **, WhoWas *);
extern uint64_t hash_whowas_name(const char *name);
extern void create_whowas_entry(Client *client, WhoWas *e, WhoWasEvent event);
extern void free_whowas_fields(WhoWas *e);
extern int add_to_client_hash_table(const char *, Client *);
extern int del_from_client_hash_table(const char *, Client *);
extern int add_to_id_hash_table(const char *, Client *);
extern int del_from_id_hash_table(const char *, Client *);
extern int add_to_channel_hash_table(const char *, Channel *);
extern void del_from_channel_hash_table(const char *, Channel *);
extern Channel *hash_get_chan_bucket(uint64_t);
extern Client *hash_find_client(const char *, Client *);
extern Client *hash_find_id(const char *, Client *);
extern Client *hash_find_nickatserver(const char *, Client *);
extern Channel *find_channel(const char *name);
extern Client *hash_find_server(const char *, Client *);
extern IpUsersBucket *find_ipusers_bucket(Client *client);
extern IpUsersBucket *add_ipusers_bucket(Client *client);
extern void decrease_ipusers_bucket(Client *client);
extern MODVAR struct ThrottlingBucket *ThrottlingHash[THROTTLING_HASH_TABLE_SIZE];
extern MODVAR IpUsersBucket *IpUsersHash_ipv4[IPUSERS_HASH_TABLE_SIZE];
extern MODVAR IpUsersBucket *IpUsersHash_ipv6[IPUSERS_HASH_TABLE_SIZE];


/* Mode externs
*/
extern MODVAR long UMODE_INVISIBLE; /*  makes user invisible */
extern MODVAR long UMODE_OPER;      /*  Operator */
extern MODVAR long UMODE_REGNICK;   /*  Nick set by services as registered */
extern MODVAR long UMODE_SERVNOTICE;/* server notices such as kill */
extern MODVAR long UMODE_HIDE;	     /* Hide from Nukes */
extern MODVAR long UMODE_SECURE;    /*	User is a secure connect */
extern MODVAR long UMODE_DEAF;      /* Deaf */
extern MODVAR long UMODE_HIDEOPER;  /* Hide oper mode */
extern MODVAR long UMODE_SETHOST;   /* used sethost */
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

#ifndef HAVE_STRNLEN
extern size_t strnlen(const char *s, size_t maxlen);
#endif
#ifndef HAVE_STRLCPY
extern size_t strlcpy(char *dst, const char *src, size_t size);
#endif
#ifndef HAVE_STRLNCPY
extern size_t strlncpy(char *dst, const char *src, size_t size, size_t n);
#endif
#ifndef HAVE_STRLCAT
extern size_t strlcat(char *dst, const char *src, size_t size);
#endif
#ifndef HAVE_STRLNCAT
extern size_t strlncat(char *dst, const char *src, size_t size, size_t n);
#endif
extern void strlcat_letter(char *buf, char c, size_t buflen);
extern char *strldup(const char *src, size_t n);

extern void dopacket(Client *, char *, int);

extern void debug(int, FORMAT_STRING(const char *), ...) __attribute__((format(printf,2,3)));
#if defined(DEBUGMODE)
extern void send_usage(Client *, const char *);
extern void count_memory(Client *, const char *);
extern int checkprotoflags(Client *, int, const char *, int);
#endif

extern const char *inetntop(int af, const void *in, char *local_dummy, size_t the_size);

extern void delletterfromstring(char *s, char letter);
extern void addlettertodynamicstringsorted(char **str, char letter);
extern int sort_character_lowercase_before_uppercase(char x, char y);

/* Internal command stuff - not for modules */
extern MODVAR RealCommand *CommandHash[256];
extern void init_CommandHash(void);

/*
 * Close all local socket connections, invalidate client fd's
 * WIN32 cleanup winsock lib
 */
extern void close_connections(void);

extern int b64_encode(unsigned char const *src, size_t srclength, char *target, size_t targsize);
extern int b64_decode(char const *src, unsigned char *target, size_t targsize);

extern AuthenticationType Auth_FindType(const char *hash, const char *type);
extern AuthConfig	*AuthBlockToAuthConfig(ConfigEntry *ce);
extern void		Auth_FreeAuthConfig(AuthConfig *as);
extern int		Auth_Check(Client *cptr, AuthConfig *as, const char *para);
extern const char	*Auth_Hash(AuthenticationType type, const char *text);
extern int   		Auth_CheckError(ConfigEntry *ce, int warn_on_plaintext);
extern int              Auth_AutoDetectHashType(const char *hash);

extern void make_cloakedhost(Client *client, const char *curr, char *buf, size_t buflen);
extern int  channel_canjoin(Client *client, const char *name);
extern char *collapse(char *pattern);
extern void dcc_sync(Client *client);
extern void request_rehash(Client *client);
extern int rehash_internal(Client *client);
extern void s_die();
extern int match_simple(const char *mask, const char *name);
extern int match_esc(const char *mask, const char *name);
extern int add_listener(ConfigItem_listen *conf);
extern void link_cleanup(ConfigItem_link *link_ptr);
extern void       listen_cleanup();
extern int  numeric_collides(long numeric);
extern void      flag_add(char ch);
extern void      flag_del(char ch);
extern void init_dynconf(void);
extern int config_read_start(void);
extern int is_config_read_finished(void);
extern int config_test(void);
extern void config_run(void);
extern void rehash_motdrules();
extern void read_motd(const char *filename, MOTDFile *motd); /* s_serv.c */
extern void send_proto(Client *, ConfigItem_link *);
extern void unload_all_modules(void);
extern void set_sock_opts(int fd, Client *cptr, SocketType socket_type);
extern void stripcrlf(char *line);
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
#define safe_strdup(dst,str) do { if (dst) free(dst); if ((str) == NULL) dst = NULL; else dst = our_strdup(str); } while(0)

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

#define safe_json_decref(result)	do { json_decref(result); result = NULL; } while(0)

/** @} */
extern char *our_strdup(const char *str);
extern char *our_strldup(const char *str, size_t max);
extern char *our_strdup_sensitive(const char *str);

extern long config_checkval(const char *value, unsigned short flags);
extern void config_status(FORMAT_STRING(const char *format), ...) __attribute__((format(printf,1,2)));
extern void init_random();
extern u_char getrandom8();
extern uint16_t getrandom16();
extern uint32_t getrandom32();
extern void gen_random_alnum(char *buf, int numbytes);

/* Check config entry for empty/missing parameter */
#define CheckNull(x) if ((!(x)->value) || (!(*((x)->value)))) { config_error("%s:%i: missing parameter", (x)->file->filename, (x)->line_number); errors++; continue; }
/* as above, but accepting empty string */
#define CheckNullAllowEmpty(x) if ((!(x)->value)) { config_error("%s:%i: missing parameter", (x)->file->filename, (x)->line_number); errors++; continue; }

extern MODVAR char extchmstr[4][64];

extern int extcmode_default_requirechop(Client *, Channel *, char, const char *, int, int);
extern int extcmode_default_requirehalfop(Client *, Channel *, char, const char *, int, int);
extern Cmode_t extcmode_get(Cmode *);
extern void extcmode_init(void);
extern void make_extcmodestr();
extern void extcmode_duplicate_paramlist(void **xi, void **xo);
extern void extcmode_free_paramlist(void **ar);

extern void chmode_str(struct ChMode *, char *, char *, size_t, size_t);
extern const char *get_client_status(Client *);
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
extern void verify_opercount(Client *, const char *);
extern int valid_host(const char *host, int strict);
extern int valid_username(const char *username);
extern int valid_vhost(const char *userhost);
extern int count_oper_sessions(const char *);
extern char *unreal_mktemp(const char *dir, const char *suffix);
extern char *unreal_getpathname(const char *filepath, char *path);
extern const char *unreal_getfilename(const char *path);
extern const char *unreal_getmodfilename(const char *path);
extern int unreal_create_directory_structure_for_file(const char *fname, mode_t mode);
extern int unreal_create_directory_structure(const char *dname, mode_t mode);
extern int unreal_mkdir(const char *pathname, mode_t mode);
extern int unreal_copyfile(const char *src, const char *dest);
extern int unreal_copyfileex(const char *src, const char *dest, int tryhardlink);
extern time_t unreal_getfilemodtime(const char *filename);
extern void unreal_setfilemodtime(const char *filename, time_t mtime);
extern void DeleteTempModules(void);
extern MODVAR Extban *extbaninfo;
extern Extban *findmod_by_bantype(const char *str, const char **remainder);
extern Extban *findmod_by_bantype_raw(const char *str, int ban_name_length);
extern Extban *ExtbanAdd(Module *reserved, ExtbanInfo req);
extern void ExtbanDel(Extban *);
extern void extban_init(void);
extern char *trim_str(char *str, int len);
extern MODVAR char *ban_realhost, *ban_virthost, *ban_ip;
extern void parse_ban_action_config(ConfigEntry *ce, BanAction **store_action);
extern int test_ban_action_config(ConfigEntry *ce);
extern void free_single_ban_action(BanAction *action);
extern void free_all_ban_actions(BanAction *actions);
#define safe_free_all_ban_actions(x) do { free_all_ban_actions(x); x = NULL; } while(0)
#define safe_free_single_ban_action(x) do { free_single_ban_action(x); x = NULL; } while(0)
BanAction *duplicate_ban_actions(BanAction *actions);
extern int highest_ban_action(BanAction *action);
extern BanActionValue banact_stringtoval(const char *s);
extern const char *banact_valtostring(BanActionValue val);
extern BanActionValue banact_chartoval(char c);
extern char banact_valtochar(BanActionValue val);
extern BanAction *banact_value_to_struct(BanActionValue val);
extern int banact_config_only(BanActionValue action);
extern int only_actions_of_type(BanAction *actions, BanActionValue what);
extern int has_actions_of_type(BanAction *actions, BanActionValue what);
extern int only_soft_actions(BanAction *actions);
extern const char *ban_actions_to_string(BanAction *actions);
extern void lower_ban_action_to_maximum(BanAction *actions, BanActionValue limit_action);
extern int spamfilter_gettargets(const char *s, Client *client);
extern char *spamfilter_target_inttostring(int v);
extern char *our_strcasestr(const char *haystack, const char *needle);
extern int spamfilter_getconftargets(const char *s);
extern void remove_all_snomasks(Client *client);
extern void remove_oper_modes(Client *client);
extern char *spamfilter_inttostring_long(int v);
extern int is_invited(Client *client, Channel *channel);
extern void channel_modes(Client *client, char *mbuf, char *pbuf, size_t mbuf_size, size_t pbuf_size, Channel *channel, int hide_local_modes);
extern int op_can_override(const char *acl, Client *client,Channel *channel,void* extra);
extern Client *find_chasing(Client *client, const char *user, int *chasing);
extern MODVAR long opermode;
extern MODVAR long sajoinmode;
extern void add_user_to_channel(Channel *channel, Client *who, const char *modes);
extern int add_banid(Client *, Channel *, const char *);
extern int add_exbanid(Client *cptr, Channel *channel, const char *banid);
extern int sub1_from_channel(Channel *);
extern MODVAR CoreChannelModeTable corechannelmodetable[];
extern char *unreal_encodespace(const char *s);
extern char *unreal_decodespace(const char *s);
extern MODVAR Link *helpign;
extern void reread_motdsandrules();
extern MODVAR int SVSNOOP;
extern int callbacks_check(void);
extern void callbacks_switchover(void);
extern int efunctions_check(void);
extern void efunctions_switchover(void);
extern const char *encode_ip(const char *);
extern const char *decode_ip(const char *);
extern void sendto_one_nickcmd(Client *server, MessageTag *mtags, Client *client, const char *umodes);
extern int on_dccallow_list(Client *to, Client *from);
extern int add_dccallow(Client *client, Client *optr);
extern int del_dccallow(Client *client, Client *optr);
extern void delete_linkblock(ConfigItem_link *link_ptr);
extern void delete_classblock(ConfigItem_class *class_ptr);
extern void del_async_connects(void);
extern void isupport_init(void);
extern void clicap_init(void);
extern void efunctions_init(void);
extern void do_cmd(Client *client, MessageTag *mtags, const char *cmd, int parc, const char *parv[]);
extern MODVAR char *me_hash;
extern MODVAR int dontspread;
extern MODVAR int labeled_response_inhibit;
extern MODVAR int labeled_response_inhibit_end;
extern MODVAR int labeled_response_force;

/* Efuncs */
extern MODVAR void (*do_join)(Client *, int, const char **);
extern MODVAR void (*join_channel)(Channel *channel, Client *client, MessageTag *mtags, const char *flags);
extern MODVAR int (*can_join)(Client *client, Channel *channel, const char *key, char **errmsg);
extern MODVAR void (*do_mode)(Channel *channel, Client *client, MessageTag *mtags, int parc, const char *parv[], time_t sendts, int samode);
extern MODVAR MultiLineMode *(*set_mode)(Channel *channel, Client *cptr, int parc, const char *parv[], u_int *pcount,
                            char pvar[MAXMODEPARAMS][MODEBUFLEN + 3]);
extern MODVAR void (*set_channel_mode)(Channel *channel, MessageTag *mtags, const char *modes, const char *parameters);
extern MODVAR void (*set_channel_topic)(Client *client, Channel *channel, MessageTag *recv_mtags, const char *topic, const char *set_by, time_t set_at);
extern MODVAR void (*cmd_umode)(Client *, MessageTag *, int, const char **);
extern MODVAR int (*register_user)(Client *client);
extern MODVAR int (*tkl_hash)(unsigned int c);
extern MODVAR char (*tkl_typetochar)(int type);
extern MODVAR int (*tkl_chartotype)(char c);
extern MODVAR char (*tkl_configtypetochar)(const char *name);
extern MODVAR const char *(*tkl_type_string)(TKL *tk);
extern MODVAR const char *(*tkl_type_config_string)(TKL *tk);
extern MODVAR TKL *(*tkl_add_serverban)(int type, const char *usermask, const char *hostmask, const char *reason, const char *setby,
                                            time_t expire_at, time_t set_at, int soft, int flags);
extern MODVAR TKL *(*tkl_add_banexception)(int type, const char *usermask, const char *hostmask, SecurityGroup *match,
                                           const char *reason, const char *set_by,
                                           time_t expire_at, time_t set_at, int soft, const char *bantypes, int flags);
extern MODVAR TKL *(*tkl_add_nameban)(int type, const char *name, int hold, const char *reason, const char *setby,
                                          time_t expire_at, time_t set_at, int flags);
extern MODVAR TKL *(*tkl_add_spamfilter)(int type, const char *id, unsigned short target, BanAction *action,
                                         Match *match, const char *rule, SecurityGroup *except,
                                         const char *setby,
                                         time_t expire_at, time_t set_at,
                                         time_t spamf_tkl_duration, const char *spamf_tkl_reason,
                                         int flags);
extern MODVAR TKL *(*find_tkl_serverban)(int type, const char *usermask, const char *hostmask, int softban);
extern MODVAR TKL *(*find_tkl_banexception)(int type, const char *usermask, const char *hostmask, int softban);
extern MODVAR TKL *(*find_tkl_nameban)(int type, const char *name, int hold);
extern MODVAR TKL *(*find_tkl_spamfilter)(int type, const char *match_string, unsigned short action, unsigned short target);
extern MODVAR void (*sendnotice_tkl_del)(const char *removed_by, TKL *tkl);
extern MODVAR void (*sendnotice_tkl_add)(TKL *tkl);
extern MODVAR void (*free_tkl)(TKL *tkl);
extern MODVAR TKL *(*tkl_del_line)(TKL *tkl);
extern MODVAR void (*tkl_check_local_remove_shun)(TKL *tmp);
extern MODVAR int (*find_tkline_match)(Client *cptr, int skip_soft);
extern MODVAR int (*find_shun)(Client *cptr);
extern MODVAR int (*find_spamfilter_user)(Client *client, int flags);
extern MODVAR TKL *(*find_qline)(Client *cptr, const char *nick, int *ishold);
extern MODVAR TKL *(*find_tkline_match_zap)(Client *cptr);
extern MODVAR void (*tkl_stats)(Client *cptr, int type, const char *para, int *cnt);
extern MODVAR void (*tkl_sync)(Client *client);
extern MODVAR void (*cmd_tkl)(Client *client, MessageTag *recv_mtags, int parc, const char *parv[]);
extern MODVAR int (*take_action)(Client *client, BanAction *actions, const char *reason, long duration, int take_action_flags, int *stopped);
extern MODVAR int (*match_spamfilter)(Client *client, const char *str_in, int type, const char *cmd, const char *target, int flags, TKL **rettk);
extern MODVAR int (*match_spamfilter_mtags)(Client *client, MessageTag *mtags, const char *cmd);
extern MODVAR int (*join_viruschan)(Client *client, TKL *tk, int type);
extern MODVAR const char *(*StripColors)(const char *text);
extern MODVAR void (*spamfilter_build_user_string)(char *buf, const char *nick, Client *acptr);
extern MODVAR void (*send_protoctl_servers)(Client *client, int response);
extern MODVAR ConfigItem_link *(*verify_link)(Client *client);
extern MODVAR void (*send_server_message)(Client *client);
extern MODVAR void (*broadcast_md_client)(ModDataInfo *mdi, Client *acptr, ModData *md);
extern MODVAR void (*broadcast_md_channel)(ModDataInfo *mdi, Channel *channel, ModData *md);
extern MODVAR void (*broadcast_md_member)(ModDataInfo *mdi, Channel *channel, Member *m, ModData *md);
extern MODVAR void (*broadcast_md_membership)(ModDataInfo *mdi, Client *acptr, Membership *m, ModData *md);
extern MODVAR void (*broadcast_md_client_cmd)(Client *except, Client *sender, Client *acptr, const char *varname, const char *value);
extern MODVAR void (*broadcast_md_channel_cmd)(Client *except, Client *sender, Channel *channel, const char *varname, const char *value);
extern MODVAR void (*broadcast_md_member_cmd)(Client *except, Client *sender, Channel *channel, Client *acptr, const char *varname, const char *value);
extern MODVAR void (*broadcast_md_membership_cmd)(Client *except, Client *sender, Client *acptr, Channel *channel, const char *varname, const char *value);
extern MODVAR void (*moddata_add_s2s_mtags)(Client *client, MessageTag **mtags);
extern MODVAR void (*moddata_extract_s2s_mtags)(Client *client, MessageTag *mtags);
extern MODVAR void (*send_moddata_client)(Client *srv, Client *acptr);
extern MODVAR void (*send_moddata_channel)(Client *srv, Channel *channel);
extern MODVAR void (*send_moddata_members)(Client *srv);
extern MODVAR void (*broadcast_moddata_client)(Client *acptr);
extern MODVAR void (*introduce_user)(Client *to, Client *acptr);
extern MODVAR int (*check_deny_version)(Client *cptr, const char *software, int protocol, const char *flags);
extern MODVAR int (*match_user)(const char *rmask, Client *acptr, int options);
extern MODVAR void (*userhost_save_current)(Client *client);
extern MODVAR void (*userhost_changed)(Client *client);
extern MODVAR void (*send_join_to_local_users)(Client *client, Channel *channel, MessageTag *mtags);
extern MODVAR int (*do_nick_name)(char *nick);
extern MODVAR int (*do_remote_nick_name)(char *nick);
extern MODVAR const char *(*charsys_get_current_languages)(void);
extern MODVAR void (*broadcast_sinfo)(Client *acptr, Client *to, Client *except);
extern MODVAR void (*connect_server)(ConfigItem_link *aconf, Client *by, struct hostent *hp);
extern MODVAR int (*is_services_but_not_ulined)(Client *client);
extern MODVAR void (*parse_message_tags)(Client *cptr, char **str, MessageTag **mtag_list);
extern MODVAR const char *(*mtags_to_string)(MessageTag *m, Client *acptr);
extern MODVAR int (*can_send_to_channel)(Client *cptr, Channel *channel, const char **msgtext, const char **errmsg, int notice);
extern MODVAR void (*broadcast_md_globalvar)(ModDataInfo *mdi, ModData *md);
extern MODVAR void (*broadcast_md_globalvar_cmd)(Client *except, Client *sender, const char *varname, const char *value);
extern MODVAR int (*tkl_ip_hash)(const char *ip);
extern MODVAR int (*tkl_ip_hash_type)(int type);
extern MODVAR int (*find_tkl_exception)(int ban_type, Client *cptr);
extern MODVAR int (*server_ban_parse_mask)(Client *client, int add, char type, const char *str, char **usermask_out, char **hostmask_out, int *soft, const char **error);
extern MODVAR int (*server_ban_exception_parse_mask)(Client *client, int add, const char *bantypes, const char *str, char **usermask_out, char **hostmask_out, int *soft, const char **error);
extern MODVAR void (*tkl_added)(Client *client, TKL *tkl);
extern MODVAR int (*del_silence)(Client *client, const char *mask);
extern MODVAR int (*add_silence)(Client *client, const char *mask, int senderr);
extern MODVAR int (*is_silenced)(Client *client, Client *acptr);
extern MODVAR void *(*labeled_response_save_context)(void);
extern MODVAR void (*labeled_response_set_context)(void *ctx);
extern MODVAR void (*labeled_response_force_end)(void);
extern MODVAR void (*kick_user)(MessageTag *mtags, Channel *channel, Client *client, Client *victim, const char *comment);
extern MODVAR int (*watch_add)(const char *nick, Client *client, int flags);
extern MODVAR int (*watch_del)(const char *nick, Client *client, int flags);
extern MODVAR int (*watch_del_list)(Client *client, int flags);
extern MODVAR Watch *(*watch_get)(const char *nick);
extern MODVAR int (*watch_check)(Client *client, int reply, void *data, int (*watch_notify)(Client *client, Watch *watch, Link *lp, int event, void *data));
extern MODVAR char *(*tkl_uhost)(TKL *tkl, char *buf, size_t buflen, int options);
extern MODVAR void (*do_unreal_log_remote_deliver)(LogLevel loglevel, const char *subsystem, const char *event_id, MultiLine *msg, const char *json_serialized);
extern MODVAR char *(*get_chmodes_for_user)(Client *client, const char *flags);
extern MODVAR WhoisConfigDetails (*whois_get_policy)(Client *client, Client *target, const char *name);
extern MODVAR int (*make_oper)(Client *client, const char *operblock_name, const char *operclass, ConfigItem_class *clientclass, long modes, const char *snomask, const char *vhost);
extern MODVAR int (*unreal_match_iplist)(Client *client, NameList *l);
extern MODVAR void (*webserver_send_response)(Client *client, int status, char *msg);
extern MODVAR void (*webserver_close_client)(Client *client);
extern MODVAR int (*webserver_handle_body)(Client *client, WebRequest *web, const char *readbuf, int length);
extern MODVAR void (*rpc_response)(Client *client, json_t *request, json_t *result);
extern MODVAR void (*rpc_error)(Client *client, json_t *request, JsonRpcError error_code, const char *error_message);
extern MODVAR void (*rpc_error_fmt)(Client *client, json_t *request, JsonRpcError error_code, FORMAT_STRING(const char *fmt), ...) __attribute__((format(printf,4,5)));
extern MODVAR void (*rpc_send_request_to_remote)(Client *source, Client *target, json_t *request);
extern MODVAR void (*rpc_send_response_to_remote)(Client *source, Client *target, json_t *request);
extern MODVAR int (*rrpc_supported_simple)(Client *target, char **problem_server);
extern MODVAR int (*rrpc_supported)(Client *target, const char *module, const char *minimum_version, char **problem_server);
extern MODVAR int (*websocket_handle_websocket)(Client *client, WebRequest *web, const char *readbuf2, int length2, int callback(Client *client, char *buf, int len));
extern MODVAR int (*websocket_create_packet)(int opcode, char **buf, int *len);
extern MODVAR int (*websocket_create_packet_ex)(int opcode, char **buf, int *len, char *sendbuf, size_t sendbufsize);
extern MODVAR int (*websocket_create_packet_simple)(int opcode, const char **buf, int *len);
extern MODVAR const char *(*check_deny_link)(ConfigItem_link *link, int auto_connect);
extern MODVAR void (*mtag_add_issued_by)(MessageTag **mtags, Client *client, MessageTag *recv_mtags);
extern MODVAR void (*cancel_ident_lookup)(Client *client);
extern MODVAR int (*spamreport)(Client *client, const char *ip, NameValuePrioList *details, const char *spamreport_block);
extern MODVAR int (*crule_test)(const char *rule);
extern MODVAR CRuleNode *(*crule_parse)(const char *rule);
extern int (*crule_eval)(crule_context *context, CRuleNode *rule);
#define safe_crule_free(x) do { if (x) crule_free(&x); } while(0)
extern void (*crule_free)(CRuleNode **);
extern const char *(*crule_errstring)(int errcode);
extern void (*ban_act_set_reputation)(Client *client, BanAction *action);
/* /Efuncs */

/* TLS functions */
extern int early_init_tls();
extern int init_tls();
extern int ssl_handshake(Client *);   /* Handshake the accpeted con.*/
extern int ssl_client_handshake(Client *, ConfigItem_link *); /* and the initiated con.*/
extern int unreal_tls_accept(Client *acptr, int fd);
extern int unreal_tls_connect(Client *acptr, int fd);
extern int SSL_smart_shutdown(SSL *ssl);
extern const char *ssl_error_str(int err, int my_errno);
extern void unreal_tls_client_handshake(int, int, void *);
extern void SSL_set_nonblocking(SSL *s);
extern SSL_CTX *init_ctx(TLSOptions *tlsoptions, int server);
extern const char *tls_get_cipher(Client *client);
extern TLSOptions *get_tls_options_for_client(Client *acptr);
extern int outdated_tls_client(Client *acptr);
extern const char *outdated_tls_client_build_string(const char *pattern, Client *acptr);
extern int check_certificate_expiry_ctx(SSL_CTX *ctx, char **errstr);
extern EVENT(tls_check_expiry);
extern MODVAR EVP_MD *sha256_function;
extern MODVAR EVP_MD *sha1_function;
extern MODVAR EVP_MD *md5_function;
/* End of TLS functions */

/* Default handlers for efunctions */
extern void parse_message_tags_default_handler(Client *client, char **str, MessageTag **mtag_list);
extern const char *mtags_to_string_default_handler(MessageTag *m, Client *client);
extern void *labeled_response_save_context_default_handler(void);
extern void labeled_response_set_context_default_handler(void *ctx);
extern void labeled_response_force_end_default_handler(void);
extern int add_silence_default_handler(Client *client, const char *mask, int senderr);
extern int del_silence_default_handler(Client *client, const char *mask);
extern int is_silenced_default_handler(Client *client, Client *acptr);
extern void do_unreal_log_remote_deliver_default_handler(LogLevel loglevel, const char *subsystem, const char *event_id, MultiLine *msg, const char *json_serialized);
extern int make_oper_default_handler(Client *client, const char *operblock_name, const char *operclass, ConfigItem_class *clientclass, long modes, const char *snomask, const char *vhost);
extern void webserver_send_response_default_handler(Client *client, int status, char *msg);
extern void webserver_close_client_default_handler(Client *client);
extern int webserver_handle_body_default_handler(Client *client, WebRequest *web, const char *readbuf, int length);
extern void rpc_response_default_handler(Client *client, json_t *request, json_t *result);
extern void rpc_error_default_handler(Client *client, json_t *request, JsonRpcError error_code, const char *error_message);
extern void rpc_error_fmt_default_handler(Client *client, json_t *request, JsonRpcError error_code, const char *fmt, ...);
extern void rpc_send_request_to_remote_default_handler(Client *source, Client *target, json_t *request);
extern void rpc_send_response_to_remote_default_handler(Client *source, Client *target, json_t *response);
extern int rrpc_supported_simple_default_handler(Client *target, char **problem_server);
extern int rrpc_supported_default_handler(Client *target, const char *module, const char *minimum_version, char **problem_server);
extern int websocket_handle_websocket_default_handler(Client *client, WebRequest *web, const char *readbuf2, int length2, int callback(Client *client, char *buf, int len));
extern int websocket_create_packet_default_handler(int opcode, char **buf, int *len);
extern int websocket_create_packet_ex_default_handler(int opcode, char **buf, int *len, char *sendbuf, size_t sendbufsize);
extern int websocket_create_packet_simple_default_handler(int opcode, const char **buf, int *len);
extern void mtag_add_issued_by_default_handler(MessageTag **mtags, Client *client, MessageTag *recv_mtags);
extern void cancel_ident_lookup_default_handler(Client *client);
extern int spamreport_default_handler(Client *client, const char *ip, NameValuePrioList *details, const char *spamreport_block);
extern void ban_act_set_reputation_default_handler(Client *client, BanAction *action);
/* End of default handlers for efunctions */

extern MODVAR MOTDFile opermotd, svsmotd, motd, botmotd, smotd, rules;
extern MODVAR int max_connection_count;
extern int add_listmode(Ban **list, Client *cptr, Channel *channel, const char *banid);
extern int add_listmode_ex(Ban **list, Client *cptr, Channel *channel, const char *banid, const char *setby, time_t seton);
extern int del_listmode(Ban **list, Channel *channel, const char *banid);
extern int Halfop_mode(long mode);
extern const char *convert_regular_ban(char *mask, char *buf, size_t buflen);
extern const char *clean_ban_mask(const char *, int, Client *, int);
extern int find_invex(Channel *channel, Client *client);
extern void DoMD5(char *mdout, const char *src, unsigned long n);
extern char *md5hash(char *dst, const char *src, unsigned long n);
extern char *sha256hash(char *dst, const char *src, unsigned long n);
extern void sha256hash_binary(char *dst, const char *src, unsigned long n);
extern void sha1hash_binary(char *dst, const char *src, unsigned long n);
extern MODVAR TKL *tklines[TKLISTLEN];
extern MODVAR TKL *tklines_ip_hash[TKLIPHASHLEN1][TKLIPHASHLEN2];
extern const char *cmdname_by_spamftarget(int target);
extern void unrealdns_delreq_bycptr(Client *cptr);
extern void unrealdns_gethostbyname_link(const char *name, ConfigItem_link *conf, int ipv4_only);
extern void unrealdns_delasyncconnects(void);
extern EVENT(unrealdns_timeout);
extern void unreal_free_hostent(struct hostent *he);
extern struct hostent *unreal_create_hostent(const char *name, const char *ip);
extern const char *unreal_time_sync_error(void);
extern int unreal_time_synch(int timeout);
extern const char *getcloak(Client *client);
extern MODVAR unsigned char param_to_slot_mapping[256];
extern const char *cm_getparameter(Channel *channel, char mode);
extern const char *cm_getparameter_ex(void **p, char mode);
extern void cm_putparameter(Channel *channel, char mode, const char *str);
extern void cm_putparameter_ex(void **p, char mode, const char *str);
extern void cm_freeparameter(Channel *channel, char mode);
extern void cm_freeparameter_ex(void **p, char mode, char *str);
extern int file_exists(const char *file);
extern time_t get_file_time(const char *fname);
extern long get_file_size(const char *fname);
extern void free_motd(MOTDFile *motd); /* s_serv.c */
extern void fix_timers(void);
extern const char *chfl_to_sjoin_symbol(int s);
extern char chfl_to_chanmode(int s);
extern void add_pending_net(Client *client, const char *str);
extern void free_pending_net(Client *client);
extern Client *find_non_pending_net_duplicates(Client *cptr);
extern PendingNet *find_pending_net_by_sid_butone(const char *sid, Client *exempt);
extern Client *find_pending_net_duplicates(Client *cptr, Client **srv, char **sid);
extern MODVAR char serveropts[];
extern MODVAR char *ISupportStrings[];
extern void read_packet(int fd, int revents, void *data);
extern int process_packet(Client *cptr, char *readbuf, int length, int killsafely);
extern int parse_chanmode(ParseMode *pm, const char *modebuf_in, const char *parabuf_in);
extern int dead_socket(Client *to, const char *notice);
extern Match *unreal_create_match(MatchType type, const char *str, char **error);
extern void unreal_delete_match(Match *m);
extern int unreal_match(Match *m, const char *str);
extern int unreal_match_method_strtoval(const char *str);
extern char *unreal_match_method_valtostr(int val);
#ifdef _WIN32
extern MODVAR BOOL IsService;
#endif
extern void extcmodes_check_for_changes(void);
extern void umodes_check_for_changes(void);
extern int config_parse_flood(const char *orig, int *times, int *period);
extern int swhois_add(Client *acptr, const char *tag, int priority, const char *swhois, Client *from, Client *skip);
extern int swhois_delete(Client *acptr, const char *tag, const char *swhois, Client *from, Client *skip);
extern void remove_oper_privileges(Client *client, int broadcast_mode_change);
extern int client_starttls(Client *acptr);
extern void start_server_handshake(Client *cptr);
extern void reject_insecure_server(Client *cptr);
extern void report_crash(void);
extern void modulemanager(int argc, char *argv[]);
extern int inet_pton4(const char *src, unsigned char *dst);
extern int inet_pton6(const char *src, unsigned char *dst);
extern int unreal_bind(int fd, const char *ip, int port, SocketType socket_type);
extern int unreal_connect(int fd, const char *ip, int port, SocketType socket_type);
extern int is_valid_ip(const char *str);
extern int ipv6_capable(void);
extern int unix_sockets_capable(void);
#ifdef _WIN32
extern void init_winsock(void);
#endif
extern MODVAR Client *remote_rehash_client;
extern MODVAR json_t *json_rehash_log;
extern MODVAR int debugfd;
extern void convert_to_absolute_path(char **path, const char *reldir);
extern int has_user_mode(Client *acptr, char mode);
extern int has_channel_mode(Channel *channel, char mode);
extern int has_channel_mode_raw(Cmode_t m, char mode);
extern Cmode_t get_extmode_bitbychar(char m);
extern long find_user_mode(char mode);
extern void start_listeners(void);
extern void buildvarstring(const char *inbuf, char *outbuf, size_t len, const char *name[], const char *value[]);
extern void buildvarstring_nvp(const char *inbuf, char *outbuf, size_t len, NameValuePrioList *list, int flags);
extern int reinit_tls(void);
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
extern void cmd_alias(Client *client, MessageTag *recv_mtags, int parc, const char *parv[], const char *cmd); /* special! */
extern const char *pcre2_version(void);
extern int get_terminal_width(void);
extern int has_common_channels(Client *c1, Client *c2);
extern int user_can_see_member(Client *user, Client *target, Channel *channel);
extern int user_can_see_member_fast(Client *user, Client *target, Channel *channel, Member *target_member, const char *user_member_modes);
extern int invisible_user_in_channel(Client *target, Channel *channel);
extern MODVAR int tls_client_index;
extern TLSOptions *FindTLSOptionsForUser(Client *acptr);
extern int IsWebsocket(Client *acptr);
extern Policy policy_strtoval(const char *s);
extern const char *policy_valtostr(Policy policy);
extern char policy_valtochar(Policy policy);
extern int verify_certificate(SSL *ssl, const char *hostname, char **errstr);
extern const char *certificate_name(SSL *ssl);
extern void start_of_normal_client_handshake(Client *acptr);
extern void clicap_pre_rehash(void);
extern void clicap_check_for_changes(void);
extern void unload_all_unused_mtag_handlers(void);
extern void send_cap_notify(int add, const char *token);
extern void sendbufto_one(Client *to, char *msg, unsigned int quick);
extern MODVAR int current_serial;
extern const char *spki_fingerprint(Client *acptr);
extern const char *spki_fingerprint_ex(X509 *x509_cert);
extern int is_module_loaded(const char *name);
extern int is_blacklisted_module(const char *name);
extern void close_std_descriptors(void);
extern void banned_client(Client *acptr, const char *bantype, const char *reason, int global, int noexit);
extern char *mystpcpy(char *dst, const char *src);
extern size_t add_sjsby(char *buf, const char *setby, time_t seton);
extern MaxTarget *findmaxtarget(const char *cmd);
extern void setmaxtargets(const char *cmd, int limit);
extern void freemaxtargets(void);
extern int max_targets_for_command(const char *cmd);
extern void set_targmax_defaults(void);
extern void parse_chanmodes_protoctl(Client *client, const char *str);
extern void concat_params(char *buf, int len, int parc, const char *parv[]);
extern void charsys_check_for_changes(void);
extern void dns_check_for_changes(void);
extern MODVAR int maxclients;
extern int fast_badword_match(ConfigItem_badword *badword, const char *line);
extern int fast_badword_replace(ConfigItem_badword *badword, const char *line, char *buf, int max);
extern const char *stripbadwords(const char *str, ConfigItem_badword *start_bw, int *blocked);
extern int badword_config_process(ConfigItem_badword *ca, const char *str);
extern void badword_config_free(ConfigItem_badword *ca);
extern const char *badword_config_check_regex(const char *s, int fastsupport, int check_broadness);
extern AllowedChannelChars allowed_channelchars_strtoval(const char *str);
extern const char *allowed_channelchars_valtostr(AllowedChannelChars v);
extern HideIdleTimePolicy hideidletime_strtoval(const char *str);
extern const char *hideidletime_valtostr(HideIdleTimePolicy v);
extern long ClientCapabilityBit(const char *token);
extern int is_handshake_finished(Client *client);
extern void SetCapability(Client *acptr, const char *token);
extern void ClearCapability(Client *acptr, const char *token);
extern void new_message(Client *sender, MessageTag *recv_mtags, MessageTag **mtag_list);
extern void new_message_special(Client *sender, MessageTag *recv_mtags, MessageTag **mtag_list, FORMAT_STRING(const char *pattern), ...) __attribute__((format(printf,4,5)));
extern void generate_batch_id(char *str);
extern MessageTag *find_mtag(MessageTag *mtags, const char *token);
extern MessageTag *duplicate_mtag(MessageTag *mtag);
#define safe_free_message_tags(x) do { if (x) free_message_tags(x); x = NULL; } while(0)
extern void free_message_tags(MessageTag *m);
extern int history_set_limit(const char *object, int max_lines, long max_t);
extern int history_add(const char *object, MessageTag *mtags, const char *line);
extern HistoryResult *history_request(const char *object, HistoryFilter *filter);
extern int history_delete(const char *object, HistoryFilter *filter, int *rejected_deletes);
extern int history_destroy(const char *object);
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
extern int write_str(FILE *fd, const char *x);
extern int read_str(FILE *fd, char **x);
extern void _free_entire_name_list(NameList *n);
extern void _add_name_list(NameList **list, const char *name);
extern void _del_name_list(NameList **list, const char *name);
extern NameList *duplicate_name_list(NameList *e);
extern NameList *find_name_list(NameList *list, const char *name);
extern NameList *find_name_list_match(NameList *list, const char *name);
extern int minimum_msec_since_last_run(struct timeval *tv_old, long minimum);
extern int unrl_utf8_validate(const char *str, const char **end);
extern char *unrl_utf8_make_valid(const char *str, char *outputbuf, size_t outputbuflen, int strict_length_check);
extern void utf8_test(void);
extern MODVAR int non_utf8_nick_chars_in_use;
extern void short_motd(Client *client);
extern int should_show_connect_info(Client *client);
extern void send_invalid_channelname(Client *client, const char *channelname);
extern int is_extended_ban(const char *str);
extern int is_extended_server_ban(const char *str);
extern int empty_mode(const char *m);
extern void free_multilinemode(MultiLineMode *m);
#define safe_free_multilinemode(m) do { if (m) free_multilinemode(m); m = NULL; } while(0)
extern int valid_sid(const char *name);
extern int valid_uid(const char *name);
extern void parse_client_queued(Client *client);
extern const char *sha256sum_file(const char *fname);
extern char *filename_strip_suffix(const char *fname, const char *suffix);
extern char *filename_add_suffix(const char *fname, const char *suffix);
extern int filename_has_suffix(const char *fname, const char *suffix);
extern void addmultiline(MultiLine **l, const char *line);
extern void freemultiline(MultiLine *l);
#define safe_free_multiline(x) do { if (x) freemultiline(x); x = NULL; } while(0)
extern MultiLine *line2multiline(const char *str);
extern void sendnotice_multiline(Client *client, MultiLine *m);
extern void unreal_del_quotes(char *i);
extern const char *unreal_add_quotes(const char *str);
extern int unreal_add_quotes_r(const char *i, char *o, size_t len);
extern void user_account_login(MessageTag *recv_mtags, Client *client);
extern void link_generator(void);
extern void update_throttling_timer_settings(void);
extern int hide_idle_time(Client *client, Client *target);
extern void lost_server_link(Client *serv, const char *tls_error_string);
extern const char *sendtype_to_cmd(SendType sendtype);
extern MODVAR MessageTagHandler *mtaghandlers;
extern MODVAR RPCHandler *rpchandlers;
#define nv_find_by_name(stru, name)       do_nv_find_by_name(stru, name, ARRAY_SIZEOF((stru)))
extern long do_nv_find_by_name(NameValue *table, const char *cmd, int numelements);
#define nv_find_by_value(stru, value)       do_nv_find_by_value(stru, value, ARRAY_SIZEOF((stru)))
extern const char *do_nv_find_by_value(NameValue *table, long value, int numelements);
extern NameValuePrioList *add_nvplist(NameValuePrioList **lst, int priority, const char *name, const char *value);
extern void add_fmt_nvplist(NameValuePrioList **lst, int priority, const char *name, FORMAT_STRING(const char *format), ...) __attribute__((format(printf,4,5)));
/** Combination of add_nvplist() and buildnumeric() for convenience - only used in WHOIS response functions.
 * @param lst		The NameValuePrioList &head
 * @param priority	The priority of the item being added
 * @param name		The name of the item being added (eg: "certfp")
 * @param to		The recipient
 * @param numeric	The numeric, one of RPL_* or ERR_*, see include/numeric.h
 * @param ...		The parameters for the numeric
 * @note Be sure to provide the correct number and type of parameters that belong to the numeric. Check include/numeric.h when in doubt!
 */
#define add_nvplist_numeric(lst, priority, name, to, numeric, ...) add_nvplist_numeric_fmt(lst, priority, name, to, numeric, STR_ ## numeric, ##__VA_ARGS__)
extern void add_nvplist_numeric_fmt(NameValuePrioList **lst, int priority, const char *name, Client *to, int numeric, FORMAT_STRING(const char *pattern), ...) __attribute__((format(printf,6,7)));
extern NameValuePrioList *find_nvplist(NameValuePrioList *list, const char *name);
extern const char *get_nvplist(NameValuePrioList *list, const char *name);
extern void free_nvplist(NameValuePrioList *lst);
#define safe_free_nvplist(x)	do { free_nvplist(x); x = NULL; } while(0)
extern void del_nvplist_entry(NameValuePrioList *nvp, NameValuePrioList **lst);
extern NameValuePrioList *duplicate_nvplist(NameValuePrioList *e);
extern NameValuePrioList *duplicate_nvplist_append(NameValuePrioList *e, NameValuePrioList **list);
extern void unreal_add_name_values(NameValuePrioList **n, const char *name, ConfigEntry *ce);
extern const char *namevalue(NameValuePrioList *n);
extern const char *namevalue_nospaces(NameValuePrioList *n);
extern const char *get_connect_extinfo(Client *client);
extern char *unreal_strftime(const char *str);
extern void strtolower(char *str);
extern void strtolower_safe(char *dst, const char *src, int size);
extern void strtoupper(char *str);
extern void strtoupper_safe(char *dst, const char *src, int size);
extern int running_interactively(void);
extern int terminal_supports_color(void);
extern void skip_whitespace(char **p);
extern void read_until(char **p, char *stopchars);
extern int is_ip_valid(const char *ip);
extern int is_file_readable(const char *file, const char *dir);
/* json.c */
extern int log_json_filter;
extern json_t *json_string_unreal(const char *s);
extern const char *json_object_get_string(json_t *j, const char *name);
extern int json_object_get_integer(json_t *j, const char *name, int default_value);
extern int json_object_get_boolean(json_t *j, const char *name, int default_value);
extern json_t *json_timestamp(time_t v);
extern const char *timestamp_iso8601_now(void);
extern const char *timestamp_iso8601(time_t v);
extern const char *json_get_value(json_t *t);
extern void json_expand_client(json_t *j, const char *key, Client *client, int detail);
extern void json_expand_client_security_groups(json_t *parent, Client *client);
extern void json_expand_channel(json_t *j, const char *key, Channel *channel, int detail);
extern void json_expand_tkl(json_t *j, const char *key, TKL *tkl, int detail);
/* end of json.c */
/* securitygroup.c start */
extern MODVAR SecurityGroup *securitygroups;
extern void unreal_delete_masks(ConfigItem_mask *m);
extern void unreal_add_masks(ConfigItem_mask **head, ConfigEntry *ce);
extern void unreal_add_mask_string(ConfigItem_mask **head, const char *name);
extern ConfigItem_mask *unreal_duplicate_masks(ConfigItem_mask *existing);
extern int unreal_mask_match(Client *acptr, ConfigItem_mask *m);
extern int unreal_mask_match_string(const char *name, ConfigItem_mask *m);
extern int test_match_item(ConfigFile *conf, ConfigEntry *cep, int *errors);
extern int test_match_block(ConfigFile *conf, ConfigEntry *ce, int *errors_out);
extern int test_match_block_too_broad(ConfigFile *conf, ConfigEntry *ce);
extern int security_group_valid_name(const char *name);
extern int security_group_exists(const char *name);
extern SecurityGroup *add_security_group(const char *name, int order);
extern SecurityGroup *find_security_group(const char *name);
extern void free_security_group(SecurityGroup *s);
extern SecurityGroup *duplicate_security_group(SecurityGroup *s);
extern void set_security_group_defaults(void);
extern int user_allowed_by_security_group(Client *client, SecurityGroup *s);
extern int user_allowed_by_security_group_name(Client *client, const char *secgroupname);
extern const char *get_security_groups(Client *client);
extern int test_match_item(ConfigFile *conf, ConfigEntry *cep, int *errors);
extern int conf_match_item(ConfigFile *conf, ConfigEntry *cep, SecurityGroup **block);
extern int test_match_block(ConfigFile *conf, ConfigEntry *ce, int *errors_out);
extern int conf_match_block(ConfigFile *conf, ConfigEntry *ce, SecurityGroup **block);
extern int test_extended_list(Extban *extban, ConfigEntry *cep, int *errors);
extern int test_set_security_group(ConfigFile *conf, ConfigEntry *ce);
extern int config_set_security_group(ConfigFile *conf, ConfigEntry *ce);
/* securitygroup.c end */
/* src/unrealdb.c start */
extern UnrealDB *unrealdb_open(const char *filename, UnrealDBMode mode, char *secret_block);
extern int unrealdb_close(UnrealDB *c);
extern char *unrealdb_test_db(const char *filename, char *secret_block);
extern int unrealdb_write_int64(UnrealDB *c, uint64_t t);
extern int unrealdb_write_int32(UnrealDB *c, uint32_t t);
extern int unrealdb_write_int16(UnrealDB *c, uint16_t t);
extern int unrealdb_write_str(UnrealDB *c, const char *x);
extern int unrealdb_write_char(UnrealDB *c, char t);
extern int unrealdb_read_int64(UnrealDB *c, uint64_t *t);
extern int unrealdb_read_int32(UnrealDB *c, uint32_t *t);
extern int unrealdb_read_int16(UnrealDB *c, uint16_t *t);
extern int unrealdb_read_str(UnrealDB *c, char **x);
extern int unrealdb_read_char(UnrealDB *c, char *t);
extern const char *unrealdb_test_secret(const char *name);
extern UnrealDBConfig *unrealdb_copy_config(UnrealDBConfig *src);
extern UnrealDBConfig *unrealdb_get_config(UnrealDB *db);
extern void unrealdb_free_config(UnrealDBConfig *c);
extern UnrealDBError unrealdb_get_error_code(void);
extern const char *unrealdb_get_error_string(void);
/* src/unrealdb.c end */
/* secret { } related stuff */
extern Secret *find_secret(const char *secret_name);
extern void free_secret_cache(SecretCache *c);
extern void free_secret(Secret *s);
extern Secret *secrets;
/* end */
extern int check_password_strength(const char *pass, int min_length, int strict, char **err);
extern int valid_secret_password(const char *pass, char **err);
extern int flood_limit_exceeded(Client *client, FloodOption opt);
extern FloodSettings *find_floodsettings_block(const char *name);
extern FloodSettings *get_floodsettings_for_user(Client *client, FloodOption opt);
extern MODVAR const char *floodoption_names[];
extern void flood_limit_exceeded_log(Client *client, const char *floodname);
/* logging */
extern int config_test_log(ConfigFile *conf, ConfigEntry *ce);
extern int config_run_log(ConfigFile *conf, ConfigEntry *ce);
extern const char *log_level_terminal_color(LogLevel loglevel);
#define TERMINAL_COLOR_RESET "\033[0m"
extern LogType log_type_stringtoval(const char *str);
extern const char *log_type_valtostring(LogType v);
#ifdef DEBUGMODE
#define unreal_log(...) do_unreal_log(__VA_ARGS__, log_data_source(__FILE__, __LINE__, __FUNCTION__), NULL)
#define unreal_log_raw(...) do_unreal_log_raw(__VA_ARGS__, log_data_source(__FILE__, __LINE__, __FUNCTION__), NULL)
#else
#define unreal_log(...) do_unreal_log(__VA_ARGS__, NULL)
#define unreal_log_raw(...) do_unreal_log_raw(__VA_ARGS__, NULL)
#endif
extern void do_unreal_log(LogLevel loglevel, const char *subsystem, const char *event_id, Client *client, const char *msg, ...) __attribute__((format(printf,5,0)));
extern void do_unreal_log_raw(LogLevel loglevel, const char *subsystem, const char *event_id, Client *client, const char *msg, ...);
extern void do_unreal_log_internal_from_remote(LogLevel loglevel, const char *subsystem, const char *event_id, MultiLine *msg, json_t *json, const char *json_serialized, Client *from_server);
extern LogData *log_data_string(const char *key, const char *str);
extern LogData *log_data_char(const char *key, const char c);
extern LogData *log_data_integer(const char *key, int64_t integer);
extern LogData *log_data_timestamp(const char *key, time_t ts);
extern LogData *log_data_client(const char *key, Client *client);
extern LogData *log_data_channel(const char *key, Channel *channel);
extern LogData *log_data_source(const char *file, int line, const char *function);
extern LogData *log_data_socket_error(int fd);
extern LogData *log_data_link_block(ConfigItem_link *link);
extern LogData *log_data_tkl(const char *key, TKL *tkl);
extern LogData *log_data_tls_error(void);
extern void log_pre_rehash(void);
extern int log_tests(void);
extern void config_pre_run_log(void);
extern void log_blocks_switchover(void);
extern void postconf_defaults_log_block(void);
extern int valid_loglevel(int v);
extern LogLevel log_level_stringtoval(const char *str);
extern const char *log_level_valtostring(LogLevel loglevel);
extern LogLevel log_level_stringtoval(const char *str);
extern int valid_event_id(const char *s);
extern int valid_subsystem(const char *s);
extern LogSource *add_log_source(const char *str);
extern void free_log_sources(LogSource *l);
extern int log_sources_match(LogSource *logsource, LogLevel loglevel, const char *subsystem, const char *event_id, int matched_already);
extern const char *timestamp_iso8601_now(void);
extern const char *timestamp_iso8601(time_t v);
extern int is_valid_snomask(char c);
extern int is_valid_snomask_string_testing(const char *str, char **wrong);
extern MODVAR LogEntry *memory_log;
extern MODVAR LogEntry *memory_log_tail;
extern MODVAR int memory_log_entries;
extern void free_memory_log_item(LogEntry *e);
extern void memory_log_do_add_message(time_t t, LogLevel loglevel, const char *subsystem, const char *event_id, json_t *json);
extern void memory_log_add_message(time_t t, LogLevel loglevel, const char *subsystem, const char *event_id, json_t *json);
extern EVENT(memory_log_cleaner);
/* end of logging */
extern void add_fake_lag(Client *client, long msec);
extern char *prefix_with_extban(const char *remainder, BanContext *b, Extban *extban, char *buf, size_t buflen);
extern GeoIPResult *geoip_client(Client *client);
extern GeoIPResult *geoip_lookup(const char *ip);
extern void free_geoip_result(GeoIPResult *r);
extern const char *get_operlogin(Client *client);
extern const char *get_operclass(Client *client);
extern struct sockaddr *raw_client_ip(Client *client);
/* url stuff */
extern const char *unreal_mkcache(const char *url);
extern int has_cached_version(const char *url);
extern int url_is_valid(const char *);
extern const char *displayurl(const char *url);
extern char *url_getfilename(const char *url);
extern void download_file_async(const char *url, time_t cachetime, vFP callback, void *callback_data, char *original_url, int maxredirects);
extern void url_start_async(const char *url, HttpMethod http_method, const char *body, NameValuePrioList *request_headers, int store_in_file, time_t cachetime, vFP callback, void *callback_data, char *original_url, int maxredirects);
extern void url_init(void);
extern void url_cancel_handle_by_callback_data(void *ptr);
extern EVENT(url_socket_timeout);
extern int downloads_in_progress(void);
/* end of url stuff */
extern char *collapse(char *pattern);
extern void clear_scache_hash_table(void);
extern void sendto_one(Client *, MessageTag *mtags, FORMAT_STRING(const char *), ...) __attribute__((format(printf,3,4)));
extern void mark_data_to_send(Client *to);
extern EVENT(garbage_collect);
extern EVENT(loop_event);
extern EVENT(check_pings);
extern EVENT(handshake_timeout);
extern EVENT(check_deadsockets);
extern EVENT(try_connections);
extern const char *my_itoa(int i);
extern void load_tunefile(void);
extern EVENT(save_tunefile);
extern EVENT(central_spamfilter_download_evt);
extern void read_motd(const char *filename, MOTDFile *motd);
extern int target_limit_exceeded(Client *client, void *target, const char *name);
extern void make_umodestr(void);
extern void initwhowas(void);
extern void uid_init(void);
extern const char *uid_get(void);
/* proc i/o */
extern void add_proc_io_server(void);
extern void procio_post_rehash(int failure);
/* end of proc i/o */
extern int minimum_msec_since_last_run(struct timeval *tv_old, long minimum);
extern long get_connected_time(Client *client);
extern time_t get_creationtime(Client *client);
extern const char *StripControlCodes(const char *text);
extern const char *StripControlCodesEx(const char *text, char *output, size_t outputlen, int strip_flags);
extern MODVAR Module *Modules;
extern const char *command_issued_by_rpc(MessageTag *mtags);
extern MODVAR int quick_close;
extern MODVAR int connections_past_period;
extern void deadsocket_exit(Client *client, int special);
extern void close_listener(ConfigItem_listen *listener);
extern int str_starts_with_case_sensitive(const char *haystack, const char *needle);
extern int str_ends_with_case_sensitive(const char *haystack, const char *needle);
extern int str_starts_with_case_insensitive(const char *haystack, const char *needle);
extern int str_ends_with_case_insensitive(const char *haystack, const char *needle);
extern void init_dynamic_set_block(DynamicSetBlock *s);
extern void free_dynamic_set_block(DynamicSetBlock *s);
extern int test_dynamic_set_block_item(ConfigFile *conf, const char *security_group, ConfigEntry *cep);
extern int config_set_dynamic_set_block_item(ConfigFile *conf, DynamicSetBlock *s, ConfigEntry *cep);
extern DynamicSetOption *get_setting_for_user(Client *client, SetOption opt);
extern long long get_setting_for_user_number(Client *client, SetOption opt);
extern const char *get_setting_for_user_string(Client *client, SetOption opt);
extern void dynamic_set_string(DynamicSetBlock *s, int settingname, const char *value);
extern void dynamic_set_number(DynamicSetBlock *s, int settingname, long long value);
extern MODVAR DynamicSetBlock unknown_users_set;
extern MODVAR DynamicSetBlock dynamic_set;
extern void start_dns_and_ident_lookup(Client *client);
extern void free_webserver(WebServer *webserver);
#define safe_free_webserver(x)	do { if (x) { free_webserver(x); x = NULL; } } while(0)
extern Tag *find_tag(Client *client, const char *name);
extern Tag *add_tag(Client *client, const char *name, int value);
extern void free_all_tags(Client *client);
extern void del_tag(Client *client, const char *name);
extern void bump_tag_serial(Client *client);
extern int valid_spamfilter_id(const char *s);
extern void download_complete_dontcare(const char *url, const char *file, const char *memory, int memory_len, const char *errorbuf, int cached, void *ptr);
extern char *urlencode(const char *s, char *wbuf, int wlen);
extern const char *config_item_name(ConfigEntry *ce);
extern int inchannel_compareflags(char symbol, const char *member_modes);
extern int highest_channel_member_count(Client *client);
extern MODVAR long long central_spamfilter_last_download;
extern int valid_operclass_character(char c);
extern int valid_operclass_name(const char *str);
