/*
* SScript - See the sscript.doc
* (C) 1998 Drow <drow@wildstar.net>
* http://devplanet.fastethernet.net
*/

#include "sscript.h"
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <net/if.h>
#include <strings.h>
#include <sys/file.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#ifndef FNDELAY
#define FNDELAY O_NONBLOCK
#endif
#ifdef POSIX
#include <pwd.h>
#include <sys/utsname.h>
#endif

char global_var[9][1024]; /* need to find why gcc outputs warns without this */
char remoteIP[30];

char *sscript_lindex(char *input_string, int word_number)
{
 char *tokens[1024];
 static char tmpstring[1024];
 int i;
 strncpy(tmpstring,input_string,1024);
 (char *)tokens[i=0] = (char *)strtok(tmpstring, " ");
 while (((char *)tokens[++i] = (char *)strtok(NULL, " ")));
 tokens[i] = NULL;
 return(tokens[word_number]);
}

int sscript_connect(char *server, int port, char *virtual)
{
 struct sockaddr_in address;
 struct sockaddr_in la;
 int len, sockfd;
 sockfd = socket(AF_INET, SOCK_STREAM, 0);
 if(sockfd<1) 
 {
  errno = SSCRIPT_SOCKET_FAILED;
  return -1;
 }
 address.sin_family = AF_INET;
 address.sin_addr.s_addr = inet_addr(server);
 address.sin_port = htons(port);
 len = sizeof(address);
 if(virtual!=NULL)
 {
  la.sin_family = AF_INET;
  la.sin_addr.s_addr = inet_addr(virtual);
  la.sin_port = 0;
  bind(sockfd, (struct sockaddr *)&la, sizeof(la));
 }
 if(connect(sockfd, (struct sockaddr *)&address, len)<0)
 {
  errno = SSCRIPT_CONNECT_FAILED;
  return -1;
 }
 return sockfd;
}

int sscript_server(int port)
{
 int sockfd2, listen_len;
 struct sockaddr_in listen_addr;
 sockfd2 = socket(AF_INET, SOCK_STREAM, 0);
 if(sockfd2<1)
 {
  errno = SSCRIPT_SOCKET_FAILED;
  return -1;
 }
 listen_addr.sin_family = AF_INET;
 listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
 listen_addr.sin_port = htons(port);
 listen_len = sizeof(listen_addr); 
 if(bind(sockfd2, (struct sockaddr *)&listen_addr, listen_len))
 {
  errno = SSCRIPT_BIND_FAILED;
  return -1;
 }
 return sockfd2;
}

int sscript_wait_clients(int sockfd2, int port, int forking)
{
 int sockfd=(int)NULL,len,from_len,pid;
 struct sockaddr_in address;
 struct sockaddr_in from_addr;
 struct sockaddr_in listen_addr;
 listen_addr.sin_family = AF_INET;
 listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
 listen_addr.sin_port = htons(port);
 len = sizeof(address);
 listen(sockfd2, 5);
 for(;;)
 {
  if(forking) if(sockfd!=(int)NULL) close(sockfd);
  sockfd = accept(sockfd2, (struct sockaddr *)&address, &len);
  if(forking) if((pid=fork())) break;
 }
 from_len=sizeof(from_addr);
 memset(&from_addr, 0, sizeof(from_addr));
 if(getpeername(sockfd, (struct sockaddr *)&from_addr,&from_len) < 0)
 {
  strcpy(remoteIP,"unknown");
 }
 else
 {
  strcpy(remoteIP,inet_ntoa(from_addr.sin_addr));
 }
 return sockfd;
}

char *sscript_get_remote_ip()
{
 return remoteIP;
}

void sscript_disconnect(int sockfd)
{
 shutdown(sockfd,2);
 close(sockfd);
}

void sscript_dump(int sockfd, char *filename)
{
 char temp[1024]="";
 FILE *fpa;
 fpa=fopen(filename,"r");
 if(fpa==NULL) return;
 while(fgets(temp,1024,fpa)!=NULL)
  write(sockfd, temp, strlen(temp));
 fclose(fpa);
}

