/*
 * Configuration file updater - upgrade from 3.2.x to 4.0
 * (C) Copyright 2015 Bram Matthys and the UnrealIRCd team
 *
 * License: GPLv2
 */
 
#include "unrealircd.h"

extern ConfigFile *config_load(char *filename);
extern void config_free(ConfigFile *cfptr);

char configfiletmp[512];

struct _upgrade
{
	char *locop_host;
	char *oper_host;
	char *coadmin_host;
	char *admin_host;
	char *sadmin_host;
	char *netadmin_host;
	int host_on_oper_up;
};

struct _upgrade upgrade;

typedef struct flagmapping FlagMapping;
struct flagmapping
{
	char shortflag;
	char *longflag;
};

static FlagMapping FlagMappingTable[] = {
	{ 'o', "local" },
	{ 'O', "global" },
	{ 'r', "can_rehash" },
	{ 'D', "can_die" },
	{ 'R', "can_restart" },
	{ 'w', "can_wallops" },
	{ 'g', "can_globops" },
	{ 'c', "can_localroute" },
	{ 'L', "can_globalroute" },
	{ 'K', "can_globalkill" },
	{ 'b', "can_kline" },
	{ 'B', "can_unkline" },
	{ 'n', "can_localnotice" },
	{ 'G', "can_globalnotice" },
	{ 'A', "admin" },
	{ 'a', "services-admin" },
	{ 'N', "netadmin" },
	{ 'C', "coadmin" },
	{ 'z', "can_zline" },
	{ 'W', "get_umodew" },
	{ 'H', "get_host" },
	{ 't', "can_gkline" },
	{ 'Z', "can_gzline" },
	{ 'v', "can_override" },
	{ 'q', "can_setq" },
	{ 'd', "can_dccdeny" },
	{ 'T', "can_tsctl" },
	{ 0, NULL },
};

int needs_modules_default_conf = 1;
int needs_operclass_default_conf = 1;

static void die()
{
#ifdef _WIN32
	win_error(); /* ? */
#endif
	exit(0);
}

#define CFGBUFSIZE 1024
void modify_file(int start, char *ins, int stop)
{
	char configfiletmp2[512];
	FILE *fdi, *fdo;
	char *rdbuf = NULL, *wbuf;
	int n;
	int first = 1;
		
	snprintf(configfiletmp2, sizeof(configfiletmp2), "%s.tmp", configfiletmp); // .tmp.tmp :D

#ifndef _WIN32
	fdi = fopen(configfiletmp, "r");
	fdo = fopen(configfiletmp2, "w");
#else
	fdi = fopen(configfiletmp, "rb");
	fdo = fopen(configfiletmp2, "wb");
#endif

	if (!fdi || !fdo)
	{
		config_error("could not read/write to %s/%s", configfiletmp, configfiletmp2);
		die();
	}

	rdbuf = MyMallocEx(start);
	
	if ((n = fread(rdbuf, 1, start, fdi)) != start)
	{
		config_error("read error in remove_section(%d,%d): %d", start, stop, n);
		die();
	}
	
	fwrite(rdbuf, 1, start, fdo);
	
	safefree(rdbuf);

	if (ins)
		fwrite(ins, 1, strlen(ins), fdo); /* insert this piece */

	if (stop > 0)
	{
		if (fseek(fdi, stop+1, SEEK_SET) != 0)
			goto end; /* end of file we hope.. */
	}
	
	// read the remaining stuff
	rdbuf = MyMallocEx(CFGBUFSIZE);
	
	while(1)
	{
		n = fread(rdbuf, 1, CFGBUFSIZE, fdi);
		if (n <= 0)
			break; // done
		
		wbuf = rdbuf;
		
		if (first && (stop > 0))
		{
			if ((n > 0) && (*wbuf == '\r'))
			{
				wbuf++;
				n--;
			}
			if ((n > 0) && (*wbuf == '\n'))
			{
				wbuf++;
				n--;
			}
			first = 0;
			if (n <= 0)
				break; /* we are done (EOF) */
		}
		
		fwrite(wbuf, 1, n, fdo);
	}

end:
	fclose(fdi);
	fclose(fdo);
	
	safefree(rdbuf);
	// todo: handle write errors and such..

	unlink(configfiletmp);
	if (rename(configfiletmp2, configfiletmp) < 0)
	{
		config_error("Could not rename '%s' to '%s': %s", configfiletmp2, configfiletmp, strerror(errno));
		die();
	}
}

void remove_section(int start, int stop)
{
	modify_file(start, NULL, stop);
}

void insert_section(int start, char *buf)
{
#ifdef _WIN32
static char realbuf[16384];
char *i, *o;

	if (strlen(buf) > ((sizeof(realbuf)/2)-2))
		abort(); /* damn lazy you !!! */

	for (i = buf, o = realbuf; *i; i++)
	{
		if (*i == '\n')
		{
			*o++ = '\r';
			*o++ = '\n';
		} else
		{
			*o++ = *i;
		}
	}
	*o = '\0';
	
	modify_file(start, realbuf, 0);
#else
	modify_file(start, buf, 0);
#endif
}

void replace_section(ConfigEntry *ce, char *buf)
{
	remove_section(ce->ce_fileposstart, ce->ce_fileposend);
	insert_section(ce->ce_fileposstart, buf);
}

static char buf[8192];

int updconf_addquotes_r(char *i, char *o, size_t len)
{
	if (len == 0)
		return 0;
	
	len--; /* reserve room for nul byte */

	if (len == 0)
	{
		*o = '\0';
		return 0;
	}
	
	for (; *i; i++)
	{
		if ((*i == '"') || (*i == '\\')) /* only " and \ need to be quoted */
		{
			if (len < 2)
				break;
			*o++ = '\\';
			*o++ = *i;
			len -= 2;
		} else
		{
			if (len == 0)
				break;
			*o++ = *i;
			len--;
		}
	}
	*o = '\0';
	
	return 1;
}	

char *updconf_addquotes(char *str)
{
	static char qbuf[2048];
	
	*qbuf = '\0';
	updconf_addquotes_r(str, qbuf, sizeof(qbuf));
	return qbuf;
}

