#ifndef _CHOPSOCKET_H_
#define _CHOPSOCKET_H_

#include <netinet/in.h>    /* Internet domain header, for struct sockaddr_in */

/*
 * Initialize a server address associated with the given port.
 */
int init_server_addr(struct sockaddr_in *addr, const int port);

/*
 * Create and setup a socket for a server to listen on.
 */
int setup_server_socket(struct sockaddr_in *self, const int port, const int num_queue);

/*
 * Wait for and accept a new connection.
 * Return -1 if the accept call failed.
 */
int accept_connection(const int listenfd);

/*
 * Waits for an incoming connection, immediately closing the connection.
 * Returns 0 on success, negative on error.
 */
int refuse_connection(const int listenfd);

/*
 * Create a socket and connect to the server indicated by the port and hostname
 */
int connect_to_server(struct sockaddr_in *addr, const char *hostname, const int port);

#endif
