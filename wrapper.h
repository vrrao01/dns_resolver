
void closeAndExit(int exitcode, int sockfd)
{
    close(sockfd);
    exit(exitcode);
}

int getPort(char *input, int sockfd)
{
    char *end;
    int number = strtol(input, &end, 10);
    if (input == end)
    {
        printf("invalid port number\n");
        closeAndExit(1, sockfd);
    }
    return number;
}

void Inet_pton(int family, const char *strptr, void *addrptr)
{
    int n;

    if ((n = inet_pton(family, strptr, addrptr)) <= 0)
    {
        printf("inet_pton error for %s\n", strptr);
        exit(1);
    }
}
