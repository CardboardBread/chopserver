#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <stdarg.h>

#include "socket.h"
#include "chophelper.h"

/*
 * Client Management functions
 */

int setup_new_client(const int listen_fd, Client *clients[]) {
  // precondition for invalid arguments
  if (listen_fd < MIN_FD || clients == NULL) {
    debug_print("setup_new_client: invalid arguments");
    return -1;
  }

  // blocking until client connects
  int fd = accept_connection(listen_fd);
  if (fd < MIN_FD) {
    debug_print("setup_new_client: failed to accept new client");
    return -1;
  }
  debug_print("setup_new_client: accepted new client on %d", fd);

  // finding an 'empty' space in the client array
  for (int i = 0; i < MAX_CONNECTIONS; i++) {

    // placing new client in empty space
    if (clients[i] == NULL) {
      clients[i] = (Client *) malloc(sizeof(Client));
      clients[i]->buffer = (Buffer *) malloc(sizeof(Buffer));
      reset_client_struct(clients[i]);
      clients[i]->socket_fd = fd;
      debug_print("setup_new_client: placed client at index %d", i);
      return fd;
    }
  }

  // close the connection since we can't store it
  close(fd);
  debug_print("setup_new_client: no empty space for client found, closing connection");
  return -1;
}

int establish_server_connection(const int port, const char *address, Client *cli) {
  if (port < 0 || address == NULL || cli == NULL) {
    debug_print("establish_server_connection: invalid arguments");
    return -1;
  }

  int fd = connect_to_server(port, address);
  if (fd < MIN_FD) {
    debug_print("establish_server_connection: failed to connect to server");
    return -1;
  }
  debug_print("establish_server_connection: connected to server");

  cli->buffer = (Buffer *) malloc(sizeof(Buffer));
  reset_client_struct(cli);
  cli->socket_fd = fd;

  return fd;
}

int remove_client(const int client_index, Client *clients[]) {
  // precondition for invalid arguments
  if (client_index < MIN_FD) {
    debug_print("remove_client: invalid arguments");
    return -1;
  }

  // no client at index
  if (clients[client_index] == NULL) {
    debug_print("remove_client: target index %d has no client", client_index);
    return 1;
  }

  // closing fd and freeing heap memory
  int sav = clients[client_index]->socket_fd;
  close(clients[client_index]->socket_fd);
  free(clients[client_index]);
  clients[client_index] = NULL;

  debug_print("remove_client: client %d at index %d removed", sav, client_index);
  return 0;
}

/*
 * Sending functions
 */

int write_buf_to_client(Client *cli, const char *msg, const int msg_len) {
  // precondition for invalid arguments
  if (cli == NULL || msg == NULL || msg_len < 0) {
    debug_print("write_buf_to_client: invalid arguments");
    return -1;
  }

  // check if message is nonexistent
  if (msg_len == 0) {
    debug_print("write_buf_to_client: message is empty");
    return 0;
  }

  // check if message is too long
  if (msg_len > TEXT_LEN) { // TODO: negotiate a 'long message' for text with more than 255 bytes
    debug_print("write_buf_to_client: message is too long");
    return 1;
  }

  // assemble header with text signals
  char head[PACKET_LEN] = {0};
  head[PACKET_STATUS] = START_TEXT;
  head[PACKET_CONTROL1] = 1;
  head[PACKET_CONTROL2] = msg_len;

  // write header packet to target
  int head_written = write(cli->socket_fd, head, PACKET_LEN);
  if (head_written != PACKET_LEN) {

    // in case write was not perfectly successful
    if (head_written < 0) {
      debug_print("write_buf_to_client: failed to write header to client");
      return 1;
    } else {
      debug_print("write_buf_to_client: wrote incomplete header to client");
      return 1;
    }
  }

  // assemble message into buffer
  char buf[TEXT_LEN] = {0};
  snprintf(buf, TEXT_LEN, "%.*s", msg_len, msg);

  // write message to target
  int bytes_written = write(cli->socket_fd, buf, msg_len);
  if (bytes_written != msg_len) {

    // in case write was not perfectly successful
    if (bytes_written < 0) {
      debug_print("write_buf_to_client: failed to write message to client");
      return 1;
    } else {
      debug_print("write_buf_to_client: wrote incomplete message to client");
      return 1;
    }
  }

  debug_print("write_buf_to_client: wrote text packet of %d bytes to client", bytes_written);
  return 0;
}

