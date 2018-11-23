#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <arpa/inet.h> 
#include <fcntl.h>  // for setting non-blocking socket option
#include <thread>
#include <chrono>
#include "parse_arguments.hpp"
#include "shared_library.hpp"

using namespace std;

int main(int argc, char *argv[]) {

    // process arguments
    Options opt = parse_arguments(argc, argv, true);

    if (opt.fork)
        loop_client_fork(opt);
    else
        loop_client_nofork(opt);

    // how can the loop possibly return?
    return 0;
}

















int main_reference(int argc, char *argv[]) {
    if (argc != 4) {
       cerr << "Usage: " << argv[0] << "hostname port1 port2\n";
       exit(EXIT_FAILURE);
    }

    char *hostname = argv[1], *server_port1 = argv[2], *server_port2 = argv[3];;

    /////////////// connect server 1
    // hints
    struct addrinfo hints, *ai, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // get host address and connect
    int rv = getaddrinfo(hostname, server_port1, &hints, &ai);
    if (rv != 0) {
        cerr << gai_strerror(rv) << endl;
        exit(1);
    }

    int sfd1, sfd2;
    for (p = ai; p != NULL; p = p->ai_next) {
        if ((sfd1 = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) continue;
        if (connect(sfd1, p->ai_addr, p->ai_addrlen) == 0) break; // success
        close(sfd1);
    }
    if (p == NULL) { // no address succeeded
        perror("Could not connect");
        exit(2);
    }
    else cout << "Connection established\n";
    freeaddrinfo(ai);

    // set non-blocking connection
    fcntl(sfd1, F_SETFL, O_NONBLOCK);


    /////////////// connect server 2
    // hints
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // get host address and connect
    rv = getaddrinfo(hostname, server_port2, &hints, &ai);
    if (rv != 0) {
        cerr << gai_strerror(rv) << endl;
        exit(1);
    }

    for (p = ai; p != NULL; p = p->ai_next) {
        if ((sfd2 = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) continue;
        if (connect(sfd2, p->ai_addr, p->ai_addrlen) == 0) break; // success
        close(sfd2);
    }
    if (p == NULL) { // no address succeeded
        perror("Could not connect");
        exit(2);
    }
    else cout << "Connection established\n";
    freeaddrinfo(ai);

    // set non-blocking connection
    fcntl(sfd2, F_SETFL, O_NONBLOCK);

    /////////////// finished connection

    // prepare variables used by select()
    fd_set master, select_fd;      // master file descriptor list
    int fdmax = sfd2;          // maximum file descriptor number

    // recv loop
    // initialize buffer
    int buf_size = 10;
    char buf_send[buf_size+1];
    for (int i = 0; i < buf_size; i++)
        buf_send[i] = 'a' + i % 26;
    buf_send[buf_size] = '\0';

    int buf_read_size = 100;
    char buf_read[buf_read_size+1];

    FD_ZERO(&master);   // clear the master and temp sets
    FD_SET(sfd1, &master);
    FD_SET(sfd2, &master);
    cout << "Entering loop\n";
    int counter = 0;
    timeval tv = {1, 0};
    for(;;) {
        // use tv to hold the timer select() consumes. only write when the timer is up
        counter++;
        select_fd = master;
        int rv = select(fdmax + 1, &select_fd, NULL, NULL, &tv);
        int nbytes;
        switch (rv) {
            case -1:
                perror("Select on write");
                exit(4);
                break;
            case 0:
                // time's up, send
                nbytes = send(sfd1, buf_send, buf_size, 0);
                nbytes = send(sfd2, buf_send, buf_size, 0);
                cout << "send() returned with value " << nbytes << endl;
                if (nbytes <= 0) {
                    perror("send");
                    exit(5);
                }

                // reset timer to 3 second
                tv = {3, 0};

                break;
            case 1:
            case 2:
                nbytes = recv(sfd1, buf_read, buf_read_size, 0);
                nbytes = recv(sfd2, buf_read, buf_read_size, 0);
                cout << nbytes << " bytes received.\n";
                if (nbytes <= 0) {
                    perror("recv");
                    exit(6);
                }
                // successfully received
                buf_read[nbytes] = '\0';
                cout << "Here is the content transmitted: " << buf_read << endl;
                break;
            default:
                break;
        }
    }

    // aftermath
    close(sfd1);
    close(sfd2);
    return 0;
}
