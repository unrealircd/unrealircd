/* UnrealIRCd crash reporter code.
 * (C) Copyright 2015 Bram Matthys ("Syzop") and the UnrealIRCd Team.
 *
 * GPLv2
 */
#include "unrealircd.h"
#ifndef _WIN32
#include <dirent.h>
#else
extern void StartUnrealAgain(void);
#endif

#include "version.h"

extern char *getosname(void);


time_t get_file_time(char *fname)
{
	struct stat st;
	
	if (stat(fname, &st) != 0)
		return 0;

	return (time_t)st.st_ctime;
}

char *find_best_coredump(void)
{
	static char best_fname[512];
	TS best_time = 0, t;
	struct dirent *dir;
#ifndef _WIN32
	DIR *fd = opendir(TMPDIR);

	if (!fd)
		return NULL;
	
	*best_fname = '\0';
	
	while ((dir = readdir(fd)))
	{
		char *fname = dir->d_name;
		if (strstr(fname, "core") && !strstr(fname, ".so") &&
		    !strstr(fname, ".conf") && !strstr(fname, ".txt") &&
		    !strstr(fname, ".done"))
		{
			char buf[512];
			
			snprintf(buf, sizeof(buf), "%s/%s", TMPDIR, fname);
			t = get_file_time(buf);
			if (t && (t > best_time))
			{
				best_time = t;
				strlcpy(best_fname, buf, sizeof(best_fname));
			}
		}
	}
	closedir(fd);
#else
	/* Windows */
	WIN32_FIND_DATA hData;
	HANDLE hFile;
	
	hFile = FindFirstFile("unrealircd.*.core", &hData);
	if (hFile == INVALID_HANDLE_VALUE)
		return NULL;
	
	while (FindNextFile(hFile, &hData))
	{
		char *fname = hData.cFileName;
		if (!strstr(fname, ".done"))
		{
			char buf[512];
			strlcpy(buf, fname, sizeof(buf));
			t = get_file_time(buf);
			if (t && (t > best_time))
			{
				best_time = t;
				strlcpy(best_fname, buf, sizeof(best_fname));
			}
		}
	}
	FindClose(hFile);
#endif	
	
	if (*best_fname)
		return best_fname;
	
	return NULL; /* none found */
}

void stripcrlf(char *p)
{
	for (; *p; p++)
	{
		if ((*p == '\r') || (*p == '\n'))
		{
			*p = '\0';
			return;
		}
	}
}

#define EL_AR_MAX MAXPARA
char **explode(char *str, char *delimiter)
{
	static char *ret[EL_AR_MAX+1];
	static char buf[1024];
	char *p, *name;
	int cnt = 0;
	
	memset(&ret, 0, sizeof(ret)); /* make sure all elements are NULL */
	
	strlcpy(buf, str, sizeof(buf));
	for (name = strtoken(&p, buf, delimiter); name; name = strtoken(&p, NULL, delimiter))
	{
		ret[cnt++] = name;
		if (cnt == EL_AR_MAX)
			break;
	}
	ret[cnt] = NULL;
	
	return ret;
}

void crash_report_fix_libs(char *coredump)
{
#ifndef _WIN32
	FILE *fd;
	char cmd[512], buf[1024];
	
	snprintf(cmd, sizeof(cmd), "echo info sharedlibrary|gdb %s/unrealircd %s 2>&1",
		BINDIR, coredump);

	fd = popen(cmd, "r");
	if (!fd)
		return;
	
	while((fgets(buf, sizeof(buf), fd)))
	{
		char *file, *path;
		char target[512];
		char **arr;

		stripcrlf(buf);

		/* Output we are interested is something like this:
		 * <many spaces>    No        /home/blabla/unrealircd/tmp/5114DF16.m_kick.so
		 */
		if (!strstr(buf, " No "))
			continue;
		
		path = strchr(buf, '/');
		if (!path)
			continue;

		if (!strstr(path, TMPDIR))
			continue; /* we only care about our TMPDIR stuff */
		
		file = strrchr(path, '/');
		if (!file)
			continue;
		file++;
		
		/* files have the following two formats:
		 * 5BE7DF9.m_svsnline.so          for modules/m_svsnline.so
		 * 300AA138.chanmodes.nokick.so   for modules/chanmodes/nokick.so
		 */
		arr = explode(file, ".");
		if (!arr[3])
			snprintf(target, sizeof(target), "%s/%s.%s", MODULESDIR, arr[1], arr[2]);
		else
			snprintf(target, sizeof(target), "%s/%s/%s.%s", MODULESDIR, arr[1], arr[2], arr[3]);
		
		if (!file_exists(target))
		{
			printf("WARNING: could not resolve %s: %s does not exist\n", path, target);
		} else {
			if (symlink(target, path) < 0)
				printf("WARNING: could not create symlink %s -> %s\n", path, target);
		}
		
	}
	pclose(fd);
#endif
}

