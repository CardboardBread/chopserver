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

/*
* Sending functions
*/

int write_dataless(struct client *cli, const pack_head head, const pack_stat status, const pack_con1 control1, const pack_con2 control2) {
	// precondition for invalid arguments
	if (cli == NULL) {
		DEBUG_PRINT("invalid argument");
		return -EINVAL;
	}

	// initalize packet
	struct packet *out;
	if (init_packet_struct(&out) < 0) {
		DEBUG_PRINT("failed init packet");
		return -1;
	}

	// setting header value
	if (assemble_header(out, head, status, control1, control2) < 0) {
		DEBUG_PRINT("failed header assemble");
		return -1;
	}

	// write to client
	int ret = write_packet(cli, out);
	if (ret < 0) {
		DEBUG_PRINT("failed write");
		return ret;
	}

	// destroy allocated packet
	if (destroy_packet_struct(&out) < 0) {
		DEBUG_PRINT("failed packet destroy");
		return -1;
	}

	return ret;
}

int write_datapack(struct client *cli, const pack_head head, const pack_stat status, const pack_con1 control1, const pack_con2 control2, const char *buf, const int buflen) {
	// check valid arguments
	if (cli == NULL || buf == NULL || buflen < 0) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	// initalize packet
	struct packet *out;
	if (init_packet_struct(&out) < 0) {
		DEBUG_PRINT("failed init packet");
		return -ENOMEM;
	}

	// setting header value
	if (assemble_header(out, head, status, control1, control2) < 0) {
		DEBUG_PRINT("failed header assemble");
		return -EINVAL;
	}

	// allocate data segment
	struct buffer *segment;
	if (append_buffer(out, buflen, &segment) < 0) {
		DEBUG_PRINT("failed data expansion");
		return -1;
	}

	// place data in section
	if (assemble_body(segment, buf, buflen) < 0) {
		DEBUG_PRINT("failed body assemble");
		return -1;
	}

	// write to client
	if (write_packet(cli, out) < 0) {
		DEBUG_PRINT("failed write");
		return -1;
	}

	// destroy allocated packet
	if (destroy_packet_struct(&out) < 0) {
		DEBUG_PRINT("failed packet destroy");
		return -1;
	}

	return 0;
}

int write_wordpack(struct client *cli, const pack_head head, const pack_stat status, const pack_con1 control1, const pack_con2 control2, unsigned long int value) {
	// precondition for invalid arguments
	if (cli == NULL || head < 0 || status < 0 || control1 < 0 || control2 < 0) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	// initalize packet
	struct packet *out;
	if (init_packet_struct(&out) < 0) {
		DEBUG_PRINT("failed init packet");
		return -1;
	}

	// setting header value
	if (assemble_header(out, head, status, control1, control2) < 0) {
		DEBUG_PRINT("failed header assemble");
		return -1;
	}

	// allocate data segment
	struct buffer *segment;
	if (append_buffer(out, sizeof(unsigned long int), &segment) < 0) {
		DEBUG_PRINT("failed data expansion");
		return -1;
	}

	// create buffer to place value in
	char buffer[sizeof(unsigned long int)];

	// move value into buffer
	memmove(buffer, &value, sizeof(unsigned long int));

	// place data in section
	if (assemble_body(segment, buffer, sizeof(unsigned long int)) < 0) {
		DEBUG_PRINT("failed body assemble");
		return -1;
	}

	// write to client
	if (write_packet(cli, out) < 0) {
		DEBUG_PRINT("failed write");
		return -1;
	}

	// destroy allocated packet
	if (destroy_packet_struct(&out) < 0) {
		DEBUG_PRINT("failed packet destroy");
		return -1;
	}

	return 0;
}

/*
* Receiving Functions
*/

