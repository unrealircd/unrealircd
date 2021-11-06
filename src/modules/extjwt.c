/*
 *   IRC - Internet Relay Chat, src/modules/extjwt.c
 *   (C) 2021 The UnrealIRCd Team
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

#include "unrealircd.h"

#if defined(__GNUC__)
/* Temporarily ignore these for this entire file. FIXME later when updating the code for OpenSSL 3: */
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

/* internal definitions */

#define MSG_EXTJWT	"EXTJWT"
#define MYCONF "extjwt"

#undef NEW_ISUPPORT /* enable this for https://github.com/ircv3/ircv3-specifications/pull/341#issuecomment-617038799 */

#define EXTJWT_METHOD_NOT_SET 0
#define EXTJWT_METHOD_HS256 1
#define EXTJWT_METHOD_HS384 2
#define EXTJWT_METHOD_HS512 3
#define EXTJWT_METHOD_RS256 4
#define EXTJWT_METHOD_RS384 5
#define EXTJWT_METHOD_RS512 6
#define EXTJWT_METHOD_ES256 7
#define EXTJWT_METHOD_ES384 8
#define EXTJWT_METHOD_ES512 9
#define EXTJWT_METHOD_NONE 10

#define NEEDS_KEY(x) (x>=EXTJWT_METHOD_RS256 && x<=EXTJWT_METHOD_ES512)

#define URL_LENGTH 4096
#define MODES_SIZE 41 /* about 10 mode chars */
#define TS_LENGTH 19 /* 64-bit integer */
#define MAX_TOKEN_CHUNK (510-sizeof(extjwt_message_pattern)-HOSTLEN-CHANNELLEN)

/* OpenSSL 1.0.x compatibility */

#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
void ECDSA_SIG_get0(const ECDSA_SIG *sig, const BIGNUM **pr, const BIGNUM **ps)
{
	if (pr != NULL)
		*pr = sig->r;
	if (ps != NULL)
		*ps = sig->s;
}
#endif

/* struct definitions */

struct extjwt_config {
	time_t exp_delay;
	char *secret;
	int method;
	char *vfy;
};

struct jwt_service {
	char *name;
	struct extjwt_config *cfg;
	struct jwt_service *next;
};

/* function declarations */

CMD_FUNC(cmd_extjwt);
char *extjwt_make_payload(Client *client, Channel *channel, struct extjwt_config *config);
char *extjwt_generate_token(const char *payload, struct extjwt_config *config);
void b64url(char *b64);
unsigned char *extjwt_hmac_extjwt_hash(int method, const void *key, int keylen, const unsigned char *data, int datalen, unsigned int* resultlen);
unsigned char *extjwt_sha_pem_extjwt_hash(int method, const void *key, int keylen, const unsigned char *data, int datalen, unsigned int* resultlen);
unsigned char *extjwt_hash(int method, const void *key, int keylen, const unsigned char *data, int datalen, unsigned int* resultlen);
char *extjwt_gen_header(int method);
int extjwt_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int extjwt_configrun(ConfigFile *cf, ConfigEntry *ce, int type);
int extjwt_configposttest(int *errs);
void extjwt_free_services(struct jwt_service **services);
struct jwt_service *find_jwt_service(struct jwt_service *services, const char *name);
int extjwt_valid_integer_string(const char *in, int min, int max);
char *extjwt_test_key(const char *file, int method);
char *extjwt_read_file_contents(const char *file, int absolute, int *size);
int EXTJWT_METHOD_from_string(const char *in);
#ifdef NEW_ISUPPORT
char *extjwt_isupport_param(void);
#endif

/* string constants */

const char extjwt_message_pattern[] = ":%s EXTJWT %s %s %s%s";

/* global structs */

ModuleHeader MOD_HEADER = {
	"extjwt",
	"6.0",
	"Command /EXTJWT (web service authorization)", 
	"UnrealIRCd Team",
	"unrealircd-6"
};

struct {
	int have_secret;
	int have_key;
	int have_method;
	int have_expire;
	int have_vfy;
	char *key_filename;
} cfg_state;

struct extjwt_config cfg;
struct jwt_service *jwt_services;

MOD_TEST()
{
	memset(&cfg_state, 0, sizeof(cfg_state));
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, extjwt_configtest);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, extjwt_configposttest);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_EXTJWT, cmd_extjwt, 2, CMD_USER);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, extjwt_configrun);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	struct jwt_service *service = jwt_services;
#ifdef NEW_ISUPPORT
	ISupportAdd(modinfo->handle, "EXTJWT", extjwt_isupport_param());
#else
	ISupportAdd(modinfo->handle, "EXTJWT", "1");
