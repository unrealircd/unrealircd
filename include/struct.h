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
/* need to include ssl stuff here coz otherwise you get
 * conflicting types with isalnum/isalpha/etc @ redhat. -- Syzop
 */
#if defined(USE_SSL)
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
#endif
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
#include "md5.h"

#ifdef HAVE_SYSLOG
# include <syslog.h>
# ifdef SYSSYSLOGH
#  include <sys/syslog.h>
# endif
#endif
#ifdef ZIP_LINKS
#include "zip.h"
#endif
#include "auth.h" 
#ifndef _WIN32
#include "tre/regex.h"
#else
#include "win32/regex.h"
#endif

#include "channel.h"

#if defined(_WIN32) && !defined(NOSPOOF)
 #error "Compiling win32 without nospoof is VERY insecure and NOT supported"
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
typedef struct _configitem_admin ConfigItem_admin;
typedef struct _configitem_class ConfigItem_class;
typedef struct _configitem_oper ConfigItem_oper;
typedef struct _configitem_oper_from ConfigItem_oper_from;
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
typedef struct _configitem_cgiirc ConfigItem_cgiirc;
typedef struct _configitem_ban ConfigItem_ban;
typedef struct _configitem_badword ConfigItem_badword;
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

#define CFG_TIME 0x0001
#define CFG_SIZE 0x0002
#define CFG_YESNO 0x0004

typedef struct Watch aWatch;
typedef struct Client aClient;
typedef struct Channel aChannel;
typedef struct User anUser;
typedef struct Server aServer;
typedef struct SLink Link;
typedef struct SBan Ban;
typedef struct SMode Mode;
typedef struct SChanFloodProt ChanFloodProt;
typedef struct SRemoveFld RemoveFld;
typedef struct ListOptions LOpts;
typedef struct FloodOpt aFloodOpt;
typedef struct MotdItem aMotd;
typedef struct trecord aTrecord;
typedef struct Command aCommand;
typedef struct _cmdoverride Cmdoverride;
typedef struct SMember Member;
typedef struct SMembership Membership;
typedef struct SMembershipL MembershipL;
typedef struct JFlood aJFlood;

#ifdef ZIP_LINKS
typedef struct  Zdata   aZdata;
#endif

#ifdef NEED_U_INT32_T
typedef unsigned int u_int32_t;	/* XXX Hope this works! */
#endif

#ifndef VMSP
#include "class.h"
#include "dbuf.h"		/* THIS REALLY SHOULDN'T BE HERE!!! --msa */
#endif

#define	HOSTLEN		63	/* Length of hostname.  Updated to         */
				/* comply with RFC1123                     */

#define	NICKLEN		30
#define	USERLEN		10
#define	REALLEN	 	50
#define	TOPICLEN	307
#define	CHANNELLEN	32
#define	PASSWDLEN 	32	/* orig. 20, changed to 32 for nickpasswords */
#define	KEYLEN		23
#define LINKLEN		32
#define	BUFSIZE		512	/* WARNING: *DONT* CHANGE THIS!!!! */
#define	MAXRECIPIENTS 	20
#define	MAXKILLS	20
#define	MAXSILELENGTH	NICKLEN+USERLEN+HOSTLEN+10
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

#ifdef USE_SSL
#define IsSSLAcceptHandshake(x)	((x)->status == STAT_SSL_ACCEPT_HANDSHAKE)
#define IsSSLConnectHandshake(x)	((x)->status == STAT_SSL_CONNECT_HANDSHAKE)
#define IsSSLHandshake(x) (IsSSLAcceptHandshake(x) || IsSSLConnectHandshake(x))
#define SetSSLAcceptHandshake(x)	((x)->status = STAT_SSL_ACCEPT_HANDSHAKE)
#define SetSSLConnectHandshake(x)	((x)->status = STAT_SSL_CONNECT_HANDSHAKE)
#endif

#define	SetConnecting(x)	((x)->status = STAT_CONNECTING)
#define	SetHandshake(x)		((x)->status = STAT_HANDSHAKE)
#define	SetMe(x)		((x)->status = STAT_ME)
#define	SetUnknown(x)		((x)->status = STAT_UNKNOWN)
#define	SetServer(x)		((x)->status = STAT_SERVER)
#define	SetClient(x)		((x)->status = STAT_CLIENT)
#define	SetLog(x)		((x)->status = STAT_LOG)

#define IsSynched(x)	(x->serv->flags.synced)

/* opt.. */
#define OPT_SJOIN	0x0001
#define OPT_NOT_SJOIN	0x0002
#define OPT_NICKv2	0x0004
#define OPT_NOT_NICKv2	0x0008
#define OPT_SJOIN2	0x0010
#define OPT_NOT_SJOIN2	0x0020
#define OPT_UMODE2	0x0040
#define OPT_NOT_UMODE2	0x0080
#define OPT_SJ3		0x0100
#define OPT_NOT_SJ3	0x0200
#define OPT_SJB64	0x0400
#define OPT_NOT_SJB64	0x0800
#define OPT_VHP		0x1000
#define OPT_NOT_VHP	0x2000
#define OPT_TKLEXT	0x4000
#define OPT_NOT_TKLEXT	0x8000
#define OPT_NICKIP	0x10000
#define OPT_NOT_NICKIP  0x20000

/* client->flags (32 bits): 28 used, 4 free */
#define	FLAGS_PINGSENT   0x0001	/* Unreplied ping sent */
#define	FLAGS_DEADSOCKET 0x0002	/* Local socket is dead--Exiting soon */
#define	FLAGS_KILLED     0x0004	/* Prevents "QUIT" from being sent for this */
#define	FLAGS_BLOCKED    0x0008	/* socket is in a blocked condition */
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
#define FLAGS_CGIIRC     0x8000 /* CGI IRC host: flag set = ip/host data has been filled in already */
#define FLAGS_ULINE      0x10000	/* User/server is considered U-lined */
#define FLAGS_SQUIT      0x20000	/* Server has been /squit by an oper */
#define FLAGS_PROTOCTL   0x40000	/* Received a PROTOCTL message */
#define FLAGS_PING       0x80000
#define FLAGS_ASKEDPING  0x100000
#define FLAGS_NETINFO    0x200000
#define FLAGS_HYBNOTICE  0x400000
#define FLAGS_QUARANTINE 0x800000
#ifdef ZIP_LINKS
#define FLAGS_ZIP        0x1000000
#endif
#define FLAGS_DCCNOTICE  0x2000000 /* Has the user seen a notice on how to use DCCALLOW already? */
#define FLAGS_SHUNNED    0x4000000
#define FLAGS_VIRUS      0x8000000 /* tagged by spamfilter */
#ifdef USE_SSL
#define FLAGS_SSL        0x10000000
#endif
#define FLAGS_UNOCCUP4   0x20000000 /* [FREE] */
#define FLAGS_DCCBLOCK   0x40000000 /* Block all DCC send requests */
#define FLAGS_MAP        0x80000000	/* Show this entry in /map */
/* Dec 26th, 1997 - added flags2 when I ran out of room in flags -DuffJ */