int parse_header(struct client *cli, struct packet *pack) {
	// precondition for invalid arguments
	if (cli == NULL || pack == NULL) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	// parse the status
	int status = 0;
	switch (pack->status) {
		case NULL_BYTE:
			DEBUG_PRINT("received NULL header");
			break;

		case START_HEADER:
			DEBUG_PRINT("received extended header");
			status = parse_long_header(cli, pack);
			break;

		case START_TEXT:
			DEBUG_PRINT("received text header");
			status = parse_text(cli, pack);

			// print incoming text
			if (print_text(cli, pack) < 0) {
				DEBUG_PRINT("failed print");
				return -1;
			}
			break;

		case ENQUIRY:
			DEBUG_PRINT("received enquiry header");
			status = parse_enquiry(cli, pack);

			// print incoming enquiry
			if (print_enquiry(cli, pack) < 0) {
				DEBUG_PRINT("failed print");
				return -1;
			}
			break;

		case ACKNOWLEDGE:
			DEBUG_PRINT("received acknowledge header");
			status = parse_acknowledge(cli, pack);

			// print incoming acknowledge
			if (print_acknowledge(cli, pack) < 0) {
				DEBUG_PRINT("failed print");
				return -1;
			}
			break;

		case WAKEUP:
			DEBUG_PRINT("received wakeup header");
			status = parse_wakeup(cli, pack);

			// print incoming wakeup
			if (print_wakeup(cli, pack) < 0) {
				DEBUG_PRINT("failed print");
				return -1;
			}
			break;

		case NEG_ACKNOWLEDGE:
			DEBUG_PRINT("received negative acknowledge header");
			status = parse_neg_acknowledge(cli, pack);

			// print incoming negative acknowledge
			if (print_neg_acknowledge(cli, pack) < 0) {
				DEBUG_PRINT("failed print");
				return -1;
			}
			break;

		case IDLE:
			DEBUG_PRINT("received idle header");
			status = parse_idle(cli, pack);

			// print incoming idle
			if (print_idle(cli, pack) < 0) {
				DEBUG_PRINT("failed print");
				return -1;
			}
			break;

		case ESCAPE:
			DEBUG_PRINT("received escape header");
			status = parse_escape(cli, pack);

			// print incoming escape
			if (print_escape(cli, pack) < 0) {
				DEBUG_PRINT("failed print");
				return -1;
			}
			break;

		default: // unsupported/invalid
			DEBUG_PRINT("received invalid header");
			status = -1;
			break;
	}

	return status;
}

int parse_long_header(struct client *cli, struct packet *pack) {
	// precondition for invalid arguments
	if (cli == NULL || pack == NULL) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	int packetcount = pack->control1;
	DEBUG_PRINT("long header of %d", packetcount);

	// TODO: adapt to new code
	int status = 0;
	for (int i = 0; i < packetcount; i++) {
		status = read_header(cli, pack);
		if (status) break;
		status = parse_header(cli, pack);
		if (status) break;
	}

	return 0;
}

int parse_text(struct client *cli, struct packet *pack) {
	// precondition for invalid arguments
	if (cli == NULL || pack == NULL) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	// translate names for readability
	int count = pack->control1;
	int width = pack->control2;

	// signals set to unknown length read
	if (count == 0 && width == 0) {
		int long_len = read_long_text(cli, pack);
		if (long_len < 0) {
			DEBUG_PRINT("long text failed");
			return -1;
		}

		DEBUG_PRINT("long text section length %d, %d segments", long_len, pack->datalen);
		return 0;
	} else {

		// read normally
		int remaining = count * width;
		int received = read_data(cli, pack, remaining);
		if (received < 0) {
			DEBUG_PRINT("failed normal read");
			return -1;
		}
	}

	if (write_dataless(cli, 0, ACKNOWLEDGE, START_TEXT, 0) < 0) {
		DEBUG_PRINT("failed confirm packet");
		return -1;
	}

	return 0;
}