#endif
	while (service)
	{ /* copy default exp to all services not having one specified */
		if (service->cfg->exp_delay == 0)
			service->cfg->exp_delay = cfg.exp_delay;
		service = service->next;
	}
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	extjwt_free_services(&jwt_services);
	return MOD_SUCCESS;
}

#ifdef NEW_ISUPPORT
char *extjwt_isupport_param(void)
{
	struct jwt_service *services = jwt_services;
	int count = 0;
	static char buf[500];
	strlcpy(buf, "V:1", sizeof(buf));
	while (services)
	{
		strlcat(buf, count?",":"&S:", sizeof(buf));
		strlcat(buf, services->name, sizeof(buf));
		count++;
		services = services->next;
	}
	return buf;
}
#endif

void extjwt_free_services(struct jwt_service **services){
	struct jwt_service *ss, *next;
	ss = *services;
	while (ss)
	{
		next = ss->next;
		safe_free(ss->name);
		if (ss->cfg)
			safe_free(ss->cfg->secret);
		safe_free(ss->cfg);
		safe_free(ss);
		ss = next;
	}
	*services = NULL;
}

struct jwt_service *find_jwt_service(struct jwt_service *services, const char *name)
{
	if (!name)
		return NULL;
	while (services)
	{
		if (services->name && !strcmp(services->name, name))
			return services;
		services = services->next;
	}
	return NULL;
}

int extjwt_valid_integer_string(const char *in, int min, int max)
{
	int i, val;
	if (BadPtr(in))
		return 0;
	for (i=0; in[i]; i++){
		if (!isdigit(in[i]))
			return 0;
	}
	val = atoi(in);
	if (val < min || val > max)
		return 0;
	return 1;
}

int vfy_url_is_valid(const char *string)
{
	return 1; /* TODO enable */
	if (strstr(string, "http://") == string || strstr(string, "https://") == string)
	{
		if (strstr(string, "%s"))
			return 1;
	}
	return 0;
}

char *extjwt_test_key(const char *file, int method)
{ /* returns NULL when valid */
	int fsize;
	char *fcontent = NULL;
	char *retval = NULL;
	BIO *bufkey = NULL;
	EVP_PKEY *pkey = NULL;
	int type, pkey_type;
	do {
		switch (method)
		{
			case EXTJWT_METHOD_RS256: case EXTJWT_METHOD_RS384: case EXTJWT_METHOD_RS512:
				type = EVP_PKEY_RSA;
				break;
			case EXTJWT_METHOD_ES256: case EXTJWT_METHOD_ES384: case EXTJWT_METHOD_ES512:
				type = EVP_PKEY_EC;
				break;
			default:
				retval = "Internal error (invalid type)";
				return retval;
		}
		fcontent = extjwt_read_file_contents(file, 0, &fsize);
		if (!fcontent)
		{
			retval = "Cannot open file";
			break;
		}
		if (fsize == 0)
		{
			retval = "File is empty";
			break;
		}
		if (!(bufkey = BIO_new_mem_buf(fcontent, fsize)))
		{
			retval = "Unknown error";
			break;
		}
		if (!(pkey = PEM_read_bio_PrivateKey(bufkey, NULL, NULL, NULL)))
		{
			retval = "Key is invalid";
			break;
		}
		pkey_type = EVP_PKEY_id(pkey);
		if (type != pkey_type)
		{
			retval = "Key does not match method";
			break;
		}
	} while (0);
	safe_free(fcontent);
	if (bufkey)
		BIO_free(bufkey);
	if (pkey)
		EVP_PKEY_free(pkey);
	return retval;
}

int EXTJWT_METHOD_from_string(const char *in)
{
	if (!strcmp(in, "HS256"))
		return EXTJWT_METHOD_HS256;
	if (!strcmp(in, "HS384"))
		return EXTJWT_METHOD_HS384;
	if (!strcmp(in, "HS512"))
		return EXTJWT_METHOD_HS512;
	if (!strcmp(in, "RS256"))
		return EXTJWT_METHOD_RS256;
	if (!strcmp(in, "RS384"))
		return EXTJWT_METHOD_RS384;
	if (!strcmp(in, "RS512"))
		return EXTJWT_METHOD_RS512;
	if (!strcmp(in, "ES256"))
		return EXTJWT_METHOD_ES256;
	if (!strcmp(in, "ES384"))
		return EXTJWT_METHOD_ES384;
	if (!strcmp(in, "ES512"))
		return EXTJWT_METHOD_ES512;
	if (!strcmp(in, "NONE"))
		return EXTJWT_METHOD_NONE;
	return EXTJWT_METHOD_NOT_SET;
}

/* Configuration is described in conf/modules.optional.conf */

