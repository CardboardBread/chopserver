#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

#include "chopdebug.h"
#include "chopconst.h"

int header_type = 0;
static const char *all_headers[] = {"[CLIENT %d]", "[SERVER %d]"};

/*
 * Header-to-String Section
 */

static const int packet_id_len = 4;
static const char *packet_id[] = {
"PACKET_HEAD",
"PACKET_STATUS",
"PACKET_CONTROL1",
"PACKET_CONTROL2"};

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
		"UNIT_SEPARATOR"};

static const int enquiry_str_len = 4;
static const char *enquiry_str[] = {
		"ENQUIRY_NORMAL",
		"ENQUIRY_RETURN",
		"ENQUIRY_TIME",
		"ENQUIRY_RTIME"};

// errno is preserved through this function,
// it will not change between this function calling and returning.
void _debug_print(const char *function, const char *format, ...) {
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
	dprintf(debug_fd, dbg_fcn_head, function);
	vdprintf(debug_fd, format, args);
	dprintf(debug_fd, dbg_fcn_tail);

	// in case errno is nonzero
	if (errsav > 0) {
		dprintf(debug_fd, dbg_err, errsav, strerror(errsav));
	}

	// cleaning up
	va_end(args);
	errno = errsav;
	return;
}

int print_text(struct client *client, struct packet *pack) {
    // check valid arguments
    if (client == NULL || pack == NULL || pack->status != START_TEXT) {
        DEBUG_PRINT("invalid arguments");
        return -EINVAL;
    }

    // print prefix for message
    printf(msg_header(), client->socket_fd);

    printf(recv_text_start);
    // print every buffer out sequentially
    struct buffer *cur;
    for (cur = pack->data; cur != NULL; cur = cur->next) {
        printf(recv_text_seg, cur->inbuf, cur->buf);
    }

    // print line end for spacing
    printf(recv_text_end);

    return 0;
}

int print_enquiry(struct client *client, struct packet *pack) {
    // check valid arguments
    if (client == NULL || pack == NULL || pack->status != ENQUIRY) {
        DEBUG_PRINT("invalid arguments");
        return -EINVAL;
    }

    switch (pack->control1) {
        case ENQUIRY_NORMAL:
            printf(msg_header(), client->socket_fd);
            printf(recv_ping_norm);
            break;

        case ENQUIRY_RETURN:
            printf(msg_header(), client->socket_fd);
            printf(recv_ping_send);
            break;

        case ENQUIRY_TIME:
            return print_time(client, pack);
            break;

        case ENQUIRY_RTIME:
            printf(msg_header(), client->socket_fd);
            printf(recv_ping_time_send);
            break;

        default:
            return 1;
    }

    return 0;
}

int print_time(struct client *client, struct packet *pack) {
    // check valid arguments
    if (client == NULL || pack == NULL || pack->data == NULL) {
        DEBUG_PRINT("invalid arguments");
        return -EINVAL;
    }

    // make sure incoming packet is time packet
    if (pack->status != ENQUIRY || pack->control1 != ENQUIRY_TIME) {
        DEBUG_PRINT("invalid packet type");
        return -1;
    }

    // print time message
    time_t recv_time;
    memmove(&recv_time, pack->data->buf, sizeof(time_t));
    printf(msg_header(), client->socket_fd);
    printf(recv_ping_time, recv_time);

    return 0;
}

int print_acknowledge(struct client *client, struct packet *pack) {
    // check valid arguments
    if (client == NULL || pack == NULL) {
        DEBUG_PRINT("invalid arguments");
        return -EINVAL;
    }

    // print acknowledge contents
    printf(msg_header(), client->socket_fd);
    printf(ackn_text, stat_to_str(pack->control1));

    return 0;
}

int print_wakeup(struct client *client, struct packet *pack) {
    // check valid arguments
    if (client == NULL || pack == NULL) {
        DEBUG_PRINT("invalid arguments");
        return -EINVAL;
    }

    // print wakeup
    printf(msg_header(), client->socket_fd);
    printf(wakeup_text);

    return 0;
}

int print_neg_acknowledge(struct client *client, struct packet *pack) {
    // check valid arguments
    if (client == NULL || pack == NULL) {
        DEBUG_PRINT("invalid arguments");
        return -EINVAL;
    }

    // print negative acknowledge contents
    printf(msg_header(), client->socket_fd);
    printf(neg_ackn_text, stat_to_str(pack->control1));

    return 0;
}

int print_idle(struct client *client, struct packet *pack) {
    // check valid arguments
    if (client == NULL || pack == NULL) {
        DEBUG_PRINT("invalid arguments");
        return -EINVAL;
    }

    // print idle contents
    printf(msg_header(), client->socket_fd);
    printf(idle_text);

    return 0;
}

int print_error(struct client *client, struct packet *pack) {
	// check valid arguments
	if (client == NULL || pack == NULL) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	// print error contents
	printf(dbg_err, pack->control1, strerror(pack->control1));

	return 0;
}

int print_escape(struct client *client, struct packet *pack) {
    // check valid arguments
    if (client == NULL || pack == NULL) {
        DEBUG_PRINT("invalid arguments");
        return -EINVAL;
    }

    // print escape contents
    printf(msg_header(), client->socket_fd);
    printf(esc_text);

    return 0;
}

const char *stat_to_str(char status) {
	if (status < 0) {
		return NULL;
	}

	int index = (int) status;

	if (index > status_str_len) {
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
