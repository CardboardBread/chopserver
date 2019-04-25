#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdarg.h>
#include <errno.h>

#include "socket.h"
#include "chophelper.h"

#ifndef PORT
  #define PORT 50001
#endif

const char server_header[] = "[SERVER] %s\n";
const char client_header[] = "[CLIENT %d] %s\n";

const char server_shutdown[] = "[SERVER] Shutting down.\n";

const char client_closed[] = "[CLIENT %d] Connection closed.\n";
const char connection_accept[] = "[CLIENT %d] Connected.\n";

int sigint_received;

Client *global_clients[MAX_CONNECTIONS];

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
  // Reset SIGINT received flag.
  sigint_received = 0;

  // setup SIGINT handler
  struct sigaction act1;
  act1.sa_handler = sigint_handler;
  sigemptyset(&act1.sa_mask);
  act1.sa_flags = SA_RESTART; // ensures interrupted calls dont error out
  if (sigaction(SIGINT, &act1, NULL) < 0) {
    debug_print("SIGINT handler: error");
    exit(1);
  }

  // setup server address
  struct sockaddr_in *self = init_server_addr(PORT);
  if (self == NULL) {
    debug_print("init_server_addr: error");
    exit(1);
  }

  // setup server socket
  int sock_fd = setup_server_socket(self, CONNECTION_QUEUE);
  if (sock_fd < MIN_FD) {
    debug_print("setup_server_socket: error");
    exit(1);
  }

  // setup fd set for selecting
  int max_fd = sock_fd;
  fd_set all_fds, listen_fds;
  FD_ZERO(&all_fds);
  FD_SET(sock_fd, &all_fds);

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

    // check all clients if they can read
    for (int index = 0; index < MAX_CONNECTIONS; index++) {
      Client *client = global_clients[index];
      if (client != NULL && FD_ISSET(client->socket_fd, &listen_fds)) {
        read_header(client);

        // if a client requested a cancel
        if (client->inc_flag < 0 && client->out_flag < 0) {
          FD_CLR(client->socket_fd, &all_fds);
          remove_client(index, global_clients);
        }
      }
    }

    if (FD_ISSET(sock_fd, &listen_fds)) {
      int client_fd = setup_new_client(sock_fd, global_clients);
      if (client_fd < 0) {
        continue;
      }

      if (client_fd > max_fd) max_fd = client_fd;

      FD_SET(client_fd, &all_fds);
      printf(connection_accept, client_fd);
    }
  }

  return 0;
}
