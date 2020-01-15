#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/select.h>

#include "chopconn.h"
#include "chopconst.h"
#include "chopdata.h"
#include "chopdebug.h"
#include "choppacket.h"
#include "chopsocket.h"

/*
 * Client/Server Management functions
 */

int accept_new_client(struct server *receiver, const size_t bufsize) {
	// precondition for invalid arguments
	if (receiver == NULL || bufsize < 1) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	// init new client
	struct client *newcli;
	if (init_client_struct(&newcli, bufsize) < 0) {
		DEBUG_PRINT("init client fail, refusing incoming");
		refuse_connection(receiver->server_fd);
		return -ENOMEM;
	}

	// accept new client
	int client_fd = accept_connection(receiver->server_fd, &(newcli->address));
	if (client_fd < 0) {
		DEBUG_PRINT("accept fail");
		return client_fd;
	}
	DEBUG_PRINT("new client on fd %d", client_fd);

	// find space to put potential new client in
	int destination = -1;
	for (int i = 0; i < receiver->max_connections; i++) {
		if (receiver->clients[i] == NULL) {
			destination = i;
		}
	}

	// no empty space found
	if (destination < 0) {
		DEBUG_PRINT("no space for new client, refusing");
		close(client_fd);
		destroy_client_struct(&newcli);
		return -ENOSPC;
	}

	// setup new client
	receiver->clients[destination] = newcli;
	newcli->socket_fd = client_fd;
	newcli->server_fd = receiver->server_fd;
	newcli->inc_flag = 0;
	newcli->out_flag = 0;
	newcli->window = bufsize;

	// track new client
	receiver->cur_connections++;
	DEBUG_PRINT("new client, index %d", destination);
	return client_fd;
}

int establish_server_connection(const char *address, const int port, struct client **dest, const int bufsize) {
	// precondition for invalid arguments
	if (address == NULL || port < 0 || dest == NULL || bufsize < 1) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	// make new client struct
	if (init_client_struct(dest, bufsize) < 0) {
		DEBUG_PRINT("failed to store connection, closing");
		return -ENOMEM;
	}

	// connect to server
	int fd = connect_to_server(&((*dest)->address), address, port);
	if (fd < 0) {
		DEBUG_PRINT("failed connect to %s:%d", address, port);
		return fd;
	}
	DEBUG_PRINT("connected to %s:%d", address, port);

	// initialize client struct
	struct client *init = *dest;
	init->socket_fd = fd;
	init->server_fd = -1;
	init->inc_flag = 0;
	init->out_flag = 0;
	init->window = bufsize;

	DEBUG_PRINT("connection successful");
	return 0;
}

int remove_client_index(const int client_index, struct server *host) {
	// precondition for invalid arguments
	if (client_index < MIN_FD || host == NULL) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	// no client at index
	if (host->clients[client_index] == NULL) {
		DEBUG_PRINT("target index %d empty", client_index);
		return -ENOENT;
	}

	// destroy client
	if (destroy_client_struct(host->clients + client_index) < 0) {
		DEBUG_PRINT("failed client destruct");
		return -EINVAL;
	}

	DEBUG_PRINT("removed client at index %d", client_index);
	return 0;
}

int remove_client_address(const int client_index, struct client **target) {
	// precondition for invalid arguments
	if (client_index < MIN_FD) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	// no client at pointer
	if (*target == NULL) {
		DEBUG_PRINT("target is empty");
		return -EINVAL;
	}

	// destroy client
	if (destroy_client_struct(target) < 0) {
		DEBUG_PRINT("failed client destruct");
		return -EINVAL;
	}

	DEBUG_PRINT("removed client at arbitrary address");
	return 0;
}

int process_request(struct client *cli, fd_set *all_fds) {
	// precondition for invalid arguments
	if (cli == NULL || all_fds == NULL) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	// init packet for recieving
	struct packet *pack;
	if (init_packet_struct(&pack) > 0) {
		DEBUG_PRINT("failed packet init");
		return -ENOMEM;
	}

	int status = 0;
	switch (cli->inc_flag) {
		case CANCEL:
			DEBUG_PRINT("client closed");
			// TODO: flush the fd and close the connection (this shouldn't need to happen)
			//break;
		default:
			DEBUG_PRINT("client %d flag", cli->inc_flag);
			status = read_header(cli, pack);
			if (status < 0) {
				break;
			}
			status = parse_header(cli, pack);
			break;
	}

	return status;
}

