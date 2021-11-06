/*
 *   IRC - Internet Relay Chat, src/modules/geoip_csv.c
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

ModuleHeader MOD_HEADER
  = {
	"geoip_csv",
	"5.0",
	"GEOIP using csv data files", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

struct geoip_csv_config_s {
	char *v4_db_file;
	char *v6_db_file;
	char *countries_db_file;
/* for config reading only */
	int have_config;
	int have_ipv4_database;
	int have_ipv6_database;
	int have_countries;
};

struct geoip_csv_ip_range {
	uint32_t addr;
	uint32_t mask;
	int geoid;
	struct geoip_csv_ip_range *next;
};

struct geoip_csv_ip6_range {
	uint16_t addr[8];
	uint16_t mask[8];
	int geoid;
	struct geoip_csv_ip6_range *next;
};

struct geoip_csv_country {
	char code[10];
	char name[100];
	char continent[25];
	int id;
	struct geoip_csv_country *next;
};

/* Variables */
struct geoip_csv_config_s geoip_csv_config;
struct geoip_csv_ip_range *geoip_csv_ip_range_list[256]; // we are keeping a separate list for each possible first octet to speed up searching
struct geoip_csv_ip6_range *geoip_csv_ip6_range_list = NULL; // for ipv6 there would be too many separate lists so just use a single one
struct geoip_csv_country *geoip_csv_country_list = NULL;

/* Forward declarations */
static void geoip_csv_free_ipv4(void);
static void geoip_csv_free_ipv6(void);
static void geoip_csv_free_ipv6(void);
static void geoip_csv_free_countries(void);
static void geoip_csv_free(void);
static int geoip_csv_read_ipv4(char *file);
static int geoip_csv_ip6_convert(char *ip, uint16_t out[8]);
static int geoip_csv_read_ipv6(char *file);
static int geoip_csv_read_countries(char *file);
static struct geoip_csv_country *geoip_csv_get_country(int id);
static int geoip_csv_get_v4_geoid(char *iip);
static int geoip_csv_get_v6_geoid(char *iip);
int geoip_csv_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int geoip_csv_configposttest(int *errs);
int geoip_csv_configrun(ConfigFile *cf, ConfigEntry *ce, int type);
void geoip_csv_free(void);
GeoIPResult *geoip_lookup_csv(char *ip);

int geoip_csv_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	ConfigEntry *cep;
	int errors = 0;
	int i;
	
	if (type != CONFIG_SET)
		return 0;

	if (!ce || !ce->name)
		return 0;

	if (strcmp(ce->name, "geoip-csv"))
		return 0;

	geoip_csv_config.have_config = 1;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "ipv4-blocks-file"))
		{
			if (geoip_csv_config.have_ipv4_database)
			{
				config_error("%s:%i: duplicate item set::geoip-csv::%s", cep->file->filename, cep->line_number, cep->name);
				continue;
			}
			if (!is_file_readable(cep->value, PERMDATADIR))
			{
				config_error("%s:%i: set::geoip-csv::%s: cannot open file \"%s/%s\" for reading (%s)", cep->file->filename, cep->line_number, cep->name, PERMDATADIR, cep->value, strerror(errno));
				errors++;
				continue;
			}
			geoip_csv_config.have_ipv4_database = 1;
			continue;
		}
		if (!strcmp(cep->name, "ipv6-blocks-file"))
		{
			if (geoip_csv_config.have_ipv6_database)
			{
				config_error("%s:%i: duplicate item set::geoip-csv::%s", cep->file->filename, cep->line_number, cep->name);
				continue;
			}
			if (!is_file_readable(cep->value, PERMDATADIR))
			{
				config_error("%s:%i: set::geoip-csv::%s: cannot open file \"%s/%s\" for reading (%s)", cep->file->filename, cep->line_number, cep->name, PERMDATADIR, cep->value, strerror(errno));
				errors++;
				continue;
			}
			geoip_csv_config.have_ipv6_database = 1;
			continue;
		}
		if (!strcmp(cep->name, "countries-file"))
		{
			if (geoip_csv_config.have_countries)
			{
				config_error("%s:%i: duplicate item set::geoip-csv::%s", cep->file->filename, cep->line_number, cep->name);
				continue;
			}
			if (!is_file_readable(cep->value, PERMDATADIR))
			{
				config_error("%s:%i: set::geoip-csv::%s: cannot open file \"%s/%s\" for reading (%s)", cep->file->filename, cep->line_number, cep->name, PERMDATADIR, cep->value, strerror(errno));
				errors++;
				continue;
			}
			geoip_csv_config.have_countries = 1;
			continue;
		}
		config_warn("%s:%i: unknown item set::geoip-csv::%s", cep->file->filename, cep->line_number, cep->name);
	}
	
	*errs = errors;
	return errors ? -1 : 1;
}

