#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <queue>

#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/prctl.h>
#include <sys/wait.h>

#include "parse_arguments.hpp"

#define MAX_SENDLEN     32768
#define MAX_RECVLEN     32768
#define BUFFER_LEN      100000
#define WAIT_TIME_S     10           // 10 s
#define WAIT_TIME_US    500000      // 0.5 s
#define MAX_CONN        1000        // no more than 1000 connections is allowed

#define STR_1           "StuNo"
#define STR_2           "pid"
#define STR_3           "TIME"
#define STR_4           "end"

#define STU_NO          1551713

// gracefully perror and exit
inline void graceful(const char *s, int x) { perror(s); exit(x); }

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

#define graceful_nonfork(opt) {client_nofork(opt);}

#define minimum(a, b) (a < b ? a : b)

//Options opt; // work around for not retransmission from signal

struct Socket {
};

class Socket {
    public:
        Socket(int socket_fd, bool is_server) 
        : socket_fd(socket_fd), stage_send(is_server)
        {
        }

        int socketfd;
        bool stage_send; // if is server, the first stage should be send
        bool has_been_active = false;
        int stage = 0;
        int bytes_processed = 0;
}

const int max_active_connections = 200;

const int timeout_seconds = 2;
const int timeout_microseconds = 0;
int remove_dead_connections(fd_set &master, const int fdmax, const int listener, const int* const active_connections);
// remove connections that has been unresponsive in a certain period of time
// precondition: master is a set of sockets the connections to be removed from
//      listener is the 

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
    // return -3: peer offline
    // return -4: not permitted to send
    // return -5: ready_to_send error
    // return -6: send error
    // return -7: message sent is of wrong quantity of byte
    // return -8: not permitted to recv
    // return -9: ready_to_recv error
    // return -10: not received exact designated quantity of bytes
    // return -11: write_file error


int client_communicate(int socketfd, const Options &opt);
    // exchange messages with server according to the protocol
    // Precondition: a connection is already established on socketfd
    // Postcondition: a sequence of messages are exchanged with the server,
    // abording connection if network error is encountered.

    // not implemented
    // remember to handle partial sends here
    // return 0: all good
    // return -1: select error
    // return -2: time up
    // return -3: peer offline
    // return -4: not permitted to send
    // return -5: ready_to_send error
    // return -6: send error
    // return -7: message sent is of wrong quantity of byte
    // return -8: not permitted to recv
    // return -9: ready_to_recv error
    // return -10: not received exact designated quantity of bytes
    // return -11: write_file error
    // return -12: not received correct string

// client specific
int client_nofork(const Options &opt);
    // must be non-blocking 
int client_fork(const Options &opt);
    // can be either blocking or non-blocking

int creat_connection(const Options &opt);
    //reconnect implemented, only return after correctly communicating with server 

int ready_to_send(int socketfd, const Options &opt);
    // return 1 means ready to send
    // return -1: select error
    // return -2: time up
    // return -3: peer offline
    // return -4: not permitted to send

int ready_to_recv(int socketfd, const Options &opt);
    // return 1 means ready to recv
    // return -1: select error
    // return -2: time up
    // return -3: not permitted to recv

bool peer_is_disconnected(int socketfd);
    // check if peer is disconnected

int write_file(const char *str_filename, int stuNo, int pid, const char *time_str, const unsigned char *client_string);
    // write file as designated

int str_current_time(char *time_str);
    // format: yyyy-mm-dd hh:mm:ss, 19 bytes

int create_random_str(const int length, unsigned char *random_string);
    // create random string with designated length

bool same_string(const char *str1, const char *str2, const int cmp_len);
    // compare strings from first byte to designated length

int parse_str(const char *str);
    // parse "str*****" into a 5-digit number

int send_thing(const int socketfd, const char *str, const Options &opt, const int send_len);

int recv_thing(const int socketfd, char *buffer, const Options &opt, const int recv_len);
