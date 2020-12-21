#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "chopconst.h"
#include "chopdata.h"
#include "chopdebug.h"
#include "choppacket.h"
#include "chopsend.h"

#include "hashtable.h"

/*
 * Parsing Function Array
 */
const status_function parsers[MAX_STATUS] = {
		NULL,					// 0
		parse_long_header,		// 1
		parse_text,				// 2
		NULL,					// 3
		NULL,					// 4
		parse_enquiry,			// 5
		parse_acknowledge,		// 6
		parse_wakeup,			// 7
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
		parse_neg_acknowledge,	// 21
		parse_idle,				// 22
		NULL,					// 23
		NULL,					// 24
		NULL,					// 25
		parse_error,			// 26
		parse_escape,			// 27
		NULL,					// 28
		NULL,					// 29
		NULL,					// 30
		NULL					// 31
};

/*
* Receiving Functions
*/

int parse_header(struct client *cli, struct packet *pack) {
	// precondition for invalid arguments
	INVAL_CHECK(cli == NULL || pack == NULL);

	// mark client incoming flag with status
	cli->inc_flag = pack->header.status;

	// get packet parser
	int subcall = 0;
	int sub_index = pack->header.status;
	status_function parser = parsers[sub_index];

	// null status
	if (sub_index == 0) {
		DEBUG_PRINT("received NULL header");
		return subcall;
	}

	// unsupported/invalid status
	if (parser == NULL) {
		DEBUG_PRINT("received invalid header");
		return -ENOTSUP;
	}

	// call parsing function
	DEBUG_PRINT("received %s header", status_str_arr[sub_index].name);
	subcall = parsers[sub_index](cli, pack);
	if (subcall < 0) {
		DEBUG_PRINT("failed parse");
		return subcall;
	}

	// call printing function
	subcall = print_packet(cli, pack);
	if (subcall < 0) {
		DEBUG_PRINT("failed print");
		return subcall;
	}

	return subcall;
}

int parse_long_header(struct client *cli, struct packet *pack) {
	// precondition for invalid arguments
	INVAL_CHECK(cli == NULL || pack == NULL);

	int packet_count = pack->header.control1;
	DEBUG_PRINT("long header of %d", packet_count);

	// TODO: adapt to new code
	int status = 0;
	for (int i = 0; i < packet_count; i++) {
		status = read_header(cli, pack);
		if (status) break;
		status = parse_header(cli, pack);
		if (status) break;
	}

	return 0;
}

int parse_text(struct client *cli, struct packet *pack) {
	// precondition for invalid arguments
	INVAL_CHECK(cli == NULL || pack == NULL);

	// translate names for readability
	pack_con1 width = pack->header.control1;
	pack_con2 second = pack->header.control2;

	// signals set to unknown length read
	if (width == 0 && second == 1) {
		ssize_t long_len = read_long_text(cli, pack);
		if (long_len < 0) {
			DEBUG_PRINT("long text failed");
			return -1;
		}

		DEBUG_PRINT("long text section length %d, %zd segments", long_len, pack->datalen);
		return 0;
	} else {
		// read normally
		if (read_data(cli, pack, width) < 0) {
			DEBUG_PRINT("failed normal read");
			return -errno;
		}
	}

	// confirm text
	if (send_ackn(cli, pack, START_TEXT) < 0) {
		DEBUG_PRINT("failed confirm packet");
		return -errno;
	}

	return 0;
}

int read_long_text(struct client *cli, struct packet *pack) { // TODO: update
	// precondition for invalid arguments
	INVAL_CHECK(cli == NULL || pack == NULL);

	int found = 1;
	size_t total = 0;
	while (found) {

		// allocate buffer to hold incoming data
		struct buffer *receive;
		if (append_buffer(pack, cli->window, &receive) < 0) {
			return -ENOSPC;
		}

		// read data into new buffer
		ssize_t bytes_read = read(cli->socket_fd, receive->buf, cli->window);
		if (bytes_read < 0) {
			DEBUG_PRINT("failed to read long text section");
			return -errno;
		} else if (bytes_read == 0) {
			DEBUG_PRINT("long text section empty");
			return 0;
		}

		total += bytes_read;

		int stop_len = buf_contains_symbol(receive->buf, bytes_read, END_TEXT);
		if (stop_len < 0) {
			// did not find the end of the long text
			receive->inbuf = bytes_read;
			found = 1; // TODO: might be extraneous
		} else {
			// found the end of the long text
			receive->inbuf = stop_len;
			found = 0;
		}
	}

	return total;
}

