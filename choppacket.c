#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "chopconst.h"
#include "chopdebug.h"

int write_packet_to_client(struct client *cli, struct packet *pack) {
  // precondition for invalid arguments
  if (cli == NULL || pack == NULL) {
    DEBUG_PRINT("invalid arguments");
    return 1;
  }

  // mark client outgoing flag with status
  cli->out_flag = pack->status

  // assemble header for sending
  char header[] = {pack->head, pack->status, pack->control1, pack->control2};
  int headlen = sizeof(struct packet) - sizeof(struct buffer);

  // write header packet to target
  int head_written = write(cli->socket_fd, header, headlen);
  if (head_written != headlen) {

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
  DEBUG_PRINT("packet style %d, %d bytes written", packet_style(pack), total);
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
  DEBUG_PRINT("read header, style %d", packet_style(pack);
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
      break;

    case ENQUIRY:
      DEBUG_PRINT("received enquiry header");
      status = parse_enquiry(cli, pack);
      break;

    case ACKNOWLEDGE:
      DEBUG_PRINT("received acknowledge header");
      status = parse_acknowledge(cli, pack);
      break;

    case WAKEUP:
      DEBUG_PRINT("received wakeup header");
      status = parse_wakeup(cli, pack);
      break;

    case NEG_ACKNOWLEDGE:
      DEBUG_PRINT("received negative acknowledge header");
      status = parse_neg_acknowledge(cli, pack);
      break;

    case IDLE:
      DEBUG_PRINT("received idle header");
      status = parse_idle(cli, pack);
      break;

    case ESCAPE:
      DEBUG_PRINT("received escape header");
      status = parse_escape(cli, pack);
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
    int buffers;
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

    // print incoming message
    if (print_text(cli, pack) > 0) {
      DEBUG_PRINT("failed print");
      return 1;
    }

    DEBUG_PRINT("long text section length %d, %d segments", long_len, buffers);
    return 0;
  }

  // calculate how much data is incoming and how many buffers are required
  // to hold all the data
  int remaining = count * width;
  int buffers = remaining / cli->window + ( remaining % cli->window != 0);

  // loop as many times as new buffers are needed
  int total = 0;
  int expected;
  int bytes_read;
  struct buffer *receive;
  for (int i = 0; i < buffers; i++) {

    // allocate more space to hold data
    if (append_buffer(cli, pack, &receive) > 0) {
      DEBUG_PRINT("fail allocate buffer %d", i);
      return 1;
    }

    // read expected bytes per data segment
    expected = (receive->bufsize > remaining) ? remaining : receive->bufsize;
    bytes_read = read(cli->socket_fd, receive->buf, expected);
    if (bytes_read != expected) {

      // in case read isn't perfect
      if (head_read < 0) {
        DEBUG_PRINT("failed data read");
        return 1;
      } else if (head_read == 0) {
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
  }

  // print incoming message
  if (print_text(cli, pack) > 0) {
    DEBUG_PRINT("failed print");
    return 1;
  }

  DEBUG_PRINT("text section length %d, %d segments", total, buffers);
  return 0;
}

char *read_long_text(struct client *cli, struct packet *pack, int *len_ptr, int *buffers) {
  // precondition for invalid arguments
  if (cli == NULL || pack == NULL || len_ptr == NULL || buffers == NULL) {
    DEBUG_PRINT("invalid arguments");
    *len_ptr = -1;
    return NULL;
  }

  struct buffer *receive;
  if (append_buffer(cli, pack, &receive) > 0) {
    *len_ptr = -1;
    return NULL;
  }
  *buffers++;

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
    ptr = malloc(stop_len);

    // move data onto heap, notify parent fn of length of heap data
    memmove(ptr, receive->buf, stop_len);
    *len_ptr = stop_len;
  } else {
    // recurse for rest of the data
    int sub_len;
    char *nptr = read_long_text(cli, pack, &sub_len, buffers);

    // alloc memory to hold all the data
    ptr = malloc(bytes_read + sub_len);

    // move data from here in
    memmove(ptr, buffer, bytes_read);

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

  time_t current;
  struct buffer *buf;
  switch(pack->control1) {
    case ENQUIRY_NORMAL:
      DEBUG_PRINT("normal enquiry");
      // just return an acknowledge
      if (assemble_header(out, 0 , ACKNOWLEDGE, ENQUIRY, 0) > 0) {
        DEBUG_PRINT("failed header assemble");
        return 1;
      }

    case ENQUIRY_RETURN:
      DEBUG_PRINT("return enquiry");
      // return a enquiry signal 0
      if (assemble_header(out, 0 , ENQUIRY, 0, 0) > 0) {
        DEBUG_PRINT("failed header assemble");
        return 1;
      }

    case ENQUIRY_TIME:
      DEBUG_PRINT("time enquiry");
      // parse time in data section
      if (print_time(cli, pack) > 0) {
        DEBUG_PRINT("failed time print");
        return 1;
      }

      // return an acknowledge
      if (assemble_header(out, 0 , ACKNOWLEDGE, ENQUIRY, 0) > 0) {
        DEBUG_PRINT("failed header assemble");
        return 1;
      }

    case ENQUIRY_RTIME:
      DEBUG_PRINT("time request");
      // return time enquiry packet
      if (assemble_header(out, 0 , ENQUIRY, ENQUIRY_TIME, sizeof(time_t)) > 0) {
        DEBUG_PRINT("failed header assemble");
        return 1;
      }

      // allocate data for time section
      if (append_buffer(out, sizeof(time_t), &buf) > 0) {
        DEBUG_PRINT("failed data expansion")
        return 1;
      }

      // place time in data section
      current = time(NULL); // convert time_t to char *
      if (assemble_body(buf, (char *) &current, sizeof(time_t)) > 0) {
        DEBUG_PRINT("failed body assemble");
        return 1;
      }

    default:
      DEBUG_PRINT("invalid control signal");
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
      printf("pong\n");
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

    // mark client as sleeping on both channels
    cli->inc_flag = IDLE;
    cli->out_flag = IDLE;
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
   if (client == NULL || pack == NULL) {
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

 int extend_buffer(struct buffer *buffer, int additional) {
   // check valid arguments
   if (client == NULL || additional < 0) {
     DEBUG_PRINT("invalid arguments");
     return 1;
   }

   // add new buffer structs onto the end of the existing buffer in client
   // buffers are all of the same size
   for (int i = 0; i < additional; i++) {

     // append new buffer
     if (init_buffer_struct(&(buffer->next), bufsize) > 0) {
       DEBUG_PRINT("failed buffer extension o %d", i);
       return 1;
     }

     // move target to new buffer
     buffer = buffer->next;
   }

   return 0;
 }

 int packet_style(struct packet *pack) {
   // check valid argument
   if (packet == NULL) {
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
   printf("[CLIENT %d]: \"", client->socket_fd);

   // print every buffer out sequentially
   struct buffer *cur;
   for (cur = pack->data; cur != NULL; cur = cur->next) {
     printf("%.*s", cur->inbuf, cur->buf);
   }

   // print line end for spacing
   printf("\"\n");

   return 0;
 }

 int print_time(struct client *client, struct packet *pack) {
   // check valid arguments
   if (client == NULL || pack == NULL || pack->data == NULL) {
     DEBUG_PRINT("invalid arguments");
     return 1;
   }

   // grab first buffer in packet, convert char * to time_t
   struct buffer *buf = pack->data;
   time_t message = *((time_t *) buf->buf);

   // print time message
   printf("[CLIENT %d]: Reported Time \"%ld\"\n", client->socket_fd, message);

   return 0;
 }
