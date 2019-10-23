#ifndef _CHOPSOCKET_H_
#define _CHOPSOCKET_H_

#include <netinet/in.h>    /* Internet domain header, for struct sockaddr_in */

int init_server_addr(struct sockaddr_in **addr, const int port);

int setup_server_socket(struct sockaddr_in *self, int *socket, const int num_queue);

int accept_connection(const int listenfd, int *newfd);

int connect_to_server(const char *hostname, const int port, int *socket);

#endif