int crash_report_backtrace(FILE *reportfd, char *coredump)
{
	FILE *fd;
	char cmd[512], buf[1024];

#ifndef _WIN32
	snprintf(buf, sizeof(buf), "%s/gdb.commands", TMPDIR);
	fd = fopen(buf, "w");
	if (!fd)
	{
		printf("ERROR: Could not write to %s.\n", buf);
		return 0;
	}
	fprintf(fd, "frame\n"
	            "echo \\n\n"
	            "list\n"
	            "echo \\n\n"
	            "x/s our_mod_version\n"
	            "echo \\n\n"
	            "x/s backupbuf\n"
	            "echo \\n\n"
	            "bt\n"
	            "echo \\n\n"
	            "bt full\n"
	            "echo \\n\n"
	            "quit\n");
	fclose(fd);

	
	snprintf(cmd, sizeof(cmd), "gdb -batch -x %s %s/unrealircd %s 2>&1",
		buf, BINDIR, coredump);
	
	fd = popen(cmd, "r");
	if (!fd)
		return 0;
	
	fprintf(reportfd, "START OF BACKTRACE\n");
	while((fgets(buf, sizeof(buf), fd)))
	{
		char *file, *path;
		char target[512];
		char **arr;

		stripcrlf(buf);
		fprintf(reportfd, " %s\n", buf);
	}
	pclose(fd);
	
	fprintf(reportfd, "END OF BACKTRACE\n");
	return 1;
#else
	fd = fopen(coredump, "r");
	if (!fd)
		return 0;
	fprintf(reportfd, "START OF CRASH DUMP\n");
	while((fgets(buf, sizeof(buf), fd)))
	{
		stripcrlf(buf);
		fprintf(reportfd, " %s\n", buf);
	}
	fclose(fd);
	fprintf(reportfd, "END OF CRASH DUMP\n");
	return 1;
#endif
}

void crash_report_header(FILE *reportfd, char *coredump)
{
	char buf[512];
	time_t t;
	
	fprintf(reportfd, "== UNREALIRCD CRASH REPORT ==\n"
	                  "\n"
	                  "SYSTEM INFORMATION:\n");
	
	fprintf(reportfd, "UnrealIRCd version: %s\n", VERSIONONLY);
#if defined(__VERSION__)
	fprintf(reportfd, "          Compiler: %s\n", __VERSION__);
#endif
	
	fprintf(reportfd, "  Operating System: %s\n", MYOSNAME);

	
	fprintf(reportfd, "Using core file: %s\n", coredump);
	
	t = get_file_time(coredump);
	if (t != 0)
	{
		fprintf(reportfd, "Crash date/time: %s\n", myctime(t) ? myctime(t) : "???");
		fprintf(reportfd, " Crash secs ago: %ld\n",
			(long)(time(NULL) - t));
	} else {
		fprintf(reportfd, "Crash date/time: UNKNOWN\n");
		fprintf(reportfd, " Crash secs ago: UNKNOWN\n");
	}
	
	fprintf(reportfd, "\n");
}

/** Checks if the binary is newer than the coredump.
 * If that's the case (1) then the core dump is likely not very usable.
 */
int corefile_vs_binary_mismatch(char *coredump)
{
#ifndef _WIN32
	time_t core, binary;
	char fname[512];
	
	snprintf(fname, sizeof(fname), "%s/unrealircd", BINDIR);
	
	core = get_file_time(coredump);
	binary = get_file_time(fname);
	
	if (!core || !binary)
		return 0; /* don't know then */
	
	if (binary > core)
		return 1; /* yup, mismatch ;/ */
	
	return 0; /* GOOD! */
#else
	return 0; /* guess we don't check this on Windows? Or will we check UnrealIRCd.exe... hmm.. yeah maybe good idea */
#endif
}

char *generate_crash_report(char *coredump)
{
	static char reportfname[512];
	FILE *reportfd;

	if (coredump == NULL)
		coredump = find_best_coredump();
	
	if (coredump == NULL)
		return NULL; /* nothing available */

	if (corefile_vs_binary_mismatch(coredump))
		return NULL;
	
	snprintf(reportfname, sizeof(reportfname), "%s/crash.report.%s.%ld.txt",
		TMPDIR, unreal_getfilename(coredump), (long)time(NULL));

	reportfd = fopen(reportfname, "w");
	if (!reportfd)
	{
		printf("ERROR: could not open '%s' for writing\n", reportfname);
		return NULL;
	}

	crash_report_header(reportfd, coredump);
	crash_report_fix_libs(coredump);
	
	if (!crash_report_backtrace(reportfd, coredump))
	{
		printf("ERROR: Could not produce a backtrace. "
		       "Possibly your system is missing the 'gdb' package or something else is wrong.\n");
		fclose(reportfd);
		return NULL;
	}

	fclose(reportfd);
	return reportfname;
}

