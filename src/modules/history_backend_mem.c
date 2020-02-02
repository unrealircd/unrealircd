/* src/modules/history_backend_mem.c - History Backend: memory
 * (C) Copyright 2019 Bram Matthys (Syzop) and the UnrealIRCd team
 * License: GPLv2
 */
#include "unrealircd.h"

/* This is the memory type backend. It is optimized for speed.
 * For example, per-channel, it caches the field "number of lines"
 * and "oldest record", so frequent cleaning operations such as
 * "delete any record older than time T" or "keep only N lines"
 * are executed as fast as possible.
 */

ModuleHeader MOD_HEADER
= {
	"history_backend_mem",
	"2.0",
	"History backend: memory",
	"UnrealIRCd Team",
	"unrealircd-5",
};

/* Defines */
#define OBJECTLEN	((NICKLEN > CHANNELLEN) ? NICKLEN : CHANNELLEN)
#define HISTORY_BACKEND_MEM_HASH_TABLE_SIZE 1019

/* The regular history cleaning (by timer) is spread out
 * a bit, rather than doing ALL channels every T time.
 * HISTORY_SPREAD: how much to spread the "cleaning", eg 1 would be
 *  to clean everything in 1 go, 2 would mean the first event would
 *  clean half of the channels, and the 2nd event would clean the rest.
 *  Obviously more = better to spread the load, but doing a reasonable
 *  amount of work is also benefitial for performance (think: CPU cache).
 * HISTORY_MAX_OFF_SECS: how many seconds may the history be 'off',
 *  that is: how much may we store the history longer than required.
 * The other 2 macros are calculated based on that target.
 */
#define HISTORY_SPREAD	16
#define HISTORY_MAX_OFF_SECS	128
#define HISTORY_CLEAN_PER_LOOP	(HISTORY_BACKEND_MEM_HASH_TABLE_SIZE/HISTORY_SPREAD)
#define HISTORY_TIMER_EVERY	(HISTORY_MAX_OFF_SECS/HISTORY_SPREAD)

/* Definitions (structs, etc.) */
typedef struct HistoryLogLine HistoryLogLine;
struct HistoryLogLine {
	HistoryLogLine *prev, *next;
	time_t t;
	MessageTag *mtags;
	char line[1];
};

typedef struct HistoryLogObject HistoryLogObject;
struct HistoryLogObject {
	HistoryLogObject *prev, *next;
	HistoryLogLine *head; /**< Start of the log (the earliest entry) */
	HistoryLogLine *tail; /**< End of the log (the latest entry) */
	int num_lines; /**< Number of lines of log */
	time_t oldest_t; /**< Oldest time in log */
	int max_lines; /**< Maximum number of lines permitted */
	long max_time; /**< Maximum number of seconds to retain history */
	char name[OBJECTLEN+1];
};

/* Global variables */
static char siphashkey_history_backend_mem[SIPHASH_KEY_LENGTH];
HistoryLogObject *history_hash_table[HISTORY_BACKEND_MEM_HASH_TABLE_SIZE];

/* Forward declarations */
int hbm_history_add(char *object, MessageTag *mtags, char *line);
int hbm_history_cleanup(HistoryLogObject *h);
int hbm_history_request(Client *client, char *object, HistoryFilter *filter);
int hbm_history_destroy(char *object);
int hbm_history_set_limit(char *object, int max_lines, long max_time);
EVENT(history_mem_clean);

