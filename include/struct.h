/************************************************************************
 *   Unreal Internet Relay Chat Daemon, include/struct.h
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
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

#ifndef	__struct_include__
#define __struct_include__

#include "config.h"
#include "sys.h"
/* need to include ssl stuff here coz otherwise you get
 * conflicting types with isalnum/isalpha/etc @ redhat. -- Syzop
 */
#define OPENSSL_NO_KRB5
#include <openssl/rsa.h>       /* SSL stuff */
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>    
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/md5.h>
#include <openssl/ripemd.h>
#ifdef HAS_X509_check_host
#include <openssl/x509v3.h>
#endif
#include <jansson.h>
#include "common.h"
#include "sys.h"
#include <stdio.h>
#include <sys/types.h>
#ifndef _WIN32
#include <netinet/in.h>
#include <netdb.h>
#include <stddef.h>
#endif

#ifdef HAVE_SYSLOG
# include <syslog.h>
# ifdef SYSSYSLOGH
#  include <sys/syslog.h>
# endif
#ifndef UNREAL_LOGGER_CODE
/* undef these as they cause confusion with our ULOG_xxx codes */
#undef LOG_DEBUG
#undef LOG_INFO
#undef LOG_WARNING
#undef LOG_ERROR
#undef LOG_FATAL
#endif
#endif
#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"

#include "channel.h"

typedef struct LoopStruct LoopStruct;
typedef struct TKL TKL;
typedef struct Spamfilter Spamfilter;
typedef struct ServerBan ServerBan;
typedef struct BanException BanException;
typedef struct NameBan NameBan;
typedef struct SpamExcept SpamExcept;
typedef struct ConditionalConfig ConditionalConfig;
typedef struct ConfigEntry ConfigEntry;
typedef struct ConfigFile ConfigFile;
typedef struct ConfigFlag ConfigFlag;
typedef struct ConfigFlag_except ConfigFlag_except;
typedef struct ConfigFlag_ban ConfigFlag_ban;
typedef struct ConfigFlag_tld ConfigFlag_tld;
typedef struct ConfigItem ConfigItem;
typedef struct ConfigItem_me ConfigItem_me;
typedef struct ConfigItem_files ConfigItem_files;
typedef struct ConfigItem_admin ConfigItem_admin;
typedef struct ConfigItem_class ConfigItem_class;
typedef struct ConfigItem_oper ConfigItem_oper;
typedef struct ConfigItem_operclass ConfigItem_operclass;
typedef struct ConfigItem_mask ConfigItem_mask;
typedef struct ConfigItem_drpass ConfigItem_drpass;
typedef struct ConfigItem_ulines ConfigItem_ulines;
typedef struct ConfigItem_tld ConfigItem_tld;
typedef struct ConfigItem_listen ConfigItem_listen;
typedef struct ConfigItem_sni ConfigItem_sni;
typedef struct ConfigItem_allow ConfigItem_allow;
typedef struct ConfigFlag_allow ConfigFlag_allow;
typedef struct ConfigItem_allow_channel ConfigItem_allow_channel;
typedef struct ConfigItem_allow_dcc ConfigItem_allow_dcc;
typedef struct ConfigItem_vhost ConfigItem_vhost;
typedef struct ConfigItem_link	ConfigItem_link;
typedef struct ConfigItem_ban ConfigItem_ban;
typedef struct ConfigItem_deny_dcc ConfigItem_deny_dcc;
typedef struct ConfigItem_deny_channel ConfigItem_deny_channel;
typedef struct ConfigItem_deny_version ConfigItem_deny_version;
typedef struct ConfigItem_alias ConfigItem_alias;
typedef struct ConfigItem_alias_format ConfigItem_alias_format;
typedef struct ConfigResource ConfigResource;
typedef struct ConfigItem_blacklist_module ConfigItem_blacklist_module;
typedef struct ConfigItem_help ConfigItem_help;
typedef struct ConfigItem_offchans ConfigItem_offchans;
typedef struct SecurityGroup SecurityGroup;
typedef struct Secret Secret;
typedef struct ListStruct ListStruct;
typedef struct ListStructPrio ListStructPrio;

#define CFG_TIME 0x0001
#define CFG_SIZE 0x0002
#define CFG_YESNO 0x0004

typedef struct Watch Watch;
typedef struct Client Client;
typedef struct LocalClient LocalClient;
typedef struct Channel Channel;
typedef struct User User;
typedef struct Server Server;
typedef struct RPCClient RPCClient;
typedef struct Link Link;
typedef struct Ban Ban;
typedef struct Mode Mode;
typedef struct MessageTag MessageTag;
typedef struct MOTDFile MOTDFile; /* represents a whole MOTD, including remote MOTD support info */
typedef struct MOTDLine MOTDLine; /* one line of a MOTD stored as a linked list */

typedef struct RealCommand RealCommand;
typedef struct CommandOverride CommandOverride;
typedef struct Member Member;
typedef struct Membership Membership;

typedef enum OperClassEntryType { OPERCLASSENTRY_ALLOW=1, OPERCLASSENTRY_DENY=2} OperClassEntryType;

typedef enum OperPermission { OPER_ALLOW=1, OPER_DENY=0} OperPermission;

typedef enum SendType {
	SEND_TYPE_PRIVMSG	= 0,
	SEND_TYPE_NOTICE	= 1,
	SEND_TYPE_TAGMSG	= 2
} SendType;

struct OperClassValidator;
typedef struct OperClassValidator OperClassValidator;
typedef struct OperClassACLPath OperClassACLPath;
typedef struct OperClass OperClass;
typedef struct OperClassACL OperClassACL;
typedef struct OperClassACLEntry OperClassACLEntry;
typedef struct OperClassACLEntryVar OperClassACLEntryVar;
typedef struct OperClassCheckParams OperClassCheckParams;

typedef OperPermission (*OperClassEntryEvalCallback)(OperClassACLEntryVar* variables,OperClassCheckParams* params);

#ifndef VMSP
#include "dbuf.h"		/* THIS REALLY SHOULDN'T BE HERE!!! --msa */
#endif

#define	HOSTLEN		63	/* Length of hostname */
#define	NICKLEN		30
#define	USERLEN		10
#define	REALLEN	 	50
#define ACCOUNTLEN	30
#define MAXTOPICLEN	360	/* absolute maximum permitted topic length (above this = potential desync) */
#define MAXAWAYLEN	360	/* absolute maximum permitted away length (above this = potential desync) */
#define MAXKICKLEN	360	/* absolute maximum kick length (above this = only cutoff danger) */
#define MAXQUITLEN	395	/* absolute maximum quit length (above this = only cutoff danger) */
#define	CHANNELLEN	32
#define	PASSWDLEN 	256	/* some insane large limit (previously: 20, 32, 48) */
#define	KEYLEN		23
#define LINKLEN		32
#define	BUFSIZE		512	/* WARNING: *DONT* CHANGE THIS!!!! */
#define MAXTAGSIZE	8192	/**< Maximum length of message tags (4K user + 4K server) */
#define MAXLINELENGTH	(MAXTAGSIZE+BUFSIZE)	/**< Maximum length of a line on IRC: 4k client tags + 4k server tags + 512 bytes (IRCv3) */
#define READBUFSIZE	MAXLINELENGTH	/* for the read buffer */
#define	MAXRECIPIENTS 	20
#define	MAXSILELENGTH	NICKLEN+USERLEN+HOSTLEN+10
#define IDLEN		12
#define SIDLEN		3
#define SWHOISLEN	256
#define UMODETABLESZ (sizeof(long) * 8)
#define MAXCCUSERS		20 /* Maximum for set::anti-flood::max-concurrent-conversations */
#define BATCHLEN	22

/*
 * Watch it - Don't change this unless you also change the ERR_TOOMANYWATCH
 * and PROTOCOL_SUPPORTED settings.
 */
#define MAXWATCH	128

#define	USERHOST_REPLYLEN	(NICKLEN+HOSTLEN+USERLEN+5)

/* NOTE: this must be down here so the stuff from struct.h IT uses works */
#include "whowas.h"

/* Logging types */
#define LOG_ERROR 0x0001
#define LOG_KILL  0x0002
#define LOG_SACMDS 0x0080
#define LOG_CHGCMDS 0x0100
#define LOG_OVERRIDE 0x0200

typedef enum LogFieldType {
	LOG_FIELD_INTEGER, // and unsigned?
	LOG_FIELD_STRING,
	LOG_FIELD_CLIENT,
	LOG_FIELD_CHANNEL,
	LOG_FIELD_OBJECT
} LogFieldType;

typedef struct LogData {
	LogFieldType type;
	char *key;
	union {
		int64_t integer;
		char *string;
		Client *client;
		Channel *channel;
		json_t *object;
	} value;
} LogData;

/** New log levels for unreal_log() */
/* Note: the reason for these high numbers is so we can easily catch
 * if someone makes a mistake to use LOG_INFO (from syslog.h) instead
 * of the ULOG_xxx levels.
 */
typedef enum LogLevel {
	ULOG_INVALID = 0,
	ULOG_DEBUG = 1000,
	ULOG_INFO = 2000,
	ULOG_WARNING = 3000,
	ULOG_ERROR = 4000,
	ULOG_FATAL = 5000
} LogLevel;

/** Logging types (text, json, etc) */
typedef enum LogType {
	LOG_TYPE_INVALID = 0,
	LOG_TYPE_TEXT = 1,
	LOG_TYPE_JSON = 2,
} LogType;

#define LOG_CATEGORY_LEN	32
#define LOG_EVENT_ID_LEN	64
typedef struct LogSource LogSource;
struct LogSource {
	LogSource *prev, *next;
	LogLevel loglevel;
	char negative; /**< 1 if negative match (eg !operoverride), 0 if normal */
	char subsystem[LOG_CATEGORY_LEN+1];
	char event_id[LOG_EVENT_ID_LEN+1];
};

typedef struct Log Log;
struct Log {
	Log *prev, *next;
	LogSource *sources;
	int type;
	char destination[CHANNELLEN+1];
	int show_event;
	/* for destination::file */
	char *file;
	char *filefmt;
	long maxsize;
	int logfd;
	/* for destination::channel */
	int color;
	int json_message_tag;
	int oper_only;
};

/** This is used for deciding the <index> in logs[<index>] and temp_logs[<index>] */
typedef enum LogDestination { LOG_DEST_SNOMASK=0, LOG_DEST_OPER=1, LOG_DEST_REMOTE=2, LOG_DEST_CHANNEL=3, LOG_DEST_DISK=4 } LogDestination;
#define NUM_LOG_DESTINATIONS 5

/*
** 'offsetof' is defined in ANSI-C. The following definition
** is not absolutely portable (I have been told), but so far
** it has worked on all machines I have needed it. The type
** should be size_t but...  --msa
*/
#ifndef offsetof
#define	offsetof(t,m) (int)((&((t *)0L)->m))
#endif

#define	elementsof(x) (sizeof(x)/sizeof(x[0]))

/* Calculate the size of an array */
#define ARRAY_SIZEOF(x) (sizeof((x))/sizeof((x)[0]))

/*
** flags for bootup options (command line flags)
*/
#define	BOOT_DEBUG	1
#define	BOOT_TTY	2
#define BOOT_NOFORK     4

/* Length of the key that you feed into siphash_generate_key()
 * DO NOT CHANGE THIS as the siphash code depends on it.
 */
#define SIPHASH_KEY_LENGTH 16

/** The length of a standard 'msgid' tag (note that special
 * msgid tags will be longer).
 * The 22 alphanumeric characters provide slightly more
 * than 128 bits of randomness (62^22 > 2^128).
 * See mtag_add_or_inherit_msgid() for more information.
 */
#define MSGIDLEN	22

/** This specifies the current client status or the client type - see @link ClientStatus @endlink in particular.
 * You may think "server" or "client" are the only choices here, but there are many more
 * such as states where the user is in the middle of an TLS handshake.
 * @defgroup ClientStatuses Client statuses / types
 * @{
 */
typedef enum ClientStatus {
	CLIENT_STATUS_RPC			= -10,	/**< RPC Client (either local or remote) */
	CLIENT_STATUS_CONTROL			= -9,	/**< Client is on the control channel */
	CLIENT_STATUS_LOG			= -8,	/**< Client is a log file */
	CLIENT_STATUS_TLS_STARTTLS_HANDSHAKE	= -7,	/**< Client is doing a STARTTLS handshake */
	CLIENT_STATUS_CONNECTING		= -6,	/**< Client is an outgoing connect */
	CLIENT_STATUS_TLS_CONNECT_HANDSHAKE	= -5,	/**< Client is doing an TLS handshake - outgoing connection */
	CLIENT_STATUS_TLS_ACCEPT_HANDSHAKE	= -4,	/**< Client is doing an TLS handshake - incoming connection */
	CLIENT_STATUS_HANDSHAKE			= -3,	/**< Client is doing a server handshake - outgoing connection */
	CLIENT_STATUS_ME			= -2,	/**< Client is &me (this server) */
	CLIENT_STATUS_UNKNOWN			= -1,	/**< Client is doing a hanshake. May become a server or user later, we don't know yet */
	CLIENT_STATUS_SERVER			= 0,	/**< Client is a server (fully authenticated) */
	CLIENT_STATUS_USER			= 1,	/**< Client is a user (fully authenticated) */
} ClientStatus;

