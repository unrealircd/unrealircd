/*
 * Anti mixed UTF8 - a filter written by Bram Matthys ("Syzop").
 * Reported by Mr_Smoke in https://bugs.unrealircd.org/view.php?id=5163
 * Tested by PeGaSuS (The_Myth) with some of the most used spam lines.
 * Help with testing and fixing Cyrillic from 'i' <info@servx.org>
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

static void free_config(void);
static void init_config(void);
int antimixedutf8_config_test(ConfigFile *, ConfigEntry *, int, int *);
int antimixedutf8_config_run(ConfigFile *, ConfigEntry *, int);

#define SCRIPT_UNDEFINED	0
#define SCRIPT_LATIN		1
#define SCRIPT_CYRILLIC		2
#define SCRIPT_CJK		3
#define SCRIPT_HANGUL		4
#define SCRIPT_CANADIAN		5
#define SCRIPT_TELUGU		6

/**** the detection algorithm follows first, the module/config code is at the end ****/

/** Detect which script the current character is,
 * such as latin script or cyrillic script.
 * @retval See SCRIPT_*
 */
int detect_script(const char *t)
{
	/* Safety: as long as *t is never \0 then at worst
	 * the character after this will be \0 and since we
	 * only look at 2 characters (at most) at a time
	 * this will be safe.
	 */

	/* Currently we only detect cyrillic and call all the
	 * rest latin (which is not true). This can always
	 * be enhanced later.
	 */

	if ((t[0] == 0xd0) && (t[1] >= 0x80) && (t[1] <= 0xbf))
		return SCRIPT_CYRILLIC;
	else if ((t[0] == 0xd1) && (t[1] >= 0x80) && (t[1] <= 0xbf))
		return SCRIPT_CYRILLIC;
	else if ((t[0] == 0xd2) && (t[1] >= 0x80) && (t[1] <= 0xbf))
		return SCRIPT_CYRILLIC;
	else if ((t[0] == 0xd3) && (t[1] >= 0x80) && (t[1] <= 0xbf))
		return SCRIPT_CYRILLIC;

	if ((t[0] == 0xe4) && (t[1] >= 0xb8) && (t[1] <= 0xbf))
		return SCRIPT_CJK;
	else if ((t[0] >= 0xe5) && (t[0] <= 0xe9) && (t[1] >= 0x80) && (t[1] <= 0xbf))
		return SCRIPT_CJK;

	if ((t[0] == 0xea) && (t[1] >= 0xb0) && (t[1] <= 0xbf))
		return SCRIPT_HANGUL;
	else if ((t[0] >= 0xeb) && (t[0] <= 0xec) && (t[1] >= 0x80) && (t[1] <= 0xbf))
		return SCRIPT_HANGUL;
	else if ((t[0] == 0xed) && (t[1] >= 0x80) && (t[1] <= 0x9f))
		return SCRIPT_HANGUL;

	if ((t[0] == 0xe1) && (t[1] >= 0x90) && (t[1] <= 0x99))
		return SCRIPT_CANADIAN;

	if ((t[0] == 0xe0) && (t[1] >= 0xb0) && (t[1] <= 0xb1))
		return SCRIPT_TELUGU;

	if ((t[0] >= 'a') && (t[0] <= 'z'))
		return SCRIPT_LATIN;
	if ((t[0] >= 'A') && (t[0] <= 'Z'))
		return SCRIPT_LATIN;

	return SCRIPT_UNDEFINED;
}

/** Returns length of an (UTF8) character. May return <1 for error conditions.
 * Made by i <info@servx.org>
 */
static int utf8_charlen(const char *str)
{
	struct { char mask; char val; } t[4] =
	{ { 0x80, 0x00 }, { 0xE0, 0xC0 }, { 0xF0, 0xE0 }, { 0xF8, 0xF0 } };
	unsigned k, j;

	for (k = 0; k < 4; k++)
	{
		if ((*str & t[k].mask) == t[k].val)
		{
			for (j = 0; j < k; j++)
			{
				if ((*(++str) & 0xC0) != 0x80)
					return -1;
			}
			return k + 1;
		}
	}
	return 1;
}

int lookalikespam_score(const char *text)
{
	const char *p;
	int last_script = SCRIPT_UNDEFINED;
	int current_script;
	int points = 0;
	int last_character_was_word_separator = 0;
	int skip = 0;

	for (p = text; *p; p++)
	{
		current_script = detect_script(p);

		if (current_script != SCRIPT_UNDEFINED)
		{
			if ((current_script != last_script) && (last_script != SCRIPT_UNDEFINED))
			{
				/* A script change = 1 point */
				points++;

				/* Give an additional point if the script change happened
				 * within the same word, as that would be rather unusual
				 * in normal cases.
				 */
				if (!last_character_was_word_separator)
					points++;
			}
			last_script = current_script;
		}

		if (strchr("., ", *p))
			last_character_was_word_separator = 1;
		else
			last_character_was_word_separator = 0;

		skip = utf8_charlen(p);
		if (skip > 1)
			p += skip - 1;
	}

	return points;
}

CMD_OVERRIDE_FUNC(override_msg)
{
	int score, retval;

	if (!MyUser(client) || (parc < 3) || BadPtr(parv[2]) ||
	    user_allowed_by_security_group(client, cfg.except))
	{
		/* Short circuit for: remote clients, insufficient parameters,
		 * antimixedutf8::except.
		 */
		CALL_NEXT_COMMAND_OVERRIDE();
		return;
	}

	score = lookalikespam_score(StripControlCodes(parv[2]));
	if ((score >= cfg.score) && !find_tkl_exception(TKL_ANTIMIXEDUTF8, client))
	{
		unreal_log(ULOG_INFO, "antimixedutf8", "ANTIMIXEDUTF8_HIT", client,
		           "[antimixedutf8] Client $client.details hit score $score -- taking action",
		           log_data_integer("score", score));
		/* Take the action */
		retval = take_action(client, cfg.ban_action, cfg.ban_reason, cfg.ban_time, 0, NULL);
		if ((retval == BAN_ACT_WARN) || (retval == BAN_ACT_SOFT_WARN))
		{
			/* no action */
		} else
		if ((retval == BAN_ACT_BLOCK) || (retval == BAN_ACT_SOFT_BLOCK))
		{
			sendnotice(client, "%s", cfg.ban_reason);
			return;
		} else if (retval > 0)
		{
			return;
		}
		/* fallthrough for retval <=0 */
	}

	CALL_NEXT_COMMAND_OVERRIDE();
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
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, antimixedutf8_config_run);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	if (!CommandOverrideAdd(modinfo->handle, "PRIVMSG", 0, override_msg))
		return MOD_FAILED;

	if (!CommandOverrideAdd(modinfo->handle, "NOTICE", 0, override_msg))
		return MOD_FAILED;

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
		if (!strcmp(cep->name, "except"))
		{
			test_match_block(cf, cep, &errors);
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
