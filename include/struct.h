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
typedef struct ConfigItem_except ConfigItem_except;
typedef struct ConfigItem_link	ConfigItem_link;
typedef struct ConfigItem_ban ConfigItem_ban;
typedef struct ConfigItem_deny_dcc ConfigItem_deny_dcc;
typedef struct ConfigItem_deny_link ConfigItem_deny_link;
typedef struct ConfigItem_deny_channel ConfigItem_deny_channel;
typedef struct ConfigItem_deny_version ConfigItem_deny_version;
typedef struct ConfigItem_log ConfigItem_log;
typedef struct ConfigItem_unknown ConfigItem_unknown;
typedef struct ConfigItem_unknown_ext ConfigItem_unknown_ext;
typedef struct ConfigItem_alias ConfigItem_alias;
typedef struct ConfigItem_alias_format ConfigItem_alias_format;
typedef struct ConfigItem_include ConfigItem_include;
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
typedef struct Link Link;
typedef struct Ban Ban;
typedef struct Mode Mode;
typedef struct MessageTag MessageTag;
typedef struct MOTDFile MOTDFile; /* represents a whole MOTD, including remote MOTD support info */
typedef struct MOTDLine MOTDLine; /* one line of a MOTD stored as a linked list */
#ifdef USE_LIBCURL
typedef struct MOTDDownload MOTDDownload; /* used to coordinate download of a remote MOTD */
#endif

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

#define	HOSTLEN		63	/* Length of hostname.  Updated to         */
				/* comply with RFC1123                     */

