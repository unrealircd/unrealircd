/*
 * src/modules/smtp.c
 * Asynchronous SMTP client for ObbyIRCd.
 *
 * Modeled on src/url_unreal.c.  Uses the same async primitives
 * (fd_setselect, fd_socket, c-ares resolver, OpenSSL) so a slow or
 * unreachable mail server CANNOT block the IRCd event loop.
 *
 * Public API: see include/smtp.h.
 *
 * Config:
 *   smtp {
 *       host "smtp.example.com";    // SMTP server
 *       port 587;                   // 25 / 465 / 587
 *       security starttls;          // none | starttls | tls
 *       username "alerts@example.com";
 *       password "...";
 *       from "ObbyIRCd <alerts@example.com>";
 *       connect-timeout 30;
 *       transfer-timeout 60;
 *   };
 *
 * Supported AUTH mechanisms: PLAIN.  Refuses to send credentials over an
 * un-encrypted channel unless username/password are unset.
 */

#include "unrealircd.h"
#include "smtp.h"

ModuleHeader MOD_HEADER = {
	"smtp",
	"1.0",
	"Asynchronous SMTP client for outbound email",
	"ObbyIRCd Team",
	"unrealircd-6",
};

/* External resolver channel from src/dns.c (used by url_unreal.c too). */
extern ares_channel resolver_channel_https;

/* ===================================================================
 * Constants
 * =================================================================== */
#define SMTP_DEFAULT_CONNECT_TIMEOUT   30
#define SMTP_DEFAULT_TRANSFER_TIMEOUT  60
#define SMTP_READ_BUFSIZE              4096
#define SMTP_MAX_REPLY_LINE            8192
#define SMTP_MAX_BODY_BYTES            (1024 * 1024)
#define SMTP_TIMER_INTERVAL_MS         1000

#define SMTP_SEC_NONE      0
#define SMTP_SEC_STARTTLS  1
#define SMTP_SEC_TLS       2

/* ===================================================================
 * Types
 * =================================================================== */

typedef enum SmtpState
{
	SMTP_STATE_DNS,
	SMTP_STATE_CONNECTING,
	SMTP_STATE_TLS_INITIAL,        /* implicit-TLS handshake (port 465) */
	SMTP_STATE_WAIT_BANNER,        /* expect 220 */
	SMTP_STATE_SENT_EHLO,          /* expect 250 */
	SMTP_STATE_SENT_STARTTLS,      /* expect 220 */
	SMTP_STATE_TLS_AFTER_STARTTLS,
	SMTP_STATE_SENT_EHLO2,         /* expect 250 (post-TLS) */
	SMTP_STATE_SENT_AUTH,          /* expect 235 */
	SMTP_STATE_SENT_MAIL_FROM,     /* expect 250 */
	SMTP_STATE_SENT_RCPT_TO,       /* expect 250 */
	SMTP_STATE_SENT_DATA,          /* expect 354 */
	SMTP_STATE_SENT_BODY,          /* expect 250 */
	SMTP_STATE_SENT_QUIT,          /* terminal, ignore */
	SMTP_STATE_DONE
} SmtpState;

typedef struct SmtpJob_ SmtpJob;

struct SmtpJob_
{
	SmtpJob *prev, *next;

	/* Caller payload */
	char *to_addr;
	char *subject;
	char *body;
	SMTPCallback callback;
	void *userdata;

	/* Network state */
	int fd;
	SSL *ssl;
	SocketType socket_type;
	char *ip4;
	char *ip6;
	int dns_refcnt;
	int connected;
	int tls_active;
	SmtpState state;
	time_t started;

	/* Read assembly buffer (bytes received, not yet consumed) */
	char readbuf[SMTP_MAX_REPLY_LINE];
	int readlen;

	/* Write buffer for partial sends */
	char *writebuf;
	int writelen;
	int writepos;

	/* Last server reply that triggered an error, for diagnostics */
	char errorbuf[512];
};

/* ===================================================================
 * Module state
 * =================================================================== */
static struct
{
	int configured;
	char *host;
	int port;
	int security;
	char *username;
	char *password;
	char *from;
	int connect_timeout;
	int transfer_timeout;
} cfg;

static SmtpJob *jobs = NULL;
static SSL_CTX *smtp_tls_ctx = NULL;

/* ===================================================================
 * Forward decls
 * =================================================================== */
static void smtp_setconf_defaults(void);
static void smtp_freeconf(void);
static int smtp_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
static int smtp_configrun(ConfigFile *cf, ConfigEntry *ce, int type);
static EVENT(smtp_timer);

static void job_start(SmtpJob *j);
static void job_finish_ok(SmtpJob *j);
static void job_finish_err(SmtpJob *j, FORMAT_STRING(const char *fmt), ...) __attribute__((format(printf,2,3)));
static void job_free(SmtpJob *j);

