#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> 
#include <ifaddrs.h>
#include <netdb.h>
#include <thread>
#include <chrono>
#include <fcntl.h>  // setting non-blocking socket option
#include "parse_arguments.hpp"
#include "shared_library.hpp"

using namespace std;

int get_listener(Options opt) {
    // Precondition: the port field and block field of opt is properly set
    // Postcondition: an ipv4 socket that is initialized, bound, and set to listening to all interfaces is returned

    int listen_port = stoi(opt.port);

    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener < 0) 
        graceful("socket", 1);

    // lose the pesky "address already in use" error message
    int yes = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    // set nonblocking listener
    if (opt.block)
        fcntl(listener, F_SETFL, O_NONBLOCK);
    
    // bind
    if (server_bind_port(listener, listen_port) == -1) 
        graceful("bind", 2);

    cout << "Listening on port " << listen_port << " on all interfaces...\n";

    // set listening
    listen(listener,50);

    return listener;
}


int main(int argc, char *argv[]) {

    // process arguments
    Options opt = process_arguments(argc, argv, false);

    int listener = get_listener(opt.port);


}













// old main function, for reference
int main_reference(int argc, char *argv[]) {
    Options opt = process_arguments(argc, argv, false);

    int listen_port = stoi(opt.port);

    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener < 0) {
        perror("socket");
        exit(3);
    }

    // lose the pesky "address already in use" error message
    int yes = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    // set nonblocking listener
    if (opt.block)
        fcntl(listener, F_SETFL, O_NONBLOCK);
    
    // bind
    if (server_bind_port(listener, listen_port) == -1) {
        graceful("bind", 1);
    }

    cout << "Listening on port " << listen_port << " on all interfaces...\n";

    // set listening
    listen(listener,50);

    // accept with select
    // prepare variables used by select()
    fd_set master, readfds;      // master file descriptor list
    int fdmax = listener;          // maximum file descriptor number

    FD_ZERO(&master);
    FD_SET(listener, &master);
    fd_set select_fd = master; // copy
    if (select(fdmax+1, &select_fd, NULL, NULL, NULL) == -1) {
        perror("Select on read");
        exit(4);
    }

    socklen_t addrlen;
    struct sockaddr_storage remoteaddr; // client address
    int newfd = accept(listener, (struct sockaddr *) &remoteaddr, &addrlen);
    FD_SET(newfd, &master);
    if (newfd > fdmax) fdmax = newfd; // update fdmax
    cout << "A client is accepted on sfd " << newfd << endl;

    // set non-blocking connection
    fcntl(newfd, F_SETFL, O_NONBLOCK);

    // main loop
    // initialize buffer
    int buf_send_size = 10;
    char buf_send[buf_send_size+1];
    for (int i = 0; i < buf_send_size; i++)
        buf_send[i] = 'a' + i % 26;
    buf_send[buf_send_size] = '\0';

    int buf_read_size = 100;
    char buf_read[buf_read_size+1];

    // client info storage
    char remoteIP[INET6_ADDRSTRLEN]; 
    timeval tv = {1, 0}; // only send when the timer is up
    for(;;) {
        readfds = master; // copy at the last minutes
        int rv = select(fdmax+1, &readfds, NULL, NULL, &tv);
        int nbytes;
        switch (rv) {
            case -1:
                perror("select on write");
                exit(4);
                break;
            case 0:
                // time's up, send
                // run through the existing connections looking for data to send
                cout << "time's up\n";
                for (int i = 0; i <= fdmax; i++) {
                    if (FD_ISSET(i, &readfds)) { // we got one!!
                        // time's up, send
                        nbytes = send(i, buf_send, buf_send_size, 0);
                        if (nbytes <= 0) {
                            perror("send");
                            exit(5);
                        }
                        cout << nbytes << " bytes sent.\n";
                    }
                }
                // reset timer to 1 second
                tv = {1, 0};
                break;
            case 1:
                // run through the existing connections looking for data to read
                for (int i = 0; i <= fdmax; i++) {
                    if (FD_ISSET(i, &readfds)) { // we got one!!
                        if (i == listener) {
                            // handle new connections
                            addrlen = sizeof remoteaddr;
                            newfd = accept(listener, (struct sockaddr *) &remoteaddr, &addrlen);

                            // set non-blocking connection
                            fcntl(newfd, F_SETFL, O_NONBLOCK);

                            if (newfd == -1) {
                                perror("accept");
                            } else {
                                FD_SET(newfd, &master); // add to master set
                                if (newfd > fdmax)      // keep track of the max
                                    fdmax = newfd;

                                cout << "New connection from " << inet_ntop(remoteaddr.ss_family,
                                                                    get_in_addr((struct sockaddr*) &remoteaddr),                                                   
                                                                    remoteIP, INET6_ADDRSTRLEN)
                                     << " on socket " << newfd << endl;
                            }
                        } else { 
                            // handle data from a client
                            nbytes = recv(i, buf_read, buf_read_size, 0);
                            if (nbytes <= 0) {
                                perror("recv");
                                exit(6);
                            }
                            cout << nbytes << " bytes received.\n";
                        }
                    }
                }
            default: break;
        }
    }

    // finish up
    close(listener);
    close(newfd);
    return 0;
}
