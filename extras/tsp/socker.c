/*
	SOCKER Socket redirector  version 0.1
	Patrick Doyle  Oct 1998
	Based on tserver by Michael Johnson and Erik Troan

	Puts a normal stdin & stdout based program up on a port
	as a server process.  Each connection spawns a new copy
	of the program.

	Please notify me of any changes to this code tha you
	subsequently redistribute.  I can be contacted at
	patrick@minotaursoftware.com.

	Also, please leave my name and those of Michael Johnson
	and Erik Troan at the top of this file.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/wait.h>

#define ZOMBIE  /* Causes zombies to be collected.  Only works under Linux. */

#define debug printf
#undef debug
void debug(char *format, ...){}

int sock = -1;

void die(char *msg){
	perror(msg);
	exit(1);
}

void handle_sig(int signum){
	if(signum == SIGCHLD){
		/* Collect exit statuses.  Prevent zombies. */
		int status;
		while(0 < waitpid(-1, &status, WNOHANG));
	}else{
		fprintf(stderr, "\nSocker exiting normally.\n");
		close(sock);
		exit(0);
	}
}

#define asizeof(x) (sizeof(x)/sizeof(x[0]))

void setup_sig_handler(){
	int sa_num;
	static int sigs[] = {
		SIGHUP, SIGINT, SIGQUIT, SIGXCPU, SIGXFSZ, SIGTERM
#	ifdef ZOMBIE
		, SIGCHLD
#	endif
	};
	static struct sigaction sig_actions[asizeof(sigs)];
	debug("Starting setup_sig_handler\n");
	memset(sig_actions, 0, sizeof(sig_actions));
	sig_actions[0].sa_handler = handle_sig;
	sigemptyset(&(sig_actions[0].sa_mask));
	for(sa_num=1; sa_num < asizeof(sigs); sa_num++){
		memcpy(sig_actions+sa_num, sig_actions, sizeof(sig_actions[0]));
	}
	for(sa_num=0; sa_num < asizeof(sigs); sa_num++){
		if(sigaction(sigs[sa_num],  sig_actions+sa_num, NULL))
			die("sigaction");
	}
#	ifdef ZOMBIE
	siginterrupt(SIGCHLD, 0); /* Don't let SIGCHLD interrupt socket calls */
#	endif
	debug("Ending setup_sig_handler\n");
}

int main(int argc, char *argv[]) {
	struct sockaddr_in address;
	int conn, i, portnum;
	size_t addrLength = sizeof(struct sockaddr_in);

	fprintf(stderr, "SOCKER  Socket Redirector  Patrick Doyle  Oct 1998\n");

	if (argc < 3 || !(portnum = atoi(argv[1]))){
		fprintf(stderr, "Usage: socker {port_num} {command}\n");
		fprintf(stderr, "Waits for TCP connections on the given port, and then\n");
		fprintf(stderr, "spawns a new process executing {command} for each connection.\n");
		fprintf(stderr, "Exit status: 0=caught signal and exited; 1=error\n");
		exit(1);
	}

	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
		die("socket");

	debug("Calling setup_sig_handler\n");
	setup_sig_handler();

	/* Let the kernel reuse the socket address. This lets us run
	   twice in a row, without waiting for the (ip, port) tuple
	   to time out. */
	i = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void*)&i, i); 

	address.sin_family = AF_INET;
	address.sin_port = htons(portnum);
	memset(&address.sin_addr, 0, sizeof(address.sin_addr));

	if (bind(sock, (struct sockaddr *) &address, sizeof(address)))
		die("bind");

	if (listen(sock, 5))
		die("listen");

	while ((conn = accept(sock, (struct sockaddr *) &address, &addrLength)) >= 0) {
		if(fork()){ /* Parent; loop back to accept another */
			close(conn);
		}else{ /* Child; exec given command line */
			close(sock);
			/* Redirect stdin & stdout to socket */
			if(0 != dup2(conn, 0) || 1 != dup2(conn, 1))
				die("dup2 redirection");
			/* Turn off buffering */
			/* Note: this seems to have no effect beyond execvp */
			setbuf(stdin, 0);
			setbuf(stdout, 0);
			setbuf(stderr, 0);
			/* Execute command */
			execvp(argv[2], argv+2);
			/* Error if we get here */
			fprintf(stderr, "execvp failed.  Please make sure that '%s' refers to a valid program.\n", argv[2]);
			_exit(1)/*
				man fork says _exit should be called to prevent parent from
				being corrupted.
			*/;
		}
	}

	if (conn < 0) 
		die("accept");
	
	/* Shouldn't get here */
	close(sock);
	return 2;
}
