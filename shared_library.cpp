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
    struct sockaddr_in listen_addr;
    memset(&listen_addr, 0, sizeof(listen_addr));
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
    listen(listener, 1000);

    return listener;
}

int loop_server_fork(int listener, const Options &opt) {
    int num_connections = 0;

    // main loop
    for (;;) {
        if (num_connections >= (int)opt.num) {
            // saturated
            wait();
            num_connections--;
        } else {
            int newfd = server_accept_client(listener, false, (fd_set*)NULL, (int*)NULL);
            int fpid = fork();
            switch (fpid) {
                case 0:
                    // in child
                    client_communicate(newfd, opt);
                    break;
                case -1:
                    // error
                    graceful("loop_server_fork", -20);
                    break;
                default:
                    // in parent
                    num_connections++;
                    break;
            }
        }
    }
    return 0;
}

int loop_server_nofork(int listener, const Options &opt) {
    // prepare variables used by select()
    fd_set master, readfds, writefds;      // master file descriptor list
    FD_SET(listener, &master);
    int fdmax = listener;          // maximum file descriptor number 

    // main loop
    for(;;) {
        readfds = master; // copy at the last minutes
        writefds = master;
        int rv = select(fdmax+1, &readfds, &writefds, NULL, NULL);
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
                    if (!FD_ISSET(i, &readfds) && FD_ISSET(i, &writefds))  { // we got a writable socket
                        // because the first message requires the client socket to be writable

                        // handle data
                        if (server_communicate(i, opt) < 0) {
                            close(i); FD_CLR(i, &master);
                        }
                    } else if (FD_ISSET(i, &readfds) && (i == listener)) { // we got a new client
                        // handle new connections;
                        server_accept_client(listener, opt.block, &master, &fdmax);
                    }
                }
                break;
        }
    }
    return 0;
}

