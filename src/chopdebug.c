#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#include "chopdebug.h"

pthread_mutex_t print_lock;

/*
 * Client/Server Print Headers
 */
int header_type = -1;
static const char *all_headers[] = {
		"[CLIENT %d]: ",
		"[SERVER %d]: "
};

/*
 * Printing Function Array
 */
status_function printers[MAX_STATUS] = {
		NULL,					// 0
		NULL,					// 1
		print_text,				// 2
		NULL,					// 3
		NULL,					// 4
		print_enquiry,			// 5
		print_acknowledge,		// 6
		print_wakeup,			// 7
		NULL,					// 8
		NULL,					// 9
		NULL,					// 10
		NULL,					// 11
		NULL,					// 12
		NULL,					// 13
		NULL,					// 14
		NULL,					// 15
		NULL,					// 16
		NULL,					// 17
		NULL,					// 18
		NULL,					// 19
		NULL,					// 20
		print_neg_acknowledge,	// 21
		print_idle,			// 22
		NULL,					// 23
		NULL,					// 24
		NULL,					// 25
		print_error,			// 26
		print_escape,			// 27
		NULL,					// 28
		NULL,					// 29
		NULL,					// 30
		NULL					// 31
};

void no_operation() {

}

// errno is preserved through this function,
// it will not change between this function calling and returning.
void debug_print(const char *function, const char *format, ...) {
	// check valid arguments
	if (format == NULL) {
		return;
	}

	// saving errno
	int errsav = errno;

	// grab id of calling thread
	unsigned long caller = pthread_self();

	// capturing variable argument list
	va_list args;
	va_start(args, format);

	// lock access to the print stream
	pthread_mutex_lock(&print_lock);

	// print standard or error header
	if (errsav > 0) {
		dprintf(debug_fd, dbg_err, errsav, caller, function, strerror(errsav));
	} else {
		dprintf(debug_fd, dbg_head, caller, function);
	}

	// print arguments and tail
	vdprintf(debug_fd, format, args);
	dprintf(debug_fd, msg_tail);

	// unlock access to the debug stream
	pthread_mutex_unlock(&print_lock);

	// cleaning up
	va_end(args);
	errno = errsav;

	return;
}

void message_print(int caller_id, const char *format, ...) {
	// check valid arguments
	if (format == NULL || header_type < 0) {
		return;
	}

	// saving errno
	int errsav = errno;

	// capturing variable argument list
	va_list args;
	va_start(args, format);

	// lock access to the print stream
	pthread_mutex_lock(&print_lock);

	// printing argument
	dprintf(msg_fd, msg_header(), caller_id);
	vdprintf(msg_fd, format, args);
	dprintf(msg_fd, msg_tail);

	// unlock access to the debug stream
	pthread_mutex_unlock(&print_lock);

	// cleaning up
	va_end(args);
	errno = errsav;
	return;
}

int print_packet(struct client *client, struct packet *pack) {
	INVAL_CHECK(client == NULL || pack == NULL);

	// call function responsible for printing packet
	int subcall = 0;
	int sub_index = pack->header.status;
	subcall = printers[sub_index](client, pack);

	return subcall;
}

int print_text(struct client *client, struct packet *pack) {
    // check valid arguments
    INVAL_CHECK(client == NULL || pack == NULL || pack->header.status != START_TEXT);

	// lock access to the print stream
	pthread_mutex_lock(&print_lock);

    // print prefix for message
    printf(msg_header(), client->socket_fd);

    printf(recv_text_start);
    // print every buffer out sequentially
    struct buffer *cur;
    for (cur = pack->data; cur != NULL; cur = cur->next) {
        printf(recv_text_seg, (int) cur->inbuf, cur->buf);
    }

    // print line end for spacing
    printf(recv_text_end);

	// unlock access to the debug stream
	pthread_mutex_unlock(&print_lock);

    return 0;
}

int print_enquiry(struct client *client, struct packet *pack) {
    // check valid arguments
    INVAL_CHECK(client == NULL || pack == NULL || pack->header.status != ENQUIRY);

    switch (pack->header.control1) {
        case ENQUIRY_NORMAL:
			MESSAGE_PRINT(client->socket_fd, recv_ping_norm);
            break;

        case ENQUIRY_RETURN:
			MESSAGE_PRINT(client->socket_fd, recv_ping_send);
            break;

        case ENQUIRY_TIME:
            return print_time(client, pack);
            break;

        case ENQUIRY_RTIME:
			MESSAGE_PRINT(client->socket_fd, recv_ping_time_send);
            break;

        default:
            return 1;
    }

    return 0;
}

int print_time(struct client *client, struct packet *pack) {
    // check valid arguments
    INVAL_CHECK(client == NULL || pack == NULL || pack->data == NULL);

    // make sure incoming packet is time packet
    if (pack->header.status != ENQUIRY || pack->header.control1 != ENQUIRY_TIME) {
        DEBUG_PRINT("invalid packet type");
        return -1;
    }

    // print time message
    time_t recv_time;
    memmove(&recv_time, pack->data->buf, sizeof(time_t));
	MESSAGE_PRINT(client->socket_fd, recv_ping_time, recv_time);

    return 0;
}

int print_acknowledge(struct client *client, struct packet *pack) {
    // check valid arguments
    INVAL_CHECK(client == NULL || pack == NULL || pack->header.status != ACKNOWLEDGE);

    // print acknowledge contents
	MESSAGE_PRINT(client->socket_fd, ackn_text, stat_to_str(pack->header.control1));

    return 0;
}

int print_wakeup(struct client *client, struct packet *pack) {
    // check valid arguments
    INVAL_CHECK(client == NULL || pack == NULL || pack->header.status != WAKEUP);

    // print wakeup
    MESSAGE_PRINT(client->socket_fd, wakeup_text);

    return 0;
}

int print_neg_acknowledge(struct client *client, struct packet *pack) {
    // check valid arguments
    INVAL_CHECK(client == NULL || pack == NULL || pack->header.status != NEG_ACKNOWLEDGE);

    // print negative acknowledge contents
    MESSAGE_PRINT(client->socket_fd, neg_ackn_text, stat_to_str(pack->header.control1));

    return 0;
}

int print_idle(struct client *client, struct packet *pack) {
    // check valid arguments
    INVAL_CHECK(client == NULL || pack == NULL || pack->header.status != IDLE);

    // print idle contents
    MESSAGE_PRINT(client->socket_fd, idle_text);

    return 0;
}

int print_error(struct client *client, struct packet *pack) {
	// check valid arguments
	INVAL_CHECK(client == NULL || pack == NULL || pack->header.status != SUBSTITUTE);

	// print error contents
	int error_number = pack->header.control1;
	const char *error_desc = strerror(pack->header.control1);
	MESSAGE_PRINT(client->socket_fd, err_text, error_number, error_desc);

	return 0;
}

int print_escape(struct client *client, struct packet *pack) {
    // check valid arguments
    INVAL_CHECK(client == NULL || pack == NULL || pack->header.status != ESCAPE);

    // print escape contents
    MESSAGE_PRINT(client->socket_fd, esc_text);

    return 0;
}

const char *stat_to_str(pack_stat status) {
	if (status < 0 || status >= MAX_STATUS) {
		return NULL;
	}

	int index = (int) status;
	return status_str_arr[index].name;
}

const char *enq_cont_to_str(pack_con1 control1) {
	if (control1 < 0 || control1 >= MAX_ENQUIRY) {
		return NULL;
	}

	int index = (int) control1;
	return enquiry_str_arr[index].name;
}

const char *msg_header() {
	return all_headers[header_type];
}
