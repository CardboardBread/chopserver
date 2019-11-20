#include <stdlib.h>
#include <unistd.h>

#include "chopconst.h"
#include "chopdata.h"
#include "chopdebug.h"
#include "choppacket.h"

int read_to_buf(struct buffer *buffer, const int input, int *received) {
	// check valid inputs
	if (buffer == NULL || input < 0) {
		DEBUG_PRINT("invalid arguments");
		return 1;
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
		return 1;
	}

	// increment buffer's written space
	buffer->inbuf += readlen;

	// report how much was read
	if (received != NULL) {
		*received = readlen;
	}

	DEBUG_PRINT("read %d", readlen);
	return 0;
}

int read_data(struct client *cli, struct packet *pack, int remaining, int *received) {
	// check valid inputs
	if (cli == NULL || pack == NULL || remaining < 0) {
		DEBUG_PRINT("invalid arguments");
		return 1;
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
		if (append_buffer(pack, cli->window, &receive) > 0) {
			DEBUG_PRINT("fail allocate buffer %d", i);
			return 1;
		}

		// read expected bytes per data segment
		expected = (receive->bufsize > remaining) ? remaining : receive->bufsize;
		bytes_read = read(cli->socket_fd, receive->buf, expected);
		if (bytes_read != expected) {

			// in case read isn't perfect
			if (bytes_read < 0) {
				DEBUG_PRINT("failed data read");
				return 1;
			} else if (bytes_read == 0) {
				DEBUG_PRINT("socket closed");
				return 1;
			} else {
				DEBUG_PRINT("incomplete data read");
				return 1;
			}
		}

		// update tracker fields
		remaining -= bytes_read;
		total += bytes_read;
		receive->inbuf = bytes_read;
	}

	// report incoming width if possible
	if (received != NULL) {
		*received = total;
	}

	DEBUG_PRINT("data section length %d, %d segments", total, buffers);
	return 0;
}

int find_newline(const char *buf, const int len, int *location) {
	// check valid arguments
	if (buf == NULL || len < 0) {
		DEBUG_PRINT("invalid arguments");
		return 1;
	}

	// first element case
	if (buf[0] == '\n') {
		DEBUG_PRINT("unix newline at 0");
		if (location != NULL) *location = 0;
		return 0;
	}

	// loop through buffer until first newline is found
	int index;
	for (index = 1; index < len; index++) {
		// network newline
		if (buf[index - 1] == '\r' && buf[index] == '\n') {
			DEBUG_PRINT("network newline at %d", index);
			if (location != NULL) *location = index; // record index if possible
			return 0;
		}

		// unix newline
		if (buf[index - 1] != '\r' && buf[index] == '\n') {
			DEBUG_PRINT("unix newline at %d", index);
			if (location != NULL) *location = index; // record index if possible
			return 0;
		}
	}

	// nothing found
	DEBUG_PRINT("none found");
	return 1;
}

int remove_newline(char *buf, const int len, int *location) {
	// check valid arguments
	if (buf == NULL || len < 0) {
		DEBUG_PRINT("invalid arguments");
		return 1;
	}

	// first element case
	if (buf[0] == '\n') {
		DEBUG_PRINT("unix newline at 0");
		buf[0] = '\0';
		if (location != NULL) *location = 0;
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
			if (location != NULL) *location = index;
			return 0;
		}

		// unix newline
		if (buf[index - 1] != '\r' && buf[index] == '\n') {
			DEBUG_PRINT("unix newline at %d", index);
			buf[index] = '\0';
			if (location != NULL) *location = index;
			return 0;
		}
	}

	// nothing found
	DEBUG_PRINT("none found");
	return 1;
}

int buf_contains_symbol(const char *buf, const int len, const char symbol, int *symbol_index) {
	// check valid arguments
	if (buf == NULL || len < 1) {
		DEBUG_PRINT("invalid arguments");
		return 1;
	}

	// loop through buffer until first symbol is found
	int index;
	for (index = 0; index < len; index++) {
		if (buf[index] == symbol) {
			if (symbol_index != NULL) *symbol_index = index;
			DEBUG_PRINT("%c found at %d", symbol, index);
			return 0;
		}
	}

	// no symbol found
	DEBUG_PRINT("none found");
	return 1;
}

char *char_to_bin(char value) {
	char *ret = (char *) malloc(sizeof(char) * 9);
	for (int i = 0; i < 8; i++) {
		if (value & (0x1 << i)) {
			ret[7 - i] = '1';
		} else {
			ret[7 - i] = '0';
		}
	}
	ret[8] = '\0';
	return ret;
}
