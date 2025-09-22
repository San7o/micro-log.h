// SPDX-License-Identifier: MIT

// Create an inet server that reads data sent to it

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h> 

#define READ_BUFF_SIZE 1024
#define PORT 5000

int main(void)
{
  int listenfd = 0, connfd = 0;
  struct sockaddr_in serv_addr; 

  char readBuff[READ_BUFF_SIZE];

  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  memset(&serv_addr, 0, sizeof(serv_addr));
  memset(readBuff, 0, sizeof(readBuff)); 

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(PORT);

  bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)); 

  printf("Listening 0.0.0.0 on port %d\n", PORT);
  
  listen(listenfd, 10); 

  connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);

  ssize_t bytes;
  while (1)
  {
    bytes = read(connfd, readBuff, READ_BUFF_SIZE);
    if (bytes == 0) continue;
    printf("Server received: %s\n", readBuff);
    memset(readBuff, 0, READ_BUFF_SIZE);
  }

  close(connfd);
}
