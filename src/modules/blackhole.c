/*
 *   IRC - Internet Relay Chat, src/modules/scan.c
 *   (C) 2001 Carsten Munk (Techie/Stskeeps) <stskeeps@tspre.org>
 *
 *   Blackhole, /dev/null for IRCds, often utilized for proxy checks
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers. 
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
 */
#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include "userload.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#else
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/resource.h>

#endif
#include <fcntl.h>
#include "h.h"
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif
#include "modules/blackhole.h"
ModuleInfo blackhole_info
  = {
  	1,
	"dummy",	/* Name of module */
	"$Id$", /* Version */
	"command /dummy", /* Short description of module */
	NULL, /* Pointer to our dlopen() return value */
	NULL 
    };

/*
 * The purpose of these ifdefs, are that we can "static" link the ircd if we
 * want to
*/
MUTEX			blackhole_mutex;
ConfigItem_blackhole	blackhole_conf;
char			blackhole_stop = 0;

DLLFUNC int	h_config_set_blackhole(void);
DLLFUNC void blackhole(void *p);

/* This is called on module init, before Server Ready */
#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_init(void)
#else
void    blackhole_init(void)
#endif
{
	/* extern variable to export blackhole_info to temporary
           ModuleInfo *modulebuffer;
	   the module_load() will use this to add to the modules linked 
	   list
	*/
	module_buffer = &blackhole_info;
	blackhole_stop = 0;
	bzero(&blackhole_conf, sizeof(blackhole_conf));
	add_Hook(HOOKTYPE_CONFIG_UNKNOWN, h_config_set_blackhole);
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_load(void)
#else
void    blackhole_load(void)
#endif
{
	THREAD			thread;
	THREAD_ATTR		thread_attr;
	if (!blackhole_conf.ip || !blackhole_conf.port)
	{
		config_error("set::blackhole: missing ip/port mask");
	}
	IRCCreateThread(thread, thread_attr, blackhole, NULL);
}


/* Called when module is unloaded */

#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_unload(void)
#else
void	blackhole_unload(void)
#endif
{
	blackhole_stop = 1;
	while (blackhole_stop == 1) {}
	del_Hook(HOOKTYPE_CONFIG_UNKNOWN, h_config_set_blackhole);
}

DLLFUNC int	h_config_set_blackhole(void)
{
	ConfigItem_unknown_ext *sets;
	
	char	*ip;
	char	*port;
	int	iport;
		
	for (sets = conf_unknown_set; sets; 
		sets = (ConfigItem_unknown_ext *)sets->next)
	{
		if (!strcmp(sets->ce_varname, "blackhole"))
		{
			if (!sets->ce_vardata)
			{
				config_error("%s:%i: set::blackhole - missing parameter");
				goto explodeblackhole;
			}
			ipport_seperate(sets->ce_vardata, &ip, &port);
			if (!ip || !*ip)
			{
				config_error("%s:%i: set::blackhole: illegal ip:port mask",
					sets->ce_fileptr->cf_filename, sets->ce_varlinenum);
				goto explodeblackhole;
			}
			if (strchr(ip, '*'))
			{
				config_error("%s:%i: set::blackhole: illegal ip, (mask)",
					sets->ce_fileptr->cf_filename, sets->ce_varlinenum);
				goto explodeblackhole;
			}
			if (!port || !*port)
			{
				config_error("%s:%i: set::blackhole: missing port in mask",
					sets->ce_fileptr->cf_filename, sets->ce_varlinenum);
				goto explodeblackhole;
			}
			iport = atol(port);
			if ((iport < 0) || (iport > 65535))
			{
				config_error("%s:%i: set::blackhole: illegal port (must be 0..65536)",
					sets->ce_fileptr->cf_filename, sets->ce_varlinenum);
				goto explodeblackhole;
			}
 			blackhole_conf.ip = strdup(ip);
 			blackhole_conf.port = iport;			
	explodeblackhole:
			del_ConfigItem((ConfigItem *)sets, 
				(ConfigItem **) &conf_unknown_set);
			continue;
		}	
	}
	if (!blackhole_conf.ip || !blackhole_conf.port)
	{
		config_error("set::blackhole: missing ip/port mask");
	}
	return 0;
}

DLLFUNC void blackhole(void *p)
{
	int	blackholefd;
	int	callerfd;
	struct SOCKADDR_IN sin;

	/* Set up blackhole socket */

	if ((blackholefd = socket(AFINET, SOCK_STREAM, 0)) == -1)
		goto end;

	sin.SIN_ADDR.S_ADDR = inet_addr(blackhole_conf.ip);
	sin.SIN_PORT = htons(blackhole_conf.port);
	sin.SIN_FAMILY = AFINET;

	if (bind(blackholefd, (struct SOCKADDR *)&sin, sizeof(sin)))
	{
		CLOSE_SOCK(blackholefd);
		goto end;
	}

	listen(blackholefd, LISTEN_SIZE);
	
		
	while (!blackhole_stop)
	{
		callerfd = accept(blackholefd, NULL, NULL);
		CLOSE_SOCK(callerfd);
	}
	end:
	blackhole_stop = 0;
	IRCExitThread(NULL);	
}