static void resolve_cb(void *arg, int status, int timeouts, struct hostent *he);
static void initiate_connect(SmtpJob *j);
static void on_connect_ready(int fd, int revents, void *data);
static int begin_tls(SmtpJob *j);
static int continue_tls(SmtpJob *j);
static void on_tls_progress(int fd, int revents, void *data);

static void on_readable(int fd, int revents, void *data);
static void on_writable(int fd, int revents, void *data);
static int try_send_writebuf(SmtpJob *j);
static void send_line(SmtpJob *j, const char *line);
static void send_payload(SmtpJob *j, const char *data, int len);
static void arm_read(SmtpJob *j);

static int parse_reply(SmtpJob *j, int *code, int *more, char **text);
static void process_reply(SmtpJob *j, int code, const char *text);
static char *build_dot_stuffed_message(SmtpJob *j, int *outlen);

/* ===================================================================
 * Helpers
 * =================================================================== */

static char *xstrdup(const char *s)
{
	char *r = NULL;
	if (s)
		safe_strdup(r, s);
	return r;
}

/* AUTH PLAIN: base64( \0 user \0 pass ) */
static char *make_auth_plain(const char *user, const char *pass)
{
	int ulen = strlen(user);
	int plen = strlen(pass);
	int rawlen = 1 + ulen + 1 + plen;
	unsigned char *raw = safe_alloc(rawlen);
	int outsize = ((rawlen + 2) / 3) * 4 + 1;
	char *out = safe_alloc(outsize);
	int n;

	raw[0] = 0;
	memcpy(raw + 1, user, ulen);
	raw[1 + ulen] = 0;
	memcpy(raw + 2 + ulen, pass, plen);
	n = b64_encode(raw, rawlen, out, outsize);
	safe_free(raw);
	if (n <= 0)
	{
		safe_free(out);
		return NULL;
	}
	return out;
}

/* ===================================================================
 * Job lifecycle
 * =================================================================== */

static void job_finish_ok(SmtpJob *j)
{
	if (j->callback)
		j->callback(1, NULL, j->userdata);
	j->callback = NULL;
	j->state = SMTP_STATE_DONE;
	if (j->fd > 0)
	{
		fd_close(j->fd);
		fd_unnotify(j->fd);
		j->fd = -1;
	}
	job_free(j);
}

static void job_finish_err(SmtpJob *j, FORMAT_STRING(const char *fmt), ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(j->errorbuf, sizeof(j->errorbuf), fmt, ap);
	va_end(ap);

	unreal_log(ULOG_DEBUG, "smtp", "SMTP_FAIL", NULL,
	           "SMTP delivery to $to failed: $err",
	           log_data_string("to", j->to_addr ? j->to_addr : "?"),
	           log_data_string("err", j->errorbuf));

	if (j->callback)
		j->callback(0, j->errorbuf, j->userdata);
	j->callback = NULL;
	j->state = SMTP_STATE_DONE;
	if (j->fd > 0)
	{
		fd_close(j->fd);
		fd_unnotify(j->fd);
		j->fd = -1;
	}
	job_free(j);
}

static void job_free(SmtpJob *j)
{
	DelListItem(j, jobs);
	if (j->ssl)
	{
		SSL_free(j->ssl);
		j->ssl = NULL;
	}
	safe_free(j->to_addr);
	safe_free(j->subject);
	safe_free(j->body);
	safe_free(j->ip4);
	safe_free(j->ip6);
	safe_free(j->writebuf);
	safe_free(j);
}

/* ===================================================================
 * DNS + connect
 * =================================================================== */

static void job_start(SmtpJob *j)
{
	j->started = TStime();
	j->state = SMTP_STATE_DNS;
	AddListItem(j, jobs);

	if (is_valid_ip(cfg.host))
	{
		if (strchr(cfg.host, ':'))
			safe_strdup(j->ip6, cfg.host);
		else
			safe_strdup(j->ip4, cfg.host);
		initiate_connect(j);
		return;
	}

	j->dns_refcnt = 2;
	ares_gethostbyname(resolver_channel_https, cfg.host, AF_INET, resolve_cb, j);
	ares_gethostbyname(resolver_channel_https, cfg.host, AF_INET6, resolve_cb, j);
}