int upgrade_me_block(ConfigEntry *ce)
{
	ConfigEntry *cep, *cepp;
	char *name = NULL;
	char *info = NULL;
	int numeric = 0;

	char sid[16];

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
			continue; /* impossible? */
		else if (!strcmp(cep->ce_varname, "sid"))
			return 0; /* no upgrade needed */
		else if (!cep->ce_vardata)
		{
			config_error_empty(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"me", cep->ce_varname);
			return 0;
		}
		else if (!strcmp(cep->ce_varname, "name"))
			name = cep->ce_vardata;
		else if (!strcmp(cep->ce_varname, "info"))
			info = cep->ce_vardata;
		else if (!strcmp(cep->ce_varname, "numeric"))
			numeric = atoi(cep->ce_vardata);
	}
	
	if (!name || !info || !numeric)
	{
		/* Invalid block as it does not contain the 3.2.x mandatory items */
		return 0;
	}

	snprintf(sid, sizeof(sid), "%.3d", numeric);
		
	snprintf(buf, sizeof(buf),
			 "me {\n"
			 "\tname %s;\n"
			 "\tinfo \"%s\";\n"
			 "\tsid %s;\n"
			 "};\n",
			 name,
			 info,
			 sid);

	replace_section(ce, buf);
	
	config_status("- me block upgraded");
	return 1;
}

int upgrade_link_block(ConfigEntry *ce)
{
	ConfigEntry *cep, *cepp;
	char *bind_ip = NULL;
	char *username = NULL;
	char *hostname = NULL;
	char *port = NULL;
	char *password_receive = NULL;
	char *password_connect = NULL;
	char *class = NULL;
	int options_ssl = 0;
	int options_autoconnect = 0;
	int options_nohostcheck = 0;
	int options_quarantine = 0;
	/* options_nodnscache is deprecated, always now.. */
	char *hub = NULL;
	char *leaf = NULL;
	int leaf_depth = -1;
	char *ciphers = NULL;
	char *password_receive_authmethod = NULL;
	int need_incoming = 0, need_outgoing = 0;
	int upg_passwd_warning = 0;

	/* ripped from test_link */
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
			continue; /* impossible? */
		else if (!strcmp(cep->ce_varname, "incoming") || !strcmp(cep->ce_varname, "outgoing"))
			return 0; /* no upgrade needed */
		else if (!strcmp(cep->ce_varname, "options"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "ssl"))
					options_ssl = 1;
				if (!strcmp(cepp->ce_varname, "autoconnect"))
					options_autoconnect = 1;
				if (!strcmp(cepp->ce_varname, "nohostcheck"))
					options_nohostcheck = 1;
				if (!strcmp(cepp->ce_varname, "quarantine"))
					options_quarantine = 1;
			}
		}
		else if (!cep->ce_vardata)
		{
			config_error_empty(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"link", cep->ce_varname);
			return 0;
		}
		else if (!strcmp(cep->ce_varname, "username"))
			username = cep->ce_vardata;
		else if (!strcmp(cep->ce_varname, "hostname"))
			hostname = cep->ce_vardata;
		else if (!strcmp(cep->ce_varname, "bind-ip"))
			bind_ip = cep->ce_vardata;
		else if (!strcmp(cep->ce_varname, "port"))
			port = cep->ce_vardata;
		else if (!strcmp(cep->ce_varname, "password-receive"))
		{
			password_receive = cep->ce_vardata;
			if (cep->ce_entries)
				password_receive_authmethod = cep->ce_entries->ce_varname;
		}
		else if (!strcmp(cep->ce_varname, "password-connect"))
			password_connect = cep->ce_vardata;
		else if (!strcmp(cep->ce_varname, "class"))
			class = cep->ce_vardata;
		else if (!strcmp(cep->ce_varname, "hub"))
			hub = cep->ce_vardata;
		else if (!strcmp(cep->ce_varname, "leaf"))
			leaf = cep->ce_vardata;
		else if (!strcmp(cep->ce_varname, "leafdepth"))
			leaf_depth = atoi(cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "ciphers"))
			ciphers = cep->ce_vardata;
	}
	
	if (!username || !hostname || !class || !password_receive ||
	    !password_connect || !port)
	{
		/* Invalid link block as it does not contain the 3.2.x mandatory items */
		return 0;
	}
	
	if (strchr(hostname, '?') || strchr(hostname, '*'))
	{
		/* Wildcards in hostname: incoming only */
		need_incoming = 1;
		need_outgoing = 0;
	}
	else
	{
		/* IP (or hostname with nohostcheck) */
		need_incoming = 1;
		need_outgoing = 1;
	}

	snprintf(buf, sizeof(buf), "link %s {\n", ce->ce_vardata);
	
	if (need_incoming)
	{
		char upg_mask[HOSTLEN+USERLEN+8];
		
		if (options_nohostcheck)
		{
			strlcpy(upg_mask, "*", sizeof(upg_mask));
		}
		else
		{
			if (!strcmp(username, "*"))
				strlcpy(upg_mask, hostname, sizeof(upg_mask)); /* just host */
			else
				snprintf(upg_mask, sizeof(upg_mask), "%s@%s", username, hostname); /* user@host */
		}

		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\tincoming {\n\t\tmask %s;\n\t};\n", upg_mask);
	}

	if (need_outgoing)
	{
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf),
		         "\toutgoing {\n"
		         "\t\tbind-ip %s;\n"
		         "\t\thostname %s;\n"
		         "\t\tport %s;\n"
		         "\t\toptions { %s%s};\n"
		         "\t};\n",
		         bind_ip,
		         hostname,
		         port,
		         options_ssl ? "ssl; " : "",
		         options_autoconnect ? "autoconnect; " : "");
	}

	if (strcasecmp(password_connect, password_receive))
	{
		if (!password_receive_authmethod)
		{
			/* Prompt user ? */
			config_warn("Link block '%s' has a different connect/receive password. "
			            "This is no longer supported in UnrealIRCd 4.0",
			            ce->ce_vardata);
			
			snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf),
			         "\tpassword \"%s\"; /* WARNING: password changed due to 4.0 upgrade */\n",
			         options_autoconnect ? password_connect : password_receive);
		} else
		{
			/* sslcertificate or sslcertficatefp */
			snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf),
			         "\tpassword \"%s\" { %s; };\n",
			         password_receive,
			         password_receive_authmethod);
		}
	} else {
		/* identical connect & receive passwords. easy. */
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf),
		         "\tpassword \"%s\";\n", password_receive);
	}

	if (hub)
	{
		if (strcmp(hub, "*")) // only if it's something other than *, as * is the default anyway..
			snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\thub %s;\n", hub);
	} else
	if (leaf)
	{
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\tleaf %s;\n", leaf);
		if (leaf_depth)
			snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\tleaf-depth %d;\n", leaf_depth);
	}

	snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\tclass %s;\n", class);

	if (ciphers)
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\tciphers %s;\n", ciphers);

	if (options_quarantine)
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\toptions { quarantine; };\n");

	snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "};\n"); /* end */
	
	replace_section(ce, buf);
	
	config_status("- link block '%s' upgraded", ce->ce_vardata);
	return 1;
}