/* Dec 26th, 1997 - having a go at
 * splitting flags into flags and umodes
 * -DuffJ
 */

#define SNO_DEFOPER "+kscfvGqo"
#define SNO_DEFUSER "+ks"

#define SEND_UMODES (SendUmodes)
#define ALL_UMODES (AllUmodes)
/* SEND_UMODES and ALL_UMODES are now handled by umode_get/umode_lget/umode_gget -- Syzop. */

#define	FLAGS_ID	(FLAGS_DOID|FLAGS_GOTID)

#define PROTO_NOQUIT	0x0001	/* Negotiated NOQUIT protocol */
#define PROTO_TOKEN		0x0002	/* Negotiated TOKEN protocol */
#define PROTO_SJOIN		0x0004	/* Negotiated SJOIN protocol */
#define PROTO_NICKv2	0x0008	/* Negotiated NICKv2 protocol */
#define PROTO_SJOIN2	0x0010	/* Negotiated SJOIN2 protocol */
#define PROTO_UMODE2	0x0020	/* Negotiated UMODE2 protocol */
#define PROTO_NS		0x0040	/* Negotiated NS protocol */
#define PROTO_ZIP		0x0080	/* Negotiated ZIP protocol */
#define PROTO_VL		0x0100	/* Negotiated VL protocol */
#define PROTO_SJ3		0x0200	/* Negotiated SJ3 protocol */
#define PROTO_VHP		0x0400	/* Send hostnames in NICKv2 even if not sethosted */
#define PROTO_SJB64		0x0800
#define PROTO_TKLEXT	0x1000	/* TKL extension: 10 parameters instead of 8 (3.2RC2) */
#define PROTO_NICKIP	0x2000  /* Send IP addresses in the NICK command */
#define PROTO_NAMESX	0x4000  /* Send all rights in NAMES output */

/*
 * flags macros.
 */
#define IsVictim(x)             ((x)->umodes & UMODE_VICTIM)
#define IsDeaf(x)               ((x)->umodes & UMODE_DEAF)
#define IsKillsF(x)		((x)->user->snomask & SNO_KILLS)
#define IsClientF(x)		((x)->user->snomask & SNO_CLIENT)
#define IsFloodF(x)		((x)->user->snomask & SNO_FLOOD)
#define IsEyes(x)		((x)->user->snomask & SNO_EYES)
#define IsWhois(x)	        ((x)->umodes & UMODE_WHOIS)
#define IsKix(x)		((x)->umodes & UMODE_KIX)
#define IsHelpOp(x)		((x)->umodes & UMODE_HELPOP)
#define IsAdmin(x)		((x)->umodes & UMODE_ADMIN)

#ifdef STRIPBADWORDS
#define IsFilteringWords(x)	((x)->umodes & UMODE_STRIPBADWORDS)
#endif
#define IsNetAdmin(x)		((x)->umodes & UMODE_NETADMIN)
#define IsCoAdmin(x)		((x)->umodes & UMODE_COADMIN)
#define IsSAdmin(x)		((x)->umodes & UMODE_SADMIN)
#define SendFailops(x)		((x)->umodes & UMODE_FAILOP)
#define	IsOper(x)		((x)->umodes & UMODE_OPER)
#define	IsLocOp(x)		((x)->umodes & UMODE_LOCOP)
#define	IsInvisible(x)		((x)->umodes & UMODE_INVISIBLE)
#define IsServices(x)		((x)->umodes & UMODE_SERVICES)
#define	IsAnOper(x)		((x)->umodes & (UMODE_OPER|UMODE_LOCOP))
#define IsARegNick(x)		((x)->umodes & (UMODE_REGNICK))
#define IsRegNick(x)		((x)->umodes & UMODE_REGNICK)
#define IsRegNickMsg(x)		((x)->umodes & UMODE_RGSTRONLY)
#define IsNoCTCP(x)		((x)->umodes & UMODE_NOCTCP)
#define IsWebTV(x)		((x)->umodes & UMODE_WEBTV)
#define	IsPerson(x)		((x)->user && IsClient(x))
#define	IsPrivileged(x)		(IsAnOper(x) || IsServer(x))
#define	SendWallops(x)		(!IsMe(x) && IsPerson(x) && ((x)->umodes & UMODE_WALLOP))
#define	SendServNotice(x)	(((x)->user) && ((x)->user->snomask & SNO_SNOTICE))
#define	IsListening(x)		((x)->flags & FLAGS_LISTEN)
// #define	DoAccess(x)		((x)->flags & FLAGS_CHKACCESS)
#define	IsLocal(x)		((x)->flags & FLAGS_LOCAL)
#define	IsDead(x)		((x)->flags & FLAGS_DEADSOCKET)
#define GotProtoctl(x)		((x)->flags & FLAGS_PROTOCTL)
#define IsBlocked(x)		((x)->flags & FLAGS_BLOCKED)
#define IsOutgoing(x)		((x)->flags & FLAGS_OUTGOING)
#define GotNetInfo(x) 		((x)->flags & FLAGS_NETINFO)
#define SetNetInfo(x)		((x)->flags |= FLAGS_NETINFO)
#define IsCGIIRC(x)			((x)->flags & FLAGS_CGIIRC)

#define IsShunned(x)		((x)->flags & FLAGS_SHUNNED)
#define SetShunned(x)		((x)->flags |= FLAGS_SHUNNED)
#define ClearShunned(x)		((x)->flags &= ~FLAGS_SHUNNED)
#define IsVirus(x)			((x)->flags & FLAGS_VIRUS)
#define SetVirus(x)			((x)->flags |= FLAGS_VIRUS)
#define ClearVirus(x)		((x)->flags &= ~FLAGS_VIRUS)
#ifdef USE_SSL
#define IsSecure(x)		((x)->flags & FLAGS_SSL)
#else
#define IsSecure(x)		(0)
#endif

#ifdef ZIP_LINKS
#define IsZipped(x) 	((x)->flags & FLAGS_ZIP)
#define IsZipStart(x)	(((x)->flags & FLAGS_ZIP) && ((x)->zip->first == 1))
#else
#define IsZipped(x)		(0)
#define IsZipStart(x)	(0)
#endif

