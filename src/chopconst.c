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
 * Data Structure Management Functions
 */

int create_buffer(struct buffer **target, size_t size) {
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

	// allocate memory and initialize structure fields
	if (init_buffer(init, size) < 0) {
		DEBUG_PRINT("init");
		free(init);
		return -errno;
	}

	// set given pointer to new struct
	*target = init;
	return 0;
}

int init_buffer(struct buffer *target, size_t size) {
	// check valid argument
	if (target == NULL || size < 1) {
		return -EINVAL;
	}

	// allocate buffer memory
	char *mem = (char *) calloc(size, sizeof(char));
	if (mem == NULL) {
		DEBUG_PRINT("calloc, memory");
		return -errno;
	}

	// initialize structure fields
	target->buf = mem;
	target->inbuf = 0;
	target->bufsize = size;
	target->next = NULL;

	return 0;
}

int realloc_buffer(struct buffer *target, size_t size) {
	INVAL_CHECK(target == NULL || size < 1);

	// TODO: check for reallocation in-place

	// no reallocation if the buffer doesn't change
	if (size == target->bufsize) {
		return 0;
	}

	char *old_mem = target->buf;
	struct buffer *old_next = target->next;

	// reinitialize buffer with new memory
	if (init_buffer(target, size) < 0) {
		DEBUG_PRINT("init");
		return -errno;
	}

	// free old buffer, retain next ptr
	free(old_mem);
	target->next = old_next;
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

int create_packet(struct packet **target) {
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

	// reset fields
	if (init_packet(init) < 0) {
		DEBUG_PRINT("init");
		free(init);
		return -errno;
	}

	// set given pointer to new struct
	*target = init;
	return 0;
}

int init_packet(struct packet *target) {
	// check valid argument
	if (target == NULL) {
		return -EINVAL;
	}

	// initialize structure fields
	target->header.head = 0;
	target->header.status = 0;
	target->header.control1 = 0;
	target->header.control2 = 0;
	target->data = NULL;
	target->datalen = 0;

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

	target->datalen = 0;

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

/*
 * Client/Server Manangement Functions
 */

int create_server(struct server **target, int port, size_t max_conns, size_t queue_len, size_t window) {
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

	// initialize structure fields
	if (init_server(init, port, max_conns, queue_len, window) < 0) {
		DEBUG_PRINT("init");
		free(init);
		return -errno;
	}

	// set given pointer to new struct
	*target = init;
	return 0;
}

int init_server(struct server *target, int port, size_t max_conns, size_t queue_len, size_t window) {
	// check valid argument
	if (target == NULL || max_conns < 1 || queue_len < 1 || window < 1) {
		return -EINVAL;
	}

	// allocate client array
	struct client **mem = (struct client **) calloc(max_conns, sizeof(struct client *));
	if (mem == NULL) {
		DEBUG_PRINT("malloc, memory");
		return -errno;
	}

	// set client array to empty
	for (int i = 0; i < max_conns; i++) {
		mem[i] = NULL;
	}

	// initialize structure fields
	target->server_fd = -1;
	target->server_port = port;
	target->clients = mem;
	target->max_connections = max_conns;
	target->cur_connections = 0;
	target->connect_queue = queue_len;
	target->window = window;

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

int create_client(struct client **target, size_t window) {
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
	if (init_client(init, window) < 0) {
		DEBUG_PRINT("init");
		free(init);
		return -errno;
	}

	// set given pointer to new struct
	*target = init;
	return 0;
}

int init_client(struct client *target, size_t window) {
	// check valid argument
	if (target == NULL || window < 1) {
		return -EINVAL;
	}

	// initialize structure fields
	target->socket_fd = -1;
	target->server_fd = -1;
	target->inc_flag = 0;
	target->out_flag = 0;
	target->window = window;

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
