#include "shared_library.hpp"

using namespace std;

//function:
//          maintain fdset in a size of MAX_NUM(MAX_NUM = 200)
//          close error socket(returned from lower layer)
//          calling server_communicate(Socket socket)
//precondition:
//			upper layer maintains a (queue)socket_q;
//postcondition:
//			pop() the socketfd which correctly interacted with client from socket_q,
//			and close the socketfd which failed interacting with client;
//			return 0 when socket_q is empty
//			return -1 when encountered wrong sockfd
int server_loop_nofork(){

	if(socket_q.empty())
		return 0;

	fd_set fds;
	int MAX_NUM = 200, set_cnt = 0;
	int fdmax;          // maximum file descriptor number 
	int sockfd;
	FD_ZERO(&fds);
    
    while(set_cnt < MAX_NUM && !socket_q.empty())
    {
	    if((sockfd = socket_q.front()) < 0)
	    	graceful_return("wrong sockfd", -1);
	    socket_q.pop();

		FD_SET(sockfd, &fds);


		while(socket_q.empty())
	}


}
