#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include "chopconst.h"
#include "chopdata.h"
#include "chopdebug.h"
#include "choppacket.h"

/*
* Sending functions
*/

int write_packet_to_client(struct client *cli, struct packet *pack) {
  // precondition for invalid arguments
  if (cli == NULL || pack == NULL) {
    DEBUG_PRINT("invalid arguments");
    return 1;
  }

  // mark client outgoing flag with status
  cli->out_flag = pack->status;

  // assemble header for sending
  char header[] = {pack->head, pack->status, pack->control1, pack->control2};

  // write header packet to target
  int head_written = write(cli->socket_fd, header, HEADER_LEN);
  if (head_written != HEADER_LEN) {

    // in case write was not perfectly successful
    if (head_written < 0) {
      DEBUG_PRINT("failed header write");
      return 1;
    } else if (head_written == 0) {
      DEBUG_PRINT("socket closed");
      return 1;
    } else {
      DEBUG_PRINT("incomplete header write");
      return 1;
    }
  }

  // loop through all buffers in packet's data section
  int total = 0;
  int tracker = 0;
  int bytes_written;
  struct buffer *segment;
  for (segment = pack->data; segment != NULL; segment = segment->next) {

    // write buffer to target (if any)
    bytes_written = write(cli->socket_fd, segment->buf, segment->inbuf);
    if (bytes_written != segment->inbuf) {

      // in case write was not perfectly successful
      if (bytes_written < 0) {
        DEBUG_PRINT("failed data write, segment %d", tracker);
        return 1;
      } else if (bytes_written == 0) {
        DEBUG_PRINT("socket closed");
        return 1;
      } else {
        DEBUG_PRINT("incomplete data write, segment %d", tracker);
        return 1;
      }
    }

    // track depth into data section, as well as total data length
    tracker++;
    total += bytes_written;
  }

  // demark client outgoing flag
  cli->out_flag = 0;

  // TODO: this is very platform dependent
  DEBUG_PRINT("packet style %d, %d bytes body", packet_style(pack), total);
  return 0;
}

/*
* Receiving Functions
*/

int read_header(struct client *cli, struct packet *pack) {
  // precondition for invalid argument
  if (cli == NULL || pack == NULL) {
    DEBUG_PRINT("invalid argument");
    return 1;
  }

  // buffer to receive header
  char header[HEADER_LEN];

  // read packet from client
  int head_read = read(cli->socket_fd, header, HEADER_LEN);
  if (head_read != HEADER_LEN) {

    // in case read isn't perfect
    if (head_read < 0) {
      DEBUG_PRINT("failed header read");
      return 1;
    } else if (head_read == 0) {
      DEBUG_PRINT("socket closed");
      return 1;
    } else {
      DEBUG_PRINT("incomplete header read");
      return 1;
    }
  }

  // move buffer to packet fields
  if (split_header(pack, header) > 0) {
    DEBUG_PRINT("failed header assembly");
    return 1;
  }

  // TODO: this is very platform dependent
  DEBUG_PRINT("read header, style %d", packet_style(pack));

	char *headb = char_to_bin(pack->head);
	char *statb = char_to_bin(pack->status);
	char *cont1b = char_to_bin(pack->control1);
	char *cont2b = char_to_bin(pack->control2);
	DEBUG_PRINT("header bits: \"%s:%s:%s:%s\"", headb, statb, cont1b, cont2b);
  return 0;
}

