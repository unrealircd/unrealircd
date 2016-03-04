/*
 * Blacklist support (currently just DNS Blacklists) 
 * (C) Copyright 2015-.. Bram Matthys (Syzop) and the UnrealIRCd team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "unrealircd.h"
#include "res.h"

ModuleHeader MOD_HEADER(blacklist)
= {
	"blacklist",
	"4.0",
	"Check connecting users against DNS Blacklists",
	"3.2-b8-1",
	NULL
};

/* In this module and the config syntax I tried to 'abstract' things
 * a little, so things could later be extended if we ever want
 * to introduce another blacklist type (other than DNSBL).
 * Once that happens, best to re-check/audit the source.
 */

/* Types */

typedef enum {
	DNSBL_RECORD=1, DNSBL_BITMASK=2
} DNSBLType;

typedef struct _dnsbl DNSBL;
struct _dnsbl {
	char *name;
	DNSBLType type;
	int *reply;
};

typedef union _blacklistbackend BlacklistBackend;
union _blacklistbackend
{
	DNSBL *dns;
};

typedef enum {
	BLACKLIST_BACKEND_DNS = 1
} BlacklistBackendType;

typedef struct _blacklist Blacklist;
struct _blacklist {
	Blacklist *prev, *next;
	char *name;
	BlacklistBackendType backend_type;
	BlacklistBackend *backend;
	int action;
	long ban_time;
	char *reason;
};

/* Blacklist user struct. In the c-ares DNS reply callback we need to pass
 * some metadata. We can't use cptr directly there as the client may
 * be gone already by the time we receive the DNS reply.
 */
typedef struct _bluser BLUser;
struct _bluser {
	aClient *cptr;
	int refcnt;
};

/* Global variables */
ModDataInfo *blacklist_md = NULL;
Blacklist *conf_blacklist = NULL;

/* Forward declarations */
int blacklist_config_test(ConfigFile *, ConfigEntry *, int, int *);
int blacklist_config_run(ConfigFile *, ConfigEntry *, int);
void blacklist_free_conf(void);
void delete_blacklist_block(Blacklist *e);
void blacklist_md_free(ModData *md);
int blacklist_handshake(aClient *cptr);
int blacklist_quit(aClient *cptr, char *comment);
void blacklist_resolver_callback(void *arg, int status, int timeouts, struct hostent *he);
int blacklist_start_check(aClient *cptr);
int blacklist_dns_request(aClient *cptr, Blacklist *bl);
int blacklist_rehash(void);
void blacklist_free_bluser_if_able(BLUser *bl);

#define SetBLUser(x, y)	do { moddata_client(x, blacklist_md).ptr = y; } while(0)
#define BLUSER(x)	((BLUser *)moddata_client(x, blacklist_md).ptr)

long SNO_BLACKLIST = 0L;

MOD_TEST(blacklist)
{
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, blacklist_config_test);
	return MOD_SUCCESS;
}

/** Called upon module init */
MOD_INIT(blacklist)
{
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);
	ModuleSetOptions(modinfo->handle, MOD_OPT_PERM, 1);
	
	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "blacklist";
	mreq.type = MODDATATYPE_CLIENT;
	mreq.free = blacklist_md_free;
	blacklist_md = ModDataAdd(modinfo->handle, mreq);
	if (!blacklist_md)
	{
		config_error("could not register blacklist moddata");
		return MOD_FAILED;
	}

	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, blacklist_config_run);
	HookAdd(modinfo->handle, HOOKTYPE_HANDSHAKE, 0, blacklist_handshake);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_QUIT, 0, blacklist_quit);
	HookAdd(modinfo->handle, HOOKTYPE_UNKUSER_QUIT, 0, blacklist_quit);
	HookAdd(modinfo->handle, HOOKTYPE_REHASH, 0, blacklist_rehash);

	SnomaskAdd(modinfo->handle, 'b', 1, umode_allow_opers, &SNO_BLACKLIST);

	return MOD_SUCCESS;
}

/** Called upon module load */
MOD_LOAD(blacklist)
{
	return MOD_SUCCESS;
}

/** Called upon unload */
MOD_UNLOAD(blacklist)
{
	blacklist_free_conf();
	return MOD_SUCCESS;
}

int blacklist_rehash(void)
{
	blacklist_free_conf();
	return 0;
}

/** Find blacklist { } block */
Blacklist *blacklist_find_block_by_dns(char *name)
{
	Blacklist *d;
	
	for (d = conf_blacklist; d; d = d->next)
		if ((d->backend_type == BLACKLIST_BACKEND_DNS) && !strcmp(name, d->backend->dns->name))
			return d;

	return NULL;
}

