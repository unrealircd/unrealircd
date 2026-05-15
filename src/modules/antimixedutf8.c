/*
 * Anti mixed UTF8 - a filter written by Bram Matthys ("Syzop").
 * Reported by Mr_Smoke in https://bugs.unrealircd.org/view.php?id=5163
 * Tested by PeGaSuS (The_Myth) with some of the most used spam lines.
 * Help with testing and fixing Cyrillic from 'i' <info@servx.org>
 * In 2025 a major overhaul, with a lot of the detection code moved
 * to generic text analysis in src/modules/utf8functions.c (and
 * no longer in the file you are viewing right now).
 *
 * ==[ ABOUT ]==
 * This module will detect and stop spam containing of characters of
 * mixed "scripts", where some characters are in Latin script and other
 * characters are in Cyrillic.
 * This unusual behavior can be detected easily and action can be taken.
 *
 * ==[ MODULE LOADING AND CONFIGURATION ]==
 * loadmodule "antimixedutf8";
 * set {
 *         antimixedutf8 {
 *                 score 10;
 *                 ban-action block;
 *                 ban-reason "Possible mixed character spam";
 *                 ban-time 4h; // For other types
 *                 except {
 *                 }
 *         };
 * };
 *
 * ==[ LICENSE AND PORTING ]==
 * Feel free to copy/move the idea or code to other IRCds.
 * The license is GPLv1 (or later, at your option):
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

ModuleHeader MOD_HEADER
= {
	"antimixedutf8",
	"1.0",
	"Mixed UTF8 character filter (look-alike character spam) - by Syzop",
	"UnrealIRCd Team",
	"unrealircd-6",
};

struct {
	int score;
	BanAction *ban_action;
	char *ban_reason;
	long ban_time;
	SecurityGroup *except;
} cfg;

/* Forward declarations */
static void free_config(void);
static void init_config(void);
int antimixedutf8_config_test(ConfigFile *, ConfigEntry *, int, int *);
int antimixedutf8_config_run(ConfigFile *, ConfigEntry *, int);
int stripcolor_can_send_to_channel(Client *client, Channel *channel, Membership *lp, const char **msg, const char **errmsg, SendType sendtype, ClientContext *clictx);
int antimixedutf8_can_send_to_user(Client *client, Client *target, const char **text, const char **errmsg, SendType sendtype, ClientContext *clictx);

int antimixedutf8_check(Client *client, TextAnalysis *txa, const char **errmsg)
{
	int score, retval;

	if (!txa || !MyUser(client) || user_allowed_by_security_group(client, cfg.except))
		return HOOK_CONTINUE;

	if ((txa->antimixedutf8_points >= cfg.score) && !find_tkl_exception(TKL_ANTIMIXEDUTF8, client))
	{
		unreal_log(ULOG_INFO, "antimixedutf8", "ANTIMIXEDUTF8_HIT", client,
		           "[antimixedutf8] Client $client.details hit score $score -- taking action",
		           log_data_integer("score", txa->antimixedutf8_points),
		           log_data_textanalysis("text_analysis",txa));
		/* Take the action */
		retval = take_action(client, cfg.ban_action, cfg.ban_reason, cfg.ban_time, 0, NULL);
		if ((retval == BAN_ACT_WARN) || (retval == BAN_ACT_SOFT_WARN))
		{
			/* no action */
		} else
		if ((retval == BAN_ACT_BLOCK) || (retval == BAN_ACT_SOFT_BLOCK))
		{
			*errmsg = cfg.ban_reason;
			return HOOK_DENY;
		} else if (retval > 0)
		{
			/* TODO: verify this works correctly with like kill/gline/etc */
			*errmsg = cfg.ban_reason;
			return HOOK_DENY;
		}
		/* fallthrough for retval <=0 */
	}

	return HOOK_CONTINUE;
}

int antimixedutf8_can_send_to_channel(Client *client, Channel *channel, Membership *lp, const char **msg, const char **errmsg, SendType sendtype, ClientContext *clictx)
{
	return antimixedutf8_check(client, clictx->textanalysis, errmsg);
}

int antimixedutf8_can_send_to_user(Client *client, Client *target, const char **text, const char **errmsg, SendType sendtype, ClientContext *clictx)
{
	return antimixedutf8_check(client, clictx->textanalysis, errmsg);
}

/*** rest is module and config stuff ****/

MOD_TEST()
{
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, antimixedutf8_config_test);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);

	init_config();
	HookAdd(modinfo->handle, HOOKTYPE_CAN_SEND_TO_CHANNEL, 0, antimixedutf8_can_send_to_channel);
	HookAdd(modinfo->handle, HOOKTYPE_CAN_SEND_TO_USER, 0, antimixedutf8_can_send_to_user);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, antimixedutf8_config_run);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	free_config();
	return MOD_SUCCESS;
}

static void init_config(void)
{
	memset(&cfg, 0, sizeof(cfg));
	/* Default values */
	cfg.score = 10;
	safe_strdup(cfg.ban_reason, "Possible mixed character spam");
	cfg.ban_action = banact_value_to_struct(BAN_ACT_BLOCK);
	cfg.ban_time = 60 * 60 * 4; /* irrelevant for block, but some default for others */
}

static void free_config(void)
{
	safe_free(cfg.ban_reason);
	free_security_group(cfg.except);
	safe_free_all_ban_actions(cfg.ban_action);
	memset(&cfg, 0, sizeof(cfg)); /* needed! */
}

int antimixedutf8_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep;

	if (type != CONFIG_SET)
		return 0;

	/* We are only interrested in set::antimixedutf8... */
	if (!ce || !ce->name || strcmp(ce->name, "antimixedutf8"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "except"))
		{
			test_match_block(cf, cep, &errors);
		} else
		if (!cep->value)
		{
			config_error("%s:%i: set::antimixedutf8::%s with no value",
				cep->file->filename, cep->line_number, cep->name);
			errors++;
		} else
		if (!strcmp(cep->name, "score"))
		{
			int v = atoi(cep->value);
			if ((v < 1) || (v > 99))
			{
				config_error("%s:%i: set::antimixedutf8::score: must be between 1 - 99 (got: %d)",
					cep->file->filename, cep->line_number, v);
				errors++;
			}
		} else
		if (!strcmp(cep->name, "ban-action"))
		{
			errors += test_ban_action_config(cep);
		} else
		if (!strcmp(cep->name, "ban-reason"))
		{
		} else
		if (!strcmp(cep->name, "ban-time"))
		{
		} else
		{
			config_error("%s:%i: unknown directive set::antimixedutf8::%s",
				cep->file->filename, cep->line_number, cep->name);
			errors++;
		}
	}
	*errs = errors;
	return errors ? -1 : 1;
}

int antimixedutf8_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;

	if (type != CONFIG_SET)
		return 0;

	/* We are only interrested in set::antimixedutf8... */
	if (!ce || !ce->name || strcmp(ce->name, "antimixedutf8"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "score"))
		{
			cfg.score = atoi(cep->value);
		} else
		if (!strcmp(cep->name, "ban-action"))
		{
			parse_ban_action_config(cep, &cfg.ban_action);
		} else
		if (!strcmp(cep->name, "ban-reason"))
		{
			safe_strdup(cfg.ban_reason, cep->value);
		} else
		if (!strcmp(cep->name, "ban-time"))
		{
			cfg.ban_time = config_checkval(cep->value, CFG_TIME);
		} else
		if (!strcmp(cep->name, "except"))
		{
			conf_match_block(cf, cep, &cfg.except);
		}
	}
	return 1;
}
