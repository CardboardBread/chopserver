#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>     /* inet_ntoa */
#include <netdb.h>         /* gethostname */
#include <sys/socket.h>

#include "chopconst.h"
#include "chopdebug.h"

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

int setup_server_socket(struct sockaddr_in *self, const int port, const int num_queue) {
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

int accept_connection(const int listenfd, struct sockaddr_in *peer) {
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

int refuse_connection(const int listenfd) {
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

int connect_to_server(struct sockaddr_in *addr, const char *hostname, const int port) {
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
