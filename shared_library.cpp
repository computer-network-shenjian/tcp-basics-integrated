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


int loop_server_fork(int listener, const Options &opt) {
    for (;;) {
        if (!opt.block){
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(listener, &readfds);
            int rv = select(listener+1, &readfds, NULL, NULL, NULL);

            if(rv == -1){
                graceful("loop_server_fork", -20);
            }
        }

        int newfd = server_accept_client(listener, opt.block, (fd_set*)NULL, (int*)NULL);
        int fpid = fork();
        switch (fpid) {
            case 0:
                // in child
                _exit(server_communicate(newfd, opt));
                break;
            case -1:
                // error
                graceful("loop_server_fork", -20);
                break;
            default:
                // in parent
                break;
        }
    }
    return 0;
}

int loop_server_nofork(int listener, const Options &opt) {
    // prepare variables used by select()
    fd_set master, readfds, writefds;      // master file descriptor list
    int num_good = 0;

    while (num_good < 1000) {
        FD_ZERO(&master);
        FD_SET(listener, &master);
        int fdmax = listener;          // maximum file descriptor number 

        int num_remaining = 1000 - num_good; // number of remainings in this iteration
        for (int i = 0; i < num_remaining; i++) {
            readfds = master; // copy at the last minutes
            int rv = select(listener+1, &readfds, NULL, NULL, NULL);
            switch (rv) {
                case -1:
                    graceful("select in main loop", 5);
                    break;
                case 0:
                    graceful("select returned 0\n", 6);
                    break;
                default:
                    server_accept_client(listener, opt.block, &master, &fdmax);
                    break;
            }
        }
        FD_CLR(listener, &master);

        // main loop
        while (num_remaining) {
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
                        if (FD_ISSET(i, &writefds) || FD_ISSET(i, &readfds))  { // we got a writable socket
                            num_remaining--; // regardless of the result
                            if (server_communicate(i, opt) < 0) {
                                close(i); FD_CLR(i, &master);
                            } else {
                                num_good++;
                            }
                        }
                    }
                    break;
                }
            }
        }
    return 0;
}

