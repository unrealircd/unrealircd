/* src/modules/chathistory.c - IRCv3 CHATHISTORY command.
 * (C) Copyright 2021 Bram Matthys (Syzop) and the UnrealIRCd team
 * License: GPLv2 or later
 *
 * This implements the "CHATHISTORY" command, the CAP and 005 token.
 * https://ircv3.net/specs/extensions/chathistory
 */
#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"chathistory",
	"1.0",
	"IRCv3 CHATHISTORY command",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* Structs */
typedef struct ChatHistoryTarget ChatHistoryTarget;
struct ChatHistoryTarget {
	ChatHistoryTarget *prev, *next;
	char *datetime;
	char *object;
};

/* Forward declarations */
CMD_FUNC(cmd_chathistory);

/* Global variables */
long CAP_CHATHISTORY = 0L;

#define CHATHISTORY_LIMIT 50

MOD_INIT()
{
	ClientCapabilityInfo c;

	MARK_AS_OFFICIAL_MODULE(modinfo);
	CommandAdd(modinfo->handle, "CHATHISTORY", cmd_chathistory, MAXPARA, CMD_USER);

	memset(&c, 0, sizeof(c));
	c.name = "draft/chathistory";
	ClientCapabilityAdd(modinfo->handle, &c, &CAP_CHATHISTORY);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	ISupportSetFmt(modinfo->handle, "CHATHISTORY", "%d", CHATHISTORY_LIMIT);
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

int chathistory_token(const char *str, char *token, char **store)
{
	char request[BUFSIZE];
	char *p;

	strlcpy(request, str, sizeof(request));

	p = strchr(request, '=');
	if (!p)
		return 0;
	*p = '\0'; // frag
	if (!strcmp(request, token))
	{
		*p = '='; // restore
		*store = strdup(p + 1); // can be \0
		return 1;
	}
	*p = '='; // restore
	return 0;
}

static void add_chathistory_target_list(ChatHistoryTarget *new, ChatHistoryTarget **list)
{
	ChatHistoryTarget *x, *last = NULL;

	if (!*list)
	{
		/* We are the only item. Easy. */
		*list = new;
		return;
	}

	for (x = *list; x; x = x->next)
	{
		last = x;
		if (strcmp(new->datetime, x->datetime) >= 0)
			break;
	}

	if (x)
	{
		if (x->prev)
		{
			/* We will insert ourselves just before this item */
			new->prev = x->prev;
			new->next = x;
			x->prev->next = new;
			x->prev = new;
		} else {
			/* We are the new head */
			*list = new;
			new->next = x;
			x->prev = new;
		}
	} else
	{
		/* We are the last item */
		last->next = new;
		new->prev = last;
	}
}

static void add_chathistory_target(ChatHistoryTarget **list, HistoryResult *r)
{
	MessageTag *m;
	time_t ts;
	char *datetime;
	ChatHistoryTarget *e;

	if (!r->log || !((m = find_mtag(r->log->mtags, "time"))) || !m->value)
		return;
	datetime = m->value;

	e = safe_alloc(sizeof(ChatHistoryTarget));
	safe_strdup(e->datetime, datetime);
	safe_strdup(e->object, r->object);
	add_chathistory_target_list(e, list);
}

static void chathistory_targets_send_line(Client *client, ChatHistoryTarget *r, char *batchid)
{
	MessageTag *mtags = NULL;
	MessageTag *m;

	if (!BadPtr(batchid))
	{
		mtags = safe_alloc(sizeof(MessageTag));
		mtags->name = strdup("batch");
		mtags->value = strdup(batchid);
	}

	sendto_one(client, mtags, ":%s CHATHISTORY TARGETS %s %s",
		me.name, r->object, r->datetime);

	if (mtags)
		free_message_tags(mtags);
}

void chathistory_targets(Client *client, HistoryFilter *filter, int limit)
{
	Membership *mp;
	HistoryResult *r;
	char batch[BATCHLEN+1];
	int sent = 0;
	ChatHistoryTarget *targets = NULL, *targets_next;

	/* 1. Grab all information we need */

	filter->cmd = HFC_BEFORE;
	if (strcmp(filter->timestamp_a, filter->timestamp_b) < 0)
	{
		/* Swap if needed */
		char *swap = filter->timestamp_a;
		filter->timestamp_a = filter->timestamp_b;
		filter->timestamp_b = swap;
	}
	filter->limit = 1;

	for (mp = client->user->channel; mp; mp = mp->next)
	{
		Channel *channel = mp->channel;
		r = history_request(channel->name, filter);
		if (r)
		{
			add_chathistory_target(&targets, r);
			free_history_result(r);
		}
	}

	/* 2. Now send it to the client */

	batch[0] = '\0';
	if (HasCapability(client, "batch"))
	{
		/* Start a new batch */
		generate_batch_id(batch);
		sendto_one(client, NULL, ":%s BATCH +%s draft/chathistory-targets", me.name, batch);
	}

	for (; targets; targets = targets_next)
	{
		targets_next = targets->next;
		if (++sent < limit)
			chathistory_targets_send_line(client, targets, batch);
		safe_free(targets->datetime);
		safe_free(targets->object);
		safe_free(targets);
	}

	/* End of batch */
	if (*batch)
		sendto_one(client, NULL, ":%s BATCH -%s", me.name, batch);
}

void send_empty_batch(Client *client, const char *target)
{
	char batch[BATCHLEN+1];

	if (HasCapability(client, "batch"))
	{
		generate_batch_id(batch);
		sendto_one(client, NULL, ":%s BATCH +%s chathistory %s", me.name, batch, target);
		sendto_one(client, NULL, ":%s BATCH -%s", me.name, batch);
	}
}

CMD_FUNC(cmd_chathistory)
{
	HistoryFilter *filter = NULL;
	HistoryResult *r = NULL;
	Channel *channel;

	memset(&filter, 0, sizeof(filter));

	/* This command is only for local users */
	if (!MyUser(client))
		return;

	if ((parc < 5) || BadPtr(parv[4]))
	{
		sendto_one(client, NULL, ":%s FAIL CHATHISTORY INVALID_PARAMS :Insufficient parameters", me.name);
		return;
	}

	if (!HasCapability(client, "server-time"))
	{
		sendnotice(client, "Your IRC client does not support the 'server-time' capability");
		sendnotice(client, "https://ircv3.net/specs/extensions/server-time");
		sendnotice(client, "History request refused.");
		return;
	}

	if (!strcasecmp(parv[1], "TARGETS"))
	{
		Membership *mp;
		int limit;

		filter = safe_alloc(sizeof(HistoryFilter));
		/* Below this point, instead of 'return', use 'goto end' */

		if (!chathistory_token(parv[2], "timestamp", &filter->timestamp_a))
		{
			sendto_one(client, NULL, ":%s FAIL CHATHISTORY INVALID_PARAMS %s %s :Invalid parameter, must be timestamp=xxx",
				me.name, parv[1], parv[3]);
			goto end;
		}
		if (!chathistory_token(parv[3], "timestamp", &filter->timestamp_b))
		{
			sendto_one(client, NULL, ":%s FAIL CHATHISTORY INVALID_PARAMS %s %s :Invalid parameter, must be timestamp=xxx",
				me.name, parv[1], parv[4]);
			goto end;
		}
		limit = atoi(parv[4]);
		chathistory_targets(client, filter, limit);
		goto end;
	}

	/* We don't support retrieving chathistory for PM's. Send empty response/batch, similar to channels without +H. */
	if (parv[2][0] != '#')
	{
		send_empty_batch(client, parv[2]);
		return;
	}

	channel = find_channel(parv[2]);
	if (!channel)
	{
		sendto_one(client, NULL, ":%s FAIL CHATHISTORY INVALID_TARGET %s %s :Messages could not be retrieved, not an existing channel",
			me.name, parv[1], parv[2]);
		return;
	}

	if (!IsMember(client, channel))
	{
		sendto_one(client, NULL, ":%s FAIL CHATHISTORY INVALID_TARGET %s %s :Messages could not be retrieved, you are not a member",
			me.name, parv[1], parv[2]);
		return;
	}

	/* Channel is not +H? Send empty response/batch (as per IRCv3 discussion) */
	if (!has_channel_mode(channel, 'H'))
	{
		send_empty_batch(client, channel->name);
		return;
	}

	filter = safe_alloc(sizeof(HistoryFilter));
	/* Below this point, instead of 'return', use 'goto end', which takes care of the freeing of 'filter' and 'history' */

	if (!strcasecmp(parv[1], "BEFORE"))
	{
		filter->cmd = HFC_BEFORE;
		if (!chathistory_token(parv[3], "timestamp", &filter->timestamp_a) &&
		    !chathistory_token(parv[3], "msgid", &filter->msgid_a))
		{
			sendto_one(client, NULL, ":%s FAIL CHATHISTORY INVALID_PARAMS %s %s :Invalid parameter, must be timestamp=xxx or msgid=xxx",
				me.name, parv[1], parv[3]);
			goto end;
		}
		filter->limit = atoi(parv[4]);
	} else
	if (!strcasecmp(parv[1], "AFTER"))
	{
		filter->cmd = HFC_AFTER;
		if (!chathistory_token(parv[3], "timestamp", &filter->timestamp_a) &&
		    !chathistory_token(parv[3], "msgid", &filter->msgid_a))
		{
			sendto_one(client, NULL, ":%s FAIL CHATHISTORY INVALID_PARAMS %s %s :Invalid parameter, must be timestamp=xxx or msgid=xxx",
				me.name, parv[1], parv[3]);
			goto end;
		}
		filter->limit = atoi(parv[4]);
	} else
	if (!strcasecmp(parv[1], "LATEST"))
	{
		filter->cmd = HFC_LATEST;
		if (!chathistory_token(parv[3], "timestamp", &filter->timestamp_a) &&
		    !chathistory_token(parv[3], "msgid", &filter->msgid_a) &&
		    strcmp(parv[3], "*"))
		{
			sendto_one(client, NULL, ":%s FAIL CHATHISTORY INVALID_PARAMS %s %s :Invalid parameter, must be timestamp=xxx or msgid=xxx or *",
				me.name, parv[1], parv[3]);
			goto end;
		}
		filter->limit = atoi(parv[4]);
	} else
	if (!strcasecmp(parv[1], "AROUND"))
	{
		filter->cmd = HFC_AROUND;
		if (!chathistory_token(parv[3], "timestamp", &filter->timestamp_a) &&
		    !chathistory_token(parv[3], "msgid", &filter->msgid_a))
		{
			sendto_one(client, NULL, ":%s FAIL CHATHISTORY INVALID_PARAMS %s %s :Invalid parameter, must be timestamp=xxx or msgid=xxx",
				me.name, parv[1], parv[3]);
			goto end;
		}
		filter->limit = atoi(parv[4]);
	} else
	if (!strcasecmp(parv[1], "BETWEEN"))
	{
		filter->cmd = HFC_BETWEEN;
		if (BadPtr(parv[5]))
		{
			sendto_one(client, NULL, ":%s FAIL CHATHISTORY INVALID_PARAMS %s :Insufficient parameters", parv[1], me.name);
			goto end;
		}
		if (!chathistory_token(parv[3], "timestamp", &filter->timestamp_a) &&
		    !chathistory_token(parv[3], "msgid", &filter->msgid_a))
		{
			sendto_one(client, NULL, ":%s FAIL CHATHISTORY INVALID_PARAMS %s %s :Invalid parameter, must be timestamp=xxx or msgid=xxx",
				me.name, parv[1], parv[3]);
			goto end;
		}
		if (!chathistory_token(parv[4], "timestamp", &filter->timestamp_b) &&
		    !chathistory_token(parv[4], "msgid", &filter->msgid_b))
		{
			sendto_one(client, NULL, ":%s FAIL CHATHISTORY INVALID_PARAMS %s %s :Invalid parameter, must be timestamp=xxx or msgid=xxx",
				me.name, parv[1], parv[4]);
			goto end;
		}
		filter->limit = atoi(parv[5]);
	} else {
		sendto_one(client, NULL, ":%s FAIL CHATHISTORY INVALID_PARAMS %s :Invalid subcommand", me.name, parv[1]);
		goto end;
	}

	if (filter->limit <= 0)
	{
		sendto_one(client, NULL, ":%s FAIL CHATHISTORY INVALID_PARAMS %s %d :Specified limit is =<0",
			me.name, parv[1], filter->limit);
		goto end;
	}

	if (filter->limit > CHATHISTORY_LIMIT)
		filter->limit = CHATHISTORY_LIMIT;

	if ((r = history_request(channel->name, filter)))
		history_send_result(client, r);

end:
	if (filter)
		free_history_filter(filter);
	if (r)
		free_history_result(r);
}
