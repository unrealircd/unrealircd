#ifndef __BADWORDS_H
#define __BADWORDS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_REGEX
#include <regex.h>
#else
#include "../extras/regex/regex.h"
#endif

#define MAX_MATCH       1
#define MAX_WORDLEN	64

#define PATTERN		"\\w*%s\\w*"
#define REPLACEWORD	"<censored>"

char *stripbadwords(char *, int);

#endif