int parse_enquiry(struct client *cli, struct packet *pack) {
	// precondition for invalid argments
	INVAL_CHECK(cli == NULL || pack == NULL);

	switch (pack->header.control1) {
		case ENQUIRY_NORMAL:
			DEBUG_PRINT("normal enquiry");

			// just return an acknowledge
			if (send_ackn(cli, pack, ENQUIRY) < 0) {
				DEBUG_PRINT("failed acknowledge packet");
				return -errno;
			}
			break;

		case ENQUIRY_RETURN:
			DEBUG_PRINT("return enquiry");

			// return an acknowledge
			if (send_ackn(cli, pack, ENQUIRY) < 0) {
				DEBUG_PRINT("failed acknowledge packet");
				return -errno;
			}

			// return a enquiry signal 0
			if (send_enqu(cli, ENQUIRY_NORMAL) < 0) {
				DEBUG_PRINT("failed enquiry return");
				return -errno;
			}
			break;

		case ENQUIRY_TIME:
			DEBUG_PRINT("time enquiry %u wide", pack->header.control2);
			// read packet data
			if (read_data(cli, pack, pack->header.control2) < 0) {
				DEBUG_PRINT("failed time data read");
				return -1;
			}

			// return acknowledge
			if (send_ackn(cli, pack, ENQUIRY) < 0) {
				DEBUG_PRINT("failed acknowledge packet");
				return -1;
			}
			break;

		case ENQUIRY_RTIME:
			DEBUG_PRINT("time request");

			// return an acknowledge
			if (send_ackn(cli, pack, ENQUIRY) < 0) {
				DEBUG_PRINT("failed acknowledge packet");
				return -errno;
			}

			// return time enquiry packet
			if (send_enqu(cli, ENQUIRY_TIME) < 0) {
				DEBUG_PRINT("failed time enquiry return");
				return -1;
			}
			break;

		default:
			DEBUG_PRINT("invalid/unsupported control signal");
			return -1;
	}

	return 0;
}

int parse_acknowledge(struct client *cli, struct packet *pack) {
	// precondition for invalid argument
	INVAL_CHECK(cli == NULL || pack == NULL);

	hash_key key = (hash_key) pack->header.head;
	hash_value val;

	// check for confirming known packet
	struct packet *reg;
	if (!table_remove(cli->pair_table, key, &val)) {
		DEBUG_PRINT("unknown packet confirmed, exchange %zu", key);
		reg = NULL;
	} else {
		DEBUG_PRINT("known packet confirmed, exchange %zu", key);
		reg = (struct packet *) val;
	}

	switch (pack->header.control1) {
		case START_TEXT: // text confirmed
			DEBUG_PRINT("text confirmed");
			break;

		case ENQUIRY:
			// TODO: ping was received
			DEBUG_PRINT("ping confirmed");
			break;

		case WAKEUP:
			// TODO: the sender says it has woken up
			DEBUG_PRINT("wakeup confirmed");
			cli->inc_flag = NULL_BYTE;
			break;

		case IDLE:
			// TODO: the sender says it has gone asleep
			DEBUG_PRINT("idle confirmed");
			cli->inc_flag = IDLE;
			break;

		case ESCAPE:
			// TODO: the sender knows you're stopping
			DEBUG_PRINT("escape confirmed");
			// marking this client as closed
			cli->inc_flag = CANCEL;
			cli->out_flag = CANCEL;
			break;

		default:
			DEBUG_PRINT("invalid/unsupported control signal");
			return -1;
	}

	// destroy registered packet (if one exists)
	if (destroy_packet(&reg) < 0) {
		DEBUG_PRINT("failed paired packet destroy");
		return -errno;
	}

	return 0;
}

