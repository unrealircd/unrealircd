/************************************************************************
 *   IRC - Internet Relay Chat, include/dynconf.h
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
typedef struct zNetwork		aNetwork;
struct	zNetwork {
	char		*x_ircnetwork;
	char		*x_defserv;
	char		*x_services_name;
	char		*x_oper_host;
	char		*x_admin_host;
	char		*x_locop_host;
	char		*x_sadmin_host;
	char		*x_netadmin_host;
	char		*x_coadmin_host;
	char		*x_techadmin_host;
	char		*x_hidden_host;
	char		*x_netdomain;
	char		*x_helpchan;
	char		*x_stats_server;
	int			x_halfhub;
	int			x_inah;
	char		*x_net_quit;
	int			x_se;
};

typedef struct zConfiguration aConfiguration;
struct	zConfiguration {
	long		nospoof_seed01;
	long		nospoof_seed02;
	char		*kline_address;
	char    	*include;
	char		*domainname;
	char		*domainmask; /* '*' + domainname */
	int		som;
	int		mode_x;
	int		mode_i;
	int		truehub;
	int		stop;
	int		showopers;
	int		killdiff;
	int     	hide_ulines;
	int     	allow_chatops;
	int		socksbantime;
	char		*socksbanmessage;
	char		*socksquitmessage;	
	aNetwork	network;
};

#ifndef DYNCONF_C
extern	aConfiguration iConf;
#endif

// #define NOSPOOF_SEED01		iConf.nospoof_seed01
// #define NOSPOOF_SEED02		iConf.nospoof_seed02
#define KLINE_ADDRESS		iConf.kline_address
#define INCLUDE				iConf.include
#define DOMAINNAMEMASK		"*" DOMAINNAME
#define MODE_X				iConf.mode_x
#define MODE_I				iConf.mode_i
#define TRUEHUB				iConf.truehub
#define SHOWOPERS			iConf.showopers
#define KILLDIFF			iConf.killdiff
#define SHOWOPERMOTD			iConf.som
#define HIDE_ULINES			iConf.hide_ulines
#define ALLOW_CHATOPS		iConf.allow_chatops

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
#define helpchan			iConf.network.x_helpchan
#define STATS_SERVER		iConf.network.x_stats_server
#define iNAH				iConf.network.x_inah
#define net_quit			iConf.network.x_net_quit
#define STOPSE				iConf.network.x_se