static void resolve_cb(void *arg, int status, int timeouts, struct hostent *he)
{
	SmtpJob *j = arg;
	char ipbuf[HOSTLEN + 1];
	const char *ip = NULL;

	j->dns_refcnt--;

	if ((status == 0) && he && he->h_addr_list && he->h_addr_list[0])
	{
		if (he->h_length == 16)
			ip = inetntop(AF_INET6, he->h_addr_list[0], ipbuf, sizeof(ipbuf));
		else if (he->h_length == 4)
			ip = inetntop(AF_INET, he->h_addr_list[0], ipbuf, sizeof(ipbuf));
		if (ip)
		{
			if (he->h_length == 16)
				safe_strdup(j->ip6, ip);
			else
				safe_strdup(j->ip4, ip);
		}
	}

	if (j->dns_refcnt > 0)
		return;

	if (!j->ip4 && !j->ip6)
	{
		job_finish_err(j, "DNS resolution failed for %s", cfg.host);
		return;
	}

	if (j->ip4)
		j->socket_type = SOCKET_TYPE_IPV4;
	else
		j->socket_type = SOCKET_TYPE_IPV6;
	initiate_connect(j);
}

static void initiate_connect(SmtpJob *j)
{
	const char *ip;

	j->state = SMTP_STATE_CONNECTING;
	j->fd = fd_socket(j->socket_type == SOCKET_TYPE_IPV6 ? AF_INET6 : AF_INET,
	                  SOCK_STREAM, 0, "SMTP");
	if (j->fd < 0)
	{
		if (j->socket_type == SOCKET_TYPE_IPV4 && j->ip6 && !DISABLE_IPV6)
		{
			j->socket_type = SOCKET_TYPE_IPV6;
			initiate_connect(j);
			return;
		}
		job_finish_err(j, "socket(): %s", strerror(ERRNO));
		return;
	}
	set_sock_opts(j->fd, NULL, j->socket_type);
	ip = (j->socket_type == SOCKET_TYPE_IPV4) ? j->ip4 : j->ip6;
	if (!unreal_connect(j->fd, ip, cfg.port, j->socket_type))
	{
		if (j->socket_type == SOCKET_TYPE_IPV4 && j->ip6 && !DISABLE_IPV6)
		{
			fd_close(j->fd);
			j->fd = -1;
			j->socket_type = SOCKET_TYPE_IPV6;
			initiate_connect(j);
			return;
		}
		job_finish_err(j, "connect(): %s", strerror(ERRNO));
		return;
	}
	fd_setselect(j->fd, FD_SELECT_WRITE, on_connect_ready, j);
}

static void on_connect_ready(int fd, int revents, void *data)
{
	SmtpJob *j = data;
	int sockerr = 0;
	socklen_t len = sizeof(sockerr);

	fd_setselect(fd, FD_SELECT_WRITE, NULL, j);

	if (!getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)&sockerr, &len) && sockerr)
	{
		if (j->socket_type == SOCKET_TYPE_IPV4 && j->ip6 && !DISABLE_IPV6)
		{
			fd_close(fd);
			fd_unnotify(fd);
			j->fd = -1;
			j->socket_type = SOCKET_TYPE_IPV6;
			initiate_connect(j);
			return;
		}
		job_finish_err(j, "connect failed: %s", STRERROR(sockerr));
		return;
	}

	j->connected = 1;

	if (cfg.security == SMTP_SEC_TLS)
	{
		j->state = SMTP_STATE_TLS_INITIAL;
		if (begin_tls(j) < 0)
			return;
	}
	else
	{
		j->state = SMTP_STATE_WAIT_BANNER;
		arm_read(j);
	}
}

/* ===================================================================
 * TLS
 * =================================================================== */

static int begin_tls(SmtpJob *j)
{
	if (!smtp_tls_ctx)
	{
		smtp_tls_ctx = https_new_ctx();
		if (!smtp_tls_ctx)
		{
			job_finish_err(j, "TLS context init failed");
			return -1;
		}
	}
	j->ssl = SSL_new(smtp_tls_ctx);
	if (!j->ssl)
	{
		job_finish_err(j, "SSL_new() failed");
		return -1;
	}
	SSL_set_fd(j->ssl, j->fd);
	SSL_set_connect_state(j->ssl);
	SSL_set_nonblocking(j->ssl);
	SSL_set_tlsext_host_name(j->ssl, cfg.host);
	return continue_tls(j);
}