int read_long_text(struct client *cli, struct packet *pack) {
	// precondition for invalid arguments
	if (cli == NULL || pack == NULL) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	int found = 1;
	int total = 0;
	while (found) {

		// allocate buffer to hold incoming data
		struct buffer *receive;
		if (append_buffer(pack, cli->window, &receive) < 0) {
			return -ENOSPC;
		}

		// read data into new buffer
		int bytes_read = read(cli->socket_fd, receive->buf, cli->window);
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
	if (cli == NULL || pack == NULL) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	switch (pack->control1) {
		case ENQUIRY_NORMAL:
			DEBUG_PRINT("normal enquiry");

			// just return an acknowledge
			if (write_dataless(cli, 0, ACKNOWLEDGE, ENQUIRY, 0) < 0) {
				DEBUG_PRINT("failed acknowledge packet");
				return -1;
			}
			break;

		case ENQUIRY_RETURN:
			DEBUG_PRINT("return enquiry");

			// return a enquiry signal 0
			if (write_dataless(cli, 0, ENQUIRY, 0, 0) < 0) {
				DEBUG_PRINT("failed enquiry return");
				return -1;
			}
			break;

		case ENQUIRY_TIME:
			DEBUG_PRINT("time enquiry %d wide", pack->control2);
			// read packet data
			if (read_data(cli, pack, pack->control2) < 0) {
				DEBUG_PRINT("failed time data read");
				return -1;
			}

			// return acknowledge
			if (write_dataless(cli, 0, ACKNOWLEDGE, ENQUIRY, 0) < 0) {
				DEBUG_PRINT("failed acknowledge packet");
				return -1;
			}
			break;

		case ENQUIRY_RTIME:
			DEBUG_PRINT("time request");

			// return time enquiry packet
			if (write_wordpack(cli, 0, ENQUIRY, ENQUIRY_TIME, sizeof(time_t), time(NULL)) < 0) {
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
	if (cli == NULL || pack == NULL) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	switch (pack->control1) {
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

	return 0;
}

int parse_wakeup(struct client *cli, struct packet *pack) {
	// check valid arguments
	if (cli == NULL || pack == NULL) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	// setting values of header
	if (cli->inc_flag == IDLE) {
		// demark incoming client flag
		cli->inc_flag = NULL_BYTE;

		// confirm client wake
		if (write_dataless(cli, 0, ACKNOWLEDGE, WAKEUP, 0) < 0) {
			DEBUG_PRINT("failed confirm packet");
			return -1;
		}

		DEBUG_PRINT("client %d now awake", cli->socket_fd);
	} else {
		// refuse client wake
		if (write_dataless(cli, 0, NEG_ACKNOWLEDGE, WAKEUP, 0) < 0) {
			DEBUG_PRINT("failed deny packet");
			return -1;
		}

		DEBUG_PRINT("client %d awake, refusing", cli->socket_fd);
	}

	return 0;
}

int parse_neg_acknowledge(struct client *cli, struct packet *pack) {
	// precondition for invalid argument
	if (cli == NULL || pack == NULL) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	switch (pack->control1) {
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
			DEBUG_PRINT("invalid packet %d refused", pack->control1);
			return -1;
	}

	return 0;
}

int parse_idle(struct client *cli, struct packet *pack) {
	// precondition for invalid argument
	if (cli == NULL || pack == NULL) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	// setting values of header
	if (cli->inc_flag == NULL_BYTE) {
		// mark client as sleeping on incoming channel
		cli->inc_flag = IDLE;

		// confirm client idle
		if (write_dataless(cli, 0, ACKNOWLEDGE, IDLE, 0) < 0) {
			DEBUG_PRINT("failed confirm packet");
			return -1;
		}

		DEBUG_PRINT("client %d now sleeping", cli->socket_fd);
	} else {
		// refuse client idle
		if (write_dataless(cli, 0, NEG_ACKNOWLEDGE, IDLE, 0) < 0) {
			DEBUG_PRINT("failed deny packet");
			return -1;
		}

		DEBUG_PRINT("client %d busy, refusing", cli->socket_fd);
	}

	return 0;
}

int parse_escape(struct client *cli, struct packet *pack) {
	// precondition for invalid argument
	if (cli == NULL || pack == NULL) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	// confirm client escape
	if (write_dataless(cli, 0, ACKNOWLEDGE, ESCAPE, 0) < 0) {
		DEBUG_PRINT("failed confirm packet");
		return -1;
	}
	DEBUG_PRINT("shutting down client %d connection", cli->socket_fd);

	// marking this client as closed
	cli->inc_flag = CANCEL;
	cli->out_flag = CANCEL;

	return 0;
}

/*
* Packet Utility Functions
*/

int assemble_header(struct packet *pack, pack_head head, pack_stat status, pack_con1 control1, pack_con2 control2) {
	// check valid arguments
	if (pack == NULL) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	// assemble packet header with given values
	pack->head = head;
	pack->status = status;
	pack->control1 = control1;
	pack->control2 = control2;

	return 0;
}

int assemble_body(struct buffer *buffer, const char *data, const int len) {
	// check valid arguments
	if (buffer == NULL || data == NULL || len < 0) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	// cannot fit message in buffer
	if (len > buffer->bufsize) {
		DEBUG_PRINT("message too large");
		return -1;
	}

	// move data into packet
	memmove(buffer->buf, data, len);
	buffer->inbuf = len;

	return 0;
}

int append_buffer(struct packet *pack, const int bufsize, struct buffer **out) {
	// check valid argument
	if (pack == NULL || bufsize < 0) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	// data section is empty
	if (pack->data == NULL) {
		if (init_buffer_struct(&(pack->data), bufsize) < 0) {
			DEBUG_PRINT("failed buffer init");
			return -1;
		}

		// return reference to new buffer
		if (out != NULL) {
			*out = pack->data;
		}

		pack->datalen++;
		return 0;
	}

	// loop to end of data section
	struct buffer *cur;
	for (cur = pack->data; cur->next != NULL; cur = cur->next);

	// append new empty data segment
	if (init_buffer_struct(&(cur->next), bufsize) < 0) {
		DEBUG_PRINT("failed buffer init");
		return -1;
	}

	// return reference to new buffer
	if (out != NULL) {
		*out = cur->next;
	}

	pack->datalen++;
	return 0;
}

int packet_style(struct packet *pack) {
	// check valid argument
	if (pack == NULL) {
		DEBUG_PRINT("invalid arguments");
		return 0;
	}

	// declare on-stack container for header
	int style;

	// create references to each byte of container
	char *hd = (char *) &style;
	char *st = hd + 1;
	char *c1 = st + 1;
	char *c2 = c1 + 1;

	// copy values from packet header segments
	*hd = (char) pack->head;
	*st = (char) pack->status;
	*c1 = (char) pack->control1;
	*c2 = (char) pack->control2;

	return style;
}