#define IsHybNotice(x)		((x)->flags & FLAGS_HYBNOTICE)
#define SetHybNotice(x)         ((x)->flags |= FLAGS_HYBNOTICE)
#define ClearHybNotice(x)	((x)->flags &= ~FLAGS_HYBNOTICE)
#define IsHidden(x)             ((x)->umodes & UMODE_HIDE)
#define IsHideOper(x)		((x)->umodes & UMODE_HIDEOPER)
#ifdef USE_SSL
#define IsSSL(x)		IsSecure(x)
#endif
#ifdef NOSPOOF
#define	IsNotSpoof(x)		((x)->nospoof == 0)
#else
#define IsNotSpoof(x)           (1)
#endif

#define GetHost(x)			(IsHidden(x) ? (x)->user->virthost : (x)->user->realhost)
#define GetIP(x)			((x->user && x->user->ip_str) ? x->user->ip_str : (MyConnect(x) ? Inet_ia2p(&x->ip) : NULL))

#define SetKillsF(x)		((x)->user->snomask |= SNO_KILLS)
#define SetClientF(x)		((x)->user->snomask |= SNO_CLIENT)
#define SetFloodF(x)		((x)->user->snomask |= SNO_FLOOD)
#define SetHelpOp(x)		((x)->umodes |= UMODE_HELPOP)
#define	SetOper(x)		((x)->umodes |= UMODE_OPER)
#define	SetLocOp(x)    		((x)->umodes |= UMODE_LOCOP)
#define SetAdmin(x)		((x)->umodes |= UMODE_ADMIN)
#define SetSAdmin(x)		((x)->umodes |= UMODE_SADMIN)
#define SetNetAdmin(x)		((x)->umodes |= UMODE_NETADMIN)
#define SetCoAdmin(x)		((x)->umodes |= UMODE_COADMIN)
#define	SetInvisible(x)		((x)->umodes |= UMODE_INVISIBLE)
#define SetEyes(x)		((x)->user->snomask |= SNO_EYES)
#define	SetWallops(x)  		((x)->umodes |= UMODE_WALLOP)
#define	SetDNS(x)		((x)->flags |= FLAGS_DOINGDNS)
#define	DoingDNS(x)		((x)->flags & FLAGS_DOINGDNS)
#define	SetAccess(x)		((x)->flags |= FLAGS_CHKACCESS); Debug((DEBUG_DEBUG, "SetAccess(%s)", (x)->name))
#define SetBlocked(x)		((x)->flags |= FLAGS_BLOCKED)
#define SetOutgoing(x)		do { x->flags |= FLAGS_OUTGOING; } while(0)
#define SetCGIIRC(x)		do { x->flags |= FLAGS_CGIIRC; } while(0)
#define	DoingAuth(x)		((x)->flags & FLAGS_AUTH)
#define	NoNewLine(x)		((x)->flags & FLAGS_NONL)
#define IsDCCNotice(x)		((x)->flags & FLAGS_DCCNOTICE)
#define SetDCCNotice(x)		do { x->flags |= FLAGS_DCCNOTICE; } while(0)
#define SetRegNick(x)		((x)->umodes & UMODE_REGNICK)
#define SetHidden(x)            ((x)->umodes |= UMODE_HIDE)
#define SetHideOper(x)      ((x)->umodes |= UMODE_HIDEOPER)
#define IsSecureConnect(x)	((x)->umodes & UMODE_SECURE)
#define ClearAdmin(x)		((x)->umodes &= ~UMODE_ADMIN)
#define ClearNetAdmin(x)	((x)->umodes &= ~UMODE_NETADMIN)
#define ClearCoAdmin(x)		((x)->umodes &= ~UMODE_COADMIN)
#define ClearSAdmin(x)		((x)->umodes &= ~UMODE_SADMIN)
#define ClearKillsF(x)		((x)->user->snomask &= ~SNO_KILLS)
#define ClearClientF(x)		((x)->user->snomask &= ~SNO_CLIENT)
#define ClearFloodF(x)		((x)->user->snomask &= ~SNO_FLOOD)
#define ClearEyes(x)		((x)->user->snomask &= ~SNO_EYES)
#define ClearHelpOp(x)		((x)->umodes &= ~UMODE_HELPOP)
#define ClearFailops(x)		((x)->umodes &= ~UMODE_FAILOP)
#define	ClearOper(x)		((x)->umodes &= ~UMODE_OPER)
#define	ClearInvisible(x)	((x)->umodes &= ~UMODE_INVISIBLE)
#define ClearServices(x)	((x)->umodes &= ~UMODE_SERVICES)
#define	ClearWallops(x)		((x)->umodes &= ~UMODE_WALLOP)
#define	ClearDNS(x)		((x)->flags &= ~FLAGS_DOINGDNS)
#define	ClearAuth(x)		((x)->flags &= ~FLAGS_AUTH)
#define	ClearAccess(x)		((x)->flags &= ~FLAGS_CHKACCESS)
#define ClearBlocked(x)		((x)->flags &= ~FLAGS_BLOCKED)
#define ClearHidden(x)          ((x)->umodes &= ~UMODE_HIDE)
#define ClearHideOper(x)    ((x)->umodes &= ~UMODE_HIDEOPER)

#ifdef ZIP_LINKS
#define SetZipped(x)        ((x)->flags |= FLAGS_ZIP)
#define ClearZipped(x)      ((x)->flags &= ~FLAGS_ZIP)
#endif

/*
 * ProtoCtl options
 */
#ifndef DEBUGMODE
#define CHECKPROTO(x,y)	((x)->proto & y)
#else
#define CHECKPROTO(x,y) (checkprotoflags(x, y, __FILE__, __LINE__))
#endif

#define DontSendQuit(x)		(CHECKPROTO(x, PROTO_NOQUIT))
#define IsToken(x)		(CHECKPROTO(x, PROTO_TOKEN))
#define SupportSJOIN(x)		(CHECKPROTO(x, PROTO_SJOIN))
#define SupportNICKv2(x)	(CHECKPROTO(x, PROTO_NICKv2))
#define SupportNICKIP(x)	(CHECKPROTO(x, PROTO_NICKIP))
#define SupportSJOIN2(x)	(CHECKPROTO(x, PROTO_SJOIN2))
#define SupportUMODE2(x)	(CHECKPROTO(x, PROTO_UMODE2))
#define SupportNS(x)		(CHECKPROTO(x, PROTO_NS))
#define SupportVL(x)		(CHECKPROTO(x, PROTO_VL))
#define SupportSJ3(x)		(CHECKPROTO(x, PROTO_SJ3))
#define SupportVHP(x)		(CHECKPROTO(x, PROTO_VHP))
#define SupportTKLEXT(x)	(CHECKPROTO(x, PROTO_TKLEXT))
#define SupportNAMESX(x)	(CHECKPROTO(x, PROTO_NAMESX))