/** oper::from::userhost becomes oper::mask & vhost::from::userhost becomes vhost::mask */
#define MAXFROMENTRIES 100
int upgrade_from_subblock(ConfigEntry *ce)
{
	ConfigEntry *cep, *cepp;
	char *list[MAXFROMENTRIES];
	int listcnt = 0;

	memset(list, 0, sizeof(list));
	
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname || !cep->ce_vardata)
			continue;
		else if (!strcmp(cep->ce_varname, "userhost"))
		{
			if (listcnt == MAXFROMENTRIES)
				break; // no room, sorry.
			list[listcnt++] = cep->ce_vardata;
		}
	}
	
	if (listcnt == 0)
		return 0; /* invalid block. strange. */

	if (listcnt == 1)
	{
		char *m = !strncmp(list[0], "*@", 2) ? list[0]+2 : list[0]; /* skip or don't skip the user@ part */
		snprintf(buf, sizeof(buf), "mask %s;\n", m);
	} else
	{
		/* Multiple (list of masks) */
		int i;
		snprintf(buf, sizeof(buf), "mask {\n");
		
		for (i=0; i < listcnt; i++)
		{
			char *m = !strncmp(list[i], "*@", 2) ? list[i]+2 : list[i]; /* skip or don't skip the user@ part */
			snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\t%s;\n", m);
		}
		
		strlcat(buf, "\t};\n", sizeof(buf));
	}

	replace_section(ce, buf);
	
	config_status("- %s::from::userhost sub-block upgraded", ce->ce_prevlevel ? ce->ce_prevlevel->ce_varname : "???");
	return 1;
}

int upgrade_loadmodule(ConfigEntry *ce)
{
	char *file = ce->ce_vardata;
	char tmp[512], *p, *newfile;
	
	if (!file)
		return 0;

	if (our_strcasestr(file, "commands.dll") || our_strcasestr(file, "/commands.so"))
	{
		snprintf(buf, sizeof(buf), "include \"modules.default.conf\";\n");
		needs_modules_default_conf = 0;
		if (needs_operclass_default_conf)
		{
			/* This is a nice place :) */
			snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "include \"operclass.default.conf\";\n");
			needs_operclass_default_conf = 0;
		}
		replace_section(ce, buf);
		config_status("- loadmodule for '%s' replaced with an include \"modules.default.conf\"", file);
		return 1;
	}
	
	if (our_strcasestr(file, "cloak.dll") || our_strcasestr(file, "/cloak.so"))
	{
		replace_section(ce, "/* NOTE: cloaking module is included in modules.default.conf */");
		config_status("- loadmodule for '%s' removed as this is now in modules.default.conf", file);
		return 1;
	}

	/* All other loadmodule commands... */
	
	strlcpy(tmp, file, sizeof(tmp));
	p = strstr(tmp, ".so");
	if (p)
		*p = '\0';
	p = our_strcasestr(tmp, ".dll");
	if (p)
		*p = '\0';

	newfile = !strncmp(tmp, "src/", 4) ? tmp+4 : tmp;
	
	newfile = !strncmp(newfile, "modules/", 8) ? newfile+8 : newfile;
	
	if (!strcmp(newfile, file))
		return 0; /* no change */
	
	snprintf(buf, sizeof(buf), "loadmodule \"%s\";\n", newfile);
	replace_section(ce, buf);
	config_status("- loadmodule line converted to new syntax");
	return 1;
}

int upgrade_include(ConfigEntry *ce)
{
	char *file = ce->ce_vardata;
	char tmp[512], *p, *newfile;
	static int badwords_upgraded_already = 0;
	
	if (!file)
		return 0;

	if (!strstr(file, "help/") && !_match("help*.conf", file))
	{
		snprintf(buf, sizeof(buf), "include \"help/%s\";\n", file);
		replace_section(ce, buf);
		config_status("- include for '%s' replaced with 'help/%s'", file, file);
		return 1;
	}
	
	if (!strcmp("badwords.quit.conf", file))
	{
		*buf = '\0';
		replace_section(ce, buf);
		config_status("- include for '%s' removed (now in badwords.conf)", file);
		return 1;
	}

	if (!_match("badwords.*.conf", file))
	{
		if (badwords_upgraded_already)
		{
			*buf = '\0';
			config_status("- include for '%s' removed (now in badwords.conf)", file);
		} else {
			strcpy(buf, "/* all badwords are now in badwords.conf */\ninclude \"badwords.conf\";\n");
			badwords_upgraded_already = 1;
			config_status("- include for '%s' replaced with 'badwords.conf'", file);
		}
		replace_section(ce, buf);
		return 1;
	}
	
	return 0;
}

