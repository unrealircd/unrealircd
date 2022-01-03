/************************************************************************
 *   UnrealIRCd - Unreal Internet Relay Chat Daemon - src/proc_io_client.c
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
 * @brief Inter-process I/O
 */
#include "unrealircd.h"

int procio_client_connect(const char *file)
{
	int fd;
	struct sockaddr_un addr;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
	{
#ifdef _WIN32
		fprintf(stderr, "Your Windows version does not support UNIX sockets, "
		                "so cannot communicate to UnrealIRCd.\n"
		                "Windows 10 version 1803 (April 2018) or later is needed.\n");
#else
		fprintf(stderr, "Cannot communicate to UnrealIRCd: %s\n"
		                "Perhaps your operating system does not support UNIX Sockets?\n",
				strerror(ERRNO));
#endif
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strlcpy(addr.sun_path, file, sizeof(addr.sun_path));

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		fprintf(stderr, "Could not connect to '%s': %s\n",
			CONTROLFILE, strerror(errno));
		fprintf(stderr, "The IRC server does not appear to be running.\n");
		close(fd);
		return -1;
	}

	return fd;
}

int procio_send(int fd, const char *command)
{
	char buf[512];
	int n;
	snprintf(buf, sizeof(buf), "%s\r\n", command);
	n = strlen(buf);
	if (send(fd, buf, n, 0) != n)
		return 0;
	return 1;
}

const char *recolor_logs(const char *str)
{
	static char retbuf[2048];
	char buf[2048], *p;
	const char *color = NULL;

	strlcpy(buf, str, sizeof(buf));
	p = strchr(buf, ' ');
	if ((*str != '[') || !p)
		return str;
	*p++ = '\0';

	if (!strcmp(buf, "[debug]"))
		color = log_level_terminal_color(ULOG_DEBUG);
	else if (!strcmp(buf, "[info]"))
		color = log_level_terminal_color(ULOG_INFO);
	else if (!strcmp(buf, "[warning]"))
		color = log_level_terminal_color(ULOG_WARNING);
	else if (!strcmp(buf, "[error]"))
		color = log_level_terminal_color(ULOG_ERROR);
	else if (!strcmp(buf, "[fatal]"))
		color = log_level_terminal_color(ULOG_FATAL);
	else
		color = log_level_terminal_color(ULOG_INVALID);

	snprintf(retbuf, sizeof(retbuf), "%s%s%s %s",
	         color, buf, TERMINAL_COLOR_RESET, p);
	return retbuf;
}

const char *recolor_split(const char *str)
{
	static char retbuf[2048];
	char buf[2048], *p;
	const char *color = NULL;

	strlcpy(buf, str, sizeof(buf));
	p = strchr(buf, ' ');
	if (!p)
		return str;
	*p++ = '\0';

	snprintf(retbuf, sizeof(retbuf), "%s%s %s%s%s",
	         "\033[92m", buf,
	         "\033[93m", p,
	         TERMINAL_COLOR_RESET);
	return retbuf;
}

int procio_client(const char *command, int auto_color_logs)
{
	int fd;
	char buf[READBUFSIZE];
	int n;
	dbuf queue;

	if (auto_color_logs && !terminal_supports_color())
		auto_color_logs = 0;

	fd = procio_client_connect(CONTROLFILE);
	if (fd < 0)
		return -1;

	/* Expect the welcome message */
	memset(buf, 0, sizeof(buf));
	n = recv(fd, buf, sizeof(buf), 0);
	if ((n < 0) || strncmp(buf, "READY", 4))
	{
		fprintf(stderr, "Error while communicating to IRCd via '%s': %s\n"
		                "Maybe the IRC server is not running?\n",
		                CONTROLFILE, strerror(errno));
		close(fd);
		return -1;
	}

	if (!procio_send(fd, command))
	{
		fprintf(stderr, "Error while sending command to IRCd via '%s'. Strange!\n",
		                CONTROLFILE);
		close(fd);
		return -1;
	}

	*buf = '\0';
	dbuf_queue_init(&queue);
	while(1)
	{
		n = recv(fd, buf, sizeof(buf)-1, 0);
		if (n <= 0)
			break;
		buf[n] = '\0'; /* terminate the string */
		dbuf_put(&queue, buf, n);

		/* And try to read all complete lines: */
		do
		{
			n = dbuf_getmsg(&queue, buf);
			if (n > 0)
			{
				if (!strncmp(buf, "REPLY ", 6))
				{
					char *reply = buf+6;
					if (auto_color_logs == 0)
						printf("%s\n", reply);
					else if (auto_color_logs == 1)
						printf("%s\n", recolor_logs(reply));
					else
						printf("%s\n", recolor_split(reply));
				} else
				if (!strncmp(buf, "END ", 4))
				{
					int exitcode = atoi(buf+4);
					close(fd);
					return exitcode;
				}
			}
		} while(n > 0);
	}

	/* IRCd hung up without saying goodbye, possibly problematic,
	 * or at least we cannot determine, so exit with status 66.
	 */
	close(fd);
	return 66;
}
