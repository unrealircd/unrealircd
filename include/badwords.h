#ifndef __BADWORDS_H
#define __BADWORDS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

#define MAX_MATCH       1
#define MAX_WORDLEN	64
#define MAX_WORDS	50

#define PATTERN		"\\w*%s\\w*"
#define REPLACEWORD	"<censored>"

char 	*stripbadwords(char *, int);
int	 loadbadwords(char *, int);
void	 freebadwords(void);

#endif