int parse_wakeup(struct client *cli, struct packet *pack) {
	// check valid arguments
	INVAL_CHECK(cli == NULL || pack == NULL);

	// setting values of header
	if (cli->inc_flag == IDLE) {
		// demark incoming client flag
		cli->inc_flag = NULL_BYTE;

		// confirm client wake
		if (send_ackn(cli, pack, WAKEUP) < 0) {
			DEBUG_PRINT("failed confirm packet");
			return -1;
		}

		DEBUG_PRINT("client %d now awake", cli->socket_fd);
	} else {
		// refuse client wake
		if (send_neg_ackn(cli, pack, WAKEUP) < 0) {
			DEBUG_PRINT("failed deny packet");
			return -1;
		}

		DEBUG_PRINT("client %d awake, refusing", cli->socket_fd);
	}

	return 0;
}

int parse_neg_acknowledge(struct client *cli, struct packet *pack) {
	// precondition for invalid argument
	INVAL_CHECK(cli == NULL || pack == NULL);

	switch (pack->header.control1) {
		case START_TEXT: // text arguments are invalid/message is invalid
			DEBUG_PRINT("client %d refused text", cli->socket_fd);
			break;

		case ENQUIRY: // ping refused
			DEBUG_PRINT("client %d refused ping", cli->socket_fd);
			break;

		case WAKEUP: // sender is already awake/cannot wakeup
			DEBUG_PRINT("client %d refused wakeup", cli->socket_fd);
			break;

		case IDLE: // sender is already sleeping/cannot sleep
			DEBUG_PRINT("client %d refused idle", cli->socket_fd);
			break;

		case ESCAPE: // you cannot disconnect
			DEBUG_PRINT("client %d refused disconnect", cli->socket_fd);
			break;

		default: // invalid/unsupported packet status
			DEBUG_PRINT("invalid packet %d refused", pack->header.control1);
			return -1;
	}

	return 0;
}

int parse_idle(struct client *cli, struct packet *pack) {
	// precondition for invalid argument
	INVAL_CHECK(cli == NULL || pack == NULL);

	// setting values of header
	if (cli->inc_flag == NULL_BYTE) {
		// mark client as sleeping on incoming channel
		cli->inc_flag = IDLE;

		// confirm client idle
		if (send_ackn(cli, pack, IDLE) < 0) {
			DEBUG_PRINT("failed confirm packet");
			return -1;
		}

		DEBUG_PRINT("client %d now sleeping", cli->socket_fd);
	} else {
		// refuse client idle
		if (send_neg_ackn(cli, pack, IDLE) < 0) {
			DEBUG_PRINT("failed deny packet");
			return -1;
		}

		DEBUG_PRINT("client %d busy, refusing", cli->socket_fd);
	}

	return 0;
}

int parse_error(struct client *cli, struct packet *pack) {
	// precondition for invalid argument
	INVAL_CHECK(cli == NULL || pack == NULL);

	// confirm error
	if (send_ackn(cli, pack, SUBSTITUTE) < 0) {
		DEBUG_PRINT("failed confirm packet");
		return -1;
	}
	DEBUG_PRINT("client %d encountered error", cli->socket_fd);

	return 0;
}

int parse_escape(struct client *cli, struct packet *pack) {
	// precondition for invalid argument
	INVAL_CHECK(cli == NULL || pack == NULL);

	// confirm client escape
	if (send_ackn(cli, pack, ESCAPE) < 0) {
		DEBUG_PRINT("failed confirm packet");
		return -1;
	}
	DEBUG_PRINT("shutting down client %d connection", cli->socket_fd);

	// marking this client as closed
	cli->inc_flag = CANCEL;
	cli->out_flag = CANCEL;

	return 0;
}