#define MAXSPFTARGETS 32
int upgrade_spamfilter_block(ConfigEntry *ce)
{
	ConfigEntry *cep, *cepp;
	char *reason = NULL;
	char *regex = NULL;
	char *action = NULL;
	char *ban_time = NULL;
	char *target[MAXSPFTARGETS];
	char targets[512];
	int targetcnt = 0;
	char *match_type = NULL;
	char *p;

	memset(target, 0, sizeof(target));
		
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
			continue; /* impossible? */
		else if (!strcmp(cep->ce_varname, "match") || !strcmp(cep->ce_varname, "match-type"))
			return 0; /* no upgrade needed */
		else if (!strcmp(cep->ce_varname, "target"))
		{
			if (cep->ce_vardata)
			{
				target[0] = cep->ce_vardata;
			}
			else if (cep->ce_entries)
			{
				for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
				{
					if (targetcnt == MAXSPFTARGETS)
						break;
					target[targetcnt++] = cepp->ce_varname;
				}
			}
		}
		else if (!cep->ce_vardata)
			continue; /* invalid */
		else if (!strcmp(cep->ce_varname, "regex"))
		{
			regex = cep->ce_vardata;
		}
		else if (!strcmp(cep->ce_varname, "action"))
		{
			action = cep->ce_vardata;
		}
		else if (!strcmp(cep->ce_varname, "reason"))
		{
			reason = cep->ce_vardata;
		}
		else if (!strcmp(cep->ce_varname, "ban-time"))
		{
			ban_time = cep->ce_vardata;
		}
	}

	if (!regex || !target[0] || !action)
		return 0; /* invalid spamfilter block */

	/* build target(s) list */
	if (targetcnt > 1)
	{
		int i;
		
		strlcpy(targets, "{ ", sizeof(targets));
		
		for (i=0; i < targetcnt; i++)
		{
			snprintf(targets+strlen(targets), sizeof(targets)-strlen(targets),
			         "%s; ", target[i]);
		}
		strlcat(targets, "}", sizeof(target));
	} else {
		strlcpy(targets, target[0], sizeof(targets));
	}

	/* Determine match-type, fallback to 'posix' (=3.2.x regex engine) */
	
	match_type = "simple";
	for (p = regex; *p; p++)
		if (!isalnum(*p) && !isspace(*p))
		{
			match_type = "posix";
			break;
		}

	snprintf(buf, sizeof(buf), "spamfilter {\n"
	                           "\tmatch-type %s;\n"
	                           "\tmatch \"%s\";\n"
	                           "\ttarget %s;\n"
	                           "\taction %s;\n",
	                           match_type,
	                           updconf_addquotes(regex),
	                           targets,
	                           action);

	/* optional: reason */
	if (reason)
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\treason \"%s\";\n", updconf_addquotes(reason));
	
	/* optional: ban-time */
	if (ban_time)
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\tban-time \"%s\";\n", ban_time);

	strlcat(buf, "};\n", sizeof(buf));
	
	replace_section(ce, buf);
	config_status("- spamfilter block converted to new syntax");
	return 1;
}

#define MAXOPTIONS 32
int upgrade_allow_block(ConfigEntry *ce)
{
	ConfigEntry *cep, *cepp;
	char *hostname = NULL;
	char *ip = NULL;
	char *maxperip = NULL;
	char *ipv6_clone_mask = NULL;
	char *password = NULL;
	char *password_type = NULL;
	char *class = NULL;
	char *redirect_server = NULL;
	int redirect_port = 0;
	char *options[MAXOPTIONS];
	int optionscnt = 0;
	char options_str[512], comment[512];

	memset(options, 0, sizeof(options));
	*comment = *options_str = '\0';
		
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
			continue; /* impossible? */
		else if (!strcmp(cep->ce_varname, "options"))
		{
			if (cep->ce_vardata)
			{
				options[0] = cep->ce_vardata;
				optionscnt = 1;
			}
			else if (cep->ce_entries)
			{
				for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
				{
					if (optionscnt == MAXOPTIONS)
						break;
					options[optionscnt++] = cepp->ce_varname;
				}
			}
		}
		else if (!cep->ce_vardata)
			continue; /* invalid */
		else if (!strcmp(cep->ce_varname, "hostname"))
		{
			hostname = cep->ce_vardata;
		}
		else if (!strcmp(cep->ce_varname, "ip"))
		{
			ip = cep->ce_vardata;
		}
		else if (!strcmp(cep->ce_varname, "maxperip"))
		{
			maxperip = cep->ce_vardata;
		}
		else if (!strcmp(cep->ce_varname, "ipv6-clone-mask"))
		{
			ipv6_clone_mask = cep->ce_vardata;
		}
		else if (!strcmp(cep->ce_varname, "password"))
		{
			password = cep->ce_vardata;
			if (cep->ce_entries)
				password_type = cep->ce_entries->ce_varname;
		}
		else if (!strcmp(cep->ce_varname, "class"))
		{
			class = cep->ce_vardata;
		}
		else if (!strcmp(cep->ce_varname, "redirect-server"))
		{
			redirect_server = cep->ce_vardata;
		}
		else if (!strcmp(cep->ce_varname, "redirect-port"))
		{
			redirect_port = atoi(cep->ce_vardata);
		}
	}

	if (!ip || !hostname || !class)
		return 0; /* missing 3.2.x items in allow block, upgraded already! (or invalid) */

	/* build target(s) list */
	if (optionscnt == 0)
	{
		*options_str = '\0';
	}
	else
	{
		int i;
		
		for (i=0; i < optionscnt; i++)
		{
			snprintf(options_str+strlen(options_str), sizeof(options_str)-strlen(options_str),
			         "%s; ", options[i]);
		}
	}

	/* drop either 'ip' or 'hostname' */
	if (!strcmp(ip, "*@*") && !strcmp(hostname, "*@*"))
		hostname = NULL; /* just ip */
	else if (strstr(ip, "NOMATCH"))
		ip = NULL;
	else if (strstr(hostname, "NOMATCH"))
		hostname = NULL;
	else if (!strchr(hostname, '.') && strcmp(hostname, "localhost"))
		hostname = NULL;
	else if (!strchr(ip, '.'))
		ip = NULL;
	else
	{
		/* very rare case -- let's bet on IP */
		snprintf(comment, sizeof(comment), "/* CHANGED BY 3.2.x TO 4.0 CONF UPGRADE!! Was: ip %s; hostname %s; */\n", ip, hostname);
		hostname = NULL;
	}

	snprintf(buf, sizeof(buf), "allow {\n");

	if (*comment)
		strlcat(buf, comment, sizeof(buf));

	if (ip)
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\tip \"%s\";\n", ip);

	if (hostname)
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\thostname \"%s\";\n", hostname);

	snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\tclass %s;\n", class);
	
	/* maxperip: optional in 3.2.x, mandatory in 4.0 */
	if (maxperip)
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\tmaxperip %s;\n", maxperip);
	else
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\tmaxperip 3; /* CHANGED BY 3.2.x TO 4.0 CONF UPGRADE! */\n");
	
	if (ipv6_clone_mask)
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\tipv6-clone-mask %s;\n", ipv6_clone_mask);

	if (password)
	{
		if (password_type)
			snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\tpassword \"%s\" { %s; };\n", password, password_type);
		else
			snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\tpassword \"%s\";\n", password);
	}

	if (redirect_server)
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\tredirect-server %s;\n", redirect_server);

	if (redirect_port)
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\tredirect-port %d;\n", redirect_port);

	if (*options_str)
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\toptions { %s};\n", options_str);

	strlcat(buf, "};\n", sizeof(buf));
	
	replace_section(ce, buf);
	config_status("- allow block converted to new syntax");
	return 1;
}

