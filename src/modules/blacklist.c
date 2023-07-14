/*
 * Blacklist support (currently only DNS Blacklists)
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
#include "dns.h"

ModuleHeader MOD_HEADER
= {
	"blacklist",
	"5.0",
	"Check connecting users against DNS Blacklists",
	"UnrealIRCd Team",
	"unrealircd-6",
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

typedef struct DNSBL DNSBL;
struct DNSBL {
	char *name;
	DNSBLType type;
	int *reply;
};

typedef union BlacklistBackend BlacklistBackend;
union BlacklistBackend
{
	DNSBL *dns;
};

typedef enum {
	BLACKLIST_BACKEND_DNS = 1
} BlacklistBackendType;

typedef struct Blacklist Blacklist;
struct Blacklist {
	Blacklist *prev, *next;
	char *name;
	BlacklistBackendType backend_type;
	BlacklistBackend *backend;
	BanAction *action;
	long ban_time;
	char *reason;
	SecurityGroup *except;
};

/* Blacklist user struct. In the c-ares DNS reply callback we need to pass
 * some metadata. We can't use client directly there as the client may
 * be gone already by the time we receive the DNS reply.
 */
typedef struct BLUser BLUser;
struct BLUser {
	Client *client;
	int is_ipv6;
	int refcnt;
	/* The following save_* fields are used by softbans: */
	BanAction *save_action;
	long save_tkltime;
	char *save_opernotice;
	char *save_reason;
	char *save_blacklist;
	char *save_blacklist_dns_name;
	int save_blacklist_dns_reply;
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
int blacklist_handshake(Client *client);
int blacklist_ip_change(Client *client, const char *oldip);
int blacklist_quit(Client *client, MessageTag *mtags, const char *comment);
int blacklist_preconnect(Client *client);
void blacklist_resolver_callback(void *arg, int status, int timeouts, struct hostent *he);
int blacklist_start_check(Client *client);
int blacklist_dns_request(Client *client, Blacklist *bl);
int blacklist_rehash(void);
int blacklist_rehash_complete(void);
void blacklist_set_handshake_delay(void);
void blacklist_free_bluser_if_able(BLUser *bl);

#define SetBLUser(x, y)	do { moddata_client(x, blacklist_md).ptr = y; } while(0)
#define BLUSER(x)	((BLUser *)moddata_client(x, blacklist_md).ptr)

MOD_TEST()
{
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, blacklist_config_test);

	CallbackAdd(modinfo->handle, CALLBACKTYPE_BLACKLIST_CHECK, blacklist_start_check);
	return MOD_SUCCESS;
}

/** Called upon module init */
MOD_INIT()
{
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);
	/* This module needs to be permanent.
	 * Not because of UnrealIRCd restrictions,
	 * but because we use c-ares callbacks and the address
	 * of those functions will change if we REHASH.
	 */
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
	HookAdd(modinfo->handle, HOOKTYPE_IP_CHANGE, 0, blacklist_ip_change);
	HookAdd(modinfo->handle, HOOKTYPE_PRE_LOCAL_CONNECT, 0, blacklist_preconnect);
	HookAdd(modinfo->handle, HOOKTYPE_REHASH, 0, blacklist_rehash);
	HookAdd(modinfo->handle, HOOKTYPE_REHASH_COMPLETE, 0, blacklist_rehash_complete);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_QUIT, 0, blacklist_quit);

	return MOD_SUCCESS;
}

/** Called upon module load */
MOD_LOAD()
{
	blacklist_set_handshake_delay();
	return MOD_SUCCESS;
}

/** Called upon unload */
MOD_UNLOAD()
{
	blacklist_free_conf();
	return MOD_SUCCESS;
}

int blacklist_rehash(void)
{
	blacklist_free_conf();
	return 0;
}

int blacklist_rehash_complete(void)
{
	blacklist_set_handshake_delay();
	return 0;
}

void blacklist_set_handshake_delay(void)
{
	if ((iConf.handshake_delay == -1) && conf_blacklist)
	{
		/*
		Too noisy?
		config_status("[blacklist] I'm setting set::handshake-delay to 2 seconds. "
		              "You may wish to set an explicit setting in the configuration file.");
		config_status("See https://www.unrealircd.org/docs/Set_block#set::handshake-delay");
		*/
		iConf.handshake_delay = 2;
	}
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
			safe_free(e->backend->dns->name);
			safe_free(e->backend->dns->reply);
			safe_free(e->backend->dns);
		}
	}
	
	safe_free(e->backend);

	safe_free(e->name);
	safe_free(e->reason);
	safe_free_all_ban_actions(e->action);
	free_security_group(e->except);

	safe_free(e);
}