void sscript_ping(char *hostname)
{
 struct sockaddr_in other_addr;
 int sockfd, result;
 char temp[255];
 sockfd=socket(AF_INET, SOCK_STREAM, 0);
 if(sockfd<0)
 {
  errno = SSCRIPT_SOCKET_FAILED;
  return;
 }
 other_addr.sin_family = AF_INET;
 other_addr.sin_addr.s_addr = inet_addr(hostname);
 other_addr.sin_port = htons(7);
 connect(sockfd, (struct sockaddr*) &other_addr,sizeof(other_addr));
 result=write(sockfd,"ping\n",strlen("ping\n"));
 result=read(sockfd,temp,result);
 close(sockfd);
}

int sscript_test(char *hostname, int port)
{
 int sockfd;
 struct sockaddr_in other_addr;
 if((sockfd=socket(AF_INET, SOCK_STREAM, 0))<0)
 {
  errno = SSCRIPT_SOCKET_FAILED;
  return -1;
 }
 other_addr.sin_family = AF_INET;
 other_addr.sin_addr.s_addr = inet_addr(hostname);
 other_addr.sin_port = htons(port);
 if(connect(sockfd, (struct sockaddr*)&other_addr,sizeof(other_addr))==-1)
 {
  errno = SSCRIPT_CONNECT_FAILED;
  close(sockfd);
  return -1;
 }
 close(sockfd);
 return 0;
}

char *sscript_version()
{
 return ABOUT;
}

char *sscript_read(int sockfd, int chop)
{
 int i, result;
 char inchar;
 char string[1024];
 bzero(string,1024);
 strcpy(string,"");
 for(i=0;(result=read(sockfd,&inchar,1))!='\0';i++)
 {
  string[i]=inchar;
  if(inchar=='\n') break;
 }
 if (chop) string[i-1]=' ';
 strcpy(global_var[0],string);
 return global_var[0];
}

void sscript_write(int sockfd, char *string)
{
 write(sockfd, string, strlen(string));
}

int sscript_compare(char *case1, char *case2)
{
 return (strcmp(case1,case2));
}

char *sscript_lrange(char *input_string, int starting_at)
{
 char *tokens[555];
 static char tmpstring[512]="";
 int i;
 char out_string[512]="";
 strcpy(out_string,"");
 if(input_string==NULL) {
  strcpy(out_string," ");
  strcat(out_string,NULL);
  strcpy(global_var[1],out_string);
  return global_var[1]; }
 strcpy(tmpstring,input_string);
 (char *)tokens[i=0] = (char *)strtok(tmpstring, " ");
 while(((char *)tokens[++i] = (char *)strtok(NULL, " ")));
 tokens[i] = NULL;
 i++;
 if(i<starting_at) return (int)NULL;
 while(tokens[starting_at] != NULL)
 {
  strcat(out_string,tokens[starting_at]);
  strcat(out_string, " ");
  starting_at++;
 }
 strcpy(global_var[2],out_string);
 return global_var[2];
}

int sscript_udp_send(char *hostname, int port, char *msg)
{
 int udpsock;
 struct sockaddr_in udpaddr;
 udpsock = socket(AF_INET, SOCK_DGRAM, 0);
 if(udpsock<0)
 {
  errno = SSCRIPT_SOCKET_FAILED;
  return -1;
 }
 udpaddr.sin_family = AF_INET;
 udpaddr.sin_port = htons(port);
 udpaddr.sin_addr.s_addr = inet_addr(hostname);
 if(sendto(udpsock,msg,sizeof(msg),0,(struct sockaddr *)&udpaddr,sizeof(udpaddr))<0)
 {
  errno = SSCRIPT_UDPSEND_FAILED;
  return -1;
 }
 return 0;
}