int parse_header(struct client *cli, struct packet *pack) {
  // precondition for invalid arguments
  if (cli == NULL || pack == NULL) {
    DEBUG_PRINT("invalid arguments");
    return 1;
  }

  // parse the status
  int status;
  switch (pack->status) {
    case NULL_BYTE:
    	DEBUG_PRINT("received NULL header");
    	status = 0;
    	break;

    case START_HEADER:
    	DEBUG_PRINT("received extended header");
    	status = parse_long_header(cli, pack);
    	break;

    case START_TEXT:
    	DEBUG_PRINT("received text header");
    	status = parse_text(cli, pack);

    	// print incoming text
    	if (print_text(cli, pack) > 0) {
      	DEBUG_PRINT("failed print");
      	return 1;
    	}
    	break;

    case ENQUIRY:
    	DEBUG_PRINT("received enquiry header");
    	status = parse_enquiry(cli, pack);

    	// print incoming enquiry
    	if (print_enquiry(cli, pack) > 0) {
      	DEBUG_PRINT("failed print");
      	return 1;
    	}
    	break;

    case ACKNOWLEDGE:
    	DEBUG_PRINT("received acknowledge header");
    	status = parse_acknowledge(cli, pack);

    	// print incoming acknowledge
    	if (print_acknowledge(cli, pack) > 0) {
      	DEBUG_PRINT("failed print");
      	return 1;
    	}
    	break;

    case WAKEUP:
    	DEBUG_PRINT("received wakeup header");
    	status = parse_wakeup(cli, pack);

    	// print incoming wakeup
    	if (print_wakeup(cli, pack) > 0) {
      	DEBUG_PRINT("failed print");
      	return 1;
    	}
    	break;

    case NEG_ACKNOWLEDGE:
    	DEBUG_PRINT("received negative acknowledge header");
    	status = parse_neg_acknowledge(cli, pack);

    	// print incoming negative acknowledge
    	if (print_neg_acknowledge(cli, pack) > 0) {
      	DEBUG_PRINT("failed print");
      	return 1;
    	}
    	break;

    case IDLE:
    	DEBUG_PRINT("received idle header");
    	status = parse_idle(cli, pack);

    	// print incoming idle
    	if (print_idle(cli, pack) > 0) {
      	DEBUG_PRINT("failed print");
      	return 1;
    	}
    	break;

    case ESCAPE:
    	DEBUG_PRINT("received escape header");
    	status = parse_escape(cli, pack);

    	// print incoming escape
    	if (print_escape(cli, pack) > 0) {
      	DEBUG_PRINT("failed print");
      	return 1;
    	}
    	break;

    default: // unsupported/invalid
    	DEBUG_PRINT("received invalid header");
    	status = 1;
    	break;
  }

  return status;
}

int parse_long_header(struct client *cli, struct packet *pack) {
  // precondition for invalid arguments
  if (cli == NULL || pack == NULL) {
    DEBUG_PRINT("invalid arguments");
    return 1;
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
    DEBUG_PRINT("invalid argument");
    return 1;
  }

  // translate names for readability
  int count = pack->control1;
  int width = pack->control2;

  // signals set to unknown length read
  if (count == 0 && width == 0) {
    int buffers = 0;
    int long_len;
    char *head = read_long_text(cli, pack, &long_len, &buffers);
    if (head == NULL) {
      DEBUG_PRINT("failed long text parsing");
      if (long_len < 0) {
        return 1; // error
      } else {
        return 1; // socket closed
      }
    }

    DEBUG_PRINT("long text section length %d, %d segments", long_len, buffers);
    return 0;
  } else {

		// read normally
		int received;
		int remaining = count * width;
		if (read_data(cli, pack, remaining, &received) > 0) {
			return 1;
		}
	}

  return 0;
}

char *read_long_text(struct client *cli, struct packet *pack, int *len_ptr, int *buffers) {
  // precondition for invalid arguments
  if (cli == NULL || pack == NULL || len_ptr == NULL || buffers == NULL) {
    DEBUG_PRINT("invalid arguments");
    return NULL;
  }

  struct buffer *receive;
  if (append_buffer(pack, cli->window, &receive) > 0) {
    *len_ptr = -1;
    return NULL;
  }
  *buffers = *buffers + 1;

  int bytes_read = read(cli->socket_fd, receive->buf, cli->window);
  if (bytes_read < 0) {
    DEBUG_PRINT("failed to read long text section");
    *len_ptr = 0;
    return NULL;
  }

  // check if new data contains a stop
  char *ptr;
  int stop_len;
  if (buf_contains_symbol(receive->buf, bytes_read, END_TEXT, &stop_len) > 0) {
    // alloc heap to store the data
    ptr = (char *) malloc(stop_len);

    // move data onto heap, notify parent fn of length of heap data
    memmove(ptr, receive->buf, stop_len);
    *len_ptr = stop_len;
  } else {
    // recurse for rest of the data
    int sub_len;
    char *nptr = read_long_text(cli, pack, &sub_len, buffers);

    // alloc memory to hold all the data
    ptr = (char *) malloc(bytes_read + sub_len);

    // move data from here in
    memmove(ptr, receive->buf, bytes_read);

    // move data from recursion in
    memmove(ptr + bytes_read, nptr, sub_len);

    // notify parent fn of length of heap data
    *len_ptr = bytes_read + sub_len;
  }

  return ptr;
}