#define	MyConnect(x)			((x)->local)			/**< Is a locally connected client (server or user) */
#define	MyUser(x)			(MyConnect(x) && IsUser(x))	/**< Is a locally connected user */
#define	IsUser(x)	((x)->status == CLIENT_STATUS_USER)	/**< Is a user that has completed the connection handshake */
#define	IsRegistered(x)		((x)->status >= CLIENT_STATUS_SERVER)	/**< Client has completed the connection handshake (user or server) */
#define	IsConnecting(x)		((x)->status == CLIENT_STATUS_CONNECTING)	/**< Is an outgoing connect to another server */
#define	IsHandshake(x)		((x)->status == CLIENT_STATUS_HANDSHAKE)	/**< Is doing a handshake (while connecting to another server) */
#define	IsMe(x)			((x)->status == CLIENT_STATUS_ME)	/**< This is true for &me */
/** Client is not fully registered yet. May become a user or a server, we don't know yet. */
#define	IsUnknown(x)		(((x)->status == CLIENT_STATUS_UNKNOWN) || ((x)->status == CLIENT_STATUS_TLS_STARTTLS_HANDSHAKE))	
#define	IsServer(x)		((x)->status == CLIENT_STATUS_SERVER)	/**< Is a server that has completed the connection handshake */
#define	IsControl(x)		((x)->status == CLIENT_STATUS_CONTROL)	/**< Is on the control channel (not an IRC client) */
#define	IsRPC(x)		((x)->status == CLIENT_STATUS_RPC)	/**< Is doing RPC (not an IRC client) */
#define	IsLog(x)		((x)->status == CLIENT_STATUS_LOG)	/**< Is a log file, not a user or server */
#define IsStartTLSHandshake(x)	((x)->status == CLIENT_STATUS_TLS_STARTTLS_HANDSHAKE)	/**< Currently doing a STARTTLS handshake */
#define IsTLSAcceptHandshake(x)	((x)->status == CLIENT_STATUS_TLS_ACCEPT_HANDSHAKE)	/**< Currently doing a TLS handshake - incoming */
#define IsTLSConnectHandshake(x)	((x)->status == CLIENT_STATUS_TLS_CONNECT_HANDSHAKE)	/**< Currently doing a TLS handshake - outgoing */
#define IsTLSHandshake(x) (IsTLSAcceptHandshake(x) || IsTLSConnectHandshake(x) | IsStartTLSHandshake(x))	/**< Currently doing a TLS handshake (incoming/outgoing/STARTTLS) */

#define SetStartTLSHandshake(x)	((x)->status = CLIENT_STATUS_TLS_STARTTLS_HANDSHAKE)
#define SetTLSAcceptHandshake(x)	((x)->status = CLIENT_STATUS_TLS_ACCEPT_HANDSHAKE)
#define SetTLSConnectHandshake(x)	((x)->status = CLIENT_STATUS_TLS_CONNECT_HANDSHAKE)
#define	SetConnecting(x)	((x)->status = CLIENT_STATUS_CONNECTING)
#define	SetHandshake(x)		((x)->status = CLIENT_STATUS_HANDSHAKE)
#define	SetMe(x)		((x)->status = CLIENT_STATUS_ME)
#define	SetUnknown(x)		((x)->status = CLIENT_STATUS_UNKNOWN)
#define	SetServer(x)		((x)->status = CLIENT_STATUS_SERVER)
#define	SetUser(x)		((x)->status = CLIENT_STATUS_USER)
#define	SetLog(x)		((x)->status = CLIENT_STATUS_LOG)
#define	SetControl(x)		((x)->status = CLIENT_STATUS_CONTROL)
#define	SetRPC(x)		((x)->status = CLIENT_STATUS_RPC)
#define	SetUser(x)		((x)->status = CLIENT_STATUS_USER)

/** @} */

/** Used for checking certain properties of clients, such as IsSecure() and IsULine().
 * @defgroup ClientFlags Client flags
 * @{
 */
#define	CLIENT_FLAG_PINGSENT		0x00000001	/**< PING sent, no reply yet */
#define	CLIENT_FLAG_DEAD		0x00000002	/**< Client is dead: already quit/exited and removed from all lists -- Remaining part will soon be freed in main loop */
#define	CLIENT_FLAG_DEADSOCKET		0x00000004	/**< Local socket is dead but otherwise the client still exists fully -- Will soon exit in main loop */
#define	CLIENT_FLAG_KILLED		0x00000008	/**< Prevents "QUIT" from being sent for this */
#define CLIENT_FLAG_MONITOR_REHASH	0x00000010	/**< Client is monitoring rehash output */
#define CLIENT_FLAG_OUTGOING		0x00000020	/**< Outgoing connection (do not touch cptr->listener->clients) */
#define	CLIENT_FLAG_CLOSING		0x00000040	/**< Set when closing to suppress errors */
#define	CLIENT_FLAG_LISTEN		0x00000080	/**< Used to mark clients which we listen() on */
#define	CLIENT_FLAG_DNSLOOKUP		0x00000100	/**< Client is doing a DNS lookup */
#define	CLIENT_FLAG_IDENTLOOKUP		0x00000200	/**< Client is doing an Ident lookup (RFC931) */
#define	CLIENT_FLAG_IDENTLOOKUPSENT	0x00000400	/**< Set if we havent writen to ident server */
#define	CLIENT_FLAG_LOCALHOST		0x00000800	/**< Set for localhost clients */
#define	CLIENT_FLAG_IDENTSUCCESS	0x00001000	/**< Successful ident lookup achieved */
#define	CLIENT_FLAG_USEIDENT		0x00002000	/**< The allow { } block says we should use the ident (if available) */
#define CLIENT_FLAG_NEXTCALL		0x00004000	/**< Next call (don't ask...) */
#define CLIENT_FLAG_ULINE		0x00008000	/**< User/server is considered U-lined (eg: services) */
#define CLIENT_FLAG_SQUIT		0x00010000	/**< Server has been /SQUIT by an oper */
#define CLIENT_FLAG_PROTOCTL		0x00020000	/**< Received at least 1 PROTOCTL message */
#define CLIENT_FLAG_EAUTH		0x00040000	/**< Server authenticated via PROTOCTL EAUTH */
#define CLIENT_FLAG_NETINFO		0x00080000	/**< Received a NETINFO message */
#define CLIENT_FLAG_QUARANTINE		0x00100000	/**< Quarantined server (don't allow ircops on this server) */
#define CLIENT_FLAG_DCCNOTICE		0x00200000	/**< Has the user seen a notice on how to use DCCALLOW already? */
#define CLIENT_FLAG_SHUNNED		0x00400000	/**< Connection is shunned (user cannot execute any commands) */
#define CLIENT_FLAG_VIRUS		0x00800000	/**< Tagged by spamfilter as a virus */
#define CLIENT_FLAG_TLS			0x01000000	/**< Connection is using TLS */
#define CLIENT_FLAG_NOFAKELAG		0x02000000	/**< Exemption from fake lag */
#define CLIENT_FLAG_DCCBLOCK		0x04000000	/**< Block all DCC send requests */
#define CLIENT_FLAG_MAP			0x08000000	/**< Show this entry in /MAP (only used in map module) */
#define CLIENT_FLAG_PINGWARN		0x10000000	/**< Server ping warning (remote server slow with responding to PINGs) */
#define CLIENT_FLAG_NOHANDSHAKEDELAY	0x20000000	/**< No handshake delay */
#define CLIENT_FLAG_SERVER_DISCONNECT_LOGGED	0x40000000	/**< Server disconnect message is (already) logged */
#define CLIENT_FLAG_ASYNC_RPC			0x80000000	/**< Asynchronous remote RPC request - special case for rehash etc. */

/** @} */

#define OPER_SNOMASKS "+bBcdfkqsSoO"

#define SEND_UMODES (SendUmodes)
#define ALL_UMODES (AllUmodes)
/* SEND_UMODES and ALL_UMODES are now handled by umode_get/umode_lget/umode_gget -- Syzop. */

#define	CLIENT_FLAG_ID	(CLIENT_FLAG_USEIDENT|CLIENT_FLAG_IDENTSUCCESS)

/* PROTO_*: Server protocol extensions (acptr->local->proto).
 * Note that client protocol extensions have been moved
 * to the ClientCapability API which uses acptr->local->caps.
 */
#define PROTO_VL	0x000001	/* Negotiated VL protocol */
#define PROTO_VHP	0x000002	/* Send hostnames in NICKv2 even if not sethosted */
#define PROTO_CLK	0x000004	/* Send cloaked host in the NICK command (regardless of +x/-x) */
#define PROTO_MLOCK	0x000008	/* server supports MLOCK */
#define PROTO_EXTSWHOIS 0x000010	/* extended SWHOIS support */
#define PROTO_SJSBY	0x000020	/* SJOIN setby information (TS and nick) */
#define PROTO_MTAGS	0x000040	/* Support message tags and big buffers */
#define PROTO_NEXTBANS	0x000080	/* Server supports named extended bans */

/* For client capabilities: */
#define CAP_INVERT	1L

/** HasCapabilityFast() checks for a token if you know exactly which bit to check */
#define HasCapabilityFast(cptr, val) ((cptr)->local->caps & (val))
/** HasCapability() checks for a token by name and is slightly slower */
#define HasCapability(cptr, token) ((cptr)->local->caps & ClientCapabilityBit(token))
#define SetCapabilityFast(cptr, val)  do { (cptr)->local->caps |= (val); } while(0)
#define ClearCapabilityFast(cptr, val)  do { (cptr)->local->caps &= ~(val); } while(0)

/* Usermode and snomask macros: */
#define IsDeaf(x)               ((x)->umodes & UMODE_DEAF)
#define	IsOper(x)		((x)->umodes & UMODE_OPER)
#define	IsInvisible(x)		((x)->umodes & UMODE_INVISIBLE)
#define IsRegNick(x)		((x)->umodes & UMODE_REGNICK)
#define IsHidden(x)             ((x)->umodes & UMODE_HIDE)
#define IsSetHost(x)		((x)->umodes & UMODE_SETHOST)
#define IsHideOper(x)		((x)->umodes & UMODE_HIDEOPER)
#define	SetOper(x)		((x)->umodes |= UMODE_OPER)
#define	SetInvisible(x)		((x)->umodes |= UMODE_INVISIBLE)
#define SetRegNick(x)		((x)->umodes & UMODE_REGNICK)
#define SetHidden(x)            ((x)->umodes |= UMODE_HIDE)
#define SetHideOper(x)		((x)->umodes |= UMODE_HIDEOPER)
#define IsSecureConnect(x)	((x)->umodes & UMODE_SECURE)
#define	ClearOper(x)		((x)->umodes &= ~UMODE_OPER)
#define	ClearInvisible(x)	((x)->umodes &= ~UMODE_INVISIBLE)
#define ClearHidden(x)          ((x)->umodes &= ~UMODE_HIDE)
#define ClearHideOper(x)	((x)->umodes &= ~UMODE_HIDEOPER)

/* Client flags macros: to check for via IsXX(),
 * to set via SetXX() and to clear the flag via ClearXX()
 */
/**
 * @addtogroup ClientFlags
 * @{
 */
