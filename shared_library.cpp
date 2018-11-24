#include "shared_library.hpp"

using namespace std;

// get sockaddr. Supports IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
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


int get_listener(const Options &opt) {
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

    std::cout << "Listening on port " << listen_port << " on all interfaces...\n";

    // set listening
    listen(listener,50);

    return listener;
}


int loop_server_fork(int listener, const Options &opt) {
    int num_connections = 0;

    // main loop
    for (;;) {
        if (num_connections >= opt.num) {
            // saturated
            wait();
            num_connections--;
        } else {
            int newfd = server_accept_client(listener, false, (fd_set*)NULL, (int*)NULL);
            if (fork() == 0) {
                // in child
                client_communicate(newfd, opt);
            } else {
                // in parent
                num_connections++;
            }
        }
    }
}

int loop_server_nofork(int listener, const Options &opt) {
    // prepare variables used by select()
    fd_set master, readfds;      // master file descriptor list
    FD_SET(listener, &master);
    int fdmax = listener;          // maximum file descriptor number

    // main loop
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
                                server_accept_client(listener, opt.block, &master, &fdmax);
                                num_connections++;
                            }
                        } else {
                            // handle data
                            if (server_communicate(i, opt) == -1) {
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

int server_communicate(int socketfd, const Options &opt) {
    // return -1: select error
    // return -2: server offline
    // return -3: not permitted to write
    // return -4: ready_to_send error
    // return -5: send error
    // return -6: message sent is of wrong quantity of byte

    // debug
    std::cout << "server_communicate" << std::endl;

    char buffer[BUFFER_LEN];

    int val_send_ready, val_send;
    // server send a string "StuNo"
    val_send_ready = ready_to_send(socketfd);
    if (val_send_ready < 0) {
        if (val_send_ready == -1) {
            graceful_return("select", -1);
        }
        else if (val_send_ready == -2) {
            graceful_return("server offline", -2);
        }
        else if (val_send_ready == -3) {
            graceful_return("not permitted to write", -3);
        }
        else {
            graceful_return("ready_to_send", -4);
        }
    }
    std::string str_1 = "StuNo";
    val_send = send(socketfd, str_1.c_str(), str_1.length(), MSG_NOSIGNAL);
    if (val_send != str_1.length()) {
        if (errno == EPIPE) {
            graceful_return("server offline", -2);
        }
        else if (val_send == -1) {
            graceful_return("send", -5);
        }
        else {
            graceful_return("message sent is of wrong quantity of byte", -6);
        }
    }

    // server recv an int as student number, network byte order

    // server send a string "pid"

    // server recv an int as client's pid, network byte order

    // server send a string "TIME"

    // server recv client's time as a string with a fixed length of 19 bytes

    // server send a string "str*****", where ***** is a 5-digit random number ranging from 32768-99999, inclusively.

    // server recv a random string with length *****, and each character is in ASCII 0~255.

    // server send a string "end"

    // after server catch that client is closed, close s/c socket, write file

    // return 0 as success
    return 0;
}

int client_communicate(int socketfd, const Options &opt) {
    // debug
    std::cout << "client_communicate" << std::endl;
    char buffer[BUFFER_LEN];
    int nbytes = recv(socketfd, buffer, 20, 0);
    std::cout << nbytes << std::endl;

    // return -1 if the connection is closed
    return -1;
}

int server_accept_client(int listener, bool block, fd_set *master, int *fdmax) {
    // Accept connections from listener and insert them to the fd_set.

    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen = sizeof(remoteaddr);
    int newfd = accept(listener, (sockaddr *) &remoteaddr, &addrlen);

    if (newfd == -1) {
        graceful("server_accept_new_client", 7);
    } else {
        // set non-blocking connection
        if (block) {
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

        if (master != NULL && fdmax != NULL) { // if using select
            // add to the set
            FD_SET(newfd, master); // add to master set
            if (newfd > *fdmax)      // keep track of the max
                *fdmax = newfd;
        }


        char remoteIP[INET6_ADDRSTRLEN];
        std::cout << "New connection from " << inet_ntop(remoteaddr.ss_family,
                                            get_in_addr((struct sockaddr*) &remoteaddr),
                                            remoteIP, INET6_ADDRSTRLEN)
                << " on socket " << newfd << std::endl;
    }
    return newfd;

}


int _recv(int sockfd, fd_set &rfds, char *expecline, int cmp_len, const Options &opt);
// --ARGUMENTS--
//sockfd:    socket id
//rfds:      fd_set for select()
//expecline: comparing expecline and recvline, return -1 if they are not the same
//cmp_len:   comparing only first cmp_len words of expecline and recvline
// --RETURN--
// -1   recv unexpected data
//  0   nothing wrong, comparing succeed!

int _send(int sockfd, fd_set &wfds, char *sendline, int send_len, const Options &opt);
//function   client_send
//sockfd:    socket id
//rfds:      fd_set for select()
//sendline:  (const char*) for send()
//send_len:  sending only first send_len words of sendline
// --RETURN--
// -2   send error
//  0   nothing wrong

int client_fork(const Options &opt) {
    // can be either blocking or non-blocking

}


int client_nofork(const Options &opt) {
    // must be non-blocking

}

int ready_to_send(int socketfd) {
    // return 1 means ready to send
    // return -1: select error
    // return -2: time up
    // return -3: server offline
    // return -4: not permitted to send
    fd_set readfds, writefds;
    struct timeval tv;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_SET(socketfd, &readfds);
    FD_SET(socketfd, &writefds);
    tv.tv_sec = WAIT_TIME_S;
    tv.tv_usec = WAIT_TIME_US;

    int val_select = select(socketfd+1, &readfds, &writefds, NULL, &tv);
    if (val_select < 0) {
        graceful_return("select", -1);
    }
    else if (val_select == 0) {
        graceful_return("time up and no change", -2);
    }
	else if (FD_ISSET(socketfd, &readfds) && FD_ISSET(socketfd, &writefds)) {
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
		close(socketfd);
		graceful_return("server offline", -3);
	}
    else if (FD_ISSET(socketfd, &writefds)){
        FD_ZERO(&writefds);
        return 1;
    }
    else {
        graceful_return("not permitted to send", -4);
    }
}

int ready_to_recv(int socketfd) {
    // return 1 means ready to recv
    // return -1: select error
    // return -2: time up
    // return -3: not permitted to recv
    fd_set readfds;
    struct timeval tv;
    FD_ZERO(&readfds);
    FD_SET(socketfd, &readfds);
    tv.tv_sec = WAIT_TIME_S;
    tv.tv_usec = WAIT_TIME_US;

    int val_select = select(socketfd+1, &readfds, NULL, NULL, &tv);
    if (val_select < 0) {
        graceful_return("select", -1);
    }
    else if (val_select == 0) {
        graceful_return("time up and no change", -2);
    }
    else if (FD_ISSET(socketfd, &readfds)){
        FD_ZERO(&readfds);
        return 1;
    }
    else {
        graceful_return("not permitted to recv", -3);
    }
}
