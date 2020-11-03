#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "chopconst.h"
#include "chopdata.h"
#include "chopdebug.h"

int fill_buf(struct buffer *buffer, int input_fd) {
	// check valid inputs
	INVAL_CHECK(buffer == NULL || input_fd < MIN_FD);

	// calculate open space
	char *head = buffer->buf + buffer->inbuf;
	size_t left = buffer->bufsize - buffer->inbuf;

	// buffer full case
	if (left == 0) {
		DEBUG_PRINT("buffer full");
		return 0;
	}

	// read as much as possible
	ssize_t bytes_read = read(input_fd, head, left);

	// check for error
	if (bytes_read < 0) {
		DEBUG_PRINT("read fail");
		return -errno;
	} else if (bytes_read == 0) {
		DEBUG_PRINT("input closed");
		return bytes_read;
	}

	// increment buffer's written space
	buffer->inbuf += bytes_read;

	DEBUG_PRINT("read %zd", bytes_read);
	return bytes_read;
}

int read_data(struct client *cli, struct packet *pack, size_t remaining) {
	// check valid inputs
	INVAL_CHECK(cli == NULL || pack == NULL || remaining < 0);

	// calculate how many buffers will hold all the incoming data
	size_t buffers = remaining / cli->window + (remaining % cli->window != 0);

	// loop as many times as new buffers are needed
	size_t total = 0;
	size_t expected;
	ssize_t bytes_read;
	struct buffer *receive;
	for (size_t i = 0; i < buffers; i++) {

		// allocate space to hold incoming data
		if (append_buffer(pack, cli->window, &receive) < 0) {
			DEBUG_PRINT("fail allocate buffer %zu", i);
			return -ENOMEM;
		}

		// read expected bytes per data segment
		expected = (receive->bufsize > remaining) ? remaining : receive->bufsize;
		bytes_read = force_read(cli->socket_fd, receive->buf, expected);
		if (bytes_read != expected) {
			// in case read isn't perfect
			if (bytes_read < 0) {
				DEBUG_PRINT("failed data read");
				return -errno;
			} else if (bytes_read == 0) {
				DEBUG_PRINT("failed data read, socket closed");
				return -ENETDOWN;
			} else {
				DEBUG_PRINT("incomplete data read, %zd remaining", expected - bytes_read);
				return -EINTR;
			}
		}

		// update tracker fields
		remaining -= bytes_read;
		total += bytes_read;
		receive->inbuf = bytes_read;
	}

	DEBUG_PRINT("data section received, length %zu, segments %zu", total, buffers);
	return total;
}

int read_header(struct client *cli, struct packet *pack) {
    // precondition for invalid argument
    INVAL_CHECK(cli == NULL || pack == NULL);

    // buffer to receive header, and converted pointer
    struct packet_header header;
    char *header_ptr = (char *) &header;

    // read packet from client
    size_t expected = sizeof(struct packet_header);
    ssize_t bytes_read = force_read(cli->socket_fd, header_ptr, expected);
    if (bytes_read != expected) {
        // in case read isn't perfect
        if (bytes_read < 0) {
            DEBUG_PRINT("failed header read");
            return -errno;
        } else if (bytes_read == 0) {
            DEBUG_PRINT("failed header read, socket closed");
            return -ENETDOWN;
        } else {
            DEBUG_PRINT("incomplete header read, %zd remaining", expected - bytes_read);
            return -EINTR;
        }
    }

    // move buffer to packet fields
    pack->header = header;

    // print incoming header // TODO: centralize header printing
    DEBUG_PRINT(dbg_pack, pack->header.head, pack->header.status,
								pack->header.control1, pack->header.control2);

    // TODO: this is very platform dependent
    DEBUG_PRINT("read header, style %d, width %zd", packet_style(pack), bytes_read);
    return 0;
}