int write_packet_to_client(Client *cli, Packet *pack) {
  // precondition for invalid arguments
  if (cli == NULL || pack == NULL) {
    debug_print("write_packet_to_client: invalid arguments");
    return -1;
  }

  // mark client outgoing flag with status
  cli->out_flag = pack->head[PACKET_STATUS];

  // write header packet to target
  int head_written = write(cli->socket_fd, pack->head, PACKET_LEN);
  if (head_written != PACKET_LEN) {

    // in case write was not perfectly successful
    if (head_written < 0) {
      debug_print("write_packet_to_client: failed to write header to client");
      return 1;
    } else {
      debug_print("write_packet_to_client: wrote incomplete header to client");
      return 1;
    }
  }

  // write message to target (if any)
  int bytes_written = write(cli->socket_fd, pack->buf, pack->inbuf);
  if (bytes_written != pack->inbuf) {

    // in case write was not perfectly successful
    if (bytes_written < 0) {
      debug_print("write_packet_to_client: failed to write message to client");
      return 1;
    } else {
      debug_print("write_packet_to_client: wrote incomplete message to client");
      return 1;
    }
  }

  // demark client outgoing flag
  cli->out_flag = 0;

  debug_print("write_packet_to_client: wrote packet of %d bytes to client", bytes_written);
  return 0;
}

int send_str_to_client(Client *cli, char *str) {
  // precondition for invalid arguments
  if (cli == NULL || str == NULL) {
    debug_print("send_str_to_client: invalid arguments");
    return -1;
  }

  return write_buf_to_client(cli, str, strlen(str));
}

int send_fstr_to_client(Client *cli, const char *format, ...) {
  // precondition for invalid argument
  if (cli == NULL) {
    debug_print("send_fstr_to_client: invalid arguments");
    return -1;
  }

  // buffer for assembling format string
  char msg[TEXT_LEN + 1];

  va_list args;
  va_start(args, format);
  vsnprintf(msg, TEXT_LEN, format, args);
  va_end(args);

  return write_buf_to_client(cli, msg, TEXT_LEN);
}

/*
 * Receiving functions
 */

int read_header(Client *cli) {
  // precondition for invalid argument
  if (cli == NULL) {
    debug_print("read_header: invalid argument");
    return -1;
  }

  // read packet from client
  char head[PACKET_LEN];
  int head_read = read(cli->socket_fd, head, PACKET_LEN);
  if (head_read != PACKET_LEN) {

    // in case read isn't perfect
    if (head_read < 0) {
      debug_print("read_header: failed to read header");
      return 1;
    } else {
      debug_print("read_header: received incomplete header");
      return 1;
    }
  }

  // mark client incoming flag
  cli->inc_flag = head[PACKET_STATUS];

  // parse the status
  int status;
  switch(head[PACKET_STATUS]) {
    case NULL_BYTE:
      debug_print("read_header: received NULL header");
      break;

    case START_HEADER:
      debug_print("read_header: received extended header");
      status = parse_ext_header(cli, head);
      break;

    case START_TEXT:
      debug_print("read_header: received text header");
      status = parse_text(cli, head[PACKET_CONTROL1], head[PACKET_CONTROL2]);
      break;

    case ENQUIRY:
      debug_print("read_header: received enquiry header");
      status = parse_enquiry(cli, head[PACKET_CONTROL1]);
      break;

    case ACKNOWLEDGE:
      debug_print("read_header: received acknowledge header");
      status = parse_acknowledge(cli, head[PACKET_CONTROL1]);
      break;

    case WAKEUP:
      debug_print("read_header: received wakeup header");
      //status = parse_wakeup();
      break;

    case NEG_ACKNOWLEDGE:
      debug_print("read_header: received negative acknowledge header");
      status = parse_neg_acknowledge(cli, head[PACKET_CONTROL1]);
      break;

    case IDLE:
      debug_print("read_header: received idle header");
      //status = parse_idle();
      break;

    case ESCAPE:
      debug_print("read_header: received escape header");
      status = parse_escape(cli);
      break;

    default: // unsupported/invalid
      debug_print("read_header: received invalid header");
      status = -1;
      break;

  }

  return status;
}

int parse_text(Client *cli, const int control1, const int control2) {
  // precondition for invalid arguments
  if (cli == NULL || control1 < 0 || control2 < 0) {
    debug_print("parse_text: invalid argument");
    return -1;
  }

  int count = control1;
  int width = control2;

  int bytes_read;
  char buffer[TEXT_LEN];
  for (int i = 0; i < count; i++) {

    // read the expected amount of data
    bytes_read = read(cli->socket_fd, buffer, width);
    if (bytes_read != width) {

      // in case read isn't perfect
      if (bytes_read < 0) {
        debug_print("parse_text: failed to read text section");
        return 1;
      } else {
        debug_print("parse_text: read incomplete text section");
        return 1;
      }
    }

    // TODO: consume the message
    printf("Received \"%.*s\"", width, buffer);
  }

  return 0;
}

