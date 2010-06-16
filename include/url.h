#ifndef URL_H
#define URL_H
#include "types.h"

int MODFUNC url_is_valid(const char *);
char MODFUNC *url_getfilename(const char *url);
char MODFUNC *download_file(const char *, char **);
void MODFUNC download_file_async(const char *, time_t, vFP, void *callback_data);
void MODFUNC url_do_transfers_async(void);
void MODFUNC url_init(void);

#endif