char *sscript_udp_listen(int port)
{
 int udpsock,len;
 struct sockaddr_in udpaddr, from;
 char msg[255];
 udpsock = socket(AF_INET, SOCK_DGRAM, 0);
 if(udpsock<0)
 {
  errno = SSCRIPT_SOCKET_FAILED;
  return (char *)NULL;
 }
 udpaddr.sin_family = AF_INET;
 udpaddr.sin_addr.s_addr = INADDR_ANY;
 udpaddr.sin_port = htons(port);
 if(bind(udpsock,(struct sockaddr *)&udpaddr,sizeof(udpaddr))<0)
 {
  errno = SSCRIPT_BIND_FAILED;
  close(udpsock);
  return (char *)NULL;
 }
 len = sizeof(from);
 if(recvfrom(udpsock,msg,sizeof(msg),0,(struct sockaddr *)&from,&len)<0)
 {
  errno = SSCRIPT_UDPRECEIVE_FAILED;
  close(udpsock);
  return (char *)NULL;
 }
 close(udpsock);
 strcpy(global_var[3],msg);
 return global_var[3];
}

char *sscript_icmp_detect()
{
 int icmpsock,len,result,type;
 struct sockaddr_in icmpaddr;
 char readbuf[1024]="";
 char msg[255];
 if((icmpsock=socket(AF_INET, SOCK_RAW, IPPROTO_ICMP))<0)
 {
  errno = SSCRIPT_SOCKET_FAILED;
  return (char *)NULL;
 }
 icmpaddr.sin_family = AF_INET;
 icmpaddr.sin_addr.s_addr = INADDR_ANY;
 icmpaddr.sin_port = 0;
 if(bind(icmpsock,(struct sockaddr *)&icmpaddr,sizeof(icmpaddr))<0)
 {
  errno = SSCRIPT_BIND_FAILED;
  close(icmpsock);
  return (char *)NULL;
 }
 len=sizeof(icmpaddr);
 if(getsockname(icmpsock,(struct sockaddr *)&icmpaddr,&len)<0)
 {
  errno = SSCRIPT_GETSOCKETNAME_FAILED;
  close(icmpsock);
  return (char *)NULL;
 }
 if((result=read(icmpsock,readbuf,sizeof(readbuf)))<0)
 {
  errno = SSCRIPT_READ_FAILED;
  close(icmpsock);
  return (char *)NULL;
 }
 type=readbuf[20] & 0xff;
 sprintf(msg,"%d %d.%d.%d.%d ",type,readbuf[12]&0xff,readbuf[13]&0xff,readbuf[14]&0xff,readbuf[15]&0xff);
 close(icmpsock);
 strcpy(global_var[4],msg);
 return global_var[4];
}

char *sscript_resolve_host(char *hostname)
{
 struct hostent *hp;
 struct sockaddr_in from;
 char result[255];
 memset(&from, 0, sizeof(struct sockaddr_in));
 from.sin_family = AF_INET;
 hp=gethostbyname(hostname);
 if(hp==NULL) strcpy(result,"unknown");
 else
 {
  memcpy(&from.sin_addr,hp->h_addr,hp->h_length);
  strcpy(result,inet_ntoa(from.sin_addr));
 }
 strcpy(global_var[5],result);
 return global_var[5];
}

char *sscript_resolve_ip(char *ip)
{
 struct hostent *hp;
 struct sockaddr_in from;
 char result[255];
 from.sin_family = AF_INET;
 from.sin_addr.s_addr = inet_addr(ip);
 hp=gethostbyaddr((char *)&from.sin_addr, sizeof(struct in_addr),from.sin_family);
 if(hp==NULL) strcpy(result,"unknown");
 else strcpy(result,(char *)hp->h_name);;
 strcpy(global_var[6],result);
 return global_var[6];
}

char *sscript_get_localhost()
{
 char result[255];
 gethostname(result,sizeof(result));
 strcpy(global_var[7],result);
 return global_var[7];
}

void sscript_binary_send(int sockfd, char *string)
{
 char temp4[255], temp2[255];
 int cnt;
 FILE *fpa;
 sprintf(temp4,"uuencode %s %s > %s/.temp.uue 2>/dev/null",string,string,TMP_DIR);
 system(temp4);
 sprintf(temp2,"%s/.temp.uue",TMP_DIR);
 fpa=fopen(temp2,"r");
 if(fpa==NULL || fileno(fpa)<0) return;
 else {
  while((cnt = read(fileno(fpa), temp4, 250))>0)
  write(sockfd, temp4, cnt);
  if(fpa!=NULL) fclose(fpa);
  sprintf(temp4,"rm -f %s/.temp.uue",TMP_DIR);
  system(temp4);
 }
}

