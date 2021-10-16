#ifndef CHOPSOCKET_H__
#define CHOPSOCKET_H__

#include <netinet/in.h>    /* Internet domain header, for struct sockaddr_in */

/*
 * Initialize a server address associated with the given port.
 */
int init_server_addr(struct sockaddr_in *addr, int port);

/*
 * Create and setup a socket for a server to listen on.
 */
int setup_server_socket(struct sockaddr_in *self, int port, int num_queue);

/*
 * Wait for and accept a new connection.
 * Return -1 if the accept call failed.
 */
int accept_connection(int listenfd, struct sockaddr_in *peer);

/*
 * Waits for an incoming connection, immediately closing the connection.
 * Returns 0 on success, negative on error.
 */
int refuse_connection(int listenfd);

/*
 * Create a socket and connect to the server indicated by the port and hostname
 */
int connect_to_server(struct sockaddr_in *addr, const char *hostname, int port);

/*
 * Starts a server on the values that are set in its struct
 */
int open_server(struct server *target);

/*
 * Accepts a client into the given struct.
 */
int accept_client(struct client *target, int listen_fd);

/*
 * Connects to a server, placing the results in the given client
 */
int client_connect(struct client *target, const char *hostname, int port);

#endif // CHOPSOCKET_H__