/* Pick out the ip address and the port number from a string.
 * The string syntax is:  ip:port.  ip must be enclosed in brackets ([]) if its an ipv6
 * address because they contain colon (:) separators.  The ip part is optional.  If the string
 * contains a single number its assumed to be a port number.
 *
 * Returns with ip pointing to the ip address (if one was specified), a "*" (if only a port
 * was specified), or an empty string if there was an error.  port is returned pointing to the
 * port number if one was specified, otherwise it points to a empty string.
 */
void ipport_separate(char *string, char **ip, char **port)
{
	char *f;

	/* assume failure */
	*ip = *port = "";

	/* sanity check */
	if (string && strlen(string) > 0)
	{
		/* handle ipv6 type of ip address */
		if (*string == '[')
		{
			if ((f = strrchr(string, ']')))
			{
				*ip = string + 1;	/* skip [ */
				*f = '\0';			/* terminate the ip string */
				/* next char must be a : if a port was specified */
				if (*++f == ':')
				{
					*port = ++f;
				}
			}
		}
		/* handle ipv4 and port */
		else if ((f = strchr(string, ':')))
		{
			/* we found a colon... we may have ip:port or just :port */
			if (f == string)
			{
				/* we have just :port */
				*ip = "*";
			}
			else
			{
				/* we have ip:port */
				*ip = string;
				*f = '\0';
			}
			*port = ++f;
		}
		/* no ip was specified, just a port number */
		else if (!strcmp(string, my_itoa(atoi(string))))
		{
			*ip = "*";
			*port = string;
		}
	}
}

int upgrade_listen_block(ConfigEntry *ce)
{
	ConfigEntry *cep, *cepp;
	char *ip = NULL;
	char *port = NULL;
	char *options[MAXOPTIONS];
	int optionscnt = 0;
	char options_str[512];
	char copy[128];

	memset(options, 0, sizeof(options));
	*options_str = '\0';

	if (!ce->ce_vardata)
		return 0; /* already upgraded */

	strlcpy(copy, ce->ce_vardata, sizeof(copy));
	ipport_separate(copy, &ip, &port);
	if (!ip || !*ip || !port || !*port)
		return 0; /* invalid conf */
	
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "options"))
		{
			if (cep->ce_entries)
			{
				for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
				{
					if (optionscnt == MAXOPTIONS)
						break;
					options[optionscnt++] = cepp->ce_varname;
				}
			}
		}
	}

	/* build options list */
	if (optionscnt == 0)
	{
		*options_str = '\0';
	}
	else
	{
		int i;
		
		for (i=0; i < optionscnt; i++)
		{
			snprintf(options_str+strlen(options_str), sizeof(options_str)-strlen(options_str),
			         "%s; ", options[i]);
		}
	}

	snprintf(buf, sizeof(buf), "listen {\n"
	                           "\tip %s;\n"
	                           "\tport %s;\n",
	                           ip,
	                           port);

	if (*options_str)
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\toptions { %s};\n", options_str);

	strlcat(buf, "};\n", sizeof(buf));
	
	replace_section(ce, buf);
	config_status("- listen block converted to new syntax");
	return 1;
}

int upgrade_cgiirc_block(ConfigEntry *ce)
{
	ConfigEntry *cep, *cepp;
	char *type = NULL;
	char *username = NULL;
	char *hostname = NULL;
	char *password = NULL, *password_type = NULL;
	char mask[USERLEN+HOSTLEN+8];

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
			continue; /* impossible? */
		else if (!cep->ce_vardata)
		{
			config_error_empty(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"cgiirc", cep->ce_varname);
			return 0;
		}
		else if (!strcmp(cep->ce_varname, "type"))
			type = cep->ce_vardata;
		else if (!strcmp(cep->ce_varname, "username"))
			username = cep->ce_vardata;
		else if (!strcmp(cep->ce_varname, "hostname"))
			hostname = cep->ce_vardata;
		else if (!strcmp(cep->ce_varname, "password"))
		{
			password = cep->ce_vardata;
			if (cep->ce_entries)
				password_type = cep->ce_entries->ce_varname;
		}
	}
	
	if (!type || !hostname)
	{
		/* Invalid block as it does not contain the 3.2.x mandatory items */
		return 0;
	}

	if (username)
		snprintf(mask, sizeof(mask), "%s@%s", username, hostname);
	else
		strlcpy(mask, hostname, sizeof(mask));

	if (!strcmp(type, "old"))
	{
		snprintf(buf, sizeof(buf),
		         "webirc {\n"
		         "\ttype old;\n"
		         "\tmask %s;\n",
		         mask);
	} else
	{
		if (password_type)
		{
			snprintf(buf, sizeof(buf),
					 "webirc {\n"
					 "\tmask %s;\n"
					 "\tpassword \"%s\" { %s; };\n"
					 "};\n",
					 mask,
					 password,
					 password_type);
		} else
		{
			snprintf(buf, sizeof(buf),
					 "webirc {\n"
					 "\tmask %s;\n"
					 "\tpassword \"%s\";\n"
					 "};\n",
					 mask,
					 password);
		}
	}

	replace_section(ce, buf);
	
	config_status("- cgiirc block upgraded and renamed to webirc");
	return 1;
}

int contains_flag(char **flags, int flagscnt, char *needle)
{
	int i;
	
	for (i = 0; i < flagscnt; i++)
		if (!strcmp(flags[i], needle))
			return 1;

	return 0;
}

