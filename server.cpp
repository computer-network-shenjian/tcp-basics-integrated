#include "shared_library.hpp"

using namespace std;


int main(int argc, char *argv[]) {

    // process arguments
    Options opt = parse_arguments(argc, argv, false);

    // socket(), set blocking/nonblocking, bind(), listen()
    int listener = get_listener(opt);

    int keepalive = 1;      // enable keepalive 
    int keepidle = 1;       // if no data coming in 1 second, start detecting...
    int keepinterval = 1;   // detecting interval: 1 second
    int keepcount = 2;      // detecting time = 2: if neither of them receive ACK from peer, peer disconnected
    setsockopt(listener, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepalive , sizeof(keepalive ));
    setsockopt(listener, SOL_TCP, TCP_KEEPIDLE, (void*)&keepidle , sizeof(keepidle ));
    setsockopt(listener, SOL_TCP, TCP_KEEPINTVL, (void *)&keepinterval , sizeof(keepinterval ));
    setsockopt(listener, SOL_TCP, TCP_KEEPCNT, (void *)&keepcount , sizeof(keepcount ));

    if (opt.fork)
        //loop_server_fork(listener, opt);
        ;
    else
        loop_server_nofork(listener, opt);

    // how can the loop possibly return?
    close(listener);
    return 0;
}