int server_communicate(int socketfd, const Options &opt) {
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

    // debug
    std::cout << "server_communicate" << std::endl;

    int val_send_ready, val_send;
    std::string str;
    // server send a string "StuNo"
    val_send_ready = ready_to_send(socketfd, opt);
    if (val_send_ready < 0) {
        if (val_send_ready == -1) {
            graceful_return("select", -1);
        }
        else if (val_send_ready == -2) {
            graceful_return("time up", -2)
        }
        else if (val_send_ready == -3) {
            graceful_return("peer offline", -3);
        }
        else if (val_send_ready == -4) {
            graceful_return("not permitted to send", -4);
        }
        else {
            graceful_return("ready_to_send", -5);
        }
    }
    str = STR_1;
    val_send = send(socketfd, str.c_str(), str.length(), MSG_NOSIGNAL);
    if (val_send != (int)str.length()) {
        if (errno == EPIPE) {
            graceful_return("peer offline", -3);
        }
        else if (val_send == -1) {
            graceful_return("send", -6);
        }
        else {
            graceful_return("message sent is of wrong quantity of byte", -7);
        }
    }

    int val_recv_ready, val_recv, total_recv;
    char buffer[BUFFER_LEN] = {0};
    // server recv an int as student number, network byte order
    uint32_t h_stuNo = 0;
    uint32_t n_stuNo = 0;

    val_recv_ready = ready_to_recv(socketfd, opt);
    if (val_recv_ready < 0) {
        if (val_recv_ready == -1) {
            graceful_return("select", -1);
        }
        else if (val_recv_ready == -2) {
            graceful_return("time up", -2);
        }
        else if (val_recv_ready == -3) {
            graceful_return("not permitted to recv", -8);
        }
        else {
            graceful_return("ready_to_recv", -9);
        }
    }

    memset(buffer, 0, sizeof(char) * BUFFER_LEN);
    total_recv = 0;
    while (total_recv < (int)sizeof(uint32_t)) {
        val_recv = recv(socketfd, buffer+total_recv, sizeof(uint32_t), 0);
        if (val_recv < 0) {
            graceful_return("recv", -10);
        }
        else if (val_recv == 0) {
            graceful_return("peer offline", -3);
        }
        else {
            total_recv += val_recv;
        }
    }
    memcpy(&n_stuNo, buffer, sizeof(uint32_t));
    h_stuNo = ntohl(n_stuNo);

    // server send a string "pid"
    val_send_ready = ready_to_send(socketfd, opt);
    if (val_send_ready < 0) {
        if (val_send_ready == -1) {
            graceful_return("select", -1);
        }
        else if (val_send_ready == -2) {
            graceful_return("time up", -2)
        }
        else if (val_send_ready == -3) {
            graceful_return("peer offline", -3);
        }
        else if (val_send_ready == -4) {
            graceful_return("not permitted to send", -4);
        }
        else {
            graceful_return("ready_to_send", -5);
        }
    }
    str = STR_2;
    val_send = send(socketfd, str.c_str(), str.length(), MSG_NOSIGNAL);
    if (val_send != (int)str.length()) {
        if (errno == EPIPE) {
            graceful_return("peer offline", -3);
        }
        else if (val_send == -1) {
            graceful_return("send", -6);
        }
        else {
            graceful_return("message sent is of wrong quantity of byte", -7);
        }
    }

    // server recv an int as client's pid, network byte order
    uint32_t h_pid = 0;
    uint32_t n_pid = 0;

    val_recv_ready = ready_to_recv(socketfd, opt);
    if (val_recv_ready < 0) {
        if (val_recv_ready == -1) {
            graceful_return("select", -1);
        }
        else if (val_recv_ready == -2) {
            graceful_return("time up", -2);
        }
        else if (val_recv_ready == -3) {
            graceful_return("not permitted to recv", -8);
        }
        else {
            graceful_return("ready_to_recv", -9);
        }
    }

    memset(buffer, 0, sizeof(char) * BUFFER_LEN);
    total_recv = 0;
    while (total_recv < (int)sizeof(uint32_t)) {
        val_recv = recv(socketfd, buffer+total_recv, sizeof(uint32_t), 0);
        if (val_recv < 0) {
            graceful_return("recv", -10);
        }
        else if (val_recv == 0) {
            graceful_return("peer offline", -3);
        }
        else {
            total_recv += val_recv;
        }
    }
    memcpy(&n_pid, buffer, sizeof(uint32_t));
    h_pid = ntohl(n_pid);

    // server send a string "TIME"
    val_send_ready = ready_to_send(socketfd, opt);
    if (val_send_ready < 0) {
        if (val_send_ready == -1) {
            graceful_return("select", -1);
        }
        else if (val_send_ready == -2) {
            graceful_return("time up", -2)
        }
        else if (val_send_ready == -3) {
            graceful_return("peer offline", -3);
        }
        else if (val_send_ready == -4) {
            graceful_return("not permitted to send", -4);
        }
        else {
            graceful_return("ready_to_send", -5);
        }
    }
    str = STR_3;
    val_send = send(socketfd, str.c_str(), str.length()+1, MSG_NOSIGNAL);
    if (val_send != (int)str.length()) {
        if (errno == EPIPE) {
            graceful_return("peer offline", -3);
        }
        else if (val_send == -1) {
            graceful_return("send", -6);
        }
        else {
            graceful_return("message sent is of wrong quantity of byte", -7);
        }
    }

    // server recv client's time as a string with a fixed length of 19 bytes
    char time_buf[20] = {0};

    val_recv_ready = ready_to_recv(socketfd, opt);
    if (val_recv_ready < 0) {
        if (val_recv_ready == -1) {
            graceful_return("select", -1);
        }
        else if (val_recv_ready == -2) {
            graceful_return("time up", -2);
        }
        else if (val_recv_ready == -3) {
            graceful_return("not permitted to recv", -8);
        }
        else {
            graceful_return("ready_to_recv", -9);
        }
    }

    memset(buffer, 0, sizeof(char) * BUFFER_LEN);
    total_recv = 0;
    while (total_recv < 19) {
        val_recv = recv(socketfd, buffer+total_recv, 19, 0);
        if (val_recv < 0) {
            graceful_return("recv", -10);
        }
        else if (val_recv == 0) {
            graceful_return("peer offline", -3);
        }
        else {
            total_recv += val_recv;
        }
    }
    if (total_recv != 19) {
        graceful_return("not received exact designated quantity of bytes", -10);
    }
    else {
        memcpy(time_buf, buffer, 19);
    }

    // server send a string "str*****", where ***** is a 5-digit random number ranging from 32768-99999, inclusively.
    val_send_ready = ready_to_send(socketfd, opt);
    if (val_send_ready < 0) {
        if (val_send_ready == -1) {
            graceful_return("select", -1);
        }
        else if (val_send_ready == -2) {
            graceful_return("time up", -2)
        }
        else if (val_send_ready == -3) {
            graceful_return("peer offline", -3);
        }
        else if (val_send_ready == -4) {
            graceful_return("not permitted to send", -4);
        }
        else {
            graceful_return("ready_to_send", -5);
        }
    }
    int random = rand() % 67232 + 32768;
    std::stringstream ss;
    ss << "str" << random << '\0';
    str = ss.str();
    val_send = send(socketfd, str.c_str(), str.length()+1, MSG_NOSIGNAL);
    if (val_send != (int)str.length()) {
        if (errno == EPIPE) {
            graceful_return("peer offline", -3);
        }
        else if (val_send == -1) {
            graceful_return("send", -6);
        }
        else {
            graceful_return("message sent is of wrong quantity of byte", -7);
        }
    }
    // server recv a random string with length *****, and each character is in ASCII 0~255.
    unsigned char client_string[BUFFER_LEN] = {0};
    memset(buffer, 0, sizeof(char) * BUFFER_LEN);
    total_recv = 0;
    while (total_recv < random){
        val_recv_ready = ready_to_recv(socketfd, opt);
        if (val_recv_ready < 0) {
            if (val_recv_ready == -1) {
                graceful_return("select", -1);
            }
            else if (val_recv_ready == -2) {
                graceful_return("time up", -2);
            }
            else if (val_recv_ready == -3) {
                graceful_return("not permitted to recv", -8);
            }
            else {
                graceful_return("ready_to_recv", -9);
            }
        }

        val_recv = recv(socketfd, buffer+total_recv, minimum(random-total_recv, MAX_RECVLEN), 0);
        if (val_recv < 0) {
            graceful_return("recv", -10);
        }
        else if (val_recv == 0) {
            graceful_return("peer offline", -3);
        }
        else if (val_recv != minimum(random-total_recv, MAX_RECVLEN)) {
            graceful_return("not received exact designated quantity of bytes", -10);
        }
        else {
            total_recv += val_recv;
        }

        memcpy(client_string, buffer, random);
    }    

    // server send a string "end"
    val_send_ready = ready_to_send(socketfd, opt);
    if (val_send_ready < 0) {
        if (val_send_ready == -1) {
            graceful_return("select", -1);
        }
        else if (val_send_ready == -2) {
            graceful_return("time up", -2)
        }
        else if (val_send_ready == -3) {
            graceful_return("peer offline", -3);
        }
        else if (val_send_ready == -4) {
            graceful_return("not permitted to send", -4);
        }
        else {
            graceful_return("ready_to_send", -5);
        }
    }
    str = STR_4;
    val_send = send(socketfd, str.c_str(), str.length(), MSG_NOSIGNAL);
    if (val_send != (int)str.length()) {
        if (errno == EPIPE) {
            graceful_return("peer offline", -3);
        }
        else if (val_send == -1) {
            graceful_return("send", -6);
        }
        else {
            graceful_return("message sent is of wrong quantity of byte", -7);
        }
    }

    // after server catch that client is closed, close s/c socket, write file

    peer_is_disconnected(socketfd);
    if (write_file(h_stuNo, h_pid, time_buf, client_string) == -1) {
        graceful_return("write_file", -11);
    }

    // return 0 as success
    return 0;
}