void blacklist_free_conf(void)
{
	Blacklist *d, *d_next;

	for (d = conf_blacklist; d; d = d_next)
	{
		d_next = d->next;
		delete_blacklist_block(d);
	}
	conf_blacklist = NULL;
}

void delete_blacklist_block(Blacklist *e)
{
	if (e->backend_type == BLACKLIST_BACKEND_DNS)
	{
		if (e->backend->dns)
		{
			MyFree(e->backend->dns->name);
			MyFree(e->backend->dns->reply);
			MyFree(e->backend->dns);
		}
	}
	
	MyFree(e->backend);

	MyFree(e->name);
	MyFree(e->reason);
	MyFree(e);
}

int blacklist_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	ConfigEntry	*cep, *cepp, *ceppp;
	int errors = 0;
	char has_reason = 0, has_ban_time = 0, has_action = 0;
	char has_dns_type = 0, has_dns_reply = 0, has_dns_name = 0;
	DNSBLType blacklist_type = 0;

	if (type != CONFIG_MAIN)
		return 0;
	
	if (!ce || !ce->ce_varname)
		return 0;
	
	if (strcmp(ce->ce_varname, "blacklist"))
		return 0; /* not interested in non-blacklist stuff.. */
	
	if (!ce->ce_vardata)
	{
		config_error("%s:%i: blacklist block without name (use: blacklist somename { })",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		*errs = 1;
		return -1;
	}

	/* Now actually go parse the blacklist { } block */
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error_blank(cep->ce_fileptr->cf_filename, cep->ce_varlinenum, "blacklist");
			errors++;
			continue;
		}
		if (!strcmp(cep->ce_varname, "dns"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "reply"))
				{
					if (has_dns_reply)
					{
						/* this is an error (not a warning) */
						config_error("%s:%i: blacklist block may contain only one blacklist::dns::reply item. "
									 "You can specify multiple replies by using: reply { 1; 2; 4; };",
									 cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum);
						errors++;
						continue;
					}
					if (!cepp->ce_vardata && !(cepp->ce_entries && cepp->ce_entries->ce_varname))
					{
						config_error_blank(cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum, "blacklist::dns::reply");
						errors++;
						continue;
					}
					has_dns_reply = 1; /* we have a reply. now whether it's actually valid is another story.. */
					if (cepp->ce_vardata && cepp->ce_entries)
					{
						config_error("%s:%i: blacklist::dns::reply must be either using format 'reply 1;' or "
									 "'reply { 1; 2; 4; }; but not both formats at the same time.",
									 cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum);
						errors++;
						continue;
					}
					if (cepp->ce_vardata)
					{
						if (atoi(cepp->ce_vardata) <= 0)
						{
							config_error("%s:%i: blacklist::dns::reply must be >0",
								cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum);
							errors++;
							continue;
						}
					}
					if (cepp->ce_entries)
					{
						for (ceppp = cepp->ce_entries; ceppp; ceppp=ceppp->ce_next)
						{
							if (atoi(ceppp->ce_varname) <= 0)
							{
								config_error("%s:%i: all items in blacklist::dns::reply must be >0",
									cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum);
								errors++;
							}
						}
					}
				} else
				if (!cepp->ce_vardata)
				{
					config_error_empty(cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum,
						"blacklist::dns", cepp->ce_varname);
					errors++;
					continue;
				} else
				if (!strcmp(cepp->ce_varname, "name"))
				{
					if (has_dns_name)
					{
						config_warn_duplicate(cepp->ce_fileptr->cf_filename,
							cepp->ce_varlinenum, "blacklist::dns::name");
					}
					has_dns_name = 1;
				}
				if (!strcmp(cepp->ce_varname, "type"))
				{
					if (has_dns_type)
					{
						config_warn_duplicate(cepp->ce_fileptr->cf_filename,
							cepp->ce_varlinenum, "blacklist::dns::type");
					}
					has_dns_type = 1;
					if (!strcmp(cepp->ce_vardata, "record"))
						blacklist_type = DNSBL_RECORD;
					else if (!strcmp(cepp->ce_vardata, "bitmask"))
						blacklist_type = DNSBL_BITMASK;
					else
					{
						config_error("%s:%i: unknown blacklist::dns::type '%s', must be either 'record' or 'bitmask'",
							cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum, cepp->ce_vardata);
						errors++;
					}
				}
			}
		} else
		if (!cep->ce_vardata)
		{
			config_error_empty(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"blacklist", cep->ce_varname);
			errors++;
			continue;
		}
		else if (!strcmp(cep->ce_varname, "action"))
		{
			if (has_action)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "blacklist::action");
				continue;
			}
			has_action = 1;
			if (!banact_stringtoval(cep->ce_vardata))
			{
				config_error("%s:%i: blacklist::action has unknown action type '%s'",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_vardata);
				errors++;
			}
		}
		else if (!strcmp(cep->ce_varname, "ban-time"))
		{
			if (has_ban_time)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "blacklist::ban-time");
				continue;
			}
			has_ban_time = 1;
		} else
		if (!strcmp(cep->ce_varname, "reason"))
		{
			if (has_reason)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "blacklist::reason");
				continue;
			}
			has_reason = 1;
		}
		else
		{
			config_error_unknown(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"blacklist", cep->ce_varname);
			errors++;
		}
	}

	if (!has_action)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"blacklist::action");
		errors++;
	}

	if (!has_reason)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"blacklist::reason");
		errors++;
	}

	if (!has_dns_name)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"blacklist::dns::name");
		errors++;
	}

	if (!has_dns_type)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"blacklist::dns::type");
		errors++;
	}

	if (!has_dns_reply)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"blacklist::dns::reply");
		errors++;
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int blacklist_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep, *cepp, *ceppp;
	Blacklist *d = NULL;
	
	if (type != CONFIG_MAIN)
		return 0;
	
	if (!ce || !ce->ce_varname || strcmp(ce->ce_varname, "blacklist"))
		return 0; /* not interested */

	d = MyMallocEx(sizeof(Blacklist));
	d->name = strdup(ce->ce_vardata);
	/* set some defaults. TODO: use set::blacklist or something ? */
	d->action = BAN_ACT_KILL;
	d->reason = strdup("Your IP is on a DNS Blacklist");
	d->ban_time = 3600;
	
	/* assume dns for now ;) */
	d->backend_type = BLACKLIST_BACKEND_DNS;
	d->backend = MyMallocEx(sizeof(BlacklistBackend));
	d->backend->dns = MyMallocEx(sizeof(DNSBL));

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "dns"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "reply"))
				{
					if (cepp->ce_vardata)
					{
						/* single reply */
						d->backend->dns->reply = MyMallocEx(sizeof(int)*2);
						d->backend->dns->reply[0] = atoi(cepp->ce_vardata);
						d->backend->dns->reply[1] = 0;
					} else
					if (cepp->ce_entries)
					{
						/* (potentially) multiple reply values */
						int cnt = 0;
						for (ceppp = cepp->ce_entries; ceppp; ceppp = ceppp->ce_next)
						{
							if (ceppp->ce_varname)
								cnt++;
						}
						
						if (cnt == 0)
							abort(); /* impossible */
						
						d->backend->dns->reply = MyMallocEx(sizeof(int)*(cnt+1));
						
						cnt = 0;
						for (ceppp = cepp->ce_entries; ceppp; ceppp = ceppp->ce_next)
						{
							d->backend->dns->reply[cnt++] = atoi(ceppp->ce_varname);
						}
						d->backend->dns->reply[cnt] = 0;
					}
				} else
				if (!strcmp(cepp->ce_varname, "type"))
				{
					if (!strcmp(cepp->ce_vardata, "record"))
						d->backend->dns->type = DNSBL_RECORD;
					else if (!strcmp(cepp->ce_vardata, "bitmask"))
						d->backend->dns->type = DNSBL_BITMASK;
				} else
				if (!strcmp(cepp->ce_varname, "name"))
				{
					safestrdup(d->backend->dns->name, cepp->ce_vardata);
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "action"))
		{
			d->action = banact_stringtoval(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "reason"))
		{
			safestrdup(d->reason, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "ban-time"))
		{
			d->ban_time = config_checkval(cep->ce_vardata, CFG_TIME);
		}
	}

	AddListItem(d, conf_blacklist);
	
	return 0;
}