int geoip_csv_configposttest(int *errs)
{
	int errors = 0;
	if (geoip_csv_config.have_config)
	{
		if (!geoip_csv_config.have_countries)
		{
			config_error("[geoip_csv] no countries file specified! Remove set::geoip-csv to use defaults");
			errors++;
		}
		if (!geoip_csv_config.have_ipv4_database && !geoip_csv_config.have_ipv6_database)
		{
			config_error("[geoip_csv] no database files specified! Remove set::geoip-csv to use defaults");
			errors++;
		}
	} else
	{
		safe_strdup(geoip_csv_config.v4_db_file, "GeoLite2-Country-Blocks-IPv4.csv");
		safe_strdup(geoip_csv_config.v6_db_file, "GeoLite2-Country-Blocks-IPv6.csv");
		safe_strdup(geoip_csv_config.countries_db_file, "GeoLite2-Country-Locations-en.csv");

		if (is_file_readable(geoip_csv_config.v4_db_file, PERMDATADIR))
		{
			geoip_csv_config.have_ipv4_database = 1;
		} else
		{
			config_warn("[geoip_csv] cannot open IPv4 blocks file \"%s/%s\" for reading (%s)", PERMDATADIR, geoip_csv_config.v4_db_file, strerror(errno));
			safe_free(geoip_csv_config.v4_db_file);
		}
		if (is_file_readable(geoip_csv_config.v6_db_file, PERMDATADIR))
		{
			geoip_csv_config.have_ipv6_database = 1;
		} else
		{
			config_warn("[geoip_csv] cannot open IPv6 blocks file \"%s/%s\" for reading (%s)", PERMDATADIR, geoip_csv_config.v6_db_file, strerror(errno));
			safe_free(geoip_csv_config.v6_db_file);
		}
		if (!is_file_readable(geoip_csv_config.countries_db_file, PERMDATADIR))
		{
			config_error("[geoip_csv] cannot open countries file \"%s/%s\" for reading (%s)", PERMDATADIR, geoip_csv_config.countries_db_file, strerror(errno));
			safe_free(geoip_csv_config.countries_db_file);
			errors++;
		}
		if (!geoip_csv_config.have_ipv4_database && !geoip_csv_config.have_ipv6_database)
		{
			config_error("[geoip_csv] couldn't read any blocks file! Either put these in %s location "
					"or specify another in set::geoip-csv config block", PERMDATADIR);
			errors++;
		}
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int geoip_csv_configrun(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;

	if (type != CONFIG_SET)
		return 0;

	if (!ce || !ce->name)
		return 0;

	if (strcmp(ce->name, "geoip-csv"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "ipv4-blocks-file") && geoip_csv_config.have_ipv4_database)
			safe_strdup(geoip_csv_config.v4_db_file, cep->value);
		if (!strcmp(cep->name, "ipv6-blocks-file") && geoip_csv_config.have_ipv6_database)
			safe_strdup(geoip_csv_config.v6_db_file, cep->value);
		if (!strcmp(cep->name, "countries-file"))
			safe_strdup(geoip_csv_config.countries_db_file, cep->value);
	}
	return 1;
}

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	if (!CallbackAddPVoid(modinfo->handle, CALLBACKTYPE_GEOIP_LOOKUP, TO_PVOIDFUNC(geoip_lookup_csv)))
	{
		unreal_log(ULOG_ERROR, "geoip_csv", "GEOIP_ADD_CALLBACK_FAILED", NULL,
		           "geoip_csv: Could not install GEOIP_LOOKUP callback. "
		           "Most likely another geoip module is already loaded. "
		           "You can only load one!");
		return MOD_FAILED;
	}

	geoip_csv_config.have_config = 0;
	geoip_csv_config.have_ipv4_database = 0;
	geoip_csv_config.have_ipv6_database = 0;
	geoip_csv_config.have_countries = 0;
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, geoip_csv_configtest);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, geoip_csv_configposttest);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	geoip_csv_free();
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, geoip_csv_configrun);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	int found_good_file = 0;

	if (geoip_csv_config.v4_db_file)
	{
		convert_to_absolute_path(&geoip_csv_config.v4_db_file, PERMDATADIR);
		if (!geoip_csv_read_ipv4(geoip_csv_config.v4_db_file))
		{
			found_good_file = 1;
		}
	}
	if (geoip_csv_config.v6_db_file)
	{
		convert_to_absolute_path(&geoip_csv_config.v6_db_file, PERMDATADIR);
		if (!geoip_csv_read_ipv6(geoip_csv_config.v6_db_file))
		{
			found_good_file = 1;
		}
	}
	if (!geoip_csv_config.countries_db_file)
	{
		unreal_log(ULOG_DEBUG, "geoip_csv", "GEOIP_NO_COUNTRIES", NULL,
				"[BUG] No countries file specified");
		geoip_csv_free();
		return MOD_FAILED;
	}
	convert_to_absolute_path(&geoip_csv_config.countries_db_file, PERMDATADIR);
	if (geoip_csv_read_countries(geoip_csv_config.countries_db_file))
	{
		unreal_log(ULOG_ERROR, "geoip_csv", "GEOIP_CANNOT_OPEN_DB", NULL,
					"could not open required countries file!");
		geoip_csv_free();
		return MOD_FAILED;
	}

	if (!found_good_file)
	{
		unreal_log(ULOG_ERROR, "geoip_csv", "GEOIP_CANNOT_OPEN_DB", NULL,
					"could not open any database!");
		geoip_csv_free();
		return MOD_FAILED;
	}
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	geoip_csv_free();
	return MOD_SUCCESS;
}