int client_nofork(const Options &opt) {
    for(unsigned int i=0; i<opt.num; i++)
        creat_connection(opt);
    return 0;
}

int client_fork(const Options &opt) {
    // int status;
    unsigned int i, j;
    for(i=0; i<opt.num; i++)
    {
        pid_t fpid;
        fpid = fork();
        if(fpid < 0)
            graceful("fork", -10);
        else if(fpid == 0)
            break;
        else
            continue;
    }

    if(i == opt.num)    //fp
        for(j=0; j<opt.num; j++)
            wait();
    else
        creat_connection(opt);

    return 0;
}

int creat_connection(const Options &opt) {
    int sockfd;
    fd_set fds;      
    int error = -1, slen = sizeof(int);

    struct sockaddr_in   servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(stoi(opt.port));
    if(inet_pton(AF_INET, opt.ip.c_str(), &servaddr.sin_addr) < 0)
        graceful("Invalid ip address", -1);

    //reconnection flag
    bool reconn = true;
    while(reconn)
    {
        if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            graceful("socket", -2);

        if(!opt.block)
        {
            //nonblock
            int flags;
            flags = fcntl(sockfd, F_GETFL, 0);
            fcntl(sockfd, F_SETFL, flags | O_NONBLOCK); 

            if(connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1)
            {
                //EINPROGRESS means connection is in progress
                if(errno != EINPROGRESS)
                    graceful("connect", -3);

                
                FD_ZERO(&fds);      
                FD_SET(sockfd, &fds);       
                int select_rtn;

                if((select_rtn = select(sockfd+1, NULL, &fds, NULL, NULL)) > 0)
                {
                    getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, (socklen_t *)&slen);
                    //error == 0 means connect succeeded
                    if(error)
                        graceful("connect", -3);
                }
            }
            //connect succeed   
        }
        //block
        else
        {
            if(connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1)
                graceful("connect", -3);
        }

        if(client_communicate(sockfd, opt) == -1)
        {
            close(sockfd);
                continue;
        }
        /*
            SO_LINGER check
        */
        close(sockfd);
        reconn = false;
    }
    return 0;
}