#define IsIdentLookup(x)		((x)->flags & CLIENT_FLAG_IDENTLOOKUP)	/**< Is doing Ident lookups */
#define IsClosing(x)			((x)->flags & CLIENT_FLAG_CLOSING)	/**< Is closing the connection */
#define IsDCCBlock(x)			((x)->flags & CLIENT_FLAG_DCCBLOCK)
#define IsDCCNotice(x)			((x)->flags & CLIENT_FLAG_DCCNOTICE)
#define IsDead(x)			((x)->flags & CLIENT_FLAG_DEAD)
#define IsDeadSocket(x)			((x)->flags & CLIENT_FLAG_DEADSOCKET)
#define IsServerDisconnectLogged(x)	((x)->flags & CLIENT_FLAG_SERVER_DISCONNECT_LOGGED)
#define IsUseIdent(x)			((x)->flags & CLIENT_FLAG_USEIDENT)
#define IsDNSLookup(x)			((x)->flags & CLIENT_FLAG_DNSLOOKUP)
#define IsEAuth(x)			((x)->flags & CLIENT_FLAG_EAUTH)
#define IsIdentSuccess(x)		((x)->flags & CLIENT_FLAG_IDENTSUCCESS)
#define IsKilled(x)			((x)->flags & CLIENT_FLAG_KILLED)
#define IsMonitorRehash(x)		((x)->flags & CLIENT_FLAG_MONITOR_REHASH)
#define IsListening(x)			((x)->flags & CLIENT_FLAG_LISTEN)
#define IsLocalhost(x)			((x)->flags & CLIENT_FLAG_LOCALHOST)
#define IsMap(x)			((x)->flags & CLIENT_FLAG_MAP)
#define IsNextCall(x)			((x)->flags & CLIENT_FLAG_NEXTCALL)
#define IsNetInfo(x)			((x)->flags & CLIENT_FLAG_NETINFO)
#define IsNoFakeLag(x)			((x)->flags & CLIENT_FLAG_NOFAKELAG)
#define IsOutgoing(x)			((x)->flags & CLIENT_FLAG_OUTGOING)
#define IsPingSent(x)			((x)->flags & CLIENT_FLAG_PINGSENT)
#define IsPingWarning(x)		((x)->flags & CLIENT_FLAG_PINGWARN)
#define IsNoHandshakeDelay(x)		((x)->flags & CLIENT_FLAG_NOHANDSHAKEDELAY)
#define IsProtoctlReceived(x)		((x)->flags & CLIENT_FLAG_PROTOCTL)
#define IsQuarantined(x)		((x)->flags & CLIENT_FLAG_QUARANTINE)
#define IsShunned(x)			((x)->flags & CLIENT_FLAG_SHUNNED)
#define IsSQuit(x)			((x)->flags & CLIENT_FLAG_SQUIT)
#define IsTLS(x)			((x)->flags & CLIENT_FLAG_TLS)
#define IsSecure(x)			((x)->flags & CLIENT_FLAG_TLS)
#define IsULine(x)			((x)->flags & CLIENT_FLAG_ULINE)
#define IsSvsCmdOk(x)			(((x)->flags & CLIENT_FLAG_ULINE) || ((iConf.limit_svscmds == LIMIT_SVSCMDS_SERVERS) && (IsServer((x)) || IsMe((x)))))
#define IsVirus(x)			((x)->flags & CLIENT_FLAG_VIRUS)
#define IsIdentLookupSent(x)		((x)->flags & CLIENT_FLAG_IDENTLOOKUPSENT)
#define IsAsyncRPC(x)			((x)->flags & CLIENT_FLAG_ASYNC_RPC)
#define SetIdentLookup(x)		do { (x)->flags |= CLIENT_FLAG_IDENTLOOKUP; } while(0)
#define SetClosing(x)			do { (x)->flags |= CLIENT_FLAG_CLOSING; } while(0)
#define SetDCCBlock(x)			do { (x)->flags |= CLIENT_FLAG_DCCBLOCK; } while(0)
#define SetDCCNotice(x)			do { (x)->flags |= CLIENT_FLAG_DCCNOTICE; } while(0)
#define SetDead(x)			do { (x)->flags |= CLIENT_FLAG_DEAD; } while(0)
#define SetDeadSocket(x)		do { (x)->flags |= CLIENT_FLAG_DEADSOCKET; } while(0)
#define SetServerDisconnectLogged(x)	do { (x)->flags |= CLIENT_FLAG_SERVER_DISCONNECT_LOGGED; } while(0)
#define SetUseIdent(x)			do { (x)->flags |= CLIENT_FLAG_USEIDENT; } while(0)
#define SetDNSLookup(x)			do { (x)->flags |= CLIENT_FLAG_DNSLOOKUP; } while(0)
#define SetEAuth(x)			do { (x)->flags |= CLIENT_FLAG_EAUTH; } while(0)
#define SetIdentSuccess(x)		do { (x)->flags |= CLIENT_FLAG_IDENTSUCCESS; } while(0)
#define SetKilled(x)			do { (x)->flags |= CLIENT_FLAG_KILLED; } while(0)
#define SetMonitorRehash(x)		do { (x)->flags |= CLIENT_FLAG_MONITOR_REHASH; } while(0)
#define SetListening(x)			do { (x)->flags |= CLIENT_FLAG_LISTEN; } while(0)
#define SetLocalhost(x)			do { (x)->flags |= CLIENT_FLAG_LOCALHOST; } while(0)
#define SetMap(x)			do { (x)->flags |= CLIENT_FLAG_MAP; } while(0)
#define SetNextCall(x)			do { (x)->flags |= CLIENT_FLAG_NEXTCALL; } while(0)
#define SetNetInfo(x)			do { (x)->flags |= CLIENT_FLAG_NETINFO; } while(0)
#define SetNoFakeLag(x)			do { (x)->flags |= CLIENT_FLAG_NOFAKELAG; } while(0)
#define SetOutgoing(x)			do { (x)->flags |= CLIENT_FLAG_OUTGOING; } while(0)
#define SetPingSent(x)			do { (x)->flags |= CLIENT_FLAG_PINGSENT; } while(0)
#define SetPingWarning(x)		do { (x)->flags |= CLIENT_FLAG_PINGWARN; } while(0)
#define SetNoHandshakeDelay(x)		do { (x)->flags |= CLIENT_FLAG_NOHANDSHAKEDELAY; } while(0)
#define SetProtoctlReceived(x)		do { (x)->flags |= CLIENT_FLAG_PROTOCTL; } while(0)
#define SetQuarantined(x)		do { (x)->flags |= CLIENT_FLAG_QUARANTINE; } while(0)
#define SetShunned(x)			do { (x)->flags |= CLIENT_FLAG_SHUNNED; } while(0)
#define SetSQuit(x)			do { (x)->flags |= CLIENT_FLAG_SQUIT; } while(0)
#define SetTLS(x)			do { (x)->flags |= CLIENT_FLAG_TLS; } while(0)
#define SetULine(x)			do { (x)->flags |= CLIENT_FLAG_ULINE; } while(0)
#define SetVirus(x)			do { (x)->flags |= CLIENT_FLAG_VIRUS; } while(0)
#define SetIdentLookupSent(x)		do { (x)->flags |= CLIENT_FLAG_IDENTLOOKUPSENT; } while(0)
#define SetAsyncRPC(x)			do { (x)->flags |= CLIENT_FLAG_ASYNC_RPC; } while(0)
#define ClearIdentLookup(x)		do { (x)->flags &= ~CLIENT_FLAG_IDENTLOOKUP; } while(0)
#define ClearClosing(x)			do { (x)->flags &= ~CLIENT_FLAG_CLOSING; } while(0)
#define ClearDCCBlock(x)		do { (x)->flags &= ~CLIENT_FLAG_DCCBLOCK; } while(0)
#define ClearDCCNotice(x)		do { (x)->flags &= ~CLIENT_FLAG_DCCNOTICE; } while(0)
#define ClearDead(x)			do { (x)->flags &= ~CLIENT_FLAG_DEAD; } while(0)
#define ClearDeadSocket(x)		do { (x)->flags &= ~CLIENT_FLAG_DEADSOCKET; } while(0)
#define ClearUseIdent(x)		do { (x)->flags &= ~CLIENT_FLAG_USEIDENT; } while(0)
#define ClearDNSLookup(x)		do { (x)->flags &= ~CLIENT_FLAG_DNSLOOKUP; } while(0)
#define ClearEAuth(x)			do { (x)->flags &= ~CLIENT_FLAG_EAUTH; } while(0)
#define ClearIdentSuccess(x)		do { (x)->flags &= ~CLIENT_FLAG_IDENTSUCCESS; } while(0)
#define ClearKilled(x)			do { (x)->flags &= ~CLIENT_FLAG_KILLED; } while(0)
#define ClearMonitorRehash(x)		do { (x)->flags &= ~CLIENT_FLAG_MONITOR_REHASH; } while(0)
#define ClearListening(x)		do { (x)->flags &= ~CLIENT_FLAG_LISTEN; } while(0)
#define ClearLocalhost(x)		do { (x)->flags &= ~CLIENT_FLAG_LOCALHOST; } while(0)
#define ClearMap(x)			do { (x)->flags &= ~CLIENT_FLAG_MAP; } while(0)
#define ClearNextCall(x)		do { (x)->flags &= ~CLIENT_FLAG_NEXTCALL; } while(0)
#define ClearNetInfo(x)			do { (x)->flags &= ~CLIENT_FLAG_NETINFO; } while(0)
#define ClearNoFakeLag(x)		do { (x)->flags &= ~CLIENT_FLAG_NOFAKELAG; } while(0)
#define ClearOutgoing(x)		do { (x)->flags &= ~CLIENT_FLAG_OUTGOING; } while(0)
#define ClearPingSent(x)		do { (x)->flags &= ~CLIENT_FLAG_PINGSENT; } while(0)
#define ClearPingWarning(x)		do { (x)->flags &= ~CLIENT_FLAG_PINGWARN; } while(0)
#define ClearNoHandshakeDelay(x)	do { (x)->flags &= ~CLIENT_FLAG_NOHANDSHAKEDELAY; } while(0)
#define ClearProtoctlReceived(x)	do { (x)->flags &= ~CLIENT_FLAG_PROTOCTL; } while(0)
#define ClearQuarantined(x)		do { (x)->flags &= ~CLIENT_FLAG_QUARANTINE; } while(0)
#define ClearShunned(x)			do { (x)->flags &= ~CLIENT_FLAG_SHUNNED; } while(0)
#define ClearSQuit(x)			do { (x)->flags &= ~CLIENT_FLAG_SQUIT; } while(0)
#define ClearTLS(x)			do { (x)->flags &= ~CLIENT_FLAG_TLS; } while(0)
#define ClearULine(x)			do { (x)->flags &= ~CLIENT_FLAG_ULINE; } while(0)
#define ClearVirus(x)			do { (x)->flags &= ~CLIENT_FLAG_VIRUS; } while(0)
#define ClearIdentLookupSent(x)		do { (x)->flags &= ~CLIENT_FLAG_IDENTLOOKUPSENT; } while(0)
#define ClearAsyncRPC(x)		do { (x)->flags &= ~CLIENT_FLAG_ASYNC_RPC; } while(0)
/** @} */

#define IsIPV6(x)			((x)->local->socket_type == SOCKET_TYPE_IPV6)
#define IsUnixSocket(x)			((x)->local->socket_type == SOCKET_TYPE_UNIX)
#define SetIPV6(x)			do { (x)->local->socket_type = SOCKET_TYPE_IPV6; } while(0)
#define SetUnixSocket(x)			do { (x)->local->socket_type = SOCKET_TYPE_UNIX; } while(0)

/* Others that access client structs: */
#define	IsNotSpoof(x)	((x)->local->nospoof == 0)
#define GetHost(x)	(IsHidden(x) ? (x)->user->virthost : (x)->user->realhost)
#define GetIP(x)	(x->ip ? x->ip : "255.255.255.255")
#define IsLoggedIn(x)	(x->user && (*x->user->account != '*') && !isdigit(*x->user->account)) /**< Logged into services */
#define IsSynched(x)	(x->server->flags.synced)
#define IsServerSent(x) (x->server && x->server->flags.server_sent)

/* And more that access client stuff - but actually modularized */
#define GetReputation(client) (moddata_client_get(client, "reputation") ? atoi(moddata_client_get(client, "reputation")) : 0) /**< Get reputation value for a client */

/* PROTOCTL (Server protocol) stuff */
#ifndef DEBUGMODE
#define CHECKSERVERPROTO(x,y)	(((x)->local->proto & y) == y)
#else
#define CHECKSERVERPROTO(x,y) (checkprotoflags(x, y, __FILE__, __LINE__))
#endif

#define SupportVL(x)		(CHECKSERVERPROTO(x, PROTO_VL))
#define SupportSJSBY(x)		(CHECKSERVERPROTO(x, PROTO_SJSBY))
#define SupportVHP(x)		(CHECKSERVERPROTO(x, PROTO_VHP))
#define SupportCLK(x)		(CHECKSERVERPROTO(x, PROTO_CLK))
#define SupportMTAGS(x)		(CHECKSERVERPROTO(x, PROTO_MTAGS))
#define SupportNEXTBANS(x)	(CHECKSERVERPROTO(x, PROTO_NEXTBANS))

#define SetVL(x)		((x)->local->proto |= PROTO_VL)
#define SetSJSBY(x)		((x)->local->proto |= PROTO_SJSBY)
#define SetVHP(x)		((x)->local->proto |= PROTO_VHP)
#define SetCLK(x)		((x)->local->proto |= PROTO_CLK)
#define SetMTAGS(x)		((x)->local->proto |= PROTO_MTAGS)
#define SetNEXTBANS(x)		((x)->local->proto |= PROTO_NEXTBANS)

/* Dcc deny types (see src/s_extra.c) */
#define DCCDENY_HARD	0
#define DCCDENY_SOFT	1

/* Linked list dcc flags */
#define DCC_LINK_ME		1 /* My dcc allow */
#define DCC_LINK_REMOTE	2 /* I need to remove dccallows from these clients when I die */

/** Union for moddata objects */
typedef union ModData ModData;
union ModData
{
        int i;
        long l;
        long long ll;
        char *str;
        void *ptr;
};

#ifndef _WIN32
 #define CHECK_LIST_ENTRY(list)		if (offsetof(typeof(*list),prev) != offsetof(ListStruct,prev)) \
					{ \
						unreal_log(ULOG_FATAL, "main", "BUG_LIST_OPERATION", NULL, \
						           "[BUG] $file:$line: List operation on struct with incorrect order ($error_details)", \
						           log_data_string("error_details", "->prev must be 1st struct member"), \
						           log_data_string("file", __FILE__), \
						           log_data_integer("line", __LINE__)); \
						abort(); \
					} \
					if (offsetof(typeof(*list),next) != offsetof(ListStruct,next)) \
					{ \
						unreal_log(ULOG_FATAL, "main", "BUG_LIST_OPERATION", NULL, \
						           "[BUG] $file:$line: List operation on struct with incorrect order ($error_details)", \
						           log_data_string("error_details", "->next must be 2nd struct member"), \
						           log_data_string("file", __FILE__), \
						           log_data_integer("line", __LINE__)); \
						abort(); \
					}
#else
 #define CHECK_LIST_ENTRY(list)		/* not available on Windows, typeof() not reliable */
#endif

#ifndef _WIN32
 #define CHECK_PRIO_LIST_ENTRY(list)	if (offsetof(typeof(*list),prev) != offsetof(ListStructPrio,prev)) \
					{ \
						unreal_log(ULOG_FATAL, "main", "BUG_LIST_OPERATION", NULL, \
						           "[BUG] $file:$line: List operation on struct with incorrect order ($error_details)", \
						           log_data_string("error_details", "->prev must be 1st struct member"), \
						           log_data_string("file", __FILE__), \
						           log_data_integer("line", __LINE__)); \
						abort(); \
					} \
					if (offsetof(typeof(*list),next) != offsetof(ListStructPrio,next)) \
					{ \
						unreal_log(ULOG_FATAL, "main", "BUG_LIST_OPERATION", NULL, \
						           "[BUG] $file:$line: List operation on struct with incorrect order ($error_details)", \
						           log_data_string("error_details", "->next must be 2nd struct member"), \
						           log_data_string("file", __FILE__), \
						           log_data_integer("line", __LINE__)); \
						abort(); \
					} \
					if (offsetof(typeof(*list),priority) != offsetof(ListStructPrio,priority)) \
					{ \
						unreal_log(ULOG_FATAL, "main", "BUG_LIST_OPERATION", NULL, \
						           "[BUG] $file:$line: List operation on struct with incorrect order ($error_details)", \
						           log_data_string("error_details", "->priority must be 3rd struct member"), \
						           log_data_string("file", __FILE__), \
						           log_data_integer("line", __LINE__)); \
						abort(); \
					}
#else
 #define CHECK_PRIO_LIST_ENTRY(list)	/* not available on Windows, typeof() not reliable */
#endif

#define CHECK_NULL_LIST_ITEM(item)	if ((item)->prev || (item)->next) \
					{ \
						unreal_log(ULOG_FATAL, "main", "BUG_LIST_OPERATION_DOUBLE_ADD", NULL, \
						           "[BUG] $file:$line: List operation on item with non-NULL 'prev' or 'next' -- are you adding to a list twice?", \
						           log_data_string("file", __FILE__), \
						           log_data_integer("line", __LINE__)); \
						abort(); \
					}

/** These are the generic list functions that are used all around in UnrealIRCd.
 * @defgroup ListFunctions List functions
 * @{
 */

/** Generic linked list HEAD */
struct ListStruct {
	ListStruct *prev, *next;
};

/** Generic linked list HEAD with priority */
struct ListStructPrio {
	ListStructPrio *prev, *next;
	int priority;
};

/** Add an item to a standard linked list (in the front)
 */
#define AddListItem(item,list)		do { \
						CHECK_LIST_ENTRY(list) \
						CHECK_LIST_ENTRY(item) \
						CHECK_NULL_LIST_ITEM(item) \
						add_ListItem((ListStruct *)item, (ListStruct **)&list); \
					} while(0)