static void geoip_csv_free_ipv4(void)
{
	struct geoip_csv_ip_range *ptr, *oldptr;
	int i;
	for (i=0; i<256; i++)
	{
		ptr = geoip_csv_ip_range_list[i];
		geoip_csv_ip_range_list[i] = NULL;
		while (ptr)
		{
			oldptr = ptr;
			ptr = ptr->next;
			safe_free(oldptr);
		}
	}
}

static void geoip_csv_free_ipv6(void)
{
	struct geoip_csv_ip6_range *ptr, *oldptr;
	ptr = geoip_csv_ip6_range_list;
	geoip_csv_ip6_range_list = NULL;
	while (ptr)
	{
		oldptr = ptr;
		ptr = ptr->next;
		safe_free(oldptr);
	}
}

static void geoip_csv_free_countries(void)
{
	struct geoip_csv_country *ptr, *oldptr;
	ptr = geoip_csv_country_list;
	geoip_csv_country_list = NULL;
	while (ptr)
	{
		oldptr = ptr;
		ptr = ptr->next;
		safe_free(oldptr);
	}
}

static void geoip_csv_free(void)
{
	geoip_csv_free_ipv4();
	geoip_csv_free_ipv6();
	geoip_csv_free_countries();
}

/* reading data from files */

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define BUFLEN 8191