#define	NICKLEN		30
#define	USERLEN		10
#define	REALLEN	 	50
#define SVIDLEN		30
#define MAXTOPICLEN	360	/* absolute maximum permitted topic length (above this = potential desync) */
#define MAXAWAYLEN	360	/* absolute maximum permitted away length (above this = potential desync) */
#define MAXKICKLEN	360	/* absolute maximum kick length (above this = only cutoff danger) */
#define MAXQUITLEN	395	/* absolute maximum quit length (above this = only cutoff danger) */
#define	CHANNELLEN	32
#define	PASSWDLEN 	256	/* some insane large limit (previously: 20, 32, 48) */
#define	KEYLEN		23
#define LINKLEN		32
#define	BUFSIZE		512	/* WARNING: *DONT* CHANGE THIS!!!! */
#define READBUFSIZE	8192	/* for the read buffer */
#define	MAXRECIPIENTS 	20
#define	MAXSILELENGTH	NICKLEN+USERLEN+HOSTLEN+10
#define IDLEN		12
#define SIDLEN           3
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
#define LOG_TKL   0x0004
#define LOG_KLINE 0x0008
#define LOG_CLIENT 0x0010
#define LOG_SERVER 0x0020
#define LOG_OPER   0x0040
#define LOG_SACMDS 0x0080
#define LOG_CHGCMDS 0x0100
#define LOG_OVERRIDE 0x0200
#define LOG_SPAMFILTER 0x0400
#define LOG_FLOOD 0x0800

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
 * such as states where the user is in the middle of an SSL/TLS handshake.
 * @defgroup ClientStatuses Client statuses / types
 * @{
 */
typedef enum ClientStatus {
	CLIENT_STATUS_LOG			= -7,	/**< Client is a log file */
	CLIENT_STATUS_TLS_STARTTLS_HANDSHAKE	= -8,	/**< Client is doing a STARTTLS handshake */
	CLIENT_STATUS_CONNECTING		= -6,	/**< Client is an outgoing connect */
	CLIENT_STATUS_TLS_CONNECT_HANDSHAKE	= -5,	/**< Client is doing an SSL/TLS handshake - outgoing connection */
	CLIENT_STATUS_TLS_ACCEPT_HANDSHAKE	= -4,	/**< Client is doing an SSL/TLS handshake - incoming connection */
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

/** @} */

/** Used for checking certain properties of clients, such as IsSecure() and IsULine().
 * @defgroup ClientFlags Client flags
 * @{
 */
#define	CLIENT_FLAG_PINGSENT		0x00000001	/**< PING sent, no reply yet */
#define	CLIENT_FLAG_DEAD		0x00000002	/**< Client is dead: already quit/exited and removed from all lists -- Remaining part will soon be freed in main loop */
#define	CLIENT_FLAG_DEADSOCKET		0x00000004	/**< Local socket is dead but otherwise the client still exists fully -- Will soon exit in main loop */
#define	CLIENT_FLAG_KILLED		0x00000008	/**< Prevents "QUIT" from being sent for this */
#define CLIENT_FLAG_IPV6		0x00000010	/**< Connection is using IPv6 */
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
#define CLIENT_FLAG_TLS			0x01000000	/**< Connection is using SSL/TLS */
#define CLIENT_FLAG_NOFAKELAG		0x02000000	/**< Exemption from fake lag */
#define CLIENT_FLAG_DCCBLOCK		0x04000000	/**< Block all DCC send requests */
#define CLIENT_FLAG_MAP			0x08000000	/**< Show this entry in /MAP (only used in map module) */
#define CLIENT_FLAG_PINGWARN		0x10000000	/**< Server ping warning (remote server slow with responding to PINGs) */
#define CLIENT_FLAG_NOHANDSHAKEDELAY	0x20000000	/**< No handshake delay */
/** @} */

#define SNO_DEFOPER "+kscfvGqobS"
#define SNO_DEFUSER "+ks"

#define SEND_UMODES (SendUmodes)
#define ALL_UMODES (AllUmodes)
/* SEND_UMODES and ALL_UMODES are now handled by umode_get/umode_lget/umode_gget -- Syzop. */

#define	CLIENT_FLAG_ID	(CLIENT_FLAG_USEIDENT|CLIENT_FLAG_IDENTSUCCESS)

/* PROTO_*: Server protocol extensions (acptr->local->proto).
 * Note that client protocol extensions have been moved
 * to the ClientCapability API which uses acptr->local->caps.
 */
#define PROTO_VL	0x000040	/* Negotiated VL protocol */
#define PROTO_VHP	0x000100	/* Send hostnames in NICKv2 even if not sethosted */
#define PROTO_CLK	0x001000	/* Send cloaked host in the NICK command (regardless of +x/-x) */
#define PROTO_MLOCK	0x002000	/* server supports MLOCK */
#define PROTO_EXTSWHOIS 0x004000	/* extended SWHOIS support */
#define PROTO_SJSBY	0x008000	/* SJOIN setby information (TS and nick) */
#define PROTO_MTAGS	0x010000	/* Support message tags and big buffers */

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
#define	SendWallops(x)		(!IsMe(x) && IsUser(x) && ((x)->umodes & UMODE_WALLOP))
#define IsHidden(x)             ((x)->umodes & UMODE_HIDE)
#define IsSetHost(x)		((x)->umodes & UMODE_SETHOST)
#define IsHideOper(x)		((x)->umodes & UMODE_HIDEOPER)
#define	SetOper(x)		((x)->umodes |= UMODE_OPER)
#define	SetInvisible(x)		((x)->umodes |= UMODE_INVISIBLE)
#define	SetWallops(x)  		((x)->umodes |= UMODE_WALLOP)
#define SetRegNick(x)		((x)->umodes & UMODE_REGNICK)
#define SetHidden(x)            ((x)->umodes |= UMODE_HIDE)
#define SetHideOper(x)		((x)->umodes |= UMODE_HIDEOPER)
#define IsSecureConnect(x)	((x)->umodes & UMODE_SECURE)
#define	ClearOper(x)		((x)->umodes &= ~UMODE_OPER)
#define	ClearInvisible(x)	((x)->umodes &= ~UMODE_INVISIBLE)
#define	ClearWallops(x)		((x)->umodes &= ~UMODE_WALLOP)
#define ClearHidden(x)          ((x)->umodes &= ~UMODE_HIDE)
#define ClearHideOper(x)	((x)->umodes &= ~UMODE_HIDEOPER)

/* Snomask macros: */
#define	SendServNotice(x)	(((x)->user) && ((x)->user->snomask & SNO_SNOTICE))
#define IsKillsF(x)		((x)->user->snomask & SNO_KILLS)
#define IsClientF(x)		((x)->user->snomask & SNO_CLIENT)
#define IsFloodF(x)		((x)->user->snomask & SNO_FLOOD)
#define IsEyes(x)		((x)->user->snomask & SNO_EYES)
#define SetKillsF(x)		((x)->user->snomask |= SNO_KILLS)
#define SetClientF(x)		((x)->user->snomask |= SNO_CLIENT)
#define SetFloodF(x)		((x)->user->snomask |= SNO_FLOOD)
#define SetEyes(x)		((x)->user->snomask |= SNO_EYES)
#define ClearKillsF(x)		((x)->user->snomask &= ~SNO_KILLS)
#define ClearClientF(x)		((x)->user->snomask &= ~SNO_CLIENT)
#define ClearFloodF(x)		((x)->user->snomask &= ~SNO_FLOOD)
#define ClearEyes(x)		((x)->user->snomask &= ~SNO_EYES)


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
#define IsUseIdent(x)			((x)->flags & CLIENT_FLAG_USEIDENT)
#define IsDNSLookup(x)			((x)->flags & CLIENT_FLAG_DNSLOOKUP)
#define IsEAuth(x)			((x)->flags & CLIENT_FLAG_EAUTH)
#define IsIdentSuccess(x)		((x)->flags & CLIENT_FLAG_IDENTSUCCESS)
#define IsIPV6(x)			((x)->flags & CLIENT_FLAG_IPV6)
#define IsKilled(x)			((x)->flags & CLIENT_FLAG_KILLED)
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
#define IsVirus(x)			((x)->flags & CLIENT_FLAG_VIRUS)
#define IsIdentLookupSent(x)		((x)->flags & CLIENT_FLAG_IDENTLOOKUPSENT)
#define SetIdentLookup(x)		do { (x)->flags |= CLIENT_FLAG_IDENTLOOKUP; } while(0)
#define SetClosing(x)			do { (x)->flags |= CLIENT_FLAG_CLOSING; } while(0)
#define SetDCCBlock(x)			do { (x)->flags |= CLIENT_FLAG_DCCBLOCK; } while(0)
#define SetDCCNotice(x)			do { (x)->flags |= CLIENT_FLAG_DCCNOTICE; } while(0)
#define SetDead(x)			do { (x)->flags |= CLIENT_FLAG_DEAD; } while(0)
#define SetDeadSocket(x)		do { (x)->flags |= CLIENT_FLAG_DEADSOCKET; } while(0)
#define SetUseIdent(x)			do { (x)->flags |= CLIENT_FLAG_USEIDENT; } while(0)
#define SetDNSLookup(x)			do { (x)->flags |= CLIENT_FLAG_DNSLOOKUP; } while(0)
#define SetEAuth(x)			do { (x)->flags |= CLIENT_FLAG_EAUTH; } while(0)
#define SetIdentSuccess(x)		do { (x)->flags |= CLIENT_FLAG_IDENTSUCCESS; } while(0)
#define SetIPV6(x)			do { (x)->flags |= CLIENT_FLAG_IPV6; } while(0)
#define SetKilled(x)			do { (x)->flags |= CLIENT_FLAG_KILLED; } while(0)
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
#define ClearIPV6(x)			do { (x)->flags &= ~CLIENT_FLAG_IPV6; } while(0)
#define ClearKilled(x)			do { (x)->flags &= ~CLIENT_FLAG_KILLED; } while(0)
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
/** @} */


/* Others that access client structs: */
#define	IsNotSpoof(x)	((x)->local->nospoof == 0)
#define GetHost(x)	(IsHidden(x) ? (x)->user->virthost : (x)->user->realhost)
#define GetIP(x)	(x->ip ? x->ip : "255.255.255.255")
#define IsLoggedIn(x)	(x->user && (*x->user->svid != '*') && !isdigit(*x->user->svid)) /**< Logged into services */
#define IsSynched(x)	(x->serv->flags.synced)
#define IsServerSent(x) (x->serv && x->serv->flags.server_sent)

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

#define SetVL(x)		((x)->local->proto |= PROTO_VL)
#define SetSJSBY(x)		((x)->local->proto |= PROTO_SJSBY)
#define SetVHP(x)		((x)->local->proto |= PROTO_VHP)
#define SetCLK(x)		((x)->local->proto |= PROTO_CLK)
#define SetMTAGS(x)		((x)->local->proto |= PROTO_MTAGS)

/*
 * defined debugging levels
 */
#define	DEBUG_FATAL  0
#define	DEBUG_ERROR  1		/* report_error() and other errors that are found */
#define	DEBUG_NOTICE 3
#define	DEBUG_DNS    4		/* used by all DNS related routines - a *lot* */
#define	DEBUG_INFO   5		/* general usful info */
#define	DEBUG_NUM    6		/* numerics */
#define	DEBUG_SEND   7		/* everything that is sent out */
#define	DEBUG_DEBUG  8		/* anything to do with debugging, ie unimportant :) */
#define	DEBUG_MALLOC 9		/* malloc/free calls */
#define	DEBUG_LIST  10		/* debug list use */

/*
 * defines for curses in client
 */
#define	DUMMY_TERM	0
#define	CURSES_TERM	1
#define	TERMCAP_TERM	2

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
        char *str;
        void *ptr;
};

#ifndef _WIN32
 #define CHECK_LIST_ENTRY(list)		if (offsetof(typeof(*list),prev) != offsetof(ListStruct,prev)) \
					{ \
						ircd_log(LOG_ERROR, "[BUG] %s:%d: List operation on struct with incorrect order (->prev must be 1st struct member)", __FILE__, __LINE__); \
						abort(); \
					} \
					if (offsetof(typeof(*list),next) != offsetof(ListStruct,next)) \
					{ \
						ircd_log(LOG_ERROR, "[BUG] %s:%d: List operation on struct with incorrect order (->next must be 2nd struct member))", __FILE__, __LINE__); \
						abort(); \
					}
#else
 #define CHECK_LIST_ENTRY(list)		/* not available on Windows, typeof() not reliable */
#endif

#ifndef _WIN32
 #define CHECK_PRIO_LIST_ENTRY(list)	if (offsetof(typeof(*list),prev) != offsetof(ListStructPrio,prev)) \
					{ \
						ircd_log(LOG_ERROR, "[BUG] %s:%d: List operation on struct with incorrect order (->prev must be 1st struct member)", __FILE__, __LINE__); \
						abort(); \
					} \
					if (offsetof(typeof(*list),next) != offsetof(ListStructPrio,next)) \
					{ \
						ircd_log(LOG_ERROR, "[BUG] %s:%d: List operation on struct with incorrect order (->next must be 2nd struct member))", __FILE__, __LINE__); \
						abort(); \
					} \
					if (offsetof(typeof(*list),priority) != offsetof(ListStructPrio,priority)) \
					{ \
						ircd_log(LOG_ERROR, "[BUG] %s:%d: List operation on struct with incorrect order (->priority must be 3rd struct member))", __FILE__, __LINE__); \
						abort(); \
					}
#else
 #define CHECK_PRIO_LIST_ENTRY(list)	/* not available on Windows, typeof() not reliable */
#endif

#define CHECK_NULL_LIST_ITEM(item)	if ((item)->prev || (item)->next) \
					{ \
						ircd_log(LOG_ERROR, "[BUG] %s:%d: List operation on item with non-NULL 'prev' or 'next' -- are you adding to a list twice?", __FILE__, __LINE__); \
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

/** @} */

typedef struct MultiLine MultiLine;
/** Multi-line list.
 * @see addmultiline(), freemultiline(), sendnotice_multiline()
 */
struct MultiLine {
	MultiLine *prev, *next;
	char *line;
};

#ifdef USE_LIBCURL
struct MOTDDownload
{
	MOTDFile *themotd;
};
#endif /* USE_LIBCURL */

struct MOTDFile 
{
	struct MOTDLine *lines;
	struct tm last_modified; /* store the last modification time */

#ifdef USE_LIBCURL
	/*
	  This pointer is used to communicate with an asynchronous MOTD
	  download. The problem is that a download may take 10 seconds or
	  more to complete and, in that time, the IRCd could be rehashed.
	  This would mean that TLD blocks are reallocated and thus the
	  aMotd structs would be free()d in the meantime.

	  To prevent such a situation from leading to a segfault, we
	  introduce this remote control pointer. It works like this:
	  1. read_motd() is called with a URL. A new MOTDDownload is
	     allocated and the pointer is placed here. This pointer is
	     also passed to the asynchrnous download handler.
	  2.a. The download is completed and read_motd_async_downloaded()
	       is called with the same pointer. From this function, this pointer
	       if free()d. No other code may free() the pointer. Not even free_motd().
	    OR
	  2.b. The user rehashes the IRCd before the download is completed.
	       free_motd() is called, which sets motd_download->themotd to NULL
	       to signal to read_motd_async_downloaded() that it should ignore
	       the download. read_motd_async_downloaded() is eventually called
	       and frees motd_download.
	 */
	struct MOTDDownload *motd_download;
#endif /* USE_LIBCURL */
};

struct MOTDLine {
	char *line;
	struct MOTDLine *next;
};

struct LoopStruct {
	unsigned do_garbage_collect : 1;
	unsigned config_test : 1;
	unsigned ircd_booted : 1;
	unsigned ircd_forked : 1;
	unsigned do_bancheck : 1; /* perform *line bancheck? */
	unsigned do_bancheck_spamf_user : 1; /* perform 'user' spamfilter bancheck */
	unsigned do_bancheck_spamf_away : 1; /* perform 'away' spamfilter bancheck */
	unsigned ircd_rehashing : 1;
	unsigned ircd_terminating : 1;
	unsigned tainted : 1;
	Client *rehash_save_cptr, *rehash_save_client;
	int rehash_save_sig;
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
	char *servername;
	char *realname;
	long umodes;
	time_t   logoff;
	struct Client *online;	/* Pointer to new nickname for chasing or NULL */
	struct Whowas *next;	/* for hash table... */
	struct Whowas *prev;	/* for hash table... */
	struct Whowas *cnext;	/* for client struct linked list */
	struct Whowas *cprev;	/* for client struct linked list */
} aWhowas;

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

/** Command function - used by all command handlers.
 * This is used in the code like <pre>CMD_FUNC(cmd_yourcmd)</pre> as a function definition.
 * @param cptr        The client direction pointer.
 * @param client        The source client pointer (you usually need this one).
 * @param recv_mtags  Received message tags for this command.
 * @param parc        Parameter count *plus* 1.
 * @param parv        Parameter values.
 * @note  Slightly confusing, but parc will be 2 if 1 parameter was provided.
 *        It is two because parv will still have 2 elements, parv[1] will be your first parameter,
 *        and parv[2] will be NULL.
 *        Note that reading parv[parc] and beyond is OUT OF BOUNDS and will cause a crash.
 *        E.g. parv[3] in the above example is out of bounds.
 */
#define CMD_FUNC(x) void (x) (Client *client, MessageTag *recv_mtags, int parc, char *parv[])
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
#define CMD_OVERRIDE_FUNC(x) void (x)(CommandOverride *ovr, Client *client, MessageTag *recv_mtags, int parc, char *parv[])



typedef void (*CmdFunc)(Client *client, MessageTag *mtags, int parc, char *parv[]);
typedef void (*AliasCmdFunc)(Client *client, MessageTag *mtags, int parc, char *parv[], char *cmd);
typedef void (*OverrideCmdFunc)(CommandOverride *ovr, Client *client, MessageTag *mtags, int parc, char *parv[]);

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
	int  clients;		/* total */
	int  invisible;		/* invisible */
	unsigned short  servers;		/* servers */
	int  operators;		/* operators */
	int  unknown;		/* unknown local connections */
	int  channels;		/* channels */
	int  me_clients;	/* my clients */
	unsigned short  me_servers;	/* my servers */
	int  me_max;		/* local max */
	int  global_max;	/* global max */
};

/** The /LUSERS stats information */
extern MODVAR IRCCounts irccounts;

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

extern MODVAR Umode *Usermode_Table;
extern MODVAR short	 Usermode_highest;

extern MODVAR Snomask *Snomask_Table;
extern MODVAR short Snomask_highest;

extern MODVAR Cmode *Channelmode_Table;
extern MODVAR unsigned short Channelmode_highest;

extern Umode *UmodeAdd(Module *module, char ch, int options, int unset_on_deoper, int (*allowed)(Client *client, int what), long *mode);
extern void UmodeDel(Umode *umode);

extern Snomask *SnomaskAdd(Module *module, char ch, int (*allowed)(Client *client, int what), long *mode);
extern void SnomaskDel(Snomask *sno);

extern Cmode *CmodeAdd(Module *reserved, CmodeInfo req, Cmode_t *mode);
extern void CmodeDel(Cmode *cmode);

extern void moddata_init(void);
extern ModDataInfo *ModDataAdd(Module *module, ModDataInfo req);
extern void ModDataDel(ModDataInfo *md);
extern void unload_all_unused_moddata(void);

#define LISTENER_NORMAL		0x000001
#define LISTENER_CLIENTSONLY	0x000002
#define LISTENER_SERVERSONLY	0x000004
#define LISTENER_TLS		0x000010
#define LISTENER_BOUND		0x000020
#define LISTENER_DEFER_ACCEPT	0x000040

#define IsServersOnlyListener(x)	((x) && ((x)->options & LISTENER_SERVERSONLY))

#define CONNECT_TLS		0x000001
//0x000002 unused (was ziplinks)
#define CONNECT_AUTO		0x000004
#define CONNECT_QUARANTINE	0x000008
#define CONNECT_NODNSCACHE	0x000010
#define CONNECT_NOHOSTCHECK	0x000020
#define CONNECT_INSECURE	0x000040

#define TLSFLAG_FAILIFNOCERT 	0x1
#define TLSFLAG_NOSTARTTLS	0x8
#define TLSFLAG_DISABLECLIENTCERT 0x10

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
} FloodOption;
#define MAXFLOODOPTIONS 10


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
	Server *serv;				/**< Additional information, if this is a server */
	ClientStatus status;			/**< Client status, one of CLIENT_STATUS_* */
	struct list_head client_hash;		/**< For name hash table (clientTable) */
	char name[HOSTLEN + 1];			/**< Unique name of the client: nickname for users, hostname for servers */
	time_t lastnick;			/**< Timestamp on nick */
	long flags;				/**< Client flags (one or more of CLIENT_FLAG_*) */
	long umodes;				/**< Client usermodes (if user) */
	Client *direction;			/**< Direction from which this client originated.
	                                             This always points to a directly connected server or &me.
	                                             It is never NULL */
	unsigned char hopcount;			/**< Number of servers to this, 0 means local client */
	char ident[USERLEN + 1];		/**< Ident of the user, if available. Otherwise set to "unknown". */
	char info[REALLEN + 1];			/**< Additional client information text. For users this is gecos/realname */
	char id[IDLEN + 1];			/**< Unique ID: SID or UID */
	struct list_head id_hash;		/**< For UID/SID hash table (idTable) */
	Client *srvptr;				/**< Server on where this client is connected to (can be &me) */
	char *ip;				/**< IP address of user or server (never NULL) */
	ModData moddata[MODDATA_MAX_CLIENT];	/**< Client attached module data, used by the ModData system */
};

