#include <sys/types.h>
#include <sys/socket.h>
#include<pthread.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#define PORT 7788
#define MAXSIZE 1024

int main(int argc, char **argv)
{
  	printf("Welcome! This is a UDP server\n");
							    
  	struct sockaddr_in addr;
    addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

   	int sock;
   	if ( (sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
   	{
      	perror("socket");
      	exit(1);
   	}

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
   	{
       	perror("bind");
       	exit(1);
   	}

  	char buff[MAXSIZE];
 	struct sockaddr_in clientAddr;
   	int n;
 	socklen_t len = sizeof(clientAddr);

  	while (1)
  	{
     	n = recvfrom(sock, buff, MAXSIZE, 0, (struct sockaddr*)&clientAddr, &len);
     	if (n>0)
     	{
         	buff[n] = 0;
     		printf("%s %u says: %s\n", (char *)inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), buff);
        	n = sendto(sock, buff, n, 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
        	if (n < 0)
          	{
            	perror("sendto");
           		break;
        	}
      	}
     	else
     	{
       		perror("recv");
     		break;
   		}
  	}
  	return 0;
}