int parse_enquiry(Client *cli, const int control1) {
  if (cli == NULL || control1 < 0) {
    debug_print("parse_enquiry: invalid arguments");
    return -1;
  }

  // initializing packet struct
  Packet pack;
  reset_packet_struct(&pack);

  // setting values of header
  char header[PACKET_LEN] = {0};
  header[PACKET_STATUS] = ACKNOWLEDGE;
  header[PACKET_CONTROL1] = ENQUIRY;

  // copy into struct, send to client
  assemble_packet(&pack, header, NULL, 0);
  return write_packet_to_client(cli, &pack);
}

int parse_acknowledge(Client *cli, const int control1) {
  // precondition for invalid argument
  if (cli == NULL || control1 < 0) {
    debug_print("parse_acknowledge: invalid arguments");
    return -1;
  }

  switch(control1) {
    case ENQUIRY:
      // TODO: response to ping
      break;
    case WAKEUP:
      // TODO: the sender says it has woken up
      break;
    case IDLE:
      // TODO: the sender says it has gone asleep
      break;
    case ESCAPE:
      // TOOD: the sender knows you're stopping
      break;
  }

  return 0;
}

int parse_neg_acknowledge(Client *cli, const int control1) {
  // precondition for invalid argument
  if (cli == NULL || control1 < 0) {
    debug_print("parse_neg_acknowledge: invalid arguments");
    return -1;
  }

  switch(control1) {
    case START_TEXT:
      // TODO: text arguments are invalid/message is invalid
      break;
    case ENQUIRY:
      // TODO: ping refused
      break;
    case WAKEUP:
      // TODO: sender is already awake
      break;
    case IDLE:
      // TODO: sender is already sleeping
      break;
    case ESCAPE:
      // TODO: you cannot disconnect
      break;
  }

  return 0;
}

int parse_escape(Client *cli) {
  // precondition for invalid argument
  if (cli == NULL) {
    debug_print("parse_escape: invalid arguments");
    return -1;
  }

  // initializing packet struct
  Packet pack;
  reset_packet_struct(&pack);

  // setting values of header
  char header[PACKET_LEN] = {0};
  header[PACKET_STATUS] = ACKNOWLEDGE;
  header[PACKET_CONTROL1] = ESCAPE;

  // copy into struct, send to client
  assemble_packet(&pack, header, NULL, 0);
  write_packet_to_client(cli, &pack);

  // marking this client as closed
  cli->inc_flag = -1;
  cli->out_flag = -1;

  return 0;
}

/*
 * Utility Functions
 */

int assemble_packet(Packet *pack, const char header[PACKET_LEN], const char *buf, const int buf_len) {
  // precondition for invalid arguments
  if (pack == NULL || header == NULL || (buf == NULL && buf_len != 0) || (buf != NULL && buf_len < 1)) {
    debug_print("assemble_packet: invalid arguments");
    return -1;
  }

  // copy header into packet
  memmove(pack->head, header, PACKET_LEN);

  // copy message into packet
  memmove(pack->buf, buf, buf_len);

  // set message length
  pack->inbuf = buf_len;

  return 0;
}

// errno is preserved through this function,
// it will not change between this function calling and returning.
void debug_print(const char *format, ...) {
  // precondition checking if debugging is turned off
  if (debug_fd < MIN_FD) return;

  // saving errno
  int errsav = errno;

  // capturing variable argument list
  va_list args;
  va_start(args, format);

  // printing argument
  dprintf(debug_fd, "%s", debug_header);
  vdprintf(debug_fd, format, args);
  dprintf(debug_fd, "\n");

  // in case errno is nonzero
  if (errsav > 0) {
    dprintf(debug_fd, "%s%s\n", debug_header, strerror(errsav));
  }

  // cleaining up
  va_end(args);
  errno = errsav;
  return;
}

int reset_client_struct(Client *client) {
  // precondition for invalid argument
  if (client == NULL) {
    debug_print("reset_client_struct: invalid arguments");
    return -1;
  }

  // struct fields
  client->socket_fd = -1;
  client->inc_flag = 0;
  client->out_flag = 0;

  // client buffer
  return reset_buffer_struct(client->buffer);
}

int reset_buffer_struct(Buffer *buffer) {
  // precondition for invalid argument
  if (buffer == NULL) {
    debug_print("reset_buffer_struct: invalid arguments");
    return -1;
  }

  memset(buffer->buf, 0, TEXT_LEN);
  buffer->consumed = 0;
  buffer->inbuf = 0;

  return 0;
}

int reset_packet_struct(Packet *pack) {
  // precondition for invalid argument
  if (pack == NULL) {
    debug_print("reset_packet_struct: invalid arguments");
    return -1;
  }

  // reset struct fields
  memset(pack->head, 0, PACKET_LEN);
  memset(pack->buf, 0, TEXT_LEN);
  pack->inbuf = 0;

  return 0;
}
