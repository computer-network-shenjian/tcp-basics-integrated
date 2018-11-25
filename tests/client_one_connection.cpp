#include "../shared_library.hpp"

using namespace std;

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

        // debug
        cout << "communicate\n";
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

int main(int argc, char *argv[]) {
    // process arguments
    Options opt = parse_arguments(argc, argv, true);

    if (opt.fork)
        client_fork(opt);
    else
        client_nofork(opt);
    return 0;
}
