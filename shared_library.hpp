#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include "parse_arguments.hpp"
#include <fcntl.h>  // setting non-blocking socket option
#include <iostream>
#include <unistd.h> // read

#define MAX_RECVLEN 1024

// gracefully perror and exit
#define graceful(s, x) {\
    perror((s));\
    exit((x)); }

// gracefully perror and return
#define graceful_return(s, x) {\
    perror((s));\
    return((x)); }

// shared functions
void *get_in_addr(struct sockaddr *sa);


// server specific
int server_bind_port(int listener, int listen_port);
    // this function binds socket listener to listen_port on all interfaces
    // Precondition: listener is a valid ipv4 socket and listen_port is not used
    // Postconfition: socket listener is bound to listen_port on all interfaces

int get_listener(Options opt);
    // socket(), set blocking/nonblocking, bind(), listen()
    // Precondition: the port field and block field of opt is properly set
    // Postcondition: an ipv4 socket that is initialized, bound, and set to listening to all interfaces is returned

int loop_server_fork(int listener, Options opt);
    // can be either blocking or non-blocking

int loop_server_nofork(int listener, Options opt);
    // must be non-blocking otherwise simultaneous connections can't be handled

int server_accept_client(int listener, bool block, fd_set &master, int &fdmax);

int server_communicate(int socketfd, bool block);
    // exchange messages with client according to the protocol
    // Precondition: a connection is already established on socketfd
    // Postcondition: a sequence of messages are exchanged with the client,
    // abording connection if network error is encountered.

    // not implemented
    // remember to handle partial sends here

int client_communicate(int socketfd, bool block);
    // exchange messages with server according to the protocol
    // Precondition: a connection is already established on socketfd
    // Postcondition: a sequence of messages are exchanged with the server,
    // abording connection if network error is encountered.

    // not implemented
    // remember to handle partial sends here

// client specific
int loop_client_nofork(Options opt);
int loop_client_fork(Options opt);