int server_communicate(int socketfd, const Options &opt) {
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

    // debug
    std::cout << "server_communicate" << std::endl;

    int val_send_thing;
    std::string str;
    char buffer[BUFFER_LEN] = {0};
    int var_recv_thing;
    
    // 1. server send a string "StuNo"
    val_send_thing = send_thing(socketfd, STR_1, opt, strlen(STR_1));
    if (val_send_thing < 0) {
        return val_send_thing;
    }
    std::cout << "server send " << STR_1 << std::endl;

    // 2. server recv an int as student number, network byte order
    uint32_t h_stuNo = 0;
    uint32_t n_stuNo = 0;

    var_recv_thing = recv_thing(socketfd, buffer, opt, (int)sizeof(uint32_t));
    if (var_recv_thing < 0) {
        return var_recv_thing;
    }

    memcpy(&n_stuNo, buffer, sizeof(uint32_t));
    h_stuNo = ntohl(n_stuNo);
    std::cout << "server recv " << h_stuNo << std::endl;

    // 3. server send a string "pid"
    val_send_thing = send_thing(socketfd, STR_2, opt, strlen(STR_2));
    if (val_send_thing < 0) {
        return val_send_thing;
    }
    std::cout << "server send " << STR_2 << std::endl;

    // 4. server recv an int as client's pid, network byte order
    uint32_t h_pid = 0;
    uint32_t n_pid = 0;

    var_recv_thing = recv_thing(socketfd, buffer, opt, (int)sizeof(uint32_t));
    if (var_recv_thing < 0) {
        return var_recv_thing;
    }

    memcpy(&n_pid, buffer, sizeof(uint32_t));
    h_pid = ntohl(n_pid);

    std::cout << "server recv " << h_pid << std::endl;

    // 5. server send a string "TIME"
    val_send_thing = send_thing(socketfd, STR_3, opt, strlen(STR_3)+1);
    if (val_send_thing < 0) {
        return val_send_thing;
    }
    std::cout << "server send " << STR_3 << std::endl;

    // 6. server recv client's time as a string with a fixed length of 19 bytes
    char time_buf[20] = {0};

    var_recv_thing = recv_thing(socketfd, buffer, opt, 19);
    if (var_recv_thing < 0) {
        return var_recv_thing;
    }
    else {
        memcpy(time_buf, buffer, 19);
    }
    std::cout << "server recv " << time_buf << std::endl;

    // 7. server send a string "str*****", where ***** is a 5-digit random number ranging from 32768-99999, inclusively.
    srand( (unsigned)time( NULL ) ); 
    int random = rand() % 67232 + 32768;
    std::stringstream ss;
    ss << "str" << random;
    str = ss.str();
    val_send_thing = send_thing(socketfd, str.c_str(), opt, str.length()+1);

    if (val_send_thing < 0) {
        return val_send_thing;
    }

    std::cout << "server send " << str << std::endl;

    // 8. server recv a random string with length *****, and each character is in ASCII 0~255.
    unsigned char client_string[BUFFER_LEN] = {0};
    memset(buffer, 0, sizeof(char) * BUFFER_LEN);
    var_recv_thing = recv_thing(socketfd, buffer, opt, random);
    if (var_recv_thing < 0) {
        return var_recv_thing;
    }

    memcpy(client_string, buffer, random);

    std::cout << "server recv ok" << std::endl;

    // 9. server send a string "end"
    val_send_thing = send_thing(socketfd, STR_4, opt, strlen(STR_4));
    if (val_send_thing < 0) {
        return val_send_thing;
    }
    std::cout << "server send " << STR_4 << std::endl;

    // 10. after server catch that client is closed, close s/c socket, write file
    while(1) {
        // check every second        
        if (peer_is_disconnected(socketfd)) {
            std::cout << "peer is disconnected!" << std::endl;
            close(socketfd);
            break;
        }
        else {
            sleep(1);
        }
    }

    std::cout << "server begin write file" << std::endl;
    std::stringstream ss_filename;
    ss_filename << h_stuNo << '.' << h_pid << ".pid.txt";
    std::string str_filename = ss_filename.str();
    if (write_file(str_filename.c_str(), h_stuNo, h_pid, time_buf, client_string) == -1) {
        graceful_return("write_file", -11);
    }
    std::cout << "server end write file" << std::endl;

    // return 0 as success
    return 0;
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

void handler(int sig) {
   int newfd;
    switch (sig) {
       case SIGUSR1:
            newfd = create_connection(opt);
           client_communicate(newfd, opt);
           while(1) sleep(10);
            break;
       default:
           // unknown signal
         break;
   }
}


pid_t r_wait(int * stat_loc)
{
    int revalue;
    while(((revalue = wait(stat_loc)) == -1) && (errno == EINTR));//如果等待的过程中被一个不可阻塞的信号终断则继续循环等待
    return revalue;
}

int client_fork(const Options &opt) {
    prctl(PR_SET_PDEATHSIG, SIGHUP);
    signal(SIGUSR1, handler); // register handler to handle failed connections

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
                sleep(1);
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

    char buffer[BUFFER_LEN] = {0};
    int var_recv_thing = 0;
    int val_send_thing = 0;
    
    // 1. recv "StuNo" from server
    var_recv_thing = recv_thing(socketfd, buffer, opt, strlen(STR_1));
    if (var_recv_thing < 0) {
        return var_recv_thing;
    }

    std::cout << "client recv " << buffer << std::endl;
    if (!same_string(buffer, STR_1, strlen(STR_1))) {
        graceful_return("not received correct string", -12);
    }

    // 2. send client student number
    uint32_t h_stuNo = STU_NO;
    uint32_t n_stuNo = htonl(h_stuNo);
    memcpy(buffer, &n_stuNo, sizeof(uint32_t));
    val_send_thing = send_thing(socketfd, buffer, opt, sizeof(uint32_t));
    if (val_send_thing < 0) {
        return val_send_thing;
    }
    std::cout << "client send " << h_stuNo << std::endl;

    // 3. recv "pid" from server
    var_recv_thing = recv_thing(socketfd, buffer, opt, strlen(STR_2));
    if (var_recv_thing < 0) {
        return var_recv_thing;
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
    var_recv_thing = recv_thing(socketfd, buffer, opt, strlen(STR_3)+1);
    if (var_recv_thing < 0) {
        return var_recv_thing;
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
    var_recv_thing = recv_thing(socketfd, buffer, opt, 9);
    if (var_recv_thing < 0) {
        return var_recv_thing;
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
    unsigned char client_string[BUFFER_LEN] = {0};
    create_random_str(rand_length, client_string);

    memcpy(buffer, client_string, BUFFER_LEN);

    val_send_thing = send_thing(socketfd, buffer, opt, rand_length);
    if (val_send_thing < 0) {
        return val_send_thing;
    }

    std::cout << "client send ok" << std::endl;

    // 9. recv "end" from server
    var_recv_thing = recv_thing(socketfd, buffer, opt, strlen(STR_4));
    if (var_recv_thing < 0) {
        return var_recv_thing;
    }

    std::cout << "client recv " << buffer << std::endl;
    if (!same_string(buffer, STR_4, strlen(STR_4))) {
        graceful_return("not received correct string", -12);
    }

    close(socketfd);
    std::cout << "client begin write file" << std::endl;
    std::stringstream ss_filename;
    ss_filename << h_stuNo << '.' << h_pid << ".pid.txt";
    std::string str_filename = ss_filename.str();
    if (write_file(str_filename.c_str(), h_stuNo, h_pid, time_buf, client_string) == -1) {
        graceful_return("write_file", -11);
    }
    std::cout << "client end write file" << std::endl;

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
    tv.tv_sec = WAIT_TIME_S;
    tv.tv_usec = WAIT_TIME_US;
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
    tv.tv_sec = WAIT_TIME_S;
    tv.tv_usec = WAIT_TIME_US;
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

bool peer_is_disconnected(int socketfd) {  
    char buf[10];
    if(recv(socketfd, buf, 1, MSG_PEEK) == 0)
        return true;
    else
        return false;
}

int write_file(const char *str_filename, int stuNo, int pid, const char *time_str, const unsigned char *client_string) {
    // return 0: all good
    // return -1: file open error
    std::ofstream myfile;
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

        val_send = send(socketfd, str+total_send, minimum(send_len-total_send, MAX_SENDLEN), MSG_NOSIGNAL);
        if (val_send != minimum(send_len-total_send, MAX_SENDLEN)) {
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

int recv_thing(const int socketfd, char *buffer, const Options &opt, const int recv_len) {
    int val_recv_ready, val_recv, total_recv;

    memset(buffer, 0, sizeof(char)*BUFFER_LEN);

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
        val_recv = recv(socketfd, buffer+total_recv, MAX_SENDLEN, 0);
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