int upgrade_oper_block(ConfigEntry *ce)
{
	ConfigEntry *cep, *cepp;
	char *name = NULL;
	char *password = NULL;
	char *password_type = NULL;
	char *require_modes = NULL;
	char *class = NULL;
	char *flags[MAXOPTIONS];
	int flagscnt = 0;
	char *swhois = NULL;
	char *snomask = NULL;
	char *modes = NULL;
	int maxlogins = -1;
	char *fromlist[MAXFROMENTRIES];
	int fromlistcnt = 0;
	char maskbuf[1024];
	char *operclass = NULL; /* set by us, not read from conf */
	char *vhost = NULL; /* set by us, not read from conf */
	int i;
	char silly[64];

	memset(flags, 0, sizeof(flags));
	*maskbuf = '\0';

	name = ce->ce_vardata;
	
	if (!name)
		return 0; /* oper block without a name = invalid */

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "operclass"))
			return 0; /* already 4.0 conf */
		else if (!strcmp(cep->ce_varname, "flags"))
		{
			if (cep->ce_vardata) /* short options (flag letters) */
			{
				char *p;
				for (p = cep->ce_vardata; *p; p++)
				{
					if (flagscnt == MAXOPTIONS)
						break;
					for (i = 0; FlagMappingTable[i].shortflag; i++)
					{
						if (FlagMappingTable[i].shortflag == *p)
						{
							flags[flagscnt] = FlagMappingTable[i].longflag;
							flagscnt++;
							break;
						}
					}
				}
			}
			else if (cep->ce_entries) /* long options (flags written out) */
			{
				for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
				{
					if (flagscnt == MAXOPTIONS)
						break;
					flags[flagscnt++] = cepp->ce_varname;
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "from"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "userhost") && cepp->ce_vardata)
				{
					if (fromlistcnt == MAXFROMENTRIES)
						break; // no room, sorry.
					fromlist[fromlistcnt++] = cepp->ce_vardata;
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "mask"))
		{
			/* processing mask here means we can also upgrade 3.4-alphaX oper blocks.. */
			if (cep->ce_vardata)
			{
				if (fromlistcnt == MAXFROMENTRIES)
					break; // no room, sorry.
				fromlist[fromlistcnt++] = cep->ce_vardata;
			} else
			{
				for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
				{
					if (fromlistcnt == MAXFROMENTRIES)
						break; // no room, sorry.
					fromlist[fromlistcnt++] = cepp->ce_varname;
				}
			}
		}
		else if (!cep->ce_vardata)
			continue; /* invalid */
		else if (!strcmp(cep->ce_varname, "password"))
		{
			password = cep->ce_vardata;
			if (cep->ce_entries)
				password_type = cep->ce_entries->ce_varname;
		}
		else if (!strcmp(cep->ce_varname, "require-modes"))
		{
			require_modes = cep->ce_vardata;
		}
		else if (!strcmp(cep->ce_varname, "class"))
		{
			class = cep->ce_vardata;
		}
		else if (!strcmp(cep->ce_varname, "swhois"))
		{
			swhois = cep->ce_vardata;
		}
		else if (!strcmp(cep->ce_varname, "snomasks"))
		{
			snomask = cep->ce_vardata;
		}
		else if (!strcmp(cep->ce_varname, "modes"))
		{
			modes = cep->ce_vardata;
		}
		else if (!strcmp(cep->ce_varname, "maxlogins"))
		{
			maxlogins = atoi(cep->ce_vardata);
		}
	}

	if ((fromlistcnt == 0) || !password || !class)
		return 0; /* missing 3.2.x items in allow block (invalid or upgraded already) */

	/* build oper::mask list in 'maskbuf' (includes variable name) */
	if (fromlistcnt == 1)
	{
		char *m = !strncmp(fromlist[0], "*@", 2) ? fromlist[0]+2 : fromlist[0]; /* skip or don't skip the user@ part */
		snprintf(maskbuf, sizeof(maskbuf), "mask %s;\n", m);
	} else
	{
		/* Multiple (list of masks) */
		int i;
		snprintf(maskbuf, sizeof(maskbuf), "mask {\n");
		
		for (i=0; i < fromlistcnt; i++)
		{
			char *m = !strncmp(fromlist[i], "*@", 2) ? fromlist[i]+2 : fromlist[i]; /* skip or don't skip the user@ part */
			snprintf(maskbuf+strlen(maskbuf), sizeof(maskbuf)-strlen(maskbuf), "\t%s;\n", m);
		}
		strlcat(maskbuf, "\t};\n", sizeof(maskbuf));
	}
	
	/* Figure out which default operclass looks most suitable (="find highest rank") */
	if (contains_flag(flags, flagscnt, "netadmin"))
		operclass = "netadmin";
	else if (contains_flag(flags, flagscnt, "services-admin"))
		operclass = "services-admin";
	else if (contains_flag(flags, flagscnt, "coadmin"))
		operclass = "coadmin";
	else if (contains_flag(flags, flagscnt, "admin"))
		operclass = "admin";
	else if (contains_flag(flags, flagscnt, "global"))
		operclass = "globop";
	else if (contains_flag(flags, flagscnt, "local"))
		operclass = "locop";
	else
	{
		/* Hmm :) */
		config_status("WARNING: I have trouble converting oper block '%s' to the new system. "
		              "I made it use the locop operclass. Feel free to change", name);
		operclass = "locop";
	}

	if (contains_flag(flags, flagscnt, "get_host") || upgrade.host_on_oper_up)
	{
		if (!strcmp(operclass, "netadmin"))
			vhost = upgrade.netadmin_host;
		else if (!strcmp(operclass, "services-admin"))
			vhost = upgrade.sadmin_host;
		else if (!strcmp(operclass, "coadmin"))
			vhost = upgrade.coadmin_host;
		else if (!strcmp(operclass, "admin"))
			vhost = upgrade.admin_host;
		else if (!strcmp(operclass, "globop"))
			vhost = upgrade.oper_host;
		else if (!strcmp(operclass, "locop"))
			vhost = upgrade.locop_host;
	}

	/* If no swhois is set, then set a title. Just because people are used to it. */
	if (!swhois)
	{
		if (!strcmp(operclass, "netadmin"))
			swhois = "is a Network Administrator";
		else if (!strcmp(operclass, "services-admin"))
			swhois = "is a Services Administrator";
		else if (!strcmp(operclass, "coadmin"))
			swhois = "is a Co Administrator";
		else if (!strcmp(operclass, "admin"))
			swhois = "is a Server Administrator";
	}

	/* The 'coadmin' operclass is actually 'admin'. There's no difference in privileges. */
	if (!strcmp(operclass, "coadmin"))
		operclass = "admin";
	
	/* convert globop and above w/override to operclassname-with-override */
	if (contains_flag(flags, flagscnt, "can_override") && strcmp(operclass, "locop"))
	{
		snprintf(silly, sizeof(silly), "%s-with-override", operclass);
		operclass = silly;
	}

	/* Ok, we got everything we need. Now we will write out the actual new oper block! */

	/* oper block header & oper::mask */
	snprintf(buf, sizeof(buf), "oper %s {\n"
	                           "\t%s",
	                           name,
	                           maskbuf);

	/* oper::password */
	if (password_type)
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\tpassword \"%s\" { %s; };\n", password, password_type);
	else
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\tpassword \"%s\";\n", password);

	/* oper::require-modes */
	if (require_modes)
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\trequire-modes \"%s\";\n", require_modes);

	/* oper::maxlogins */
	if (maxlogins != -1)
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\tmaxlogins %d;\n", maxlogins);

	/* oper::class */
	snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\tclass %s;\n", class);

	/* oper::operclass */
	snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\toperclass %s;\n", operclass);
	
	/* oper::modes */
	if (modes)
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\tmodes \"%s\";\n", modes);

	/* oper::snomask */
	if (snomask)
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\tsnomask \"%s\";\n", snomask);

	/* oper::vhost */
	if (vhost)
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\tvhost \"%s\";\n", vhost);

	/* oper::swhois */
	if (swhois)
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\tswhois \"%s\";\n", swhois);

	strlcat(buf, "};\n", sizeof(buf));
	
	replace_section(ce, buf);
	config_status("- oper block (%s) converted to new syntax", name);
	return 1;
}