#define SetSJOIN(x)		((x)->proto |= PROTO_SJOIN)
#define SetNoQuit(x)		((x)->proto |= PROTO_NOQUIT)
#define SetToken(x)		((x)->proto |= PROTO_TOKEN)
#define SetNICKv2(x)		((x)->proto |= PROTO_NICKv2)
#define SetSJOIN2(x)		((x)->proto |= PROTO_SJOIN2)
#define SetUMODE2(x)		((x)->proto |= PROTO_UMODE2)
#define SetNS(x)		((x)->proto |= PROTO_NS)
#define SetVL(x)		((x)->proto |= PROTO_VL)
#define SetSJ3(x)		((x)->proto |= PROTO_SJ3)
#define SetVHP(x)		((x)->proto |= PROTO_VHP)
#define SetTKLEXT(x)	((x)->proto |= PROTO_TKLEXT)
#define SetNAMESX(x)	((x)->proto |= PROTO_NAMESX)

#define ClearSJOIN(x)		((x)->proto &= ~PROTO_SJOIN)
#define ClearNoQuit(x)		((x)->proto &= ~PROTO_NOQUIT)
#define ClearToken(x)		((x)->proto &= ~PROTO_TOKEN)
#define ClearNICKv2(x)		((x)->proto &= ~PROTO_NICKv2)
#define ClearSJOIN2(x)		((x)->proto &= ~PROTO_SJOIN2)
#define ClearUMODE2(x)		((x)->proto &= ~PROTO_UMODE2)
#define ClearVL(x)		((x)->proto &= ~PROTO_VL)
#define ClearVHP(x)		((x)->proto &= ~PROTO_VHP)
#define ClearSJ3(x)		((x)->proto &= ~PROTO_SJ3)

/*
 * defined operator access levels
 */
#define OFLAG_REHASH	0x00000001	/* Oper can /rehash server */
#define OFLAG_DIE		0x00000002	/* Oper can /die the server */
#define OFLAG_RESTART	0x00000004	/* Oper can /restart the server */
#define OFLAG_DCCDENY	0x00000008	/* Oper can use /dccdeny and /undccdeny */
#define OFLAG_HELPOP	0x00000010	/* Oper can send /HelpOps */
#define OFLAG_GLOBOP	0x00000020	/* Oper can send /GlobOps */
#define OFLAG_WALLOP	0x00000040	/* Oper can send /WallOps */
#define OFLAG_LOCOP		0x00000080	/* Oper can send /LocOps */
#define OFLAG_LROUTE	0x00000100	/* Oper can do local routing */
#define OFLAG_GROUTE	0x00000200	/* Oper can do global routing */
#define OFLAG_LKILL		0x00000400	/* Oper can do local kills */
#define OFLAG_GKILL		0x00000800	/* Oper can do global kills */
#define OFLAG_KLINE		0x00001000	/* Oper can /kline users */
#define OFLAG_UNKLINE	0x00002000	/* Oper can /unkline users */
#define OFLAG_LNOTICE	0x00004000	/* Oper can send local serv notices */
#define OFLAG_GNOTICE	0x00008000	/* Oper can send global notices */
#define OFLAG_ADMIN		0x00010000	/* Admin */
#define OFLAG_ADDLINE	0x00020000	/* Oper can use /addline */
#define OFLAG_ZLINE		0x00080000	/* Oper can use /zline and /unzline */
#define OFLAG_NETADMIN	0x00200000	/* netadmin gets +N */
#define OFLAG_COADMIN	0x00800000	/* co admin gets +C */
#define OFLAG_SADMIN	0x01000000	/* services admin gets +a */
#define OFLAG_WHOIS     0x02000000	/* gets auto +W on oper up */
#define OFLAG_HIDE      0x04000000	/* gets auto +x on oper up */
#define OFLAG_TKL       0x10000000	/* can use G:lines and shuns */
#define OFLAG_GZL       0x20000000	/* can use global Z:lines */
#define OFLAG_OVERRIDE	0x40000000	/* can use oper-override */
#define OFLAG_UMODEQ	0x80000000	/* can set +q */
#define OFLAG_LOCAL	(OFLAG_REHASH|OFLAG_HELPOP|OFLAG_GLOBOP|OFLAG_WALLOP|OFLAG_LOCOP|OFLAG_LROUTE|OFLAG_LKILL|OFLAG_KLINE|OFLAG_UNKLINE|OFLAG_LNOTICE)
#define OFLAG_GLOBAL	(OFLAG_LOCAL|OFLAG_GROUTE|OFLAG_GKILL|OFLAG_GNOTICE)
#define OFLAG_ISGLOBAL	(OFLAG_GROUTE|OFLAG_GKILL|OFLAG_GNOTICE|OFLAG_TKL|OFLAG_GZL|OFLAG_OVERRIDE)
#define OFLAG_NADMIN	(OFLAG_NETADMIN | OFLAG_SADMIN | OFLAG_ADMIN | OFLAG_GLOBAL | OFLAG_UMODEQ | OFLAG_DCCDENY)
#define OFLAG_ADMIN_	(OFLAG_ADMIN | OFLAG_GLOBAL | OFLAG_DCCDENY)
#define OFLAG_COADMIN_	(OFLAG_COADMIN | OFLAG_GLOBAL | OFLAG_DCCDENY)
#define OFLAG_SADMIN_	(OFLAG_SADMIN | OFLAG_GLOBAL | OFLAG_UMODEQ | OFLAG_DCCDENY)

#define OPCanOverride(x) ((x)->oflag & OFLAG_OVERRIDE)
#define OPCanUmodeq(x)	((x)->oflag & OFLAG_UMODEQ)
#define OPCanDCCDeny(x)	((x)->oflag & OFLAG_DCCDENY)
#define OPCanTKL(x)	((x)->oflag & OFLAG_TKL)
#define OPCanGZL(x)	((x)->oflag & OFLAG_GZL)
#define OPCanAddline(x)   ((x)->oflag & OFLAG_ADDLINE)
#define OPCanZline(x)   ((x)->oflag & OFLAG_ZLINE)
#define OPCanRehash(x)	((x)->oflag & OFLAG_REHASH)
#define OPCanDie(x)	((x)->oflag & OFLAG_DIE)
#define OPCanRestart(x)	((x)->oflag & OFLAG_RESTART)
#define OPCanHelpOp(x)	((x)->oflag & OFLAG_HELPOP)
#define OPCanGlobOps(x)	((x)->oflag & OFLAG_GLOBOP)
#define OPCanWallOps(x)	((x)->oflag & OFLAG_WALLOP)
#define OPCanLocOps(x)	((x)->oflag & OFLAG_LOCOP)
#define OPCanLRoute(x)	((x)->oflag & OFLAG_LROUTE)
#define OPCanGRoute(x)	((x)->oflag & OFLAG_GROUTE)
#define OPCanLKill(x)	((x)->oflag & OFLAG_LKILL)
#define OPCanGKill(x)	((x)->oflag & OFLAG_GKILL)
#define OPCanKline(x)	((x)->oflag & OFLAG_KLINE)
#define OPCanUnKline(x)	((x)->oflag & OFLAG_UNKLINE)
#define OPCanLNotice(x)	((x)->oflag & OFLAG_LNOTICE)
#define OPCanGNotice(x)	((x)->oflag & OFLAG_GNOTICE)
#define OPIsAdmin(x)	((x)->oflag & OFLAG_ADMIN)
#define OPIsSAdmin(x)	((x)->oflag & OFLAG_SADMIN)
#define OPIsNetAdmin(x) ((x)->oflag & OFLAG_NETADMIN)
#define OPIsCoAdmin(x)	((x)->oflag & OFLAG_COADMIN)
#define OPIsWhois(x)    ((x)->oflag & OFLAG_WHOIS)