int parse_enquiry(struct client *cli, struct packet *pack) {
  if (cli == NULL || pack == NULL) {
    DEBUG_PRINT("invalid arguments");
    return 1;
  }

  // initalize packet for return
  struct packet *out;
  if (init_packet_struct(&out) > 0) {
    DEBUG_PRINT("failed init packet");
    return 1;
  }

	int head = 0;
	int status = 0;
	int control1 = 0;
	int control2 = 0;

  time_t current;
  struct buffer *buf;
  switch(pack->control1) {
    case ENQUIRY_NORMAL:
    	DEBUG_PRINT("normal enquiry");
    	// just return an acknowledge
			status = ACKNOWLEDGE;
			control1 = ENQUIRY;
    	break;

    case ENQUIRY_RETURN:
    	DEBUG_PRINT("return enquiry");
    	// return a enquiry signal 0
			status = ENQUIRY;
    	break;

    case ENQUIRY_TIME:
    	DEBUG_PRINT("time enquiry");
    	// return an acknowledge
			status = ACKNOWLEDGE;
			control1 = ENQUIRY;

			// read packet data
			if (read_data(cli, pack, pack->control2, NULL) > 0) {
				DEBUG_PRINT("failed time data read");
				return 1;
			}
    	break;

    case ENQUIRY_RTIME:
    	DEBUG_PRINT("time request");
    	// return time enquiry packet
			status = ENQUIRY;
			control1 = ENQUIRY_TIME;
			control2 = sizeof(time_t);

    	// allocate data for time section
    	if (append_buffer(out, sizeof(time_t), &buf) > 0) {
      	DEBUG_PRINT("failed data expansion");
      	return 1;
    	}

    	// place time in data section
    	current = time(NULL); // convert time_t to char *
    	if (assemble_body(buf, (char *) &current, sizeof(time_t)) > 0) {
      	DEBUG_PRINT("failed body assemble");
      	return 1;
    	}
    	break;

    default:
    	DEBUG_PRINT("invalid/unsupported control signal");
    	return 1;
  }

	// setup header for return packet
	if (assemble_header(out, head, status, control1, control2) > 0) {
		DEBUG_PRINT("failed header assemble");
		return 1;
	}

  // write to client
  if (write_packet_to_client(cli, out) > 0) {
    DEBUG_PRINT("failed write");
    return 1;
  }

  // destroy allocated packet
  if (destroy_packet_struct(&out) > 0) {
    DEBUG_PRINT("failed packet destroy");
    return 1;
  }

  return 0;
}

int parse_acknowledge(struct client *cli, struct packet *pack) {
  // precondition for invalid argument
  if (cli == NULL || pack == NULL) {
    DEBUG_PRINT("invalid arguments");
    return 1;
  }

  switch (pack->control1) {
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
    	// TOOD: the sender knows you're stopping
    	DEBUG_PRINT("escape confirmed");
    	// marking this client as closed
    	cli->inc_flag = CANCEL;
    	cli->out_flag = CANCEL;
    	break;

		default:
			DEBUG_PRINT("invalid/unsupported control signal");
			return 1;
  }

  return 0;
}

