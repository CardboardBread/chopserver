#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "socket.h"
#include "chophelper.h"

#ifndef PORT
  #define PORT 50001
#endif

int sigint_received;

Client server_connection;

void sigint_handler(int code);

/*
 * SIGINT handler:
 * We are just raising the sigint_received flag here. Our program will
 * periodically check to see if this flag has been raised, and any necessary
 * work will be done in main() and/or the helper functions.
 */
void sigint_handler(int code) {
  debug_print("sigint_handler: received SIGINT, setting flag");
  sigint_received = 1;
}

int main(void) {
  // connect to locally hosted server
  int sock_fd = establish_server_connection(PORT, "127.0.0.1", &server_connection);

  // setup fd set for selecting
  int max_fd = sock_fd;
  fd_set all_fds, listen_fds;
  FD_ZERO(&all_fds);
  FD_SET(sock_fd, &all_fds);
  FD_SET(STDIN_FILENO, &all_fds);

  int run = 1;
  while(run) {

    // closing connections and freeing memory before the process ends
    if (sigint_received) {
      debug_print("main: caught SIGINT, exiting");
      exit(1);
    }

    // selecting
    listen_fds = all_fds;
    int nready = select(max_fd + 1, &listen_fds, NULL, NULL, NULL);
    if (nready < 0) {
      if (errno == EINTR) {
        continue;
      } else {
        debug_print("select: non-interrupt error");
        exit(1);
      }
    }

    // reading from server
    if (FD_ISSET(server_connection.socket_fd, &listen_fds)) {
      read_header(&server_connection);

      // if a client requested a cancel
      if (server_connection.inc_flag < 0 && server_connection.out_flag < 0) {
        FD_CLR(server_connection.socket_fd, &all_fds);
        run = 0;
      }
    }

    // reading from stdin, parsing and sending to server
    int num_read;
    int num_write;
    char buffer[265];
    if (FD_ISSET(STDIN_FILENO, &listen_fds)) {
      num_read = read(STDIN_FILENO, buffer, 265);
      if (num_read == 0) break;
      buffer[num_read] = '\0';

      num_write = write(server_connection.socket_fd, buffer, num_read);
      
    }
  }

  return 0;
}
