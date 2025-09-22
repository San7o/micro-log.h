// SPDX-License-Identifier: MIT

// Create an unix socket server that reads data sent to it

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
#include <sys/un.h>

#define READ_BUFF_SIZE 1024
#define UNIX_SOCKET_FILE "my-unix-socket"

int main(void)
{
  int listenfd = 0, connfd = 0;
  struct sockaddr_un serv_addr; 

  char readBuff[READ_BUFF_SIZE];

  listenfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (listenfd < 0)
  {
    perror("Error creating socket");
    return 1;
  }
  
  memset(&serv_addr, 0, sizeof(serv_addr));
  memset(readBuff, 0, sizeof(readBuff)); 

  serv_addr.sun_family = AF_UNIX;
  strcpy(serv_addr.sun_path, UNIX_SOCKET_FILE);
  if (bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
  {
    perror("Error binding");
    return 1;
  }

  printf("Listening on file %s\n", UNIX_SOCKET_FILE);
  
  listen(listenfd, 10); 

  connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);
  if (connfd < 0)
  {
    perror("Error accepting connection");
    return 1;
  }

  ssize_t bytes;
  while (1)
  {
    bytes = read(connfd, readBuff, READ_BUFF_SIZE);
    if (bytes == 0) continue;
    printf("Server received: %s\n", readBuff);
    memset(readBuff, 0, READ_BUFF_SIZE);
  }

  close(connfd);
  return 0;
}
