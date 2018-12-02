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

void fill_up_sets(fd_set &master, int &fdmax, set<Socket> &set_data_socket, queue<Socket> &socket_q, const bool is_server) {
    // fill up the sets from the queue
    while (!socket_q.empty() && set_data_socket.size() < max_active_connections) {
        // pop a new socket from the queue
        int socketfd = socket_q.front().socketfd;
        socket_q.pop();

        // insert it to the sets
        FD_SET(socketfd, &master); // add to master set
        set_data_socket.emplace(socketfd);
        if (socketfd > fdmax)      // keep track of the max
            fdmax = socketfd;
    }
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
    if (!opt.block) {
        fcntl(listener, F_SETFL, O_NONBLOCK);
    }

    // bind
    if (server_bind_port(listener, listen_port) == -1)
        graceful("bind", 2);

    std::cout << "Listening on port " << listen_port << " on all interfaces...\n";

    // set listening
    listen(listener, 1000);

    return listener;
}


// pid_t r_wait(int * stat_loc)
// {
//     int revalue;
//     while(((revalue = wait(&stat_loc)) == -1) && (errno == EINTR));
//     return revalue;
// }


void remove_dead_connections(fd_set &master, int &fdmax, set<Socket> &set_data_socket, queue<Socket> &socket_q, const bool is_server) {
    for (auto s: set_data_socket) {
        if (!s.has_been_active) {
            FD_CLR(s.socketfd, &master);
        }
    }
    fill_up_sets(master, fdmax, set_data_socket, socket_q, is_server);
}

template <class T>
typename T::iterator find_socketfd(int socketfd, T collection) {
    return find_if(collection.begin(), collection.end(), [=] (const Socket &s) { return s.socketfd == socketfd; });
}

void loop_server_nofork(int listener, const Options &opt) {
    queue<Socket> socket_q;
    set<Socket> set_data_socket;

    fd_set master, readfds, writefds;      // master file descriptor list
    FD_ZERO(&master);
    FD_SET(listener, &master);
    set_data_socket.emplace(listener);
    int fdmax = listener; // maximum file descriptor number 
    timeval tv {timeout_seconds, timeout_microseconds}; // set a 2 second client timeout

    for (;;) {
        sleep(1);
        readfds = master; // copy at the last minutes
        writefds = master; // copy at the last minutes
        FD_CLR(listener, &writefds); // avoid selecting writable listener
        int rv = select(fdmax+1, &readfds, &writefds, NULL, NULL);
        cout << "DEBUG: rv\t" << rv << endl;
        switch (rv) {
            case -1:
                graceful("select in main loop", 5);
                break;
            case 0:
                // timeout, close sockets that haven't responded in an interval, exept for listener
                remove_dead_connections(master, fdmax, set_data_socket, socket_q, true);
                tv = {timeout_seconds, timeout_microseconds}; // set a 2 second client timeout
                break;
            default:
                for (auto socket_it = set_data_socket.begin(); socket_it != set_data_socket.end(); socket_it++) {
                    // won't touch the variables that are used for the sorting
                    // cast to mutable
                    auto socket = const_cast<Socket&>(*socket_it); 
                    int i = socket.socketfd;
                    cout << "DEBUG: i = " << i << endl;
                    if (FD_ISSET(i, &readfds) && i == listener) { // we got a new connection
                        server_accept_client(listener, opt.block, &master, &fdmax, &set_data_socket, &socket_q);

                        // TODO: check if this possible workaround works
                        tv = {timeout_seconds, timeout_microseconds}; // should the timer be reset?
                    } else if (FD_ISSET(i, &writefds) || FD_ISSET(i, &readfds))  { // we got a readable or writable socket
                        int comm_rv = server_communicate_new(socket);
                        if (comm_rv < 0) {
                            // only close socket if an error is encountered
                            close(i); 
                            // remove the socket from the sets
                            FD_CLR(i, &master);
                            set_data_socket.erase(find_socketfd(i, set_data_socket));
                            fill_up_sets(master, fdmax, set_data_socket, socket_q, true);
                        } else {
                            // re-insert socket into the set
                            set_data_socket.erase(socket_it);
                            set_data_socket.insert(socket);
                        }
                    }
                }
                break;
        }

    }

}