/** Append an item to a standard linked list (at the back)
*/
#define AppendListItem(item,list)	do { \
						CHECK_LIST_ENTRY(list) \
						CHECK_LIST_ENTRY(item) \
						CHECK_NULL_LIST_ITEM(item) \
						append_ListItem((ListStruct *)item, (ListStruct **)&list); \
					} while(0)

/** Delete an item from a standard linked list
*/
#define DelListItem(item,list)		do { \
						CHECK_LIST_ENTRY(list) \
						CHECK_LIST_ENTRY(item) \
						del_ListItem((ListStruct *)item, (ListStruct **)&list); \
					} while(0)

/** Add an item to a standard linked list - UNCHECKED function, only use if absolutely necessary!
*/
#define AddListItemUnchecked(item,list)	add_ListItem((ListStruct *)item, (ListStruct **)&list)
/** Append an item to a standard linked list - UNCHECKED function, only use if absolutely necessary!
*/
#define AppendListItemUnchecked(item,list) append_ListItem((ListStruct *)item, (ListStruct **)&list)
/** Delete an item from a standard linked list - UNCHECKED function, only use if absolutely necessary!
*/
#define DelListItemUnchecked(item,list) del_ListItem((ListStruct *)item, (ListStruct **)&list)

#define AddListItemPrio(item,list,prio)	do { \
						CHECK_PRIO_LIST_ENTRY(list) \
						CHECK_PRIO_LIST_ENTRY(item) \
						CHECK_NULL_LIST_ITEM(item) \
						item->priority = prio; \
						add_ListItemPrio((ListStructPrio *)item, (ListStructPrio **)&list, prio); \
					} while(0)

#define DelListItemPrio(item,list,prio)	do { \
						CHECK_PRIO_LIST_ENTRY(list) \
						CHECK_PRIO_LIST_ENTRY(item) \
						del_ListItem((ListStruct *)item, (ListStruct **)&list); \
					} while(0)

typedef struct NameList NameList;
/** Generic linked list where each entry has a name which you can use.
 * Use this if you simply want to have a list of entries
 * that only have a name and no other properties.
 *
 * Use the following functions to add, find and delete entries:
 * add_name_list(), find_name_list(), del_name_list(), free_entire_name_list()
 */
struct NameList {
	NameList *prev, *next;
	char name[1];
};

/** Free an entire NameList */
#define free_entire_name_list(list) do { _free_entire_name_list(list); list = NULL; } while(0)
/** Add an entry to a NameList */
#define add_name_list(list, str)  _add_name_list(&list, str)
/** Delete an entry from a NameList - AND free it */
#define del_name_list(list, str)  _del_name_list(&list, str)

extern void unreal_add_names(NameList **n, ConfigEntry *ce);

/** @} */

typedef struct MultiLine MultiLine;
/** Multi-line list.
 * @see addmultiline(), freemultiline(), sendnotice_multiline()
 */
struct MultiLine {
	MultiLine *prev, *next;
	char *line;
};

struct MOTDFile 
{
	struct MOTDLine *lines;
	struct tm last_modified; /* store the last modification time */
};

struct MOTDLine {
	char *line;
	struct MOTDLine *next;
};

/** Current status of configuration in memory (what stage are we in..) */
typedef enum ConfigStatus {
	CONFIG_STATUS_NONE = 0,		/**< Config files have not been parsed yet */
	CONFIG_STATUS_TEST = 1,		/**< Currently running MOD_TEST() */
	CONFIG_STATUS_POSTTEST = 2,	/**< Currently running post_config_test hooks */
	CONFIG_STATUS_PRE_INIT = 3,	/**< In-between */
	CONFIG_STATUS_INIT = 4,		/**< Currently running MOD_INIT() */
	CONFIG_STATUS_RUN_CONFIG = 5,	/**< Currently running CONFIG_RUN hooks */
	CONFIG_STATUS_LOAD = 6,		/**< Currently running MOD_LOAD() */
	CONFIG_STATUS_POSTLOAD = 7,	/**< Doing post-load stuff like activating listeners */
	CONFIG_STATUS_COMPLETE = 8,	/**< Load or rehash complete */
	CONFIG_STATUS_ROLLBACK = 99,	/**< Configuration failed, rolling back changes */
} ConfigStatus;

struct LoopStruct {
	unsigned do_garbage_collect : 1;
	unsigned config_test : 1;
	unsigned booted : 1;
	unsigned forked : 1;
	unsigned do_bancheck : 1; /* perform *line bancheck? */
	unsigned do_bancheck_spamf_user : 1; /* perform 'user' spamfilter bancheck */
	unsigned do_bancheck_spamf_away : 1; /* perform 'away' spamfilter bancheck */
	unsigned terminating : 1;
	unsigned config_load_failed : 1;
	unsigned rehash_download_busy : 1; /* don't return "all downloads complete", needed for race condition */
	unsigned tainted : 1;
	int rehashing;
	ConfigStatus config_status;
	Client *rehash_save_client;
	void (*boot_function)();
};

/** Matching types for Match.type */
typedef enum {
	MATCH_SIMPLE=1, /**< Simple pattern with * and ? */
	MATCH_PCRE_REGEX=2, /**< PCRE2 Perl-like regex (new) */
} MatchType;

/** Match struct, which allows various matching styles, see MATCH_* */
typedef struct Match {
	char *str; /**< Text of the glob/regex/whatever. Always set. */
	MatchType type;
	union {
		pcre2_code *pcre2_expr; /**< PCRE2 Perl-like Regex */
	} ext;
} Match;

typedef struct Whowas {
	int  hashv;
	char *name;
	char *username;
	char *hostname;
	char *virthost;
	char *ip;
	char *servername;
	char *realname;
	char *account;
	long umodes;
	time_t   logoff;
	struct Client *online;	/* Pointer to new nickname for chasing or NULL */
	struct Whowas *next;	/* for hash table... */
	struct Whowas *prev;	/* for hash table... */
	struct Whowas *cnext;	/* for client struct linked list */
	struct Whowas *cprev;	/* for client struct linked list */
} WhoWas;

typedef struct SWhois SWhois;
struct SWhois {
	SWhois *prev, *next;
	int priority;
	char *line;
	char *setby;
};

/** The command API - used by modules and the core to add commands, overrides, etc.
 * See also https://www.unrealircd.org/docs/Dev:Command_API for a higher level overview and example.
 * @defgroup CommandAPI Command API
 * @{
 */
/** Command can be called by unregistered users (still in handshake) */
#define CMD_UNREGISTERED	0x0001
/** Command can be called by users (either directly connected, or remote) */
#define CMD_USER		0x0002
/** Command can be called by servers */
#define CMD_SERVER		0x0004
/** Command can be used by shunned users (only very few commands need this) */
#define CMD_SHUN		0x0008
/** Command will NOT add fake lag (extremely rare, use with care) */
#define CMD_NOLAG		0x0010
/** Command is actually an alias */
#define CMD_ALIAS		0x0020
/** Command will reset the idle time (only for PRIVMSG) */
#define CMD_RESETIDLE		0x0040
/** Command can be used by virus tagged users (only very few commands) */
#define CMD_VIRUS		0x0080
/** Command requires IRCOp privileges */
#define CMD_OPER		0x0200
/** Command is for control channel only (unrealircd.ctl socket) */
#define CMD_CONTROL		0x0400

/** Command function - used by all command handlers.
 * This is used in the code like <pre>CMD_FUNC(cmd_yourcmd)</pre> as a function definition.
 * It allows UnrealIRCd devs to change the parameters in the function without
 * (necessarily) breaking your code.
 * @param client      The client
 * @param recv_mtags  Received message tags for this command.
 * @param parc        Parameter count *plus* 1.
 * @param parv        Parameter values.
 * @note  Slightly confusing, but parc will be 2 if 1 parameter was provided.
 *        It is two because parv will still have 2 elements, parv[1] will be your first parameter,
 *        and parv[2] will be NULL.
 *        Note that reading parv[parc] and beyond is OUT OF BOUNDS and will cause a crash.
 *        E.g. parv[3] in the above example is out of bounds.
 */
#define CMD_FUNC(x) void (x) (Client *client, MessageTag *recv_mtags, int parc, const char *parv[])

/** Call a command function - can be useful if you are calling another command function in your own module.
 * For example in cmd_nick() we call cmd_nick_local() for local functions,
 * and then we can just use CALL_CMD_FUNC(cmd_nick_local); and don't have
 * to bother with passing the right command arguments. Which is nice because
 * command arguments may change in future UnrealIRCd versions.
 */
#define CALL_CMD_FUNC(x)	(x)(client, recv_mtags, parc, parv)

/** @} */

/** Command override function - used by all command override handlers.
 * This is used in the code like <pre>CMD_OVERRIDE_FUNC(ovr_somecmd)</pre> as a function definition.
 * @param ovr         The command override structure.
 * @param cptr        The client direction pointer.
 * @param client        The source client pointer (you usually need this one).
 * @param recv_mtags  Received message tags for this command.
 * @param parc        Parameter count *plus* 1.
 * @param parv        Parameter values.
 * @note  Slightly confusing, but parc will be 2 if 1 parameter was provided.
 *        It is two because parv will still have 2 elements, parv[1] will be your first parameter,
 *        and parv[2] will be NULL.
 *        Note that reading parv[parc] and beyond is OUT OF BOUNDS and will cause a crash.
 *        E.g. parv[3] in the above example.
 */
#define CMD_OVERRIDE_FUNC(x) void (x)(CommandOverride *ovr, Client *client, MessageTag *recv_mtags, int parc, const char *parv[])



typedef void (*CmdFunc)(Client *client, MessageTag *mtags, int parc, const char *parv[]);
typedef void (*AliasCmdFunc)(Client *client, MessageTag *mtags, int parc, const char *parv[], const char *cmd);
typedef void (*OverrideCmdFunc)(CommandOverride *ovr, Client *client, MessageTag *mtags, int parc, const char *parv[]);

#include <sodium.h>

/* This is the 'chunk size', the size of encryption blocks.
 * We choose 4K here since that is a decent amount as of 2021 and
 * more would not benefit performance anyway.
 * Note that you cannot change this value easily afterwards
 * (you cannot read files with a different chunk size).
 */
#define UNREALDB_CRYPT_FILE_CHUNK_SIZE 4096

/** The salt length. Don't change. */
#define UNREALDB_SALT_LEN 16

/** Database modes of operation (read or write)
 * @ingroup UnrealDBFunctions
 */
typedef enum UnrealDBMode {
	UNREALDB_MODE_READ = 0,
	UNREALDB_MODE_WRITE = 1
} UnrealDBMode;

typedef enum UnrealDBCipher {
	UNREALDB_CIPHER_XCHACHA20 = 0x0001
} UnrealDBCipher;

typedef enum UnrealDBKDF {
	UNREALDB_KDF_ARGON2ID = 0x0001
} UnrealDBKDF;

/** Database configuration for a particular file */
typedef struct UnrealDBConfig {
	uint16_t kdf;					/**< Key derivation function (always 0x01) */
	uint16_t t_cost;				/**< Time cost (number of rounds) */
	uint16_t m_cost; 				/**< Memory cost (in number of bitshifts, eg 15 means 1<<15=32M) */
	uint16_t p_cost;				/**< Parallel cost (number of concurrent threads) */
	uint16_t saltlen;				/**< Length of the salt (normally UNREALDB_SALT_LEN) */
	char *salt;					/**< Salt */
	uint16_t cipher;				/**< Encryption cipher (always 0x01) */
	uint16_t keylen;				/**< Key length */
	char *key;					/**< The key used for encryption/decryption */
} UnrealDBConfig;

/** Error codes returned by @ref UnrealDBFunctions
 * @ingroup UnrealDBFunctions
 */
typedef enum UnrealDBError {
	UNREALDB_ERROR_SUCCESS = 0,			/**< Success, not an error */
	UNREALDB_ERROR_FILENOTFOUND = 1,		/**< File does not exist */
	UNREALDB_ERROR_CRYPTED = 2,			/**< File is crypted but no password provided */
	UNREALDB_ERROR_NOTCRYPTED = 3,			/**< File is not crypted and a password was provided */
	UNREALDB_ERROR_HEADER = 4,			/**< Header is corrupt, invalid or unknown format */
	UNREALDB_ERROR_SECRET = 5,			/**< Invalid secret { } block provided - either does not exist or does not meet requirements */
	UNREALDB_ERROR_PASSWORD = 6,			/**< Invalid password provided */
	UNREALDB_ERROR_IO = 7,				/**< I/O error */
	UNREALDB_ERROR_API = 8,				/**< API call violation, eg requesting to write on a file opened for reading */
	UNREALDB_ERROR_INTERNAL = 9,			/**< Internal error, eg crypto routine returned something unexpected */
} UnrealDBError;

/** Database handle
 * This is returned by unrealdb_open() and used by all other @ref UnrealDBFunctions
 * @ingroup UnrealDBFunctions
 */
typedef struct UnrealDB {
	FILE *fd;					/**< File descriptor */
	UnrealDBMode mode;				/**< UNREALDB_MODE_READ / UNREALDB_MODE_WRITE */
	int crypted;					/**< Are we doing any encryption or just plaintext? */
	uint64_t creationtime;				/**< When this file was created/updates */
	crypto_secretstream_xchacha20poly1305_state st; /**< Internal state for crypto engine */
	char buf[UNREALDB_CRYPT_FILE_CHUNK_SIZE];	/**< Buffer used for reading/writing */
	int buflen;					/**< Length of current data in buffer */
	UnrealDBError error_code;			/**< Last error code. Whenever this happens we will set this, never overwrite, and block further I/O */
	char *error_string;				/**< Error string upon failure */
	UnrealDBConfig *config;				/**< Config */
} UnrealDB;

/** Used for speeding up reading/writing of DBs (so we don't have to run argon2 repeatedly) */
typedef struct SecretCache SecretCache;
struct SecretCache {
	SecretCache *prev, *next;
	UnrealDBConfig *config;
	time_t cache_hit;
};

/** Used for storing secret { } blocks */
struct Secret {
	Secret *prev, *next;
	char *name;
	char *password;
	SecretCache *cache;
};


/* tkl:
 *   TKL_KILL|TKL_GLOBAL 	= Global K-Line (GLINE)
 *   TKL_ZAP|TKL_GLOBAL		= Global Z-Line (ZLINE)
 *   TKL_KILL			= Local K-Line
 *   TKL_ZAP			= Local Z-Line
 */
