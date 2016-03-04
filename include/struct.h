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
#include "hash.h"
#include <stdio.h>
#include <sys/types.h>
#ifndef _WIN32
#include <netinet/in.h>
#include <netdb.h>
#endif
#ifdef STDDEFH
# include <stddef.h>
#endif

#ifdef HAVE_SYSLOG
# include <syslog.h>
# ifdef SYSSYSLOGH
#  include <sys/syslog.h>
# endif
#endif
#include "auth.h" 
#include "tre/regex.h"
#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"

#include "channel.h"

#if defined(_MSC_VER)
/* needed to workaround a warning / prototype/dll inconsistency crap */
#define vsnprintf unrl_vsnprintf
#endif

extern MODVAR int sendanyways;


typedef struct aloopStruct LoopStruct;
typedef struct ConfItem aConfItem;
typedef struct t_kline aTKline;
typedef struct _spamfilter Spamfilter;
typedef struct _spamexcept SpamExcept;
/* New Config Stuff */
typedef struct _configentry ConfigEntry;
typedef struct _configfile ConfigFile;
typedef struct _configflag ConfigFlag;
typedef struct _configflag_except ConfigFlag_except;
typedef struct _configflag_ban ConfigFlag_ban;
typedef struct _configflag_tld ConfigFlag_tld;
typedef struct _configitem ConfigItem;
typedef struct _configitem_me ConfigItem_me;
typedef struct _configitem_files ConfigItem_files;
typedef struct _configitem_admin ConfigItem_admin;
typedef struct _configitem_class ConfigItem_class;
typedef struct _configitem_oper ConfigItem_oper;
typedef struct _configitem_operclass ConfigItem_operclass;
typedef struct _configitem_mask ConfigItem_mask;
typedef struct _configitem_drpass ConfigItem_drpass;
typedef struct _configitem_ulines ConfigItem_ulines;
typedef struct _configitem_tld ConfigItem_tld;
typedef struct _configitem_listen ConfigItem_listen;
typedef struct _configitem_allow ConfigItem_allow;
typedef struct _configflag_allow ConfigFlag_allow;
typedef struct _configitem_allow_channel ConfigItem_allow_channel;
typedef struct _configitem_allow_dcc ConfigItem_allow_dcc;
typedef struct _configitem_vhost ConfigItem_vhost;
typedef struct _configitem_except ConfigItem_except;
typedef struct _configitem_link	ConfigItem_link;
typedef struct _configitem_ban ConfigItem_ban;
typedef struct _configitem_deny_dcc ConfigItem_deny_dcc;
typedef struct _configitem_deny_link ConfigItem_deny_link;
typedef struct _configitem_deny_channel ConfigItem_deny_channel;
typedef struct _configitem_deny_version ConfigItem_deny_version;
typedef struct _configitem_log ConfigItem_log;
typedef struct _configitem_unknown ConfigItem_unknown;
typedef struct _configitem_unknown_ext ConfigItem_unknown_ext;
typedef struct _configitem_alias ConfigItem_alias;
typedef struct _configitem_alias_format ConfigItem_alias_format;
typedef struct _configitem_include ConfigItem_include;
typedef struct _configitem_help ConfigItem_help;
typedef struct _configitem_offchans ConfigItem_offchans;
typedef struct liststruct ListStruct;
typedef struct liststructprio ListStructPrio;

#define CFG_TIME 0x0001
#define CFG_SIZE 0x0002
#define CFG_YESNO 0x0004

typedef struct Watch aWatch;
typedef struct Client aClient;
typedef struct LocalClient aLocalClient;
typedef struct Channel aChannel;
typedef struct User anUser;
typedef struct Server aServer;
typedef struct SLink Link;
typedef struct SBan Ban;
typedef struct SMode Mode;
typedef struct ListOptions LOpts;
typedef struct Motd aMotdFile; /* represents a whole MOTD, including remote MOTD support info */
typedef struct MotdItem aMotdLine; /* one line of a MOTD stored as a linked list */
#ifdef USE_LIBCURL
typedef struct MotdDownload aMotdDownload; /* used to coordinate download of a remote MOTD */
#endif

typedef struct trecord aTrecord;
typedef struct Command aCommand;
typedef struct _cmdoverride Cmdoverride;
typedef struct SMember Member;
typedef struct SMembership Membership;
typedef struct SMembershipL MembershipL;

#ifdef NEED_U_INT32_T
typedef unsigned int u_int32_t;	/* XXX Hope this works! */
#endif

typedef enum OperClassEntryType { OPERCLASSENTRY_ALLOW=1, OPERCLASSENTRY_DENY=2} OperClassEntryType;

typedef enum OperPermission { OPER_ALLOW=1, OPER_DENY=0} OperPermission;

struct _operClass_Validator;
typedef struct _operClass_Validator OperClassValidator;
typedef struct _operClassACLPath OperClassACLPath;
typedef struct _operClass OperClass;
typedef struct _operClassACL OperClassACL;
typedef struct _operClassACLEntry OperClassACLEntry;
typedef struct _operClassACLEntryVar OperClassACLEntryVar;
typedef struct _operClassCheckParams OperClassCheckParams;

typedef OperPermission (*OperClassEntryEvalCallback)(OperClassACLEntryVar* variables,OperClassCheckParams* params);

#ifndef VMSP
#include "class.h"
#include "dbuf.h"		/* THIS REALLY SHOULDN'T BE HERE!!! --msa */
#endif

#define	HOSTLEN		63	/* Length of hostname.  Updated to         */
				/* comply with RFC1123                     */

#define	NICKLEN		30
#define	USERLEN		10
#define	REALLEN	 	50
#define SVIDLEN		30
#define	TOPICLEN	307
#define	CHANNELLEN	32
#define	PASSWDLEN 	48	/* was 20, then 32, now 48. */
#define	KEYLEN		23
#define LINKLEN		32
#define	BUFSIZE		512	/* WARNING: *DONT* CHANGE THIS!!!! */
#define	MAXRECIPIENTS 	20
#define	MAXKILLS	20
#define	MAXSILELENGTH	NICKLEN+USERLEN+HOSTLEN+10
#define IDLEN		10
#define SWHOISLEN	256
#define UMODETABLESZ (sizeof(long) * 8)
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
#define LOG_DBG    0x0800 /* fixme */

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

/*
** flags for bootup options (command line flags)
*/
#define	BOOT_CONSOLE	1
#define	BOOT_QUICK	2
#define	BOOT_DEBUG	4
#define	BOOT_INETD	8
#define	BOOT_TTY	16
#define	BOOT_OPER	32
#define	BOOT_AUTODIE	64
#define BOOT_NOFORK     128

#define	STAT_LOG	-7	/* logfile for -x */
#define	STAT_CONNECTING	-6
#define STAT_SSL_STARTTLS_HANDSHAKE -8
#define STAT_SSL_CONNECT_HANDSHAKE -5
#define STAT_SSL_ACCEPT_HANDSHAKE -4
#define	STAT_HANDSHAKE	-3
#define	STAT_ME		-2
#define	STAT_UNKNOWN	-1
#define	STAT_SERVER	0
#define	STAT_CLIENT	1

/*
 * status macros.
 */
