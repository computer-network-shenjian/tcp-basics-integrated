#include <arpa/inet.h> 
#include <ifaddrs.h>
#include <netdb.h>

#define graceful(s, x) {\
    perror((s));\
    exit((x)); }

// shared functions
void *get_in_addr(struct sockaddr *sa);


// server specific
int server_bind_port(int listener, int listen_port);
int get_listener(Options opt);

int loop_server_fork(Options opt);
int loop_server_nofork(Options opt);


// client specific
int loop_client_nofork(Options opt);
int loop_client_fork(Options opt);