#define TKL_KILL		0x00000001
#define TKL_ZAP			0x00000002
#define TKL_GLOBAL		0x00000004
#define TKL_SHUN		0x00000008
#define TKL_SPAMF		0x00000020
#define TKL_NAME		0x00000040
#define TKL_EXCEPTION		0x00000080
/* these are not real tkl types, but only used for exceptions: */
#define TKL_BLACKLIST		0x0001000
#define TKL_CONNECT_FLOOD	0x0002000
#define TKL_MAXPERIP		0x0004000
#define TKL_HANDSHAKE_DATA_FLOOD	0x0008000
#define TKL_ANTIRANDOM          0x0010000
#define TKL_ANTIMIXEDUTF8       0x0020000
#define TKL_BAN_VERSION         0x0040000

#define TKLIsServerBan(tkl)		((tkl)->type & (TKL_KILL|TKL_ZAP|TKL_SHUN))
#define TKLIsServerBanType(tpe)		((tpe) & (TKL_KILL|TKL_ZAP|TKL_SHUN))
#define TKLIsSpamfilter(tkl)		((tkl)->type & TKL_SPAMF)
#define TKLIsSpamfilterType(tpe)	((tpe) & TKL_SPAMF)
#define TKLIsNameBan(tkl)		((tkl)->type & TKL_NAME)
#define TKLIsNameBanType(tpe)		((tpe) & TKL_NAME)
#define TKLIsBanException(tkl)		((tkl)->type & TKL_EXCEPTION)
#define TKLIsBanExceptionType(tpe)	((tpe) & TKL_EXCEPTION)

#define SPAMF_CHANMSG		0x0001 /* c */
#define SPAMF_USERMSG		0x0002 /* p */
#define SPAMF_USERNOTICE	0x0004 /* n */
#define SPAMF_CHANNOTICE	0x0008 /* N */
#define SPAMF_PART		0x0010 /* P */
#define SPAMF_QUIT		0x0020 /* q */
#define SPAMF_DCC		0x0040 /* d */
#define SPAMF_USER		0x0080 /* u */
#define SPAMF_AWAY		0x0100 /* a */
#define SPAMF_TOPIC		0x0200 /* t */
#define SPAMF_MTAG		0x0400 /* m */

/* Other flags only for function calls: */
#define SPAMFLAG_NOWARN		0x0001

/* Ban actions. These must be ordered by severity (!) */
typedef enum BanAction {
	BAN_ACT_GZLINE		=1100,
	BAN_ACT_GLINE		=1000,
	BAN_ACT_SOFT_GLINE	= 950,
	BAN_ACT_ZLINE		= 900,
	BAN_ACT_KLINE		= 800,
	BAN_ACT_SOFT_KLINE	= 850,
	BAN_ACT_SHUN		= 700,
	BAN_ACT_SOFT_SHUN	= 650,
	BAN_ACT_KILL		= 600,
	BAN_ACT_SOFT_KILL	= 550,
	BAN_ACT_TEMPSHUN	= 500,
	BAN_ACT_SOFT_TEMPSHUN	= 450,
	BAN_ACT_VIRUSCHAN	= 400,
	BAN_ACT_SOFT_VIRUSCHAN	= 350,
	BAN_ACT_DCCBLOCK	= 300,
	BAN_ACT_SOFT_DCCBLOCK	= 250,
	BAN_ACT_BLOCK		= 200,
	BAN_ACT_SOFT_BLOCK	= 150,
	BAN_ACT_WARN		= 100,
	BAN_ACT_SOFT_WARN	=  50,
} BanAction;

#define IsSoftBanAction(x)   ((x == BAN_ACT_SOFT_GLINE) || (x == BAN_ACT_SOFT_KLINE) || \
                              (x == BAN_ACT_SOFT_SHUN) || (x == BAN_ACT_SOFT_KILL) || \
                              (x == BAN_ACT_SOFT_TEMPSHUN) || (x == BAN_ACT_SOFT_VIRUSCHAN) || \
                              (x == BAN_ACT_SOFT_DCCBLOCK) || (x == BAN_ACT_SOFT_BLOCK) || \
                              (x == BAN_ACT_SOFT_WARN))


/** Server ban sub-struct of TKL entry (KLINE/GLINE/ZLINE/GZLINE/SHUN) */
struct ServerBan {
	char *usermask; /**< User mask */
	char *hostmask; /**< Host mask */
	unsigned short subtype; /**< See TKL_SUBTYPE_* */
	char *reason; /**< Reason */
};

/* Name ban sub-struct of TKL entry (QLINE) */
struct NameBan {
	char hold; /**< nickname hold is used by services */
	char *name; /**< the nick or channel that is banned */
	char *reason; /**< Reason */
};

/** Spamfilter sub-struct of TKL entry (Spamfilter) */
struct Spamfilter {
	unsigned short target;
	BanAction action; /**< Ban action, see BAN_ACT* */
	Match *match; /**< Spamfilter matcher */
	char *tkl_reason; /**< Reason to use for bans placed by this spamfilter, escaped by unreal_encodespace(). */
	time_t tkl_duration; /**< Duration of bans placed by this spamfilter */
};

/** Ban exception sub-struct of TKL entry (ELINE) */
struct BanException {
	char *usermask; /**< User mask */
	char *hostmask; /**< Host mask */
	SecurityGroup *match; /**< Security group (for config file items only) */
	unsigned short subtype; /**< See TKL_SUBTYPE_* */
	char *bantypes; /**< Exception types */
	char *reason; /**< Reason */
};


#define TKL_SUBTYPE_NONE	0x0000
#define TKL_SUBTYPE_SOFT	0x0001 /* (require SASL) */

#define TKL_FLAG_CONFIG		0x0001 /* Entry from configuration file. Cannot be removed by using commands. */

/** A TKL entry, such as a KLINE, GLINE, Spamfilter, QLINE, Exception, .. */
struct TKL {
	TKL *prev, *next;
	unsigned int type; /**< TKL type. One of TKL_*, such as TKL_KILL|TKL_GLOBAL for gline */
	unsigned short flags; /**< One of TKL_FLAG_*, such as TKL_FLAG_CONFIG */
	char *set_by; /**< By who was this entry added */
	time_t set_at; /**< When this entry was added */
	time_t expire_at; /**< When this entry will expire */
	union {
		Spamfilter *spamfilter;
		ServerBan *serverban;
		NameBan *nameban;
		BanException *banexception;
	} ptr;
};

/** A spamfilter except entry */
struct SpamExcept {
	SpamExcept *prev, *next;
	char name[1];
};

/** IRC Counts, used for /LUSERS */
typedef struct IRCCounts IRCCounts;
struct IRCCounts {
	int clients;		/* total */
	int invisible;		/* invisible */
	int servers;		/* servers */
	int operators;		/* operators */
	int unknown;		/* unknown local connections */
	int channels;		/* channels */
	int me_clients;		/* my clients */
	int me_servers;		/* my servers */
	int me_max;		/* local max */
	int global_max;		/* global max */
};

/** The /LUSERS stats information */
extern MODVAR IRCCounts irccounts;

typedef struct NameValue NameValue;
/** Name and value list used in a static array, such as in conf.c */
struct NameValue
{
	long value;
	char *name;
};

/** Name and value list used in dynamic linked lists */
typedef struct NameValueList NameValueList;
struct NameValueList {
	NameValueList *prev, *next;
	char *name;
	char *value;
};

typedef struct NameValuePrioList NameValuePrioList;
struct NameValuePrioList {
	NameValuePrioList *prev, *next;
	int priority;
	char *name;
	char *value;
};

#include "modules.h"

/** A "real" command (internal interface, not for modules) */
struct RealCommand {
	RealCommand		*prev, *next;
	char 			*cmd;
	CmdFunc			func;
	AliasCmdFunc		aliasfunc;
	int			flags;
	unsigned int    	count;
	unsigned		parameters : 5;
	unsigned long   	bytes;
	Module 			*owner;
	RealCommand		*friend; /* cmd if token, token if cmd */
	CommandOverride		*overriders;
#ifdef DEBUGMODE
	unsigned long 		lticks;
	unsigned long 		rticks;
#endif
};

/** A command override */
struct CommandOverride {
	CommandOverride		*prev, *next;
	int			priority;
	Module			*owner;
	RealCommand		*command;
	OverrideCmdFunc		func;
};

extern MODVAR Umode *usermodes;
extern MODVAR Cmode *channelmodes;

extern Umode *UmodeAdd(Module *module, char ch, int options, int unset_on_deoper, int (*allowed)(Client *client, int what), long *mode);
extern void UmodeDel(Umode *umode);

extern Cmode *CmodeAdd(Module *reserved, CmodeInfo req, Cmode_t *mode);
extern void CmodeDel(Cmode *cmode);

extern void moddata_init(void);
extern ModDataInfo *ModDataAdd(Module *module, ModDataInfo req);
extern void ModDataDel(ModDataInfo *md);
extern void unload_all_unused_moddata(void);

#define LISTENER_NORMAL			0x000001
#define LISTENER_CLIENTSONLY		0x000002
#define LISTENER_SERVERSONLY		0x000004
#define LISTENER_TLS			0x000010
#define LISTENER_BOUND			0x000020
#define LISTENER_DEFER_ACCEPT		0x000040
#define LISTENER_CONTROL		0x000080	/**< Control channel */
#define LISTENER_NO_CHECK_CONNECT_FLOOD	0x000100	/**< Don't check for connect-flood and max-unknown-connections-per-ip (eg for RPC) */
#define LISTENER_NO_CHECK_ZLINED	0x000200	/**< Don't check for zlines */

#define IsServersOnlyListener(x)	((x) && ((x)->options & LISTENER_SERVERSONLY))

#define CONNECT_TLS		0x000001
#define CONNECT_AUTO		0x000002
#define CONNECT_QUARANTINE	0x000004
#define CONNECT_INSECURE	0x000008

#define TLSFLAG_FAILIFNOCERT 		0x0001
#define TLSFLAG_NOSTARTTLS		0x0002
#define TLSFLAG_DISABLECLIENTCERT	0x0004

/** Flood counters for local clients */
typedef struct FloodCounter {
	int count;
	long t;
} FloodCounter;

/** This is the list of different flood counters that we keep for local clients. */
/* IMPORTANT: If you change this, update floodoption_names[] in src/user.c too !!!!!!!!!!!! */
typedef enum FloodOption {
	FLD_NICK		= 0,	/**< nick-flood */
	FLD_JOIN		= 1,	/**< join-flood */
	FLD_AWAY		= 2,	/**< away-flood */
	FLD_INVITE		= 3,	/**< invite-flood */
	FLD_KNOCK		= 4,	/**< knock-flood */
	FLD_CONVERSATIONS	= 5,	/**< max-concurrent-conversations */
	FLD_LAG_PENALTY		= 6,	/**< lag-penalty / lag-penalty-bytes */
	FLD_VHOST		= 7,	/**< vhost-flood */
} FloodOption;
#define MAXFLOODOPTIONS 10

typedef struct TrafficStats TrafficStats;
struct TrafficStats {
	long long messages_sent;	/* IRC lines sent */
	long long messages_received;	/* IRC lines received */
	long long bytes_sent;		/* Bytes sent */
	long long bytes_received;	/* Received bytes */
};

/** Socket type (IPv4, IPv6, UNIX) */
typedef enum {
	SOCKET_TYPE_IPV4=0, SOCKET_TYPE_IPV6=1, SOCKET_TYPE_UNIX=2
} SocketType;

/** This shows the Client struct (any client), the User struct (a user), Server (a server) that are commonly accessed both in the core and by 3rd party coders.
 * @defgroup CommonStructs Common structs
 * @{
 */

/** A client on this or a remote server - can be a user, server, unknown, etc..
 */
struct Client {
	struct list_head client_node;		/**< For global client list (client_list) */
	struct list_head lclient_node;		/**< For local client list (lclient_list) */
	struct list_head special_node;		/**< For special lists (server || unknown || oper) */
	LocalClient *local;			/**< Additional information regarding locally connected clients */
	User *user;				/**< Additional information, if this client is a user */
	Server *server;				/**< Additional information, if this is a server */
	RPCClient *rpc;				/**< RPC Client, or NULL */
	ClientStatus status;			/**< Client status, one of CLIENT_STATUS_* */
	struct list_head client_hash;		/**< For name hash table (clientTable) */
	char name[HOSTLEN + 1];			/**< Unique name of the client: nickname for users, hostname for servers */
	time_t lastnick;			/**< Timestamp on nick */
	uint64_t flags;				/**< Client flags (one or more of CLIENT_FLAG_*) */
	long umodes;				/**< Client usermodes (if user) */
	Client *direction;			/**< Direction from which this client originated.
	                                             This always points to a directly connected server or &me.
	                                             It is never NULL */
	unsigned char hopcount;			/**< Number of servers to this, 0 means local client */
	char ident[USERLEN + 1];		/**< Ident of the user, if available. Otherwise set to "unknown". */
	char info[REALLEN + 1];			/**< Additional client information text. For users this is gecos/realname */
	char id[IDLEN + 1];			/**< Unique ID: SID or UID */
	struct list_head id_hash;		/**< For UID/SID hash table (idTable) */
	Client *uplink;				/**< Server on where this client is connected to (can be &me) */
	char *ip;				/**< IP address of user or server (never NULL) */
	ModData moddata[MODDATA_MAX_CLIENT];	/**< Client attached module data, used by the ModData system */
};

/** Local client information, use client->local to access these (see also @link Client @endlink).
 */