int parse_wakeup(struct client *cli, struct packet *pack) {
  // check valid arguments
  if (cli == NULL || pack == NULL) {
    DEBUG_PRINT("invalid arguments");
    return 1;
  }

  // initalize packet for return
  struct packet *out;
  if (init_packet_struct(&out) > 0) {
    DEBUG_PRINT("failed init packet");
    return 1;
  }

  // setting values of header
  if (cli->inc_flag == IDLE) {
    if (assemble_header(out, 0 , ACKNOWLEDGE, WAKEUP, 0) > 0) {
      DEBUG_PRINT("failed header assemble");
      return 1;
    }

    // demark incoming client flag
    cli->inc_flag = NULL_BYTE;
    DEBUG_PRINT("client %d now awake", cli->socket_fd);
  } else {
    if (assemble_header(out, 0 , NEG_ACKNOWLEDGE, WAKEUP, 0) > 0) {
      DEBUG_PRINT("failed header assemble");
      return 1;
    }

    DEBUG_PRINT("client %d awake, refusing", cli->socket_fd);
  }

  // write to client
  if (write_packet_to_client(cli, out) > 0) {
    DEBUG_PRINT("failed write");
    return 1;
  }

  // destroy allocated packet
  if (destroy_packet_struct(&out) > 0) {
    DEBUG_PRINT("failed packet destroy");
    return 1;
  }

  return 0;
}

int parse_neg_acknowledge(struct client *cli, struct packet *pack) {
  // precondition for invalid argument
  if (cli == NULL || pack == NULL) {
    DEBUG_PRINT("invalid arguments");
    return 1;
  }

  switch (pack->control1) {
    case START_TEXT:
    // TODO: text arguments are invalid/message is invalid
    DEBUG_PRINT("client %d refused text", cli->socket_fd);
    break;

    case ENQUIRY:
    // TODO: ping refused
    DEBUG_PRINT("client %d refused ping", cli->socket_fd);
    break;

    case WAKEUP:
    // TODO: sender is already awake/cannot wakeup
    DEBUG_PRINT("client %d refused wakeup", cli->socket_fd);
    break;

    case IDLE:
    // TODO: sender is already sleeping/cannot sleep
    DEBUG_PRINT("client %d refused idle", cli->socket_fd);
    break;

    case ESCAPE:
    // TODO: you cannot disconnect
    DEBUG_PRINT("client %d refused disconnect", cli->socket_fd);
    break;

  }

  return 0;
}

int parse_idle(struct client *cli, struct packet *pack) {
  // precondition for invalid argument
  if (cli == NULL || pack == NULL) {
    DEBUG_PRINT("invalid arguments");
    return 1;
  }

  // initalize packet for return
  struct packet *out;
  if (init_packet_struct(&out) > 0) {
    DEBUG_PRINT("failed init packet");
    return 1;
  }

  // setting values of header
  if (cli->inc_flag == NULL_BYTE) {
    if (assemble_header(out, 0 , ACKNOWLEDGE, IDLE, 0) > 0) {
      DEBUG_PRINT("failed header assemble");
      return 1;
    }

    // mark client as sleeping on uncoming channel
    cli->inc_flag = IDLE;
    //cli->out_flag = IDLE;
    DEBUG_PRINT("client %d now sleeping", cli->socket_fd);
  } else {
    if (assemble_header(out, 0 , NEG_ACKNOWLEDGE, IDLE, 0) > 0) {
      DEBUG_PRINT("failed header assemble");
      return 1;
    }

    DEBUG_PRINT("client %d busy, refusing", cli->socket_fd);
  }

  // write to client
  if (write_packet_to_client(cli, out) > 0) {
    DEBUG_PRINT("failed write");
    return 1;
  }

  // destroy allocated packet
  if (destroy_packet_struct(&out) > 0) {
    DEBUG_PRINT("failed packet destroy");
    return 1;
  }

  return 0;
}

int parse_escape(struct client *cli, struct packet *pack) {
  // precondition for invalid argument
  if (cli == NULL || pack == NULL) {
    DEBUG_PRINT("invalid arguments");
    return 1;
  }

  // initalize packet for return
  struct packet *out;
  if (init_packet_struct(&out) > 0) {
    DEBUG_PRINT("failed init packet");
    return 1;
  }

  // setting values of header
  if (assemble_header(out, 0 , ACKNOWLEDGE, ESCAPE, 0) > 0) {
    DEBUG_PRINT("failed header assemble");
    return 1;
  }

  DEBUG_PRINT("shutting down client %d connection", cli->socket_fd);

  // write to client
  if (write_packet_to_client(cli, out) > 0) {
    DEBUG_PRINT("failed write");
    return 1;
  }

  // destroy allocated packet
  if (destroy_packet_struct(&out) > 0) {
    DEBUG_PRINT("failed packet destroy");
    return 1;
  }

  // marking this client as closed
  cli->inc_flag = CANCEL;
  cli->out_flag = CANCEL;

  return 0;
}

