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

#define BUFSIZE 255
#define CONNECTION_QUEUE 5
#define MAX_CONNECTIONS 20

const char server_header[] = "[SERVER] %s\n";
const char client_header[] = "[CLIENT %d] %s\n";

const char server_shutdown[] = "[SERVER] Shutting down.\n";

const char client_closed[] = "[CLIENT %d] Connection closed.\n";
const char connection_accept[] = "[CLIENT %d] Connected.\n";

int sigint_received;

struct server *host;

void sigint_handler(int code);

void sigint_handler(int code) {
    DEBUG_PRINT("received SIGINT, setting flag");
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
      DEBUG_PRINT("sigaction: error");
      exit(1);
  }
  DEBUG_PRINT("sigint_handler attached");

  // setup server address
  struct sockaddr_in *self;
  if (init_server_addr(&self, PORT) > 0) {
    DEBUG_PRINT("failed addr init");
    exit(1);
  }
  DEBUG_PRINT("addr init successful");

  // setup server socket
  int sock_fd;
  if (setup_server_socket(self, &sock_fd, CONNECTION_QUEUE) > 0) {
    DEBUG_PRINT("failed server socket init");
    exit(1);
  }
  DEBUG_PRINT("server listening on all interfaces");

  if (init_server_struct(&host, MAX_CONNECTIONS) > 0) {
    DEBUG_PRINT("failed server struct init")
    exit(1);
  }
  DEBUG_PRINT("server open on $d slots", MAX_CONNECTIONS);

  // setup fd set for selecting
  int max_fd = sock_fd;
  fd_set all_fds, listen_fds;
  FD_ZERO(&all_fds);
  FD_SET(sock_fd, &all_fds);

  int run = 1;
  while (run) {

    // closing connections and freeing memory before the process ends
    if (sigint_received) {
      DEBUG_PRINT("caught SIGINT, exiting");
      exit(1);
    }

    // selecting
    listen_fds = all_fds;
    int nready = select(max_fd + 1, &listen_fds, NULL, NULL, NULL);
    if (nready < 0) {
      if (errno == EINTR) {
        continue;
      } else {
        DEBUG_PRINT("failed select");
        exit(1);
      }
    }

    // check all clients if they can read
    for (int index = 0; index < MAX_CONNECTIONS; index++) {
      struct client *client = host->clients[index];

      // relies on short circuting
      if (client != NULL && FD_ISSET(client->socket_fd, &listen_fds)) {
        if (process_request(client, &all_fds) > 0) {
          DEBUG_PRINT("failed process");
          return 1;
        }

        // if a client requested a cancel
        if (is_client_status(client, CANCEL)) {
          FD_CLR(client->socket_fd, &all_fds);
          printf(client_closed, client->socket_fd);
          remove_client_index(index, host);
        }
      }
    }

    // accept new client
    if (FD_ISSET(sock_fd, &listen_fds)) {
      int client_fd;
      if (accept_new_client(host, &client_fd, BUFSIZE) > 0) {
        DEBUG_PRINT("failed accept");
        continue;
      }

      if (client_fd > max_fd) max_fd = client_fd;
      FD_SET(client_fd, &all_fds);
      printf(connection_accept, client_fd);
    }
  }

  return 0;
}
