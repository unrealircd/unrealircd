/*
 * include/smtp.h
 * Public API for the ObbyIRCd smtp module (src/modules/smtp.c).
 *
 * Other modules that want to send email link against this module by
 * including this header AND adding require-module "smtp"; OR by checking
 * that smtp_send_async is non-NULL via dynamic linking.
 *
 * IMPORTANT: include "unrealircd.h" BEFORE this header.
 */

#ifndef OBBY_SMTP_H
#define OBBY_SMTP_H

/**
 * Custom hook type fired by callers that want to send email without taking
 * a hard link-time dependency on the smtp module.  The smtp module registers
 * a handler for this hook in MOD_INIT.  Other modules (e.g. account-
 * registration) walk Hooks[HOOKTYPE_SEND_EMAIL] and call until one returns
 * non-zero, so they degrade gracefully when smtp is not loaded.
 *
 * Handler signature (see SMTPCallback below):
 *   int handler(const char *to, const char *subject, const char *body,
 *               SMTPCallback cb, void *userdata);
 *   returns 1 if the email was queued, 0 if no SMTP backend handled it.
 *
 * 134 is the next available hook number after the core 132 hooks plus the
 * existing custom HOOKTYPE_ACCOUNT_REGISTER (133).
 */
#define HOOKTYPE_SEND_EMAIL  134

/**
 * Outcome of an asynchronous SMTP delivery attempt.
 * 'success' is 1 on a 250 reply to the final DATA, 0 otherwise.
 * 'errmsg' is a human-readable error string when success == 0; NULL on success.
 * 'userdata' is the pointer the caller passed to smtp_send_async().
 */
typedef void (*SMTPCallback)(int success, const char *errmsg, void *userdata);

/**
 * Submit an email for asynchronous delivery via the configured SMTP server.
 *
 * @param to_addr   Recipient address (single recipient).  No display name.
 * @param subject   Subject line (one line, no CR/LF).
 * @param body      Plain-text body (UTF-8).  CRLF or LF line endings both OK.
 * @param cb        Callback fired exactly once when the transaction
 *                  completes (success or failure).  May be NULL.
 * @param userdata  Opaque pointer passed back to the callback.  May be NULL.
 *
 * @returns 1 if the job was queued, 0 if the smtp module is not loaded
 *          or not configured (in which case cb is NOT called).
 */
extern int smtp_send_async(const char *to_addr,
                           const char *subject,
                           const char *body,
                           SMTPCallback cb,
                           void *userdata);

/**
 * Returns 1 if the smtp module is loaded AND has a usable configuration
 * (host + port + from-address present).  Use this to decide whether email
 * features should be exposed to users.
 */
extern int smtp_is_configured(void);

#endif /* OBBY_SMTP_H */