/*
 * Sending functions
 */

int write_buf_to_client(struct client *cli, const char *msg, const int msg_len) {
	// precondition for invalid arguments
	if (cli == NULL || msg == NULL || msg_len < 0) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	// check if message is nonexistent
	if (msg_len == 0) {
		DEBUG_PRINT("empty message");
		return 0;
	}

	// initialize packet for buffer
	struct packet *pack;
	if (init_packet_struct(&pack) < 0) {
		DEBUG_PRINT("failed init packet");
		return -ENOMEM;
	}

	// assemble packet header with text signal
	if (assemble_header(pack, 0, START_TEXT, 1, msg_len) < 0) {
		DEBUG_PRINT("failed header assemble");
		return -EINVAL;
	}

	// add new data section
	struct buffer *buf;
	if (append_buffer(pack, msg_len, &buf) < 0) {
		DEBUG_PRINT("failed data expansion");
		return -ENOMEM;
	}

	// allocate packet body
	if (assemble_body(buf, msg, msg_len) < 0) {
		DEBUG_PRINT("failed body assemble");
		return -EINVAL;
	}

	// write to client
	if (write_packet_to_client(cli, pack) < 0) {
		DEBUG_PRINT("failed write");
		return -1; // TODO: possibly capture return for better error reporting
	}

	// destroy allocated packet
	if (destroy_packet_struct(&pack) < 0) {
		DEBUG_PRINT("failed packet destroy");
		return -EINVAL; // TODO: this probably cannot fail
	}

	DEBUG_PRINT("wrote %d byte buffer", msg_len);
	return 0;
}

int send_str_to_client(struct client *cli, const char *str) {
	// precondition for invalid arguments
	if (cli == NULL || str == NULL) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	// delegate writing
	int ret = write_buf_to_client(cli, str, strlen(str) + 1);
	if (ret < 0) {
		DEBUG_PRINT("failed write");
		return ret;
	}

	DEBUG_PRINT("str written");
	return 0;
}

int send_fstr_to_client(struct client *cli, const char *format, ...) {
	// precondition for invalid argument
	if (cli == NULL) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	// get length of assembled fstr
	va_list uargs;
	va_start(uargs, format);
	int len = vdprintf(-1, format, uargs);
	va_end(uargs);

	// buffer for assembling format string
	char msg[len + 1];

	// assemble string into buffer
	va_list args;
	va_start(args, format);
	vsnprintf(msg, strlen(format) * 2, format, args);
	va_end(args);

	// delegate writing
	int ret = write_buf_to_client(cli, msg, len + 1);
	if (ret < 0) {
		DEBUG_PRINT("failed write");
		return ret;
	}

	DEBUG_PRINT("fstr written");
	return 0;
}

int send_str_to_all(struct server *host, const char *str) {
	// precondition for invalid arguments
	if (host == NULL || str == NULL) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	// iterate through all available clients, stopping if any fail
	for (int i = 0; i < host->max_connections; i++) {
		if (host->clients[i] != NULL) {
			if (send_str_to_client(host->clients[i], str) < 0) {
				DEBUG_PRINT("failed sending at %d", i);
				return 1;
			}
		}
	}

	DEBUG_PRINT("str written to all");
	return 0;
}

int send_fstr_to_all(struct server *host, const char *format, ...) {
	// precondition for invalid arguments
	if (host == NULL) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	// get variable arguments
	va_list args;
	va_start(args, format);
	//vsnprintf(msg, TEXT_LEN, format, args);

	// iterate through all available clients, stopping if any fail
	for (int i = 0; i < host->max_connections; i++) {
		if (host->clients[i] != NULL) {
			if (send_fstr_to_client(host->clients[i], format, args) < 0) {
				DEBUG_PRINT("failed sending at %d", i);
				return 1;
			}
		}
	}

	// close access to variable arguments
	va_end(args);

	DEBUG_PRINT("fstr written to all");
	return 0;
}

/*
 * Client/Server Utility Functions
 */

int is_client_status(struct client *cli, const int status) {
	// precondition for invalid arguments
	if (cli == NULL || status < 0) {
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
	int cap = strlen(str);
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
