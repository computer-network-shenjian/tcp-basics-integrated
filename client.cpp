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
#include <malloc.h>
#include "./shared_library.hpp"
#include "./parse_arguments.hpp"

using namespace std;


struct sockaddr_in   servaddr;
int 	sockfd;
fd_set  fds;
int     flags, error = -1, slen = sizeof(int);
int  	client_send_num;	//communication step 8




unsigned char * creatRandomString()
{
	unsigned char *p;
	p = (unsigned char *)malloc(client_send_num + 1);

	srand( (unsigned)time(NULL) );
	for(int i=0; i<=client_send_num; i++)
		p[i] = rand()%256;
	
	p[client_send_num] = '\0';
	return p;
}

int _recv(int sockfd, fd_set &rfds, char *expecline, int cmp_len, const Options &opt)
{
	int n, select_rtn;
	char recvline[BUFFER_LEN];
	/*
		nonblock/block not implemented
	*/
	FD_ZERO(&rfds);		
	FD_SET(sockfd, &rfds);	
	if((select_rtn = select(sockfd+1, &rfds, NULL, NULL, NULL)) == -1)
		graceful("client_recv select", -4);

	if((n = recv(sockfd, recvline, 20, MSG_DONTWAIT)) == -1)
		graceful("client_recv recv", -5);

	recvline[n] = '\0';
	printf("recv return: %d    message: %s\n", n, recvline);		
	
	//comparing failed: strlen not equal
	if(strlen(recvline) != strlen(expecline))
		return -1;

	//comparing only first cmp_len words of expecline
	char cmp_1[100], cmp_2[100];
	memcpy(cmp_1, expecline, cmp_len);
	cmp_1[cmp_len] = '\0';
	memcpy(cmp_2, recvline, cmp_len);
	cmp_2[cmp_len] = '\0';

	if(!strcmp(cmp_1, cmp_2))
		return -1;	//unexpected data

	//especially for client step 8: recv "str*****" from server
	if(cmp_len == 3)
	{
		//copy random number received
		strcpy(cmp_1, &expecline[3]);	
		client_send_num = atoi(cmp_1);
		// if wrong random num: return -1(unexpected data)
		if(client_send_num < 32768 || client_send_num > 99999)
			return -1;
	}

	return 0;
}


int _send(int sockfd, fd_set &wfds, char *sendline, int send_len, const Options &opt)
{
	int n, select_rtn;
	/*
		nonblock/block not implemented
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
	memset(sendline, 0, sizeof(sendline));
	FD_ZERO(&rfds);		
	FD_SET(sockfd, &rfds);	

	//step 1: receive "StuNo" from server
	strcpy(expecline, "StuNo");
	if(_recv(sockfd, rfds, expecline, strlen(expecline), opt) == -1)
		return -1;

	//step 2: send client student number
	uint32_t h_stuNo = 1652571;
	uint32_t n_stuNo = htonl(h_stuNo);	//hostlong to netlong
	memcpy(sendline, &n_stuNo, sizeof(uint32_t));	
	if(_send(sockfd, wfds, sendline, sizeof(uint32_t), opt) == -2)
		graceful("client_communicate send", -6);

	//step 3: recv "pid" from server
	strcpy(expecline, "pid");
	if(_recv(sockfd, rfds, expecline, strlen(expecline), opt) == -1)
		return -1;

	//step 4: send client pid
	uint32_t n_pid;
	pid_t pid = getpid();
					//if fork,	 send: pid
	if(opt.fork)
		n_pid = htonl((uint32_t)pid); 
	else 			//if nofork, send: pid<<16 + socket_id
		n_pid = htonl((uint32_t)( ((int)pid)<<16 + sockfd ));

	memcpy(sendline, &n_pid, sizeof(uint32_t));	
	if(_send(sockfd, wfds, sendline, sizeof(uint32_t), opt) == -2)
		graceful("client_communicate send", -6);

	//step 5: recv "TIME" from server
	strcpy(expecline, "TIME");
	if(_recv(sockfd, rfds, expecline, strlen(expecline), opt) == -1)
		return -1;

	//step 6: send client current time(yyyy-mm-dd hh:mm:ss, 19 words)
	char current[1024];
	getCurrentTime(current);
	current[19] = '\0';
	strcpy(sendline, current);	
	if(_send(sockfd, wfds, sendline, 19, opt) == -2)
		graceful("client_communicate send", -6);

	//step 7: recv "str*****" from server
	strcpy(expecline, "str*****");	//set ***** to verify the length of recv data
	if(_recv(sockfd, rfds, expecline, 3, opt) == -1)	//only compare first 3 words
		return -1;	

	//step 8: send random string in required length(saved in client_send_num)
	unsigned char * u_sendline[BUFFER_LEN];
	memcpy(u_sendline, creatRandomString(), client_send_num + 1);
	FD_ZERO(&wfds);		
	FD_SET(sockfd, &wfds);
	if((select_rtn = select(sockfd+1, NULL, &wfds, NULL, NULL)) == -1)
		graceful("client_send select", -4);
	
	if((n = send(sockfd, u_sendline, client_send_num, MSG_DONTWAIT)) == -1)
		graceful("client_send send", -6);
	
	if(n != client_send_num)
		return -2;	//send error

	//step 9: recv "end" from server
	strcpy(expecline, "end");	//set ***** to verify the length of recv data
	if(_recv(sockfd, rfds, expecline, strlen(expecline), opt) == -1)	//only compare first 3 words
		return -1;	

	return 0;
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

	if(client_communicate(sockfd, opt) == -1)
	{
		/*
			reconnect!
		*/
	}
	/*
		SO_LINGER check
	*/
	cout<<"client end!"<<endl;
	close(sockfd);
}


int client_nofork(const Options &opt)
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