static int geoip_csv_read_ipv4(char *file)
{
	FILE *u;
	char buf[BUFLEN+1];
	int cidr, geoid;
	char ip[24];
	char netmask[24];
	uint32_t addr;
	uint32_t mask;
	struct geoip_csv_ip_range *curr[256];
	struct geoip_csv_ip_range *ptr;
	memset(curr, 0, sizeof(curr));
	int i;
	char *filename = NULL;
	
	safe_strdup(filename, file);
	convert_to_absolute_path(&filename, CONFDIR);
	u = fopen(filename, "r");
	safe_free(filename);
	if (!u)
	{
		config_warn("[geoip_csv] Cannot open IPv4 ranges list file");
		return 1;
	}
	
	if (!fgets(buf, BUFLEN, u))
	{
		config_warn("[geoip_csv] IPv4 list file is empty");
		fclose(u);
		return 1;
	}
	buf[BUFLEN] = '\0';
	while (fscanf(u, "%23[^/\n]/%d,%" STR(BUFLEN) "[^\n]\n", ip, &cidr, buf) == 3)
	{
		if (sscanf(buf, "%d,", &geoid) != 1)
		{
			/* missing geoid: can happen with valid files */
			continue;
		}

		if (cidr < 1 || cidr > 32)
		{
			config_warn("[geoip_csv] Invalid CIDR found! IP=%s CIDR=%d! Bad CSV file?", ip, cidr);
			continue;
		}

		if (inet_pton(AF_INET, ip, &addr) < 1)
		{
			config_warn("[geoip_csv] Invalid IP found! \"%s\" Bad CSV file?", ip);
			continue;
		}
		addr = htonl(addr);
		
		mask = 0;
		while (cidr)
		{ /* calculate netmask */
			mask >>= 1;
			mask |= (1<<31);
			cidr--;
		}
		
		i=0;
		do
		{ /* multiple iterations in case CIDR is <8 and we have multiple first octets matching */
			uint8_t index = addr>>24;
			if (!curr[index])
			{
				geoip_csv_ip_range_list[index] = safe_alloc(sizeof(struct geoip_csv_ip_range));
				curr[index] = geoip_csv_ip_range_list[index];
			} else
			{
				curr[index]->next = safe_alloc(sizeof(struct geoip_csv_ip_range));
				curr[index] = curr[index]->next;
			}
			ptr = curr[index];
			ptr->next = NULL;
			ptr->addr = addr;
			ptr->mask = mask;
			ptr->geoid = geoid;
			i++;
			index++;
		} while (i<=((~mask)>>24));
	}
	fclose(u);
	return 0;
}

static int geoip_csv_ip6_convert(char *ip, uint16_t out[8])
{ /* convert text to binary form */
	uint16_t tmp[8];
	int i;
	if (inet_pton(AF_INET6, ip, out) < 1)
		return 0;
	for (i=0; i<8; i++)
	{
		out[i] = htons(out[i]);
	}
	return 1;
}

#define IPV6_STRING_SIZE	40

static int geoip_csv_read_ipv6(char *file)
{
	FILE *u;
	char buf[BUFLEN+1];
	char *bptr, *optr;
	int cidr, geoid;
	char ip[IPV6_STRING_SIZE];
	uint16_t addr[8];
	uint16_t mask[8];
	struct geoip_csv_ip6_range *curr = NULL;
	struct geoip_csv_ip6_range *ptr;
	int error;
	int length;
	char *filename = NULL;

	safe_strdup(filename, file);
	convert_to_absolute_path(&filename, CONFDIR);
	u = fopen(filename, "r");
	safe_free(filename);
	if (!u)
	{
		config_warn("[geoip_csv] Cannot open IPv6 ranges list file");
		return 1;
	}
	if (!fgets(buf, BUFLEN, u))
	{
		config_warn("[geoip_csv] IPv6 list file is empty");
		fclose(u);
		return 1;
	}
	while (fgets(buf, BUFLEN, u))
	{
		error = 0;
		bptr = buf;
		optr = ip;
		length = 0;
		while (*bptr != '/')
		{
			if (!*bptr)
			{
				error = 1;
				break;
			}
			if (++length >= IPV6_STRING_SIZE)
			{
				ip[IPV6_STRING_SIZE-1] = '\0';
				config_warn("[geoip_csv] Too long IPv6 address found, starts with %s. Bad CSV file?", ip);
				error = 1;
				break;
			}
			*optr++ = *bptr++;
		}
		if (error)
			continue;
		*optr = '\0';
		bptr++;
		if (!geoip_csv_ip6_convert(ip, addr))
		{
			config_warn("[geoip_csv] Invalid IP found! \"%s\" Bad CSV file?", ip);
			continue;
		}
		sscanf(bptr, "%d,%d,", &cidr, &geoid);
		if (cidr < 1 || cidr > 128)
		{
			config_warn("[geoip_csv] Invalid CIDR found! CIDR=%d Bad CSV file?", cidr);
			continue;
		}

		memset(mask, 0, 16);
		
		int mask_bit = 0;
		while (cidr)
		{ /* calculate netmask */
			mask[mask_bit/16] |= 1<<(15-(mask_bit%16));
			mask_bit++;
			cidr--;
		}

		if (!curr)
		{
			geoip_csv_ip6_range_list = safe_alloc(sizeof(struct geoip_csv_ip6_range));
			curr = geoip_csv_ip6_range_list;
		} else
		{
			curr->next = safe_alloc(sizeof(struct geoip_csv_ip6_range));
			curr = curr->next;
		}
		ptr = curr;
		ptr->next = NULL;
		memcpy(ptr->addr, addr, 16);
		memcpy(ptr->mask, mask, 16);
		ptr->geoid = geoid;
	}
	fclose(u);
	return 0;
}