#define	IsRegisteredUser(x)	((x)->status == STAT_CLIENT)
#define	IsRegistered(x)		((x)->status >= STAT_SERVER)
#define	IsConnecting(x)		((x)->status == STAT_CONNECTING)
#define	IsHandshake(x)		((x)->status == STAT_HANDSHAKE)
#define	IsMe(x)			((x)->status == STAT_ME)
#define	IsUnknown(x)		((x)->status == STAT_UNKNOWN)
#define	IsServer(x)		((x)->status == STAT_SERVER)
#define	IsClient(x)		((x)->status == STAT_CLIENT)
#define	IsLog(x)		((x)->status == STAT_LOG)

#define IsSSLStartTLSHandshake(x)	((x)->status == STAT_SSL_STARTTLS_HANDSHAKE)
#define IsSSLAcceptHandshake(x)	((x)->status == STAT_SSL_ACCEPT_HANDSHAKE)
#define IsSSLConnectHandshake(x)	((x)->status == STAT_SSL_CONNECT_HANDSHAKE)
#define IsSSLHandshake(x) (IsSSLAcceptHandshake(x) || IsSSLConnectHandshake(x) | IsSSLStartTLSHandshake(x))
#define SetSSLStartTLSHandshake(x)	((x)->status = STAT_SSL_STARTTLS_HANDSHAKE)
#define SetSSLAcceptHandshake(x)	((x)->status = STAT_SSL_ACCEPT_HANDSHAKE)
#define SetSSLConnectHandshake(x)	((x)->status = STAT_SSL_CONNECT_HANDSHAKE)

#define	SetConnecting(x)	((x)->status = STAT_CONNECTING)
#define	SetHandshake(x)		((x)->status = STAT_HANDSHAKE)
#define	SetMe(x)		((x)->status = STAT_ME)
#define	SetUnknown(x)		((x)->status = STAT_UNKNOWN)
#define	SetServer(x)		((x)->status = STAT_SERVER)
#define	SetClient(x)		((x)->status = STAT_CLIENT)
#define	SetLog(x)		((x)->status = STAT_LOG)

#define IsSynched(x)	(x->serv->flags.synced)
#define IsServerSent(x) (x->serv && x->serv->flags.server_sent)

/* client->flags (32 bits): 28 used, 4 free */
#define	FLAGS_PINGSENT   0x0001	/* Unreplied ping sent */
#define	FLAGS_DEADSOCKET 0x0002	/* Local socket is dead--Exiting soon */
#define	FLAGS_KILLED     0x0004	/* Prevents "QUIT" from being sent for this */
#define FLAGS_IPV6       0x0008 /* For quick checking */
#define FLAGS_OUTGOING   0x0010 /* outgoing connection, do not touch cptr->listener->clients */
#define	FLAGS_CLOSING    0x0020	/* set when closing to suppress errors */
#define	FLAGS_LISTEN     0x0040	/* used to mark clients which we listen() on */
#define	FLAGS_CHKACCESS  0x0080	/* ok to check clients access if set */
#define	FLAGS_DOINGDNS	 0x0100	/* client is waiting for a DNS response */
#define	FLAGS_AUTH       0x0200	/* client is waiting on rfc931 response */
#define	FLAGS_WRAUTH	 0x0400	/* set if we havent writen to ident server */
#define	FLAGS_LOCAL      0x0800	/* set for local clients */
#define	FLAGS_GOTID      0x1000	/* successful ident lookup achieved */
#define	FLAGS_DOID       0x2000	/* I-lines say must use ident return */
#define	FLAGS_NONL       0x4000	/* No \n in buffer */
#define FLAGS_NCALL      0x8000 /* Next call (don't ask...) */
#define FLAGS_ULINE      0x10000	/* User/server is considered U-lined */
#define FLAGS_SQUIT      0x20000	/* Server has been /squit by an oper */
#define FLAGS_PROTOCTL   0x40000	/* Received a PROTOCTL message */
#define FLAGS_PING       0x80000
#define FLAGS_EAUTH      0x100000
#define FLAGS_NETINFO    0x200000
//0x400000 was hybnotice
#define FLAGS_QUARANTINE 0x800000
//0x1000000 unused (was ziplinks)
#define FLAGS_DCCNOTICE  0x2000000 /* Has the user seen a notice on how to use DCCALLOW already? */
#define FLAGS_SHUNNED    0x4000000
#define FLAGS_VIRUS      0x8000000 /* tagged by spamfilter */
#define FLAGS_SSL        0x10000000
#define FLAGS_NOFAKELAG  0x20000000 /* Exception from fake lag */
#define FLAGS_DCCBLOCK   0x40000000 /* Block all DCC send requests */
#define FLAGS_MAP        0x80000000	/* Show this entry in /map */
/* Dec 26th, 1997 - added flags2 when I ran out of room in flags -DuffJ */

/* Dec 26th, 1997 - having a go at
 * splitting flags into flags and umodes
 * -DuffJ
 */

#define SNO_DEFOPER "+kscfvGqob"
#define SNO_DEFUSER "+ks"

#define SEND_UMODES (SendUmodes)
#define ALL_UMODES (AllUmodes)
/* SEND_UMODES and ALL_UMODES are now handled by umode_get/umode_lget/umode_gget -- Syzop. */

#define	FLAGS_ID	(FLAGS_DOID|FLAGS_GOTID)

#define PROTO_NOQUIT	0x0001	/* Negotiated NOQUIT protocol */
#define PROTO_SJOIN		0x0004	/* Negotiated SJOIN protocol */
#define PROTO_NICKv2	0x0008	/* Negotiated NICKv2 protocol */
#define PROTO_SJOIN2	0x0010	/* Negotiated SJOIN2 protocol */
#define PROTO_UMODE2	0x0020	/* Negotiated UMODE2 protocol */
#define PROTO_TKLEXT2	0x0040	/* TKL extension 2: 11 parameters instead of 8 or 10 */
#define PROTO_INVITENOTIFY	0x0080	/* client supports invite-notify */
#define PROTO_VL		0x0100	/* Negotiated VL protocol */
#define PROTO_SJ3		0x0200	/* Negotiated SJ3 protocol */
#define PROTO_VHP		0x0400	/* Send hostnames in NICKv2 even if not sethosted */
#define PROTO_SID	0x0800	/* SID/UID mode */
#define PROTO_TKLEXT	0x1000	/* TKL extension: 10 parameters instead of 8 (3.2RC2) */
#define PROTO_NICKIP	0x2000  /* Send IP addresses in the NICK command */
#define PROTO_NAMESX	0x4000  /* Send all rights in NAMES output */
#define PROTO_CLK		0x8000	/* Send cloaked host in the NICK command (regardless of +x/-x) */
#define PROTO_UHNAMES	0x10000  /* Send n!u@h in NAMES */
#define PROTO_CLICAP	0x20000  /* client capability negotiation in process */
#define PROTO_STARTTLS	0x40000	 /* client supports STARTTLS */
#define PROTO_SASL	0x80000  /* client is doing SASL */
#define PROTO_AWAY_NOTIFY	0x100000	/* client supports away-notify */
#define PROTO_ACCOUNT_NOTIFY	0x200000	/* client supports account-notify */
#define PROTO_MLOCK		0x400000	/* server supports MLOCK */
#define PROTO_EXTSWHOIS 0x800000	/* extended SWHOIS support */

/*
 * flags macros.
 */