void blacklist_md_free(ModData *md)
{
	/* we have nothing to free actually, but we must set to zero */
	md->l = 0;
}

int blacklist_handshake(aClient *cptr)
{
	blacklist_start_check(cptr);
	return 0;
}

int blacklist_start_check(aClient *cptr)
{
	Blacklist *bl;
	
	if (!BLUSER(cptr))
	{
		SetBLUser(cptr, MyMallocEx(sizeof(BLUser)));
		BLUSER(cptr)->cptr = cptr;
	}
#ifdef DEBUGMODE
	else {
		abort(); /* hmmm. unless we add some /Blacklist CHECK command. then this needs to be removed */
	}
#endif
	for (bl = conf_blacklist; bl; bl = bl->next)
		if (bl->backend_type == BLACKLIST_BACKEND_DNS)
			blacklist_dns_request(cptr, bl);
	
	/* Free bluser entry. This only happens if you have no blacklist configured or they fail very early */
	if (BLUSER(cptr))
		blacklist_free_bluser_if_able(BLUSER(cptr));

	return 0;
}

int blacklist_dns_request(aClient *cptr, Blacklist *d)
{
	char buf[256];
	int e[4];
	char *ip = GetIP(cptr);
	
	if (!ip || !strchr(ip, '.'))
		return 0;
	
	memset(&e, 0, sizeof(e));
	if (sscanf(ip, "%d.%d.%d.%d", &e[0], &e[1], &e[2], &e[3]) != 4)
		return 0;
	
	snprintf(buf, sizeof(buf), "%d.%d.%d.%d.%s", e[3], e[2], e[1], e[0], d->backend->dns->name);

	BLUSER(cptr)->refcnt++; /* one (more) blacklist result remaining */
	
	unreal_gethostbyname(buf, AF_INET, blacklist_resolver_callback, BLUSER(cptr));
	
	return 0;
}

