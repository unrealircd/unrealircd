#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* 
 * The vulnerable pattern in src/conf.c:3665 uses strcpy+strcat into a
 * fixed-size stack buffer (typically MAX_PATH or 260 bytes on Windows,
 * or a similar fixed size). We test that path construction never exceeds
 * the buffer. Since we cannot directly call the internal scanning function
 * without a full environment, we simulate the exact vulnerable pattern
 * and verify the invariant that must hold: combined length must be bounded.
 */

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

/* Extern declaration - the real conf.c should expose or we link against it.
 * Since the vulnerable code is inside a static/internal function, we test
 * the invariant by reproducing the exact pattern and asserting safe behavior.
 * In a real build, link against the object file from src/conf.c */
extern int conf_load(const char *path);

START_TEST(test_conf_path_buffer_overflow)
{
    /* Invariant: Buffer reads/writes never exceed the declared buffer length.
     * Any path passed to configuration directory scanning must be rejected
     * or truncated if it would overflow a MAX_PATH-sized stack buffer. */
    const char *payloads[] = {
        /* Exact exploit: path that exceeds MAX_PATH when combined with a filename */
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA/",
        /* 10x overflow: massively oversized path */
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB/",
        /* Boundary: exactly MAX_PATH - 1 chars (should be rejected with any filename) */
        "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"
        "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"
        "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"
        "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC/",
        /* Valid input: short path that fits easily */
        "/tmp/burp/",
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        size_t len = strlen(payloads[i]);
        /* The invariant: any path used must leave room for a filename */
        if (len >= MAX_PATH - 1) {
            /* conf_load should fail gracefully (return non-zero) on oversized paths */
            int ret = conf_load(payloads[i