int running_interactive(void)
{
#ifndef _WIN32
	char *s;
	
	if (!isatty(0))
		return 0;
	
	s = getenv("TERM");
	if (!s || !strcasecmp(s, "dumb") || !strcasecmp(s, "none"))
		return 0;

	return 1;
#else
	return IsService ? 0 : 1;
#endif
}

#define REPORT_NEVER	-1
#define REPORT_ASK		0
#define REPORT_AUTO		1


int getfilesize(char *fname)
{
	struct stat st;
	int size;
	
	if (stat(fname, &st) != 0)
		return -1;
	
	return (int)st.st_size;
}

#define CRASH_REPORT_HOST "crash.unrealircd.org"

SSL_CTX *crashreport_init_ssl(void)
{
	SSL_CTX *ctx_client;
	
	SSL_load_error_strings();
	SSLeay_add_ssl_algorithms();

	ctx_client = SSL_CTX_new(SSLv23_client_method());
	if (!ctx_client)
		return NULL;
	SSL_CTX_set_options(ctx_client, SSL_OP_NO_SSLv2|SSL_OP_NO_SSLv3);

	return ctx_client;
}	

int crashreport_send(char *fname)
{
	char buf[1024];
	char header[512], footer[512];
	char delimiter[41];
	int filesize;
	int n;
	FILE *fd;
	SSL_CTX *ctx_client;
	BIO *socket = NULL;
	
	filesize = getfilesize(fname);
	if (filesize < 0)
		return 0;
	
	for (n = 0; n < sizeof(delimiter); n++)
		delimiter[n] = getrandom8()%26 + 'a';
	delimiter[sizeof(delimiter)-1] = '\0';
	
	snprintf(header, sizeof(header), "--%s\r\n"
	                           "Content-Disposition: form-data; name=\"upload\"; filename=\"crash.txt\"\r\n"
	                           "Content-Type: text/plain\r\n"
	                           "\r\n",
	                           delimiter);
	snprintf(footer, sizeof(footer), "\r\n--%s--\r\n", delimiter);

	ctx_client = crashreport_init_ssl();
	if (!ctx_client)
	{
		printf("ERROR: SSL initalization failure (I)\n");
		return 0;
	}
	
	socket = BIO_new_ssl_connect(ctx_client);
	if (!socket)
	{
		printf("ERROR: SSL initalization failure (II)\n");
		return 0;
	}
	
	BIO_set_conn_hostname(socket, CRASH_REPORT_HOST ":443");

	if (BIO_do_connect(socket) != 1)
	{
		printf("ERROR: Could not connect to %s\n", CRASH_REPORT_HOST);
		return 0;
	}
	
	if (BIO_do_handshake(socket) != 1)
	{
		printf("ERROR: Could not connect to %s (SSL handshake failed)\n", CRASH_REPORT_HOST);
		return 0;
	}
	
	snprintf(buf, sizeof(buf), "POST /crash.php HTTP/1.1\r\n"
	                    "User-Agent: UnrealIRCd %s\r\n"
	                    "Host: %s\r\n"
	                    "Accept: */*\r\n"
	                    "Content-Length: %d\r\n"
	                    "Expect: 100-continue\r\n"
	                    "Content-Type: multipart/form-data; boundary=%s\r\n"
	                    "\r\n",
	                    VERSIONONLY,
	                    CRASH_REPORT_HOST,
	                    (int)(filesize+strlen(header)+strlen(footer)),
	                    delimiter);
	
	BIO_puts(socket, buf);
	
	memset(buf, 0, sizeof(buf));
	n = BIO_read(socket, buf, 255);
	if ((n < 0) || strncmp(buf, "HTTP/1.1 100", 12))
	{
		printf("Error transmitting bug report (stage II, n=%d)\n", n);
		return 0;
	}
	
	fd = fopen(fname, "rb");
	if (!fd)
		return 0;

	BIO_puts(socket, header);
	
	while ((fgets(buf, sizeof(buf), fd)))
	{
		BIO_puts(socket, buf);
	}
	fclose(fd);

	BIO_puts(socket, footer);

	do { } while(BIO_should_retry(socket)); /* make sure we are really finished (you never know with SSL) */
	
	BIO_free_all(socket);
	
	SSL_CTX_free(ctx_client);
	
	return 1;
}

