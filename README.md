# Multistage DNS Resolver System 
Implemented a 2 stage DNS resolving system as shown: 
![DNS](https://user-images.githubusercontent.com/75874394/142686878-ca03111b-b175-4faf-960e-fe6b069edd09.png)
## Request Message Format: 

| Request Type | Message |
| ------- | --- | 

* Type 1: Message field contains Domain Name and requests for corresponding IP address. 
* Type 2: Message field contains IP address and request for the corresponding Domain Name. 
## Response Message Format: 
| Response Type | Message |
| ------- | --- | 

* Type 3:  Message field contains Domain Name/IP address. 
* Type 4: Message field contains error message “Entry not found in the database”.

## Commands to create executables:
```
gcc mainserver.c -o server.o
gcc -pthread proxy.c -o proxy.o
gcc client.c -o client.o
```

-pthread is used for POSIX semaphores/shared memory for concurrency. 

## Commands to run client,proxy and server:
```
./server.o <DNS_Server_Port>
./proxy.o <Proxy_Server_Port> <DNS_Server_IP> <DNS_Server_Port>
./client.o <Proxy_Server_IP> <Proxy_Server_Port>
```

The above commands must be run in the same order since the client requires proxy running and the proxy requires DNS server running. 
Example commands:
```
./server.o 8124
./proxy.o 8075 127.0.0.1 8124
./client.o 127.0.0.1 8075
```

## Notes  
* Each message transferred has a maximum size of MAXLINE = 100 bytes. 
* The first byte of every message is the request/response type followed by message
* All hostname/ip mappings must be stored in "data.txt" 
* Each line in "data.txt" must contain hostname and ip separated by only one space
* No extra new line should exist at the end of "data.txt"


