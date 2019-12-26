#ifndef __CHOPCONN_H__
#define __CHOPCONN_H__

#include <sys/select.h>

#include "chopconst.h"

/*
 * Client/Server Management functions
 */

int accept_new_client(struct server *receiver, const int bufsize);

int establish_server_connection(const char *address, const int port, struct client **dest, const int bufsize);

int remove_client_index(const int client_index, struct server *host);

int remove_client_address(const int client_index, struct client **target);

int process_request(struct client *cli, fd_set *all_fds);

/*
 * Sending functions
 */

int write_buf_to_client(struct client *cli, const char *msg, const int msg_len);

int send_str_to_all(struct server *host, const char *str);

int send_fstr_to_all(struct server *host, const char *format, ...);

int send_str_to_client(struct client *cli, const char *str);

int send_fstr_to_client(struct client *cli, const char *format, ...);

/*
 * Client/Server Utility functions
 */

int is_client_status(struct client *cli, const int status);

int is_address(const char *str);

int is_name(const char *str);

#endif