struct LocalClient {
	int fd;				/**< File descriptor, can be <0 if socket has been closed already. */
	SocketType socket_type;		/**< Type of socket: IPv4, IPV6, UNIX */
	SSL *ssl;			/**< OpenSSL/LibreSSL struct for TLS connection */
	time_t fake_lag;		/**< Time when user will next be allowed to send something (actually fake_lag<currenttime+10) */
	int fake_lag_msec;		/**< Used for calculating 'fake_lag' penalty (modulo) */
	time_t creationtime;		/**< Time user was created (connected on IRC) */
	time_t last_msg_received;	/**< Last time any message was received */
	dbuf sendQ;			/**< Outgoing send queue (data to be sent) */
	dbuf recvQ;			/**< Incoming receive queue (incoming data yet to be parsed) */
	ConfigItem_class *class;	/**< The class { } block associated to this client */
	int proto;			/**< PROTOCTL options */
	long caps;			/**< User: enabled capabilities (via CAP command) */
	time_t nexttarget;		/**< Next time that a new target will be allowed (msg/notice/invite) */
	u_char targets[MAXCCUSERS];	/**< Hash values of targets for target limiting */
	ConfigItem_listen *listener;	/**< If this client IsListening() then this is the listener configuration attached to it */
	long serial;			/**< Current serial number for send.c functions (to avoid sending duplicate messages) */
	time_t next_nick_allowed;		/**< Time the next nick change will be allowed */
	time_t idle_since;		/**< Last time a RESETIDLE message was received (PRIVMSG) */
	TrafficStats traffic;		/**< Traffic statistics */
	ModData moddata[MODDATA_MAX_LOCAL_CLIENT];	/**< LocalClient attached module data, used by the ModData system */
	char *error_str;		/**< Quit reason set by dead_socket() in case of socket/buffer error, later used by exit_client() */
	char sasl_agent[NICKLEN + 1];	/**< SASL: SASL Agent the user is interacting with */
	unsigned char sasl_out;		/**< SASL: Number of outgoing sasl messages */
	unsigned char sasl_complete;	/**< SASL: >0 if SASL authentication was successful */
	time_t sasl_sent_time;		/**< SASL: 0 or the time that the (last) AUTHENTICATE command has been sent */
	char *sni_servername;		/**< Servername as sent by client via SNI (Server Name Indication) in TLS, otherwise NULL */
	int cap_protocol;		/**< CAP protocol in use. At least 300 for any CAP capable client. 302 for 3.2, etc.. */
	uint32_t nospoof;		/**< Anti-spoofing random number (used in user handshake PING/PONG) */
	char *passwd;			/**< Password used during connect, if any (freed once connected and set to NULL) */
	int authfd;			/**< File descriptor for ident checking (RFC931) */
	int identbufcnt;		/**< Counter for 'ident' reading code */
	struct hostent *hostp;		/**< Host record for this client (used by DNS code) */
	char sockhost[HOSTLEN + 1];	/**< Hostname from the socket */
	u_short port;			/**< Remote TCP port of client */
	FloodCounter flood[MAXFLOODOPTIONS];
	RPCClient *rpc;			/**< RPC Client, or NULL */
};

/** User information (persons, not servers), you use client->user to access these (see also @link Client @endlink).
 */
struct User {
	Membership *channel;		/**< Channels that the user is in (linked list) */
	Link *dccallow;			/**< DCCALLOW list (linked list) */
	char account[ACCOUNTLEN + 1];	/**< Services account name or ID (SVID) - use IsLoggedIn(client) to check if logged in */
	int joined;			/**< Number of channels joined */
	char username[USERLEN + 1];	/**< Username, the user portion in nick!user@host. */
	char realhost[HOSTLEN + 1];	/**< Realhost, the real host of the user (IP or hostname) - usually this is not shown to other users */
	char cloakedhost[HOSTLEN + 1];	/**< Cloaked host - generated by cloaking algorithm */
	char *virthost;			/**< Virtual host - when user has user mode +x this is the active host */
	char *server;			/**< Server name the user is on (?) */
	SWhois *swhois;			/**< Special "additional" WHOIS entries such as "a Network Administrator" */
	WhoWas *whowas;			/**< Something for whowas :D :D */
	char *snomask;			/**< Server Notice Mask (snomask) - only for IRCOps */
	char *operlogin;		/**< Which oper { } block was used to oper up, otherwise NULL - used for auditting and by oper::maxlogins */
	char *away;			/**< AWAY message, or NULL if not away */
	time_t away_since;		/**< Last time the user went AWAY */
};

/** Server information (local servers and remote servers), you use client->server to access these (see also @link Client @endlink).
 */
struct Server {
	char by[NICKLEN + 1];		/**< Uhhhh - who activated this connection - AGAIN? */
	ConfigItem_link *conf;		/**< link { } block associated with this server, or NULL */
	long users;			/**< Number of users on this server */
	time_t boottime;		/**< Startup time of server (boot time) */
	struct {
		unsigned synced:1;	/**< Server synchronization finished? (3.2beta18+) */
		unsigned server_sent:1;	/**< SERVER message sent to this link? (for outgoing links) */
	} flags;
	struct {
		char *usermodes;	/**< Usermodes that this server knows about */
		char *chanmodes[4];	/**< Channel modes that this server knows (in 4 groups, like CHANMODES= in ISUPPORT/005) */
		int protocol;		/**< Link-protocol version */
		char *software;		/**< Name of the software (eg: unrealircd-X.Y.Z) */
		char *nickchars;	/**< Nick character sets active on this server) */
	} features;
};

/** @} */

typedef struct RPCClient RPCClient;
/** RPC Client information */
struct RPCClient {
	char *rpc_user; /**< Name of the rpc-user block after authentication, NULL during pre-auth */
	char *issuer; /**< Optional name of the issuer, set by rpc.set_issuer(), eg logged in user on admin panel, can be NULL */
	json_t *rehash_request; /**< If a REHASH (request) is currently running, otherwise NULL */
};

struct MessageTag {
	MessageTag *prev, *next;
	char *name;
	char *value;
};

/* conf preprocessor */
typedef enum PreprocessorItem {
	PREPROCESSOR_ERROR		= 0,
	PREPROCESSOR_DEFINE		= 1,
	PREPROCESSOR_IF			= 2,
	PREPROCESSOR_ENDIF		= 3
} PreprocessorItem;

typedef enum PreprocessorPhase {
	PREPROCESSOR_PHASE_INITIAL	= 1,
	PREPROCESSOR_PHASE_SECONDARY	= 2,
	PREPROCESSOR_PHASE_MODULE	= 3
} PreprocessorPhase;

typedef enum AuthenticationType {
	AUTHTYPE_INVALID		= -1,
	AUTHTYPE_PLAINTEXT		= 0,
	AUTHTYPE_TLS_CLIENTCERT		= 1,
	AUTHTYPE_TLS_CLIENTCERTFP	= 2,
	AUTHTYPE_SPKIFP			= 3,
	AUTHTYPE_UNIXCRYPT		= 4,
	AUTHTYPE_BCRYPT			= 5,
	AUTHTYPE_ARGON2			= 6,
} AuthenticationType;

typedef struct AuthConfig AuthConfig;
/** Authentication Configuration - this can be a password or
 * other authentication method that was parsed from the
 * configuration file.
 */
struct AuthConfig {
	AuthenticationType	type;  /**< Type of data, one of AUTHTYPE_* */
	char			*data; /**< Data associated with this record */
};

#ifndef HAVE_CRYPT
#define crypt DES_crypt
#endif

/* CRULE stuff */

#define CRULE_ALL		0
#define CRULE_AUTO		1

/* some constants and shared data types */
#define CR_MAXARGLEN 80         /**< Maximum arg length (must be > HOSTLEN) */
#define CR_MAXARGS 3            /**< Maximum number of args for a rule */

/** Evaluation function for a connection rule. */
typedef int (*crule_funcptr) (int, void **);

/** CRULE - Node in a connection rule tree. */
struct CRuleNode {
  crule_funcptr funcptr; /**< Evaluation function for this node. */
  int numargs;           /**< Number of arguments. */
  void *arg[CR_MAXARGS]; /**< Array of arguments.  For operators, each arg
                            is a tree element; for functions, each arg is
                            a string. */
};
typedef struct CRuleNode CRuleNode;
typedef struct CRuleNode* CRuleNodePtr;


/*
 * conf2 stuff -stskeeps
*/

typedef enum ConfigIfCondition { IF_DEFINED=1, IF_VALUE=2, IF_MODULE=3} ConfigIfCondition;

struct ConditionalConfig
{
	ConditionalConfig *prev, *next;
	int priority; /**< Preprocessor level. Starts with 1, then 2, 3, .. */
	ConfigIfCondition condition; /**< See ConfigIfCondition, one of: IF_* */
	int negative; /**< For ! conditions */
	char *name; /**< Name of the variable or module */
	char *opt; /**< Only for IF_VALUE */
};

/** Configuration file (config parser) */
struct ConfigFile
{
	char *filename;		/**< Filename of configuration file */
	ConfigEntry *items;	/**< All items in the configuration file */
	ConfigFile *next;	/**< Next configuration file */
};

/** Configuration entry (config parser) */
struct ConfigEntry
{
	char *name;			/**< Variable name */
	char *value;			/**< Variable value, can be NULL */
	ConfigEntry *next;		/**< Next ConfigEntry */
	ConfigEntry *items;		/**< Items (children), can be NULL */
	ConfigFile *file;		/**< To which configfile does this belong? */
	int line_number;		/**< Line number of the variable name (this one is usually used for errors) */
	int file_position_start;	/**< Position (byte) within configuration file of the start of the block, rarely used */
	int file_position_end;		/**< Position (byte) within configuration file of the end of the block, rarely used */
	int section_linenumber;		/**< Line number of the section (only used internally for parse errors) */
	ConfigEntry *parent;		/**< Parent item, can be NULL */
	ConditionalConfig *conditional_config;	/**< Used for conditional config by the main parser */
	unsigned escaped:1;
};

struct ConfigFlag 
{
	unsigned	temporary : 1;
	unsigned	permanent : 1;
};

/* configflag specialized for except socks/ban -Stskeeps */

struct ConfigFlag_except
{
	unsigned	temporary : 1;
	unsigned	type	  : 4;
};

struct ConfigFlag_ban
{
	unsigned	temporary : 1;
	unsigned	type	  : 4;
	unsigned	type2	  : 2;
};

struct ConfigFlag_tld
{
	unsigned	temporary : 1;
	unsigned	motdptr   : 1;
	unsigned	ruleclient  : 1;
};

#define CONF_BAN_SERVER          1
#define CONF_BAN_VERSION         2
#define CONF_BAN_REALNAME        3

#define CONF_BAN_TYPE_CONF	0
#define CONF_BAN_TYPE_AKILL	1
#define CONF_BAN_TYPE_TEMPORARY 2

struct ConfigItem {
	ConfigItem *prev, *next;
	ConfigFlag flag;
};

struct ConfigItem_me {
	char	   *name, *info, *sid;
};

struct ConfigItem_files {
	char	*motd_file, *rules_file, *smotd_file;
	char	*botmotd_file, *opermotd_file, *svsmotd_file;
	char	*pid_file, *tune_file;
};

struct ConfigItem_admin {
	ConfigItem_admin *prev, *next;
	ConfigFlag flag;
	char	   *line; 
};

#define CLASS_OPT_NOFAKELAG		0x1

struct ConfigItem_class {
	ConfigItem_class *prev, *next;
	ConfigFlag flag;
	char	   *name;
	int	   pingfreq, connfreq, maxclients, sendq, recvq, clients;
	int xrefcount; /* EXTRA reference count, 'clients' also acts as a reference count but
	                * link blocks also refer to classes so a 2nd ref. count was needed.
	                */
	unsigned int options;
};

struct ConfigFlag_allow {
	unsigned	noident :1;
	unsigned	useip :1;
	unsigned	tls :1;
	unsigned	reject_on_auth_failure :1;
};

/** allow { } block settings */
struct ConfigItem_allow {
	ConfigItem_allow *prev, *next;
	ConfigFlag flag;
	SecurityGroup *match;
	char *server;
	AuthConfig *auth;
	int maxperip; /**< Maximum connections permitted per IP address (locally) */
	int global_maxperip; /**< Maximum connections permitted per IP address (globally) */
	int port;
	ConfigItem_class *class;
	ConfigFlag_allow flags;
	int ipv6_clone_mask;
};

struct OperClassACLPath
{
	OperClassACLPath *prev,*next;
	char *identifier;
};

struct OperClassACLEntryVar
{
        OperClassACLEntryVar *prev,*next;
        char *name;
        char *value;
};

struct OperClassACLEntry
{
        OperClassACLEntry *prev,*next;
        OperClassACLEntryVar *variables;
        OperClassEntryType type;
};

struct OperClassACL
{
        OperClassACL *prev,*next;
        char *name;
        OperClassACLEntry *entries;
        OperClassACL *acls;
};

struct OperClass
{
        char *ISA;
        char *name;
        OperClassACL *acls;
};

struct OperClassCheckParams
{
        Client *client;
        Client *victim;
        Channel *channel;
        const void *extra;
};

struct ConfigItem_operclass {
	ConfigItem_operclass *prev, *next;
	OperClass *classStruct;
};

struct ConfigItem_oper {
	ConfigItem_oper *prev, *next;
	ConfigFlag flag;
	char *name, *snomask;
	SWhois *swhois;
	AuthConfig *auth;
	char *operclass;
	ConfigItem_class *class;
	SecurityGroup *match;
	unsigned long modes, require_modes;
	char *vhost;
	int maxlogins;
	int server_notice_colors;
	int server_notice_show_event;
	int auto_login;
};

/** The TLS options that are used in set::tls and otherblocks::tls-options.
 * NOTE: If you add something here then you must also update the
 *       conf_tlsblock() function in s_conf.c to have it inherited
 *       from set::tls to the other config blocks!
 */
typedef struct TLSOptions TLSOptions;
struct TLSOptions {
	char *certificate_file;
	char *key_file;
	char *trusted_ca_file;
	unsigned int protocols;
	char *ciphers;
	char *ciphersuites;
	char *ecdh_curves;
	char *outdated_protocols;
	char *outdated_ciphers;
	long options;
	int renegotiate_bytes;
	int renegotiate_timeout;
	int sts_port;
	long sts_duration;
	int sts_preload;
};

struct ConfigItem_mask {
	ConfigItem_mask *prev, *next;
	ConfigFlag flag;
	char *mask;
};

struct ConfigItem_drpass {
	AuthConfig	 *restartauth;
	AuthConfig	 *dieauth;
};

struct ConfigItem_ulines {
	ConfigItem_ulines  *prev, *next;
	ConfigFlag 	 flag;
	char 		 *servername;
};

#define TLD_TLS		0x1
#define TLD_REMOTE	0x2

