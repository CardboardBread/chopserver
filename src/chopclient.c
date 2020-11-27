#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#include "chopconn.h"
#include "chopdata.h"
#include "chopdebug.h"
#include "choppacket.h"
#include "chopsend.h"

#define BUF_SIZE 255

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

	// mark debug statements as clientside
	INIT_CLIENT_PRINT;

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
	if (establish_server_connection(ADDRESS, PORT, &server_connection, BUF_SIZE) < 0) {
		DEBUG_PRINT("failed connection");
		exit(1);
	}

	// setup fd set for selecting
	int max_fd = server_connection->socket_fd;
	fd_set all_fds, listen_fds;
	FD_ZERO(&all_fds);
	FD_SET(server_connection->socket_fd, &all_fds);
	FD_SET(STDIN_FILENO, &all_fds);

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
			remove_client_address(&server_connection);
			exit(1);
		}

		// selecting
		listen_fds = all_fds;
		int nready = select(max_fd + 1, &listen_fds, NULL, NULL, &timeout);
		if (nready < 0) {
			DEBUG_PRINT("select");
			exit(1);
		} if (nready == 0) {
			DEBUG_PRINT("timeout reached, resetting");
			if (send_enqu(server_connection, ENQUIRY_TIME) < 0) {
				DEBUG_PRINT("failed time update to server");
			}
		}

		// reading from server
		if (FD_ISSET(server_connection->socket_fd, &listen_fds)) {
			if (process_request(server_connection) < 0) {
				exit(1); // TODO: remove once failing a packet isn't really bad
			}

			// if escape
			if (is_client_status(server_connection, CANCEL)) {
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
				if (send_exit(server_connection) < 0) {
					DEBUG_PRINT("failed packet write");
					exit(1);
				}

			} else if (strcmp(buffer, "ping") == 0) {
				// regular ping
				if (send_enqu(server_connection, ENQUIRY_NORMAL) < 0) {
					DEBUG_PRINT("failed packet write");
					exit(1);
				}

			} else if (strcmp(buffer, "pingret") == 0) {
				// returning ping
				if (send_enqu(server_connection, ENQUIRY_RETURN) < 0) {
					DEBUG_PRINT("failed packet write");
					exit(1);
				}

			} else if (strcmp(buffer, "pingtime") == 0) {
				// sending time ping
				if (send_enqu(server_connection, ENQUIRY_TIME) < 0) {
					DEBUG_PRINT("failed packet write");
					exit(1);
				}

			} else if (strcmp(buffer, "pingtimeret") == 0) {
				// requesting time ping
				if (send_enqu(server_connection, ENQUIRY_RTIME) < 0) {
					DEBUG_PRINT("failed packet write");
					exit(1);
				}

			} else if (strcmp(buffer, "sleep") == 0) {
				// sleep request
				if (send_idle(server_connection) < 0) {
					DEBUG_PRINT("failed packet write");
					exit(1);
				}

			} else if (strcmp(buffer, "wake") == 0) {
				// wake request
				if (send_wake(server_connection) < 0) {
					DEBUG_PRINT("failed packet write");
					exit(1);
				}

			} else {
				// send user input
				if (send_text(server_connection, buffer, num_read) < 0) {
					DEBUG_PRINT("failed sending user input");
					exit(1);
				}
				continue;
			}
		}
	}

	return 0;
}