void update_read_settings(char *cfgfile)
{
	ConfigFile *cf = NULL;
	ConfigEntry *ce = NULL, *cep, *cepp;

	cf = config_load(cfgfile);
	if (!cf)
		return;
		
	if (strstr(cfgfile, "modules.default.conf"))
		needs_modules_default_conf = 0;
	else if (strstr(cfgfile, "operclass.default.conf"))
		needs_operclass_default_conf = 0;

	/* This needs to be read early, as the rest may depend on it */
	for (ce = cf->cf_entries; ce; ce = ce->ce_next)
	{
		if (!strcmp(ce->ce_varname, "set"))
		{
			for (cep = ce->ce_entries; cep; cep = cep->ce_next)
			{
				if (!strcmp(cep->ce_varname, "hosts"))
				{
					for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
					{
						if (!cepp->ce_vardata)
							continue;
						if (!strcmp(cepp->ce_varname, "local")) {
							safestrdup(upgrade.locop_host, cepp->ce_vardata);
						}
						else if (!strcmp(cepp->ce_varname, "global")) {
							safestrdup(upgrade.oper_host, cepp->ce_vardata);
						}
						else if (!strcmp(cepp->ce_varname, "coadmin")) {
							safestrdup(upgrade.coadmin_host, cepp->ce_vardata);
						}
						else if (!strcmp(cepp->ce_varname, "admin")) {
							safestrdup(upgrade.admin_host, cepp->ce_vardata);
						}
						else if (!strcmp(cepp->ce_varname, "servicesadmin")) {
							safestrdup(upgrade.sadmin_host, cepp->ce_vardata);
						}
						else if (!strcmp(cepp->ce_varname, "netadmin")) {
							safestrdup(upgrade.netadmin_host, cepp->ce_vardata);
						}
						else if (!strcmp(cepp->ce_varname, "host-on-oper-up")) {
							upgrade.host_on_oper_up = config_checkval(cepp->ce_vardata,CFG_YESNO);
						}
					}
				}
			}
		}
	}

	config_free(cf);
}


int update_conf_file(void)
{
	ConfigFile *cf = NULL;
	ConfigEntry *ce = NULL, *cep, *cepp;
	int update_conf_runs = 0;

again:
	if (update_conf_runs++ > 100)
	{
		config_error("update conf re-run overflow. whoops! upgrade failed! sorry!");
		return 0;
	}
	
	if (cf)
	{
		config_free(cf);
		cf = NULL;
	}

	cf = config_load(configfiletmp);
	if (!cf)
	{
		config_error("could not load configuration file '%s'", configfile);
		return 0;
	}

	for (ce = cf->cf_entries; ce; ce = ce->ce_next)
	{
		/*printf("%s%s%s\n",
			ce->ce_varname,
			ce->ce_vardata ? " " : "",
			ce->ce_vardata ? ce->ce_vardata : ""); */
		
		if (!strcmp(ce->ce_varname, "loadmodule"))
		{
			if (upgrade_loadmodule(ce))
				goto again;
		}
		if (!strcmp(ce->ce_varname, "include"))
		{
			if (upgrade_include(ce))
				goto again;
		}
		if (!strcmp(ce->ce_varname, "me"))
		{
			if (upgrade_me_block(ce))
				goto again;
		}
		if (!strcmp(ce->ce_varname, "link"))
		{
			if (upgrade_link_block(ce))
				goto again;
		}
		if (!strcmp(ce->ce_varname, "oper"))
		{
			if (upgrade_oper_block(ce))
				goto again;
		}
		if (!strcmp(ce->ce_varname, "vhost"))
		{
			for (cep = ce->ce_entries; cep; cep = cep->ce_next)
			{
				if (!strcmp(cep->ce_varname, "from"))
				{
					if (upgrade_from_subblock(cep))
						goto again;
				}
			}
		}
		if (!strcmp(ce->ce_varname, "spamfilter"))
		{
			if (upgrade_spamfilter_block(ce))
				goto again;
		}
		if (!strcmp(ce->ce_varname, "allow") && !ce->ce_vardata) /* 'allow' block for clients, not 'allow channel' etc.. */
		{
			if (upgrade_allow_block(ce))
				goto again;
		}
		if (!strcmp(ce->ce_varname, "listen"))
		{
			if (upgrade_listen_block(ce))
				goto again;
		}
		if (!strcmp(ce->ce_varname, "cgiirc"))
		{
			if (upgrade_cgiirc_block(ce))
				goto again;
		}
		if (!strcmp(ce->ce_varname, "set"))
		{
			for (cep = ce->ce_entries; cep; cep = cep->ce_next)
			{
				if (!strcmp(cep->ce_varname, "throttle"))
				{
					int n = 0, t = 0;
					for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
					{
						if (!cepp->ce_vardata)
							continue;
						if (!strcmp(cepp->ce_varname, "period"))
							t = config_checkval(cepp->ce_vardata, CFG_TIME);
						else if (!strcmp(cepp->ce_varname, "connections"))
							n = atoi(cepp->ce_vardata);
					}

					remove_section(cep->ce_fileposstart, cep->ce_fileposend);
					snprintf(buf, sizeof(buf), "anti-flood { connect-flood %d:%d; };\n",
					         n, t);
						
					insert_section(cep->ce_fileposstart, buf);
					goto again;
				} else
				if (!strcmp(cep->ce_varname, "hosts"))
				{
					config_status("- removed set::hosts. we now use oper::vhost for this.");
					remove_section(cep->ce_fileposstart, cep->ce_fileposend); /* hmm something is wrong here */
					goto again;
				} else
				if (!strcmp(cep->ce_varname, "dns"))
				{
					for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
						if (!strcmp(cepp->ce_varname, "nameserver") ||
						    !strcmp(cepp->ce_varname, "timeout") ||
						    !strcmp(cepp->ce_varname, "retries"))
						{
							config_status("- removed set::dns::%s. this option is never used.", cepp->ce_varname);
							remove_section(cepp->ce_fileposstart, cepp->ce_fileposend);
							goto again;
						}
				}
			}
		}
		
	}	

	if (cf)
		config_free(cf);
	
	return (update_conf_runs > 1) ? 1 : 0;
}