/** Local client information, use client->local to access these (see also @link Client @endlink).
 */
struct LocalClient {
	int fd;				/**< File descriptor, can be <0 if socket has been closed already. */
	SSL *ssl;			/**< OpenSSL/LibreSSL struct for SSL/TLS connection */
	time_t since;			/**< Time when user will next be allowed to send something (actually since<currenttime+10) */
	int since_msec;			/**< Used for calculating 'since' penalty (modulo) */
	time_t firsttime;		/**< Time user was created (connected on IRC) */
	time_t lasttime;		/**< Last time any message was received */
	dbuf sendQ;			/**< Outgoing send queue (data to be sent) */
	dbuf recvQ;			/**< Incoming receive queue (incoming data yet to be parsed) */
	ConfigItem_class *class;	/**< The class { } block associated to this client */
	int proto;			/**< PROTOCTL options */
	long caps;			/**< User: enabled capabilities (via CAP command) */
	time_t nexttarget;		/**< Next time that a new target will be allowed (msg/notice/invite) */
	u_char targets[MAXCCUSERS];	/**< Hash values of targets for target limiting */
	ConfigItem_listen *listener;	/**< If this client IsListening() then this is the listener configuration attached to it */
	long serial;			/**< Current serial number for send.c functions (to avoid sending duplicate messages) */
	time_t nextnick;		/**< Time the next nick change will be allowed */
	time_t last;			/**< Last time a RESETIDLE message was received (PRIVMSG) */
	long sendM;			/**< Statistics: protocol messages send */
	long sendK;			/**< Statistics: total k-bytes send */
	long receiveM;			/**< Statistics: protocol messages received */
	long receiveK;			/**< Statistics: total k-bytes received */
	u_short sendB;			/**< Statistics: counters to count upto 1-k lots of bytes */
	u_short receiveB;		/**< Statistics: sent and received (???) */
	short lastsq;			/**< # of 2k blocks when sendqueued called last */
	Link *watch;			/**< Watch notification list (WATCH) for this user */
	u_short watches;		/**< Number of entries in the watch list */
	ModData moddata[MODDATA_MAX_LOCAL_CLIENT];	/**< LocalClient attached module data, used by the ModData system */
#ifdef DEBUGMODE
	time_t cputime;			/**< Something with debugging (why is this a time_t? TODO) */
#endif
	char *error_str;		/**< Quit reason set by dead_socket() in case of socket/buffer error, later used by exit_client() */
	char sasl_agent[NICKLEN + 1];	/**< SASL: SASL Agent the user is interacting with */
	unsigned char sasl_out;		/**< SASL: Number of outgoing sasl messages */
	unsigned char sasl_complete;	/**< SASL: >0 if SASL authentication was successful */
	time_t sasl_sent_time;		/**< SASL: 0 or the time that the (last) AUTHENTICATE command has been sent */
	char *sni_servername;		/**< Servername as sent by client via SNI (Server Name Indication) in SSL/TLS, otherwise NULL */
	int cap_protocol;		/**< CAP protocol in use. At least 300 for any CAP capable client. 302 for 3.2, etc.. */
	uint32_t nospoof;		/**< Anti-spoofing random number (used in user handshake PING/PONG) */
	char *passwd;			/**< Password used during connect, if any (freed once connected and set to NULL) */
	int authfd;			/**< File descriptor for ident checking (RFC931) */
	int identbufcnt;		/**< Counter for 'ident' reading code */
	struct hostent *hostp;		/**< Host record for this client (used by DNS code) */
	char sockhost[HOSTLEN + 1];	/**< Hostname from the socket */
	u_short port;			/**< Remote TCP port of client */
	FloodCounter flood[MAXFLOODOPTIONS];
};