int write_packet(struct client *cli, struct packet *pack) {
    // precondition for invalid arguments
    INVAL_CHECK(cli == NULL || pack == NULL);

    // assemble writing variables
    size_t head_expected = sizeof(struct packet_header);
    const char *header_ptr = (const char *) &(pack->header);

    // write header packet to target
    ssize_t head_written = force_write(cli->socket_fd, header_ptr, head_expected);
    if (head_written != head_expected) {
        // in case write was not perfectly successful
        if (head_written < 0) {
            DEBUG_PRINT("failed header write");
            return -errno;
        } else if (head_written == 0) {
            DEBUG_PRINT("failed header write, socket closed");
            return -ENETDOWN;
        } else {
            DEBUG_PRINT("incomplete header write, %zd remaining", head_expected - head_written);
            return -EINTR;
        }
    }

    // loop through all buffers in packet's data section
    size_t total = 0;
    size_t buffer_index = 0;
    size_t data_expected;
    ssize_t data_written;
    struct buffer *segment;
    for (segment = pack->data; segment != NULL; segment = segment->next) {

        // write buffer to target (if any)
		data_expected = segment->inbuf;
        data_written = force_write(cli->socket_fd, segment->buf, data_expected);
        if (data_written != data_expected) {
            // in case write was not perfectly successful
            if (data_written < 0) {
                DEBUG_PRINT("failed data write segment %zu", buffer_index);
                return -errno;
            } else if (data_written == 0) {
                DEBUG_PRINT("failed data write segment %zu, socket closed", buffer_index);
                return -ENETDOWN;
            } else {
                DEBUG_PRINT("incomplete data write segment %zu, %zu remaining", buffer_index, data_expected);
                return -EINTR;
            }
        }

        // track depth into data section, as well as total data length
        buffer_index++;
        total += data_written;
    }

    // print outgoing header // TODO: centralize header printing
	DEBUG_PRINT(dbg_pack, pack->header.head, pack->header.status,
				pack->header.control1, pack->header.control2);

    // TODO: this is very platform dependent
    DEBUG_PRINT("packet style %d, %zd bytes header, %zu bytes body", packet_style(pack), head_written, total);
    return total;
}

int find_newline(const char *buf, size_t len) {
	// check valid arguments
	INVAL_CHECK(buf == NULL || len < 1);

	// first element case
	if (buf[0] == '\n') {
		DEBUG_PRINT("unix newline at 0");
		return 0;
	}

	// loop through buffer until first newline is found
	size_t index;
	for (index = 1; index < len; index++) {
		// network newline
		if (buf[index - 1] == '\r' && buf[index] == '\n') {
			DEBUG_PRINT("network newline at %zu", index);
			return index;
		}

		// unix newline
		if (buf[index - 1] != '\r' && buf[index] == '\n') {
			DEBUG_PRINT("unix newline at %zu", index);
			return index;
		}
	}

	// nothing found
	DEBUG_PRINT("no newline found");
	return -ENOENT;
}

int remove_newline(char *buf, size_t len) {
	// check valid arguments
	INVAL_CHECK(buf == NULL || len < 1);

	// first element case
	if (buf[0] == '\n') {
		DEBUG_PRINT("unix newline at 0");
		buf[0] = '\0';
		return 0;
	}

	// loop through buffer until first newline is found
	for (size_t index = 1; index < len; index++) {
		// network newline
		if (buf[index - 1] == '\r' && buf[index] == '\n') {
			DEBUG_PRINT("network newline at %zu", index);
			buf[index - 1] = '\0';
			buf[index] = '\0';
			return index;
		}

		// unix newline
		if (buf[index - 1] != '\r' && buf[index] == '\n') {
			DEBUG_PRINT("unix newline at %zu", index);
			buf[index] = '\0';
			return index;
		}
	}

	// nothing found
	DEBUG_PRINT("no newline found");
	return -ENOENT;
}