static int already_included(char *fname, ConfigFile *cf)
{
	for (; cf; cf = cf->cf_next)
		if (!strcmp(cf->cf_filename, fname))
			return 1;

	return 0;
}

static void add_include_list(char *fname, ConfigFile **cf)
{
	ConfigFile *n = MyMallocEx(sizeof(ConfigFile));
	
//	config_status("INCLUDE: %s", fname);
	n->cf_filename = strdup(fname);
	n->cf_next = *cf;
	*cf = n;
}

void build_include_list_ex(char *fname, ConfigFile **cf_list)
{
	ConfigFile *cf, *cf2;
	ConfigEntry *ce;

	if (strstr(fname, "://"))
		return; /* Remote include - ignored */

	add_include_list(fname, cf_list);

	cf = config_load(fname);
	if (!cf)
		return;

	for (ce = cf->cf_entries; ce; ce = ce->ce_next)
		if (!strcmp(ce->ce_varname, "include"))
		{
			if ((ce->ce_vardata[0] != '/') && (ce->ce_vardata[0] != '\\') && strcmp(ce->ce_vardata, CPATH))
			{
				char *str = MyMallocEx(strlen(ce->ce_vardata) + strlen(CONFDIR) + 4);
				sprintf(str, "%s/%s", CONFDIR, ce->ce_vardata);
				MyFree(ce->ce_vardata);
				ce->ce_vardata = str;
			}
			if (!already_included(ce->ce_vardata, *cf_list))
				build_include_list_ex(ce->ce_vardata, cf_list);
		}
	
	config_free(cf);
}

ConfigFile *build_include_list(char *fname)
{
	ConfigFile *cf_list = NULL;
	
	build_include_list_ex(fname, &cf_list);
	return cf_list;
}

void update_conf(void)
{
	ConfigFile *files;
	ConfigFile *cf;
	ConfigEntry *ce, *cep, *cepp;
	char *mainconf = configfile;
	int upgraded_files = 0;

	config_status("Attempting to upgrade '%s' (and all it's included files) from UnrealIRCd 3.2.x to UnrealIRCd 4.0...", configfile);
	
	strlcpy(me.name, "<server>", sizeof(me.name));
	memset(&upgrade, 0, sizeof(upgrade));

	files = build_include_list(mainconf);

	/* We need to read some original settings first, before we touch anything... */
	for (cf = files; cf; cf = cf->cf_next)
	{
		update_read_settings(cf->cf_filename);
	}
	
	/* Now go upgrade... */
	for (cf = files; cf; cf = cf->cf_next)
	{
		if (!file_exists(cf->cf_filename))
			continue; /* skip silently. errors were already shown earlier by build_include_list anyway. */
		configfile = cf->cf_filename;
		config_status("Checking '%s'...", cf->cf_filename);
		snprintf(configfiletmp, sizeof(configfiletmp), "%s.tmp", configfile);
		unlink(configfiletmp);
		if (!unreal_copyfileex(configfile, configfiletmp, 0))
		{
			config_error("Could not create temp file for processing!");
			die();
		}
		if (update_conf_file())
		{
			char buf[512];
			snprintf(buf, sizeof(buf), "%s.old", configfile);
			if (file_exists(buf))
			{
				int i;
				for (i=0; i<100; i++)
				{
					snprintf(buf, sizeof(buf), "%s.old.%d", configfile, i);
					if (!file_exists(buf))
						break;
				}
			}
			/* rename original config file to ... */
			if (rename(configfile, buf) < 0)
			{
				config_error("Could not rename original conf '%s' to '%s'", configfile, buf);
				die();
			}
			
			/* Rename converted conf to config file */
#ifdef _WIN32
			/* "If newpath already exists it will be atomically replaced"..
			 * well.. not on Windows! Error: "File exists"...
			 */
			unlink(configfile);
#endif
			if (rename(configfiletmp, configfile) < 0)
			{
				config_error("Could not rename converted configuration file '%s' to '%s' -- please rename this file yourself!",
					configfiletmp, configfile);
				die();
			}
			
			config_status("File '%s' upgrade complete.", configfile);
			upgraded_files++;
		} else {
			unlink(configfiletmp);
			config_status("File '%s' left unchanged (no upgrade necessary)", configfile);
		}
	}
	configfile = mainconf; /* restore */

	if (needs_operclass_default_conf)
	{
		/* There's a slight chance we never added this include, and you get mysterious
		 * oper permissions errors if you try to use such an operclass and it's missing.
		 */
		FILE *fd = fopen(mainconf, "a");
		if (fd)
		{
			fprintf(fd, "\ninclude \"operclass.default.conf\";\n");
			fclose(fd);
			config_status("Oh wait, %s needs an include for operclass.default.conf. Added.", mainconf);
			upgraded_files++;
		}
	}	
	
	if (upgraded_files > 0)
	{
		config_status("");
		config_status("%d configuration file(s) upgraded. You can now boot UnrealIRCd with your freshly converted conf's!", upgraded_files);
		config_status("You should probably take a look at the converted configuration files now or at a later time.");
		config_status("See also https://www.unrealircd.org/docs/Upgrading_from_3.2.x and the sections in there (eg: Oper block)");
		config_status("");
	} else {
		config_status("");
		config_status("No configuration files were changed. No upgrade was needed. If this is incorrect then please report on https://bugs.unrealircd.org/ !");
		config_status("");
	}
}

