#ifndef URL_H
#define URL_H
#include "types.h"

int MODFUNC url_is_valid(char *);
char MODFUNC *url_getfilename(char *);
char MODFUNC *download_file(char *, char **);
void MODFUNC download_file_async(char *, time_t, vFP);
void MODFUNC url_do_transfers_async(void);
void MODFUNC url_init(void);

#endif
