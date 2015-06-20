/*
 * Configuration file updater - upgrade from 3.2.x to 3.4.x
 * (C) Copyright 2015 Bram Matthys and the UnrealIRCd team
 *
 * License: GPLv2
 */
 
#include "unrealircd.h"

extern ConfigFile *config_load(char *filename);
extern void config_free(ConfigFile *cfptr);

char configfiletmp[512];

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
	char *rdbuf, *wbuf;
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
	
	MyFree(rdbuf);

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
	
	MyFree(rdbuf);
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
	char *name;
	char *info;
	int numeric;

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
			            "This is no longer supported in UnrealIRCd 3.4.x",
			            ce->ce_vardata);
			
			snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf),
			         "\tpassword \"%s\"; /* WARNING: password changed due to 3.4.x upgrade */\n",
			         password_connect);
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
	char sid[16];
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
		snprintf(buf, sizeof(buf), "include \"modules.full.conf\";\n");
		replace_section(ce, buf);
		config_status("- loadmodule for '%s' replaced with an include \"modules.full.conf\"", file);
		return 1;
	}
	
	if (our_strcasestr(file, "cloak.dll") || our_strcasestr(file, "/cloak.so"))
	{
		replace_section(ce, "/* NOTE: cloaking module is included in modules.full.conf */");
		config_status("- loadmodule for '%s' removed as this is now in modules.full.conf", file);
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
	
	if (!_match("badwords.*.conf", file))
	{
		if (badwords_upgraded_already)
		{
			strcpy(buf, "/* all badwords are now in badwords.conf */\n");
		} else {
			strcpy(buf, "include \"badwords.conf\";\n");
			badwords_upgraded_already = 1;
		}
		replace_section(ce, buf);
		config_status("- include for '%s' replaced (now in badwords.conf)", file);
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

#define MAXALLOWOPTIONS 16
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
	char *options[MAXSPFTARGETS];
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
					if (optionscnt == MAXALLOWOPTIONS)
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
		snprintf(comment, sizeof(comment), "/* CHANGED BY 3.2.x TO 3.4.x CONF UPGRADE!! Was: ip %s; hostname %s; */\n", ip, hostname);
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
	
	/* maxperip: optional in 3.2.x, mandatory in 3.4.x */
	if (maxperip)
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\tmaxperip %s;\n", maxperip);
	else
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "\tmaxperip 3; /* CHANGED BY 3.2.x TO 3.4.x CONF UPGRADE! */\n");
	
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
			for (cep = ce->ce_entries; cep; cep = cep->ce_next)
			{
				if (!strcmp(cep->ce_varname, "from"))
				{
					if (upgrade_from_subblock(cep))
						goto again;
				}
			}
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
			if (!already_included(ce->ce_vardata, *cf_list))
				build_include_list_ex(ce->ce_vardata, cf_list);
}

ConfigFile *build_include_list(char *fname)
{
	ConfigFile *cf_list = NULL;
	
	build_include_list_ex(fname, &cf_list);
	return cf_list;
}

void update_conf(void)
{
	ConfigFile *cf;
	ConfigEntry *ce, *cep, *cepp;
	char *mainconf = configfile;
	int upgraded_files = 0;

	config_status("Attempting to upgrade '%s' (and all it's included files) from UnrealIRCd 3.2.x to UnrealIRCd 3.4.x...", configfile);
	
	strlcpy(me.name, "<server>", sizeof(me.name));

	cf = build_include_list(mainconf);

	for (; cf; cf = cf->cf_next)
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
	
	if (upgraded_files > 0)
	{
		config_status("");
		config_status("%d configuration file(s) upgraded. You can now boot UnrealIRCd with your freshly converted conf's!", upgraded_files);
		config_status("");
	} else {
		config_status("");
		config_status("No configuration files were changed. No upgrade was needed. If this is incorrect then please report on https://bugs.unrealircd.org/ !");
		config_status("");
	}
}