#define OPSetRehash(x)	((x)->oflag |= OFLAG_REHASH)
#define OPSetDie(x)	((x)->oflag |= OFLAG_DIE)
#define OPSetRestart(x)	((x)->oflag |= OFLAG_RESTART)
#define OPSetHelpOp(x)	((x)->oflag |= OFLAG_HELPOP)
#define OPSetGlobOps(x)	((x)->oflag |= OFLAG_GLOBOP)
#define OPSetWallOps(x)	((x)->oflag |= OFLAG_WALLOP)
#define OPSetLocOps(x)	((x)->oflag |= OFLAG_LOCOP)
#define OPSetLRoute(x)	((x)->oflag |= OFLAG_LROUTE)
#define OPSetGRoute(x)	((x)->oflag |= OFLAG_GROUTE)
#define OPSetLKill(x)	((x)->oflag |= OFLAG_LKILL)
#define OPSetGKill(x)	((x)->oflag |= OFLAG_GKILL)
#define OPSetKline(x)	((x)->oflag |= OFLAG_KLINE)
#define OPSetUnKline(x)	((x)->oflag |= OFLAG_UNKLINE)
#define OPSetLNotice(x)	((x)->oflag |= OFLAG_LNOTICE)
#define OPSetGNotice(x)	((x)->oflag |= OFLAG_GNOTICE)
#define OPSSetAdmin(x)	((x)->oflag |= OFLAG_ADMIN)
#define OPSSetSAdmin(x)	((x)->oflag |= OFLAG_SADMIN)
#define OPSSetNetAdmin(x) ((x)->oflag |= OFLAG_NETADMIN)
#define OPSSetCoAdmin(x) ((x)->oflag |= OFLAG_COADMIN)
#define OPSetZLine(x)	((x)->oflag |= OFLAG_ZLINE)
#define OPSetWhois(x)   ((x)->oflag |= OFLAG_WHOIS)
#define OPClearRehash(x)	((x)->oflag &= ~OFLAG_REHASH)
#define OPClearDie(x)		((x)->oflag &= ~OFLAG_DIE)
#define OPClearRestart(x)	((x)->oflag &= ~OFLAG_RESTART)
#define OPClearHelpOp(x)	((x)->oflag &= ~OFLAG_HELPOP)
#define OPClearGlobOps(x)	((x)->oflag &= ~OFLAG_GLOBOP)
#define OPClearWallOps(x)	((x)->oflag &= ~OFLAG_WALLOP)
#define OPClearLocOps(x)	((x)->oflag &= ~OFLAG_LOCOP)
#define OPClearLRoute(x)	((x)->oflag &= ~OFLAG_LROUTE)
#define OPClearGRoute(x)	((x)->oflag &= ~OFLAG_GROUTE)
#define OPClearLKill(x)		((x)->oflag &= ~OFLAG_LKILL)
#define OPClearGKill(x)		((x)->oflag &= ~OFLAG_GKILL)
#define OPClearKline(x)		((x)->oflag &= ~OFLAG_KLINE)
#define OPClearUnKline(x)	((x)->oflag &= ~OFLAG_UNKLINE)
#define OPClearLNotice(x)	((x)->oflag &= ~OFLAG_LNOTICE)
#define OPClearGNotice(x)	((x)->oflag &= ~OFLAG_GNOTICE)
#define OPClearAdmin(x)		((x)->oflag &= ~OFLAG_ADMIN)
#define OPClearSAdmin(x)	((x)->oflag &= ~OFLAG_SADMIN)
#define OPClearNetAdmin(x)	((x)->oflag &= ~OFLAG_NETADMIN)
#define OPClearCoAdmin(x)	((x)->oflag &= ~OFLAG_COADMIN)
#define OPClearZLine(x)		((x)->oflag &= ~OFLAG_ZLINE)
#define OPClearWhois(x)         ((x)->oflag &= ~OFLAG_WHOIS)
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

/* blah */
#define IsSkoAdmin(sptr) (IsAdmin(sptr) || IsNetAdmin(sptr) || IsSAdmin(sptr))

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

struct irc_netmask
{
	short int type;
	struct IN_ADDR mask;
	short int bits;
};

struct FloodOpt {
	unsigned short nmsg;
	TS   firstmsg;
};

struct MotdItem {
	char *line;
	struct MotdItem *next;
};

struct aloopStruct {
	unsigned do_garbage_collect : 1;
	unsigned ircd_booted : 1;
	unsigned do_bancheck : 1; /* perform *line bancheck? */
	unsigned do_bancheck_spamf_user : 1; /* perform 'user' spamfilter bancheck */
	unsigned do_bancheck_spamf_away : 1; /* perform 'away' spamfilter bancheck */
	unsigned ircd_rehashing : 1;
	unsigned tainted : 1;
	aClient *rehash_save_cptr, *rehash_save_sptr;
	int rehash_save_sig;
};

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

/*
 * Client structures
 */
struct User {
	Membership *channel;		/* chain of channel pointer blocks */
	Link *invited;		/* chain of invite pointer blocks */
	Link *silence;		/* chain of silence pointer blocks */
	Link *dccallow;		/* chain of dccallowed entries */
	char *away;		/* pointer to away message */
	u_int32_t servicestamp;	/* Services' time stamp variable */
	signed char refcnt;	/* Number of times this block is referenced */
	unsigned short joined;		/* number of channels joined */
	char username[USERLEN + 1];
	char realhost[HOSTLEN + 1];
	char *virthost;
	char *server;
	char *swhois;		/* special whois thing */
	LOpts *lopt;            /* Saved /list options */
	aWhowas *whowas;
	int snomask;
#ifdef	LIST_DEBUG
	aClient *bcptr;
#endif
	char *ip_str;		/* The IP in string form */
	char *operlogin;	/* Only used if person is/was opered, used for oper::maxlogins */
	struct {
		time_t nick_t;
		unsigned char nick_c;
#ifdef NO_FLOOD_AWAY
		time_t away_t;			/* last time the user set away */
		unsigned char away_c;	/* number of times away has been set */
#endif
	} flood;
#ifdef JOINTHROTTLE
	aJFlood *jflood;
#endif
};

