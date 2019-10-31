#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>     /* inet_ntoa */
#include <netdb.h>         /* gethostname */
#include <sys/socket.h>

#include "chopdebug.h"
#include "chopsocket.h"

/*
 * Initialize a server address associated with the given port.
 */
int init_server_addr(struct sockaddr_in **addr, const int port) {
  // check valid arguments
  if (addr == NULL || port < 0) {
    DEBUG_PRINT("invalid arguments");
    return 1;
  }

  // allocate struct memory, catch ENOMEM
  *addr = malloc(sizeof(struct sockaddr_in));
  if (addr == NULL) {
    DEBUG_PRINT("malloc fail");
    return 1;
  }

  struct sockaddr_in *serv = *addr;

  // initialize struct fields
  serv->sin_family = PF_INET; // Allow sockets across machines.
  serv->sin_port = htons(port); // The port the process will listen on.
  memset(&(serv->sin_zero), 0, 8); // Clear this field; sin_zero is used for padding for the struct.
  serv->sin_addr.s_addr = INADDR_ANY; // Listen on all network interfaces.

  return 0;
}

/*
 * Create and setup a socket for a server to listen on.
 */
int setup_server_socket(struct sockaddr_in *self, int *socket, const int num_queue) {
  // check valid arguments
  if (self == NULL || socket == NULL || num_queue < 1) {
    DEBUG_PRINT("invalid arguments");
    return 1;
  }

  int soc = socket(PF_INET, SOCK_STREAM, 0);
  if (soc < 0) {
    DEBUG_PRINT("socket fail");
    return 1;
  }

  // Make sure we can reuse the port immediately after the
  // server terminates. Avoids the "address in use" error
  int on = 1;
  int status = setsockopt(soc, SOL_SOCKET, SO_REUSEADDR, (const char *) &on, sizeof(on));
  if (status < 0) {
    DEBUG_PRINT("setsockopt fail");
    return 1;
  }

  // Associate the process with the address and a port
  if (bind(soc, (struct sockaddr *) self, sizeof(*self)) < 0) {
    // bind failed; could be because port is in use.
    DEBUG_PRINT("bind fail");
    return 1;
  }

  // Set up a queue in the kernel to hold pending connections.
  if (listen(soc, num_queue) < 0) {
    // listen failed
    DEBUG_PRINT("listen fail");
    return 1;
  }

  // store new socket in given address
  *socket = soc;

  return 0;
}

/*
 * Wait for and accept a new connection.
 * Return -1 if the accept call failed.
 */
int accept_connection(const int listenfd, int *newfd) {
  // check valid arguments
  if (listenfd < MIN_FD || newfd == NULL) {
    DEBUG_PRINT("invalid arguments");
    return 1;
  }

  struct sockaddr_in peer;
  unsigned int peer_len = sizeof(peer);
  peer.sin_family = PF_INET;

  int client_socket = accept(listenfd, (struct sockaddr *) &peer, &peer_len);
  if (client_socket < MIN_FD) {
    DEBUG_PRINT("accept fail");
    return 1;
  }

  *newfd = client_socket;

  return 0;
}

/*
 * Create a socket and connect to the server indicated by the port and hostname
 */
int connect_to_server(const char *hostname, const int port, int *socket) {
  // check valid arguments
  if (hostname == NULL || port < 0 || socket == NULL) {
    return 1;
  }

  int soc = socket(PF_INET, SOCK_STREAM, 0);
  if (soc < 0) {
    DEBUG_PRINT("socket fail");
    return 1;
  }
  struct sockaddr_in addr;


  addr.sin_family = PF_INET; // Allow sockets across machines.
  addr.sin_port = htons(port); // The port the server will be listening on.
  memset(&(addr.sin_zero), 0, 8); // Clear this field; sin_zero is used for padding for the struct.

  // Lookup host IP address.
  struct hostent *hp = gethostbyname(hostname);
  if (hp == NULL) {
    DEBUG_PRINT("gethostbyname, unknown host %s", hostname);
    return 1;
  }

  addr.sin_addr = *((struct in_addr *) hp->h_addr);

  // Request connection to server.
  if (connect(soc, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
    DEBUG_PRINT("connect fail");
    return 1;
  }

  *socket = soc;

  return 0;
}