static int continue_tls(SmtpJob *j)
{
	int rc = SSL_connect(j->ssl);
	if (rc <= 0)
	{
		int err = SSL_get_error(j->ssl, rc);
		switch (err)
		{
			case SSL_ERROR_WANT_READ:
				fd_setselect(j->fd, FD_SELECT_READ, on_tls_progress, j);
				fd_setselect(j->fd, FD_SELECT_WRITE, NULL, j);
				return 0;
			case SSL_ERROR_WANT_WRITE:
				fd_setselect(j->fd, FD_SELECT_WRITE, on_tls_progress, j);
				fd_setselect(j->fd, FD_SELECT_READ, NULL, j);
				return 0;
			case SSL_ERROR_SYSCALL:
				if (ERRNO == P_EINTR || ERRNO == P_EWOULDBLOCK || ERRNO == P_EAGAIN)
				{
					fd_setselect(j->fd, FD_SELECT_READ | FD_SELECT_WRITE,
					             on_tls_progress, j);
					return 0;
				}
				job_finish_err(j, "TLS syscall error: %s", strerror(ERRNO));
				return -1;
			default:
				job_finish_err(j, "TLS handshake failed (err=%d)", err);
				return -1;
		}
	}

	/* TLS handshake finished. */
	j->tls_active = 1;
	fd_setselect(j->fd, FD_SELECT_WRITE, NULL, j);
	if (j->state == SMTP_STATE_TLS_INITIAL)
	{
		j->state = SMTP_STATE_WAIT_BANNER;
		arm_read(j);
	}
	else if (j->state == SMTP_STATE_TLS_AFTER_STARTTLS)
	{
		/* Re-EHLO after STARTTLS upgrade */
		j->state = SMTP_STATE_SENT_EHLO2;
		send_line(j, "EHLO obbyircd");
		arm_read(j);
	}
	return 1;
}

static void on_tls_progress(int fd, int revents, void *data)
{
	SmtpJob *j = data;
	continue_tls(j);
}

/* ===================================================================
 * Read / write
 * =================================================================== */

static void arm_read(SmtpJob *j)
{
	fd_setselect(j->fd, FD_SELECT_READ, on_readable, j);
}

static int try_send_writebuf(SmtpJob *j)
{
	int n;
	int remaining;

	while (j->writepos < j->writelen)
	{
		remaining = j->writelen - j->writepos;
		if (j->tls_active)
		{
			n = SSL_write(j->ssl, j->writebuf + j->writepos, remaining);
			if (n <= 0)
			{
				int err = SSL_get_error(j->ssl, n);
				if (err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ)
				{
					fd_setselect(j->fd, FD_SELECT_WRITE, on_writable, j);
					return 0;
				}
				job_finish_err(j, "TLS write error (%d)", err);
				return -1;
			}
		}
		else
		{
			n = write(j->fd, j->writebuf + j->writepos, remaining);
			if (n < 0)
			{
				if (ERRNO == P_EAGAIN || ERRNO == P_EWOULDBLOCK || ERRNO == P_EINTR)
				{
					fd_setselect(j->fd, FD_SELECT_WRITE, on_writable, j);
					return 0;
				}
				job_finish_err(j, "write(): %s", strerror(ERRNO));
				return -1;
			}
			if (n == 0)
			{
				job_finish_err(j, "write(): connection closed");
				return -1;
			}
		}
		j->writepos += n;
	}

	/* Drained */
	safe_free(j->writebuf);
	j->writebuf = NULL;
	j->writelen = j->writepos = 0;
	fd_setselect(j->fd, FD_SELECT_WRITE, NULL, j);
	return 1;
}

static void on_writable(int fd, int revents, void *data)
{
	SmtpJob *j = data;
	try_send_writebuf(j);
}

static void send_payload(SmtpJob *j, const char *data, int len)
{
	int newlen;
	char *nb;

	if (len <= 0)
		return;
	newlen = (j->writelen - j->writepos) + len;
	nb = safe_alloc(newlen);
	if (j->writebuf && j->writepos < j->writelen)
		memcpy(nb, j->writebuf + j->writepos, j->writelen - j->writepos);
	memcpy(nb + (j->writelen - j->writepos), data, len);
	safe_free(j->writebuf);
	j->writebuf = nb;
	j->writelen = newlen;
	j->writepos = 0;
	try_send_writebuf(j);
}

static void send_line(SmtpJob *j, const char *line)
{
	int llen = strlen(line);
	char *buf = safe_alloc(llen + 3);
	memcpy(buf, line, llen);
	buf[llen] = '\r';
	buf[llen + 1] = '\n';
	buf[llen + 2] = 0;
	send_payload(j, buf, llen + 2);
	safe_free(buf);
}