/** User information (persons, not servers), you use client->user to access these (see also @link Client @endlink).
 */
struct User {
	Membership *channel;		/**< Channels that the user is in (linked list) */
	Link *invited;			/**< Channels has the user been invited to (linked list) */
	Link *dccallow;			/**< DCCALLOW list (linked list) */
	char *away;			/**< AWAY message, or NULL if not away */
	char svid[SVIDLEN + 1];		/**< Services account name or ID (SVID) */
	unsigned short joined;		/**< Number of channels joined */
	char username[USERLEN + 1];	/**< Username, the user portion in nick!user@host. */
	char realhost[HOSTLEN + 1];	/**< Realhost, the real host of the user (IP or hostname) - usually this is not shown to other users */
	char cloakedhost[HOSTLEN + 1];	/**< Cloaked host - generated by cloaking algorithm */
	char *virthost;			/**< Virtual host - when user has user mode +x this is the active host */
	char *server;			/**< Server name the user is on (?) */
	SWhois *swhois;			/**< Special "additional" WHOIS entries such as "a Network Administrator" */
	aWhowas *whowas;		/**< Something for whowas :D :D */
	int snomask;			/**< Server Notice Mask (snomask) - only for IRCOps */
	char *operlogin;		/**< Which oper { } block was used to oper up, otherwise NULL - used by oper::maxlogins */
	struct {
		time_t nick_t;		/**< For set::anti-flood::nick-flood: time */
		time_t knock_t;		/**< For set::anti-flood::knock-flood: time */
		time_t invite_t;	/**< For set::anti-flood::invite-flood: time */
		unsigned char nick_c;	/**< For set::anti-flood::nick-flood: counter */
		unsigned char knock_c;	/**< For set::anti-flood::knock-flood: counter */
		unsigned char invite_c;	/**< For set::anti-flood::invite-flood: counter */
	} flood;			/**< Anti-flood counters */
	time_t lastaway;		/**< Last time the user went AWAY */
};