int blacklist_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	ConfigEntry	*cep, *cepp, *ceppp;
	int errors = 0;
	char has_reason = 0, has_ban_time = 0, has_action = 0;
	char has_dns_type = 0, has_dns_reply = 0, has_dns_name = 0;

	if (type != CONFIG_MAIN)
		return 0;
	
	if (!ce)
		return 0;
	
	if (strcmp(ce->name, "blacklist"))
		return 0; /* not interested in non-blacklist stuff.. */
	
	if (!ce->value)
	{
		config_error("%s:%i: blacklist block without name (use: blacklist somename { })",
			ce->file->filename, ce->line_number);
		*errs = 1;
		return -1;
	}

	/* Now actually go parse the blacklist { } block */
	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "dns"))
		{
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				if (!strcmp(cepp->name, "reply"))
				{
					if (has_dns_reply)
					{
						/* this is an error (not a warning) */
						config_error("%s:%i: blacklist block may contain only one blacklist::dns::reply item. "
									 "You can specify multiple replies by using: reply { 1; 2; 4; };",
									 cepp->file->filename, cepp->line_number);
						errors++;
						continue;
					}
					if (!cepp->value && !cepp->items)
					{
						config_error_blank(cepp->file->filename, cepp->line_number, "blacklist::dns::reply");
						errors++;
						continue;
					}
					has_dns_reply = 1; /* we have a reply. now whether it's actually valid is another story.. */
					if (cepp->value && cepp->items)
					{
						config_error("%s:%i: blacklist::dns::reply must be either using format 'reply 1;' or "
									 "'reply { 1; 2; 4; }; but not both formats at the same time.",
									 cepp->file->filename, cepp->line_number);
						errors++;
						continue;
					}
					if (cepp->value)
					{
						if (atoi(cepp->value) <= 0)
						{
							config_error("%s:%i: blacklist::dns::reply must be >0",
								cepp->file->filename, cepp->line_number);
							errors++;
							continue;
						}
					}
					if (cepp->items)
					{
						for (ceppp = cepp->items; ceppp; ceppp=ceppp->next)
						{
							if (atoi(ceppp->name) <= 0)
							{
								config_error("%s:%i: all items in blacklist::dns::reply must be >0",
									cepp->file->filename, cepp->line_number);
								errors++;
							}
						}
					}
				} else
				if (!cepp->value)
				{
					config_error_empty(cepp->file->filename, cepp->line_number,
						"blacklist::dns", cepp->name);
					errors++;
					continue;
				} else
				if (!strcmp(cepp->name, "name"))
				{
					if (has_dns_name)
					{
						config_warn_duplicate(cepp->file->filename,
							cepp->line_number, "blacklist::dns::name");
					}
					has_dns_name = 1;
				} else
				if (!strcmp(cepp->name, "type"))
				{
					if (has_dns_type)
					{
						config_warn_duplicate(cepp->file->filename,
							cepp->line_number, "blacklist::dns::type");
					}
					has_dns_type = 1;
					if (!strcmp(cepp->value, "record"))
						;
					else if (!strcmp(cepp->value, "bitmask"))
						;
					else
					{
						config_error("%s:%i: unknown blacklist::dns::type '%s', must be either 'record' or 'bitmask'",
							cepp->file->filename, cepp->line_number, cepp->value);
						errors++;
					}
				}
			}
		} else
		if (!strcmp(cep->name, "except"))
		{
			test_match_block(cf, cep, &errors);
		} else
		if (!cep->value)
		{
			config_error_empty(cep->file->filename, cep->line_number,
				"blacklist", cep->name);
			errors++;
			continue;
		}
		else if (!strcmp(cep->name, "action"))
		{
			if (has_action)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "blacklist::action");
				continue;
			}
			has_action = 1;
			errors += test_ban_action_config(cep);
		}
		else if (!strcmp(cep->name, "ban-time"))
		{
			if (has_ban_time)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "blacklist::ban-time");
				continue;
			}
			has_ban_time = 1;
		}
		else if (!strcmp(cep->name, "reason"))
		{
			if (has_reason)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "blacklist::reason");
				continue;
			}
			has_reason = 1;
		}
		else
		{
			config_error_unknown(cep->file->filename, cep->line_number,
				"blacklist", cep->name);
			errors++;
		}
	}

	if (!has_action)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"blacklist::action");
		errors++;
	}

	if (!has_reason)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"blacklist::reason");
		errors++;
	}

	if (!has_dns_name)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"blacklist::dns::name");
		errors++;
	}

	if (!has_dns_type)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"blacklist::dns::type");
		errors++;
	}

	if (!has_dns_reply)
	{
		config_error_missing(ce->file->filename, ce->line_number,
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
	
	if (!ce || !ce->name || strcmp(ce->name, "blacklist"))
		return 0; /* not interested */

	d = safe_alloc(sizeof(Blacklist));
	safe_strdup(d->name, ce->value);
	/* set some defaults */
	d->ban_time = 3600;
	
	/* assume dns for now ;) */
	d->backend_type = BLACKLIST_BACKEND_DNS;
	d->backend = safe_alloc(sizeof(BlacklistBackend));
	d->backend->dns = safe_alloc(sizeof(DNSBL));

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "dns"))
		{
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				if (!strcmp(cepp->name, "reply"))
				{
					if (cepp->value)
					{
						/* single reply */
						d->backend->dns->reply = safe_alloc(sizeof(int)*2);
						d->backend->dns->reply[0] = atoi(cepp->value);
						d->backend->dns->reply[1] = 0;
					} else
					if (cepp->items)
					{
						/* (potentially) multiple reply values */
						int cnt = 0;
						for (ceppp = cepp->items; ceppp; ceppp = ceppp->next)
						{
							if (ceppp->name)
								cnt++;
						}
						
						if (cnt == 0)
							abort(); /* impossible */
						
						d->backend->dns->reply = safe_alloc(sizeof(int)*(cnt+1));
						
						cnt = 0;
						for (ceppp = cepp->items; ceppp; ceppp = ceppp->next)
						{
							d->backend->dns->reply[cnt++] = atoi(ceppp->name);
						}
						d->backend->dns->reply[cnt] = 0;
					}
				} else
				if (!strcmp(cepp->name, "type"))
				{
					if (!strcmp(cepp->value, "record"))
						d->backend->dns->type = DNSBL_RECORD;
					else if (!strcmp(cepp->value, "bitmask"))
						d->backend->dns->type = DNSBL_BITMASK;
				} else
				if (!strcmp(cepp->name, "name"))
				{
					safe_strdup(d->backend->dns->name, cepp->value);
				}
			}
		}
		else if (!strcmp(cep->name, "action"))
		{
			parse_ban_action_config(cep, &d->action);
		}
		else if (!strcmp(cep->name, "ban-time"))
		{
			d->ban_time = config_checkval(cep->value, CFG_TIME);
		}
		else if (!strcmp(cep->name, "reason"))
		{
			safe_strdup(d->reason, cep->value);
		}
		else if (!strcmp(cep->name, "except"))
		{
			conf_match_block(cf, cep, &d->except);
		}
	}

	AddListItem(d, conf_blacklist);
	
	return 0;
}

