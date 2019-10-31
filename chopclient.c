#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "chopconn.h"
#include "chopconst.h"
#include "chopdata.h"
#include "chopdebug.h"
#include "choppacket.h"
#include "chopsocket.h"

#define BUFSIZE 255

#ifndef PORT
#define PORT 50001
#endif

#ifndef ADDRESS
#define ADDRESS "127.0.0.1"
#endif

int sigint_received;

struct client *server_connection;

void sigint_handler(int code);

void sigint_handler(int code) {
  DEBUG_PRINT("received SIGINT, setting flag");
  sigint_received = 1;
}

int main(void) {
  // Reset SIGINT received flag.
  sigint_received = 0;

  // init connection model
  if (init_client_struct(&server_connection) > 0) {
    DEBUG_PRINT("failed socket alloc");
    return 1;
  }

  // connect to locally hosted server
  if (establish_server_connection(ADDRESS, PORT, &server_connection, BUFSIZE) > )) {
    DEBUG_PRINT("failed connection");
    return 1;
  }

  // setup fd set for selecting
  int max_fd = sock_fd;
  fd_set all_fds, listen_fds;
  FD_ZERO(&all_fds);
  FD_SET(sock_fd, &all_fds);
  FD_SET(STDIN_FILENO, &all_fds);

  int run = 1;
  while (run) {

    // closing connections and freeing memory before the process ends
    if (sigint_received) {
      DEBUG_PRINT("caught SIGINT, exiting");
      exit(1);
    }

    // selecting
    listen_fds = all_fds;
    int nready = select(max_fd + 1, &listen_fds, NULL, NULL, NULL);
    if (nready < 0) {
      DEBUG_PRINT("select");
      exit(1);
    }

    // reading from server
    if (FD_ISSET(server_connection->socket_fd, &listen_fds)) {
      if (process_request(server_connection, &all_fds) > 0) {
        DEBUG_PRINT("incoming");
        return 1;
      }

      // if escape
      if (is_client_status(server_connection, CANCEL)) {
        exit(1);
        //FD_CLR(server_connection.socket_fd, &all_fds);
        //run = 0;
      }
    }

    // reading from stdin, parsing and sending to server
    int num_read;
    char buffer[256];
    if (FD_ISSET(STDIN_FILENO, &listen_fds)) {
      num_read = read(STDIN_FILENO, buffer, 255);
      if (num_read == 0) break;
      buffer[255] = '\0';

      // initializing packet struct
      struct packet *pack;
      if (init_packet_struct(&pack) > 0) {
        DEBUG_PRINT("failed init packet");
        return 1;
      }

      if (remove_newline(buffer, num_read) > 0) {
        DEBUG_PRINT("buffer overflow");
        return 1;
      }

      // setting values of header
      if (strcmp(buffer, "exit") == 0) {
        if (assemble_header(out, 0 , ESCAPE, 0, 0) > 0) {
          DEBUG_PRINT("failed header assemble");
          return 1;
        }

      } else if (strcmp(buffer, "ping") == 0) {
        if (assemble_header(out, 0 , ENQUIRY, 0, 0) > 0) {
          DEBUG_PRINT("failed header assemble");
          return 1;
        }

      } else if (strcmp(buffer, "sleep") == 0) {
        if (assemble_header(out, 0 , IDLE, 0, 0) > 0) {
          DEBUG_PRINT("failed header assemble");
          return 1;
        }

      } else if (strcmp(buffer, "wake") == 0) {
        if (assemble_header(out, 0 , WAKEUP, 0, 0) > 0) {
          DEBUG_PRINT("failed header assemble");
          return 1;
        }

      } else {
        if (send_str_to_client(server_connection, buffer) > 0) {
          DEBUG_PRINT("failed sending user input")
          return 1;
        }
        continue;

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
    }
  }

  return 0;
}