#define IsDeaf(x)               ((x)->umodes & UMODE_DEAF)
#define IsKillsF(x)		((x)->user->snomask & SNO_KILLS)
#define IsClientF(x)		((x)->user->snomask & SNO_CLIENT)
#define IsFloodF(x)		((x)->user->snomask & SNO_FLOOD)
#define IsEyes(x)		((x)->user->snomask & SNO_EYES)
#define	IsOper(x)		((x)->umodes & UMODE_OPER)
#define	IsInvisible(x)		((x)->umodes & UMODE_INVISIBLE)
#define IsARegNick(x)		((x)->umodes & (UMODE_REGNICK))
#define IsRegNick(x)		((x)->umodes & UMODE_REGNICK)
#define IsLoggedIn(x)		(IsRegNick(x) || (x->user && (*x->user->svid != '*') && !isdigit(*x->user->svid))) /* registered nick (+r) or just logged into services (may be -r) */
#define	IsPerson(x)		((x)->user && IsClient(x))
#define	SendWallops(x)		(!IsMe(x) && IsPerson(x) && ((x)->umodes & UMODE_WALLOP))
#define	SendServNotice(x)	(((x)->user) && ((x)->user->snomask & SNO_SNOTICE))
#define	IsListening(x)		((x)->flags & FLAGS_LISTEN)
// #define	DoAccess(x)		((x)->flags & FLAGS_CHKACCESS)
#define	IsLocal(x)		((x)->flags & FLAGS_LOCAL)
#define	IsDead(x)		((x)->flags & FLAGS_DEADSOCKET)
#define GotProtoctl(x)		((x)->flags & FLAGS_PROTOCTL)
#define IsOutgoing(x)		((x)->flags & FLAGS_OUTGOING)
#define GotNetInfo(x) 		((x)->flags & FLAGS_NETINFO)
#define SetNetInfo(x)		((x)->flags |= FLAGS_NETINFO)
#define SetEAuth(x)		((x)->flags |= FLAGS_EAUTH)
#define IsEAuth(x)		((x)->flags & FLAGS_EAUTH)
#define IsShunned(x)		((x)->flags & FLAGS_SHUNNED)
#define SetShunned(x)		((x)->flags |= FLAGS_SHUNNED)
#define ClearShunned(x)		((x)->flags &= ~FLAGS_SHUNNED)
#define IsVirus(x)			((x)->flags & FLAGS_VIRUS)
#define SetVirus(x)			((x)->flags |= FLAGS_VIRUS)
#define ClearVirus(x)		((x)->flags &= ~FLAGS_VIRUS)
#define IsSecure(x)		((x)->flags & FLAGS_SSL)

/* Fake lag exception */
#define IsNoFakeLag(x)      ((x)->flags & FLAGS_NOFAKELAG)
#define SetNoFakeLag(x)     ((x)->flags |= FLAGS_NOFAKELAG)
#define ClearNoFakeLag(x)   ((x)->flags &= ~FLAGS_NOFAKELAG)

#define IsHidden(x)             ((x)->umodes & UMODE_HIDE)
#define IsSetHost(x)			((x)->umodes & UMODE_SETHOST)
#define IsHideOper(x)		((x)->umodes & UMODE_HIDEOPER)
#define IsSSL(x)		IsSecure(x)
#define	IsNotSpoof(x)		((x)->local->nospoof == 0)

#define GetHost(x)			(IsHidden(x) ? (x)->user->virthost : (x)->user->realhost)
#define GetIP(x)			(x->ip ? x->ip : "255.255.255.255")

#define SetKillsF(x)		((x)->user->snomask |= SNO_KILLS)
#define SetClientF(x)		((x)->user->snomask |= SNO_CLIENT)
#define SetFloodF(x)		((x)->user->snomask |= SNO_FLOOD)
#define	SetOper(x)		((x)->umodes |= UMODE_OPER)
#define	SetInvisible(x)		((x)->umodes |= UMODE_INVISIBLE)
#define SetEyes(x)		((x)->user->snomask |= SNO_EYES)
#define	SetWallops(x)  		((x)->umodes |= UMODE_WALLOP)
#define	SetDNS(x)		((x)->flags |= FLAGS_DOINGDNS)
#define	DoingDNS(x)		((x)->flags & FLAGS_DOINGDNS)
#define	SetAccess(x)		((x)->flags |= FLAGS_CHKACCESS); Debug((DEBUG_DEBUG, "SetAccess(%s)", (x)->name))
#define SetOutgoing(x)		do { x->flags |= FLAGS_OUTGOING; } while(0)
#define	DoingAuth(x)		((x)->flags & FLAGS_AUTH)
#define	NoNewLine(x)		((x)->flags & FLAGS_NONL)
#define IsDCCNotice(x)		((x)->flags & FLAGS_DCCNOTICE)
#define SetDCCNotice(x)		do { x->flags |= FLAGS_DCCNOTICE; } while(0)
#define SetRegNick(x)		((x)->umodes & UMODE_REGNICK)
#define SetHidden(x)            ((x)->umodes |= UMODE_HIDE)
#define SetHideOper(x)      ((x)->umodes |= UMODE_HIDEOPER)
#define IsSecureConnect(x)	((x)->umodes & UMODE_SECURE)
#define ClearKillsF(x)		((x)->user->snomask &= ~SNO_KILLS)
#define ClearClientF(x)		((x)->user->snomask &= ~SNO_CLIENT)
#define ClearFloodF(x)		((x)->user->snomask &= ~SNO_FLOOD)
#define ClearEyes(x)		((x)->user->snomask &= ~SNO_EYES)
#define	ClearOper(x)		((x)->umodes &= ~UMODE_OPER)
#define	ClearInvisible(x)	((x)->umodes &= ~UMODE_INVISIBLE)
#define	ClearWallops(x)		((x)->umodes &= ~UMODE_WALLOP)
#define	ClearDNS(x)		((x)->flags &= ~FLAGS_DOINGDNS)
#define	ClearAuth(x)		((x)->flags &= ~FLAGS_AUTH)
#define	ClearAccess(x)		((x)->flags &= ~FLAGS_CHKACCESS)
#define ClearHidden(x)          ((x)->umodes &= ~UMODE_HIDE)
#define ClearHideOper(x)    ((x)->umodes &= ~UMODE_HIDEOPER)

#define SetIPV6(x)			do { x->flags |= FLAGS_IPV6; } while(0)
#define IsIPV6(x)			((x)->flags & FLAGS_IPV6)
/*
 * ProtoCtl options
 */
#ifndef DEBUGMODE
#define CHECKPROTO(x,y)	(((x)->local->proto & y) == y)
#else
#define CHECKPROTO(x,y) (checkprotoflags(x, y, __FILE__, __LINE__))
#endif

#define DontSendQuit(x)		(CHECKPROTO(x, PROTO_NOQUIT))
#define SupportSJOIN(x)		(CHECKPROTO(x, PROTO_SJOIN))
#define SupportNICKv2(x)	(CHECKPROTO(x, PROTO_NICKv2))
#define SupportNICKIP(x)	(CHECKPROTO(x, PROTO_NICKIP))
#define SupportSJOIN2(x)	(CHECKPROTO(x, PROTO_SJOIN2))
#define SupportUMODE2(x)	(CHECKPROTO(x, PROTO_UMODE2))
#define SupportVL(x)		(CHECKPROTO(x, PROTO_VL))
#define SupportSJ3(x)		(CHECKPROTO(x, PROTO_SJ3))
#define SupportVHP(x)		(CHECKPROTO(x, PROTO_VHP))
#define SupportTKLEXT(x)	(CHECKPROTO(x, PROTO_TKLEXT))
#define SupportTKLEXT2(x)	(CHECKPROTO(x, PROTO_TKLEXT2))
#define SupportNAMESX(x)	(CHECKPROTO(x, PROTO_NAMESX))
#define SupportCLK(x)		(CHECKPROTO(x, PROTO_CLK))
#define SupportUHNAMES(x)	(CHECKPROTO(x, PROTO_UHNAMES))
#define SupportSID(x)		(CHECKPROTO(x, PROTO_SID))

