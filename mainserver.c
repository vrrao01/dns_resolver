#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include "wrapper.h"
#define MAXLINE 100

void getHostIPMapping(char *, char *, char *);
void signalHandler(int);

int serverSocket, clientSocket; // Defined in global scope for signal handler

int main(int argc, char **argv)
{
	signal(SIGINT, signalHandler);

	char hostname[MAXLINE] = {0};
	char ip[MAXLINE] = {0};
	char request[MAXLINE] = {0};
	char databaseLine[300] = {0};
	char requestType;
	struct sockaddr_in serverAddress, clientAddress;

	// Verify input
	if (argc != 2)
	{
		printf("invalid input format: use ./server.o <ServerPort>\n");
		exit(1);
	}

	// Create server side listening socket
	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocket < 1)
	{
		printf("socket could not be created\n");
		exit(1);
	}

	// Bind address to the server socket
	bzero(&serverAddress, sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddress.sin_port = htons(getPort(argv[1], serverSocket));
	if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
	{
		printf("could not bind socket\n");
		closeAndExit(1, serverSocket);
	}

	// Wait for connection requests from clients
	if (listen(serverSocket, 25) < 0)
	{
		printf("listen error\n");
		closeAndExit(1, serverSocket);
	}

	FILE *filePointer;
	int found = 0;
	for (;;)
	{
		int clilen = sizeof(clientAddress);

		// Accept a client's request to communicate with server
		if ((clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddress, (socklen_t *)&clilen)) < 0)
		{
			printf("Could not connect to client\n");
			continue;
		}
		else
		{
			printf("Connection established with %s:%d \n", inet_ntoa(clientAddress.sin_addr), ntohs(clientAddress.sin_port));
		}

		// Fork a child process for concurrency
		if (fork() == 0) // Inside child process
		{
			close(serverSocket);				  // Close the copy of listening socket present in child
			read(clientSocket, request, MAXLINE); // Acknowledge request sent by client
			requestType = request[0] - '0';		  // First byte of request = request type
			printf("Request Type = %c, Request = %s\n", request[0], request + 1);
			// Open DNS server's database
			filePointer = fopen("data.txt", "r");
			while (fgets(databaseLine, sizeof(databaseLine), filePointer))
			{
				getHostIPMapping(databaseLine, hostname, ip); // Save hostname and IP from file into respective arrays
				if (requestType == 1 && strcasecmp(hostname + 1, request + 1) == 0)
				{
					found = 1;
					write(clientSocket, ip, MAXLINE); // Send IP for Type 1 request
					break;
				}
				else if (strcasecmp(ip + 1, request + 1) == 0)
				{
					found = 1;
					write(clientSocket, hostname, MAXLINE); // Send hostname for Type 2 request
					break;
				}
				// Clear out arrays that are reused
				bzero(hostname, MAXLINE);
				bzero(ip, MAXLINE);
				bzero(databaseLine, sizeof(databaseLine));
			}
			if (found == 0) // Matching hostname IP mapping not found in database
			{
				bzero(request, MAXLINE);
				strcpy(request, "4Entry not found in the database");
				write(clientSocket, request, MAXLINE); // Send Type 4 response
			}
			fclose(filePointer);
			closeAndExit(0, clientSocket); // Close socket and exit child process
		}
		close(clientSocket); // Close client socket in parent since child caters to client
	}
	closeAndExit(0, serverSocket);
}

/*
Breaks one line from the database into hostname and ip 
*/
void getHostIPMapping(char *line, char *host, char *ip)
{
	int linePtr = 0;
	int ptr = 1;
	host[0] = '3';
	ip[0] = '3';
	while (line[linePtr] != ' ')
	{
		host[ptr] = line[linePtr];
		ptr++;
		linePtr++;
	}
	ptr = 1;
	linePtr++;
	while (line[linePtr] != '\n')
	{
		ip[ptr] = line[linePtr];
		ptr++;
		linePtr++;
	}
}

/*
Gracefully releases the socket resources in case Ctrl C is pressed
*/
void signalHandler(int signum)
{
	close(serverSocket);
	close(clientSocket);
	exit(0);
}