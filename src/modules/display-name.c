/*
 *   IRC - Internet Relay Chat, src/modules/display-name.c
 *   (C) 2020 Syzop & The UnrealIRCd Team
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
    "display-name",
    "5.0",
    "+display-name client tag",
    "UnrealIRCd Team",
    "unrealircd-5",
    };

int dn_mtag_is_ok(Client *client, char *name, char *value);
void mtag_add_dn(Client *client, MessageTag *recv_mtags, MessageTag **mtag_list, char *signature);

MOD_INIT()
{
    MessageTagHandlerInfo mtag;

    MARK_AS_OFFICIAL_MODULE(modinfo);

    memset(&mtag, 0, sizeof(mtag));
    mtag.name = "+draft/display-name";
    mtag.is_ok = dn_mtag_is_ok;
    mtag.flags = MTAG_HANDLER_FLAGS_NO_CAP_NEEDED;
    MessageTagHandlerAdd(modinfo->handle, &mtag);

    HookAddVoid(modinfo->handle, HOOKTYPE_NEW_MESSAGE, 0, mtag_add_dn);

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

/** This function verifies if the client sending the mtag is permitted to do so.
 */
int dn_mtag_is_ok(Client *client, char *name, char *value)
{
    /* Require a non-empty parameter */
    if (BadPtr(value))
        return 0;

    /* All values are allowed */
    return 1;
}

void mtag_add_dn(Client *client, MessageTag *recv_mtags, MessageTag **mtag_list, char *signature)
{
    MessageTag *m;

    if (IsUser(client))
    {
        m = find_mtag(recv_mtags, "+draft/display-name");
        if (m)
        {
            m = duplicate_mtag(m);
            AddListItem(m, *mtag_list);
        }
    }
}