int client_communicate(int socketfd, const Options &opt) {
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

    // debug
    std::cout << "client_communicate" << std::endl;

    int val_recv_ready, val_recv, total_recv;
    char buffer[BUFFER_LEN] = {0};
    // recv "StuNo" from server
    val_recv_ready = ready_to_recv(socketfd, opt);
    if (val_recv_ready < 0) {
        if (val_recv_ready == -1) {
            graceful_return("select", -1);
        }
        else if (val_recv_ready == -2) {
            graceful_return("time up", -2);
        }
        else if (val_recv_ready == -3) {
            graceful_return("not permitted to recv", -8);
        }
        else {
            graceful_return("ready_to_recv", -9);
        }
    }

    memset(buffer, 0, sizeof(char) * BUFFER_LEN);
    total_recv = 0;
    while (total_recv < (int)strlen(STR_1)) {
        val_recv = recv(socketfd, buffer+total_recv, strlen(STR_1), 0);
        if (val_recv < 0) {
            graceful_return("recv", -10);
        }
        else if (val_recv == 0) {
            graceful_return("peer offline", -3);
        }
        else {
            total_recv += val_recv;
        }
    }

    if (!same_string(buffer, STR_1, strlen(STR_1))) {
        graceful_return("not received correct string", -12);
    }

    int val_send_ready, val_send;
    // send client student number
    val_send_ready = ready_to_send(socketfd, opt);
    if (val_send_ready < 0) {
        if (val_send_ready == -1) {
            graceful_return("select", -1);
        }
        else if (val_send_ready == -2) {
            graceful_return("time up", -2)
        }
        else if (val_send_ready == -3) {
            graceful_return("peer offline", -3);
        }
        else if (val_send_ready == -4) {
            graceful_return("not permitted to send", -4);
        }
        else {
            graceful_return("ready_to_send", -5);
        }
    }

    uint32_t h_stuNo = STU_NO;
    uint32_t n_stuNo = htonl(h_stuNo);
    memcpy(buffer, &n_stuNo, sizeof(uint32_t));
    val_send = send(socketfd, buffer, sizeof(uint32_t), MSG_NOSIGNAL);
    if (val_send != sizeof(uint32_t)) {
        if (errno == EPIPE) {
            graceful_return("peer offline", -3);
        }
        else if (val_send == -1) {
            graceful_return("send", -6);
        }
        else {
            graceful_return("message sent is of wrong quantity of byte", -7);
        }
    }

    // recv "pid" from server
    val_recv_ready = ready_to_recv(socketfd, opt);
    if (val_recv_ready < 0) {
        if (val_recv_ready == -1) {
            graceful_return("select", -1);
        }
        else if (val_recv_ready == -2) {
            graceful_return("time up", -2);
        }
        else if (val_recv_ready == -3) {
            graceful_return("not permitted to recv", -8);
        }
        else {
            graceful_return("ready_to_recv", -9);
        }
    }

    memset(buffer, 0, sizeof(char) * BUFFER_LEN);
    total_recv = 0;
    while (total_recv < (int)strlen(STR_2)) {
        val_recv = recv(socketfd, buffer+total_recv, strlen(STR_2), 0);
        if (val_recv < 0) {
            graceful_return("recv", -10);
        }
        else if (val_recv == 0) {
            graceful_return("peer offline", -3);
        }
        else {
            total_recv += val_recv;
        }
    }

    if (!same_string(buffer, STR_2, strlen(STR_2))) {
        graceful_return("not received correct string", -12);
    }

    // send client pid
    val_send_ready = ready_to_send(socketfd, opt);
    if (val_send_ready < 0) {
        if (val_send_ready == -1) {
            graceful_return("select", -1);
        }
        else if (val_send_ready == -2) {
            graceful_return("time up", -2)
        }
        else if (val_send_ready == -3) {
            graceful_return("peer offline", -3);
        }
        else if (val_send_ready == -4) {
            graceful_return("not permitted to send", -4);
        }
        else {
            graceful_return("ready_to_send", -5);
        }
    }

    uint32_t n_pid;
    pid_t pid = getpid();
                    //if fork,   send: pid
    if(opt.fork)
        n_pid = htonl((uint32_t)pid); 
    else            //if nofork, send: pid<<16 + socket_id
        n_pid = htonl((uint32_t)((((int)pid)<<16)+socketfd));
    
    memcpy(buffer, &n_pid, sizeof(uint32_t));
    val_send = send(socketfd, buffer, sizeof(uint32_t), MSG_NOSIGNAL);
    if (val_send != sizeof(uint32_t)) {
        if (errno == EPIPE) {
            graceful_return("peer offline", -3);
        }
        else if (val_send == -1) {
            graceful_return("send", -6);
        }
        else {
            graceful_return("message sent is of wrong quantity of byte", -7);
        }
    }

    // recv "TIME" from server
    val_recv_ready = ready_to_recv(socketfd, opt);
    if (val_recv_ready < 0) {
        if (val_recv_ready == -1) {
            graceful_return("select", -1);
        }
        else if (val_recv_ready == -2) {
            graceful_return("time up", -2);
        }
        else if (val_recv_ready == -3) {
            graceful_return("not permitted to recv", -8);
        }
        else {
            graceful_return("ready_to_recv", -9);
        }
    }

    memset(buffer, 0, sizeof(char) * BUFFER_LEN);
    total_recv = 0;
    while (total_recv < (int)strlen(STR_3)) {
        val_recv = recv(socketfd, buffer+total_recv, strlen(STR_3), 0);
        if (val_recv < 0) {
            graceful_return("recv", -10);
        }
        else if (val_recv == 0) {
            graceful_return("peer offline", -3);
        }
        else {
            total_recv += val_recv;
        }
    }

    if (!same_string(buffer, STR_3, strlen(STR_3))) {
        graceful_return("not received correct string", -12);
    }

    // send client current time(yyyy-mm-dd hh:mm:ss, 19 bytes)
    val_send_ready = ready_to_send(socketfd, opt);
    if (val_send_ready < 0) {
        if (val_send_ready == -1) {
            graceful_return("select", -1);
        }
        else if (val_send_ready == -2) {
            graceful_return("time up", -2)
        }
        else if (val_send_ready == -3) {
            graceful_return("peer offline", -3);
        }
        else if (val_send_ready == -4) {
            graceful_return("not permitted to send", -4);
        }
        else {
            graceful_return("ready_to_send", -5);
        }
    }

    char time_buf[20] = {0};
    str_current_time(time_buf);
    
    strncpy(buffer, time_buf, 19);
    val_send = send(socketfd, buffer, 19, MSG_NOSIGNAL);
    if (val_send != 19) {
        if (errno == EPIPE) {
            graceful_return("peer offline", -3);
        }
        else if (val_send == -1) {
            graceful_return("send", -6);
        }
        else {
            graceful_return("message sent is of wrong quantity of byte", -7);
        }
    }

    // recv "str*****" from server and parse
    val_recv_ready = ready_to_recv(socketfd, opt);
    if (val_recv_ready < 0) {
        if (val_recv_ready == -1) {
            graceful_return("select", -1);
        }
        else if (val_recv_ready == -2) {
            graceful_return("time up", -2);
        }
        else if (val_recv_ready == -3) {
            graceful_return("not permitted to recv", -8);
        }
        else {
            graceful_return("ready_to_recv", -9);
        }
    }

    memset(buffer, 0, sizeof(char) * BUFFER_LEN);
    total_recv = 0;
    while (total_recv < 8) {
        val_recv = recv(socketfd, buffer+total_recv, 8, 0);
        if (val_recv < 0) {
            graceful_return("recv", -10);
        }
        else if (val_recv == 0) {
            graceful_return("peer offline", -3);
        }
        else {
            total_recv += val_recv;
        }
    }

    if (!same_string(buffer, "str", 3)) {
        graceful_return("not received correct string", -12);
    }
    
    int rand_length = parse_str(buffer);
    if (rand_length == -1) {
        graceful_return("not received correct string", -12);
    }

    // send random string in designated length
    val_send_ready = ready_to_send(socketfd, opt);
    if (val_send_ready < 0) {
        if (val_send_ready == -1) {
            graceful_return("select", -1);
        }
        else if (val_send_ready == -2) {
            graceful_return("time up", -2)
        }
        else if (val_send_ready == -3) {
            graceful_return("peer offline", -3);
        }
        else if (val_send_ready == -4) {
            graceful_return("not permitted to send", -4);
        }
        else {
            graceful_return("ready_to_send", -5);
        }
    }

    unsigned char client_string[BUFFER_LEN] = {0};
    create_random_str(rand_length, client_string);

    int total_send = 0;
    while (total_send < rand_length) {
        val_send = send(socketfd, client_string+total_send, minimum(rand_length-total_send, MAX_SENDLEN), MSG_NOSIGNAL);
        if (val_send != minimum(rand_length-total_send, MAX_SENDLEN)) {
            if (errno == EPIPE) {
                graceful_return("peer offline", -3);
            }
            else if (val_send == -1) {
                graceful_return("send", -6);
            }
            else {
                graceful_return("message sent is of wrong quantity of byte", -7);
            }
        }
        else {
            total_send += val_send;
        }
    }

    // recv "end" from server
    val_recv_ready = ready_to_recv(socketfd, opt);
    if (val_recv_ready < 0) {
        if (val_recv_ready == -1) {
            graceful_return("select", -1);
        }
        else if (val_recv_ready == -2) {
            graceful_return("time up", -2);
        }
        else if (val_recv_ready == -3) {
            graceful_return("not permitted to recv", -8);
        }
        else {
            graceful_return("ready_to_recv", -9);
        }
    }

    memset(buffer, 0, sizeof(char) * BUFFER_LEN);
    total_recv = 0;
    while (total_recv < (int)strlen(STR_4)) {
        val_recv = recv(socketfd, buffer+total_recv, strlen(STR_4), 0);
        if (val_recv < 0) {
            graceful_return("recv", -10);
        }
        else if (val_recv == 0) {
            graceful_return("peer offline", -3);
        }
        else {
            total_recv += val_recv;
        }
    }

    if (!same_string(buffer, STR_4, strlen(STR_4))) {
        graceful_return("not received correct string", -12);
    }
    
    close(socketfd);
    if (write_file(h_stuNo, pid, time_buf, client_string) == -1) {
        graceful_return("write_file", -11);
    }

    // return 0 as success
    return 0;
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

int ready_to_send(int socketfd, const Options &opt) {
    // return 1 means ready to send
    // return -1: select error
    // return -2: time up
    // return -3: peer offline
    // return -4: not permitted to send
    if (opt.block) {
        return 1;
    }
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
		graceful_return("peer offline", -3);
	}
    else if (FD_ISSET(socketfd, &writefds)){
        FD_ZERO(&writefds);
        return 1;
    }
    else {
        graceful_return("not permitted to send", -4);
    }
}

