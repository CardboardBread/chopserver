#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "chopconst.h"
#include "chopdata.h"
#include "chopdebug.h"
#include "choppacket.h"

int fill_buf(struct buffer *buffer, const int input) {
	// check valid inputs
	if (buffer == NULL || input < 0) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	// buffer full case
	if (buffer->inbuf == buffer->bufsize) {
		DEBUG_PRINT("buffer full");
		return 0;
	}

	// calculate open space
	char *head = buffer->buf + buffer->inbuf;
	int left = buffer->bufsize - buffer->inbuf;

	// read as much as possible
	int readlen = read(input, head, left);

	// check for error
	if (readlen < 0) {
		DEBUG_PRINT("read fail");
		return -errno;
	}

	// increment buffer's written space
	buffer->inbuf += readlen;

	DEBUG_PRINT("read %d", readlen);
	return 0;
}

int read_data(struct client *cli, struct packet *pack, int remaining) {
	// check valid inputs
	if (cli == NULL || pack == NULL || remaining < 0) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	// zero incoming
	if (remaining == 0) {
		return 0;
	}

	int buffers = remaining / cli->window + (remaining % cli->window != 0);

	// loop as many times as new buffers are needed
	int total = 0;
	int expected;
	int bytes_read;
	struct buffer *receive;
	for (int i = 0; i < buffers; i++) {

		// allocate more space to hold data
		if (append_buffer(pack, cli->window, &receive) < 0) {
			DEBUG_PRINT("fail allocate buffer %d", i);
			return -ENOMEM;
		}

		// read expected bytes per data segment
		expected = (receive->bufsize > remaining) ? remaining : receive->bufsize;
		bytes_read = read(cli->socket_fd, receive->buf, expected);
		if (bytes_read != expected) {
			// in case read isn't perfect
			if (bytes_read < 0) {
				DEBUG_PRINT("failed data read");
				return -errno;
			} else if (bytes_read == 0) {
				DEBUG_PRINT("socket closed");
                cli->inc_flag = CANCEL;
                cli->out_flag = CANCEL;
				return -1;
			} else {
				DEBUG_PRINT("incomplete data read, %d remaining", expected - bytes_read);
				return -1;
			}
		}

		// update tracker fields
		remaining -= bytes_read;
		total += bytes_read;
		receive->inbuf = bytes_read;
	}

	DEBUG_PRINT("data section length %d, %d segments", total, buffers);
	return total;
}

int read_header(struct client *cli, struct packet *pack) {
    // precondition for invalid argument
    if (cli == NULL || pack == NULL) {
        DEBUG_PRINT("invalid arguments");
        return -EINVAL;
    }

    // buffer to receive header
    char header[HEADER_LEN];

    // read packet from client
    int head_read = read(cli->socket_fd, header, HEADER_LEN);
    if (head_read != HEADER_LEN) {

        // in case read isn't perfect
        if (head_read < 0) {
            DEBUG_PRINT("failed header read");
            return -errno;
        } else if (head_read == 0) {
            DEBUG_PRINT("socket closed");
            cli->inc_flag = CANCEL;
            cli->out_flag = CANCEL;
            return -1;
        } else {
            DEBUG_PRINT("incomplete header read");
            return -1;
        }
    }

    // move buffer to packet fields
    memmove(pack, header, HEADER_LEN);

    // print incoming header
    DEBUG_PRINT(dbg_pack, pack->head, stat_to_str(pack->status), pack->control1, pack->control2);

    // TODO: this is very platform dependent
    DEBUG_PRINT("read header, style %d, width %d", packet_style(pack), head_read);
    return 0;
}