/* CSV fields; no STATE_GEONAME_ID because of using %d in fscanf */
#define STATE_LOCALE_CODE	0
#define STATE_CONTINENT_CODE	1
#define STATE_CONTINENT_NAME	2
#define STATE_COUNTRY_ISO_CODE	3
#define STATE_COUNTRY_NAME	4
#define STATE_IS_IN_EU	5

#define MEMBER_SIZE(type,member) sizeof(((type *)0)->member)

static int geoip_csv_read_countries(char *file)
{
	FILE *u;
	char code[MEMBER_SIZE(struct geoip_csv_country, code)];
	char continent[MEMBER_SIZE(struct geoip_csv_country, continent)];
	char name[MEMBER_SIZE(struct geoip_csv_country, name)];
	char buf[BUFLEN+1];
	int state;
	int id;
	struct geoip_csv_country *curr = NULL;
	char *filename = NULL;

	safe_strdup(filename, file);
	convert_to_absolute_path(&filename, CONFDIR);
	u = fopen(filename, "r");
	safe_free(filename);
	if (!u)
	{
		config_warn("[geoip_csv] Cannot open countries list file");
		return 1;
	}
	
	if (!fgets(buf, BUFLEN, u))
	{
		config_warn("[geoip_csv] Countries list file is empty");
		fclose(u);
		return 1;
	}
	while (fscanf(u, "%d,%" STR(BUFLEN) "[^\n]", &id, buf) == 2)
	{ /* getting country ID integer and all other data in string */
		char *ptr = buf;
		char *codeptr = code;
		char *contptr = continent;
		char *nptr = name;
		int quote_open = 0;
		int length = 0;
		state = STATE_LOCALE_CODE;
		while (*ptr)
		{
			switch (state)
			{
				case STATE_CONTINENT_NAME:
					if (*ptr == ',')
						goto next_line; /* no continent? */
					if (length >= MEMBER_SIZE(struct geoip_csv_country, continent))
					{
						*contptr = '\0';
						config_warn("[geoip_csv] Too long continent name found: `%s`. If you are sure your countries file is correct, please file a bug report.", continent);
						goto next_line;
					}
					*contptr = *ptr; /* scan for continent name */
					contptr++;
					length++;
					break;
				case STATE_COUNTRY_ISO_CODE:
					if (*ptr == ',')		/* country code is empty */
						goto next_line;	/* -- that means only the continent is specified - we ignore it completely */
					if (length >= MEMBER_SIZE(struct geoip_csv_country, code))
					{
						*codeptr = '\0';
						config_warn("[geoip_csv] Too long country code found: `%s`. If you are sure your countries file is correct, please file a bug report.", code);
						goto next_line;
					}
					*codeptr = *ptr; // scan for country code (DE, PL, US etc)
					codeptr++;
					length++;
					break;
				case STATE_COUNTRY_NAME:
					goto read_country_name;
				default:
					break; // ignore this field and wait for next one
			}
			ptr++;
			if (*ptr == ',')
			{
				length = 0;
				ptr++;
				state++;
			}
		}
		read_country_name:
		*codeptr = '\0';
		*contptr = '\0';
		length = 0;
		while (*ptr)
		{
			switch (*ptr)
			{
				case '"':
					quote_open = !quote_open;
					ptr++;
					continue;
				case ',':
					if (!quote_open)
						goto end_country_name; /* we reached the end of current CSV field */
				/* fall through */
				default:
					*nptr++ = *ptr++;
					if (length >= MEMBER_SIZE(struct geoip_csv_country, name))
					{
						*nptr = '\0';
						config_warn("[geoip_csv] Too long country name found: `%s`. If you are sure your countries file is correct, please file a bug report.", name);
						goto next_line;
					}
					break; // scan for country name
			}
		}
		end_country_name:
		*nptr = '\0';
		if (geoip_csv_country_list)
		{
			curr->next = safe_alloc(sizeof(struct geoip_csv_country));
			curr = curr->next;
		} else
		{
			geoip_csv_country_list = safe_alloc(sizeof(struct geoip_csv_country));
			curr = geoip_csv_country_list;
		}
		curr->next = NULL;
		strcpy(curr->code, code);
		strcpy(curr->name, name);
		strcpy(curr->continent, continent);
		curr->id = id;
		next_line: continue;
	}
	fclose(u);
	return 0;
}

