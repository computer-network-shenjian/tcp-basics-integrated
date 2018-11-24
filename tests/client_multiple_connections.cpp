#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <arpa/inet.h> 
#include <fcntl.h>  // for setting non-blocking socket option
#include <thread>
#include <chrono>
#include <errno.h>
#include <time.h>
#include "../parse_arguments.hpp"
#include "../shared_library.hpp"

using namespace std;


struct sockaddr_in   servaddr;
int 	sockfd;
fd_set  fds;
int     flags, error = -1, slen = sizeof(int);

//format: yyyy-mm-dd hh:mm:ss, 19 words
char * getCurrentTime()
{
	timespec time;
	clock_gettime(CLOCK_REALTIME, &time); 
	tm nowTime;
	localtime_r(&time.tv_sec, &nowTime);
	char current[1024];
	sprintf(current, "%04d-%02d-%02d %02d:%02d:%02d", 
			nowTime.tm_year + 1900, nowTime.tm_mon, nowTime.tm_mday, 
			nowTime.tm_hour, nowTime.tm_min, nowTime.tm_sec);
	return current;
}


int client_recv(int sockfd, fd_set &rfds, char *expcline, const Options &opt)
{
	/*
		nonblock/block not implement
	*/
	FD_ZERO(&rfds);		
	FD_SET(sockfd, &rfds);	
	if((select_rtn = select(sockfd+1, &rfds, NULL, NULL, NULL)) == -1)
		graceful("client_recv select", -4);

	if((n = recv(sockfd, recvline, 20, MSG_DONTWAIT)) == -1)
		graceful("client_recv recv", -5);

	recvline[n] = '\0';
	printf("recv return: %d    message: %s\n", n, recvline);		
	
	if(!strcmp(recvline, expecline))
		return -1;	//recv unexpected data

	return 0;
}


int client_send(int sockfd, fd_set &wfds, char *sendline, int send_len, const Options &opt)
{
	/*
		nonblock/block not implement
	*/
	FD_ZERO(&wfds);		
	FD_SET(sockfd, &wfds);
	if((select_rtn = select(sockfd+1, NULL, &wfds, NULL, NULL)) == -1)
		graceful("client_send select", -4);
	
	if((n = send(sockfd, sendline, send_len, MSG_DONTWAIT)) == -1)
		graceful("client_send send", -6);
	
	if(n != send_len)
		return -2;	//send error

	return 0;
}


int client_communicate(int sockfd, const Options &opt)
{
	int select_rtn, n;
	char recvline[BUFFER_LEN], sendline[BUFFER_LEN], expecline[BUFFER_LEN];
	fd_set rfds, wfds;
	
	memset(recvline, 0, sizeof(recvline));
	memeset(sendline, 0, sizeof(sendline));
	FD_ZERO(&rfds);		
	FD_SET(sockfd, &rfds);	

	//step 1: receive "StuNo" from server
	strcpy(expecline, "StuNo");
	if(client_recv(sockfd, rfds, expcline, opt) == -1)
		return -1;

	//step 2: send client student number
	uint32_t h_stuNo = 1652571;
	uint32_t n_stuNo = htonl(h_stuNo);	//hostlong to netlong
	memcpy(sendline, &n_stuNo, sizeof(uint32_t));	
	if(client_send(sockfd, wfds, sendline, sizeof(uint32_t), const Options &opt) == -2)
		graceful("client_communicate send", -6);

	//step 3: recv "pid" from server
	strcpy(expecline, "pid");
	if(client_recv(sockfd, rfds, expecline, opt) == -1)
		return -1;

	//step 4: send client pid
	uint32_t n_pid;
	pid_t pid = getpid();
				//if fork,	 send: pid
	if(opt.fork)
		n_pid = htonl((uint32_t)pid); 
				//if nofork, send: pid<<16 + socket_id
	else
		n_pid = htonl((uint32_t)( ((int)pid)<<16 + sockfd ));
	memcpy(sendline, &n_pid, sizeof(uint32_t));	
	if(client_send(sockfd, wfds, sendline, sizeof(uint32_t), const Options &opt) == -2)
		graceful("client_communicate send", -6);

	//step 5: recv "TIME" from server
	strcpy(expecline, "TIME");
	if(client_recv(sockfd, rfds, expecline, opt) == -1)
		return -1;

	//step 6: send client current time(yyyy-mm-dd hh:mm:ss, 19 words)
	strcpy(sendline, getCurrentTime());	
	if(client_send(sockfd, wfds, sendline, 19, const Options &opt) == -2)
		graceful("client_communicate send", -6);

	//step 7: recv "str*****" from server

	//step 8: send rank string 

	//step 9: recv "end" from server

}