int extjwt_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep, *cep2;
	int i;
	struct jwt_service *services = NULL;
	struct jwt_service **ss = &services; /* list for checking whether service names repeat */
	int have_ssecret, have_smethod, have_svfy, have_scert;
	unsigned int sfilename_line_number = 0;
	char *sfilename = NULL;

	if (type != CONFIG_MAIN)
		return 0;

	if (!ce || strcmp(ce->name, MYCONF))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!cep->value)
		{
			config_error("%s:%i: blank %s::%s without value", cep->file->filename, cep->line_number, MYCONF, cep->name);
			errors++;
			continue;
		}
		if (!strcmp(cep->name, "method"))
		{
			if (cfg_state.have_method)
			{
				config_error("%s:%i: duplicate %s::%s item", cep->file->filename, cep->line_number, MYCONF, cep->name);
				errors++;
				continue;
			}
			cfg_state.have_method = EXTJWT_METHOD_from_string(cep->value);
			if (cfg_state.have_method == EXTJWT_METHOD_NOT_SET)
			{
				config_error("%s:%i: invalid value %s::%s \"%s\" (check docs for allowed options)", cep->file->filename, cep->line_number, MYCONF, cep->name, cep->value);
				errors++;
			}
			continue;
		}
		if (!strcmp(cep->name, "expire-after"))
		{
			if (!extjwt_valid_integer_string(cep->value, 1, 9999))
			{
				config_error("%s:%i: %s::%s must be an integer between 1 and 9999 (seconds)", cep->file->filename, cep->line_number, MYCONF, cep->name);
				errors++;
			}
			continue;
		}
		if (!strcmp(cep->name, "secret"))
		{
			if (cfg_state.have_secret)
			{
				config_error("%s:%i: duplicate %s::%s item", cep->file->filename, cep->line_number, MYCONF, cep->name);
				errors++;
				continue;
			}
			cfg_state.have_secret = 1;
			if (strlen(cep->value) < 4)
			{
				config_error("%s:%i: Secret specified in %s::%s is too short!", cep->file->filename, cep->line_number, MYCONF, cep->name);
				errors++;
			}
			continue;
		}
		if (!strcmp(cep->name, "key"))
		{
			if (cfg_state.have_key)
			{
				config_error("%s:%i: duplicate %s::%s item", cep->file->filename, cep->line_number, MYCONF, cep->name);
				errors++;
				continue;
			}
			if (!is_file_readable(cep->value, CONFDIR))
			{
				config_error("%s:%i: Cannot open file \"%s\" specified in %s::%s for reading", cep->file->filename, cep->line_number, cep->value, MYCONF, cep->name);
				errors++;
			}
			safe_strdup(cfg_state.key_filename, cep->value);
			cfg_state.have_key = 1;
			continue;
		}
		if (!strcmp(cep->name, "verify-url"))
		{
			if (cfg_state.have_vfy)
			{
				config_error("%s:%i: duplicate %s:%s item", cep->file->filename, cep->line_number, MYCONF, cep->name);
				errors++;
				continue;
			}
			cfg_state.have_vfy = 1;
			if (!vfy_url_is_valid(cep->value))
			{
				config_error("%s:%i: Optional URL specified in %s::%s is invalid!", cep->file->filename, cep->line_number, MYCONF, cep->name);
				errors++;
				continue;
			}
			if (strlen(cep->value) > URL_LENGTH)
			{
				config_error("%s:%i: Optional URL specified in %s::%s is too long!", cep->file->filename, cep->line_number, MYCONF, cep->name);
				errors++;
			}
			continue;
		}
		if (!strcmp(cep->name, "service"))
		{
			have_ssecret = 0;
			have_smethod = 0;
			have_svfy = 0;
			have_scert = 0;
			if (strchr(cep->value, ' ') || strchr(cep->value, ','))
			{
				config_error("%s:%i: Invalid %s::%s name (contains spaces or commas)", cep->file->filename, cep->line_number, MYCONF, cep->name);
				errors++;
				continue;
			}
			if (find_jwt_service(services, cep->value))
			{
				config_error("%s:%i: Duplicate %s::%s name \"%s\"", cep->file->filename, cep->line_number, MYCONF, cep->name, cep->value);
				errors++;
				continue;
			}
			*ss = safe_alloc(sizeof(struct jwt_service)); /* store the new name for further checking */
			safe_strdup((*ss)->name, cep->value);
			ss = &(*ss)->next;
			for (cep2 = cep->items; cep2; cep2 = cep2->next)
			{
				if (!cep2->name || !cep2->value || !cep2->value[0])
				{
					config_error("%s:%i: blank/incomplete %s::service entry", cep2->file->filename, cep2->line_number, MYCONF);
					errors++;
					continue;
				}

				if (!strcmp(cep2->name, "method"))
				{
					if (have_smethod)
					{
						config_error("%s:%i: duplicate %s::service::%s item", cep2->file->filename, cep2->line_number, MYCONF, cep2->name);
						errors++;
						continue;
					}
					have_smethod = EXTJWT_METHOD_from_string(cep2->value);
					if (have_smethod == EXTJWT_METHOD_NOT_SET || have_smethod == EXTJWT_METHOD_NONE)
					{
						config_error("%s:%i: invalid value of optional %s::service::%s \"%s\" (check docs for allowed options)", cep2->file->filename, cep2->line_number, MYCONF, cep2->name, cep2->value);
						errors++;
					}
					continue;
				}

				if (!strcmp(cep2->name, "secret"))
				{
					if (have_ssecret)
					{
						config_error("%s:%i: duplicate %s::service::%s item", cep2->file->filename, cep2->line_number, MYCONF, cep2->name);
						errors++;
						continue;
					}
					have_ssecret = 1;
					if (strlen(cep2->value) < 4) /* TODO maybe a better check? */
					{
						config_error("%s:%i: Secret specified in %s::service::%s is too short!", cep2->file->filename, cep2->line_number, MYCONF, cep2->name);
						errors++;
					}
					continue;
				}

				if (!strcmp(cep2->name, "key"))
				{
					if (have_scert)
					{
						config_error("%s:%i: duplicate %s::service::%s item", cep2->file->filename, cep2->line_number, MYCONF, cep2->name);
						errors++;
						continue;
					}
					if (!is_file_readable(cep2->value, CONFDIR))
					{
						config_error("%s:%i: Cannot open file \"%s\" specified in %s::service::%s for reading", cep2->file->filename, cep2->line_number, cep2->value, MYCONF, cep2->name);
						errors++;
					}
					have_scert = 1;
					safe_strdup(sfilename, cep2->value);
					sfilename_line_number = cep2->line_number;
					continue;
				}

				if (!strcmp(cep2->name, "expire-after"))
				{
					if (!extjwt_valid_integer_string(cep2->value, 1, 9999))
					{
						config_error("%s:%i: %s::%s must be an integer between 1 and 9999 (seconds)", cep2->file->filename, cep2->line_number, MYCONF, cep2->name);
						errors++;
					}
					continue;
				}

				if (!strcmp(cep2->name, "verify-url"))
				{
					if (have_svfy)
					{
						config_error("%s:%i: duplicate %s::service::%s item", cep2->file->filename, cep2->line_number, MYCONF, cep2->name);
						errors++;
						continue;
					}
					have_svfy = 1;
					if (!vfy_url_is_valid(cep2->value))
					{
						config_error("%s:%i: Optional URL specified in %s::service::%s is invalid!", cep2->file->filename, cep2->line_number, MYCONF, cep2->name);
						errors++;
						continue;
					}
					if (strlen(cep2->value) > URL_LENGTH)
					{
						config_error("%s:%i: Optional URL specified in %s::service::%s is too long!", cep2->file->filename, cep2->line_number, MYCONF, cep2->name);
						errors++;
					}
					continue;
				}

				config_error("%s:%i: invalid %s::service attribute %s (must be one of: name, secret, expire-after)", cep2->file->filename, cep2->line_number, MYCONF, cep2->name);
				errors++;
			}
			if (!have_smethod)
			{
				config_error("%s:%i: invalid %s::service entry (no %s::service::method specfied)", cep->file->filename, cep->line_number, MYCONF, MYCONF);
				errors++;
				continue;
			}
			if (have_ssecret && NEEDS_KEY(have_smethod))
			{
				config_error("%s:%i: invalid %s::service entry (this method needs %s::service::key and not %s::service::secret option)", cep->file->filename, cep->line_number, MYCONF, MYCONF, MYCONF);
				errors++;
				continue;
			}
			if (have_scert && !NEEDS_KEY(have_smethod))
			{
				config_error("%s:%i: invalid %s::service entry (this method needs %s::service::secret and not %s::service::key option)", cep->file->filename, cep->line_number, MYCONF, MYCONF, MYCONF);
				errors++;
				continue;
			}
			if (!have_ssecret && !NEEDS_KEY(have_smethod))
			{
				config_error("%s:%i: invalid %s::service entry (must contain %s::service::secret option)", cep->file->filename, cep->line_number, MYCONF, MYCONF);
				errors++;
				continue;
			}
			if (!have_scert && NEEDS_KEY(have_smethod)) {
				config_error("%s:%i: invalid %s::service entry (must contain %s::service::key option)", cep->file->filename, cep->line_number, MYCONF, MYCONF);
				errors++;
				continue;
			}
			if (NEEDS_KEY(have_smethod) && have_scert)
			{
				char *keyerr;
				keyerr = extjwt_test_key(sfilename, have_smethod);
				if (keyerr)
				{
					config_error("%s:%i: Invalid key file specified for %s::key: %s", cep->file->filename, sfilename_line_number, MYCONF, keyerr);
					errors++;
				}
			}
			continue;
		}
		config_error("%s:%i: unknown directive %s::%s", cep->file->filename, cep->line_number, MYCONF, cep->name);
		errors++;
	}
	*errs = errors;
	extjwt_free_services(&services);
	if (errors)
		safe_free(cfg_state.key_filename);
	safe_free(sfilename);
	return errors ? -1 : 1;
}

