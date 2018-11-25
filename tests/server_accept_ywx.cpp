#include "../shared_library.hpp"

using namespace std;

int loop_server_nofork_2000_connections(int listener, const Options &opt) {
    // prepare variables used by select()
    fd_set master, readfds;      // master file descriptor list
    FD_SET(listener, &master);
    int fdmax = listener;          // maximum file descriptor number

    // main loop
    int num_connections = 0;
    for(;;) {
        readfds = master; // copy at the last minutes
        int rv = select(fdmax+1, &readfds, NULL, NULL, NULL);

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
                            // debug: don't check for number of connections and
                            // let the select handle max connections
                            if (opt.block == false) { // must be taken with no fork
                                server_accept_client(listener, opt.block, &master, &fdmax);
                                num_connections++;
                            }
                            //if (opt.num - num_connections > 0) {
                            //    server_accept_client(listener, opt.block, &master, &fdmax);
                            //    num_connections++;
                            //}
                        } else {
                            // handle data
                            // debug: don't close connection
                            cout << "\nconnection\t" << num_connections << endl;
                            
                            
                            
                            if (false) {
                                num_connections--;
                                close(i); FD_CLR(i, &master);
                            }
                        }
                    }
                }
                break;
        }
    sleep(1);
    }

    return 0;
}


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
        loop_server_fork(listener, opt);
    else
        loop_server_nofork_2000_connections(listener, opt);

    // how can the loop possibly return?
    close(listener);
    return 0;
}
