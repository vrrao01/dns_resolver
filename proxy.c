#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <signal.h>
#include <semaphore.h>
#include <sys/shm.h>
#include <time.h>
#include "wrapper.h"
#define MAXLINE 100
#define CACHE_SIZE 3

int getResponsefromDNSServer(char *, char *, const char *, char *);
int searchProxyCache(const char *, char *);
void saveInCache(const char *, const char *);
void signalHandler(int);

struct proxyCache
{
    int head;
    int tail;
    char hostname[CACHE_SIZE + 1][MAXLINE];
    char ip[CACHE_SIZE + 1][MAXLINE];
    sem_t mutex;
};

int proxySocket, clientSocket, sockfd;
int cacheID;
struct proxyCache *cache;

int main(int argc, char **argv)
{
    signal(SIGINT, signalHandler);
    char request[MAXLINE] = {0};
    char response[MAXLINE] = {0};
    struct sockaddr_in proxyAddress, clientAddress;

    // Verify input
    if (argc != 4)
    {
        printf("invalid input format: use ./proxy.o <ProxyPort> <ServerIP> <ServerPort>\n");
        exit(1);
    }

    // Creating shared memory region for proxy cache.
    cacheID = shmget(IPC_PRIVATE, sizeof(struct proxyCache), IPC_CREAT | 0666);
    cache = (struct proxyCache *)shmat(cacheID, NULL, 0);
    bzero(cache, sizeof(struct proxyCache));
    // Initialising semaphore for mutually exclusive access to cache between child processes
    sem_init(&(cache->mutex), 1, 1);

    // Create server side listening socket
    proxySocket = socket(AF_INET, SOCK_STREAM, 0);
    if (proxySocket < 1)
    {
        printf("socket could not be created\n");
        exit(1);
    }

    // Bind address to the server socket
    bzero(&proxyAddress, sizeof(proxyAddress));
    proxyAddress.sin_family = AF_INET;
    proxyAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    proxyAddress.sin_port = htons(getPort(argv[1], proxySocket));
    if (bind(proxySocket, (struct sockaddr *)&proxyAddress, sizeof(proxyAddress)) < 0)
    {
        printf("could not bind socket\n");
        closeAndExit(1, proxySocket);
    }

    // Wait for connection requests from clients
    if (listen(proxySocket, 25) < 0)
    {
        printf("listen error\n");
        closeAndExit(1, proxySocket);
    }

    for (;;)
    {
        int clilen = sizeof(clientAddress);
        // Accept a client's request to communicate with server
        if ((clientSocket = accept(proxySocket, (struct sockaddr *)&clientAddress, (socklen_t *)&clilen)) < 0)
        {
            printf("couldnt not connect to client\n");
            continue;
        }
        else
        {
            printf("Connection established with %s:%d \n", inet_ntoa(clientAddress.sin_addr), ntohs(clientAddress.sin_port));
        }

        // Fork a child process for concurrent service to multiple clients

        if (fork() == 0)
        {
            close(proxySocket);                   // Close copy of listening socket in child
            read(clientSocket, request, MAXLINE); // Acknowledge request from client
            printf("%s:%d - | %c | %s |\n", inet_ntoa(clientAddress.sin_addr), ntohs(clientAddress.sin_port), request[0], request + 1);

            sem_wait(&(cache->mutex));                   // Lock the cache
            if (searchProxyCache(request, response) < 0) // Mapping not present in proxy cache
            {
                // Forward request to DNS Server. If communication with server fails, send Type 4 Response
                if (getResponsefromDNSServer(argv[2], argv[3], request, response) < 0)
                {
                    bzero(response, MAXLINE);
                    strcpy(response, "4Entry not found in the database");
                }
                else if (response[0] == '3')
                {
                    saveInCache(request, response);
                }
            }
            sem_post(&(cache->mutex));              // Unlock the cache
            write(clientSocket, response, MAXLINE); // Send response to client
            closeAndExit(0, clientSocket);
        }

        close(clientSocket);
    }
    shmctl(cacheID, IPC_RMID, NULL); // Delete shared memory resource
    close(proxySocket);
    exit(0);
}