int ready_to_recv(int socketfd, const Options &opt) {
    // return 1 means ready to recv
    // return -1: select error
    // return -2: time up
    // return -3: not permitted to recv
    if (opt.block) {
        return 1;
    }
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

bool peer_is_disconnected(int socketfd) {
    sleep(1);
    // if peer is disconnected
    return true;
}

int write_file(int stuNo, int pid, const char *time_str, const unsigned char *client_string) {
    // return 0: all good
    // return -1: file open error
    std::ofstream myfile;
    std::stringstream ss_filename;
    ss_filename << stuNo << '.' << pid << ".pid.txt";
    std::string str_filename = ss_filename.str();
    myfile.open(str_filename, std::ios::out|std::ios::trunc);
    if (!myfile.is_open()) {
        graceful_return("file open", -1);
    }
    myfile << stuNo << '\n';
    myfile << pid << '\n';
    myfile << time_str << '\n';
    myfile << client_string << '\n';
    myfile.close();
    return 0;
}

int str_current_time(char *time_str) {
    timespec time;
	clock_gettime(CLOCK_REALTIME, &time); 
	tm nowTime;
	localtime_r(&time.tv_sec, &nowTime);
	char current[1024];
	sprintf(current, "%04d-%02d-%02d %02d:%02d:%02d", 
			nowTime.tm_year + 1900, nowTime.tm_mon, nowTime.tm_mday, 
			nowTime.tm_hour, nowTime.tm_min, nowTime.tm_sec);
    memcpy(time_str, current, 19);
	return 0;
}

int create_random_str(const int length, unsigned char *random_string) {
	unsigned char *p;
    p = (unsigned char *)malloc(length+1);
	srand((unsigned)time(NULL));
	for(int i = 0; i <= length; i++) {
		p[i] = rand() % 256;
    }
	p[length] = '\0';
    memcpy(random_string, p, length+1);
	return 0;
}

bool same_string(const char *str1, const char *str2, const int cmp_len) {
    char cmp_1[100], cmp_2[100];
    memcpy(cmp_1, str1, cmp_len);
    memcpy(cmp_2, str2, cmp_len);
    cmp_1[cmp_len] = '\0';
    cmp_2[cmp_len] = '\0';
    if(!strcmp(cmp_1, cmp_2)) {
        return false;	//unexpected data        
    }
    else {
        return true;
    }
}

int parse_str(const char *str) {
    char num[10] = {0};
    strncpy(num, str+3, 5);
    int parsed = atoi(num);
    if (parsed < 32768 || parsed > 99999) {
        return -1;      // unexpected
    }
    else {
        return parsed;
    }
}