int extjwt_configposttest(int *errs)
{
	int errors = 0;
	if (cfg_state.have_method == EXTJWT_METHOD_NOT_SET)
	{
		config_error("No %s::method specfied!", MYCONF);
		errors++;
	} else
	{
		if (cfg_state.have_method != EXTJWT_METHOD_NONE && !NEEDS_KEY(cfg_state.have_method) && !cfg_state.have_secret)
		{
			config_error("No %s::secret specfied as required by requested method!", MYCONF);
			errors++;
		}
		if ((cfg_state.have_method == EXTJWT_METHOD_NONE || NEEDS_KEY(cfg_state.have_method)) && cfg_state.have_secret)
		{
			config_error("A %s::secret specfied but it should not be when using requested method!", MYCONF);
			errors++;
		}
		if (NEEDS_KEY(cfg_state.have_method) && !cfg_state.have_key)
		{
			config_error("No %s::key specfied as required by requested method!", MYCONF);
			errors++;
		}
		if (!NEEDS_KEY(cfg_state.have_method) && cfg_state.have_key)
		{
			config_error("A %s::key specfied but it should not be when using requested method!", MYCONF);
			errors++;
		}
		if (NEEDS_KEY(cfg_state.have_method) && cfg_state.have_key && cfg_state.key_filename)
		{
			char *keyerr;
			
			keyerr = extjwt_test_key(cfg_state.key_filename, cfg_state.have_method);
			if (keyerr)
			{
				config_error("Invalid key file specified for %s::key: %s", MYCONF, keyerr);
				errors++;
			}
		}
	}
	safe_free(cfg_state.key_filename);
	if (errors)
	{
		*errs = errors;
		return -1;
	}
	/* setting defaults, FIXME this may behave incorrectly if there's another module failing POSTTEST */
	if (!cfg_state.have_expire)
		cfg.exp_delay = 30;
	/* prepare service list to load new data */
	extjwt_free_services(&jwt_services);
	return 1;
}