MOD_INIT()
{
	HistoryBackendInfo hbi;

	MARK_AS_OFFICIAL_MODULE(modinfo);
	ModuleSetOptions(modinfo->handle, MOD_OPT_PERM, 1);

	memset(&history_hash_table, 0, sizeof(history_hash_table));
	siphash_generate_key(siphashkey_history_backend_mem);

	memset(&hbi, 0, sizeof(hbi));
	hbi.name = "mem";
	hbi.history_add = hbm_history_add;
	hbi.history_request = hbm_history_request;
	hbi.history_destroy = hbm_history_destroy;
	hbi.history_set_limit = hbm_history_set_limit;
	if (!HistoryBackendAdd(modinfo->handle, &hbi))
		return MOD_FAILED;

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	EventAdd(modinfo->handle, "history_mem_clean", history_mem_clean, NULL, HISTORY_TIMER_EVERY*1000, 0);
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

uint64_t hbm_hash(char *object)
{
	return siphash_nocase(object, siphashkey_history_backend_mem) % HISTORY_BACKEND_MEM_HASH_TABLE_SIZE;
}

HistoryLogObject *hbm_find_object(char *object)
{
	int hashv = hbm_hash(object);
	HistoryLogObject *h;

	for (h = history_hash_table[hashv]; h; h = h->next)
	{
		if (!strcasecmp(object, h->name))
			return h;
	}
	return NULL;
}

HistoryLogObject *hbm_find_or_add_object(char *object)
{
	int hashv = hbm_hash(object);
	HistoryLogObject *h;

	for (h = history_hash_table[hashv]; h; h = h->next)
	{
		if (!strcasecmp(object, h->name))
			return h;
	}
	/* Create new one */
	h = safe_alloc(sizeof(HistoryLogObject));
	strlcpy(h->name, object, sizeof(h->name));
	AddListItem(h, history_hash_table[hashv]);
	return h;
}

void hbm_delete_object_hlo(HistoryLogObject *h)
{
	int hashv = hbm_hash(h->name);

	DelListItem(h, history_hash_table[hashv]);
	safe_free(h);
}

void hbm_duplicate_mtags(HistoryLogLine *l, MessageTag *m)
{
	MessageTag *n;

	/* Duplicate all message tags */
	for (; m; m = m->next)
	{
		n = duplicate_mtag(m);
		AppendListItem(n, l->mtags);
	}
	n = find_mtag(l->mtags, "time");
	if (!n)
	{
		/* This is duplicate code from src/modules/server-time.c
		 * which seems silly.
		 */
		struct timeval t;
		struct tm *tm;
		time_t sec;
		char buf[64];

		gettimeofday(&t, NULL);
		sec = t.tv_sec;
		tm = gmtime(&sec);
		snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
			tm->tm_year + 1900,
			tm->tm_mon + 1,
			tm->tm_mday,
			tm->tm_hour,
			tm->tm_min,
			tm->tm_sec,
			(int)(t.tv_usec / 1000));

		n = safe_alloc(sizeof(MessageTag));
		safe_strdup(n->name, "time");
		safe_strdup(n->value, buf);
		AddListItem(n, l->mtags);
	}
	/* Now convert the "time" message tag to something we can use in l->t */
	l->t = server_time_to_unix_time(n->value);
}

/** Add a line to a history object */
void hbm_history_add_line(HistoryLogObject *h, MessageTag *mtags, char *line)
{
	HistoryLogLine *l = safe_alloc(sizeof(HistoryLogLine) + strlen(line));
	strcpy(l->line, line); /* safe, see memory allocation above ^ */
	hbm_duplicate_mtags(l, mtags);
	if (h->tail)
	{
		/* append to tail */
		h->tail->next = l;
		l->prev = h->tail;
		h->tail = l;
	} else {
		/* no tail, no head */
		h->head = h->tail = l;
	}
	h->num_lines++;
	if ((l->t < h->oldest_t) || (h->oldest_t == 0))
		h->oldest_t = l->t;
}

/** Delete a line from a history object */
void hbm_history_del_line(HistoryLogObject *h, HistoryLogLine *l)
{
	if (l->prev)
		l->prev->next = l->next;
	if (l->next)
		l->next->prev = l->prev;
	if (h->head == l)
	{
		/* New head */
		h->head = l->next;
	}
	if (h->tail == l)
	{
		/* New tail */
		h->tail = l->prev; /* could be NULL now */
	}

	free_message_tags(l->mtags);
	safe_free(l);

	h->num_lines--;

	/* IMPORTANT: updating h->oldest_t takes place at the caller
	 * because it is in a better position to optimize the process
	 */
}

/** Add history entry */
int hbm_history_add(char *object, MessageTag *mtags, char *line)
{
	HistoryLogObject *h = hbm_find_or_add_object(object);
	if (!h->max_lines)
	{
		sendto_realops("hbm_history_add() for '%s', which has no limit", h->name);
#ifdef DEBUGMODE
		abort();
#else
		h->max_lines = 50;
		h->max_time = 86400;
#endif
	}
	if (h->num_lines >= h->max_lines)
	{
		/* Delete previous line */
		hbm_history_del_line(h, h->head);
	}
	hbm_history_add_line(h, mtags, line);
	return 0;
}

int can_receive_history(Client *client)
{
	if (HasCapability(client, "server-time"))
		return 1;
	return 0;
}

void hbm_send_line(Client *client, HistoryLogLine *l, char *batchid)
{
	if (can_receive_history(client))
	{
		if (BadPtr(batchid))
		{
			sendto_one(client, l->mtags, "%s", l->line);
		} else {
			MessageTag *m = safe_alloc(sizeof(MessageTag));
			m->name = "batch";
			m->value = batchid;
			AddListItem(m, l->mtags);
			sendto_one(client, l->mtags, "%s", l->line);
			DelListItem(m, l->mtags);
			safe_free(m);
		}
	} else {
		/* without server-time, log playback is a bit annoying, so skip it? */
	}
}