/** Server information (local servers and remote servers), you use client->serv to access these (see also @link Client @endlink).
 */
struct Server {
	char *up;			/**< Name of uplink for this server */
	char by[NICKLEN + 1];		/**< Uhhhh - who activated this connection - AGAIN? */
	ConfigItem_link *conf;		/**< link { } block associated with this server, or NULL */
	time_t timestamp;		/**< Remotely determined connect try time */
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
	PREPROCESSOR_PHASE_MODULE	= 2
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

struct ConfigFile
{
        char            *cf_filename;
        ConfigEntry     *cf_entries;
        ConfigFile     *cf_next;
};

struct ConfigEntry
{
        ConfigFile	*ce_fileptr;
        int 	 	ce_varlinenum, ce_fileposstart, ce_fileposend, ce_sectlinenum;
        char 		*ce_varname, *ce_vardata;
        ConfigEntry     *ce_entries, *ce_prevlevel, *ce_next;
        ConditionalConfig *ce_cond;
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

#define CRULE_ALL		0
#define CRULE_AUTO		1

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
	ConfigItem_mask *mask;
	char *server;
	AuthConfig *auth;
	int maxperip; /**< Maximum connections permitted per IP address (locally) */
	int global_maxperip; /**< Maximum connections permitted per IP address (globally) */
	int port;
	ConfigItem_class *class;
	ConfigFlag_allow flags;
	unsigned short ipv6_clone_mask;
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
        void *extra;
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
	ConfigItem_mask *mask;
	unsigned long modes, require_modes;
	char *vhost;
	int maxlogins;
};

/** The SSL/TLS options that are used in set::tls and otherblocks::tls-options.
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
	char 		*mask, *channel;
	char 		*motd_file, *rules_file, *smotd_file;
	char 		*botmotd_file, *opermotd_file;
	MOTDFile	rules, motd, smotd, botmotd, opermotd;
	u_short		options;
};

struct ConfigItem_listen {
	ConfigItem_listen *prev, *next;
	ConfigFlag flag;
	char *ip;
	int port;
	int options, clients;
	int fd;
	int ipv6;
	SSL_CTX *ssl_ctx;
	TLSOptions *tls_options;
	int websocket_options; /* should be in module, but lazy */
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
	ConfigItem_mask *mask;
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
		ConfigItem_mask *mask; /**< incoming mask(s) to accept */
	} incoming;
	struct {
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

struct ConfigItem_except {
	ConfigItem_except      *prev, *next;
	ConfigFlag_except      flag;
	int type;
	char		*mask;
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

struct ConfigItem_deny_link {
	ConfigItem_deny_link *prev, *next;
	ConfigFlag_except flag;
	ConfigItem_mask  *mask;
	char *rule, *prettyrule;
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
	ConfigItem_mask *mask;
};

struct ConfigItem_allow_channel {
	ConfigItem_allow_channel		*prev, *next;
	ConfigFlag		flag;
	char			*channel, *class;
	ConfigItem_mask *mask;
};

struct ConfigItem_allow_dcc {
	ConfigItem_allow_dcc		*prev, *next;
	ConfigFlag_ban	flag;
	char			*filename;
};

struct ConfigItem_log {
	ConfigItem_log *prev, *next;
	ConfigFlag flag;
	char *file; /**< Filename to log to (either generated or specified) */
	char *filefmt; /**< Filename with dynamic % stuff */
	long maxsize;
	int  flags;
	int  logfd;
};

struct ConfigItem_unknown {
	ConfigItem_unknown *prev, *next;
	ConfigFlag flag;
	ConfigEntry *ce;
};

struct ConfigItem_unknown_ext {
	ConfigItem_unknown_ext *prev, *next;
	ConfigFlag flag;
	char *ce_varname, *ce_vardata;
	ConfigFile      *ce_fileptr;
	int             ce_varlinenum;
	ConfigEntry     *ce_entries;
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

/**
 * In a rehash scenario, conf_include will contain all of the included
 * configs that are actually in use. It also will contain includes
 * that are being processed so that the configuration may be updated.
 * INCLUDE_NOTLOADED is set on all of the config files that are being
 * loaded and unset on already-loaded files. See
 * unload_loaded_includes() and load_includes().
 */
#define INCLUDE_NOTLOADED  0x1
#define INCLUDE_REMOTE     0x2
#define INCLUDE_DLQUEUED   0x4
/**
 * Marks that an include was loaded without error. This seems to
 * overlap with the INCLUDE_NOTLOADED meaning(?). --binki
 */
#define INCLUDE_USED       0x8
	
struct ConfigItem_include {
	ConfigItem_include *prev, *next;
	ConfigFlag_ban flag;
	char *file;
#ifdef USE_LIBCURL
	char *url;
	char *errorbuf;
#endif
	char *included_from;
	int included_from_line;
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
	char chname[CHANNELLEN+1];
	char *topic;
};

#define SECURITYGROUPLEN 48
struct SecurityGroup {
	SecurityGroup *prev, *next;
	int priority;
	char name[SECURITYGROUPLEN+1];
	int identified;
	int reputation_score;
	int webirc;
	int tls;
	ConfigItem_mask *include_mask;
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
	unsigned short is_cbs;	/* bytes sent to clients */
	unsigned short is_cbr;	/* bytes received to clients */
	unsigned short is_sbs;	/* bytes sent to servers */
	unsigned short is_sbr;	/* bytes received to servers */
	unsigned long is_cks;	/* k-bytes sent to clients */
	unsigned long is_ckr;	/* k-bytes received to clients */
	unsigned long is_sks;	/* k-bytes sent to servers */
	unsigned long is_skr;	/* k-bytes received to servers */
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

typedef struct MemoryInfo {
	unsigned int classes;
	unsigned long classesmem;
} MemoryInfo;

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
	long mode;				/**< Core modes set on this channel (one of MODE_*) */
	Cmode_t extmode;			/**< Other ("extended") channel modes set on this channel */
	void *extmodeparams[MAXPARAMMODES+1];	/**< Parameters for extended channel modes */
	int  limit;				/**< The +l limit in effect (eg: 40), if any - otherwise 0 */
	char key[KEYLEN + 1];			/**< The +k key in effect (eg: secret), if any - otherwise NULL */
};

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
	Link *invites;				/**< List of outstanding /INVITE's from ops */
	Ban *banlist;				/**< List of bans (+b) */
	Ban *exlist;				/**< List of ban exceptions (+e) */
	Ban *invexlist;				/**< List of invite exceptions (+I) */
	char *mode_lock;			/**< Mode lock (MLOCK) applied to channel - usually by Services */
	ModData moddata[MODDATA_MAX_CHANNEL];	/**< Channel attached module data, used by the ModData system */
	char chname[1];				/**< Channel name */
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
	int		flags;				/**< The access of the user on this channel (one or more of CHFL_*) */
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
	int			flags;			/**< The access of the user on this channel (one or more of CHFL_*) */
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

/*
** Channel Related macros follow
*/

/* Channel related flags */
#ifdef PREFIX_AQ
 #define CHFL_CHANOP_OR_HIGHER (CHFL_CHANOP|CHFL_CHANADMIN|CHFL_CHANOWNER)
 #define CHFL_HALFOP_OR_HIGHER (CHFL_CHANOWNER|CHFL_CHANADMIN|CHFL_CHANOP|CHFL_HALFOP)
#else
 #define CHFL_CHANOP_OR_HIGHER (CHFL_CHANOP)
 #define CHFL_HALFOP_OR_HIGHER (CHFL_CHANOP|CHFL_HALFOP)
#endif

/** Channel flags (privileges) of users on a channel.
 * This is used by Member and Membership (m->flags) to indicate the access rights of a user in a channel.
 * Also used by SJOIN and MODE to set some flags while a JOIN or MODE is in process.
 * @defgroup ChannelFlags Channel access flags
 * @{
 */
/** Is channel owner (+q) */
#define is_chanowner(cptr,channel) (get_access(cptr,channel) & CHFL_CHANOWNER)
/** Is channel admin (+a) */
#define is_chanadmin(cptr,channel) (get_access(cptr,channel) & CHFL_CHANADMIN)
/** Is channel operator or higher (+o/+a/+q) */
#define is_chan_op(cptr,channel) (get_access(cptr,channel) & CHFL_CHANOP_OR_HIGHER)
/** Is some kind of channel op (+h/+o/+a/+q) */
#define is_skochanop(cptr,channel) (get_access(cptr,channel) & CHFL_HALFOP_OR_HIGHER)
/** Is half-op (+h) */
#define is_half_op(cptr,channel) (get_access(cptr,channel) & CHFL_HALFOP)
/** Has voice (+v) */
#define has_voice(cptr,channel) (get_access(cptr,channel) & CHFL_VOICE)
/* Important:
 * Do not blindly change the values of CHFL_* as they must match the
 * ones in MODE_*. I already screwed this up twice. -- Syzop
 * Obviously these should be decoupled in a code cleanup.
 */
#define	CHFL_CHANOP     0x0001	/**< Channel operator (+o) */
#define	CHFL_VOICE      0x0002	/**< Voice (+v, can speak through bans and +m) */
#define	CHFL_DEOPPED	0x0004	/**< De-oped by a server (temporary state) */
#define CHFL_CHANOWNER	0x0040	/**< Channel owner (+q) */
#define CHFL_CHANADMIN	0x0080	/**< Channel admin (+a) */
#define CHFL_HALFOP	0x0100	/**< Channel halfop (+h) */
#define	CHFL_BAN     	0x0200	/**< Channel ban (+b) - not a real flag, only used in sjoin.c */
#define CHFL_EXCEPT	0x0400	/**< Channel except (+e) - not a real flag, only used in sjoin.c */
#define CHFL_INVEX	0x0800  /**< Channel invite exception (+I) - not a real flag, only used in sjoin.c */
/** @} */

#define CHFL_REJOINING	0x8000  /* used internally by rejoin_* */

#define	CHFL_OVERLAP    (CHFL_CHANOWNER|CHFL_CHANADMIN|CHFL_CHANOP|CHFL_VOICE|CHFL_HALFOP)

/* Channel macros */
/* Don't blindly change these MODE_* values, see comment 20 lines up! */
#define	MODE_CHANOP		CHFL_CHANOP
#define	MODE_VOICE		CHFL_VOICE
#define	MODE_PRIVATE		0x0004
#define	MODE_SECRET		0x0008
#define	MODE_MODERATED  	0x0010
#define	MODE_TOPICLIMIT 	0x0020
#define MODE_CHANOWNER		0x0040
#define MODE_CHANADMIN		0x0080
#define	MODE_HALFOP		0x0100
#define MODE_EXCEPT		0x0200
#define	MODE_BAN		0x0400
#define	MODE_INVITEONLY 	0x0800
#define	MODE_NOPRIVMSGS 	0x1000
#define	MODE_KEY		0x2000
#define	MODE_LIMIT		0x4000
#define MODE_RGSTR		0x8000
#define MODE_INVEX		0x8000000

/*
 * mode flags which take another parameter (With PARAmeterS)
 */
#define	MODE_WPARAS (MODE_HALFOP|MODE_CHANOP|MODE_VOICE|MODE_CHANOWNER|MODE_CHANADMIN|MODE_BAN|MODE_KEY|MODE_LIMIT|MODE_EXCEPT|MODE_INVEX)
/*
 * Undefined here, these are used in conjunction with the above modes in
 * the source.
#define	MODE_DEL       0x200000000
#define	MODE_ADD       0x400000000
 */

#define	HoldChannel(x)		(!(x))
/* name invisible */
#define	SecretChannel(x)	((x) && ((x)->mode.mode & MODE_SECRET))
/* channel not shown but names are */
#define	HiddenChannel(x)	((x) && ((x)->mode.mode & MODE_PRIVATE))
/* channel visible */
#define	ShowChannel(v,c)	(PubChannel(c) || IsMember((v),(c)))
#define	PubChannel(x)		((!x) || ((x)->mode.mode &\
				 (MODE_PRIVATE | MODE_SECRET)) == 0)

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
	char *modebuf; /* curr pos */
	char *parabuf; /* curr pos */
	char buf[512]; /* internal parse buffer */
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

/* old.. please don't use anymore */
#define CHANOPPFX "@"

/* used for is_banned type field: */
#define BANCHK_JOIN		0	/* checking if a ban forbids the person from joining */
#define BANCHK_MSG		1	/* checking if a ban forbids the person from sending messages */
#define BANCHK_NICK		2	/* checking if a ban forbids the person from changing his/her nick */
#define BANCHK_LEAVE_MSG	3	/* checking if a ban forbids the person from leaving a message in PART or QUIT */
#define BANCHK_TKL		4	/* called from a server ban routine, or other match_user() usage */

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

#define MATCH_USE_IDENT             0x0100

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

#endif /* __struct_include__ */

#include "dynconf.h"