void sscript_binary_get(int sockfd)
{
 char temp2[255], temp4[255], inchar, inall[1024];
 FILE *fpa;
 int i;
 sprintf(temp2,"%s/.temp.uue",TMP_DIR);
 fpa=fopen(temp2,"w");
 if(fpa==NULL || fileno(fpa)<0) return;
 while(strcasecmp(inall,"end\n"))
 {
  bzero(inall, 1024);
  for(i=0;read(sockfd,&inchar,1)!='\0';i++)
  {
   inall[i]=inchar;
   if(inchar=='\n') break;
  }
  fputs(inall,fpa);
 }
 if(fpa!=NULL) fclose(fpa);
 sprintf(temp4,"uudecode %s/.temp.uue",TMP_DIR);
 system(temp4);
 sprintf(temp4,"rm -f %s/.temp.uue",TMP_DIR);
 system(temp4);
}

char *sscript_login_to_passwd(char *login)
{
#ifdef POSIX
 struct passwd *pw;
 pw = getpwnam(login);
 if(pw!=NULL) return pw->pw_passwd;
#endif
 return (char *)NULL;
}

char *sscript_uid_to_login(long my_uid)
{ 
#ifdef POSIX
 struct passwd *pw;
 pw = getpwuid(my_uid);
 if(pw!=NULL) return pw->pw_name;
#endif
 return (char *)NULL;
}

int sscript_sokstat(char *option, int sockfd)
{  
 int optlen=sizeof(int),optval=1;
 if(!strcasecmp(option,"sendbuf")) getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (char *)&optval, &optlen);
 else if(!strcasecmp(option,"recvbuf")) getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char *)&optval, &optlen);
 else if(!strcasecmp(option,"error")) getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (char *)&optval, &optlen);
 else if(!strcasecmp(option,"type")) getsockopt(sockfd, SOL_SOCKET, SO_TYPE, (char *)&optval, &optlen);
 else optval=-1;
 return optval;
}

char *sscript_time_read(int sockfd, int time_sec)
{
 struct timeval timeout;
 int max_fd;
 fd_set readfs, newfs;
 timeout.tv_sec=time_sec;
 timeout.tv_usec=0;
 FD_ZERO(&readfs);
 FD_SET(sockfd, &readfs);
 max_fd = sockfd;
 memcpy(&newfs, &readfs, sizeof(readfs));
 select(max_fd+1, &newfs, NULL, NULL, &timeout);
 if(FD_ISSET (sockfd, &newfs))
 {
  read(sockfd, global_var[8], sizeof(global_var[8]));
  return(global_var[8]); 
 }
 return("timeout");
}

void sscript_redir(int sockfd, int rsck)
{
 char buf[4096];
 fd_set readfs, newfs;
 int max_fd, len;
 FD_ZERO(&readfs);
 FD_SET(sockfd, &readfs);
 FD_SET(rsck, &readfs);
 if(sockfd>rsck) max_fd = sockfd;
 else max_fd = rsck;
 while(1) {
  memcpy(&newfs, &readfs, sizeof(readfs));
  select(max_fd+1, &newfs, NULL, NULL, NULL);
  if(FD_ISSET(sockfd, &newfs))
  {
   if((len=read(sockfd, buf, sizeof(buf)))<1) break;
   if(write(rsck, buf, len)!=len) break;
  }
  if(FD_ISSET(rsck, &newfs))
  {
   if((len=read(rsck, buf, sizeof(buf)))<1) break;
   if(write(sockfd, buf, len)!=len) break;
  }
 }
}

void sscript_nodelay(int sockfd)
{
 int i;
 if(( i = fcntl(sockfd, F_GETFL, 0)) == -1);
 else if (fcntl(sockfd, F_SETFL, i | FNDELAY) == -1);
}

