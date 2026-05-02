/*
 * src/modules/emoji.c
 * IRCv3 draft/custom-emoji server-side bits.
 *
 * Spec: https://github.com/skizzerz/ircv3-specifications/blob/12a841ca/extensions/custom-emoji.md
 *
 * The spec asks the server to do exactly two things:
 *
 *   1. Advertise the URL of the network-wide emoji pack document via
 *      the `draft/EMOJI` ISUPPORT token.
 *   2. Allow channels to advertise their own pack document via the
 *      `draft/emoji` channel METADATA key.
 *
 * Step 2 is already free-form-supported by the existing `metadata`
 * module (any chanop or higher can SET any METADATA key on a channel),
 * so this module only handles step 1 plus a tiny config block.
 *
 * Config (obbyircd.conf):
 *   emoji {
 *       pack-url "https://emoji.example.com/pack.json";
 *   };
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER = {
	"emoji",
	"1.0",
	"draft/custom-emoji - server-wide emoji pack URL via ISUPPORT",
	"ObbyIRCd Team",
	"unrealircd-6",
};

#define CONF_EMOJI_BLOCK "emoji"
#define ISUPPORT_TOKEN   "draft/EMOJI"

static struct
{
	char *pack_url;
	int   configured;
} cfg;

static Module *emoji_modhandle = NULL;

/* ===================================================================
 * Forward decls
 * =================================================================== */
static void setconf(void);
static void freeconf(void);
static int  emoji_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
static int  emoji_configrun(ConfigFile *cf, ConfigEntry *ce, int type);
static void emoji_publish_isupport(void);

/* ===================================================================
 * Module entry points
 * =================================================================== */

MOD_TEST()
{
	setconf();
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, emoji_configtest);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	emoji_modhandle = modinfo->handle;
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, emoji_configrun);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	emoji_publish_isupport();
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	freeconf();
	return MOD_SUCCESS;
}

/* ===================================================================
 * Helpers
 * =================================================================== */

static void setconf(void)
{
	cfg.pack_url = NULL;
	cfg.configured = 0;
}

static void freeconf(void)
{
	safe_free(cfg.pack_url);
	cfg.configured = 0;
}

static void emoji_publish_isupport(void)
{
	/* Only advertise if a URL is actually configured.  ISupportSet on
	 * an existing token replaces the value so REHASH picks up changes
	 * automatically. */
	if (cfg.configured && cfg.pack_url && *cfg.pack_url)
		ISupportSet(emoji_modhandle, ISUPPORT_TOKEN, cfg.pack_url);
	else
		ISupportDelByName(ISUPPORT_TOKEN);
}

/* ===================================================================
 * Config parser
 * =================================================================== */

static int emoji_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	ConfigEntry *cep;
	int errors = 0;
	int seen_url = 0;

	if (type != CONFIG_MAIN)
		return 0;
	if (!ce || !ce->name || strcmp(ce->name, CONF_EMOJI_BLOCK))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!cep->name)
			continue;
		if (!strcasecmp(cep->name, "pack-url"))
		{
			if (BadPtr(cep->value))
			{
				config_error("%s:%i: %s::pack-url is empty",
				             cep->file->filename, cep->line_number,
				             CONF_EMOJI_BLOCK);
				errors++;
				continue;
			}
			/* Spec: SHOULD use https.  We warn but don't refuse so
			 * operators can point at internal http endpoints during
			 * development. */
			if (strncmp(cep->value, "https://", 8) &&
			    strncmp(cep->value, "http://", 7))
			{
				config_error("%s:%i: %s::pack-url must be an http(s) URL",
				             cep->file->filename, cep->line_number,
				             CONF_EMOJI_BLOCK);
				errors++;
				continue;
			}
			if (seen_url)
			{
				config_error("%s:%i: %s::pack-url specified more than once",
				             cep->file->filename, cep->line_number,
				             CONF_EMOJI_BLOCK);
				errors++;
				continue;
			}
			seen_url = 1;
		}
		else
		{
			config_warn("%s:%i: unknown directive %s::%s (ignored)",
			            cep->file->filename, cep->line_number,
			            CONF_EMOJI_BLOCK, cep->name);
		}
	}

	*errs = errors;
	return errors ? -1 : 1;
}

static int emoji_configrun(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;

	if (type != CONFIG_MAIN)
		return 0;
	if (!ce || !ce->name || strcmp(ce->name, CONF_EMOJI_BLOCK))
		return 0;

	freeconf();

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!cep->name)
			continue;
		if (!strcasecmp(cep->name, "pack-url"))
			safe_strdup(cfg.pack_url, cep->value);
	}

	cfg.configured = 1;
	emoji_publish_isupport();
	return 1;
}