int creat_connection(const Options &opt)
{
	if((sockfd == socket(AF_INET, SOCK_STREAM, 0)) < 0)
		graceful("socket", -2);

	if(!opt.block)
	{
		//nonblock
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

	if(communicate(sockfd, opt) == -1)
	{
		/*
			reconnect!
		/*
	}
	/*
		SO_LINGER check
	*/
	cout<<"client end!"<<endl;
	close(sockfd);
}


int client_fork(const Options &opt)
{
	error = -1;
	for(int i=0; i<opt.num; i++)
		creat_connection(opt);
}


int client_fork(const Options &opt)
{
	error = -1;
	for(int i=0; i<opt.num; i++)
	{
		pid_t fpid;
		fpid = fork();
		if(fpid < 0)
			graceful("fork", -10);
		else if(fpid == 0)
			creat_connection(opt);
		else
			continue;
	}
	/*
		handle signal from child proccess
	*/
}


int main(int argc, char *argv[]) {

    // process arguments
    Options opt = parse_arguments(argc, argv, true);

   	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(stoi(opt.port));
	if(inet_pton(AF_INET, opt.ip.c_str(), &servaddr.sin_addr) < 0)
		graceful("Invalid ip address", -1);

    if (opt.fork)
        client_fork(opt);
    else
        client_nofork(opt);
    return 0;
}

















// int main_reference(int argc, char *argv[]) {
//     if (argc != 4) {
//        cerr << "Usage: " << argv[0] << "hostname port1 port2\n";
//        exit(EXIT_FAILURE);
//     }
// 
//     char *hostname = argv[1], *server_port1 = argv[2], *server_port2 = argv[3];;
// 
//     /////////////// connect server 1
//     // hints
//     struct addrinfo hints, *ai, *p;
//     memset(&hints, 0, sizeof(hints));
//     hints.ai_family = AF_UNSPEC;
//     hints.ai_socktype = SOCK_STREAM;
// 
//     // get host address and connect
//     int rv = getaddrinfo(hostname, server_port1, &hints, &ai);
//     if (rv != 0) {
//         cerr << gai_strerror(rv) << endl;
//         exit(1);
//     }
// 
//     int sfd1, sfd2;
//     for (p = ai; p != NULL; p = p->ai_next) {
//         if ((sfd1 = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) continue;
//         if (connect(sfd1, p->ai_addr, p->ai_addrlen) == 0) break; // success
//         close(sfd1);
//     }
//     if (p == NULL) { // no address succeeded
//         perror("Could not connect");
//         exit(2);
//     }
//     else cout << "Connection established\n";
//     freeaddrinfo(ai);
// 
//     // set non-blocking connection
//     fcntl(sfd1, F_SETFL, O_NONBLOCK);
// 
// 
//     /////////////// connect server 2
//     // hints
//     memset(&hints, 0, sizeof(hints));
//     hints.ai_family = AF_UNSPEC;
//     hints.ai_socktype = SOCK_STREAM;
// 
//     // get host address and connect
//     rv = getaddrinfo(hostname, server_port2, &hints, &ai);
//     if (rv != 0) {
//         cerr << gai_strerror(rv) << endl;
//         exit(1);
//     }
// 
//     for (p = ai; p != NULL; p = p->ai_next) {
//         if ((sfd2 = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) continue;
//         if (connect(sfd2, p->ai_addr, p->ai_addrlen) == 0) break; // success
//         close(sfd2);
//     }
//     if (p == NULL) { // no address succeeded
//         perror("Could not connect");
//         exit(2);
//     }
//     else cout << "Connection established\n";
//     freeaddrinfo(ai);
// 
//     // set non-blocking connection
//     fcntl(sfd2, F_SETFL, O_NONBLOCK);
// 
//     /////////////// finished connection
// 
//     // prepare variables used by select()
//     fd_set master, select_fd;      // master file descriptor list
//     int fdmax = sfd2;          // maximum file descriptor number
// 
//     // recv loop
//     // initialize buffer
//     int buf_size = 10;
//     char buf_send[buf_size+1];
//     for (int i = 0; i < buf_size; i++)
//         buf_send[i] = 'a' + i % 26;
//     buf_send[buf_size] = '\0';
// 
//     int buf_read_size = 100;
//     char buf_read[buf_read_size+1];
// 
//     FD_ZERO(&master);   // clear the master and temp sets
//     FD_SET(sfd1, &master);
//     FD_SET(sfd2, &master);
//     cout << "Entering loop\n";
//     int counter = 0;
//     timeval tv = {1, 0};
//     for(;;) {
//         // use tv to hold the timer select() consumes. only write when the timer is up
//         counter++;
//         select_fd = master;
//         int rv = select(fdmax + 1, &select_fd, NULL, NULL, &tv);
//         int nbytes;
//         switch (rv) {
//             case -1:
//                 perror("Select on write");
//                 exit(4);
//                 break;
//             case 0:
//                 // time's up, send
//                 nbytes = send(sfd1, buf_send, buf_size, 0);
//                 nbytes = send(sfd2, buf_send, buf_size, 0);
//                 cout << "send() returned with value " << nbytes << endl;
//                 if (nbytes <= 0) {
//                     perror("send");
//                     exit(5);
//                 }
// 
//                 // reset timer to 3 second
//                 tv = {3, 0};
// 
//                 break;
//             case 1:
//             case 2:
//                 nbytes = recv(sfd1, buf_read, buf_read_size, 0);
//                 nbytes = recv(sfd2, buf_read, buf_read_size, 0);
//                 cout << nbytes << " bytes received.\n";
//                 if (nbytes <= 0) {
//                     perror("recv");
//                     exit(6);
//                 }
//                 // successfully received
//                 buf_read[nbytes] = '\0';
//                 cout << "Here is the content transmitted: " << buf_read << endl;
//                 break;
//             default:
//                 break;
//         }
//     }
// 
//     // aftermath
//     close(sfd1);
//     close(sfd2);
//     return 0;
// }
