#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "chopconn.h"
#include "chopdata.h"
#include "chopdebug.h"
#include "choppacket.h"
#include "chopsocket.h"

/*
 * Client/Server Management functions
 */

int accept_new_client(struct server *receiver) {
	// precondition for invalid arguments
	INVAL_CHECK(receiver == NULL);

	// init new client
	struct client *new_client;
	if (create_client(&new_client, receiver->window) < 0) {
		DEBUG_PRINT("init client fail");
		return -ENOMEM;
	}

	// accept new client
	int client_fd = accept_client(new_client, receiver->server_fd);
	if (client_fd < 0) {
		DEBUG_PRINT("accept fail");
		destroy_client(&new_client);
		return client_fd;
	}
	DEBUG_PRINT("new client on fd %d", client_fd);

	// find space to put potential new client in
	size_t destination = -1;
	for (size_t i = 0; i < receiver->max_connections; i++) {
		if (receiver->clients[i] == NULL) {
			destination = i;
			break;
		}
	}

	// no empty space found // TODO: implement refusal sent to client
	if (destination < 0) {
		DEBUG_PRINT("no space for new client, refusing");
		destroy_client(&new_client);
		return -ENOSPC;
	}

	// deposit new client
	receiver->clients[destination] = new_client;

	// track new client
	receiver->cur_connections++;
	DEBUG_PRINT("new client, index %zu", destination);
	return client_fd;
}

int establish_server_connection(const char *address, int port, struct client **dest, size_t buf_size) {
	// precondition for invalid arguments
	INVAL_CHECK(address == NULL || port < 0 || dest == NULL || buf_size < 1);

	// make new client struct
	if (create_client(dest, buf_size) < 0) {
		DEBUG_PRINT("failed to store connection, closing");
		return -ENOMEM;
	}

	// connect to server
	int fd = client_connect(*dest, address, port);
	if (fd < 0) {
		DEBUG_PRINT("failed connect to %s:%d", address, port);
		return fd;
	}
	DEBUG_PRINT("connected to %s:%d", address, port);

	// copy socket into container
	(*dest)->socket_fd = fd;
	(*dest)->server_fd = -1;

	DEBUG_PRINT("connection successful");
	return fd;
}

int remove_client_index(size_t client_index, struct server *host) {
	// precondition for invalid arguments
	INVAL_CHECK(client_index < 0 || host == NULL);

	// invalid index
	if (client_index >= host->max_connections) {
		DEBUG_PRINT("invalid client index");
		return -EINVAL;
	}

	// no client at index
	if (host->clients[client_index] == NULL) {
		DEBUG_PRINT("target index %zu empty", client_index);
		return -ENOENT;
	}

	// destroy client
	if (destroy_client(host->clients + client_index) < 0) {
		DEBUG_PRINT("failed client destruct");
		return -EINVAL;
	}

	DEBUG_PRINT("removed client at index %zu", client_index);
	return 0;
}

int remove_client_address(struct client **target) {
	// precondition for invalid arguments
	INVAL_CHECK(target == NULL);

	// no client at pointer
	if (*target == NULL) {
		DEBUG_PRINT("target is empty");
		return -EINVAL;
	}

	// destroy client
	if (destroy_client(target) < 0) {
		DEBUG_PRINT("failed client destruct");
		return -EINVAL;
	}

	DEBUG_PRINT("removed client at arbitrary address");
	return 0;
}

int process_request(struct client *cli) {
	// precondition for invalid arguments
	INVAL_CHECK(cli == NULL);

	// init packet for receiving
	struct packet *pack;
	if (create_packet(&pack) < 0) {
		DEBUG_PRINT("failed packet init");
		return -ENOMEM;
	}

	// parse the current status of the client
	int status = 0;
	switch (cli->inc_flag) {
		case CANCEL:
			DEBUG_PRINT("client closed");
			// TODO: flush the fd and close the connection (this shouldn't need to happen)
			break;
		default:
			DEBUG_PRINT("client %lu flag", cli->inc_flag);
			status = read_header(cli, pack);
			if (status < 0) {
				cli->inc_flag = CANCEL;
				cli->out_flag = CANCEL;
				break;
			}

			status = parse_header(cli, pack);
			if (status < 0) {
				cli->inc_flag = CANCEL;
				cli->out_flag = CANCEL;
				break;
			}
			break;
	}

	if (destroy_packet(&pack) < 0) {
	    DEBUG_PRINT("failed packet destroy");
        return -errno;
	}

	return status;
}

/*
 * Client/Server Utility Functions
 */

int is_client_status(struct client *cli, pack_stat status) {
	// precondition for invalid arguments
	if (cli == NULL) {
		DEBUG_PRINT("invalid arguments");
		return 0;
	}

	return (cli->inc_flag == status && cli->out_flag == status);
}

int is_address(const char *str) {
	// check valid argument
	if (str == NULL) {
		DEBUG_PRINT("invalid arguments");
		return 0;
	}

	// copy string to local buffer for tokenizing
	size_t cap = strlen(str);
	char addr[cap + 1];
	strcpy(addr, str);

	// tokenize by the periods
	long num;
	char *ptr;
	char *token;
	char *rest = addr;
	for (int i = 0; i < 4; i++) {
		token = strtok_r(rest, ".", &rest);

		// in case less than 4 tokens are available
		if (token == NULL) {
			DEBUG_PRINT("\"%s\" is not in address format", str);
			return 0;
		}

		// convert token to number, check if valid as byte
		num = strtol(token, &ptr, 10);
		if (num < 0 || num > 255) {
			DEBUG_PRINT("\"%s\" has an invalid value", str);
			return 0;
		}
	}

	DEBUG_PRINT("\"%s\" is a valid address", str);
	return 1;
}

int is_name(const char *str);
