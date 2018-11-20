#include <arpa/inet.h> 
#include <ifaddrs.h>
#include <netdb.h>

#define graceful(s, x) {\
    perror((s));\
    exit((x)); }

int server_bind_port(int listener, int listen_port);
void *get_in_addr(struct sockaddr *sa);
