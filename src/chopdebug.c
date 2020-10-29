#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/file.h>

#include "chopdebug.h"

int header_type = 0;
static const char *all_headers[] = {
		"[CLIENT %d]: ",
		"[SERVER %d]: "
};

/*
 * Header-to-String Section
 */

static const int packet_id_len = 4;
static const char *packet_id[] = {
		"PACKET_HEAD",
		"PACKET_STATUS",
		"PACKET_CONTROL1",
		"PACKET_CONTROL2"
};

static const int status_str_len = 32;
static const char *status_str[] = {
		"NULL_BYTE",
		"START_HEADER",
		"START_TEXT",
		"END_TEXT",
		"END_TRANSMISSION",
		"ENQURIY",
		"ACKNOWLEDGE",
		"WAKEUP",
		"8",
		"9",
		"10",
		"11",
		"12",
		"13",
		"SHIFT_OUT",
		"SHIFT_IN",
		"START_DATA",
		"CONTROL_ONE",
		"CONTROL_TWO",
		"CONTROL_THREE",
		"CONTROL_FOUR",
		"NEG_ACKNOWLEDGE",
		"IDLE",
		"END_TRANSMISSION_BLOCK",
		"CANCEL",
		"END_OF_MEDIUM",
		"SUBSTITUTE",
		"ESCAPE",
		"FILE_SEPARATOR",
		"GROUP_SEPARATOR",
		"RECORD_SEPARATOR",
		"UNIT_SEPARATOR"
};

static const int enquiry_str_len = 4;
static const char *enquiry_str[] = {
		"ENQUIRY_NORMAL",
		"ENQUIRY_RETURN",
		"ENQUIRY_TIME",
		"ENQUIRY_RTIME"
};

// errno is preserved through this function,
// it will not change between this function calling and returning.
void debug_print(const char *function, const char *format, ...) {
	// check valid arguments
	if (format == NULL) {
		return;
	}

	// saving errno
	int errsav = errno;

	// lock access to the debug stream
	flock(debug_fd, LOCK_EX);

	// capturing variable argument list
	va_list args;
	va_start(args, format);

	// print header with calling thread's shortened id
	short caller = pthread_self();
	dprintf(debug_fd, dbg_fcn_thr_head, caller, function);

	// printing argument
	vdprintf(debug_fd, format, args);
	dprintf(debug_fd, msg_tail);

	// in case errno is nonzero
	if (errsav > 0) {
		dprintf(debug_fd, dbg_err, errsav, strerror(errsav));
	}

	// unlock access to the debug stream
	flock(debug_fd, LOCK_UN);

	// cleaning up
	va_end(args);
	errno = errsav;

	return;
}

void message_print(int socketfd, const char *format, ...) {
	// check valid arguments
	if (format == NULL) {
		return;
	}

	// saving errno
	int errsav = errno;

	// capturing variable argument list
	va_list args;
	va_start(args, format);

	// printing argument
	dprintf(msg_fd, msg_header(), socketfd);
	vdprintf(msg_fd, format, args);
	dprintf(msg_fd, msg_tail);

	// cleaning up
	va_end(args);
	errno = errsav;
	return;
}

int print_text(struct client *client, struct packet *pack) {
    // check valid arguments
    INVAL_CHECK(client == NULL || pack == NULL || pack->header.status != START_TEXT);

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
	printf(dbg_err, pack->header.control1, strerror(pack->header.control1));

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
	if (status < 0) {
		return NULL;
	}

	int index = (int) status;

	if (index < 0 || index >= status_str_len) {
		return NULL;
	}

	return status_str[index];
}

const char *enq_cont_to_str(char control1) {
	if (control1 < 0) {
		return NULL;
	}

	int index = (int) control1;

	if (index > enquiry_str_len) {
		return NULL;
	}

	return enquiry_str[index];
}

const char *pack_id_to_str(int id) {
	if (id < 0 || id > packet_id_len) {
		return NULL;
	}

	return packet_id[id];
}

const char *msg_header() {
	return all_headers[header_type];
}