int extjwt_configrun(ConfigFile *cf, ConfigEntry *ce, int type)
{ /* actually use the new configuration data */
	ConfigEntry *cep, *cep2;
	struct jwt_service **ss = &jwt_services;
	if (*ss)
		ss = &((*ss)->next);

	if (type != CONFIG_MAIN)
		return 0;

	if (!ce || strcmp(ce->name, MYCONF))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "method"))
		{
			cfg.method = EXTJWT_METHOD_from_string(cep->value);
			continue;
		}
		if (!strcmp(cep->name, "expire-after"))
		{
			cfg.exp_delay = atoi(cep->value);
			continue;
		}
		if (!strcmp(cep->name, "secret"))
		{
			cfg.secret = strdup(cep->value);
			continue;
		}
		if (!strcmp(cep->name, "key"))
		{
			cfg.secret = extjwt_read_file_contents(cep->value, 0, NULL);
			continue;
		}
		if (!strcmp(cep->name, "verify-url"))
		{
			cfg.vfy = strdup(cep->value);
			continue;
		}
		if (!strcmp(cep->name, "service"))
		{ /* nested block */
			*ss = safe_alloc(sizeof(struct jwt_service));
			(*ss)->cfg = safe_alloc(sizeof(struct extjwt_config));
			safe_strdup((*ss)->name, cep->value); /* copy the service name */
			for (cep2 = cep->items; cep2; cep2 = cep2->next)
			{
				if (!strcmp(cep2->name, "method"))
				{
					(*ss)->cfg->method = EXTJWT_METHOD_from_string(cep2->value);
					continue;
				}
				if (!strcmp(cep2->name, "expire-after"))
				{
					(*ss)->cfg->exp_delay = atoi(cep2->value);
					continue;
				}
				if (!strcmp(cep2->name, "secret"))
				{
					(*ss)->cfg->secret = strdup(cep2->value);
					continue;
				}
				if (!strcmp(cep2->name, "key"))
				{
					(*ss)->cfg->secret = extjwt_read_file_contents(cep2->value, 0, NULL);
					continue;
				}
				if (!strcmp(cep2->name, "verify-url"))
				{
					(*ss)->cfg->vfy = strdup(cep2->value);
					continue;
				}
			}
			ss = &((*ss)->next);
		}
	}
	return 1;
}