void blacklist_md_free(ModData *md)
{
	BLUser *bl = md->ptr;

	/* Mark bl->client as dead. Free the struct, if able. */
	blacklist_free_bluser_if_able(bl);

	md->ptr = NULL;
}

int blacklist_handshake(Client *client)
{
	blacklist_start_check(client);
	return 0;
}

int blacklist_ip_change(Client *client, const char *oldip)
{
	blacklist_start_check(client);
	return 0;
}

int blacklist_start_check(Client *client)
{
	Blacklist *bl;

	if (find_tkl_exception(TKL_BLACKLIST, client))
	{
		/* If the user is exempt from DNSBL checking then:
		 * 1) Don't bother checking DNSBL's
		 * 2) Disable handshake delay for this user, since it serves no purpose.
		 */
		SetNoHandshakeDelay(client);
		return 0;
	}

	if (!BLUSER(client))
	{
		SetBLUser(client, safe_alloc(sizeof(BLUser)));
		BLUSER(client)->client = client;
	}

	for (bl = conf_blacklist; bl; bl = bl->next)
	{
		/* Stop processing if client is (being) killed already */
		if (!BLUSER(client))
			break;

		/* Check if user is exempt (then don't bother checking) */
		if (user_allowed_by_security_group(client, bl->except))
			continue;

		/* Initiate blacklist requests */
		if (bl->backend_type == BLACKLIST_BACKEND_DNS)
			blacklist_dns_request(client, bl);
	}
	
	return 0;
}