struct Server {
	struct Server 	*nexts;
	anUser 		*user;		/* who activated this connection */
	char 		*up;		/* uplink for this server */
	char 		by[NICKLEN + 1];
	ConfigItem_link *conf;
	TS   		timestamp;		/* Remotely determined connect try time */
	unsigned short  numeric;	/* NS numeric, 0 if none */
	long		 users;
#ifdef	LIST_DEBUG
	aClient *bcptr;
#endif
	struct {
		unsigned synced:1;		/* Server linked? (3.2beta18+) */
	} flags;
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
	regex_t expr;
	char *tkl_reason; /* spamfilter reason field [escaped by unreal_encodespace()!] */
	TS tkl_duration;
};

struct t_kline {
	aTKline *prev, *next;
	int type;
	unsigned short subtype; /* subtype (currently spamfilter only), see SPAMF_* */
	union {
		Spamfilter *spamf;
		struct irc_netmask *netmask;
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

#ifdef EXTCMODE
extern MODVAR Cmode *Channelmode_Table;
extern MODVAR unsigned short Channelmode_highest;
#endif

extern Umode *UmodeAdd(Module *module, char ch, int options, int (*allowed)(aClient *sptr, int what), long *mode);
extern void UmodeDel(Umode *umode);

extern Snomask *SnomaskAdd(Module *module, char ch, int (*allowed)(aClient *sptr, int what), long *mode);
extern void SnomaskDel(Snomask *sno);

#ifdef EXTCMODE
extern Cmode *CmodeAdd(Module *reserved, CmodeInfo req, Cmode_t *mode);
extern void CmodeDel(Cmode *cmode);
#endif

typedef struct {
	EXTCM_PAR_HEADER
	unsigned short num;
	unsigned short t;
} aModejEntry;

#define LISTENER_NORMAL		0x000001
#define LISTENER_CLIENTSONLY	0x000002
#define LISTENER_SERVERSONLY	0x000004
#define LISTENER_REMOTEADMIN	0x000008
#define LISTENER_JAVACLIENT	0x000010
#define LISTENER_MASK		0x000020
#define LISTENER_SSL		0x000040
#define LISTENER_BOUND		0x000080

#define CONNECT_SSL		0x000001
#define CONNECT_ZIP		0x000002 
#define CONNECT_AUTO		0x000004
#define CONNECT_QUARANTINE	0x000008
#define CONNECT_NODNSCACHE	0x000010
#define CONNECT_NOHOSTCHECK	0x000020

#define SSLFLAG_FAILIFNOCERT 	0x1
#define SSLFLAG_VERIFYCERT 	0x2
#define SSLFLAG_DONOTACCEPTSELFSIGNED 0x4
struct Client {
	struct Client *next, *prev, *hnext;
	anUser *user;		/* ...defined, if this is a User */
	aServer *serv;		/* ...defined, if this is a server */
	TS   lastnick;		/* TimeStamp on nick */
	long flags;		/* client flags */
	long umodes;		/* client usermodes */
	aClient *from;		/* == self, if Local Client, *NEVER* NULL! */
	int  fd;		/* >= 0, for local clients */
	unsigned char hopcount;		/* number of servers to this 0 = local */
	char name[HOSTLEN + 1];	/* Unique name of the client, nick or host */
	char username[USERLEN + 1];	/* username here now for auth stuff */
	char info[REALLEN + 1];	/* Free form additional client information */
	aClient *srvptr;	/* Server introducing this.  May be &me */
	short status;		/* client type */
	/*
	   ** The following fields are allocated only for local clients
	   ** (directly connected to *this* server with a socket.
	   ** The first of them *MUST* be the "count"--it is the field
	   ** to which the allocation is tied to! *Never* refer to
	   ** these fields, if (from != self).
	 */
	int  count;		/* Amount of data in buffer */
#if 1
	int  oflag;		/* oper access flags (removed from anUser for mem considerations) */
	TS   since;		/* time they will next be allowed to send something */
	TS   firsttime;		/* Time it was created */
	TS   lasttime;		/* last time any message was received */
	TS   last;		/* last time a RESETIDLE message was received */
	TS   nexttarget;	/* next time that a new target will be allowed (msg/notice/invite) */
 	TS   nextnick;		/* Time the next nick change will be allowed */
	u_char targets[MAXTARGETS];	/* hash values of targets */
#endif
	char buffer[BUFSIZE];	/* Incoming message buffer */
	short lastsq;		/* # of 2k blocks when sendqueued called last */
	dbuf sendQ;		/* Outgoing message queue--if socket full */
	dbuf recvQ;		/* Hold for data incoming yet to be parsed */
#ifdef NOSPOOF
	u_int32_t nospoof;	/* Anti-spoofing random number */
#endif
	int proto;		/* ProtoCtl options */
	long sendM;		/* Statistics: protocol messages send */
	long sendK;		/* Statistics: total k-bytes send */
	long receiveM;		/* Statistics: protocol messages received */
#ifdef ZIP_LINKS
	struct Zdata *zip;	/* zip data */
#elif defined(_WIN32)
	void *zip_NOTUSED; /* (win32 binary compatability) */
#endif
#ifdef USE_SSL
	SSL		*ssl;
#elif defined(_WIN32)
	void	*ssl_NOTUSED; /* (win32 binary compatability) */
#endif
#ifndef NO_FDLIST
	long lastrecvM;		/* to check for activity --Mika */
	int  priority;
#endif
	long receiveK;		/* Statistics: total k-bytes received */
	u_short sendB;		/* counters to count upto 1-k lots of bytes */
	u_short receiveB;	/* sent and received. */
	aClient *listener;
	ConfigItem_class *class;		/* Configuration record associated */
	int authfd;		/* fd for rfc931 authentication */
        short slot;         /* my offset in the local fd table */
	struct IN_ADDR ip;	/* keep real ip# too */
	u_short port;		/* and the remote port# too :-) */
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

#define BAN_ACT_KILL		1
#define BAN_ACT_TEMPSHUN	2
#define BAN_ACT_SHUN		3
#define BAN_ACT_KLINE		4
#define BAN_ACT_ZLINE		5
#define BAN_ACT_GLINE		6
#define BAN_ACT_GZLINE		7
/* below are pretty much spamfilter only */
#define BAN_ACT_BLOCK		8
#define BAN_ACT_DCCBLOCK	9
#define BAN_ACT_VIRUSCHAN	10
#define BAN_ACT_WARN		11


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
	char	   *name, *info;
	unsigned short	   numeric;
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
	struct irc_netmask	*netmask;
	ConfigFlag_allow	flags;
};

struct _configitem_oper {
	ConfigItem       *prev, *next;
	ConfigFlag 	 flag;
	char		 *name, *swhois, *snomask;
	anAuthStruct	 *auth;
	ConfigItem_class *class;
	ConfigItem	 *from;
	unsigned long	 modes;
	long		 oflags;
	int			maxlogins;
};

struct _configitem_oper_from {
	ConfigItem       *prev, *next;
	ConfigFlag 	 flag;
	char		 *name;
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
	char 		*mask, *motd_file, *rules_file, *smotd_file;
	char 		*botmotd_file, *opermotd_file, *channel;
	struct tm	motd_tm, smotd_tm;
	aMotd		*rules, *motd, *smotd, *botmotd, *opermotd;
	u_short		options;
};

struct _configitem_listen {
	ConfigItem 	*prev, *next;
	ConfigFlag 	flag;
	char		*ip;
	int		port;
	int		options, clients;
	aClient		*listener;
};

struct _configitem_vhost {
	ConfigItem 	*prev, *next;
	ConfigFlag 	flag;
	ConfigItem       *from;
	char		*login, *virthost, *virtuser, *swhois;
	anAuthStruct	*auth;
};

struct _configitem_link {
	ConfigItem	*prev, *next;
	ConfigFlag	flag;
	char		*servername, *username, *hostname, *bindip, *hubmask, *leafmask, *connpwd;
	anAuthStruct	*recvauth;
	u_short		port, options;
	unsigned char 	leafdepth;
	int		refcount;
	ConfigItem_class	*class;
	struct IN_ADDR 		ipnum;
	time_t			hold;
#ifdef USE_SSL
	char		*ciphers;
#elif defined(_WIN32)
	void *ciphers_NOTUSED;
#endif
#ifdef ZIP_LINKS
	int compression_level;
#endif
};

typedef enum {
	CGIIRC_PASS=1, CGIIRC_WEBIRC=2
} CGIIRCType;

struct _configitem_cgiirc {
	ConfigItem  *prev, *next;
	ConfigFlag  flag;
	CGIIRCType type;
	char *username;
	char *hostname;
	anAuthStruct *auth;
};

struct _configitem_except {
	ConfigItem      *prev, *next;
	ConfigFlag_except      flag;
	int type;
	char		*mask;
	struct irc_netmask *netmask;
};

struct _configitem_ban {
	ConfigItem		*prev, *next;
	ConfigFlag_ban	flag;
	char			*mask, *reason;
	struct irc_netmask	*netmask;
	unsigned short action;
};

typedef struct _iplist IPList;
struct _iplist {
	IPList *prev, *next;
	char *mask;
/*	struct irc_netmask  *netmask; */
};

#ifdef FAST_BADWORD_REPLACE
#define BADW_TYPE_INVALID 0x0
#define BADW_TYPE_FAST    0x1
#define BADW_TYPE_FAST_L  0x2
#define BADW_TYPE_FAST_R  0x4
#define BADW_TYPE_REGEX   0x8
#endif

#define BADWORD_REPLACE 1
#define BADWORD_BLOCK 2

struct _configitem_badword {
	ConfigItem      *prev, *next;
	ConfigFlag	flag;
	char		*word, *replace;
#ifdef FAST_BADWORD_REPLACE
	unsigned short	type;
#endif
	char		action;
	regex_t 	expr;
};

struct _configitem_deny_dcc {
	ConfigItem		*prev, *next;
	ConfigFlag_ban		flag;
	char			*filename, *reason;
};

struct _configitem_deny_link {
	ConfigItem              *prev, *next;
	ConfigFlag_except       flag;
	char			*mask, *rule, *prettyrule;
};

struct _configitem_deny_version {
	ConfigItem		*prev, *next;
	ConfigFlag		flag;
	char 			*mask, *version, *flags;
};

struct _configitem_deny_channel {
	ConfigItem		*prev, *next;
	ConfigFlag		flag;
	char			*channel, *reason, *redirect;
	unsigned char	warn;
};

struct _configitem_allow_channel {
	ConfigItem		*prev, *next;
	ConfigFlag		flag;
	char			*channel;
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
	ALIAS_SERVICES=1, ALIAS_STATS, ALIAS_NORMAL, ALIAS_COMMAND, ALIAS_CHANNEL
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
	regex_t expr;
};

#define INCLUDE_NOTLOADED  0x1
#define INCLUDE_REMOTE     0x2
#define INCLUDE_DLQUEUED   0x4
#define INCLUDE_USED       0x8
	
struct _configitem_include {
	ConfigItem *prev, *next;
	ConfigFlag_ban flag;
	char *file;
#ifdef USE_LIBCURL
	char *url;
	char *errorbuf;
#endif
};

struct _configitem_help {
	ConfigItem *prev, *next;
	ConfigFlag flag;
	char *command;
	aMotd *text;
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

#ifdef EXTCMODE
#define EXTCMODETABLESZ 32
#endif /* EXTCMODE */

/* this can be like ~60-90 bytes, therefore it's in a seperate struct */
#define FLD_CTCP	0 /* c */
#define FLD_JOIN	1 /* j */
#define FLD_KNOCK	2 /* k */
#define FLD_MSG		3 /* m */
#define FLD_NICK	4 /* n */
#define FLD_TEXT	5 /* t */

#define NUMFLD	6 /* 6 flood types */

struct SRemoveFld {
	struct SRemoveFld *prev, *next;
	aChannel *chptr;
	char m; /* mode to be removed */
	time_t when; /* scheduled at */
};

struct SChanFloodProt {
	unsigned short	per; /* setting: per <XX> seconds */
	time_t			t[NUMFLD]; /* runtime: timers */
	unsigned short	c[NUMFLD]; /* runtime: counters */
	unsigned short	l[NUMFLD]; /* setting: limit */
	unsigned char	a[NUMFLD]; /* setting: action */
	unsigned char	r[NUMFLD]; /* setting: remove-after <this> minutes */
	unsigned long	timer_flags; /* if a "-m timer" is running this is & MODE_MODERATED etc.. */
};

/* mode structure for channels */
struct SMode {
	long mode;
#ifdef EXTCMODE
	Cmode_t extmode;
	CmodeParam *extmodeparam;
#endif
	int  limit;
	char key[KEYLEN + 1];
	char link[LINKLEN + 1];
#ifdef NEWCHFLOODPROT
	ChanFloodProt *floodprot;
#else
	/* x:y */
	unsigned short  msgs;		/* x */
	unsigned short  per;		/* y */
	unsigned char	 kmode;	/* mode  0 = kick  1 = ban */
#endif
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
};

struct Channel {
	struct Channel *nextch, *prevch, *hnextch;
	Mode mode;
	TS   creationtime;
	char *topic;
	char *topic_nick;
	TS   topic_time;
	unsigned short users;
	Member *members;
	Link *invites;
	Ban *banlist;
	Ban *exlist;		/* exceptions */
	Ban *invexlist;         /* invite list */
#ifdef JOINTHROTTLE
	aJFlood *jflood;
#endif
	char chname[1];
};

struct SMembershipL
{
	struct SMembership 	*next;
	struct Channel		*chptr;
	int			flags;
	aFloodOpt		flood;		
};

struct SMembership
{
	struct SMembership 	*next;
	struct Channel		*chptr;
	int			flags;
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
/* Backwards compatibility */
#define add_ConfigItem(item,list) add_ListItem((ListStruct *)item, (ListStruct **)&list)
#define del_ConfigItem(item,list) del_ListItem((ListStruct *)item, (ListStruct **)&list)

struct liststruct {
	ListStruct *prev, *next;
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
#define MODE_RGSTRONLY 		0x10000
#define MODE_LINK		0x20000
#define MODE_NOCOLOR		0x40000
#define MODE_OPERONLY   	0x80000
#define MODE_ADMONLY   		0x100000
#define MODE_NOKICKS   		0x200000
#define MODE_STRIP	   	0x400000
#define MODE_NOKNOCK		0x800000
#define MODE_NOINVITE  		0x1000000
#define MODE_FLOODLIMIT		0x2000000
#define MODE_MODREG		0x4000000
#define MODE_INVEX		0x8000000
#define MODE_NOCTCP		0x10000000
#define MODE_AUDITORIUM		0x20000000
#define MODE_ONLYSECURE		0x40000000
#define MODE_NONICKCHANGE	0x80000000

#define is_halfop is_half_op
/*
 * mode flags which take another parameter (With PARAmeterS)
 */
#define	MODE_WPARAS (MODE_HALFOP|MODE_CHANOP|MODE_VOICE|MODE_CHANOWNER|MODE_CHANPROT|MODE_BAN|MODE_KEY|MODE_LINK|MODE_LIMIT|MODE_EXCEPT|MODE_INVEX)
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
#define	MyOper(x)			(MyConnect(x) && IsOper(x))

#ifdef CLEAN_COMPILE
#define TStime() (time(NULL) + TSoffset)
#else
#define TStime() (timeofday == 0 ? (timeofday = time(NULL) + TSoffset) : timeofday)
#endif

/* Lifted somewhat from Undernet code --Rak */

#define IsSendable(x)		(DBufLength(&x->sendQ) < 2048)
#define DoList(x)		((x)->user && (x)->user->lopt)

/* String manipulation macros */

/* strncopynt --> strncpyzt to avoid confusion, sematics changed
   N must be now the number of bytes in the array --msa */
#define	strncpyzt(x, y, N) do{(void)strncpy(x,y,N);x[N-1]='\0';}while(0)
#define	StrEq(x,y)	(!strcmp((x),(y)))

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

extern MODVAR char *version, *infotext[], *dalinfotext[], *unrealcredits[];
extern MODVAR char *generation, *creation;
extern MODVAR char *gnulicense[];
/* misc defines */

#define	FLUSH_BUFFER	-2
#define	COMMA		","

#define PARTFMT		":%s PART %s"
#define PARTFMT2	":%s PART %s :%s"

#ifdef USE_SSL
#include "ssl.h"
#endif
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

#ifdef THROTTLING

struct ThrottlingBucket
{
	struct ThrottlingBucket *prev, *next;
	struct IN_ADDR	in;
	time_t		since;
	char		count;
};

typedef struct {
	long mode;
	char flag;
	unsigned  halfop : 1;       /* 1 = yes 0 = no */
	unsigned  parameters : 1;
} aCtab;

#ifdef JOINTHROTTLE
/** A jointhrottle item, this is a double linked list.
 * prev_u    Previous entry of user
 * next_u    Next entry of user
 * prev_c    Previous entry of channel
 * next_c    Next entry of channel
 * chptr     The channel this entry applies to
 * cptr      The user this entry applies to
 * firstjoin Timestamp of "first join" (since last timer reset)
 * numjoin   Number of joins since that period
 * CLARIFICATION:
 * Why a double linked list? Well, the following operations need to be performed:
 * - if user quits, entry must be removed
 * - if channel is destroyed, entry must be removed
 * (and of course, more, but these are the most important ones affecting this decision)
 * While it would be possible to have a linked list only by user (for example),
 * that would mean that upon channel destroy ALL entries would have to be searched
 * trough, which might mean for example 800*8=6400 entries in a peak situation
 * (such as after a server restart and hundreds of clients connecting&joining).
 * For obvious reasons, that would be a very bad idea :).
 * So this costs us 2 pointers (8b on ia32) per entry, but in case of channel destroy
 * it means we only have for example 20 entries to scan trough rather than 2000.
 * Worth the extra memory :). -- Syzop
 * Note that in normal situations it won't be that bad since we will try to
 * regulary free up some entries.
 */
struct JFlood {
	aJFlood *prev_u, *next_u;
	aJFlood *prev_c, *next_c;
	aChannel *chptr;
	aClient *cptr;
	time_t firstjoin;
	unsigned short numjoins;
};
#endif

void	init_throttling_hash();
int	hash_throttling(struct IN_ADDR *in);
struct	ThrottlingBucket	*find_throttling_bucket(struct IN_ADDR *in);
void	add_throttling_bucket(struct IN_ADDR *in);
void	del_throttling_bucket(struct ThrottlingBucket *bucket);
int	throttle_can_connect(aClient *, struct IN_ADDR *in);

#endif

#define VERIFY_OPERCOUNT(clnt,tag) { if (IRCstats.operators < 0) verify_opercount(clnt,tag); } while(0)

#define MARK_AS_OFFICIAL_MODULE(modinf)	do { if (modinf && modinf->handle) ModuleSetOptions(modinfo->handle, MOD_OPT_OFFICIAL);  } while(0)

/* old.. please don't use anymore */
#define CHANOPPFX "@"

/* used for is_banned type field: */
#define BANCHK_JOIN		0	/* checking if a ban forbids the person from joining */
#define BANCHK_MSG		1	/* checking if a ban forbids the person from sending messages */
#define BANCHK_NICK		2	/* checking if a ban forbids the person from changing his/her nick */

#define TKLISTLEN 26

#endif /* __struct_include__ */

#include "dynconf.h"
