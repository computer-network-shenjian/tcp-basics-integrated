#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include "parse_arguments.hpp"
#include <fcntl.h>  // setting non-blocking socket option
#include <iostream>
#include <unistd.h> // read
#include <errno.h>
#include <string.h>
#include <cstdlib>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <time.h>

#define MAX_RECVLEN     32768
#define BUFFER_LEN      100000
#define WAIT_TIME_S     1           // 1 s
#define WAIT_TIME_US    500000      // 0.5 s

#define STR_1           "StuNo"
#define STR_2           "pid"
#define STR_3           "TIME"
#define STR_4           "end"

// gracefully perror and exit
inline void graceful(char *s, int x) { perror(s); exit(x); }

// gracefully perror and return
#define graceful_return(s, x) {\
    perror((s));\
    return((x)); }
/*
// gracefully close, perror and return
#define graceful_close(fd, s, x) {\
    close((fd));\
    perror((s));\
    return((x)); }
*/

#define minimum(a, b) (a < b ? a : b)

// shared functions
void *get_in_addr(struct sockaddr *sa);


// server specific
int server_bind_port(int listener, int listen_port);
    // this function binds socket listener to listen_port on all interfaces
    // Precondition: listener is a valid ipv4 socket and listen_port is not used
    // Postconfition: socket listener is bound to listen_port on all interfaces

int get_listener(const Options &opt);
    // socket(), set blocking/nonblocking, bind(), listen()
    // Precondition: the port field and block field of opt is properly set
    // Postcondition: an ipv4 socket that is initialized, bound, and set to listening to all interfaces is returned

int loop_server_fork(int listener, const Options &opt);
    // can be either blocking or non-blocking


int loop_server_nofork(int listener, const Options &opt);
    // must be non-blocking otherwise simultaneous connections can't be handled

int server_accept_client(int listener, bool block, fd_set *master, int *fdmax);


int server_communicate(int socketfd, const Options &opt);
    // exchange messages with client according to the protocol
    // Precondition: a connection is already established on socketfd
    // Postcondition: a sequence of messages are exchanged with the client,
    // abording connection if network error is encountered.

    // not implemented
    // remember to handle partial sends here
    // return 0: all good
    // return -1: select error
    // return -2: time up
    // return -3: client offline
    // return -4: not permitted to send
    // return -5: ready_to_send error
    // return -6: send error
    // return -7: message sent is of wrong quantity of byte
    // return -8: not permitted to recv
    // return -9: ready_to_recv error
    // return -10: not received exact designated quantity of bytes
    // return -11: write_file error


int client_communicate(int sockfd, const Options &opt);
    // exchange messages with server according to the protocol
    // Precondition: a connection is already established on socketfd
    // Postcondition: a sequence of messages are exchanged with the server,
    // abording connection if network error is encountered.

    // not implemented
    // remember to handle partial sends here
    
    //return:
    //   0 connect succeed
    //  -1 recv unexpected data, must reconnect
    //  -2 send error


// client specific
int client_nofork(const Options &opt);
// must be non-blocking 
int client_fork(const Options &opt);
// can be either blocking or non-blocking

int ready_to_send(int socketfd, const Options &opt);
    // return 1 means ready to send
    // return -1: select error
    // return -2: time up
    // return -3: server offline
    // return -4: not permitted to send

int ready_to_recv(int socketfd, const Options &opt);
    // return 1 means ready to recv
    // return -1: select error
    // return -2: time up
    // return -3: not permitted to recv

bool peer_is_disconnected(int socketfd);
    // check if peer is disconnected

int write_file(int stuNo, int pid, const char *time_str, const char *client_string);
    // write file as designated

int getCurrentTime(char *time_str);
    //format: yyyy-mm-dd hh:mm:ss, 19 bytes