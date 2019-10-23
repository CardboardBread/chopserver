//#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
//#include <string.h>
//#include <errno.h>
//#include <sys/stat.h>
//#include <sys/types.h>
//#include <signal.h>
//#include <stdarg.h>

#include "chopconst.h"
#include "chopdebug.h"
#include "chopsocket.h"
#include "chopdata.h"

int accept_new_client(struct server *receiver, int bufsize, int *newfd) {
  // precondition for invalid arguments
  if (receiver == NULL) {
    DEBUG_PRINT("invalid arguments");
    return 1;
  }

  // blocking until client connects
  int client_fd;
  if (accept_connection(server->server_fd, client_fd) > 0) {
    DEBUG_PRINT("accept fail");
    return 1;
  }

  DEBUG_PRINT("new client on %d", client_fd);
  server->cur_connections++;

  // finding an 'empty' space in the client array
  for (int i = 0; i < server->max_connections; i++) {
    if (server->clients[i] == NULL) {

      // placing new client in empty space
      if (init_client_struct(server->clients + i, bufsize) > 0) {
        DEBUG_PRINT("init client fail");
        break;
      }

      struct client *new = server->clients[i];
      new->socket_fd = client_fd;
      new->server_fd = server->server_fd;
      new->inc_flag = 0;
      new->out_flag = 0;

      DEBUG_PRINT("new client, index %d", i);
      if (newfd != NULL) *newfd = client_fd;
      return 0;
    }
  }

  // close the connection since we can't store it
  close(client_fd);
  DEBUG_PRINT("no space for new client, refusing");
  return 1;
}

int establish_server_connection(const char *address, const int port, Client **client) {
    // precondition for invalid arguments
    if (client == NULL || port < 0 || address == NULL) {
        debug_print("establish_server_connection: invalid arguments");
        return -1;
    }

    // connect to server
    int fd = connect_to_server(port, address);
    if (fd < MIN_FD) {
        debug_print("establish_server_connection: failed to connect to server at %s:%d", address, port);
        return -1;
    }
    debug_print("establish_server_connection: connected to server at %s:%d", address, port);

    // store fd in client struct
    if (setup_client_struct(client, fd) < 0) {
        debug_print("establish_server_connection: failed to store server connection, closing connection");
        close(fd);
        return -1;
    }
    debug_print("establish_server_connection: server connection successful");

    return fd;
}

int remove_client_index(const int client_index, Client *clients[]) {
    // precondition for invalid arguments
    if (client_index < MIN_FD) {
        debug_print("remove_client_index: invalid arguments");
        return -1;
    }

    // no client at index
    if (clients[client_index] == NULL) {
        debug_print("remove_client_index: target index %d has no client", client_index);
        return 1;
    }
    debug_print("remove_client_index: removing client at index %d", client_index);

    return destroy_client_struct(&(clients[client_index]));
}

int remove_client_address(const int client_index, Client **client) {
    // precondition for invalid arguments
    if (client_index < MIN_FD || client == NULL) {
        debug_print("remove_client_address: invalid arguments");
        return -1;
    }

    // no client at pointer
    if (*client == NULL) {
        debug_print("remove_client_address: target address has no client");
        return 1;
    }
    debug_print("remove_client_address: removing client at %p", *client);

    return destroy_client_struct(client);
}

int process_request(Client *cli, fd_set *all_fds) {
    // precondition for invalid arguments
    if (cli == NULL || all_fds == NULL) {
        debug_print("process_request: invalid arguments");
        return -1;
    }

    int status = 0;
    char header[PACKET_LEN] = {0};
    switch (cli->inc_flag) {
        case NULL_BYTE:
            debug_print("process_request: client is flagged for normal reading operation.");
            status = read_header(cli, header);
            if (status) {
                break;
            }
            status = parse_header(cli, header);
            break;
        case CANCEL:
            debug_print("process_request: client flagged as closed, refusing to read");
            break;
        default:
            break;
    }

    return status;
}

int send_str_to_all(Client *clients[], const char *str) {
    // precondition for invalid arguments
    if (clients == NULL || str == NULL) {
        debug_print("send_str_to_all: invalid arguments");
        return -1;
    }

    // iterate through all available clients, stopping if any fail
    int status;
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (clients[i] != NULL) {
            status = send_str_to_client(clients[i], str);
            if (status) return status;
        }
    }

    return 0;
}

int send_fstr_to_all(Client *clients[], const char *format, ...) {
    // precondition for invalid arguments
    if (clients == NULL) {
        debug_print("send_fstr_to_all: invalid arguments");
        return -1;
    }

    // buffer for assembling format string
    char msg[TEXT_LEN + 1];

    va_list args;
    va_start(args, format);
    vsnprintf(msg, TEXT_LEN, format, args);
    va_end(args);

    // iterate through all available clients, stopping if any fail
    int status;
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (clients[i] != NULL) {
            status = send_str_to_client(clients[i], msg);
            if (status) return status;
        }
    }

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
    if (msg_len > TEXT_LEN) {
        debug_print("write_buf_to_client: message is too long");
        return 1;
    }

    // initialize packet for buffer
    Packet pack;
    reset_packet_struct(&pack);

    // assemble packet header with text signal
    char header[PACKET_LEN] = {0};
    header[PACKET_STATUS] = START_TEXT;
    header[PACKET_CONTROL1] = 1;
    header[PACKET_CONTROL2] = msg_len;

    // assemble packet
    assemble_packet(&pack, header, msg, msg_len);

    return write_packet_to_client(cli, &pack);
}

int send_str_to_client(Client *cli, const char *str) {
    // precondition for invalid arguments
    if (cli == NULL || str == NULL) {
        debug_print("send_str_to_client: invalid arguments");
        return -1;
    }

    return write_buf_to_client(cli, str, strlen(str) + 1);
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
 * Client Utility Functions
 */

 int is_client_status(Client *cli, const int status) {
     // precondition for invalid arguments
     if (cli == NULL || status < 0) {
         debug_print("is_client_status: invalid arguments");
         return 0;
     }

     return (cli->inc_flag == status && cli->out_flag == status);
 }

 int is_address(const char *str) {
     if (str == NULL) {
         debug_print("is_address: invalid arguments");
         return -1;
     }

     // copy string to local buffer for tokenizing
     int cap = strlen(str);
     char addr[cap + 1];
     strcpy(addr, str);

     // tokenize by the periods
     long num;
     char *ptr;
     char *token;
     char *rest = addr;
     for (int i = 0; i < 4; i++) {
         token = strtok_r(rest, ".", &rest);

         // in case less than 4 tokens are available
         if (token == NULL) {
             debug_print("is_address: \"%s\" is not in address format", str);
             return 0;
         }

         // convert token to number, check if valid as byte
         num = strtol(token, &ptr, 10);
         if (num < 0 || num > 255) {
             debug_print("is_address: \"%s\" has an invalid value", str);
             return 0;
         }
     }

     debug_print("is_address: \"%s\" is a valid address", str);
     return 1;
 }

 int is_name(const char *str);