static void on_readable(int fd, int revents, void *data)
{
	SmtpJob *j = data;
	int n;
	int code, more;
	char *text;

	for (;;)
	{
		int space = (int)sizeof(j->readbuf) - j->readlen - 1;
		if (space <= 0)
		{
			job_finish_err(j, "Server reply exceeds %d bytes",
			               (int)sizeof(j->readbuf));
			return;
		}
		if (j->tls_active)
		{
			n = SSL_read(j->ssl, j->readbuf + j->readlen, space);
			if (n <= 0)
			{
				int err = SSL_get_error(j->ssl, n);
				if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
					return;
				if (err == SSL_ERROR_ZERO_RETURN)
				{
					job_finish_err(j, "TLS connection closed");
					return;
				}
				job_finish_err(j, "TLS read error (%d)", err);
				return;
			}
		}
		else
		{
			n = read(j->fd, j->readbuf + j->readlen, space);
			if (n < 0)
			{
				if (ERRNO == P_EAGAIN || ERRNO == P_EWOULDBLOCK || ERRNO == P_EINTR)
					return;
				job_finish_err(j, "read(): %s", strerror(ERRNO));
				return;
			}
			if (n == 0)
			{
				job_finish_err(j, "Server closed connection");
				return;
			}
		}
		j->readlen += n;

		/* Drain whole replies (possibly multiple) from the buffer. */
		while (parse_reply(j, &code, &more, &text))
		{
			if (more)
			{
				/* Continuation line.  Accumulate; we only act on the
				 * final (no-dash) line of a multi-line reply.  We just
				 * skip it -- the meaningful "code" for branching is
				 * delivered by the final line. */
				safe_free(text);
				continue;
			}
			process_reply(j, code, text);
			safe_free(text);
			if (j->state == SMTP_STATE_DONE)
				return;
		}
	}
}

/**
 * Try to extract one CRLF-terminated SMTP reply line from the read buffer.
 * On success, sets *code, *more (1 = multi-line continuation, 0 = final),
 * *text (the part after the code), and consumes the line from readbuf.
 *
 * @return 1 if a line was extracted, 0 if more data is needed.
 */
static int parse_reply(SmtpJob *j, int *code, int *more, char **text)
{
	char *eol;
	int linelen;
	int eat;

	*text = NULL;
	*code = 0;
	*more = 0;

	if (j->readlen <= 0)
		return 0;
	if (j->readlen >= (int)sizeof(j->readbuf))
		j->readlen = (int)sizeof(j->readbuf) - 1;
	j->readbuf[j->readlen] = 0;

	eol = strstr(j->readbuf, "\r\n");
	if (eol)
	{
		linelen = (int)(eol - j->readbuf);
		eat = linelen + 2;
	}
	else
	{
		/* Some servers send just LF.  Tolerate. */
		eol = strchr(j->readbuf, '\n');
		if (!eol)
			return 0;
		linelen = (int)(eol - j->readbuf);
		eat = linelen + 1;
	}

	if (linelen < 0 || linelen >= j->readlen)
		return 0;

	/* Format: NNN[ -]rest -- parse in place, then advance the buffer. */
	if (linelen >= 4 && isdigit(j->readbuf[0]) &&
	    isdigit(j->readbuf[1]) && isdigit(j->readbuf[2]))
	{
		*code = (j->readbuf[0] - '0') * 100 +
		        (j->readbuf[1] - '0') * 10 +
		        (j->readbuf[2] - '0');
		*more = (j->readbuf[3] == '-') ? 1 : 0;
		if (linelen > 4)
		{
			char saved = j->readbuf[linelen];
			j->readbuf[linelen] = 0;
			*text = xstrdup(j->readbuf + 4);
			j->readbuf[linelen] = saved;
		}
	}

	memmove(j->readbuf, j->readbuf + eat, j->readlen - eat);
	j->readlen -= eat;
	return 1;
}

/* ===================================================================
 * State machine
 * =================================================================== */

static char *build_dot_stuffed_message(SmtpJob *j, int *outlen)
{
	const char *body = j->body ? j->body : "";
	char *header;
	const char *src;
	char *out;
	int hdrlen, bodylen, cap, pos;
	char date[64];
	time_t now = TStime();
	struct tm *tm = gmtime(&now);

	strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S +0000", tm);

	hdrlen = 1024 + strlen(cfg.from) + strlen(j->to_addr)
	         + strlen(j->subject ? j->subject : "");
	header = safe_alloc(hdrlen);
	snprintf(header, hdrlen,
	         "From: %s\r\n"
	         "To: %s\r\n"
	         "Subject: %s\r\n"
	         "Date: %s\r\n"
	         "MIME-Version: 1.0\r\n"
	         "Content-Type: text/plain; charset=utf-8\r\n"
	         "Content-Transfer-Encoding: 8bit\r\n"
	         "\r\n",
	         cfg.from, j->to_addr,
	         j->subject ? j->subject : "(no subject)",
	         date);
	hdrlen = strlen(header);
	bodylen = strlen(body);

	/* Worst-case: every newline becomes "\r\n" AND every leading-dot line gets
	 * an extra dot.  Plus terminating "\r\n.\r\n". */
	cap = hdrlen + bodylen * 3 + 16;
	out = safe_alloc(cap);
	memcpy(out, header, hdrlen);
	pos = hdrlen;
	safe_free(header);

	src = body;
	int at_line_start = 1;
	while (*src)
	{
		if (at_line_start && *src == '.')
		{
			out[pos++] = '.';
			out[pos++] = '.';
			src++;
			at_line_start = 0;
			continue;
		}
		if (*src == '\r' && *(src + 1) == '\n')
		{
			out[pos++] = '\r';
			out[pos++] = '\n';
			src += 2;
			at_line_start = 1;
			continue;
		}
		if (*src == '\n')
		{
			out[pos++] = '\r';
			out[pos++] = '\n';
			src++;
			at_line_start = 1;
			continue;
		}
		out[pos++] = *src++;
		at_line_start = 0;
	}
	/* Ensure body ends with CRLF before the dot terminator. */
	if (pos < 2 || out[pos - 2] != '\r' || out[pos - 1] != '\n')
	{
		out[pos++] = '\r';
		out[pos++] = '\n';
	}
	out[pos++] = '.';
	out[pos++] = '\r';
	out[pos++] = '\n';

	*outlen = pos;
	return out;
}