char *extjwt_read_file_contents(const char *file, int absolute, int *size)
{
	FILE *f = NULL;
	int fsize;
	char *filename = NULL;
	char *buf = NULL;
	do
	{
		safe_strdup(filename, file);
		if (!absolute)
			convert_to_absolute_path(&filename, CONFDIR);
		f = fopen(filename, "rb");
		if (!f)
			break;
		fseek(f, 0, SEEK_END);
		fsize = ftell(f);
		fseek(f, 0, SEEK_SET);
		buf = safe_alloc(fsize + 1);
		fsize = fread(buf, 1, fsize, f);
		buf[fsize] = '\0';
		if (size)
			*size = fsize;
		fclose(f);
	} while (0);
	safe_free(filename);
	if (!buf && size)
		*size = 0;
	return buf;
}

CMD_FUNC(cmd_extjwt)
{
	Channel *channel;
	char *payload;
	char *token, *full_token;
	struct jwt_service *service = NULL;
	struct extjwt_config *config;
	int last = 0;
	char message[MAX_TOKEN_CHUNK+1];
	if (parc < 2 || BadPtr(parv[1]))
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, MSG_EXTJWT);
		return;
	}
	if (parv[1][0] == '*' && parv[1][1] == '\0')
	{
		channel = NULL; /* not linked to a channel */
	} else
	{
		channel = find_channel(parv[1]);
		if (!channel)
		{
			sendnumeric(client, ERR_NOSUCHNICK, parv[1]);
			return;
		}
	}
	if (parc > 2 && !BadPtr(parv[2]))
	{
		service = find_jwt_service(jwt_services, parv[2]);
		if (!service)
		{
			sendto_one(client, NULL, ":%s FAIL %s NO_SUCH_SERVICE :No such service", me.name, MSG_EXTJWT);
			return;
		}
	}
	if (service){
		config = service->cfg; /* service config */
	} else {
		config = &cfg; /* default config */
	}
	if (!(payload = extjwt_make_payload(client, channel, config)) || !(full_token = extjwt_generate_token(payload, config)))
	{
		sendto_one(client, NULL, ":%s FAIL %s UNKNOWN_ERROR :Failed to generate token", me.name, MSG_EXTJWT);
		return;
	}
	safe_free(payload);
	token = full_token;
	do
	{
		if (strlen(token) <= MAX_TOKEN_CHUNK)
		{ /* the remaining data (or whole token) will fit a single irc message */
			last = 1;
			strcpy(message, token);
		} else
		{ /* send a chunk and shift buffer */
			strlcpy(message, token, MAX_TOKEN_CHUNK+1);
			token += MAX_TOKEN_CHUNK;
		}
		sendto_one(client, NULL, extjwt_message_pattern, me.name, parv[1], "*", last?"":"* ", message);
	} while (!last);
	safe_free(full_token);
}

char *extjwt_make_payload(Client *client, Channel *channel, struct extjwt_config *config)
{
	Membership *lp;
	json_t *payload = NULL;
	json_t *modes = NULL;
	json_t *umodes = NULL;
	char *modestring;
	char singlemode[2] = { '\0' };
	char *result;

	if (!IsUser(client))
		return NULL;

	payload = json_object();
	modes = json_array();
	umodes = json_array();
	
	json_object_set_new(payload, "exp", json_integer(TStime()+config->exp_delay));
	json_object_set_new(payload, "iss", json_string_unreal(me.name));
	json_object_set_new(payload, "sub", json_string_unreal(client->name));
	json_object_set_new(payload, "account", json_string_unreal(IsLoggedIn(client)?client->user->account:""));
	
	if (config->vfy) /* also add the URL */
		json_object_set_new(payload, "vfy", json_string_unreal(config->vfy));

	if (IsOper(client)) /* add "o" ircop flag */
		json_array_append_new(umodes, json_string("o"));
	json_object_set_new(payload, "umodes", umodes);

	if (channel)
	{ /* fill in channel information and user flags */
		lp = find_membership_link(client->user->channel, channel);
		if (lp)
		{
			modestring = lp->member_modes;
			while (*modestring)
			{
				singlemode[0] = *modestring;
				json_array_append_new(modes, json_string(singlemode));
				modestring++;
			}
		}
		json_object_set_new(payload, "channel", json_string_unreal(channel->name));
		json_object_set_new(payload, "joined", json_integer(lp?1:0));
		json_object_set_new(payload, "cmodes", modes);
	}
	result = json_dumps(payload, JSON_COMPACT);
	json_decref(modes);
	json_decref(umodes);
	json_decref(payload);
	return result;
}

void b64url(char *b64)
{ /* convert base64 to base64-url */
	while (*b64)
	{
		if (*b64 == '+')
			*b64 = '-';
		if (*b64 == '/')
			*b64 = '_';
		if (*b64 == '=')
		{
			*b64 = '\0';
			return;
		}
		b64++;
	}
}

