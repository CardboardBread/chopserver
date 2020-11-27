#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "chopconst.h"
#include "chopdebug.h"

/*
 * Variables
 */

const struct status status_str_arr[MAX_STATUS] = {
		{NULL_BYTE, "NULL_BYTE"},
		{START_HEADER, "START_HEADER"},
		{START_TEXT, "START_TEXT"},
		{END_TEXT, "END_TEXT"},
		{END_TRANSMISSION, "END_TRANSMISSION"},
		{ENQUIRY, "ENQUIRY"},
		{ACKNOWLEDGE, "ACKNOWLEDGE"},
		{WAKEUP, "WAKEUP"},
		{8, "8"},
		{9, "9"},
		{10, "10"},
		{11, "11"},
		{12, "12"},
		{13, "13"},
		{SHIFT_OUT, "SHIFT_OUT"},
		{SHIFT_IN, "SHIFT_IN"},
		{START_DATA, "START_DATA"},
		{CONTROL_ONE, "CONTROL_ONE"},
		{CONTROL_TWO, "CONTROL_TWO"},
		{CONTROL_THREE, "CONTROL_THREE"},
		{CONTROL_FOUR, "CONTROL_FOUR"},
		{NEG_ACKNOWLEDGE, "NEG_ACKNOWLEDGE"},
		{IDLE, "IDLE"},
		{END_TRANSMISSION_BLOCK, "END_TRANSMISSION_BLOCK"},
		{CANCEL, "CANCEL"},
		{END_OF_MEDIUM, "END_OF_MEDIUM"},
		{SUBSTITUTE, "SUBSTITUTE"},
		{ESCAPE, "ESCAPE"},
		{FILE_SEPARATOR, "FILE_SEPARATOR"},
		{GROUP_SEPARATOR, "GROUP_SEPARATOR"},
		{RECORD_SEPARATOR, "RECORD_SEPARATOR"},
		{UNIT_SEPARATOR, "UNIT_SEPARATOR"}
};

const struct status enquiry_str_arr[MAX_ENQUIRY] = {
		{ENQUIRY_NORMAL, "ENQUIRY_NORMAL"},
		{ENQUIRY_RETURN, "ENQUIRY_RETURN"},
		{ENQUIRY_TIME, "ENQUIRY_TIME"},
		{ENQUIRY_RTIME, "ENQUIRY_RTIME"}
};

/*
 * Structure Management Functions
 */

int init_buffer(struct buffer **target, size_t size) {
	// check valid argument
	if (target == NULL || size < 1) {
		return -EINVAL;
	}

	// allocate structure
	struct buffer *init = (struct buffer *) malloc(sizeof(struct buffer));
	if (init == NULL) {
		DEBUG_PRINT("malloc, structure");
		return -errno;
	}

	// allocate buffer memory
	char *mem = (char *) calloc(size, sizeof(char));
	if (mem == NULL) {
		DEBUG_PRINT("calloc, memory");
		free(init);
		return -errno;
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

int init_packet(struct packet **target) {
	// check valid argument
	if (target == NULL) {
		return -EINVAL;
	}

	// allocate structure
	struct packet *init = (struct packet *) malloc(sizeof(struct packet));
	if (init == NULL) {
		DEBUG_PRINT("malloc");
		return -errno;
	}

	// initialize structure fields
	init->header.head = 0;
	init->header.status = 0;
	init->header.control1 = 0;
	init->header.control2 = 0;
	init->data = NULL;
	init->datalen = 0;

	// set given pointer to new struct
	*target = init;
	return 0;
}

int init_server(struct server **target, int port, size_t max_conns, size_t queue_len, size_t window) {
	// check valid argument
	if (target == NULL || max_conns < 1 || queue_len < 1 || window < 1) {
		return -EINVAL;
	}

	// allocate structure
	struct server *init = (struct server *) malloc(sizeof(struct server));
	if (init == NULL) {
		DEBUG_PRINT("malloc, structure");
		return -errno;
	}

	// allocate server client array
	struct client **mem = (struct client **) calloc(max_conns, sizeof(struct client *));
	if (mem == NULL) {
		DEBUG_PRINT("malloc, memory");
		free(init);
		return -errno;
	}

	// set client array to empty
	for (int i = 0; i < max_conns; i++) {
		mem[i] = NULL;
	}

	// initialize structure fields
	init->server_fd = -1;
	init->server_port = port;
	init->clients = mem;
	init->max_connections = max_conns;
	init->cur_connections = 0;
	init->connect_queue = queue_len;
	init->window = window;

	// set given pointer to new struct
	*target = init;
	return 0;
}

int init_client(struct client **target, size_t window) {
	// check valid argument
	if (target == NULL || window < 1) {
		return -EINVAL;
	}

	// allocate structure
	struct client *init = (struct client *) malloc(sizeof(struct client));
	if (init == NULL) {
		DEBUG_PRINT("malloc");
		return -errno;
	}

	// initialize structure fields
	init->socket_fd = -1;
	init->server_fd = -1;
	init->inc_flag = 0;
	init->out_flag = 0;
	init->window = window;

	// set given pointer to new struct
	*target = init;
	return 0;
}

int destroy_buffer(struct buffer **target) {
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

int empty_packet(struct packet *target) {
	// check valid argument
	if (target == NULL) {
		return -EINVAL;
	}

	// loop through data segments, dealloc each one
	struct buffer *cur;
	struct buffer *next;
	for (cur = target->data; cur != NULL; cur = next) {
		next = cur->next;
		destroy_buffer(&cur);
	}

	return 0;
}

int destroy_packet(struct packet **target) {
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
	empty_packet(old);

	// deallocate structure
	free(old);

	// dereference holder
	*target = NULL;
	return 0;
}

int destroy_server(struct server **target) {
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
		if (close(old->server_fd) < 0) {
			DEBUG_PRINT("close");
			return -errno;
		}
	}

	// deallocate remaining clients
	for (size_t i = 0; i < old->max_connections; i++) {
		if (old->clients + i != NULL) destroy_client(old->clients + i);
	}

	// deallocate clients section
	free(old->clients);

	// deallocate structure
	free(old);

	// dereference holder
	*target = NULL;
	return 0;
}

int destroy_client(struct client **target) {
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
		if (close(old->socket_fd) < 0) {
			DEBUG_PRINT("close");
			return -errno;
		}
	}

	// deallocate structure
	free(old);

	// dereference holder
	*target = NULL;
	return 0;
}

int realloc_buffer(struct buffer *target, size_t size) {
	INVAL_CHECK(target == NULL || size < 1);

	// TODO: check for reallocation in-place

	// no reallocation if the buffer doesn't change
	if (size == target->bufsize) {
		return 0;
	}

	// allocate new buffer
	char *new_mem = (char *) calloc(size, sizeof(char));
	if (new_mem == NULL) {
		DEBUG_PRINT("calloc, memory");
		return -errno;
	}

	// free old buffer
	char *old_mem = target->buf;
	free(old_mem);

	// copy in new buffer
	target->buf = new_mem;
	target->bufsize = size;
	target->inbuf = 0;

	return 0;
}