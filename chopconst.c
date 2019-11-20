#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "chopconst.h"
#include "chopdebug.h"

/*
 * Structure Management Functions
 */

int init_buffer_struct(struct buffer **target, const int size) {
	// check valid argument
	if (target == NULL || size < 0) {
		return 1;
	}

	// allocate structure
	struct buffer *new = malloc(sizeof(struct buffer));
	if (new == NULL) {
		DEBUG_PRINT("malloc, structure");
		return 1;
	}

	// allocate buffer memory
	char *mem = malloc(sizeof(char) * size);
	if (mem == NULL) {
		DEBUG_PRINT("malloc, memory");
		free(new);
		return 1;
	}

	// initialize structure fields
	new->buf = mem;
	new->inbuf = 0;
	new->bufsize = size;
	new->next = NULL;

	// set given pointer to new struct
	*target = new;

	return 0;
}

int init_packet_struct(struct packet **target) {
	// check valid argument
	if (target == NULL) {
		return 1;
	}

	// allocate structure
	struct packet *new = malloc(sizeof(struct packet));
	if (new == NULL) {
		DEBUG_PRINT("malloc");
		return 1;
	}

	// initialize structure fields
	new->head = -1;
	new->status = -1;
	new->control1 = -1;
	new->control2 = -1;
	new->data = NULL;
	new->datalen = 0;

	// set given pointer to new struct
	*target = new;

	return 0;
}

int init_server_struct(struct server **target, const int max_conns) {
	// check valid argument
	if (target == NULL || max_conns < 1) {
		return 1;
	}

	// allocate structure
	struct server *new = malloc(sizeof(struct server));
	if (new == NULL) {
		DEBUG_PRINT("malloc, structure");
		return 1;
	}

	// allocate server client array
	struct client **mem = malloc(sizeof(struct client *) * max_conns);
	if (mem == NULL && max_conns > 0) { // in case zero clients allowed
		DEBUG_PRINT("malloc, memory");
		free(new);
		return 1;
	}

	// set client array to empty
	for (int i = 0; i < max_conns; i++) {
		mem[i] = NULL;
	}

	// initialize structure fields
	new->server_fd = -1;
	new->clients = mem;
	new->max_connections = max_conns;
	new->cur_connections = 0;

	// set given pointer to new struct
	*target = new;

	return 0;
}

int init_client_struct(struct client **target, const int size) {
	// check valid argument
	if (target == NULL) {
		return 1;
	}

	// allocate structure
	struct client *new = malloc(sizeof(struct client));
	if (new == NULL) {
		DEBUG_PRINT("malloc");
		return 1;
	}

	// initialize structure fields
	new->socket_fd = -1;
	new->server_fd = -1;
	new->inc_flag = -1;
	new->out_flag = -1;
	new->window = size;

	// set given pointer to new struct
	*target = new;

	return 0;
}

int destroy_buffer_struct(struct buffer **target) {
	// check valid argument
	if (target == NULL) {
		return 1;
	}

	// struct already doesn't exist
	if (*target == NULL) {
		return 0;
	}

	// direct reference to structure
	struct buffer *old = *target;

	// deallocate data section
	free(old->buf);

	// deallocate structure
	free(old);

	// dereference holder
	*target = NULL;

	return 0;
}

int destroy_packet_struct(struct packet **target) {
	// check valid argument
	if (target == NULL) {
		return 1;
	}

	// struct already doesn't exist
	if (*target == NULL) {
		return 0;
	}

	// direct reference to structure
	struct packet *old = *target;

	// deallocate all nodes in data section
	struct buffer *cur;
	struct buffer *next;
	for (cur = old->data; cur != NULL; cur = next) {
		next = cur->next;
		free(cur->buf);
		free(cur);
		cur = NULL;
	}

	// deallocate structure
	free(old);

	// dereference holder
	*target = NULL;

	return 0;
}

int destroy_server_struct(struct server **target) {
	// check valid argument
	if (target == NULL) {
		return 1;
	}

	// struct already doesn't exist
	if (*target == NULL) {
		return 0;
	}

	// direct reference to structure
	struct server *old = *target;

	// close open channels
	if (old->server_fd > MIN_FD) {
		close(old->server_fd);
	}

	// deallocate remaining clients
	for (int i = 0; i < old->max_connections; i++) {
		if (old->clients + i != NULL) {
			destroy_client_struct(
					old->clients + i); // TODO: make sure the array of client pointers can be iterated along as such
			//destroy_client_struct(&(old->clients[i])); // alternative option
		}
	}

	// deallocate clients section
	free(old->clients);

	// deallocate structure
	free(old);

	// dereference holder
	*target = NULL;

	return 0;
}

int destroy_client_struct(struct client **target) {
	// check valid argument
	if (target == NULL) {
		return 1;
	}

	// struct already doesn't exist
	if (*target == NULL) {
		return 0;
	}

	// direct reference to structure
	struct client *old = *target;

	// close open channels
	if (old->socket_fd > MIN_FD) {
		close(old->socket_fd);
	}

	// deallocate structure
	free(old);

	// dereference holder
	*target = NULL;

	return 0;
}
