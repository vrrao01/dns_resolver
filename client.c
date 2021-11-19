
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdlib.h>
#include "wrapper.h"

const int MAXLINE = 100;
int sockfd;

int getUserRequest(char *, int);
void getResponse(int, void *, unsigned long);
void clearStdin();

int main(int argc, char **argv)
{
	char reqMessage[MAXLINE];
	struct sockaddr_in servaddr;

	// Verify input
	if (argc != 3)
	{
		printf("invalid input format: use ./client.o <ServerIP> <ServerPort>\n");
		exit(1);
	}

	// Create a socket for client
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 1)
	{
		printf("could not create socket \n");
		exit(1);
	}

	// Fill server IP and port
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(getPort(argv[2], sockfd));
	Inet_pton(AF_INET, argv[1], &servaddr.sin_addr);

	// Connect to server
	if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
	{
		printf("could not connect to server.\n");
		closeAndExit(1, sockfd);
	}

	if (getUserRequest(reqMessage, MAXLINE) >= 0)
	{
		// Send DNS request to server
		if (write(sockfd, reqMessage, MAXLINE) != MAXLINE)
		{
			printf("could not send message to server\n");
			closeAndExit(1, sockfd);
		}

		// Clear reqMessage in order to fill it with server response
		bzero(reqMessage, sizeof(reqMessage));

		// Print server response
		getResponse(sockfd, reqMessage, MAXLINE);
		printf("\n");
	}

	closeAndExit(0, sockfd);
}

/*
Takes user's request from input and saves it in requestBuffer.
Returns -1 if user enters invalid request type else returns 0.
*/
int getUserRequest(char *requestBuffer, int length)
{
	int reqType;
	bzero(requestBuffer, length);
	printf("Enter the request type: \n");
	scanf("%d", &reqType);
	if (reqType < 1 || reqType > 2)
	{
		printf("Invalid request type\n");
		return -1;
	}
	requestBuffer[0] = '0' + reqType;
	printf("Enter the request message:\n");
	scanf("%s", &requestBuffer[1]);
	return 0;
}

/*
Prints the response from server
*/
void getResponse(int sockfd, void *buffer, unsigned long size)
{
	read(sockfd, buffer, size);
	printf("\nResponse from server:\nResponse Type: %c\nResponse Message: %s", ((char *)buffer)[0], ((char *)buffer) + 1);
	bzero(buffer, size);
}

/*
Clears standard input buffer of any trailing \n 
*/
void clearStdin()
{
	char c;
	while ((c = getchar()) != '\n' && c != EOF)
	{
	}
}