unsigned char *extjwt_hash(int method, const void *key, int keylen, const unsigned char *data, int datalen, unsigned int* resultlen)
{
	switch(method)
	{
		case EXTJWT_METHOD_HS256: case EXTJWT_METHOD_HS384: case EXTJWT_METHOD_HS512:
			return extjwt_hmac_extjwt_hash(method, key, keylen, data, datalen, resultlen);
		case EXTJWT_METHOD_RS256: case EXTJWT_METHOD_RS384: case EXTJWT_METHOD_RS512: case EXTJWT_METHOD_ES256: case EXTJWT_METHOD_ES384: case EXTJWT_METHOD_ES512:
			return extjwt_sha_pem_extjwt_hash(method, key, keylen, data, datalen, resultlen);
	}
	return NULL;
}

unsigned char* extjwt_sha_pem_extjwt_hash(int method, const void *key, int keylen, const unsigned char *data, int datalen, unsigned int* resultlen)
{
	EVP_MD_CTX *mdctx = NULL;
	ECDSA_SIG *ec_sig = NULL;
	const BIGNUM *ec_sig_r = NULL;
	const BIGNUM *ec_sig_s = NULL;
	BIO *bufkey = NULL;
	const EVP_MD *alg;
	int type;
	EVP_PKEY *pkey = NULL;
	int pkey_type;
	unsigned char *sig = NULL;
	int ret = 0;
	size_t slen;
	char *retval = NULL;
	char *output = NULL;
	char *sig_ptr;

	do
	{
		switch (method)
		{
			case EXTJWT_METHOD_RS256:
				alg = EVP_sha256();
				type = EVP_PKEY_RSA;
				break;
			case EXTJWT_METHOD_RS384:
				alg = EVP_sha384();
				type = EVP_PKEY_RSA;
				break;
			case EXTJWT_METHOD_RS512:
				alg = EVP_sha512();
				type = EVP_PKEY_RSA;
				break;
			case EXTJWT_METHOD_ES256:
				alg = EVP_sha256();
				type = EVP_PKEY_EC;
				break;
			case EXTJWT_METHOD_ES384:
				alg = EVP_sha384();
				type = EVP_PKEY_EC;
				break;
			case EXTJWT_METHOD_ES512:
				alg = EVP_sha512();
				type = EVP_PKEY_EC;
				break;
			default:
				return NULL;
		}

#if (OPENSSL_VERSION_NUMBER < 0x10100003L) /* https://github.com/openssl/openssl/commit/8ab31975bacb9c907261088937d3aa4102e3af84 */
		if (!(bufkey = BIO_new_mem_buf((void *)key, keylen)))
			break; /* out of memory */
#else
		if (!(bufkey = BIO_new_mem_buf(key, keylen)))
			break; /* out of memory */
#endif
		if (!(pkey = PEM_read_bio_PrivateKey(bufkey, NULL, NULL, NULL)))
			break; /* invalid key? */
		pkey_type = EVP_PKEY_id(pkey);
		if (type != pkey_type)
			break; /* invalid key type */
		if (!(mdctx = EVP_MD_CTX_create()))
			break; /* out of memory */
		if (EVP_DigestSignInit(mdctx, NULL, alg, NULL, pkey) != 1)
			break; /* initialize error */
		if (EVP_DigestSignUpdate(mdctx, data, datalen) != 1)
			break; /* signing error */
		if (EVP_DigestSignFinal(mdctx, NULL, &slen) != 1) /* get required buffer length */
			break;
		sig = safe_alloc(slen);
		if (EVP_DigestSignFinal(mdctx, sig, &slen) != 1)
			break;
		if (pkey_type != EVP_PKEY_EC)
		{
			*resultlen = slen;
			output = safe_alloc(slen);
			memcpy(output, sig, slen);
			retval = output;
		} else
		{
			unsigned int degree, bn_len, r_len, s_len, buf_len;
			unsigned char *raw_buf = NULL;
			EC_KEY *ec_key;
			if (!(ec_key = EVP_PKEY_get1_EC_KEY(pkey)))
				break; /* out of memory */
			degree = EC_GROUP_get_degree(EC_KEY_get0_group(ec_key));
			EC_KEY_free(ec_key);
			sig_ptr = sig;
			if (!(ec_sig = d2i_ECDSA_SIG(NULL, (const unsigned char **)&sig_ptr, slen)))
				break; /* out of memory */
			ECDSA_SIG_get0(ec_sig, &ec_sig_r, &ec_sig_s);
			r_len = BN_num_bytes(ec_sig_r);
			s_len = BN_num_bytes(ec_sig_s);
			bn_len = (degree+7)/8;
			if (r_len>bn_len || s_len > bn_len)
				break;
			buf_len = bn_len*2;
			raw_buf = safe_alloc(buf_len);
			BN_bn2bin(ec_sig_r, raw_buf+bn_len-r_len);
			BN_bn2bin(ec_sig_s, raw_buf+buf_len-s_len);
			output = safe_alloc(buf_len);
			*resultlen = buf_len;
			memcpy(output, raw_buf, buf_len);
			retval = output;
			safe_free(raw_buf);
		}
	} while (0);

	if (bufkey)
		BIO_free(bufkey);
	if (pkey)
		EVP_PKEY_free(pkey);
	if (mdctx)
		EVP_MD_CTX_destroy(mdctx);
	if (ec_sig)
		ECDSA_SIG_free(ec_sig);
	safe_free(sig);
	return retval;
}

