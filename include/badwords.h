#ifndef __BADWORDS_H
#define __BADWORDS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../extras/regexp/include/tre/regex.h"

#define MAX_MATCH       1
#define MAX_WORDLEN	64

#define PATTERN		"\\w*%s\\w*"
#define REPLACEWORD	"<censored>"

#endif