#define SetSJOIN(x)		((x)->local->proto |= PROTO_SJOIN)
#define SetNoQuit(x)		((x)->local->proto |= PROTO_NOQUIT)
#define SetNICKv2(x)		((x)->local->proto |= PROTO_NICKv2)
#define SetSJOIN2(x)		((x)->local->proto |= PROTO_SJOIN2)
#define SetUMODE2(x)		((x)->local->proto |= PROTO_UMODE2)
#define SetVL(x)		((x)->local->proto |= PROTO_VL)
#define SetSJ3(x)		((x)->local->proto |= PROTO_SJ3)
#define SetVHP(x)		((x)->local->proto |= PROTO_VHP)
#define SetTKLEXT(x)	((x)->local->proto |= PROTO_TKLEXT)
#define SetTKLEXT2(x)	((x)->local->proto |= PROTO_TKLEXT2)
#define SetNAMESX(x)	((x)->local->proto |= PROTO_NAMESX)
#define SetCLK(x)		((x)->local->proto |= PROTO_CLK)
#define SetUHNAMES(x)	((x)->local->proto |= PROTO_UHNAMES)

#define ClearSJOIN(x)		((x)->local->proto &= ~PROTO_SJOIN)
#define ClearNoQuit(x)		((x)->local->proto &= ~PROTO_NOQUIT)
#define ClearNICKv2(x)		((x)->local->proto &= ~PROTO_NICKv2)
#define ClearSJOIN2(x)		((x)->local->proto &= ~PROTO_SJOIN2)
#define ClearUMODE2(x)		((x)->local->proto &= ~PROTO_UMODE2)
#define ClearVL(x)		((x)->local->proto &= ~PROTO_VL)
#define ClearVHP(x)		((x)->local->proto &= ~PROTO_VHP)
#define ClearSJ3(x)		((x)->local->proto &= ~PROTO_SJ3)
#define ClearTKLEXT(x)		((x)->local->proto &= ~PROTO_TKLEXT)
#define ClearTKLEXT2(x)		((x)->local->proto &= ~PROTO_TKLEXT2)

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

#define ID(sptr)	(*sptr->id ? sptr->id : sptr->name)

/* Maximum number of moddata objects that may be attached to an object -- maybe move to config.h? */
#define MODDATA_MAX_CLIENT 8
#define MODDATA_MAX_CHANNEL 8
#define MODDATA_MAX_MEMBER 4
#define MODDATA_MAX_MEMBERSHIP 4

/** Union for moddata objects */
typedef union _moddata ModData;
union _moddata
{
        int i;
        long l;
        char *str;
        void *ptr;
};

#ifdef USE_LIBCURL
struct Motd;
struct MotdDownload
{
	struct Motd *themotd;
};
#endif /* USE_LIBCURL */

struct Motd 
{
	struct MotdItem *lines;
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
	  1. read_motd() is called with a URL. A new MotdDownload is
	     allocated and the pointer is placed here. This pointer is
	     also passed to the asynchrnous download handler.
	  2.a. The download is completed and read_motd_asynch_downloaded()
	       is called with the same pointer. From this function, this pointer
	       if free()d. No other code may free() the pointer. Not even free_motd().
	    OR
	  2.b. The user rehashes the IRCd before the download is completed.
	       free_motd() is called, which sets motd_download->themotd to NULL
	       to signal to read_motd_asynch_downloaded() that it should ignore
	       the download. read_motd_asynch_downloaded() is eventually called
	       and frees motd_download.
	 */
	struct MotdDownload *motd_download;
#endif /* USE_LIBCURL */
};

struct MotdItem {
	char *line;
	struct MotdItem *next;
};

struct aloopStruct {
	unsigned do_garbage_collect : 1;
	unsigned ircd_booted : 1;
	unsigned ircd_forked : 1;
	unsigned do_bancheck : 1; /* perform *line bancheck? */
	unsigned do_bancheck_spamf_user : 1; /* perform 'user' spamfilter bancheck */
	unsigned do_bancheck_spamf_away : 1; /* perform 'away' spamfilter bancheck */
	unsigned ircd_rehashing : 1;
	unsigned tainted : 1;
	aClient *rehash_save_cptr, *rehash_save_sptr;
	int rehash_save_sig;
};

/** Matching types for aMatch.type */
typedef enum {
	MATCH_SIMPLE=1, /**< Simple pattern with * and ? */
	MATCH_PCRE_REGEX=2, /**< PCRE2 Perl-like regex (new) */
#ifdef USE_TRE
	MATCH_TRE_REGEX=3, /**< TRE POSIX regex (old, unreal3.2.x) */
#endif
} MatchType;

/** Match struct, which allows various matching styles, see MATCH_* */
typedef struct _match {
	char *str; /**< Text of the glob/regex/whatever. Always set. */
	MatchType type;
	union {
		pcre2_code *pcre2_expr; /**< PCRE2 Perl-like Regex (New) */
#ifdef USE_TRE
		regex_t *tre_expr; /**< TRE POSIX Regex (Old) */
#endif
	} ext;
} aMatch;

typedef struct Whowas {
	int  hashv;
	char *name;
	char *username;
	char *hostname;
	char *virthost;
	char *servername;
	char *realname;
	long umodes;
	TS   logoff;
	struct Client *online;	/* Pointer to new nickname for chasing or NULL */
	struct Whowas *next;	/* for hash table... */
	struct Whowas *prev;	/* for hash table... */
	struct Whowas *cnext;	/* for client struct linked list */
	struct Whowas *cprev;	/* for client struct linked list */
} aWhowas;

typedef struct _swhois SWhois;
struct _swhois {
	SWhois *prev, *next;
	int priority;
	char *line;
	char *setby;
};
	
/*
 * Client structures
 */
struct User {
	Membership *channel;		/* chain of channel pointer blocks */
	Link *invited;		/* chain of invite pointer blocks */
	Link *silence;		/* chain of silence pointer blocks */
	Link *dccallow;		/* chain of dccallowed entries */
	char *away;		/* pointer to away message */

	/*
	 * svid: a value that is assigned by services to this user record.
	 * in previous versions of Unreal, this was strictly a timestamp value,
	 * which is less useful in the modern world of IRC where nicks are grouped to
	 * accounts, so it is now a string.
	 */
	char svid[SVIDLEN + 1];

	signed char refcnt;	/* Number of times this block is referenced */
	unsigned short joined;		/* number of channels joined */
	char username[USERLEN + 1];
	char realhost[HOSTLEN + 1];
	char cloakedhost[HOSTLEN + 1]; /* cloaked host (masked host for caching). NOT NECESSARILY THE SAME AS virthost. */
	char *virthost;
	char *server;
	SWhois *swhois; /* special whois entries */
	LOpts *lopt;            /* Saved /list options */
	aWhowas *whowas;
	int snomask;
#ifdef	LIST_DEBUG
	aClient *bcptr;
#endif
	char *operlogin;	/* Only used if person is/was opered, used for oper::maxlogins */
	struct {
		time_t nick_t;
		unsigned char nick_c;
#ifdef NO_FLOOD_AWAY
		time_t away_t;			/* last time the user set away */
		unsigned char away_c;	/* number of times away has been set */
#endif
	} flood;
	TS lastaway;
};