int buf_contains_symbol(const char *buf, size_t len, char symbol) {
	// check valid arguments
	INVAL_CHECK(buf == NULL || len < 1);

	// loop through buffer until first symbol is found
	for (size_t index = 0; index < len; index++) {
		if (buf[index] == symbol) {
			DEBUG_PRINT("char %hhd found at %zu", symbol, index);
			return index;
		}
	}

	// no symbol found
	DEBUG_PRINT("symbol %hd not found", symbol);
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

int assemble_data(struct packet *pack, const char *buf, size_t buf_len, size_t fragment_size) {
	INVAL_CHECK(pack == NULL || buf == NULL || buf_len < 0 || fragment_size < 0);

	// calculate how many buffers will hold all the data
	size_t buffers = buf_len / fragment_size + (buf_len % fragment_size != 0);

	// create and fill the calculated number of buffers
	size_t expected;
	size_t remaining = buf_len;
	const char *depth = buf;
	struct buffer *receive;
	for (size_t i = 0; i < buffers; i++) {

		// allocate space to hold segment
		if (append_buffer(pack, fragment_size, &receive) < 0) {
			DEBUG_PRINT("fail allocate buffer %zu", i);
			return -ENOMEM;
		}

		// copy expected bytes to buffer
		expected = (receive->bufsize > remaining) ? remaining : receive->bufsize;
		memmove(receive->buf, depth, expected);

		// update progress variables
		remaining -= expected;
		depth += expected;
		receive->inbuf = expected;
	}

	// sanity check, make sure everything is moved
	if (depth != (buf + buf_len) || remaining != 0) {
		ssize_t leftover = buf_len - remaining;
		ptrdiff_t ptrdiff = buf - depth;
		DEBUG_PRINT("failed to move data, %zd leftover, %td bytes behind", leftover, ptrdiff);
		return -1; // TODO: find proper errno to return
	}

	return buffers;
}

int consolidate_packet(struct packet *pack) {
	// check valid inputs
	INVAL_CHECK(pack == NULL);

	// loop through the segments for the total amount of data
	size_t total = sizeof(struct packet_header);
	struct buffer *cur;
	for (cur = pack->data; cur != NULL; cur = cur->next) {
		total += cur->inbuf;
	}

	// create destination for packet
	struct buffer *large;
	int alloc = init_buffer(&large, total);
	if (alloc < 0) {
		return alloc;
	}

	// mark destination elements;
	large->inbuf = total;

	// copy body segments' data into large buffer in-order
	char *depth = large->buf;
	for (cur = pack->data; cur != NULL; cur = cur->next) {
		memmove(depth, cur->buf, cur->inbuf);
		depth += cur->inbuf;
	}

	// replace existing segment chain with large segment
	empty_packet(pack);
	pack->data = large;

	return 0;
}

int fragment_packet(struct packet *pack, size_t window) {
	INVAL_CHECK(pack == NULL || window < 1);
	INVAL_CHECK(pack->datalen < 1);

	// declare buffer to hold first segment of oversize buffers
	char working_buf[window];

	size_t total_frag = 0;
	struct buffer *cur;
	for (cur = pack->data; cur != NULL; cur = cur->next) {
		size_t cur_size = cur->bufsize;
		//size_t cur_fill = cur->inbuf; // TODO: have fragmenting performed for only occupied data

		// calculate how many smaller fragments are required
		size_t whole = cur_size / window;
		size_t overf = cur_size % window;
		size_t frag_count = whole + (overf != 0);

		// check for valid math (catches malformed segments)
		if (frag_count < 1) {
			DEBUG_PRINT("fragment calculation failed, buffer %zd is %zu bytes long", pack->datalen, cur_size);
			continue;
		}

		// check if we need to fragment this segment
		if (frag_count < 2) {
			continue;
		}

		// pretend data segment ends at oversize buffer, append_buffer will add fragments to next
		struct buffer *save_next = cur->next;
		cur->next = NULL;

		// pretend first buffer's data is already copied
		cur_size -= window;

		// insert fragments ahead of oversize buffer, copy oversize data into fragments
		size_t new_segments = 0;
		struct buffer *new_buf;
		for (size_t i = 1; i < frag_count; i++) {
			// place head at proper overflow segment
			char *head = cur->buf + (window * i);

			// append fragment to pretend data section
			if (append_buffer(pack, window, &new_buf) < 0) {
				DEBUG_PRINT("failed fragment in-progress");
				return -errno;
			}

			// move some overflow data
			size_t expected = (new_buf->bufsize > cur_size) ? cur_size : new_buf->bufsize;
			memmove(new_buf->buf, head, expected);

			// update tracker variable
			cur_size -= window;
			new_segments++;
		}

		// resize oversize buffer, copy first segment data
		memmove(working_buf, cur->buf, window);
		realloc_buffer(cur, window);
		memmove(cur->buf, working_buf, window);

		// skip to the end of the new fragments, attach the rest of the data segment (stop pretending)
		cur = new_buf;
		cur->next = save_next;

		// update tracker values
		total_frag += frag_count;
	}

	return 0;
}

int append_buffer(struct packet *pack, size_t buffer_len, struct buffer **out) {
	// check valid argument
	INVAL_CHECK(pack == NULL || buffer_len < 0);

	struct buffer *container;
	if (init_buffer(&container, buffer_len) < 0) {
		DEBUG_PRINT("failed buffer init");
		return -ENOMEM;
	}

	// data section is empty
	if (pack->data == NULL) {
		// place new segment in data section
		pack->data = container;
	} else {
		// loop to end of data section
		struct buffer *cur;
		for (cur = pack->data; cur->next != NULL; cur = cur->next);

		// append new empty data segment
		cur->next = container;
	}

	// return reference to new buffer
	if (out != NULL) {
		*out = container;
	}

	pack->datalen++;
	return 0;
}

int force_read(int input_fd, char *buffer, size_t incoming) {
	// check valid inputs
	INVAL_CHECK(input_fd < MIN_FD || buffer == NULL || incoming < 0);

	// keep attempting to read all the data we expect
	size_t received = 0;
	ssize_t bytes_read;
	while (incoming > 0) {
		bytes_read = read(input_fd, buffer + received, incoming);
		if (bytes_read < 0) {
			// error encountered while reading
			DEBUG_PRINT("failed force %zu bytes in", incoming);
			return -errno;
		} else if (bytes_read == 0) {
			// end of file or fd closed
			DEBUG_PRINT("end reached, %zu bytes unforced", incoming);
			return received;
		}

		received += bytes_read;
		incoming -= bytes_read;
	}

	return received;
}

int force_write(int output_fd, const char *buffer, size_t outgoing) {
	// check valid inputs
	INVAL_CHECK(output_fd < MIN_FD || buffer == NULL || outgoing < 0);

	size_t sent = 0;
	ssize_t bytes_written;
	while (outgoing > 0) {
		bytes_written = write(output_fd, buffer + sent, outgoing);
		if (bytes_written < 0) {
			// error encountered while writing
			DEBUG_PRINT("failed force %zu bytes out", outgoing);
			return -errno;
		} else if (bytes_written == 0) {
			// fd closed
			DEBUG_PRINT("output closed, %zu bytes unforced", outgoing);
			return sent;
		}

		sent += bytes_written;
		outgoing -= bytes_written;
	}

	return sent;
}

int packet_style(struct packet *pack) {
	// check valid argument
	if (pack == NULL) {
		DEBUG_PRINT("invalid arguments");
		return 0;
	}

	// declare on-stack container for header and convert
	struct packet_header raw;
	int style;

	// copy packet header and convert to value
	raw = pack->header;
	int *raw_ptr = (int *) &raw;
	style = *raw_ptr;

	return style;
}