/*
Sends request to DNS Server when proxy doesn't have mapping in its cache.
*/
int getResponsefromDNSServer(char *dnsIP, char *dnsPort, const char *request, char *response)
{
    struct sockaddr_in dnsServer;

    bzero(response, MAXLINE);
    // Create socket for proxy server to communicate with DNS server
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 1)
    {
        printf("could not create socket to communicate with server\n");
        return -1;
    }

    // Fill DNS server IP and port information
    bzero(&dnsServer, sizeof(dnsServer));
    dnsServer.sin_family = AF_INET;
    dnsServer.sin_port = htons(strtol(dnsPort, NULL, 10));
    Inet_pton(AF_INET, dnsIP, &dnsServer.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&dnsServer, sizeof(dnsServer)) < 0)
    {
        printf("could not connect to server.\n");
        close(sockfd);
        return -1;
    }

    if (write(sockfd, request, MAXLINE) != MAXLINE)
    {
        printf("could not send request to server\n");
        close(sockfd);
        return -1;
    }
    if (read(sockfd, response, MAXLINE) < 0)
    {
        printf("could not receive response from server\n");
        close(sockfd);
        return -1;
    }
    close(sockfd);
    return 0;
}

/*
Searches for the requested hostname/IP in the proxy's cache.
*/
int searchProxyCache(const char *request, char *response)
{
    int requestType = request[0] - '0';
    int pos = -1;
    if (requestType == 1)
    {
        for (int i = cache->head; i != cache->tail; i = (i + 1) % (CACHE_SIZE + 1))
        {
            if (strcasecmp(request + 1, cache->hostname[i]) == 0)
            {
                pos = i;
                break;
            }
        }
        if (pos != -1)
        {
            response[0] = '3';
            memcpy(response + 1, cache->ip[pos], MAXLINE - 1);
            return 0;
        }
    }
    else
    {
        for (int i = cache->head; i != cache->tail; i = (i + 1) % (CACHE_SIZE + 1))
        {
            if (strcasecmp(request + 1, cache->ip[i]) == 0)
            {
                pos = i;
                break;
            }
        }
        if (pos != -1)
        {
            response[0] = '3';
            memcpy(response + 1, cache->hostname[pos], MAXLINE - 2);
            return 0;
        }
    }
    return -1;
}

/*
Saves hostname/IP obtained from DNS Server response in cache
*/
void saveInCache(const char *request, const char *response)
{
    if (request[0] == '1')
    {
        bzero(cache->hostname[cache->tail], MAXLINE);
        bzero(cache->ip[cache->tail], MAXLINE);
        memcpy(cache->hostname[cache->tail], request + 1, MAXLINE - 1);
        memcpy(cache->ip[cache->tail], response + 1, MAXLINE - 1);
    }
    else
    {
        bzero(cache->hostname[cache->tail], MAXLINE);
        bzero(cache->ip[cache->tail], MAXLINE);
        memcpy(cache->ip[cache->tail], request + 1, MAXLINE - 1);
        memcpy(cache->hostname[cache->tail], response + 1, MAXLINE - 1);
    }
    cache->tail = (cache->tail + 1) % (CACHE_SIZE + 1);
    if (cache->head == cache->tail)
    {
        cache->head = (cache->head + 1) % (CACHE_SIZE + 1);
    }
    printf("Cache Miss! Printing Cache:\n");
    for (int i = cache->head; i != cache->tail; i = (i + 1) % (CACHE_SIZE + 1))
    {
        printf("%s : %s\n", cache->hostname[i], cache->ip[i]);
    }
}

/*
Gracefully releases the socket resources and shared memory cache
*/
void signalHandler(int signum)
{
    close(sockfd);
    close(proxySocket);
    close(clientSocket);
    shmctl(cacheID, IPC_RMID, NULL);
    exit(0);
}