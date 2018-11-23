#include "shared_library.hpp"

using namespace std;

// get sockaddr. Supports IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int server_bind_port(int listener, int listen_port) {
    sockaddr_in listen_addr {};
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(listen_port);
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    return bind(listener, (sockaddr*) &listen_addr, sizeof(sockaddr));
}


int get_listener() {
    int listen_port = stoi(opt.port);

    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener < 0)
        graceful("socket", 1);

    // lose the pesky "address already in use" error message
    int yes = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    // set nonblocking listener
    if (opt.block) {
        int val = fcntl(listener, F_GETFL, 0);
        if (val < 0) {
            close(listener);
            graceful_return("fcntl, GETFL", -2);
        }
        if (fcntl(listener, F_SETFL, val|O_NONBLOCK) < 0) {
            close(listener);
            graceful_return("fcntl, SETFL", -3);
        }
    }

    // bind
    if (server_bind_port(listener, listen_port) == -1)
        graceful("bind", 2);

    std::cout << "Listening on port " << listen_port << " on all interfaces...\n";

    // set listening
    listen(listener,50);

    return listener;
}


int loop_server_fork(int listener) {

}

int loop_server_nofork(int listener) {
    // prepare variables used by select()
    fd_set master, readfds;      // master file descriptor list
    FD_SET(listener, &master);
    int fdmax = listener;          // maximum file descriptor number

    // main loop
    // TODO: setup queueing mechanism
    int num_connections = 0;
    for(;;) {
        readfds = master; // copy at the last minutes
        int rv = select(fdmax+1, &readfds, NULL, NULL, NULL);
        cout << "select returned with value\t" << rv ;

        switch (rv) {
            case -1:
                graceful("select in main loop", 5);
                break;
            case 0:
                graceful("select returned 0\n", 6);
                break;
            default:
                for (int i = 0; i <= fdmax; i++) {
                    if (FD_ISSET(i, &readfds)) { // we got one
                        FD_CLR(i, &readfds);
                        if (i == listener) {
                            // handle new connections;
                            if (opt.num - num_connections > 0) {
                                server_accept_client(listener, opt.block, master, fdmax);
                                num_connections++;
                            }
                        } else {
                            // handle data
                            if (server_communicate(i, opt.block) == -1) {
                                num_connections--;
                                close(i); FD_CLR(i, &master);
                            }
                        }
                    }
                }
                break;
        }
    }
}

int server_communicate(int socketfd) {
    // debug
    std::cout << "server_communicate" << std::endl;

	fd_set readfds, writefds;
	struct timeval tv;

    char buffer[BUFFER_LEN];

    // server write string "StuNo"

    // server read int as student number, network byte order

    // server write string "pid"

    // server read int as client's pid, network byte order

    // server write string "TIME"

    // server read client's time as a string with a fixed length of 19 bytes

    // server write string "str*****", where ***** is a 5-digit random number ranging from 32768-99999, inclusively.

    // server read a random string with 

    // return -1 if the connection is closed
    return -1;
}

int client_communicate(int socketfd) {
    // debug
    std::cout << "client_communicate" << std::endl;
    char buffer[BUFFER_LEN];
    int nbytes = recv(socketfd, buffer, 20, 0);
    std::cout << nbytes << std::endl;

    // return -1 if the connection is closed
    return -1;
}

int server_accept_client(int listener, fd_set &master, int &fdmax) {
    // Accept connections from listener and insert them to the fd_set.

    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen = sizeof(remoteaddr);
    int newfd = accept(listener, (sockaddr *) &remoteaddr, &addrlen);

    if (newfd == -1) {
        graceful("server_accept_new_client", 7);
    } else {
        // set non-blocking connection
        if (opt.block) {
            int val = fcntl(newfd, F_GETFL, 0);
            if (val < 0) {
                close(newfd);
                graceful_return("fcntl, GETFL", -2);
            }
            if (fcntl(newfd, F_SETFL, val|O_NONBLOCK) < 0) {
                close(newfd);
                graceful_return("fcntl, SETFL", -3);
            }            
        }

        // add to the set
        FD_SET(newfd, &master); // add to master set
        if (newfd > fdmax)      // keep track of the max
            fdmax = newfd;

        char remoteIP[INET6_ADDRSTRLEN];
        std::cout << "New connection from " << inet_ntop(remoteaddr.ss_family,
                                            get_in_addr((struct sockaddr*) &remoteaddr),
                                            remoteIP, INET6_ADDRSTRLEN)
                << " on socket " << newfd << std::endl;
    }
    return newfd;

}


int loop_client_fork() {
    // can be either blocking or non-blocking

}

int loop_client_nofork() {
    // must be non-blocking otherwise simultaneous connections can't be handled

}