struct Server {
	struct Server 	*nexts;
	anUser 		*user;		/* who activated this connection */
	char 		*up;		/* uplink for this server */
	char 		by[NICKLEN + 1];
	ConfigItem_link *conf;
	TS   		timestamp;		/* Remotely determined connect try time */
	long		 users;
#ifdef	LIST_DEBUG
	aClient *bcptr;
#endif
	struct {
		unsigned synced:1;		/* Server linked? (3.2beta18+) */
		unsigned server_sent:1;		/* SERVER message sent to this link? (for outgoing links) */
	} flags;
	struct {
		char *chanmodes[4];
		int protocol;
	} features;
};

#define M_UNREGISTERED	0x0001
#define M_USER			0x0002
#define M_SERVER		0x0004
#define M_SHUN			0x0008
#define M_NOLAG			0x0010
#define M_ALIAS			0x0020
#define M_RESETIDLE		0x0040
#define M_VIRUS			0x0080
#define M_ANNOUNCE		0x0100
#define M_OPER			0x0200


/* tkl:
 *   TKL_KILL|TKL_GLOBAL 	= Global K:Line (G:Line)
 *   TKL_ZAP|TKL_GLOBAL		= Global Z:Line (ZLINE)
 *   TKL_KILL			= Timed local K:Line
 *   TKL_ZAP			= Local Z:Line
 */
#define TKL_KILL	0x0001
#define TKL_ZAP		0x0002
#define TKL_GLOBAL	0x0004
#define TKL_SHUN	0x0008
#define TKL_QUIET	0x0010
#define TKL_SPAMF	0x0020
#define TKL_NICK	0x0040

#define SPAMF_CHANMSG		0x0001 /* c */
#define SPAMF_USERMSG		0x0002 /* p */
#define SPAMF_USERNOTICE	0x0004 /* n */
#define SPAMF_CHANNOTICE	0x0008 /* N */
#define SPAMF_PART			0x0010 /* P */
#define SPAMF_QUIT			0x0020 /* q */
#define SPAMF_DCC			0x0040 /* d */
#define SPAMF_USER			0x0080 /* u */
#define SPAMF_AWAY			0x0100 /* a */
#define SPAMF_TOPIC			0x0200 /* t */

/* Other flags only for function calls: */
#define SPAMFLAG_NOWARN		0x0001

struct _spamfilter {
	unsigned short action; /* see BAN_ACT* */
	aMatch *expr;
	char *tkl_reason; /* spamfilter reason field [escaped by unreal_encodespace()!] */
	TS tkl_duration;
};

struct t_kline {
	aTKline *prev, *next;
	int type;
	unsigned short subtype; /* subtype (currently spamfilter only), see SPAMF_* */
	union {
		Spamfilter *spamf;
	} ptr;
	char usermask[USERLEN + 3];
	char *hostmask, *reason, *setby;
	TS expire_at, set_at;
};

struct _spamexcept {
	SpamExcept *prev, *next;
	char name[1];
};

typedef struct ircstatsx {
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
} ircstats;

extern MODVAR ircstats IRCstats;

#include "modules.h"

extern MODVAR Umode *Usermode_Table;
extern MODVAR short	 Usermode_highest;

extern MODVAR Snomask *Snomask_Table;
extern MODVAR short Snomask_highest;

extern MODVAR Cmode *Channelmode_Table;
extern MODVAR unsigned short Channelmode_highest;

extern Umode *UmodeAdd(Module *module, char ch, int options, int unset_on_deoper, int (*allowed)(aClient *sptr, int what), long *mode);
extern void UmodeDel(Umode *umode);

extern Snomask *SnomaskAdd(Module *module, char ch, int unset_on_deoper, int (*allowed)(aClient *sptr, int what), long *mode);
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
#define LISTENER_SSL		0x000010
#define LISTENER_BOUND		0x000020
#define LISTENER_DEFER_ACCEPT	0x000040

#define IsServersOnlyListener(x)	((x) && ((x)->options & LISTENER_SERVERSONLY))

#define CONNECT_SSL		0x000001
//0x000002 unused (was ziplinks)
#define CONNECT_AUTO		0x000004
#define CONNECT_QUARANTINE	0x000008
#define CONNECT_NODNSCACHE	0x000010
#define CONNECT_NOHOSTCHECK	0x000020
#define CONNECT_INSECURE	0x000040

#define SSLFLAG_FAILIFNOCERT 	0x1
#define SSLFLAG_VERIFYCERT 	0x2
#define SSLFLAG_DONOTACCEPTSELFSIGNED 0x4
#define SSLFLAG_NOSTARTTLS	0x8

struct Client {
	struct list_head client_node; 	/* for global client list (client_list) */
	struct list_head client_hash;	/* for clientTable */
	struct list_head id_hash;	/* for idTable */
	aLocalClient *local;	/* for locally connected clients */
	anUser *user;		/* ...defined, if this is a User */
	aServer *serv;		/* ...defined, if this is a server */
	TS   lastnick;		/* TimeStamp on nick */
	long flags;		/* client flags */
	long umodes;		/* client usermodes */
	aClient *from;		/* == &me, if Local Client, *NEVER* NULL! */
	int  fd;		/* >= 0, for local clients */
	unsigned char hopcount;		/* number of servers to this 0 = local */
	char name[HOSTLEN + 1];	/* Unique name of the client, nick or host */
	char username[USERLEN + 1];	/* username here now for auth stuff */
	char info[REALLEN + 1];	/* Free form additional client information */
	char id[IDLEN + 1];	/* SID or UID */
	aClient *srvptr;	/* Server introducing this.  May be &me */
	short status;		/* client type */
	ModData moddata[MODDATA_MAX_CLIENT]; /* for modules */
	/*
	   ** The following fields are allocated only for local clients
	   ** (directly connected to *this* server with a socket.
	   ** The first of them *MUST* be the "count"--it is the field
	   ** to which the allocation is tied to! *Never* refer to
	   ** these fields, if (from != self).
	 */
	int  count;		/* Amount of data in buffer */

	struct list_head lclient_node;	/* for local client list (lclient_list) */
	struct list_head special_node;	/* for special lists (server || unknown || oper) */
	
	char *ip; /* IP of user or server */
};

