#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define SSCRIPT_SOCKET_FAILED             10
#define SSCRIPT_BIND_FAILED               11
#define SSCRIPT_GETSOCKETNAME_FAILED      12
#define SSCRIPT_FLAGS_FAILED              13
#define SSCRIPT_CONNECT_FAILED            20
#define SSCRIPT_UDPSEND_FAILED            31
#define SSCRIPT_UDPRECEIVE_FAILED         32
#define SSCRIPT_READ_FAILED               33

#define ABOUT "Socket Script library 2.0 by Patrick Lambert (drow@post.com)"
#define POSIX
#define TMP_DIR "."

char *sscript_lindex(char *input_string, int word_number);
int sscript_connect(char *server, int port, char *virtual);
int sscript_server(int port);
int sscript_wait_clients(int sockfd2, int port, int forking);
char *sscript_get_remote_ip();
void sscript_disconnect(int sockfd);
void sscript_dump(int sockfd, char *filename);
void sscript_ping(char *hostname);
int sscript_test(char *hostname, int port);
char *sscript_version();
char *sscript_read(int sockfd, int chop);
void sscript_write(int sockfd, char *string);
int sscript_compare(char *case1, char *case2);
char *sscript_lrange(char *input_string, int starting_at);
int sscript_udp_send(char *hostname, int port, char *msg);
char *sscript_udp_listen(int port);
char *sscript_icmp_detect();
char *sscript_resolve_host(char *hostname);
char *sscript_resolve_ip(char *ip);
char *sscript_get_localhost();
void sscript_binary_send(int sockfd, char *string);
void sscript_binary_get(int sockfd);
char *sscript_login_to_passwd(char *login);
char *sscript_uid_to_login(long my_uid);
int sscript_sokstat(char *option, int sockfd);
char *sscript_time_read(int sockfd, int time_sec);
void sscript_redir(int sockfd, int rsck);
void sscript_nodelay(int sockfd);
