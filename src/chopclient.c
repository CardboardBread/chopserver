#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#include "chopconn.h"
#include "chopconst.h"
#include "chopdata.h"
#include "chopdebug.h"
#include "choppacket.h"

#define BUFSIZE 255

#ifndef PORT
#define PORT 50001
#endif

#ifndef ADDRESS
#define ADDRESS "127.0.0.1"
#endif

int sigint_received;

struct client *server_connection;

void sigint_handler(int code);

void sigint_handler(int code) {
	DEBUG_PRINT("received SIGINT, setting flag");
	sigint_received = 1;
}

int main(void) {
	// Reset SIGINT received flag.
	sigint_received = 0;

	// mark debug statements as clientside
	header_type = 1;

	// setup SIGINT handler
	struct sigaction act1;
	act1.sa_handler = sigint_handler;
	sigemptyset(&act1.sa_mask);
	act1.sa_flags = SA_RESTART; // ensures interrupted calls dont error out
	if (sigaction(SIGINT, &act1, NULL) < 0) {
		DEBUG_PRINT("sigaction: error");
		exit(1);
	}
	DEBUG_PRINT("sigint_handler attached");

	// connect to locally hosted server
	if (establish_server_connection(ADDRESS, PORT, &server_connection, BUFSIZE) < 0) {
		DEBUG_PRINT("failed connection");
		exit(1);
	}

	// setup fd set for selecting
	int max_fd = server_connection->socket_fd;
	fd_set all_fds, listen_fds;
	FD_ZERO(&all_fds);
	FD_SET(server_connection->socket_fd, &all_fds);
	FD_SET(STDIN_FILENO, &all_fds);

	int run = 1;
	while (run) {
		printf("\n");

		// closing connections and freeing memory before the process ends
		if (sigint_received) {
			DEBUG_PRINT("caught SIGINT, exiting");
			remove_client_address(0, &server_connection);
			exit(1);
		}

		// selecting
		listen_fds = all_fds;
		int nready = select(max_fd + 1, &listen_fds, NULL, NULL, NULL);
		if (nready < 0) {
			DEBUG_PRINT("select");
			exit(1);
		}

		// reading from server
		if (FD_ISSET(server_connection->socket_fd, &listen_fds)) {
			if (process_request(server_connection) < 0) {
				exit(1); // TODO: remove once failing a packet isn't really bad
			}

			// if escape
			if (is_client_status(server_connection, CANCEL)) {
				//exit(1);
				FD_CLR(server_connection->socket_fd, &all_fds);
				run = 0;
				continue;
			}
		}

		// reading from stdin, parsing and sending to server
		int num_read;
		char buffer[127];
		if (FD_ISSET(STDIN_FILENO, &listen_fds)) {
			num_read = read(STDIN_FILENO, buffer, 126);
			if (num_read == 0) break;
			buffer[num_read] ='\0';

			if (remove_newline(buffer, num_read) < 0) {
				DEBUG_PRINT("buffer overflow");
				exit(1);
			}

			// setting values of header
			if (strcmp(buffer, "exit") == 0) {
				// exit
				if (write_dataless(server_connection, 0, ESCAPE, 0, 0) < 0) {
					DEBUG_PRINT("failed packet write");
					exit(1);
				}

			} else if (strcmp(buffer, "ping") == 0) {
				// regular ping
				if (write_dataless(server_connection, 0, ENQUIRY, ENQUIRY_NORMAL, 0) < 0) {
					DEBUG_PRINT("failed packet write");
					exit(1);
				}

			} else if (strcmp(buffer, "pingret") == 0) {
				// returning ping
				if (write_dataless(server_connection, 0, ENQUIRY, ENQUIRY_RETURN, 0) < 0) {
					DEBUG_PRINT("failed packet write");
					exit(1);
				}

			} else if (strcmp(buffer, "pingtime") == 0) {
				// sending time ping
				if (write_wordpack(server_connection, 0, ENQUIRY, ENQUIRY_TIME, sizeof(time_t), time(NULL)) < 0) {
					DEBUG_PRINT("failed packet write");
					exit(1);
				}

			} else if (strcmp(buffer, "pingtimeret") == 0) {
				// requesting time ping
				if (write_dataless(server_connection, 0, ENQUIRY, ENQUIRY_RTIME, 0) < 0) {
					DEBUG_PRINT("failed packet write");
					exit(1);
				}

			} else if (strcmp(buffer, "sleep") == 0) {
				// sleep request
				if (write_dataless(server_connection, 0, IDLE, 0, 0) < 0) {
					DEBUG_PRINT("failed packet write");
					exit(1);
				}

			} else if (strcmp(buffer, "wake") == 0) {
				// wake request
				if (write_dataless(server_connection, 0, WAKEUP, 0, 0) < 0) {
					DEBUG_PRINT("failed packet write");
					exit(1);
				}

			} else {
				// send user input
				if (write_datapack(server_connection, 0, START_TEXT, 1, num_read, buffer, num_read) < 0) {
					DEBUG_PRINT("failed sending user input");
					exit(1);
				}
				continue;
			}
		}
	}

	return 0;
}
