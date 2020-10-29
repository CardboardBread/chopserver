#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#include "chopconn.h"
#include "chopdebug.h"
#include "choppacket.h"
#include "chopsocket.h"

#ifndef PORT
#define PORT 50001
#endif

#define BUFSIZE 255
#define CONNECTION_QUEUE 5
#define MAX_CONNECTIONS 20

int sigint_received;

struct server *host;

void sigint_handler(int code);

void sigint_handler(int code) {
	DEBUG_PRINT("received SIGINT, setting flag");
	sigint_received = 1;
}

time_t next_minute(time_t curr_second) {
	time_t curr_minute = curr_second / 60;
	time_t next_minute = curr_minute + 1;
	time_t remaining = next_minute * 60 - curr_second;
	DEBUG_PRINT("%ld seconds left to the minute", remaining);
	return remaining;
}

int main(void) {
	// Reset SIGINT received flag.
	sigint_received = 0;

	// mark debug statements as serverside
	header_type = 0;

	// setup SIGINT handler
	struct sigaction int_action;
	int_action.sa_handler = sigint_handler;
	sigemptyset(&int_action.sa_mask);
	int_action.sa_flags = SA_RESTART; // ensures interrupted calls dont error out

	// store old SIGINT behaviour
	struct sigaction int_action_old;

	// replace existing SIGINT handler with custom function
	if (sigaction(SIGINT, &int_action, &int_action_old) < 0) {
		DEBUG_PRINT("sigaction: error");
		exit(1);
	}
	DEBUG_PRINT("sigint_handler attached");

	// initialize server config
	if (init_server(&host, PORT, MAX_CONNECTIONS, CONNECTION_QUEUE, BUFSIZE) < 0) {
		DEBUG_PRINT("failed server struct init");
		exit(1);
	}
	DEBUG_PRINT("server struct on %d slots", MAX_CONNECTIONS);

	// setup server socket
	int server_start = open_server(host);
	if (server_start < 0) {
		DEBUG_PRINT("failed server socket init");
		exit(-server_start);
	}
	DEBUG_PRINT("server listening on all interfaces");

	// setup fd set for selecting
	int max_fd = host->server_fd;
	fd_set all_fds, listen_fds;
	FD_ZERO(&all_fds);
	FD_SET(host->server_fd, &all_fds);

	// setup timeout
	struct timeval timeout;

	int run = 1;
	while (run) {
		printf("\n");

		// setup timeout for time until next minute
		timeout.tv_sec = next_minute(time(NULL));
		timeout.tv_usec = 0;

		// closing connections and freeing memory before the process ends
		if (sigint_received) {
			DEBUG_PRINT("caught SIGINT, exiting");
			destroy_server(&host);
			exit(SIGINT);
		}

		// selecting
		listen_fds = all_fds;
		int nready = select(max_fd + 1, &listen_fds, NULL, NULL, &timeout);
		if (nready < 0) {
			if (errno == EINTR) {
				continue;
			} else {
				DEBUG_PRINT("failed select");
				exit(1);
			}
		} else if (nready == 0) {
			DEBUG_PRINT("timeout reached, resetting");

			// send time to all clients
			for (int i = 0; i < host->max_connections; i++) {
				if (host->clients[i] != NULL) {
					if (write_wordpack(host->clients[i],
						(struct packet_header) {0, ENQUIRY, ENQUIRY_TIME, sizeof(time_t)}, time(NULL)) < 0) {
						DEBUG_PRINT("failed time update to client %d", host->clients[i]->socket_fd);
					}
				}
			}
		}

		// check all clients if they can read
		for (int index = 0; index < host->max_connections; index++) {
			struct client *client = host->clients[index];

			// relies on short circuiting
			if (client != NULL && FD_ISSET(client->socket_fd, &listen_fds)) {
				if (process_request(client) < 0) {
					DEBUG_PRINT("request error");
					//exit(1); // TODO: remove once failing a packet isn't really bad
				}

				// if a client requested a cancel
				if (is_client_status(client, CANCEL)) {
					FD_CLR(client->socket_fd, &all_fds);
					MESSAGE_PRINT(client->socket_fd, esc_confirm);
					remove_client_index(index, host);
				}
			}
		}

		// accept new client
		if (FD_ISSET(host->server_fd, &listen_fds)) {
			int client_fd = accept_new_client(host);
			if (client_fd < 0) {
				DEBUG_PRINT("failed accept");
				continue;
			}

			if (client_fd > max_fd) max_fd = client_fd;
			FD_SET(client_fd, &all_fds);
			MESSAGE_PRINT(client_fd, acpt_text);
		}
	}

	return 0;
}
