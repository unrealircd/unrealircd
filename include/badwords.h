#ifndef __BADWORDS_H
#define __BADWORDS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tre/regex.h"

#define MAX_MATCH       1
#define MAX_WORDLEN	64

#define PATTERN		"\\w*%s\\w*"
#define REPLACEWORD	"<censored>"

#define BADW_TYPE_INVALID 0x0
#define BADW_TYPE_FAST    0x1
#define BADW_TYPE_FAST_L  0x2
#define BADW_TYPE_FAST_R  0x4
#define BADW_TYPE_REGEX   0x8

#define BADWORD_REPLACE 1
#define BADWORD_BLOCK 2

typedef struct _configitem_badword ConfigItem_badword;

struct _configitem_badword {
	ConfigItem_badword      *prev, *next;
	ConfigFlag	flag;
	char		*word, *replace;
	unsigned short	type;
	char		action;
	regex_t 	expr;
};

#endif