static void process_reply(SmtpJob *j, int code, const char *text)
{
	switch (j->state)
	{
		case SMTP_STATE_WAIT_BANNER:
			if (code != 220)
			{
				job_finish_err(j, "Bad banner: %d %s", code, text);
				return;
			}
			j->state = SMTP_STATE_SENT_EHLO;
			send_line(j, "EHLO obbyircd");
			break;

		case SMTP_STATE_SENT_EHLO:
			if (code != 250)
			{
				job_finish_err(j, "EHLO rejected: %d %s", code, text);
				return;
			}
			if (cfg.security == SMTP_SEC_STARTTLS && !j->tls_active)
			{
				j->state = SMTP_STATE_SENT_STARTTLS;
				send_line(j, "STARTTLS");
			}
			else if (cfg.username && *cfg.username)
			{
				char *blob;
				char cmd[1024];
				if (!j->tls_active && cfg.security != SMTP_SEC_NONE)
				{
					job_finish_err(j, "Refusing AUTH PLAIN over plaintext");
					return;
				}
				blob = make_auth_plain(cfg.username, cfg.password ? cfg.password : "");
				if (!blob)
				{
					job_finish_err(j, "AUTH PLAIN encode failed");
					return;
				}
				snprintf(cmd, sizeof(cmd), "AUTH PLAIN %s", blob);
				safe_free(blob);
				j->state = SMTP_STATE_SENT_AUTH;
				send_line(j, cmd);
			}
			else
			{
				char buf[512];
				snprintf(buf, sizeof(buf), "MAIL FROM:<%s>", cfg.from);
				j->state = SMTP_STATE_SENT_MAIL_FROM;
				send_line(j, buf);
			}
			break;

		case SMTP_STATE_SENT_STARTTLS:
			if (code != 220)
			{
				job_finish_err(j, "STARTTLS rejected: %d %s", code, text);
				return;
			}
			j->state = SMTP_STATE_TLS_AFTER_STARTTLS;
			fd_setselect(j->fd, FD_SELECT_READ, NULL, j);
			begin_tls(j);
			break;

		case SMTP_STATE_SENT_EHLO2:
			if (code != 250)
			{
				job_finish_err(j, "EHLO (post-TLS) rejected: %d %s", code, text);
				return;
			}
			if (cfg.username && *cfg.username)
			{
				char *blob = make_auth_plain(cfg.username, cfg.password ? cfg.password : "");
				char cmd[1024];
				if (!blob)
				{
					job_finish_err(j, "AUTH PLAIN encode failed");
					return;
				}
				snprintf(cmd, sizeof(cmd), "AUTH PLAIN %s", blob);
				safe_free(blob);
				j->state = SMTP_STATE_SENT_AUTH;
				send_line(j, cmd);
			}
			else
			{
				char buf[512];
				snprintf(buf, sizeof(buf), "MAIL FROM:<%s>", cfg.from);
				j->state = SMTP_STATE_SENT_MAIL_FROM;
				send_line(j, buf);
			}
			break;

		case SMTP_STATE_SENT_AUTH:
			if (code != 235)
			{
				job_finish_err(j, "AUTH rejected: %d %s", code, text);
				return;
			}
			{
				char buf[512];
				snprintf(buf, sizeof(buf), "MAIL FROM:<%s>", cfg.from);
				j->state = SMTP_STATE_SENT_MAIL_FROM;
				send_line(j, buf);
			}
			break;

		case SMTP_STATE_SENT_MAIL_FROM:
			if (code != 250)
			{
				job_finish_err(j, "MAIL FROM rejected: %d %s", code, text);
				return;
			}
			{
				char buf[512];
				snprintf(buf, sizeof(buf), "RCPT TO:<%s>", j->to_addr);
				j->state = SMTP_STATE_SENT_RCPT_TO;
				send_line(j, buf);
			}
			break;

		case SMTP_STATE_SENT_RCPT_TO:
			if (code != 250 && code != 251)
			{
				job_finish_err(j, "RCPT TO rejected: %d %s", code, text);
				return;
			}
			j->state = SMTP_STATE_SENT_DATA;
			send_line(j, "DATA");
			break;

		case SMTP_STATE_SENT_DATA:
			if (code != 354)
			{
				job_finish_err(j, "DATA rejected: %d %s", code, text);
				return;
			}
			{
				int mlen = 0;
				char *msg = build_dot_stuffed_message(j, &mlen);
				j->state = SMTP_STATE_SENT_BODY;
				send_payload(j, msg, mlen);
				safe_free(msg);
			}
			break;

		case SMTP_STATE_SENT_BODY:
			if (code != 250)
			{
				job_finish_err(j, "Message rejected: %d %s", code, text);
				return;
			}
			j->state = SMTP_STATE_SENT_QUIT;
			send_line(j, "QUIT");
			job_finish_ok(j);
			break;

		default:
			/* Unsolicited reply or post-DONE noise; ignore. */
			break;
	}
}