static struct geoip_csv_country *geoip_csv_get_country(int id)
{
	struct geoip_csv_country *curr = geoip_csv_country_list;
	if (!curr)
		return NULL;
	int found = 0;
	for (;curr;curr = curr->next)
	{
		if (curr->id == id)
		{
			found = 1;
			break;
		}
	}
	if (found)
		return curr;
	return NULL;
}

static int geoip_csv_get_v4_geoid(char *iip)
{
	uint32_t addr, tmp_addr;
	struct geoip_csv_ip_range *curr;
	int i;
	int found = 0;
	if (inet_pton(AF_INET, iip, &addr) < 1)
	{
		unreal_log(ULOG_WARNING, "geoip_csv", "UNSUPPORTED_IP", NULL, "Invalid or unsupported client IP $ip", log_data_string("ip", iip));
		return 0;
	}
	addr = htonl(addr);
	curr = geoip_csv_ip_range_list[addr>>24];
	if (curr)
	{
		i = 0;
		for (;curr;curr = curr->next)
		{
			tmp_addr = addr;
			tmp_addr &= curr->mask; /* mask the address to filter out net prefix only */
			if (tmp_addr == curr->addr)
			{ /* ... and match it to the loaded data */
				found = 1;
				break;
			}
			i++;
		}
	}
	if (found)
		return curr->geoid;
	return 0;
}

static int geoip_csv_get_v6_geoid(char *iip)
{
	uint16_t addr[8];
	struct geoip_csv_ip6_range *curr;
	int i;
	int found = 0;
	
	if (!geoip_csv_ip6_convert(iip, addr))
	{
		unreal_log(ULOG_WARNING, "geoip_csv", "UNSUPPORTED_IP", NULL, "Invalid or unsupported client IP $ip", log_data_string("ip", iip));
		return 0;
	}
	curr = geoip_csv_ip6_range_list;
	if (curr)
	{
		for (;curr;curr = curr->next)
		{
			found = 1;
			for (i=0; i<8; i++)
			{
				if (curr->addr[i] != (addr[i] & curr->mask[i]))
				{ /* compare net address to loaded data */
					found = 0;
					break;
				}
			}
			if(found)
				break;
		}
	}
	if (found)
		return curr->geoid;
	return 0;
}

GeoIPResult *geoip_lookup_csv(char *ip)
{
	int geoid;
	struct geoip_csv_country *country;
	GeoIPResult *r;

	if (!ip)
		return NULL;

	if (strchr(ip, ':'))
	{
		geoid = geoip_csv_get_v6_geoid(ip);
	} else
	{
		geoid = geoip_csv_get_v4_geoid(ip);
	}

	if (geoid == 0)
		return NULL;

	country = geoip_csv_get_country(geoid);

	if (!country)
		return NULL;

	r = safe_alloc(sizeof(GeoIPResult));
	safe_strdup(r->country_code, country->code);
	safe_strdup(r->country_name, country->name);
	return r;
}

