#ifndef URL_H
#define URL_H

int url_is_valid(char *);
char *url_getfilename(char *);
char *download_file(char *, char **);
void download_file_async(char *, time_t, vFP);
void url_do_transfers_async(void);
void url_init(void);

#endif