/* ===================================================================
 * Timer (timeouts)
 * =================================================================== */
static EVENT(smtp_timer)
{
	SmtpJob *j = jobs, *next;
	time_t now = TStime();
	while (j)
	{
		next = j->next;
		if (!j->connected && (now - j->started > cfg.connect_timeout))
		{
			job_finish_err(j, "Connect timeout (%ds)", cfg.connect_timeout);
			j = next;
			continue;
		}
		if (j->connected && (now - j->started > cfg.transfer_timeout))
		{
			job_finish_err(j, "Transfer timeout (%ds)", cfg.transfer_timeout);
			j = next;
			continue;
		}
		j = next;
	}
}

/* ===================================================================
 * Public API
 * =================================================================== */

int smtp_is_configured(void)
{
	return cfg.configured;
}

int smtp_send_async(const char *to_addr, const char *subject, const char *body,
                    SMTPCallback cb, void *userdata)
{
	SmtpJob *j;

	if (!cfg.configured || !cfg.host || !cfg.from)
		return 0;
	if (BadPtr(to_addr))
		return 0;
	if (body && strlen(body) > SMTP_MAX_BODY_BYTES)
		return 0;

	j = safe_alloc(sizeof(*j));
	j->fd = -1;
	safe_strdup(j->to_addr, to_addr);
	if (subject)
		safe_strdup(j->subject, subject);
	if (body)
		safe_strdup(j->body, body);
	j->callback = cb;
	j->userdata = userdata;

	job_start(j);
	return 1;
}

/* ===================================================================
 * Config
 * =================================================================== */

static void smtp_setconf_defaults(void)
{
	cfg.configured = 0;
	cfg.host = NULL;
	cfg.port = 0;
	cfg.security = SMTP_SEC_STARTTLS;
	cfg.username = NULL;
	cfg.password = NULL;
	cfg.from = NULL;
	cfg.connect_timeout = SMTP_DEFAULT_CONNECT_TIMEOUT;
	cfg.transfer_timeout = SMTP_DEFAULT_TRANSFER_TIMEOUT;
}

static void smtp_freeconf(void)
{
	safe_free(cfg.host);
	safe_free(cfg.username);
	safe_free(cfg.password);
	safe_free(cfg.from);
	smtp_setconf_defaults();
}

static int parse_security(const char *v)
{
	if (!v)
		return -1;
	if (!strcasecmp(v, "none") || !strcasecmp(v, "no"))
		return SMTP_SEC_NONE;
	if (!strcasecmp(v, "starttls"))
		return SMTP_SEC_STARTTLS;
	if (!strcasecmp(v, "tls") || !strcasecmp(v, "ssl") || !strcasecmp(v, "smtps"))
		return SMTP_SEC_TLS;
	return -1;
}

