/************************************************************************
 *   Unreal Internet Relay Chat Daemon, include/dynconf.h
 *   Copyright (C) 1999 Carsten Munk
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


#define DYNCONF_H

/* config level */
#define DYNCONF_CONF_VERSION "1.5"
#define DYNCONF_NETWORK_VERSION "2.2"

typedef struct zNetwork aNetwork;
struct zNetwork {
	unsigned x_inah:1;
	char *x_ircnetwork;
	char *x_defserv;
	char *x_services_name;
	char *x_oper_host;
	char *x_admin_host;
	char *x_locop_host;
	char *x_sadmin_host;
	char *x_netadmin_host;
	char *x_coadmin_host;
	char *x_techadmin_host;
	char *x_hidden_host;
	char *x_netdomain;
	char *x_www_site;
	char *x_ftp_site;
	char *x_prefix_quit;
	char *x_helpchan;
	char *x_stats_server;
};

typedef struct zConfiguration aConfiguration;
struct zConfiguration {
	unsigned som:1;
	unsigned mode_x:1;
	unsigned mode_i:1;
	unsigned mode_stripbadwords:1;
	unsigned truehub:1;
	unsigned stop:1;
	unsigned showopers:1;
	unsigned killdiff:1;
	unsigned hide_ulines:1;
	unsigned allow_chatops:1;
	unsigned webtv_support:1;
	unsigned no_oper_hiding:1;
	unsigned ident_check:1;
	long host_timeout;
	int  host_retries;
	char *name_server;
	char *kline_address;
	char *conn_modes;
	char *include;
	char *domainname;
	char *domainmask;	/* '*' + domainname */
	char *auto_join_chans;
	char *oper_auto_join_chans;
	int  socksbantime;
	int  maxchannelsperuser;
	char *socksbanmessage;
	char *socksquitmessage;
	aNetwork network;
};

#ifndef DYNCONF_C
extern aConfiguration iConf;
#endif

#define KLINE_ADDRESS		iConf.kline_address
#define INCLUDE				iConf.include
#define DOMAINNAMEMASK		"*" DOMAINNAME
#define MODE_X				iConf.mode_x
#define MODE_I				iConf.mode_i
#define MODE_STRIPWORDS			iConf.mode_stripbadwords
#define CONN_MODES			iConf.conn_modes
#define TRUEHUB				iConf.truehub
#define SHOWOPERS			iConf.showopers
#define KILLDIFF			iConf.killdiff
#define SHOWOPERMOTD			iConf.som
#define HIDE_ULINES			iConf.hide_ulines
#define ALLOW_CHATOPS			iConf.allow_chatops
#define MAXCHANNELSPERUSER		iConf.maxchannelsperuser
#define WEBTV_SUPPORT			iConf.webtv_support
#define NO_OPER_HIDING			iConf.no_oper_hiding
#define AUTO_JOIN_CHANS			iConf.auto_join_chans
#define OPER_AUTO_JOIN_CHANS		iConf.oper_auto_join_chans
#define HOST_TIMEOUT			iConf.host_timeout
#define HOST_RETRIES			iConf.host_retries
#define SOCKSBANMSG			iConf.socksbanmessage
#define SOCKSQUITMSG			iConf.socksquitmessage
#define SOCKSBANTIME			iConf.socksbantime
#define NAME_SERVER			iConf.name_server
#define IDENT_CHECK			iConf.ident_check


#define ircnetwork			iConf.network.x_ircnetwork
#define defserv				iConf.network.x_defserv
#define SERVICES_NAME		iConf.network.x_services_name
#define oper_host			iConf.network.x_oper_host
#define admin_host			iConf.network.x_admin_host
#define locop_host			iConf.network.x_locop_host
#define sadmin_host			iConf.network.x_sadmin_host
#define netadmin_host		iConf.network.x_netadmin_host
#define coadmin_host		iConf.network.x_coadmin_host
#define techadmin_host		iConf.network.x_techadmin_host
#define hidden_host			iConf.network.x_hidden_host
#define netdomain			iConf.network.x_netdomain
#define www_site			iConf.network.x_www_site
#define ftp_site			iConf.network.x_ftp_site
#define helpchan			iConf.network.x_helpchan
#define STATS_SERVER		iConf.network.x_stats_server
#define iNAH				iConf.network.x_inah
#define prefix_quit			iConf.network.x_prefix_quit
#define STOPSE				iConf.network.x_se
