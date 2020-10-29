#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>     /* inet_ntoa */
#include <netdb.h>         /* gethostname */

#include "chopconst.h"
#include "chopdebug.h"
#include "chopsocket.h"

int init_server_addr(struct sockaddr_in *addr, const int port) {
	// check valid arguments
	if (addr == NULL || port < 0) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	// initialize struct fields
	addr->sin_family = PF_INET; // Allow sockets across machines.
	addr->sin_port = htons(port); // The port the process will listen on.
	memset(&(addr->sin_zero), 0, 8); // Clear this field; sin_zero is used for padding for the struct.
	addr->sin_addr.s_addr = INADDR_ANY; // Listen on all network interfaces.

	return 0;
}

int setup_server_socket(struct sockaddr_in *self, int port, int num_queue) {
	// check valid arguments
	if (self == NULL || num_queue < 1 || port < 0) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	int init_serv = init_server_addr(self, port);
	if (init_serv < 0) {
		DEBUG_PRINT("failed addr init");
		return init_serv;
	}

	int soc = socket(PF_INET, SOCK_STREAM, 0);
	if (soc < 0) {
		DEBUG_PRINT("socket fail");
		return -errno;
	}

	// Make sure we can reuse the port immediately after the
	// server terminates. Avoids the "address in use" error
	int on = 1;
	int status = setsockopt(soc, SOL_SOCKET, SO_REUSEADDR, (const char *) &on, sizeof(on));
	if (status < 0) {
		DEBUG_PRINT("setsockopt fail");
		return -errno;
	}

	// Associate the process with the address and a port
	if (bind(soc, (struct sockaddr *) self, sizeof(*self)) < 0) {
		DEBUG_PRINT("bind fail"); // port might be in use
		return -errno;
	}

	// Set up a queue in the kernel to hold pending connections.
	if (listen(soc, num_queue) < 0) {
		DEBUG_PRINT("listen fail");
		return -errno;
	}

	// return server socket
	return soc;
}

int accept_connection(int listenfd, struct sockaddr_in *peer) {
	// check valid arguments
	if (listenfd < MIN_FD) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	unsigned int peer_len = sizeof(*peer);
	peer->sin_family = PF_INET;

	int client_socket = accept(listenfd, (struct sockaddr *) peer, &peer_len);
	if (client_socket < MIN_FD) {
		DEBUG_PRINT("accept fail");
		return -errno;
	}

	return client_socket;
}

int refuse_connection(int listenfd) {
	if (listenfd < MIN_FD) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	struct sockaddr_in peer;
	unsigned int peer_len = sizeof(peer);
	peer.sin_family = PF_INET;

	close(accept(listenfd, (struct sockaddr *) &peer, &peer_len)); // TODO: return a refusal message

	return 0;
}

int connect_to_server(struct sockaddr_in *addr, const char *hostname, int port) {
	// check valid arguments
	if (addr == NULL || hostname == NULL || port < 0) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	int soc = socket(PF_INET, SOCK_STREAM, 0);
	if (soc < 0) {
		DEBUG_PRINT("socket fail");
		return -errno;
	}

	addr->sin_family = PF_INET; // Allow sockets across machines.
	addr->sin_port = htons(port); // The port the server will be listening on.
	memset(&(addr->sin_zero), 0, 8); // Clear this field; sin_zero is used for padding for the struct.

	// Lookup host IP address.
	struct hostent *hp = gethostbyname(hostname);
	if (hp == NULL) {
		DEBUG_PRINT("gethostbyname, unknown host %s", hostname);
		return -h_errno;
	}

	addr->sin_addr = *((struct in_addr *) hp->h_addr);

	// Request connection to server.
	if (connect(soc, (struct sockaddr *) addr, sizeof(*addr)) == -1) {
		DEBUG_PRINT("connect fail");
		return -errno;
	}

	return soc;
}

int open_server(struct server *target) {
	// check valid arguments
	if (target == NULL) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	// initialize server address
	struct sockaddr_in self;
	self.sin_family = PF_INET;
	self.sin_port = htons(target->server_port);
	memset(&(self.sin_zero), 0, 8);
	self.sin_addr.s_addr = INADDR_ANY;

	// create socket for connecting between computers
	int sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		DEBUG_PRINT("socket");
		return -errno;
	}

	// mark the socket as reusable, ensures the address can be used after the program exits
	int option = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *) &option, sizeof(option)) < 0) {
		DEBUG_PRINT("setsockopt");
		return -errno;
	}

	// associate the process with the address and a port
	if (bind(sock, (struct sockaddr *) &self, sizeof(self)) < 0) {
		DEBUG_PRINT("bind");
		return -errno;
	}

	// set up a queue in the kernel to hold pending connections
	if (listen(sock, target->connect_queue) < 0) {
		DEBUG_PRINT("listen");
		return -errno;
	}

	// copy working data to container
	target->address = self;
	target->server_fd = sock;
	return sock;
}

int accept_client(struct client *target, int listen_fd) {
	// check valid arguments
	if (target == NULL || listen_fd < MIN_FD) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	// initialize client address
	struct sockaddr_in peer;
	socklen_t peer_len = sizeof(peer);
	peer.sin_family = PF_INET;

	// accept incoming connection
	int client_fd = accept(listen_fd, (struct sockaddr *) &peer, &peer_len);
	if (client_fd < MIN_FD) {
		DEBUG_PRINT("accept");
		return -errno;
	}

	// copy working data to container
	target->address = peer;
	target->server_fd = listen_fd;
	target->socket_fd = client_fd;
	return client_fd;
}

int client_connect(struct client *target, const char *hostname, int port) {
	if (hostname == NULL || port < 1) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	// assemble client socket for connections between computers
	int sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		DEBUG_PRINT("socket");
		return -errno;
	}

	// initialize server address
	struct sockaddr_in upstream;
	upstream.sin_family = PF_INET;
	upstream.sin_port = htons(port);
	memset(&(upstream.sin_zero), 0, 8);

	// lookup host IP address
	struct hostent *hp = gethostbyname(hostname);
	if (hp == NULL) {
		DEBUG_PRINT("gethostbyname");
		return -h_errno;
	}

	// copy discovered ip address to server address location
	struct in_addr *host_addr = (struct in_addr *) hp->h_addr;
	upstream.sin_addr = *host_addr;

	// request connection to server
	if (connect(sock, (struct sockaddr *) &upstream, sizeof(upstream)) < 0) {
		DEBUG_PRINT("connect");
		return -errno;
	}

	// copy working data to container
	target->address = upstream;
	target->socket_fd = sock;
	target->server_fd = -1;
	return sock;
}