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

#ifdef _WIN32
 #define UNREALCMD "unrealircdctl"
#else
 #define UNREALCMD "./unrealircd"
#endif


extern int procio_client(const char *command, int auto_color_logs);

void unrealircdctl_usage(const char *program_name)
{
	printf("Usage: %s <option>\n"
	       "Where <option> is one of:\n"
	       "rehash         - Rehash the server (reread configuration files)\n"
	       "reloadtls      - Reload the SSL/TLS certificates\n"
	       "status         - Show current status of server\n"
	       "module-status  - Show currently loaded modules\n"
	       "mkpasswd       - Hash a password\n"
	       "gencloak       - Display 3 random cloak keys\n"
	       "spkifp         - Display SPKI Fingerprint\n"
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

void unrealircdctl_module_status(void)
{
	if (procio_client("MODULES", 2) == 0)
		exit(0);
	printf("Could not retrieve complete module list.\n");
	exit(1);
}

void unrealircdctl_mkpasswd(int argc, char *argv[])
{
	AuthenticationType type;
	const char *result;
	char *p = argv[2];

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

void unrealircdctl_gencloak(int argc, char *argv[])
{
	#define GENERATE_CLOAKKEY_LEN 80 /* Length of cloak keys to generate. */
	char keyBuf[GENERATE_CLOAKKEY_LEN + 1];
	int keyNum;
	int charIndex;

	short has_upper;
	short has_lower;
	short has_num;

	printf("Here are 3 random cloak keys that you can copy-paste to your configuration file:\n\n");

	printf("set {\n\tcloak-keys {\n");
	for (keyNum = 0; keyNum < 3; ++keyNum)
	{
		has_upper = 0;
		has_lower = 0;
		has_num = 0;

		for (charIndex = 0; charIndex < sizeof(keyBuf)-1; ++charIndex)
		{
			switch (getrandom8() % 3)
			{
				case 0: /* Uppercase. */
					keyBuf[charIndex] = (char)('A' + (getrandom8() % ('Z' - 'A')));
					has_upper = 1;
					break;
				case 1: /* Lowercase. */
					keyBuf[charIndex] = (char)('a' + (getrandom8() % ('z' - 'a')));
					has_lower = 1;
					break;
				case 2: /* Digit. */
					keyBuf[charIndex] = (char)('0' + (getrandom8() % ('9' - '0')));
					has_num = 1;
					break;
			}
		}
		keyBuf[sizeof(keyBuf)-1] = '\0';

		if (has_upper && has_lower && has_num)
			printf("\t\t\"%s\";\n", keyBuf);
		else
			/* Try again. For this reason, keyNum must be signed. */
			keyNum--;
	}
	printf("\t}\n}\n\n");
	exit(0);
}

void unrealircdctl_spkifp(int argc, char *argv[])
{
	char *file = argv[2];
	SSL_CTX *ctx = SSL_CTX_new(SSLv23_server_method());
	SSL *ssl;
	X509 *cert;
	const char *spkifp;

	if (!ctx)
	{
		printf("Internal failure while initializing SSL/TLS library context\n");
		exit(1);
	}

	if (!file)
	{
		printf("NOTE: This script uses the default certificate location (any set::tls settings\n"
		       "are ignored). If this is not what you want then specify a certificate\n"
		       "explicitly like this: %s spkifp conf/tls/example.pem\n\n", UNREALCMD);
		safe_strdup(file, "tls/server.cert.pem");
		convert_to_absolute_path(&file, CONFDIR);
	}

	if (!file_exists(file))
	{
		printf("Could not open certificate: %s\n"
		       "You can specify a certificate like this: %s spkifp conf/tls/example.pem\n",
		       UNREALCMD, file);
		exit(1);
	}

	if (SSL_CTX_use_certificate_chain_file(ctx, file) <= 0)
	{
		printf("Could not read certificate '%s'\n", file);
		exit(1);
	}

	ssl = SSL_new(ctx);
	if (!ssl)
	{
		printf("Something went wrong when generating the SPKI fingerprint.\n");
		exit(1);
	}

	cert = SSL_get_certificate(ssl);
	spkifp = spki_fingerprint_ex(cert);
	printf("The SPKI fingerprint for certificate '%s' is:\n"
	       "%s\n"
	       "\n"
	       "You normally add this password on the other side of the link as:\n"
	       "password \"%s\" { spkifp; };\n"
	       "\n",
	       file, spkifp, spkifp);
	exit(0);
}

int main(int argc, char *argv[])
{
#ifdef _WIN32
	chdir(".."); /* go up one level from "bin" */
	init_winsock();
#else
#if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
	alarm(60); /* 60 seconds timeout - ASan can be slow... */
#else
	alarm(20); /* 20 seconds timeout */
#endif
#endif
	dbuf_init();
	init_random();
	early_init_tls();

	if (argc == 1)
		unrealircdctl_usage(argv[0]);

	if (!strcmp(argv[1], "rehash"))
		unrealircdctl_rehash();
	else if (!strcmp(argv[1], "reloadtls"))
		unrealircdctl_reloadtls();
	else if (!strcmp(argv[1], "status"))
		unrealircdctl_status();
	else if (!strcmp(argv[1], "module-status"))
		unrealircdctl_module_status();
	else if (!strcmp(argv[1], "mkpasswd"))
		unrealircdctl_mkpasswd(argc, argv);
	else if (!strcmp(argv[1], "gencloak"))
		unrealircdctl_gencloak(argc, argv);
	else if (!strcmp(argv[1], "spkifp") || !strcmp(argv[1], "spki"))
		unrealircdctl_spkifp(argc, argv);
	else
		unrealircdctl_usage(argv[0]);
	exit(0);
}