void mark_coredump_as_read(char *coredump)
{
	char buf[512];
	
	snprintf(buf, sizeof(buf), "%s.%ld.done", coredump, (long)time(NULL));
	
	(void)rename(coredump, buf);
}

static int report_pref = REPORT_ASK;

void report_crash(void)
{
	char *coredump, *fname;
	int crashed_secs_ago;

	if (!running_interactive() && (report_pref != REPORT_AUTO))
		exit(0); /* don't bother if we run through cron or something similar */

	coredump = find_best_coredump();
	if (!coredump)
		return; /* no crashes */

	crashed_secs_ago = time(NULL) - get_file_time(coredump);
	if (crashed_secs_ago > 86400*7)
		return; /* stop bothering about it after a while */

	fname = generate_crash_report(coredump);
	
	if (!fname)
		return;
		
#ifndef _WIN32
	printf("The IRCd has been started now (and is running), but it did crash %d seconds ago.\n", crashed_secs_ago);
	printf("Crash report generated in: %s\n\n", fname);
		
	if (report_pref == REPORT_NEVER)
	{
		printf("Crash report will not be sent to UnrealIRCd Team.\n\n");
		printf("Feel free to read the report at %s and if you change your mind you can submit it anyway at https://bugs.unrealircd.org/\n", fname);
	} else
	if (report_pref == REPORT_ASK)
	{
		char answerbuf[64], *answer;
		printf("May I send a crash report to the UnrealIRCd developers?\n");
		printf("Crash reports help us greatly with fixing bugs that affect you and others\n");
		printf("\n");
		
		do
		{
			printf("Answer (Y/N): ");
			*answerbuf = '\0';
			answer = fgets(answerbuf, sizeof(answerbuf), stdin);
			
			if (answer && (toupper(*answer) == 'N'))
			{
				printf("Ok, not sending bug report.\n\n");
				printf("Feel free to read the report at %s and if you change your mind you can submit it anyway at https://bugs.unrealircd.org/\n", fname);
				return;
			}
			if (answer && (toupper(*answer) == 'Y'))
			{
				break;
			}
			
			printf("Invalid response. Please enter either Y or N\n\n");
		} while(1);
	} else if (report_pref != REPORT_AUTO)
	{
		printf("Huh. report_pref setting is weird. Aborting.\n");
		return;
	}

	if (running_interactive())
	{
		char buf[8192], *line;

		printf("\nDo you want to give your e-mail address so we could ask for additional information or "
		       "give you feedback about the crash? This is completely optional, just press ENTER to skip.\n\n"
		       "E-mail address (optional): ");
		line = fgets(buf, sizeof(buf), stdin);
		
		if (line && *line && (*line != '\n'))
		{
			FILE *fd = fopen(fname, "a");
			if (fd)
			{
				fprintf(fd, "\nUSER E-MAIL ADDRESS: %s\n", line);
				fclose(fd);
			}
		}

		printf("\nDo you have anything else to tell about the crash, maybe the circumstances? You can type 1 single line\n"
		       "Again, this is completely optional. Just press ENTER to skip.\n\n"
		       "Additional information (optional): ");
		line = fgets(buf, sizeof(buf), stdin);
		
		if (line && *line && (*line != '\n'))
		{
			FILE *fd = fopen(fname, "a");
			if (fd)
			{
				fprintf(fd, "\nCOMMENT BY USER: %s\n", line);
				fclose(fd);
			}
		}
	}
	if (crashreport_send(fname))
	{
		printf("\nThe crash report has been sent to the UnrealIRCd developers. "
		       "Thanks a lot for helping to make UnrealIRCd a better product!\n\n");
	}
#else
	/* Windows */
	if (MessageBox(NULL, "UnrealIRCd crashed. May I send a report about this to the UnrealIRCd developers? This helps us a lot.",
	                     "UnrealIRCd crash",
	                     MB_YESNO|MB_ICONQUESTION) == IDYES)
	{
		/* Yay */
		
		if (crashreport_send(fname))
		{
			MessageBox(NULL, "The crash report has been sent to the UnrealIRCd developers. "
			                 "If you have any additional information (like details surrounding "
			                 "the crash) then please e-mail syzop@unrealircd.org, such "
			                 "information is most welcome. Thanks!",
			           "UnrealIRCd crash report sent", MB_ICONINFORMATION|MB_OK);
		}
	}
#endif
	mark_coredump_as_read(coredump);
	
#ifdef _WIN32
	if (MessageBox(NULL, "Start UnrealIRCd again?",
	                     "UnrealIRCd crash",
	                     MB_YESNO|MB_ICONQUESTION) == IDYES)
	{
		StartUnrealAgain();
	}
#endif
}
