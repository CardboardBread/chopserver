#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "chopconst.h"
#include "chopdebug.h"

/*
 * Structure Management Functions
 */

int init_buffer_struct(struct buffer **target, const int size) {
	// check valid argument
	if (target == NULL || size < 0) {
		return -EINVAL;
	}

	// allocate structure
	struct buffer *init = (struct buffer *) malloc(sizeof(struct buffer));
	if (init == NULL) {
		DEBUG_PRINT("malloc, structure");
		return -ENOMEM;
	}

	// allocate buffer memory
	char *mem = (char *) malloc(sizeof(char) * size);
	if (mem == NULL) {
		DEBUG_PRINT("malloc, memory");
		free(init);
		return -ENOMEM;
	}

	// initialize structure fields
	init->buf = mem;
	init->inbuf = 0;
	init->bufsize = size;
	init->next = NULL;

	// set given pointer to new struct
	*target = init;
	return 0;
}

int init_packet_struct(struct packet **target) {
	// check valid argument
	if (target == NULL) {
		return -EINVAL;
	}

	// allocate structure
	struct packet *init = (struct packet *) malloc(sizeof(struct packet));
	if (init == NULL) {
		DEBUG_PRINT("malloc");
		return -ENOMEM;
	}

	// initialize structure fields
	init->head = -1;
	init->status = -1;
	init->control1 = -1;
	init->control2 = -1;
	init->data = NULL;
	init->datalen = 0;

	// set given pointer to new struct
	*target = init;
	return 0;
}

int init_server_struct(struct server **target, const int max_conns) {
	// check valid argument
	if (target == NULL || max_conns < 1) {
		return -EINVAL;
	}

	// allocate structure
	struct server *init = (struct server *) malloc(sizeof(struct server));
	if (init == NULL) {
		DEBUG_PRINT("malloc, structure");
		return -ENOMEM;
	}

	// allocate server client array
	struct client **mem = (struct client **) malloc(sizeof(struct client *) * max_conns);
	if (mem == NULL && max_conns > 0) { // in case zero clients allowed
		DEBUG_PRINT("malloc, memory");
		free(init);
		return -ENOMEM;
	}

	// set client array to empty
	for (int i = 0; i < max_conns; i++) {
		mem[i] = NULL;
	}

	// initialize structure fields
	init->server_fd = -1;
	init->clients = mem;
	init->max_connections = max_conns;
	init->cur_connections = 0;

	// set given pointer to new struct
	*target = init;
	return 0;
}

int init_client_struct(struct client **target, const int size) {
	// check valid argument
	if (target == NULL || size < 1) {
		return -EINVAL;
	}

	// allocate structure
	struct client *init = (struct client *) malloc(sizeof(struct client));
	if (init == NULL) {
		DEBUG_PRINT("malloc");
		return -ENOMEM;
	}

	// initialize structure fields
	init->socket_fd = -1;
	init->server_fd = -1;
	init->inc_flag = -1;
	init->out_flag = -1;
	init->window = size;

	// set given pointer to new struct
	*target = init;
	return 0;
}

int destroy_buffer_struct(struct buffer **target) {
	// check valid argument
	if (target == NULL) {
		return -EINVAL;
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
		return -EINVAL;
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
		return -EINVAL;
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
		if (old->clients + i != NULL) destroy_client_struct(old->clients + i);
		// TODO: make sure the array of client pointers can be iterated along as such
		//destroy_client_struct(&(old->clients[i])); // alternative option
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
		return -EINVAL;
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
