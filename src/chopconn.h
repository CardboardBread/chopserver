#ifndef CHOPCONN_H__
#define CHOPCONN_H__

#include "chopconst.h"

/*
 * Client/Server Management functions
 */

int accept_new_client(struct server *receiver);

int establish_server_connection(const char *address, int port, struct client **dest, size_t buf_size);

int remove_client_index(size_t client_index, struct server *host);

int remove_client_address(struct client **target);

int process_request(struct client *cli);

/*
 * Client/Server Utility functions
 */

int is_client_status(struct client *cli, pack_stat status);

int is_address(const char *str);

int is_name(const char *str);

#endif // CHOPCONN_H__