int blacklist_dns_request(Client *client, Blacklist *d)
{
	char buf[256], wbuf[128];
	unsigned int e[8];
	char *ip = GetIP(client);
	
	if (!ip)
		return 0;

	memset(&e, 0, sizeof(e));

	if (strchr(ip, '.'))
	{
		/* IPv4 */
		if (sscanf(ip, "%u.%u.%u.%u", &e[0], &e[1], &e[2], &e[3]) != 4)
			return 0;
	
		snprintf(buf, sizeof(buf), "%u.%u.%u.%u.%s", e[3], e[2], e[1], e[0], d->backend->dns->name);
	} else
	if (strchr(ip, ':'))
	{
		/* IPv6 */
		int i;
		BLUSER(client)->is_ipv6 = 1;
		if (sscanf(ip, "%x:%x:%x:%x:%x:%x:%x:%x",
		    &e[0], &e[1], &e[2], &e[3], &e[4], &e[5], &e[6], &e[7]) != 8)
		{
			return 0;
		}
		*buf = '\0';
		for (i = 7; i >= 0; i--)
		{
			snprintf(wbuf, sizeof(wbuf), "%x.%x.%x.%x.",
				(unsigned int)(e[i] & 0xf),
				(unsigned int)((e[i] >> 4) & 0xf),
				(unsigned int)((e[i] >> 8) & 0xf),
				(unsigned int)((e[i] >> 12) & 0xf));
			strlcat(buf, wbuf, sizeof(buf));
		}
		strlcat(buf, d->backend->dns->name, sizeof(buf));
	}
	else
		return 0; /* unknown IP format */

	BLUSER(client)->refcnt++; /* one (more) blacklist result remaining */
	
	unreal_gethostbyname(buf, AF_INET, blacklist_resolver_callback, BLUSER(client));
	
	return 0;
}

void blacklist_cancel(BLUser *bl)
{
	bl->client = NULL;
}

int blacklist_quit(Client *client, MessageTag *mtags, const char *comment)
{
	if (BLUSER(client))
		blacklist_cancel(BLUSER(client));

	return 0;
}

/** Free the BLUSER() struct, if we are able to do so.
 * This should only be called if the underlying client is dead or dyeing
 * and not earlier.
 * Reasons why we 'are not able' are: refcnt is non-zero, that is:
 * there is still an outstanding resolver request (eg: slow blacklist).
 * In that case, no worries, we will be called again after that request
 * is finished.
 */
void blacklist_free_bluser_if_able(BLUser *bl)
{
	if (bl->client)
		bl->client = NULL;

	if (bl->refcnt > 0)
		return; /* unable, still have DNS requests/replies in-flight */

	safe_free(bl->save_opernotice);
	safe_free(bl->save_reason);
	free_all_ban_actions(bl->save_action);
	safe_free(bl);
}

char *getdnsblname(char *p, Client *client)
{
	int dots = 0;
	int dots_count;

	if (!client)
		return NULL;

	if (BLUSER(client)->is_ipv6)
		dots_count = 32;
	else
		dots_count = 4;

	for (; *p; p++)
	{
		if (*p == '.')
		{
			dots++;
			if (dots == dots_count)
				return p+1;
		}
	}
	return NULL;
}

/* Parse DNS reply.
 * A reply will be an A record in the format x.x.x.<reply>
 */
int blacklist_parse_reply(struct hostent *he, int entry)
{
	char ipbuf[64];
	char *p;

	if ((he->h_addrtype != AF_INET) || (he->h_length != 4))
		return 0;

	*ipbuf = '\0';
	if (!inet_ntop(AF_INET, he->h_addr_list[entry], ipbuf, sizeof(ipbuf)))
		return 0;
	
	p = strrchr(ipbuf, '.');
	if (!p)
		return 0;
	
	return atoi(p+1);
}

/** Take the actual ban action.
 * Called from blacklist_hit() and for immediate bans and
 * from blacklist_preconnect() for softbans that need to be delayed
 * as to give the user the opportunity to do SASL Authentication.
 */
int blacklist_action(Client *client, char *opernotice, BanAction *ban_action, char *ban_reason, long ban_time,
                     char *blacklist, char *blacklist_dns_name, int blacklist_dns_reply)
{
	unreal_log_raw(ULOG_INFO, "blacklist", "BLACKLIST_HIT", client,
	               opernotice,
	               log_data_string("blacklist_name", blacklist),
	               log_data_string("blacklist_dns_name", blacklist_dns_name),
	               log_data_integer("blacklist_dns_reply", blacklist_dns_reply),
	               log_data_string("ban_action", ban_actions_to_string(ban_action)),
	               log_data_string("ban_reason", ban_reason),
	               log_data_integer("ban_time", ban_time));
	return take_action(client, ban_action, ban_reason, ban_time, 0, NULL);
}