struct LocalClient {
	TS   since;		/* time they will next be allowed to send something */
	TS   firsttime;		/* Time it was created */
	TS   lasttime;		/* last time any message was received */
	TS   last;		/* last time a RESETIDLE message was received */
	TS   nexttarget;	/* next time that a new target will be allowed (msg/notice/invite) */
 	TS   nextnick;		/* Time the next nick change will be allowed */
	u_char targets[MAXTARGETS];	/* hash values of targets */
	char buffer[BUFSIZE];	/* Incoming message buffer */
	short lastsq;		/* # of 2k blocks when sendqueued called last */
	dbuf sendQ;		/* Outgoing message queue--if socket full */
	dbuf recvQ;		/* Hold for data incoming yet to be parsed */
	u_int32_t nospoof;	/* Anti-spoofing random number */
	int proto;		/* ProtoCtl options */
	long sendM;		/* Statistics: protocol messages send */
	long sendK;		/* Statistics: total k-bytes send */
	long receiveM;		/* Statistics: protocol messages received */
	SSL		*ssl;
	long receiveK;		/* Statistics: total k-bytes received */
	u_short sendB;		/* counters to count upto 1-k lots of bytes */
	u_short receiveB;	/* sent and received. */
	ConfigItem_listen *listener;
	ConfigItem_class *class;		/* Configuration record associated */
	int authfd;		/* fd for rfc931 authentication */
        long serial;            /* current serial for send.c functions */
	u_short port;		/* remote port of client */
	struct hostent *hostp;
	u_short watches;	/* Keep track of count of notifies */
	Link *watch;		/* Links to clients notify-structures */
	char sockhost[HOSTLEN + 1];	/* This is the host name from the socket
					   ** and after which the connection was
					   ** accepted.
					 */
	char *passwd;
#ifdef DEBUGMODE
	TS   cputime;
#endif
	char *error_str;	/* Quit reason set by dead_link in case of socket/buffer error */

	char sasl_agent[NICKLEN + 1];
	unsigned char sasl_out;
	unsigned char sasl_complete;
	u_short sasl_cookie;
};


#define	CLIENT_LOCAL_SIZE sizeof(aClient)
#define	CLIENT_REMOTE_SIZE offsetof(aClient,count)

/*
 * conf2 stuff -stskeeps
*/

/* Config flags */
 
struct _configfile
{
        char            *cf_filename;
        ConfigEntry     *cf_entries;
        ConfigFile     *cf_next;
};

struct _configentry
{
        ConfigFile	*ce_fileptr;
        int 	 	ce_varlinenum, ce_fileposstart, ce_fileposend, ce_sectlinenum;
        char 		*ce_varname, *ce_vardata;
        ConfigEntry     *ce_entries, *ce_prevlevel, *ce_next;
};

struct _configflag 
{
	unsigned	temporary : 1;
	unsigned	permanent : 1;
};

/* configflag specialized for except socks/ban -Stskeeps */

struct _configflag_except
{
	unsigned	temporary : 1;
	unsigned	type	  : 2;
};

struct _configflag_ban
{
	unsigned	temporary : 1;
	unsigned	type	  : 4;
	unsigned	type2	  : 2;
};

struct _configflag_tld
{
	unsigned	temporary : 1;
	unsigned	motdptr   : 1;
	unsigned	rulesptr  : 1;
};

#define CONF_BAN_NICK		1
#define CONF_BAN_IP		2
#define CONF_BAN_SERVER		3
#define CONF_BAN_USER   	4
#define CONF_BAN_REALNAME 	5
#define CONF_BAN_VERSION        6

#define CONF_BAN_TYPE_CONF	0
#define CONF_BAN_TYPE_AKILL	1
#define CONF_BAN_TYPE_TEMPORARY 2

/* Ban actions. These must be ordered by severity (!) */
#define BAN_ACT_GZLINE		1100
#define BAN_ACT_GLINE		1000
#define BAN_ACT_ZLINE		 900
#define BAN_ACT_KLINE		 800
#define BAN_ACT_SHUN		 700
#define BAN_ACT_KILL		 600
#define BAN_ACT_TEMPSHUN	 500
#define BAN_ACT_VIRUSCHAN	 400
#define BAN_ACT_DCCBLOCK	 300
#define BAN_ACT_BLOCK		 200
#define BAN_ACT_WARN		 100


#define CRULE_ALL		0
#define CRULE_AUTO		1

#define CONF_EXCEPT_BAN		1
#define CONF_EXCEPT_TKL		2
#define CONF_EXCEPT_THROTTLE	3


struct _configitem {
	ConfigFlag flag;
	ConfigItem *prev, *next;
};

struct _configitem_me {
	char	   *name, *info, *sid;
};

struct _configitem_files {
	char	*motd_file, *rules_file, *smotd_file;
	char	*botmotd_file, *opermotd_file, *svsmotd_file;
	char	*pid_file, *tune_file;
};

struct _configitem_admin {
	ConfigItem *prev, *next;
	ConfigFlag flag;
	char	   *line; 
};

#define CLASS_OPT_NOFAKELAG		0x1

struct _configitem_class {
	ConfigItem *prev, *next;
	ConfigFlag flag;
	char	   *name;
	int	   pingfreq, connfreq, maxclients, sendq, recvq, clients;
	int xrefcount; /* EXTRA reference count, 'clients' also acts as a reference count but
	                * link blocks also refer to classes so a 2nd ref. count was needed.
	                */
	unsigned int options;
};

struct _configflag_allow {
	unsigned	noident :1;
	unsigned	useip :1;
	unsigned	ssl :1;
	unsigned	nopasscont :1;
};

struct _configitem_allow {
	ConfigItem			*prev, *next;
	ConfigFlag			flag;
	char				*ip, *hostname, *server;
	anAuthStruct		*auth;	
	unsigned short		maxperip;
	int					port;
	ConfigItem_class	*class;
	ConfigFlag_allow	flags;
	unsigned short ipv6_clone_mask;
};

struct _operClassACLPath
{
	OperClassACLPath *prev,*next;
	char* identifier;
};

struct _operClassACLEntryVar
{
        OperClassACLEntryVar *prev,*next;
        char* name;
        char* value;
};

struct _operClassACLEntry
{
        OperClassACLEntry *prev,*next;
        OperClassACLEntryVar *variables;
        OperClassEntryType type;
};

struct _operClassACL
{
        OperClassACL *prev,*next;
        char *name;
        OperClassACLEntry *entries;
        OperClassACL *acls;
};

struct _operClass
{
        char *ISA;
        char *name;
        OperClassACL *acls;
};

struct _operClassCheckParams
{
        aClient *sptr;
        aClient *victim;
        aChannel *channel;
        void *extra;
};

struct _configitem_operclass {
	ConfigItem	*prev, *next;
	OperClass	*classStruct;
};

struct _configitem_oper {
	ConfigItem *prev, *next;
	ConfigFlag flag;
	char *name, *snomask;
	SWhois *swhois;
	anAuthStruct *auth;
	char *operclass;
	ConfigItem_class *class;
	ConfigItem_mask *mask;
	unsigned long modes, require_modes;
	char *vhost;
	int maxlogins;
};

struct _configitem_mask {
	ConfigItem_mask *prev, *next;
	ConfigFlag flag;
	char *mask;
};

struct _configitem_drpass {
	anAuthStruct	 *restartauth;
	anAuthStruct	 *dieauth;
};

struct _configitem_ulines {
	ConfigItem       *prev, *next;
	ConfigFlag 	 flag;
	char 		 *servername;
};

#define TLD_SSL		0x1
#define TLD_REMOTE	0x2

struct _configitem_tld {
	ConfigItem 	*prev, *next;
	ConfigFlag_tld 	flag;
	char 		*mask, *channel;
	char 		*motd_file, *rules_file, *smotd_file;
	char 		*botmotd_file, *opermotd_file;
	aMotdFile	rules, motd, smotd, botmotd, opermotd;
	u_short		options;
};

struct _configitem_listen {
	ConfigItem 	*prev, *next;
	ConfigFlag 	flag;
	char		*ip;
	int		port;
	int		options, clients;
	int		fd;
	int     ipv6;
};

