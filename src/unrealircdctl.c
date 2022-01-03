/************************************************************************
 *   UnrealIRCd - Unreal Internet Relay Chat Daemon - src/unrealircdctl
 *   (c) 2022- Bram Matthys and The UnrealIRCd team
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

/** @file
 * @brief UnrealIRCd Control
 */
#include "unrealircd.h"

extern int procio_client(const char *command, int auto_color_logs);

void unrealircdctl_usage(const char *program_name)
{
	printf("Usage: %s <option>\n"
	       "Where <option> is one of:\n"
	       "rehash      - Rehash the server (reread configuration files)\n"
	       "reloadtls   - Reload the SSL/TLS certificates\n"
	       "status      - Show current status of server\n"
	       "\n", program_name);
	exit(-1);
}

void unrealircdctl_rehash(void)
{
	if (procio_client("REHASH", 1) == 0)
	{
		printf("Rehashed succesfully.\n");
		exit(0);
	}
	printf("Rehash failed.\n");
	exit(1);
}

void unrealircdctl_reloadtls(void)
{
	if (procio_client("REHASH -tls", 1) == 0)
	{
		printf("Reloading of TLS certificates successful.\n");
		exit(0);
	}
	printf("Reloading TLS certificates failed.\n");
	exit(1);
}

void unrealircdctl_status(void)
{
	if (procio_client("STATUS", 2) == 0)
	{
		printf("UnrealIRCd is up and running.\n");
		exit(0);
	}
	printf("UnrealIRCd status report failed.\n");
	exit(1);
}

void unrealircdctl_mkpasswd(int argc, char *argv[])
{
	AuthenticationType type;
	const char *result;
	char *p = argv[2];

	srandom(TStime());
	type = Auth_FindType(NULL, p);
	if (type == -1)
	{
		type = AUTHTYPE_ARGON2;
	} else {
		p = argv[3];
	}
	if (BadPtr(p))
	{
#ifndef _WIN32
		p = getpass("Enter password to hash: ");
#else
		printf("ERROR: You should specify a password to hash");
		exit(1);
#endif
	}
	if ((type == AUTHTYPE_UNIXCRYPT) && (strlen(p) > 8))
	{
		/* Hmmm.. is this warning really still true (and always) ?? */
		printf("WARNING: Password truncated to 8 characters due to 'crypt' algorithm. "
		       "You are suggested to use the 'argon2' algorithm instead.");
		p[8] = '\0';
	}
	if (!(result = Auth_Hash(type, p))) {
		printf("Failed to generate password. Deprecated method? Try 'argon2' instead.\n");
		exit(0);
	}
	printf("Encrypted password is: %s\n", result);
	exit(0);
}

int main(int argc, char *argv[])
{
	dbuf_init();
#ifdef _WIN32
	init_winsock();
#endif

	if (argc == 1)
		unrealircdctl_usage(argv[0]);

	if (!strcmp(argv[1], "rehash"))
		unrealircdctl_rehash();
	else if (!strcmp(argv[1], "reloadtls"))
		unrealircdctl_reloadtls();
	else if (!strcmp(argv[1], "status"))
		unrealircdctl_status();
	else if (!strcmp(argv[1], "mkpasswd"))
		unrealircdctl_mkpasswd(argc, argv);
	else
		unrealircdctl_usage(argv[0]);
	exit(0);
}