int hbm_history_request(Client *client, char *object, HistoryFilter *filter)
{
	HistoryLogObject *h = hbm_find_object(object);
	HistoryLogLine *l;
	char batch[BATCHLEN+1];
	long redline; /* Imaginary timestamp. Before the red line, history is too old. */
	int lines_sendable = 0, lines_to_skip = 0, cnt = 0;

	if (!h || !can_receive_history(client))
		return 0;

	batch[0] = '\0';

	if (HasCapability(client, "batch"))
	{
		/* Start a new batch */
		generate_batch_id(batch);
		sendto_one(client, NULL, ":%s BATCH +%s chathistory %s", me.name, batch, object);
	}

	redline = TStime() - h->max_time;

	/* Once the filter API expands, the following will change too.
	 * For now, this is sufficient, since requests are only about lines:
	 */
	lines_sendable = 0;
	for (l = h->head; l; l = l->next)
		if (l->t >= redline)
			lines_sendable++;
	if (filter && (lines_sendable > filter->last_lines))
		lines_to_skip = lines_sendable - filter->last_lines;

	for (l = h->head; l; l = l->next)
	{
		/* Make sure we don't send too old entries:
		 * We only have to check for time here, as line count is already
		 * taken into account in hbm_history_add.
		 */
		if (l->t >= redline && (++cnt > lines_to_skip))
			hbm_send_line(client, l, batch);
	}

	/* End of batch */
	if (*batch)
		sendto_one(client, NULL, ":%s BATCH -%s", me.name, batch);
	return 1;
}

/** Clean up expired entries */
int hbm_history_cleanup(HistoryLogObject *h)
{
	HistoryLogLine *l, *l_next = NULL;
	long redline = TStime() - h->max_time;

	/* First enforce 'h->max_time', after that enforce 'h->max_lines' */

	/* Checking for time */
	if (h->oldest_t < redline)
	{
		h->oldest_t = 0; /* recalculate in next loop */

		for (l = h->head; l; l = l_next)
		{
			l_next = l->next;
			if (l->t < redline)
			{
				hbm_history_del_line(h, l); /* too old, delete it */
				continue;
			}
			if ((h->oldest_t == 0) || (l->t < h->oldest_t))
				h->oldest_t = l->t;
		}
	}

	if (h->num_lines > h->max_lines)
	{
		h->oldest_t = 0; /* recalculate in next loop */

		for (l = h->head; l; l = l_next)
		{
			l_next = l->next;
			if (h->num_lines > h->max_lines)
			{
				hbm_history_del_line(h, l);
				continue;
			}
			if ((h->oldest_t == 0) || (l->t < h->oldest_t))
				h->oldest_t = l->t;
		}
	}

	return 1;
}

int hbm_history_destroy(char *object)
{
	HistoryLogObject *h = hbm_find_object(object);
	HistoryLogLine *l, *l_next;

	if (!h)
		return 0;

	for (l = h->head; l; l = l_next)
	{
		l_next = l->next;
		/* We could use hbm_history_del_line() here but
		 * it does unnecessary work, this is quicker.
		 * The only danger is that we may forget to free some
		 * fields that are added later there but not here.
		 */
		free_message_tags(l->mtags);
		safe_free(l);
	}

	hbm_delete_object_hlo(h);
	return 1;
}

/** Set new limit on history object */
int hbm_history_set_limit(char *object, int max_lines, long max_time)
{
	HistoryLogObject *h = hbm_find_or_add_object(object);
	h->max_lines = max_lines;
	h->max_time = max_time;
	hbm_history_cleanup(h); /* impose new restrictions */
	return 1;
}

/** Periodically clean the history.
 * Instead of doing all channels in 1 go, we do a limited number
 * of channels each call, hence the 'static int' and the do { } while
 * rather than a regular for loop.
 * Note that we already impose the line limit in hbm_history_add,
 * so this history_mem_clean is for removals due to max_time limits.
 */
EVENT(history_mem_clean)
{
	static int hashnum = 0;
	int loopcnt = 0;
	Channel *channel;
	HistoryLogObject *h;

	do
	{
		for (h = history_hash_table[hashnum++]; h; h = h->next)
			hbm_history_cleanup(h);

		hashnum++;

		if (hashnum >= HISTORY_BACKEND_MEM_HASH_TABLE_SIZE)
			hashnum = 0;
	} while(loopcnt++ < HISTORY_CLEAN_PER_LOOP);
}