struct ConfigItem_tld {
	ConfigItem_tld 	*prev, *next;
	ConfigFlag_tld 	flag;
	SecurityGroup	*match;
	char 		*channel;
	char 		*motd_file, *rules_file, *smotd_file;
	char 		*botmotd_file, *opermotd_file;
	MOTDFile	rules, motd, smotd, botmotd, opermotd;
	u_short		options;
};

#define WEB_OPT_ENABLE	0x1

typedef enum HttpMethod {
	HTTP_METHOD_NONE = 0,	/**< No valid HTTP request (yet) */
	HTTP_METHOD_HEAD = 1,	/**< HEAD request */
	HTTP_METHOD_GET = 2,	/**< GET request */
	HTTP_METHOD_PUT = 3,	/**< PUT request */
	HTTP_METHOD_POST = 4,	/**< POST request */
} HttpMethod;

typedef enum TransferEncoding {
	TRANSFER_ENCODING_NONE=0,
	TRANSFER_ENCODING_CHUNKED=1
} TransferEncoding;

typedef struct WebRequest WebRequest;
struct WebRequest {
	HttpMethod method; /**< GET/PUT/POST */
	char *uri; /**< Requested resource, eg "/api" */
	NameValuePrioList *headers; /**< HTTP request headers */
	int num_headers; /**< Number of HTTP request headers (also used for sorting the list) */
	char request_header_parsed; /**< Done parsing? */
	char *lefttoparse; /**< Leftover buffer to parse */
	int lefttoparselen; /**< Length of lefttoparse buffer */
	int pending_close; /**< Set to 1 when connection should be closed as soon as all data is sent (sendq==0) */
	char *request_buffer; /**< Buffer for POST data */
	int request_buffer_size; /**< Size of buffer for POST data */
	int request_body_complete; /**< POST data has all been read */
	long long content_length; /**< "Content-Length" as sent by the client */
	long long chunk_remaining;
	TransferEncoding transfer_encoding;
	long long config_max_request_buffer_size; /**< CONFIG: Maximum request length allowed */
};

typedef struct WebServer WebServer;
struct WebServer {
	int (*handle_request)(Client *client, WebRequest *web);
	int (*handle_body)(Client *client, WebRequest *web, const char *buf, int length);
};

typedef enum WebSocketType {
	WEBSOCKET_TYPE_BINARY = 1,
	WEBSOCKET_TYPE_TEXT   = 2
} WebSocketType;

typedef struct WebSocketUser WebSocketUser;
struct WebSocketUser {
	char get; /**< GET initiated */
	char handshake_completed; /**< Handshake completed, use websocket frames */
	char *handshake_key; /**< Handshake key (used during handshake) */
	char *lefttoparse; /**< Leftover buffer to parse */
	int lefttoparselen; /**< Length of lefttoparse buffer */
	WebSocketType type; /**< WEBSOCKET_TYPE_BINARY or WEBSOCKET_TYPE_TEXT */
	char *sec_websocket_protocol; /**< Only valid during parsing of the request, after that it is NULL again */
	char *forwarded; /**< Unparsed `Forwarded:` header, RFC 7239 */
	int secure; /**< If there is a Forwarded header, this indicates if the remote connection is secure */
};

#define WEBSOCKET_MAGIC_KEY "258EAFA5-E914-47DA-95CA-C5AB0DC85B11" /* see RFC6455 */

/* Websocket operations: */
#define WSOP_CONTINUATION 0x00
#define WSOP_TEXT         0x01
#define WSOP_BINARY       0x02
#define WSOP_CLOSE        0x08
#define WSOP_PING         0x09
#define WSOP_PONG         0x0a

struct ConfigItem_listen {
	ConfigItem_listen *prev, *next;
	ConfigFlag flag;
	SocketType socket_type;		/**< Socket type, eg. SOCKET_TYPE_IPV4 or SOCKET_TYPE_UNIX */
	char *file;			/**< If the listener is a file, the full pathname */
	char *ip;			/**< IP bind address (if IP listener) */
	int port;			/**< Port to listen on (if IP listener) */
	int mode;			/**< Mode permissions (if file aka unix socket listener) */
	int options;			/**< e.g. LISTENER_BOUND if active */
	int clients;			/**< Clients connected to this socket / listener */
	int fd;				/**< File descriptor (if open), or -1 (if not open yet) */
	SSL_CTX *ssl_ctx;		/**< SSL/TLS context */
	TLSOptions *tls_options;	/**< SSL/TLS options */
	WebServer *webserver;		/**< For the webserver module */
	void (*start_handshake)(Client *client); /**< Function to call on accept() */
	int websocket_options;		/**< Websocket options (for the websocket module) */
	int rpc_options;		/**< For the RPC module */
	char *websocket_forward;	/**< For websocket module too */
};

struct ConfigItem_sni {
	ConfigItem_sni *prev, *next;
	ConfigFlag flag;
	char *name;
	SSL_CTX *ssl_ctx;
	TLSOptions *tls_options;
};

struct ConfigItem_vhost {
	ConfigItem_vhost 	*prev, *next;
	ConfigFlag 	flag;
	SecurityGroup	*match;
	char		*login, *virthost, *virtuser;
	SWhois *swhois;
	AuthConfig	*auth;
};

struct ConfigItem_link {
	ConfigItem_link	*prev, *next;
	ConfigFlag	flag;
	/* config options: */
	char *servername; /**< Name of the server ('link <servername> { }') */
	struct {
		SecurityGroup *match; /**< incoming mask(s) to accept */
	} incoming;
	struct {
		char *file; /**< UNIX domain socket to connect to */
		char *bind_ip; /**< Our IP to bind to when doing the connect */
		char *hostname; /**< Hostname or IP to connect to */
		int port; /**< Port to connect to */
		int options; /**< Connect options like tls or autoconnect */
	} outgoing;
	AuthConfig *auth; /**< authentication method (eg: password) */
	char *hub; /**< Hub mask */
	char *leaf; /**< Leaf mask */
	int leaf_depth; /**< Leaf depth */
	ConfigItem_class *class; /**< Class the server should use */
	int options; /**< Generic options such as quarantine */
	int verify_certificate;
	/* internal: */
	int	refcount; /**< Reference counter (used so we know if the struct may be freed) */
	time_t hold; /**< For how long the server is "on hold" for outgoing connects (why?) */
	char *connect_ip; /**< actual IP to use for outgoing connect (filled in after host is resolved) */
	SSL_CTX *ssl_ctx; /**< SSL Context for outgoing connection (optional) */
	TLSOptions *tls_options; /**< SSL Options for outgoing connection (optional) */
};

struct ConfigItem_ban {
	ConfigItem_ban	*prev, *next;
	ConfigFlag_ban	flag;
	char			*mask, *reason;
	unsigned short action;
};

struct ConfigItem_deny_dcc {
	ConfigItem_deny_dcc		*prev, *next;
	ConfigFlag_ban		flag;
	char			*filename, *reason;
};

struct ConfigItem_deny_version {
	ConfigItem_deny_version	*prev, *next;
	ConfigFlag		flag;
	char 			*mask, *version, *flags;
};

struct ConfigItem_deny_channel {
	ConfigItem_deny_channel		*prev, *next;
	ConfigFlag		flag;
	char			*channel, *reason, *redirect, *class;
	unsigned char	warn;
	SecurityGroup		*match;
};

struct ConfigItem_allow_channel {
	ConfigItem_allow_channel		*prev, *next;
	ConfigFlag		flag;
	char			*channel, *class;
	SecurityGroup		*match;
};

struct ConfigItem_allow_dcc {
	ConfigItem_allow_dcc		*prev, *next;
	ConfigFlag_ban	flag;
	char			*filename;
};

typedef enum { 
	ALIAS_SERVICES=1, ALIAS_STATS, ALIAS_NORMAL, ALIAS_COMMAND, ALIAS_CHANNEL, ALIAS_REAL
} AliasType;

struct ConfigItem_alias {
	ConfigItem_alias *prev, *next;
	ConfigFlag flag;
	ConfigItem_alias_format *format;
	char *alias, *nick;
	AliasType type;
	unsigned int spamfilter:1;
};

struct ConfigItem_alias_format {
	ConfigItem_alias_format *prev, *next;
	ConfigFlag flag;
	char *nick;
	AliasType type;
	char *format, *parameters;
	Match *expr;
};

#define RESOURCE_REMOTE     0x1
#define RESOURCE_DLQUEUED   0x2
#define RESOURCE_INCLUDE    0x4

typedef struct ConfigEntryWrapper ConfigEntryWrapper;
struct ConfigEntryWrapper {
	ConfigEntryWrapper *prev, *next;
	ConfigEntry *ce;
};
	
struct ConfigResource {
	ConfigResource *prev, *next;
	int type;
	ConfigEntryWrapper *wce; /**< The place(s) where this resource is begin used */
	char *file; /**< File to read: can be a conf/something file or a downloaded file */
	char *url; /**< URL, if it is an URL */
	char *cache_file; /**< Set to filename of local cached copy, if it is available */
};

struct ConfigItem_blacklist_module {
	ConfigItem_blacklist_module *prev, *next;
	char *name;
};

struct ConfigItem_help {
	ConfigItem_help *prev, *next;
	ConfigFlag flag;
	char *command;
	MOTDLine *text;
};

struct ConfigItem_offchans {
	ConfigItem_offchans *prev, *next;
	char name[CHANNELLEN+1];
	char *topic;
};

#define SECURITYGROUPLEN 48
struct SecurityGroup {
	SecurityGroup *prev, *next;
	int priority;
	char name[SECURITYGROUPLEN+1];
	NameValuePrioList *printable_list;
	int printable_list_counter;
	/* Include */
	int identified;
	int reputation_score;
	long connect_time;
	int webirc;
	int websocket;
	int tls;
	NameList *ip;
	ConfigItem_mask *mask;
	NameList *security_group;
	NameValuePrioList *extended;
	/* Exclude */
	int exclude_identified;
	int exclude_reputation_score;
	long exclude_connect_time;
	int exclude_webirc;
	int exclude_websocket;
	int exclude_tls;
	NameList *exclude_ip;
	ConfigItem_mask *exclude_mask;
	NameList *exclude_security_group;
	NameValuePrioList *exclude_extended;
};

#define HM_HOST 1
#define HM_IPV4 2
#define HM_IPV6 3

#define SETTER_NICK 0
#define SETTER_NICK_USER_HOST 1

/*
 * statistics structures
 */
typedef struct IRCStatistics IRCStatistics;
struct IRCStatistics {
	unsigned int is_cl;	/* number of client connections */
	unsigned int is_sv;	/* number of server connections */
	unsigned int is_ni;	/* connection but no idea who it was */
	time_t is_cti;		/* time spent connected by clients */
	time_t is_sti;		/* time spent connected by servers */
	unsigned int is_ac;	/* connections accepted */
	unsigned int is_ref;	/* accepts refused */
	unsigned int is_unco;	/* unknown commands */
	unsigned int is_wrdi;	/* command going in wrong direction */
	unsigned int is_unpf;	/* unknown prefix */
	unsigned int is_empt;	/* empty message */
	unsigned int is_num;	/* numeric message */
	unsigned int is_kill;	/* number of kills generated on collisions */
	unsigned int is_fake;	/* MODE 'fakes' */
	unsigned int is_asuc;	/* successful auth requests */
	unsigned int is_abad;	/* bad auth requests */
	unsigned int is_udp;	/* packets recv'd on udp port */
	unsigned int is_loc;	/* local connections made */
};

#define EXTCMODETABLESZ 32

/* Number of maximum paramter modes to allow.
 * Don't set it unnecessarily high.. we only use k, l, L, j and f at the moment.
 */
#define MAXPARAMMODES 16

/** Channel Mode.
 * NOTE: you normally don't access these struct members directly.
 * For simple checking if a mode is set, use has_channel_mode()
 * Otherwise, see the extended channel modes API, CmodeAdd(), etc.
 */
struct Mode {
	Cmode_t mode;			/**< Other ("extended") channel modes set on this channel */
	void *mode_params[MAXPARAMMODES+1];	/**< Parameters for extended channel modes */
};

/* flags for Link if used to contain Watch --k4be */

/* WATCH type */
#define WATCH_FLAG_TYPE_WATCH	(1<<0) /* added via /WATCH command */
#define WATCH_FLAG_TYPE_MONITOR	(1<<1) /* added via /MONITOR command */

/* behaviour switches */
#define WATCH_FLAG_AWAYNOTIFY	(1<<8) /* should send AWAY notifications */

/* watch triggering events */
#define WATCH_EVENT_ONLINE		0
#define WATCH_EVENT_OFFLINE		1
#define WATCH_EVENT_AWAY		2
#define WATCH_EVENT_NOTAWAY		3
#define WATCH_EVENT_REAWAY		4
#define WATCH_EVENT_USERHOST	5
#define WATCH_EVENT_REALNAME	6
#define WATCH_EVENT_LOGGEDIN	7
#define WATCH_EVENT_LOGGEDOUT	8

/* Used for notify-hash buckets... -Donwulff */

struct Watch {
	Watch *hnext;
	time_t lasttime;
	Link *watch;
	char nick[1];
};

/** General link structure used for certain chains (watch list, invite list, dccallow).
 * Note that these always require you to use the make_link() and free_link() functions.
 * Do not combine with other alloc/free functions!!
 */
struct Link {
	struct Link *next;
	int flags;
	union {
		Client *client;
		Channel *channel;
		Watch *wptr;
		/* there used to be 'char *cp' here too,
		 * but in such a case you better use NameList
		 * instead of Link!
		 */
	} value;
};

#define IsInvalidChannelTS(x)	((x) <= 1000000) /**< Invalid channel creation time */

/**
 * @addtogroup CommonStructs
 * @{
 */