int write_packet(struct client *cli, struct packet *pack) {
    // precondition for invalid arguments
    if (cli == NULL || pack == NULL) {
        DEBUG_PRINT("invalid arguments");
        return -EINVAL;
    }

    // mark client outgoing flag with status
    cli->out_flag = pack->status;

    // write header packet to target
    ssize_t head_written = write(cli->socket_fd, (void *) pack, HEADER_LEN);
    if (head_written != HEADER_LEN) {
        // in case write was not perfectly successful
        if (head_written < 0) {
            DEBUG_PRINT("failed header write");
            return -errno;
        } else if (head_written == 0) {
            DEBUG_PRINT("socket closed");
            return -1; // TODO: what to return?
        } else {
            DEBUG_PRINT("incomplete header write");
            return -1; // TODO: what to return?
        }
    }

    // loop through all buffers in packet's data section
    int total = 0;
    int tracker = 0;
    ssize_t bytes_written;
    struct buffer *segment;
    for (segment = pack->data; segment != NULL; segment = segment->next) {

        // write buffer to target (if any)
        bytes_written = write(cli->socket_fd, segment->buf, segment->inbuf);
        if (bytes_written != segment->inbuf) {
            // in case write was not perfectly successful
            if (bytes_written < 0) {
                DEBUG_PRINT("failed data write, segment %d", tracker);
                return -errno;
            } else if (bytes_written == 0) {
                DEBUG_PRINT("socket closed");
                return -1; // TODO: what to return?
            } else {
                DEBUG_PRINT("incomplete data write, segment %d", tracker);
                return -1; // TODO: what to return?
            }
        }

        // track depth into data section, as well as total data length
        tracker++;
        total += bytes_written;
    }

    // demark client outgoing flag
    cli->out_flag = 0;

    // print outgoing header
    DEBUG_PRINT(dbg_pack, pack->head, stat_to_str(pack->status), pack->control1, pack->control2);

    // TODO: this is very platform dependent
    DEBUG_PRINT("packet style %d, %d bytes header, %d bytes body", packet_style(pack), head_written, total);
    return total;
}

int find_newline(const char *buf, const int len) {
	// check valid arguments
	if (buf == NULL || len < 1) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	// first element case
	if (buf[0] == '\n') {
		DEBUG_PRINT("unix newline at 0");
		return 0;
	}

	// loop through buffer until first newline is found
	int index;
	for (index = 1; index < len; index++) {
		// network newline
		if (buf[index - 1] == '\r' && buf[index] == '\n') {
			DEBUG_PRINT("network newline at %d", index);
			return index;
		}

		// unix newline
		if (buf[index - 1] != '\r' && buf[index] == '\n') {
			DEBUG_PRINT("unix newline at %d", index);
			return index;
		}
	}

	// nothing found
	DEBUG_PRINT("none found");
	return -ENOENT;
}

int remove_newline(char *buf, const int len) {
	// check valid arguments
	if (buf == NULL || len < 1) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	// first element case
	if (buf[0] == '\n') {
		DEBUG_PRINT("unix newline at 0");
		buf[0] = '\0';
		return 0;
	}

	// loop through buffer until first newline is found
	int index;
	for (index = 1; index < len; index++) {
		// network newline
		if (buf[index - 1] == '\r' && buf[index] == '\n') {
			DEBUG_PRINT("network newline at %d", index);
			buf[index - 1] = '\0';
			buf[index] = '\0';
			return index;
		}

		// unix newline
		if (buf[index - 1] != '\r' && buf[index] == '\n') {
			DEBUG_PRINT("unix newline at %d", index);
			buf[index] = '\0';
			return index;
		}
	}

	// nothing found
	DEBUG_PRINT("none found");
	return -ENOENT;
}

int buf_contains_symbol(const char *buf, const int len, const char symbol) {
	// check valid arguments
	if (buf == NULL || len < 1) {
		DEBUG_PRINT("invalid arguments");
		return -EINVAL;
	}

	// loop through buffer until first symbol is found
	int index;
	for (index = 0; index < len; index++) {
		if (buf[index] == symbol) {
			DEBUG_PRINT("char %d found at %d", symbol, index);
			return index;
		}
	}

	// no symbol found
	DEBUG_PRINT("none found");
	return -ENOENT;
}

void char_to_bin(char value, char *ret) {
	for (int i = 0; i < 8; i++) {
		if (value & (0x1 << i)) {
			ret[7 - i] = '1';
		} else {
			ret[7 - i] = '0';
		}
	}
	ret[8] = '\0';
}