struct _configitem_vhost {
	ConfigItem 	*prev, *next;
	ConfigFlag 	flag;
	ConfigItem_mask *mask;
	char		*login, *virthost, *virtuser;
	SWhois *swhois;
	anAuthStruct	*auth;
};

struct _configitem_link {
	ConfigItem	*prev, *next;
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
		int options; /**< Connect options like ssl or autoconnect */
	} outgoing;
	anAuthStruct *auth; /**< authentication method (eg: password) */
	char *hub; /**< Hub mask */
	char *leaf; /**< Leaf mask */
	int leaf_depth; /**< Leaf depth */
	ConfigItem_class *class; /**< Class the server should use */
	char *ciphers; /**< SSL Ciphers to use */
	int options; /**< Generic options such as quarantine */
	/* internal: */
	int	refcount; /**< Reference counter (used so we know if the struct may be freed) */
	time_t hold; /**< For how long the server is "on hold" for outgoing connects (why?) */
	char *connect_ip; /**< actual IP to use for outgoing connect (filled in after host is resolved) */
};

struct _configitem_except {
	ConfigItem      *prev, *next;
	ConfigFlag_except      flag;
	int type;
	char		*mask;
};

struct _configitem_ban {
	ConfigItem		*prev, *next;
	ConfigFlag_ban	flag;
	char			*mask, *reason;
	unsigned short action;
};

struct _configitem_deny_dcc {
	ConfigItem		*prev, *next;
	ConfigFlag_ban		flag;
	char			*filename, *reason;
};

struct _configitem_deny_link {
	ConfigItem_deny_link *prev, *next;
	ConfigFlag_except flag;
	char *mask, *rule, *prettyrule;
};

struct _configitem_deny_version {
	ConfigItem		*prev, *next;
	ConfigFlag		flag;
	char 			*mask, *version, *flags;
};

struct _configitem_deny_channel {
	ConfigItem		*prev, *next;
	ConfigFlag		flag;
	char			*channel, *reason, *redirect, *class;
	unsigned char	warn;
};

struct _configitem_allow_channel {
	ConfigItem		*prev, *next;
	ConfigFlag		flag;
	char			*channel, *class;
};

struct _configitem_allow_dcc {
	ConfigItem		*prev, *next;
	ConfigFlag_ban	flag;
	char			*filename;
};

struct _configitem_log {
	ConfigItem *prev, *next;
	ConfigFlag flag;
	char *file;
	long maxsize;
	int  flags;
	int  logfd;
};

struct _configitem_unknown {
	ConfigItem *prev, *next;
	ConfigFlag flag;
	ConfigEntry *ce;
};

struct _configitem_unknown_ext {
	ConfigItem *prev, *next;
	ConfigFlag flag;
	char *ce_varname, *ce_vardata;
	ConfigFile      *ce_fileptr;
	int             ce_varlinenum;
	ConfigEntry     *ce_entries;
};


typedef enum { 
	ALIAS_SERVICES=1, ALIAS_STATS, ALIAS_NORMAL, ALIAS_COMMAND, ALIAS_CHANNEL, ALIAS_REAL
} AliasType;

struct _configitem_alias {
	ConfigItem *prev, *next;
	ConfigFlag flag;
	ConfigItem_alias_format *format;
	char *alias, *nick;
	AliasType type;
	unsigned int spamfilter:1;
};

