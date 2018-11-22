#include "shared_library.hpp"

// get sockaddr. Supports IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int server_bind_port(int listener, int listen_port) {
    // this function binds socket listener to listen_port on all interfaces
    // Precondition: listener is a valid ipv4 socket and listen_port is free
    // Postconfition: socket listener is bound to listen_port on all interfaces
    sockaddr_in listen_addr {};
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(listen_port);
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    return bind(listener, (sockaddr*) &listen_addr, sizeof(sockaddr));
}


int get_listener(Options opt) {
    // socket(), set blocking/nonblocking, bind(), listen()
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


int loop_server_fork(Options opt) {
    // can be either blocking or non-blocking

}

int loop_server_nofork(Options opt) {
    // must be non-blocking otherwise simultaneous connections can't be handled

}


int loop_client_fork(Options opt) {
    // can be either blocking or non-blocking

}

int loop_client_nofork(Options opt) {
    // must be non-blocking otherwise simultaneous connections can't be handled

}