unsigned char* extjwt_hmac_extjwt_hash(int method, const void *key, int keylen, const unsigned char *data, int datalen, unsigned int* resultlen)
{
	const EVP_MD* typ;
	char *hmac = safe_alloc(EVP_MAX_MD_SIZE);
	switch (method)
	{
		default:
		case EXTJWT_METHOD_HS256:
			typ = EVP_sha256();
			break;
		case EXTJWT_METHOD_HS384:
			typ = EVP_sha384();
			break;
		case EXTJWT_METHOD_HS512:
			typ = EVP_sha512();
			break;
	}
	if (HMAC(typ, key, keylen, data, datalen, hmac, resultlen))
	{ /* openssl call */
		return hmac;
	} else {
		safe_free(hmac);
		return NULL;
	}
}

char *extjwt_gen_header(int method)
{ /* returns header json */
	json_t *header = NULL;
	json_t *alg;
	char *result;

	header = json_object();
	json_object_set_new(header, "typ", json_string("JWT"));

	switch (method)
	{
		default:
		case EXTJWT_METHOD_HS256:
			alg = json_string("HS256");
			break;
		case EXTJWT_METHOD_HS384:
			alg = json_string("HS384");
			break;
		case EXTJWT_METHOD_HS512:
			alg = json_string("HS512");
			break;
		case EXTJWT_METHOD_RS256:
			alg = json_string("RS256");
			break;
		case EXTJWT_METHOD_RS384:
			alg = json_string("RS384");
			break;
		case EXTJWT_METHOD_RS512:
			alg = json_string("RS512");
			break;
		case EXTJWT_METHOD_ES256:
			alg = json_string("ES256");
			break;
		case EXTJWT_METHOD_ES384:
			alg = json_string("ES384");
			break;
		case EXTJWT_METHOD_ES512:
			alg = json_string("ES512");
			break;
		case EXTJWT_METHOD_NONE:
			alg = json_string("none");
			break;
	}
	json_object_set_new(header, "alg", alg);
	result = json_dumps(header, JSON_COMPACT);
	json_decref(header);
	return result;
}

char *extjwt_generate_token(const char *payload, struct extjwt_config *config)
{
	char *header = extjwt_gen_header(config->method);
	size_t b64header_size = strlen(header)*4/3 + 8; // base64 has 4/3 overhead
	size_t b64payload_size = strlen(payload)*4/3 + 8;
	size_t b64sig_size = 4096*4/3 + 8;
	size_t b64data_size = b64header_size + b64payload_size + b64sig_size + 4;
	char *b64header = safe_alloc(b64header_size);
	char *b64payload = safe_alloc(b64payload_size);
	char *b64sig = safe_alloc(b64sig_size);
	char *b64data = safe_alloc(b64data_size);
	unsigned int extjwt_hashsize;
	char *extjwt_hash_val = NULL;
	char *retval = NULL;
	b64_encode(header, strlen(header), b64header, b64header_size);
	b64_encode(payload, strlen(payload), b64payload, b64payload_size);
	b64url(b64header);
	b64url(b64payload);
	snprintf(b64data, b64data_size, "%s.%s", b64header, b64payload); // generate first part of the token
	if (config->method != EXTJWT_METHOD_NONE)
	{
		extjwt_hash_val = extjwt_hash(config->method, config->secret, strlen(config->secret), b64data, strlen(b64data), &extjwt_hashsize); // calculate the signature extjwt_hash
		if (extjwt_hash_val)
		{
			b64_encode(extjwt_hash_val, extjwt_hashsize, b64sig, b64sig_size);
			b64url(b64sig);
			strlcat(b64data, ".", b64data_size); // append signature extjwt_hash to token
			strlcat(b64data, b64sig, b64data_size);
			retval = b64data;
		}
	} else
	{
		retval = b64data;
	}
	safe_free(header);
	safe_free(b64header);
	safe_free(b64payload);
	safe_free(b64sig);
	safe_free(extjwt_hash_val);

	if (retval != b64data)
		safe_free(b64data);

	return retval;
}