struct _configitem_alias_format {
	ConfigItem *prev, *next;
	ConfigFlag flag;
	char *nick;
	AliasType type;
	char *format, *parameters;
	aMatch *expr;
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
	
struct _configitem_include {
	ConfigItem *prev, *next;
	ConfigFlag_ban flag;
	char *file;
#ifdef USE_LIBCURL
	char *url;
	char *errorbuf;
#endif
	char *included_from;
	int included_from_line;
};

struct _configitem_help {
	ConfigItem *prev, *next;
	ConfigFlag flag;
	char *command;
	aMotdLine *text;
};

struct _configitem_offchans {
	ConfigItem *prev, *next;
	char chname[CHANNELLEN+1];
	char *topic;
};

#define HM_HOST 1
#define HM_IPV4 2
#define HM_IPV6 3

/*
 * statistics structures
 */
struct stats {
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
	TS   is_cti;		/* time spent connected by clients */
	TS   is_sti;		/* time spent connected by servers */
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

typedef struct _MemoryInfo {
	unsigned int classes;
	unsigned long classesmem;
} MemoryInfo;

struct ListOptions {
	LOpts *next;
	Link *yeslist, *nolist;
	unsigned int  starthash;
	short int showall;
	unsigned short usermin;
	int  usermax;
	TS   currenttime;
	TS   chantimemin;
	TS   chantimemax;
	TS   topictimemin;
	TS   topictimemax;
};

#define EXTCMODETABLESZ 32

/* Number of maximum paramter modes to allow.
 * Don't set it unnecessarily high.. we only use k, l, L, j and f at the moment. (FIXME)
 */
#define MAXPARAMMODES 16

/* mode structure for channels */
struct SMode {
	long mode;
	Cmode_t extmode;
	void *extmodeparams[MAXPARAMMODES+1];
	int  limit;
	char key[KEYLEN + 1];
};

/* Used for notify-hash buckets... -Donwulff */

struct Watch {
	aWatch *hnext;
	TS   lasttime;
	Link *watch;
	char nick[1];
};

/* general link structure used for chains */

struct SLink {
	struct SLink *next;
	int  flags;
	union {
		aClient *cptr;
		aChannel *chptr;
		ListStruct *aconf;
		aWatch *wptr;
		aName *whowas;
		char *cp;
		struct {
			char *banstr;
			char *who;
			TS   when;
		} ban;
	} value;
};

struct SMember
{
	struct SMember *next;
	aClient	      *cptr;
	int		flags;
	ModData moddata[MODDATA_MAX_MEMBER]; /* for modules */
};

struct Channel {
	struct Channel *nextch, *prevch, *hnextch;
	Mode mode;
	TS   creationtime;
	char *topic;
	char *topic_nick;
	TS   topic_time;
	int users;
	Member *members;
	Link *invites;
	Ban *banlist;
	Ban *exlist;		/* exceptions */
	Ban *invexlist;         /* invite list */
	char *mode_lock;
	ModData moddata[MODDATA_MAX_CHANNEL]; /* for modules */
	char chname[1];
};

/** user/channel membership struct for local clients */
struct SMembershipL
{
	struct SMembership 	*next;
	struct Channel		*chptr;
	int			flags;
	ModData moddata[MODDATA_MAX_MEMBERSHIP]; /* for modules */
};

/** user/channel membership struct for remote clients */
struct SMembership
{
	struct SMembership 	*next;
	struct Channel		*chptr;
	int			flags;
	ModData moddata[MODDATA_MAX_MEMBERSHIP]; /* for modules */
};

struct SBan {
	struct SBan *next;
	char *banstr;
	char *who;
	TS   when;
};

struct DSlink {
	struct DSlink *next;
	struct DSlink *prev;
	union {
		aClient *cptr;
		aChannel *chptr;
		ListStruct *aconf;
		char *cp;
	} value;
};
#define AddListItem(item,list) add_ListItem((ListStruct *)item, (ListStruct **)&list)
#define DelListItem(item,list) del_ListItem((ListStruct *)item, (ListStruct **)&list)

#define AddListItemPrio(item,list,prio) add_ListItemPrio((ListStructPrio *)item, (ListStructPrio **)&list, prio)
#define DelListItemPrio(item,list,prio) del_ListItem((ListStruct *)item, (ListStruct **)&list)

struct liststruct {
	ListStruct *prev, *next;
};

struct liststructprio {
	ListStructPrio *prev, *next;
	int priority;
};

/* channel structure */


/*
** Channel Related macros follow
*/

/* Channel related flags */

#define	CHFL_CHANOP     0x0001	/* Channel operator */
#define	CHFL_VOICE      0x0002	/* the power to speak */

#define	CHFL_DEOPPED	0x0004	/* Is de-opped by a server */
#define	CHFL_SERVOPOK   0x0008	/* Server op allowed */
#define	CHFL_ZOMBIE     0x0010	/* Kicked from channel */
/* Bans are stored in separate linked list, so phase this out? */
#define	CHFL_BAN     	0x0020	/* ban channel flag */
#define CHFL_CHANOWNER 	0x0040	/* channel owner */
#define CHFL_CHANPROT  	0x0080	/* chan op protection */
#define CHFL_HALFOP	0x0100	/* halfop */
#define CHFL_EXCEPT	0x0200	/* phase this out ? +e */
#define CHFL_INVEX	0x0400  /* invite exception */

#define CHFL_REJOINING	0x8000  /* used internally by rejoin_* */

#define	CHFL_OVERLAP    (CHFL_CHANOWNER|CHFL_CHANPROT|CHFL_CHANOP|CHFL_VOICE|CHFL_HALFOP)

/* Channel macros */

#define	MODE_CHANOP		CHFL_CHANOP
#define	MODE_VOICE		CHFL_VOICE
#define	MODE_PRIVATE		0x0004
#define	MODE_SECRET		0x0008
#define	MODE_MODERATED  	0x0010
#define	MODE_TOPICLIMIT 	0x0020
#define MODE_CHANOWNER		0x0040
#define MODE_CHANPROT		0x0080
#define	MODE_HALFOP		0x0100
#define MODE_EXCEPT		0x0200
#define	MODE_BAN		0x0400
#define	MODE_INVITEONLY 	0x0800
#define	MODE_NOPRIVMSGS 	0x1000
#define	MODE_KEY		0x2000
#define	MODE_LIMIT		0x4000
#define MODE_RGSTR		0x8000
#define MODE_INVEX		0x8000000

#define is_halfop is_half_op
/*
 * mode flags which take another parameter (With PARAmeterS)
 */
#define	MODE_WPARAS (MODE_HALFOP|MODE_CHANOP|MODE_VOICE|MODE_CHANOWNER|MODE_CHANPROT|MODE_BAN|MODE_KEY|MODE_LIMIT|MODE_EXCEPT|MODE_INVEX)
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

/* remote fds are set to -256, else its a local fd (a local fd
 * can get -1 or -2 in case it has been closed). -- Syzop
 */
#define	MyConnect(x)			((x)->fd != -256)
#define	MyClient(x)			(MyConnect(x) && IsClient(x))

#define TStime() (timeofday)

/* Lifted somewhat from Undernet code --Rak */

#define IsSendable(x)		(DBufLength(&x->local->sendQ) < 2048)
#define DoList(x)		((x)->user && (x)->user->lopt)

/* used in SetMode() in channel.c and m_umode() in s_msg.c */

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

/* used for async dns values */

#define	ASYNC_NONE	(-1)
#define	ASYNC_CLIENT	0
#define	ASYNC_CONNECT	1
#define	ASYNC_CONF	2
#define	ASYNC_SERVER	3

/* misc variable externs */

extern MODVAR char *version, *infotext[], *dalinfotext[], *unrealcredits[], *unrealinfo[];
extern MODVAR char *generation, *creation;
extern MODVAR char *gnulicense[];
/* misc defines */

#define	FLUSH_BUFFER	-2
#define	COMMA		","

#define PARTFMT		":%s PART %s"
#define PARTFMT2	":%s PART %s :%s"

#define isexcept void

#include "ssl.h"
#define EVENT_HASHES EVENT_DRUGS
#include "events.h"
struct Command {
	aCommand		*prev, *next;
	char 			*cmd;
	int			(*func) ();
	int			flags;
	unsigned int    	count;
	unsigned		parameters : 5;
	unsigned long   	bytes;
	Module 			*owner;
	aCommand		*friend; /* cmd if token, token if cmd */
	Cmdoverride		*overriders;
	Cmdoverride		*overridetail;
#ifdef DEBUGMODE
	unsigned long 		lticks;
	unsigned long 		rticks;
#endif
};

struct _cmdoverride {
	Cmdoverride		*prev, *next;
	Module			*owner;
	aCommand		*command;
	int			(*func)();
};

struct ThrottlingBucket
{
	struct ThrottlingBucket *prev, *next;
	char *ip;
	time_t since;
	char count;
};

typedef struct {
	long mode;
	char flag;
	unsigned  halfop : 1;       /* 1 = yes 0 = no */
	unsigned  parameters : 1;
} aCtab;

/** Parse channel mode */
typedef struct _parsemode ParseMode;
struct _parsemode {
	int what;
	char modechar;
	char *param;
	Cmode *extm;
	char *modebuf; /* curr pos */
	char *parabuf; /* curr pos */
	char buf[512]; /* internal parse buffer */
};

typedef struct PendingServer aPendingServer;
struct PendingServer {
	aPendingServer *prev, *next;
	char sid[IDLEN+1];
};

typedef struct PendingNet aPendingNet;
struct PendingNet {
	aPendingNet *prev, *next; /* Previous and next in list */
	aClient *sptr; /**< Client to which these servers belong */
	aPendingServer *servers; /**< The list of servers connected to the client */
};

void init_throttling_hash();
struct ThrottlingBucket *find_throttling_bucket(aClient *);
void add_throttling_bucket(aClient *);
int throttle_can_connect(aClient *);

#define VERIFY_OPERCOUNT(clnt,tag) { if (IRCstats.operators < 0) verify_opercount(clnt,tag); } while(0)

#define MARK_AS_OFFICIAL_MODULE(modinf)	do { if (modinf && modinf->handle) ModuleSetOptions(modinfo->handle, MOD_OPT_OFFICIAL, 1);  } while(0)

/* old.. please don't use anymore */
#define CHANOPPFX "@"

/* used for is_banned type field: */
#define BANCHK_JOIN		0	/* checking if a ban forbids the person from joining */
#define BANCHK_MSG		1	/* checking if a ban forbids the person from sending messages */
#define BANCHK_NICK		2	/* checking if a ban forbids the person from changing his/her nick */

#define TKLISTLEN 26

#define MATCH_CHECK_IP              0x0001
#define MATCH_CHECK_REAL_HOST       0x0002
#define MATCH_CHECK_CLOAKED_HOST    0x0004
#define MATCH_CHECK_VISIBLE_HOST    0x0008

#define MATCH_CHECK_ALL             (MATCH_CHECK_IP|MATCH_CHECK_REAL_HOST|MATCH_CHECK_CLOAKED_HOST|MATCH_CHECK_VISIBLE_HOST)
#define MATCH_CHECK_REAL            (MATCH_CHECK_IP|MATCH_CHECK_REAL_HOST)

#define MATCH_MASK_IS_UHOST         0x1000
#define MATCH_MASK_IS_HOST          0x2000

#define MATCH_USE_IDENT             0x0100

#endif /* __struct_include__ */

#include "dynconf.h"