void blacklist_hit(Client *client, Blacklist *bl, int reply)
{
	char opernotice[512], banbuf[512], reply_num[5];
	const char *name[6], *value[6];
	BLUser *blu = BLUSER(client);

	if (find_tkline_match(client, 1))
		return; /* already klined/glined. Don't send the warning from below. */

	if (IsUser(client))
		snprintf(opernotice, sizeof(opernotice), "[Blacklist] IP %s (%s) matches blacklist %s (%s/reply=%d)",
			GetIP(client), client->name, bl->name, bl->backend->dns->name, reply);
	else
		snprintf(opernotice, sizeof(opernotice), "[Blacklist] IP %s matches blacklist %s (%s/reply=%d)",
			GetIP(client), bl->name, bl->backend->dns->name, reply);

	snprintf(reply_num, sizeof(reply_num), "%d", reply);

	name[0] = "ip";
	value[0] = GetIP(client);
	name[1] = "server";
	value[1] = me.name;
	name[2] = "blacklist";
	value[2] = bl->name;
	name[3] = "dnsname";
	value[3] = bl->backend->dns->name;
	name[4] = "dnsreply";
	value[4] = reply_num;
	name[5] = NULL;
	value[5] = NULL;
	/* when adding more, be sure to update the array elements number in the definition of const char *name[] and value[] */

	buildvarstring(bl->reason, banbuf, sizeof(banbuf), name, value);

	if (only_soft_actions(bl->action) && blu)
	{
		/* For soft bans, delay the action until later (so user can do SASL auth) */
		blu->save_action = duplicate_ban_actions(bl->action);
		blu->save_tkltime = bl->ban_time;
		safe_strdup(blu->save_opernotice, opernotice);
		safe_strdup(blu->save_reason, banbuf);
		safe_strdup(blu->save_blacklist, bl->name);
		safe_strdup(blu->save_blacklist_dns_name, bl->backend->dns->name);
		blu->save_blacklist_dns_reply = reply;
	} else {
		/* Otherwise, execute the action immediately */
		blacklist_action(client, opernotice, bl->action, banbuf, bl->ban_time, bl->name, bl->backend->dns->name, reply);
	}
}

void blacklist_process_result(Client *client, int status, struct hostent *he)
{
	Blacklist *bl;
	char *domain;
	int reply;
	int i;
	int replycnt;
	
	if ((status != 0) || (he->h_length != 4) || !he->h_name)
		return; /* invalid reply */
	
	domain = getdnsblname(he->h_name, client);
	if (!domain)
		return; /* odd */
	bl = blacklist_find_block_by_dns(domain);
	if (!bl)
		return; /* possibly just rehashed and the blacklist block is gone now */
	
	/* walk through all replies for this record... until we have a hit */
	for (replycnt=0; he->h_addr_list[replycnt]; replycnt++)
	{
		reply = blacklist_parse_reply(he, replycnt);

		for (i = 0; bl->backend->dns->reply[i]; i++)
		{
			if ((bl->backend->dns->reply[i] == -1) ||
				( (bl->backend->dns->type == DNSBL_BITMASK) && (reply & bl->backend->dns->reply[i]) ) ||
				( (bl->backend->dns->type == DNSBL_RECORD) && (bl->backend->dns->reply[i] == reply) ) )
			{
				blacklist_hit(client, bl, reply);
				return;
			}
		}
	}
}

void blacklist_resolver_callback(void *arg, int status, int timeouts, struct hostent *he)
{
	BLUser *blu = (BLUser *)arg;
	Client *client = blu->client;

	blu->refcnt--; /* one less outstanding DNS request remaining */

	/* If we are the last to resolve something and the client is gone
	 * already then free the struct.
	 */
	if ((blu->refcnt == 0) && !client)
		blacklist_free_bluser_if_able(blu);

	blu = NULL;

	if (!client)
		return; /* Client left already */
	/* ^^ note: do not merge this with the other 'if' a few lines up (refcnt!) */

	blacklist_process_result(client, status, he);
}

int blacklist_preconnect(Client *client)
{
	BLUser *blu = BLUSER(client);

	if (!blu || !blu->save_action)
		return HOOK_CONTINUE;

	/* There was a pending softban... has the user authenticated via SASL by now? */
	if (IsLoggedIn(client))
		return HOOK_CONTINUE; /* yup, so the softban does not apply. */

	if (blacklist_action(client, blu->save_opernotice, blu->save_action, blu->save_reason, blu->save_tkltime,
	                     blu->save_blacklist, blu->save_blacklist_dns_name, blu->save_blacklist_dns_reply) > 0)
	{
		return HOOK_DENY;
	}
	return HOOK_CONTINUE; /* exempt */
}