/** A channel on IRC */
struct Channel {
	struct Channel *nextch;			/**< Next channel in linked list (channel) */
	struct Channel *prevch;			/**< Previous channel in linked list (channel) */
	struct Channel *hnextch;		/**< Next channel in hash table */
	Mode mode;				/**< Channel Mode set on this channel */
	time_t creationtime;			/**< When the channel was first created */
	char *topic;				/**< Channel TOPIC */
	char *topic_nick;			/**< Person (or server) who set the TOPIC */
	time_t topic_time;			/**< Time at which the topic was last set */
	int users;				/**< Number of users in the channel */
	Member *members;			/**< List of channel members (users in the channel) */
	Ban *banlist;				/**< List of bans (+b) */
	Ban *exlist;				/**< List of ban exceptions (+e) */
	Ban *invexlist;				/**< List of invite exceptions (+I) */
	char *mode_lock;			/**< Mode lock (MLOCK) applied to channel - usually by Services */
	ModData moddata[MODDATA_MAX_CHANNEL];	/**< Channel attached module data, used by the ModData system */
	char name[CHANNELLEN+1];		/**< Channel name */
};

/** user/channel member struct (channel->members).
 * This is Member which is used in the linked list channel->members for each channel.
 * There is also Membership which is used in client->user->channels (see Membership for that).
 * Both must be kept synchronized 100% at all times.
 */
struct Member
{
	struct Member *next;				/**< Next entry in list */
	Client	      *client;				/**< The client */
	char member_modes[MEMBERMODESLEN];		/**< The access of the user on this channel (eg "vhoqa") */
	ModData moddata[MODDATA_MAX_MEMBER];		/** Member attached module data, used by the ModData system */
};

/** user/channel membership struct (client->user->channels).
 * This is Membership which is used in the linked list client->user->channels for each user.
 * There is also Member which is used in channel->members (see Member for that).
 * Both must be kept synchronized 100% at all times.
 */
struct Membership
{
	struct Membership 	*next;			/**< Next entry in list */
	struct Channel		*channel;			/**< The channel */
	char member_modes[MEMBERMODESLEN];		/**< The (new) access of the user on this channel (eg "vhoqa") */
	ModData moddata[MODDATA_MAX_MEMBERSHIP];	/**< Membership attached module data, used by the ModData system */
};

/** @} */

/** A ban, exempt or invite exception entry */
struct Ban {
	struct Ban *next;	/**< Next entry in list */
	char *banstr;		/**< The string (eg: *!*@*.example.org) */
	char *who;		/**< Person or server who set the entry (eg: Nick) */
	time_t when;		/**< When the entry was added */
};

/* Channel macros */
#define MODE_EXCEPT		0x0200
#define	MODE_BAN		0x0400
#define MODE_INVEX		0x8000000

/* name invisible */
#define	SecretChannel(x)	((x) && has_channel_mode((x), 's'))
/* channel not shown but names are */
#define	HiddenChannel(x)	((x) && has_channel_mode((x), 'p'))
/* channel visible */
#define	ShowChannel(v,c)	(PubChannel(c) || IsMember((v),(c)))
#define	PubChannel(x)		(!SecretChannel((x)) && !HiddenChannel((x)))

#define	IsChannelName(name) ((name) && (*(name) == '#'))

#define IsMember(blah,chan) ((blah && blah->user && \
                find_membership_link((blah->user)->channel, chan)) ? 1 : 0)


/* Misc macros */

#define	BadPtr(x) (!(x) || (*(x) == '\0'))

/** Is valid character in nick? [not for external usage, use do_check_nickname instead!] */
#define isvalid(c)   (char_atribs[(u_char)(c)]&ALLOWN)


#define TStime() (timeofday)

/* used in SetMode() in channel.c and cmd_umode() in s_msg.c */

#define	MODE_NULL      0
#define	MODE_ADD       0x40000000
#define	MODE_DEL       0x20000000

/* return values for hunt_server() */

#define	HUNTED_NOSUCH	(-1)	/* if the hunted server is not found */
#define	HUNTED_ISME	0	/* if this server should execute the command */
#define	HUNTED_PASS	1	/* if message passed onwards successfully */

/* used when sending to #mask or $mask */

#define	MATCH_SERVER  1
#define	MATCH_HOST    2

/* misc variable externs */

extern MODVAR char *version, *infotext[], *dalinfotext[], *unrealcredits[], *unrealinfo[];
extern MODVAR char *generation, *creation;
extern MODVAR char *gnulicense[];
/* misc defines */

#define	COMMA		","

#define isexcept void

extern MODVAR SSL_CTX *ctx;
extern MODVAR SSL_CTX *ctx_server;
extern MODVAR SSL_CTX *ctx_client;

#define TLS_PROTOCOL_TLSV1		0x0001
#define TLS_PROTOCOL_TLSV1_1	0x0002
#define TLS_PROTOCOL_TLSV1_2	0x0004
#define TLS_PROTOCOL_TLSV1_3	0x0008

#define TLS_PROTOCOL_ALL		0xffff

struct ThrottlingBucket
{
	struct ThrottlingBucket *prev, *next;
	char *ip;
	time_t since;
	char count;
};

typedef struct CoreChannelModeTable CoreChannelModeTable;
struct CoreChannelModeTable {
	long mode;			/**< Mode value (which bit will be set) */
	char flag;			/**< Mode letter (eg: 't') */
	unsigned halfop : 1;		/**< May halfop set this mode? 1/0 */
	unsigned parameters : 1;	/**< Mode requires a parameter? 1/0 */
};

/** Parse channel mode */
typedef struct ParseMode ParseMode;
struct ParseMode {
	int what;
	char modechar;
	char *param;
	Cmode *extm;
	const char *modebuf; /* curr pos */
	const char *parabuf; /* curr pos */
	char buf[512]; /* internal parse buffer */
};

#define MAXMULTILINEMODES       3
typedef struct MultiLineMode MultiLineMode;
struct MultiLineMode {
	char *modeline[MAXMULTILINEMODES+1];
	char *paramline[MAXMULTILINEMODES+1];
	int numlines;
};

typedef struct PendingServer PendingServer;
struct PendingServer {
	PendingServer *prev, *next;
	char sid[IDLEN+1];
};

typedef struct PendingNet PendingNet;
struct PendingNet {
	PendingNet *prev, *next; /* Previous and next in list */
	Client *client; /**< Client to which these servers belong */
	PendingServer *servers; /**< The list of servers connected to the client */
};

extern void init_throttling();
extern struct ThrottlingBucket *find_throttling_bucket(Client *);
extern void add_throttling_bucket(Client *);
extern int throttle_can_connect(Client *);

typedef struct MaxTarget MaxTarget;
struct MaxTarget {
	MaxTarget *prev, *next;
	char *cmd;
	int limit;
};
#define MAXTARGETS_MAX	1000000 /* used for 'max' */

#define VERIFY_OPERCOUNT(clnt,tag) { if (irccounts.operators < 0) verify_opercount(clnt,tag); } while(0)

#define MARK_AS_OFFICIAL_MODULE(modinf)	do { if (modinf && modinf->handle) ModuleSetOptions(modinfo->handle, MOD_OPT_OFFICIAL, 1);  } while(0)
#define MARK_AS_GLOBAL_MODULE(modinf)	do { if (modinf && modinf->handle) ModuleSetOptions(modinfo->handle, MOD_OPT_GLOBAL, 1);  } while(0)

/* used for is_banned type field: */
#define BANCHK_JOIN		0x0001	/* checking if a ban forbids the person from joining */
#define BANCHK_MSG		0x0002	/* checking if a ban forbids the person from sending messages */
#define BANCHK_NICK		0x0004	/* checking if a ban forbids the person from changing his/her nick */
#define BANCHK_LEAVE_MSG	0x0008	/* checking if a ban forbids the person from leaving a message in PART or QUIT */
#define BANCHK_TKL		0x0010	/* called from a server ban routine, or other match_user() usage */
#define BANCHK_ALL		(BANCHK_JOIN|BANCHK_MSG|BANCHK_NICK|BANCHK_LEAVE_MSG)	/* all events except BANCHK_TKL which is special */

#define TKLISTLEN		26
#define TKLIPHASHLEN1		4
#define TKLIPHASHLEN2		1021

#define MATCH_CHECK_IP              0x0001
#define MATCH_CHECK_REAL_HOST       0x0002
#define MATCH_CHECK_CLOAKED_HOST    0x0004
#define MATCH_CHECK_VISIBLE_HOST    0x0008
#define MATCH_CHECK_EXTENDED        0x0010

#define MATCH_CHECK_ALL             (MATCH_CHECK_IP|MATCH_CHECK_REAL_HOST|MATCH_CHECK_CLOAKED_HOST|MATCH_CHECK_VISIBLE_HOST|MATCH_CHECK_EXTENDED)
#define MATCH_CHECK_REAL            (MATCH_CHECK_IP|MATCH_CHECK_REAL_HOST|MATCH_CHECK_EXTENDED)

#define MATCH_MASK_IS_UHOST         0x1000
#define MATCH_MASK_IS_HOST          0x2000

typedef enum {
	POLICY_ALLOW=1,
	POLICY_WARN=2,
	POLICY_DENY=3
} Policy;

#define NO_EXIT_CLIENT	99

/*-- badwords --*/

#define MAX_MATCH       1
#define MAX_WORDLEN	64

#define PATTERN		"\\w*%s\\w*"
#define REPLACEWORD	"<censored>"

#define BADW_TYPE_INVALID 0x0
#define BADW_TYPE_FAST    0x1
#define BADW_TYPE_FAST_L  0x2
#define BADW_TYPE_FAST_R  0x4
#define BADW_TYPE_REGEX   0x8

#define BADWORD_REPLACE 1
#define BADWORD_BLOCK 2

typedef struct ConfigItem_badword ConfigItem_badword;

struct ConfigItem_badword {
	ConfigItem_badword      *prev, *next;
	ConfigFlag	flag;
	char		*word, *replace;
	unsigned short	type;
	char		action;
	pcre2_code	*pcre2_expr;
};

/*-- end of badwords --*/

/* Flags for 'sendflags' in 'sendto_channel' */
#define SEND_LOCAL	0x1
#define SEND_REMOTE	0x2
#define SEND_ALL	(SEND_LOCAL|SEND_REMOTE)
#define SKIP_DEAF	0x4
#define SKIP_CTCP	0x8

typedef struct GeoIPResult GeoIPResult;
struct GeoIPResult {
	char *country_code;
	char *country_name;
};

typedef enum WhoisConfigDetails {
	WHOIS_CONFIG_DETAILS_DEFAULT	= 0,
	WHOIS_CONFIG_DETAILS_NONE	= 1,
	WHOIS_CONFIG_DETAILS_LIMITED	= 2,
	WHOIS_CONFIG_DETAILS_FULL	= 3,
} WhoisConfigDetails;

/* Options for StripControlCodesEx() */
#define UNRL_STRIP_LOW_ASCII    0x1     /**< Strip all ASCII < 32 (control codes) */
#define UNRL_STRIP_KEEP_LF      0x2     /**< Do not strip LF (line feed, \n) */

/** JSON-RPC API Errors, according to jsonrpc.org spec */
typedef enum JsonRpcError {
	// Official JSON-RPC error codes:
	JSON_RPC_ERROR_PARSE_ERROR	= -32700, /**< JSON parse error (fatal) */
	JSON_RPC_ERROR_INVALID_REQUEST	= -32600, /**< Invalid JSON-RPC Request */
	JSON_RPC_ERROR_METHOD_NOT_FOUND	= -32601, /**< Method not found */
	JSON_RPC_ERROR_INVALID_PARAMS	= -32602, /**< Method parameters invalid */
	JSON_RPC_ERROR_INTERNAL_ERROR	= -32603, /**< Internal server error */
	// UnrealIRCd JSON-RPC server specific error codes:
	JSON_RPC_ERROR_API_CALL_DENIED	= -32000, /**< The api user does not have enough permissions to do this call */
	JSON_RPC_ERROR_SERVER_GONE	= -32001, /**< The request was forwarded to a remote server, but this server went gone while processing the request */
	JSON_RPC_ERROR_TIMEOUT		= -32002, /**< The request was forwarded to a remote server, but the request/response timed out (15 seconds) */
	JSON_RPC_ERROR_REMOTE_SERVER_NO_RPC	= -32003, /**< The request was going to be forwarded to a remote server, but the remote server does not support JSON-RPC */
	// UnrealIRCd specific application error codes:
	JSON_RPC_ERROR_NOT_FOUND	=  -1000, /**< Target not found (no such nick / channel / ..) */
	JSON_RPC_ERROR_ALREADY_EXISTS	=  -1001, /**< Resource already exists by that name (eg on nickchange request, a gline, etc) */
	JSON_RPC_ERROR_INVALID_NAME	=  -1002, /**< Name is not permitted (eg: nick, channel, ..) */
	JSON_RPC_ERROR_USERNOTINCHANNEL	=  -1003, /**< The user is not in the channel */
	JSON_RPC_ERROR_TOO_MANY_ENTRIES	=  -1004, /**< Too many entries (eg: banlist, ..) */
	JSON_RPC_ERROR_DENIED		=  -1005, /**< Permission denied for user (unrelated to api user permissions) */
} JsonRpcError;

/** Require a parameter in an RPC command */
#define REQUIRE_PARAM_STRING(name, varname)          do { \
                                                         varname = json_object_get_string(params, name); \
                                                         if (!varname) \
                                                         { \
                                                             rpc_error_fmt(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: '%s'", name); \
                                                             return; \
                                                         } \
                                                     } while(0)

#define REQUIRE_PARAM_INTEGER(name, varname)         do { \
                                                         json_t *t = json_object_get(params, name); \
                                                         if (!t || !json_is_integer(t)) \
                                                         { \
                                                             rpc_error_fmt(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: '%s'", name); \
                                                             return; \
                                                         } \
                                                         varname = json_integer_value(t); \
                                                     } while(0)

#define REQUIRE_PARAM_BOOLEAN(name, varname)         do { \
                                                         json_t *vvv = json_object_get(params, name); \
                                                         if (!v || !json_is_boolean(v)) \
                                                         { \
                                                             rpc_error_fmt(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: '%s'", name); \
                                                             return; \
                                                         } \
                                                         varname = json_is_true(v) ? 1 : 0; \
                                                     } while(0)

#define OPTIONAL_PARAM_STRING(name, varname)         varname = json_object_get_string(params, name)
#define OPTIONAL_PARAM_INTEGER(name, varname, def)   varname = json_object_get_integer(params, name, def)
#define OPTIONAL_PARAM_BOOLEAN(name, varname, def)   varname = json_object_get_boolean(params, name, def)

#endif /* __struct_include__ */

#include "dynconf.h"
