/* src/modules/history_backend_null.c - History Backend: null / none
 * (C) Copyright 2019 Bram Matthys (Syzop) and the UnrealIRCd team
 * License: GPLv2
 */
#include "unrealircd.h"

/* This is the null backend type. It does not store anything at all.
 * This can be useful on a hub server where you don't need channel
 * history but still need to have a backend loaded to use the
 * channel mode +H.
 */

ModuleHeader MOD_HEADER
= {
	"history_backend_null",
	"1.0",
	"History backend: null/none",
	"UnrealIRCd Team",
	"unrealircd-5",
};

/* Forward declarations */
int hbn_history_add(char *object, MessageTag *mtags, char *line);
int hbn_history_del(char *object, int max_lines, long max_time);
int hbn_history_request(Client *client, char *object, HistoryFilter *filter);
int hbn_history_destroy(char *object);

MOD_INIT()
{
	HistoryBackendInfo hbi;

	MARK_AS_OFFICIAL_MODULE(modinfo);
	ModuleSetOptions(modinfo->handle, MOD_OPT_PERM, 1);

	memset(&hbi, 0, sizeof(hbi));
	hbi.name = "mem";
	hbi.history_add = hbn_history_add;
	hbi.history_del = hbn_history_del;
	hbi.history_request = hbn_history_request;
	hbi.history_destroy = hbn_history_destroy;
	if (!HistoryBackendAdd(modinfo->handle, &hbi))
		return MOD_FAILED;

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

int hbn_history_add(char *object, MessageTag *mtags, char *line)
{
	return 1;
}

int hbn_history_request(Client *client, char *object, HistoryFilter *filter)
{
	return 0;
}

int hbn_history_del(char *object, int max_lines, long max_time)
{
	return 1;
}

int hbn_history_destroy(char *object)
{
	return 1;
}