/*
* Packet Utility Functions
*/

int assemble_header(struct packet *pack, int head, int status, int control1, int control2) {
  // check valid arguments
  if (pack == NULL || head < 0 || status < 0 || control1 < 0 || control2 < 0) {
    DEBUG_PRINT("invalid arguments");
    return 1;
  }

  // assemble packet header with given values
  pack->head = (char) head;
  pack->status = (char) status;
  pack->control1 = (char) control1;
  pack->control2 = (char) control2;

  return 0;
}

int split_header(struct packet *pack, char *header) {
  // check valid arguments
  if (pack == NULL) {
    DEBUG_PRINT("invalid arguments");
    return 1;
  }

  // grab header from given address
  pack->head = *header;
  pack->status = *(header + 1);
  pack->control1 = *(header + 2);
  pack->control2 = *(header + 3);

  return 0;
}

int assemble_body(struct buffer *buffer, const char *data, const int len) {
  // check valid arguments
  if (buffer == NULL || data == NULL || len < 0) {
    DEBUG_PRINT("invalid arguments");
    return 1;
  }

  // cannot fit message in buffer
  if (len > buffer->bufsize) {
    DEBUG_PRINT("message too large");
    return 1;
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
    return 1;
  }

  // data section is empty
  if (pack->data == NULL) {
    if (init_buffer_struct(&(pack->data), bufsize) > 0) {
      DEBUG_PRINT("failed buffer init");
      return 1;
    }
  }

  // loop to end of data section
  struct buffer *cur;
  for (cur = pack->data; cur->next != NULL; cur = cur->next);

  // append new empty data segment
  if (init_buffer_struct(&(cur->next), bufsize) > 0) {
    DEBUG_PRINT("failed buffer init");
    return 1;
  }

  // return reference to new buffer
  if (out != NULL) {
    *out = cur->next;
  }

  return 0;
}

int packet_style(struct packet *pack) {
  // check valid argument
  if (pack == NULL) {
    DEBUG_PRINT("invalid arguments");
    return 0;
  }

  return *(int *)pack;
}

int print_text(struct client *client, struct packet *pack) {
  // check valid arguments
  if (client == NULL || pack == NULL || pack->status != START_TEXT) {
    DEBUG_PRINT("invalid arguments");
    return 1;
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
    return 1;
  }

  switch(pack->control1) {
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
    return 1;
  }

  // make sure incoming packet is time packet
  if (pack->status != ENQUIRY || pack->control1 != ENQUIRY_TIME) {
    DEBUG_PRINT("invalid packet type");
    return 1;
  }

  // grab first buffer in packet, convert char * to time_t
  //struct buffer *buf = pack->data;
  //time_t *container = (time_t *) buf->buf;
  //time_t message = *container;

  // print time message
  printf(msg_header(), client->socket_fd);
  printf(recv_ping_time, *(long int *) pack->data->buf);

  return 0;
}

int print_acknowledge(struct client *client, struct packet *pack) {
  // check valid arguments
  if (client == NULL || pack == NULL) {
    DEBUG_PRINT("invalid arguments");
    return 1;
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
    return 1;
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
    return 1;
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
    return 1;
  }

	// print idle contents
	printf(msg_header(), client->socket_fd);
	printf(idle_text);

  return 0;
}

int print_escape(struct client *client, struct packet *pack) {
  // check valid arguments
  if (client == NULL || pack == NULL) {
    DEBUG_PRINT("invalid arguments");
    return 1;
  }

	// print escape contents
	printf(msg_header(), client->socket_fd);
	printf(esc_text);

  return 0;
}