void blacklist_cancel(BLUser *bl)
{
	bl->cptr = NULL;
}

int blacklist_quit(aClient *cptr, char *comment)
{
	if (BLUSER(cptr))
		blacklist_cancel(BLUSER(cptr));

	return 0;
}

void blacklist_free_bluser_if_able(BLUser *bl)
{
	if (bl->refcnt > 0)
		return; /* unable, still have DNS requests/replies in-flight */

	if (bl->cptr)
		SetBLUser(bl->cptr, NULL);
	
	MyFree(bl);
}

char *getdnsblname(char *p)
{
int dots = 0;

	for (; *p; p++)
		if (*p == '.')
		{
			dots++;
			if (dots == 4)
				return p+1;
		}
	return NULL;
}

/* Parse DNS reply.
 * A reply will be an A record in the format x.x.x.<reply>
 */
int blacklist_parse_reply(struct hostent *he)
{
	char ipbuf[64];
	char *p;

	*ipbuf = '\0';
	if (!inet_ntop(AF_INET, he->h_addr_list[0], ipbuf, sizeof(ipbuf)))
		return 0;
	
	p = strrchr(ipbuf, '.');
	if (!p)
		return 0;
	
	return atoi(p+1);
}

void blacklist_hit(aClient *acptr, Blacklist *bl, int reply)
{
	char buf[512];
	char *name[4], *value[4];

	if (find_tkline_match(acptr, 0) < 0)
		return; /* already klined/glined. Don't send the warning from below. */

	if (IsPerson(acptr))
		snprintf(buf, sizeof(buf), "[Blacklist] IP %s (%s) matches blacklist %s (%s/reply=%d)",
			GetIP(acptr), acptr->name, bl->name, bl->backend->dns->name, reply);
	else
		snprintf(buf, sizeof(buf), "[Blacklist] IP %s matches blacklist %s (%s/reply=%d)",
			GetIP(acptr), bl->name, bl->backend->dns->name, reply);
	
	sendto_snomask(SNO_BLACKLIST, "%s", buf);
	ircd_log(LOG_KILL, "%s", buf);

	name[0] = "ip";
	value[0] = GetIP(acptr);
	name[1] = "server";
	value[1] = me.name;
	name[2] = NULL;
	value[2] = NULL;

	buildvarstring(bl->reason, buf, sizeof(buf), name, value);
	
	place_host_ban(acptr, bl->action, buf, bl->ban_time);
}

void blacklist_process_result(aClient *acptr, int status, struct hostent *he)
{
	Blacklist *bl;
	char *domain;
	int reply;
	int i;
	
	if ((status != 0) || (he->h_length != 4) || !he->h_name)
		return; /* invalid reply */
	
	domain = getdnsblname(he->h_name);
	if (!domain)
		return; /* odd */
	bl = blacklist_find_block_by_dns(domain);
	if (!bl)
		return; /* possibly just rehashed and the blacklist block is gone now */
	
	reply = blacklist_parse_reply(he);
	
	for (i = 0; bl->backend->dns->reply[i]; i++)
	{
		if ((bl->backend->dns->reply[i] == -1) ||
		    ( (bl->backend->dns->type == DNSBL_BITMASK) && (reply & bl->backend->dns->reply[i]) ) ||
		    ( (bl->backend->dns->type == DNSBL_RECORD) && (bl->backend->dns->reply[i] == reply) ) )
		{
			blacklist_hit(acptr, bl, reply);
			return;
		}
	}
}

void blacklist_resolver_callback(void *arg, int status, int timeouts, struct hostent *he)
{
	BLUser *blu = (BLUser *)arg;
	aClient *acptr = blu->cptr;
	
	blu->refcnt--; /* one less outstanding DNS request remaining */
	blacklist_free_bluser_if_able(blu);
	blu = NULL;

	if (!acptr)
		return; /* Client left already */

	blacklist_process_result(acptr, status, he);
}