int create_connection(const Options &opt) {
    // create a connection from opt

    struct sockaddr_in   servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(stoi(opt.port));
    if(inet_pton(AF_INET, opt.ip.c_str(), &servaddr.sin_addr) < 0)
        graceful("Invalid ip address", -1);

    // get a socket
    int sockfd;
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        graceful("socket", -2);

    // connect
    if (!opt.block) { //non-blocking
        int flags = fcntl(sockfd, F_GETFL, 0);
        fcntl(sockfd, F_SETFL, flags | O_NONBLOCK); 

        if(connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1) {
            // EINPROGRESS means connection is in progress
            // then select on it
            fd_set fds;      
            if(errno != EINPROGRESS)
                graceful("connect", -3);
            
            FD_ZERO(&fds);      
            FD_SET(sockfd, &fds);       
            int select_rtn;

            if((select_rtn = select(sockfd+1, NULL, &fds, NULL, NULL)) > 0) {
                int error = -1, slen = sizeof(int);
                getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, (socklen_t *)&slen);
                //error == 0 means connect succeeded
                if(error != 0) graceful("connect", -3);
            }
        }
        //connect succeed   
    } else { // blocking
        if(connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1)
            graceful("connect", -3);
    }
    return sockfd;
}

// DELETE FROM HERE
int ready_to_send(int socketfd, const Options &opt) {
    // return 1 means ready to send
    // return -1: select error
    // return -2: time out
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
    tv.tv_sec = wait_time_s;
    tv.tv_usec = wait_time_us;
    errno = 0;
    int val_select = select(socketfd+1, &readfds, &writefds, NULL, &tv);
    if (val_select < 0) {
        graceful_return("select", -1);
    }
    else if (val_select == 0) {
        graceful_return("time out and no change", -2);
    }
	//else if (FD_ISSET(socketfd, &readfds) && FD_ISSET(socketfd, &writefds)) {
        //FD_ZERO(&readfds);
        //FD_ZERO(&writefds);
		//close(socketfd);
		//graceful_return("peer offline", -3);
	//}
    else if (FD_ISSET(socketfd, &writefds)){
        FD_ZERO(&writefds);
        return 1;
    }
    else {
        graceful_return("not permitted to send", -4);
    }
}
int send_thing(const int socketfd, const char *str, const Options &opt, const int send_len) {
    int val_send_ready, val_send, total_send;

    total_send = 0;
    while (total_send < send_len) {

        val_send_ready = ready_to_send(socketfd, opt);

        if (val_send_ready < 0) {
            if (val_send_ready > -5) {
                return val_send_ready;
            }
            else {
                graceful_return("ready_to_send", -5);
            }
        }

        val_send = send(socketfd, str+total_send, minimum(send_len-total_send, max_sendlen), MSG_NOSIGNAL);
        if (val_send != minimum(send_len-total_send, max_sendlen)) {
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

    //std::cout << "send" << str.c_str() << std::endl;
    return 0;       // all good
}
int ready_to_recv(int socketfd, const Options &opt) {
    // return 1 means ready to recv
    // return -1: select error
    // return -2: time out
    // return -3: not permitted to recv
    if (opt.block) {
        return 1;
    }
    fd_set readfds;
    struct timeval tv;
    FD_ZERO(&readfds);
    FD_SET(socketfd, &readfds);
    tv.tv_sec = wait_time_s;
    tv.tv_usec = wait_time_us;
    errno = 0;
    //int val_select = select(socketfd+1, &readfds, NULL, NULL, &tv);
    int val_select = select(socketfd+1, &readfds, NULL, NULL, NULL);
    if (val_select < 0) {
        graceful_return("select", -1);
    }
    else if (val_select == 0) {
        graceful_return("time out and no change", -2);
    }
    else if (FD_ISSET(socketfd, &readfds)){
        FD_ZERO(&readfds);
        return 1;
    }
    else {
        graceful_return("not permitted to recv", -3);
    }
}

int recv_thing(const int socketfd, char *buffer, const Options &opt, const int recv_len) {
    int val_recv_ready, val_recv, total_recv;

    memset(buffer, 0, sizeof(char)*buffer_len);

    total_recv = 0;
    while (total_recv < recv_len) {
        errno = 0;
        val_recv_ready = ready_to_recv(socketfd, opt);
        if (val_recv_ready < 0) {
            if (val_recv_ready == -1) {
                return -1;
            }
            else if (val_recv_ready == -2) {
                return -2;
            }
            else if (val_recv_ready == -3) {
                return -8;
            }
            else {
                graceful_return("ready_to_recv", -9);
            }
        }
        val_recv = recv(socketfd, buffer+total_recv, max_sendlen, 0);
        if (val_recv < 0) {
            cout << "DEBUG: total_recv: " << total_recv << ", val_recv: " << val_recv << endl;
            graceful_return("recv", -10);
        }
        else if (val_recv == 0) {
            graceful_return("peer offline", -3);
        }
        else {
            total_recv += val_recv;
        }
    }
    if (total_recv != recv_len) {
        graceful_return("not received exact designated quantity of bytes", -10);
    }
    return 0;       // all good
}
int client_communicate(int socketfd, const Options &opt) {
    // return 0: all good
    // return -1: select error
    // return -2: time out
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

    char buffer[buffer_len] = {0};
    int val_recv_thing = 0;
    int val_send_thing = 0;
    
    // 1. recv "StuNo" from server
    val_recv_thing = recv_thing(socketfd, buffer, opt, strlen(STR_1));
    if (val_recv_thing < 0) {
        return val_recv_thing;
    }

    std::cout << "client recv " << buffer << std::endl;
    if (!same_string(buffer, STR_1, strlen(STR_1))) {
        graceful_return("not received correct string", -12);
    }

    // 2. send client student number
    uint32_t h_stuNo = stu_no;
    uint32_t n_stuNo = htonl(h_stuNo);
    memcpy(buffer, &n_stuNo, sizeof(uint32_t));
    val_send_thing = send_thing(socketfd, buffer, opt, sizeof(uint32_t));
    if (val_send_thing < 0) {
        return val_send_thing;
    }
    std::cout << "client send " << h_stuNo << std::endl;

    // 3. recv "pid" from server
    val_recv_thing = recv_thing(socketfd, buffer, opt, strlen(STR_2));
    if (val_recv_thing < 0) {
        return val_recv_thing;
    }

    std::cout << "client recv " << buffer << std::endl;
    if (!same_string(buffer, STR_2, strlen(STR_2))) {
        graceful_return("not received correct string", -12);
    }

    // 4. send client pid
    uint32_t n_pid;
    pid_t pid = getpid();
                    //if fork,   send: pid
    if(opt.fork)
        n_pid = htonl((uint32_t)pid); 
    else            //if nofork, send: pid<<16 + socket_id
        n_pid = htonl((uint32_t)((((int)pid)<<16)+socketfd));
    int h_pid = ntohl(n_pid);
    
    memcpy(buffer, &n_pid, sizeof(uint32_t));
    val_send_thing = send_thing(socketfd, buffer, opt, sizeof(uint32_t));
    if (val_send_thing < 0) {
        return val_send_thing;
    }
    std::cout << "client send " << h_pid << std::endl;

    // 5. recv "TIME" from server
    val_recv_thing = recv_thing(socketfd, buffer, opt, strlen(STR_3)+1);
    if (val_recv_thing < 0) {
        return val_recv_thing;
    }

    std::cout << "client recv " << buffer << std::endl;
    if (!same_string(buffer, STR_3, strlen(STR_3))) {
        graceful_return("not received correct string", -12);
    }

    // 6. send client current time(yyyy-mm-dd hh:mm:ss, 19 bytes)
    char time_buf[20] = {0};
    str_current_time(time_buf);
    
    strncpy(buffer, time_buf, 19);
    val_send_thing = send_thing(socketfd, buffer, opt, 19);
    if (val_send_thing < 0) {
        return val_send_thing;
    }
    std::cout << "client send " << buffer << std::endl;

    // 7. recv "str*****" from server and parse
    val_recv_thing = recv_thing(socketfd, buffer, opt, 9);
    if (val_recv_thing < 0) {
        return val_recv_thing;
    }

    std::cout << "client recv " << buffer << std::endl;
    if (!same_string(buffer, "str", 3)) {
        graceful_return("not received correct string", -12);
    }
    
    int rand_length = parse_str(buffer);
    if (rand_length == -1) {
        graceful_return("not received correct string", -12);
    }

    std::cout << "rand number: " << rand_length << std::endl;

    // 8. send random string in designated length
    unsigned char client_string[buffer_len] = {0};
    create_random_str(rand_length, client_string);

    memcpy(buffer, client_string, buffer_len);

    val_send_thing = send_thing(socketfd, buffer, opt, rand_length);
    if (val_send_thing < 0) {
        return val_send_thing;
    }

    std::cout << "client send ok" << std::endl;

    // 9. recv "end" from server
    val_recv_thing = recv_thing(socketfd, buffer, opt, strlen(STR_4));
    if (val_recv_thing < 0) {
        return val_recv_thing;
    }

    std::cout << "client recv " << buffer << std::endl;
    if (!same_string(buffer, STR_4, strlen(STR_4))) {
        graceful_return("not received correct string", -12);
    }

    //close(socketfd);
    std::cout << "client begin write file" << std::endl;
    std::stringstream ss_filename;
    ss_filename << "./client_txt/" << h_stuNo << '.' << h_pid << ".pid.txt";
    std::string str_filename = ss_filename.str();
    if (write_file(str_filename.c_str(), h_stuNo, h_pid, time_buf, client_string, rand_length) == -1) {
        graceful_return("write_file", -11);
    }
    std::cout << "client end write file" << std::endl;

    // return 0 as success
    return 0;
}

int client_nofork(const Options &opt) {
    // initialize opt.num many connections and add them to the master set.
    // exchange data on these connections, creating new connections when these connections
    // close on network failure

    // initialize connections
    //fd_set master;
    int sockets[2000];
    for (int i = 0; i < (int)opt.num; i++) {
        //FD_SET(create_connection(opt), &master);
        sockets[i] = create_connection(opt);
        cout << "DEBUG: sockets created and connected: " << i+1 << endl;
    }

    // exchange data on these connections, creating new connections when these connections
    // close on network failure
    for (int i = 0; i < (int)opt.num; i++) {
        int rv = client_communicate(sockets[i], opt);
        if (rv < 0) {
            sockets[i--] = create_connection(opt);
            continue;
        }
    }
    return 0;
}

//void handler(int sig) {
//   int newfd;
//    switch (sig) {
//       case SIGUSR1:
//           newfd = create_connection(opt);
//           client_communicate(newfd, opt);
//           while(1) sleep(10);
//           break;
//       default:
//           // unknown signal
//         break;
//   }
//}


pid_t r_wait(int * stat_loc)
{
    int revalue;
    while(((revalue = wait(stat_loc)) == -1) && (errno == EINTR));
    return revalue;
}

int client_fork(const Options &opt) {
    prctl(PR_SET_PDEATHSIG, SIGHUP);
    //signal(SIGUSR1, handler); // register handler to handle failed connections

    for (unsigned int i = 0; i < opt.num; i++) {
        // assume block
        int fpid = fork();

        int newfd;
        switch (fpid) {
            case 0:
                // in child
                prctl(PR_SET_PDEATHSIG, SIGHUP); 
                newfd = create_connection(opt);
                if (client_communicate(newfd, opt) < 0) {
                    kill(getppid(), SIGUSR1);
                }
               // while(1) sleep(10); // hold pid to avoid log file name collition
             //   sleep(1);
               // printf("test\n");
                return 0;
            case -1:
                // error
                graceful("client_fork", -20);
                break;
            default:
                // in parent
                break;
        }
    }
    while(r_wait(NULL) > 0);    //wait for all the subprocess.

    return 0;
}


int server_accept_client(int listener, bool block, fd_set *master, int *fdmax, set<Socket> *set_data_socket, queue<Socket> *socket_q) {
    // Accept connections from listener.
    // For select()ing, new socketfds are inserted into the socket queue, 
    // then fill up the sets from the queue if the set size is 
    // less than max_active_connections.
    // For non-select()ing, the new socketfd is returned directely.

    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen = sizeof(remoteaddr);

    int newfd = accept(listener, (sockaddr *) &remoteaddr, &addrlen);
    cout << "DEBUG: accept " << newfd << endl;

    if (newfd == -1) {
        graceful("server_accept_new_client", 7);
    } else {
        // set non-blocking connection
        if (!block) {
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

        if (master != NULL && fdmax != NULL && set_data_socket != NULL && socket_q != NULL) { // if using select
            (*socket_q).emplace(newfd);
            fill_up_sets(*master, *fdmax, *set_data_socket, *socket_q, true);
        }

        char remoteIP[INET6_ADDRSTRLEN];
        std::cout << "New connection from " << inet_ntop(remoteaddr.ss_family,
                                            get_in_addr((struct sockaddr*) &remoteaddr),
                                            remoteIP, INET6_ADDRSTRLEN)
                << " on socket " << newfd << std::endl;
    }
    return newfd;
}

int write_file(const char *str_filename, int stuNo, int pid, const char *time_str, const unsigned char *client_string, const int random) {
    // return 0: all good
    // return -1: file open error
    std::ofstream myfile;
    myfile.open(str_filename, std::ofstream::binary|std::ios::out|std::ios::trunc);
    if (!myfile.is_open()) {
        graceful_return("file open", -1);
    }
    myfile << stuNo << '\n';
    myfile << pid << '\n';
    myfile << time_str << '\n';
    //myfile << client_string << '\n';
    char buffer[buffer_len] = {0};
    memcpy(buffer, client_string, random);
    myfile.write(buffer, sizeof(char)*random);
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
    //std::cout << "cmp_1: " << cmp_1 << ", cmp_2: " << cmp_2 << ", strcmp: " << strcmp(cmp_1, cmp_2) << std::endl;
    if(strcmp(cmp_1, cmp_2) != 0) {
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

int server_communicate_new(Socket &socket) {
    std::cout << "server_communicate\t" << socket.stage  << std::endl;
    switch (socket.stage) {
        // 1. server send a string "StuNo"
        case 1: {
            int val_send_thing = send_thing_new(socket, STR_1, strlen(STR_1));
            if (val_send_thing < 0) {
                return(val_send_thing);
            }
            else if (val_send_thing != 1) {
                return 0;       // not all processed
            }
            std::cout << "server send: " << STR_1 << std::endl;
            stage_done(socket);

        }
        // 2. server recv an int as student number, network byte order
        case 2: {
            int last_bytes_processed = socket.bytes_processed;
            uint32_t n_stuNo = 0;
            char buffer_in[buffer_len] = {0};
            int val_recv_thing = recv_thing_new(socket, buffer_in, (int)sizeof(uint32_t));
            if (val_recv_thing < 0) {
                return(val_recv_thing);
            }
            memcpy(&n_stuNo+last_bytes_processed, buffer_in, sizeof(uint32_t)-last_bytes_processed);
            if (val_recv_thing != 1) {
                return 0;       // not all processed
            }
            socket.stuNo = ntohl(n_stuNo);
            std::cout << "server recv: " << socket.stuNo << std::endl;
            stage_done(socket);
        }
        // 3. server send a string "pid"
        case 3: {
            int val_send_thing = send_thing_new(socket, STR_2, strlen(STR_2));
            if (val_send_thing < 0) {
                return(val_send_thing);
            }
            else if (val_send_thing != 1) {
                return 0;       // not all processed
            }
            std::cout << "server send: " << STR_2 << std::endl;
            stage_done(socket);
        }
        // 4. server recv an int as client's pid, network byte order
        case 4: {
            int last_bytes_processed = socket.bytes_processed;
            uint32_t n_pid = 0;
            char buffer_in[buffer_len] = {0};
            int val_recv_thing = recv_thing_new(socket, buffer_in, (int)sizeof(uint32_t));
            if (val_recv_thing < 0) {
                return(val_recv_thing);
            }
            memcpy(&n_pid+last_bytes_processed, buffer_in, sizeof(uint32_t)-last_bytes_processed);
            if (val_recv_thing != 1) {
                return 0;       // not all processed
            }
            socket.pid = ntohl(n_pid);
            std::cout << "server recv: " << socket.pid << std::endl;
            stage_done(socket);
        }
        // 5. server send a string "TIME"
        case 5: {
            int val_send_thing = send_thing_new(socket, STR_3, strlen(STR_3)+1);
            cout << "DEBUG TIME:\t" << val_send_thing << endl;
            if (val_send_thing < 0) {
                return(val_send_thing);
            }
            else if (val_send_thing != 1) {
                return 0;       // not all processed
            }
            std::cout << "server send: " << STR_3 << std::endl;
            stage_done(socket);
        }
        // 6. server recv client's time as a string with a fixed length of 19 bytes
        case 6: {
            int last_bytes_processed = socket.bytes_processed;
            char buffer_in[buffer_len] = {0};
            int val_recv_thing = recv_thing_new(socket, buffer_in, 19);
            if (val_recv_thing < 0) {
                return(val_recv_thing);
            }
            memcpy(socket.time_str+last_bytes_processed, buffer_in, 19-last_bytes_processed);
            if (val_recv_thing != 1) {
                return 0;       // not all processed
            }
            std::cout << "server recv: " << socket.time_str << std::endl;
            stage_done(socket);
        }     
        // 7. server send a string "str*****", where ***** is a 5-digit random number ranging from 32768-99999, inclusively
        case 7: {
            srand((unsigned)time(NULL)); 
            //int random = rand() % 67232 + 32768;
            socket.random = rand() % 67232 + 32768;
            std::stringstream ss;
            // ss << "str" << random;
            ss << "str" << socket.random;
            std::string str = ss.str();
            int val_send_thing = send_thing_new(socket, str.c_str(), str.length()+1);
            if (val_send_thing < 0) {
                return(val_send_thing);
            }
            else if (val_send_thing != 1) {
                return 0;       // not all processed
            }
            std::cout << "server send: " << str << std::endl;
            stage_done(socket);
        }
        // 8. server recv a random string with length *****, and each character is in ASCII 0~255
        case 8: {
            int last_bytes_processed = socket.bytes_processed;
            char buffer_in[buffer_len] = {0};
            int val_recv_thing = recv_thing_new(socket, buffer_in, socket.random);
            if (val_recv_thing < 0) {
                return val_recv_thing;
            }
            memcpy(socket.client_string+last_bytes_processed, buffer_in, socket.random-last_bytes_processed);
            if (val_recv_thing != 1) {
                return 0;       // not all processed
            }
            std::cout << "server recv client string ok." << std::endl;
            stage_done(socket);
        }
        // 9. server send a string "end"
        case 9: {
            int val_send_thing = send_thing_new(socket, STR_4, strlen(STR_4));
            if (val_send_thing < 0) {
                return(val_send_thing);
            }
            else if (val_send_thing != 1) {
                return 0;       // not all processed
            }
            std::cout << "server send: " << STR_4 << std::endl;
            std::cout << "server begin write file." << std::endl;
            std::stringstream ss_filename;
            ss_filename << "./server_txt/" << socket.stuNo << '.' << socket.pid << ".pid.txt";
            std::string str_filename = ss_filename.str();
            if (write_file_new(str_filename.c_str(), socket) == -1) {
                graceful_return("write_file", -11);
            }
            std::cout << "server end write file." << std::endl;
            stage_done(socket);
        }        
        default: {
            graceful_return("stage number beyond index", -13);
        }         
    }
}

int client_communicate_new(Socket &socket, const Options &opt) {
    std::cout << "client_communicate" << std::endl;
    switch (socket.stage) {
        // 1. recv "StuNo" from server
        case 1: {
            int last_bytes_processed = socket.bytes_processed;
            char buffer[buffer_len] = {0};
            char buffer_in[buffer_len] = {0};
            int val_recv_thing = recv_thing_new(socket, buffer_in, strlen(STR_1));
            if (val_recv_thing < 0) {
                return(val_recv_thing);
            }
            memcpy(buffer+last_bytes_processed, buffer_in, strlen(STR_1)-last_bytes_processed);
            if (val_recv_thing != 1) {
                return 0;       // not all processed
            }
            std::cout << "client recv: " << buffer << std::endl;
            if (!same_string(buffer, STR_1, strlen(STR_1))) {
                graceful_return("not received correct string", -12);
            }
            stage_done(socket);
        }
        // 2. send client student number
        case 2: {
            socket.stuNo = stu_no;
            uint32_t n_stuNo = htonl(socket.stuNo);
            char buffer[buffer_len] = {0};
            memcpy(buffer, &n_stuNo, sizeof(uint32_t));
            int val_send_thing = send_thing_new(socket, buffer, sizeof(uint32_t));
            if (val_send_thing < 0) {
                return(val_send_thing);
            }
            else if (val_send_thing != 1) {
                return 0;       // not all processed
            }
            std::cout << "client send: " << buffer << std::endl;
            stage_done(socket);
        }
        // 3. recv "pid" from server
        case 3: {
            int last_bytes_processed = socket.bytes_processed;
            char buffer[buffer_len] = {0};
            char buffer_in[buffer_len] = {0};
            int val_recv_thing = recv_thing_new(socket, buffer_in, strlen(STR_2));
            if (val_recv_thing < 0) {
                return(val_recv_thing);
            }
            memcpy(buffer+last_bytes_processed, buffer_in, strlen(STR_2)-last_bytes_processed);
            if (val_recv_thing != 1) {
                return 0;       // not all processed
            }
            std::cout << "client recv: " << buffer << std::endl;
            if (!same_string(buffer, STR_2, strlen(STR_2))) {
                graceful_return("not received correct string", -12);
            }
            stage_done(socket);
        }
        // 4. send client pid
        case 4: {
            uint32_t n_pid;
            pid_t pid = getpid();
            if(opt.fork) {
                n_pid = htonl((uint32_t)pid);
            }
            else {
                //if nofork, send: pid<<16 + socket_id
                n_pid = htonl((uint32_t)((((int)pid)<<16)+socket.socketfd));
            }           
            socket.pid = ntohl(n_pid);
            char buffer[buffer_len] = {0};
            memcpy(buffer, &n_pid, sizeof(uint32_t));
            int val_send_thing = send_thing_new(socket, buffer, sizeof(uint32_t));
            if (val_send_thing < 0) {
                return(val_send_thing);
            }
            else if (val_send_thing != 1) {
                return 0;       // not all processed
            }
            std::cout << "client send: " << buffer << std::endl;
            stage_done(socket);
        }
        // 5. recv "TIME" from server
        case 5: {
            int last_bytes_processed = socket.bytes_processed;
            char buffer[buffer_len] = {0};
            char buffer_in[buffer_len] = {0};
            int val_recv_thing = recv_thing_new(socket, buffer_in, strlen(STR_3)+1);
            if (val_recv_thing < 0) {
                return(val_recv_thing);
            }
            memcpy(buffer+last_bytes_processed, buffer_in, strlen(STR_3)+1-last_bytes_processed);
            if (val_recv_thing != 1) {
                return 0;       // not all processed
            }
            std::cout << "client recv: " << buffer << std::endl;
            if (!same_string(buffer, STR_3, strlen(STR_3))) {
                graceful_return("not received correct string", -12);
            }
            stage_done(socket);
        }
        // 6. send client current time(yyyy-mm-dd hh:mm:ss, 19 bytes)
        case 6: {
            char buffer[buffer_len] = {0};
            str_current_time(socket.time_str);
            strncpy(buffer, socket.time_str, 19);
            int val_send_thing = send_thing_new(socket, buffer, 19);
            if (val_send_thing < 0) {
                return(val_send_thing);
            }
            else if (val_send_thing != 1) {
                return 0;       // not all processed
            }
            std::cout << "client send: " << buffer << std::endl;
            stage_done(socket);
        }
        // 7. recv "str*****" from server and parse
        case 7: {
            int last_bytes_processed = socket.bytes_processed;
            char buffer[buffer_len] = {0};
            char buffer_in[buffer_len] = {0};
            int val_recv_thing = recv_thing_new(socket, buffer_in, 9);
            if (val_recv_thing < 0) {
                return(val_recv_thing);
            }
            memcpy(buffer+last_bytes_processed, buffer_in, 9-last_bytes_processed);
            if (val_recv_thing != 1) {
                return 0;       // not all processed
            }
            std::cout << "client recv: " << buffer << std::endl;
            if (!same_string(buffer, "str", 3)) {
                graceful_return("not received correct string", -12);
            }
            socket.random = parse_str(buffer);
            if (socket.random < 32768) {
                graceful_return("not received correct string", -12);
            }
            std::cout << "rand number: " << socket.random << std::endl;
            stage_done(socket);
        }
        // 8. send random string in designated length
        case 8: {
            char buffer[buffer_len] = {0};
            create_random_str(socket.random, socket.client_string);
            memcpy(buffer, socket.client_string, socket.random);
            int val_send_thing = send_thing_new(socket, buffer, socket.random);
            if (val_send_thing < 0) {
                return(val_send_thing);
            }
            else if (val_send_thing != 1) {
                return 0;       // not all processed
            }
            std::cout << "client send client string ok." << std::endl;
            stage_done(socket);
        }
        // 9. recv "end" from server
        case 9: {
            int last_bytes_processed = socket.bytes_processed;
            char buffer[buffer_len] = {0};
            char buffer_in[buffer_len] = {0};
            int val_recv_thing = recv_thing_new(socket, buffer_in, strlen(STR_4));
            if (val_recv_thing < 0) {
                return(val_recv_thing);
            }
            memcpy(buffer+last_bytes_processed, buffer_in, strlen(STR_4)-last_bytes_processed);
            if (val_recv_thing != 1) {
                return 0;       // not all processed
            }
            std::cout << "client recv: " << buffer << std::endl;
            if (!same_string(buffer, STR_4, strlen(STR_4))) {
                graceful_return("not received correct string", -12);
            }
            std::cout << "client begin write file." << std::endl;
            std::stringstream ss_filename;
            ss_filename << "./client_txt/" << socket.stuNo << '.' << socket.pid << ".pid.txt";
            std::string str_filename = ss_filename.str();
            if (write_file_new(str_filename.c_str(), socket) == -1) {
                graceful_return("write_file", -11);
            }
            std::cout << "client end write file." << std::endl;
            stage_done(socket);
        }                        
        default: {
            graceful_return("stage number beyond index", -13);
        }
    }
}

int send_thing_new(Socket &socket, const char *str, const int send_len) {
    int val_send = send(socket.socketfd, str+socket.bytes_processed, minimum(send_len-socket.bytes_processed, max_sendlen), MSG_NOSIGNAL);
    if (errno == EPIPE) {
        graceful_return("peer offline", -3);
    }
    else if (val_send == -1) {
        graceful_return("send", -6);
    }
    else if (socket.bytes_processed > send_len) {
        graceful_return("message sent is of wrong quantity of byte", -7);
    }
    else {
        socket.bytes_processed += val_send;
    }

    if (socket.bytes_processed == send_len) {
        socket.bytes_processed = 0;
        return 1;   // processed all bytes
    }
    return 0;       // no error, but not all bytes processed
}

int recv_thing_new(Socket &socket, char *buffer, const int recv_len) {
    memset(buffer, 0, sizeof(char)*buffer_len);
    // int val_recv = recv(socket.socketfd, buffer+socket.bytes_processed, max_recvlen, 0);
    int val_recv = recv(socket.socketfd, buffer, max_recvlen, 0);
    if (val_recv < 0) {
        cout << "DEBUG: bytes_processed: " << socket.bytes_processed << ", val_recv: " << val_recv << endl;
        graceful_return("recv", -10);
    }
    else if (val_recv == 0) {
        graceful_return("peer offline", -3);
    }
    else if (socket.bytes_processed > recv_len) {
        graceful_return("not received exact designated quantity of bytes", -10);
    }
    else {
        socket.bytes_processed += val_recv;
    }

    if (socket.bytes_processed == recv_len) {
        socket.bytes_processed = 0;
        return 1;   // processed all bytes
    }
    return 0;       // no error, but not all bytes processed
}

int write_file_new(const char *str_filename, Socket &socket) {
    // return 0: all good
    // return -1: file open error
    std::ofstream myfile;
    myfile.open(str_filename, std::ofstream::binary|std::ios::out|std::ios::trunc);
    if (!myfile.is_open()) {
        graceful_return("file open", -1);
    }

    myfile << socket.stuNo << '\n';
    myfile << socket.pid << '\n';
    myfile << socket.time_str << '\n';
    myfile.write(reinterpret_cast<const char*>(socket.client_string), socket.random);
    myfile.close();
    return 0;
}



/********************************************/
/*                 yxd                      */
/********************************************/
//function:
//      wait() child process if and only if receive SIGCHLD
//return value:
//      0   no exit child
//      1   found exit child
int check_child()
{   
    pid_t pid;   
    //set option=WNOHANG to avoid blocking in waitpid()
    //waitpid would return immediately if no exit child 
    if((pid = waitpid(-1, NULL, WNOHANG)) == -1)
        graceful("check_SIGCHLD waitpid", -1);

    //no exit child
    else if(pid == 0)   
        return 0;

    //found exit child
    else if(pid > 0)
        return 1;
}

// There is no need to maintain a socket_q in fork mode;
// all that server needs to do is:
//       maintaining less than 200 child process(implemented in check_child())
// And in every child, if connection failed, close(socketfd) and exit(0);
//       otherwise, exit(0) directlly;

int loop_server_fork(int listener, const Options &opt)
{
    //create SIGCHID handler
//  signal(SIGCHLD, sigchld_handler);

    Socket newfd;
    pid_t pid;
    //curnum: current children process num
    int curnum = 0;
    int rtr;    //return value

    while(1)    //server won't end naturally 
    {
        //curnum-1 when no child exit
        if((rtr = check_child()) == 1)
            curnum --;

        if(curnum <= max_active_connections)
        {
            if(!opt.block)  //nonblock
            {
                fd_set readfds;
                FD_ZERO(&readfds);
                FD_SET(listener, &readfds);
                if((rtr = select(listener + 1, &readfds, NULL, NULL, NULL)) == -1)
                    graceful("loop_server_fork select", -20);
            }

            newfd.socketfd = server_accept_client(listener, opt.block, (fd_set*)NULL, (int*)NULL);
            newfd.stage = 0;
            
            pid = fork();
            if(pid == -1)
                graceful("loop_server_fork fork", -20);
            
            else if(pid == 0)   //child 
            {
                //close error sockfd
                /*
                    need to replace with new server_communicate_fork()
                */
                if(server_communicate_fork(newfd, opt) < 0)
                    close(newfd.socketfd);
            
                exit(0); //exit directlly with no error
            }
            else    //father
                curnum ++;
        }
        //else, wait until socket_q.size() < MAX_NUM
        else    
            continue;
    }
}