static int smtp_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	ConfigEntry *cep;
	int errors = 0;
	int has_host = 0, has_from = 0;

	if (type != CONFIG_MAIN)
		return 0;
	if (!ce || !ce->name || strcmp(ce->name, "smtp"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!cep->name)
			continue;
		if (!strcasecmp(cep->name, "host"))
		{
			if (BadPtr(cep->value)) { errors++; config_error("%s:%i: smtp::host empty", cep->file->filename, cep->line_number); }
			else has_host = 1;
		}
		else if (!strcasecmp(cep->name, "port"))
		{
			int p = cep->value ? atoi(cep->value) : 0;
			if (p < 1 || p > 65535)
			{
				errors++;
				config_error("%s:%i: smtp::port must be 1-65535", cep->file->filename, cep->line_number);
			}
		}
		else if (!strcasecmp(cep->name, "security"))
		{
			if (parse_security(cep->value) < 0)
			{
				errors++;
				config_error("%s:%i: smtp::security must be none|starttls|tls",
				             cep->file->filename, cep->line_number);
			}
		}
		else if (!strcasecmp(cep->name, "username") ||
		         !strcasecmp(cep->name, "password"))
		{
			if (BadPtr(cep->value))
			{
				errors++;
				config_error("%s:%i: smtp::%s empty", cep->file->filename, cep->line_number, cep->name);
			}
		}
		else if (!strcasecmp(cep->name, "from"))
		{
			if (BadPtr(cep->value))
			{
				errors++;
				config_error("%s:%i: smtp::from empty", cep->file->filename, cep->line_number);
			}
			else has_from = 1;
		}
		else if (!strcasecmp(cep->name, "connect-timeout") ||
		         !strcasecmp(cep->name, "transfer-timeout"))
		{
			if (BadPtr(cep->value))
			{
				errors++;
				config_error("%s:%i: smtp::%s empty", cep->file->filename, cep->line_number, cep->name);
			}
		}
		else
		{
			config_warn("%s:%i: unknown smtp::%s",
			            cep->file->filename, cep->line_number, cep->name);
		}
	}

	if (!has_host) { errors++; config_error("%s:%i: smtp::host required", ce->file->filename, ce->line_number); }
	if (!has_from) { errors++; config_error("%s:%i: smtp::from required", ce->file->filename, ce->line_number); }

	*errs = errors;
	return errors ? -1 : 1;
}

static int smtp_configrun(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;

	if (type != CONFIG_MAIN)
		return 0;
	if (!ce || !ce->name || strcmp(ce->name, "smtp"))
		return 0;

	smtp_freeconf();

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!cep->name)
			continue;
		if (!strcasecmp(cep->name, "host"))
			safe_strdup(cfg.host, cep->value);
		else if (!strcasecmp(cep->name, "port"))
			cfg.port = atoi(cep->value);
		else if (!strcasecmp(cep->name, "security"))
			cfg.security = parse_security(cep->value);
		else if (!strcasecmp(cep->name, "username"))
			safe_strdup(cfg.username, cep->value);
		else if (!strcasecmp(cep->name, "password"))
			safe_strdup(cfg.password, cep->value);
		else if (!strcasecmp(cep->name, "from"))
			safe_strdup(cfg.from, cep->value);
		else if (!strcasecmp(cep->name, "connect-timeout"))
			cfg.connect_timeout = config_checkval(cep->value, CFG_TIME);
		else if (!strcasecmp(cep->name, "transfer-timeout"))
			cfg.transfer_timeout = config_checkval(cep->value, CFG_TIME);
	}

	if (cfg.port == 0)
	{
		if (cfg.security == SMTP_SEC_TLS) cfg.port = 465;
		else if (cfg.security == SMTP_SEC_STARTTLS) cfg.port = 587;
		else cfg.port = 25;
	}

	cfg.configured = 1;
	return 1;
}

/* ===================================================================
 * Module entry points
 * =================================================================== */

MOD_TEST()
{
	smtp_setconf_defaults();
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, smtp_configtest);
	return MOD_SUCCESS;
}

/* Hook bridge so other modules can call us without taking a link-time
 * dependency on this module's symbols.  See include/smtp.h. */
static int smtp_hook_send_email(const char *to, const char *subject,
                                const char *body,
                                SMTPCallback cb, void *userdata)
{
	if (!cfg.configured)
		return 0;
	return smtp_send_async(to, subject, body, cb, userdata);
}

MOD_INIT()
{
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, smtp_configrun);
	HookAdd(modinfo->handle, HOOKTYPE_SEND_EMAIL, 0, smtp_hook_send_email);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	EventAdd(modinfo->handle, "smtp_timer", smtp_timer, NULL,
	         SMTP_TIMER_INTERVAL_MS, 0);
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	SmtpJob *j = jobs, *next;
	while (j)
	{
		next = j->next;
		if (j->callback)
			j->callback(0, "smtp module unloading", j->userdata);
		j->callback = NULL;
		if (j->fd > 0)
		{
			fd_close(j->fd);
			fd_unnotify(j->fd);
			j->fd = -1;
		}
		job_free(j);
		j = next;
	}
	if (smtp_tls_ctx)
	{
		SSL_CTX_free(smtp_tls_ctx);
		smtp_tls_ctx = NULL;
	}
	smtp_freeconf();
	return MOD_SUCCESS;
}
