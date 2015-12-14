/*************************************************************************
    > File Name: recvmsg.cpp
    > Author: cocoii
    > Mail: ys.zhao@helang.com 
    > Created Time: Mon 14 Dec 2015 01:53:32 PM CST
 ************************************************************************/

#include<iostream>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
using namespace std;

#define PORT 7777

int main()
{
	char buf[100];
	ssize_t size;
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	int recvfd = socket(AF_INET, SOCK_DGRAM, 0);
	bind(recvfd, (struct sockaddr*)&addr, sizeof(addr));

	struct iovec iov[1];
	iov[0].iov_base = buf;
	iov[0].iov_len = sizeof(buf);

	struct msghdr msg;
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = 0;

	size = recvmsg(recvfd, &msg, 0);

	printf("%s\n", (char *)msg.msg_iov->iov_base);

	return 0;
}

