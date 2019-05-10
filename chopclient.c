#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "socket.h"
#include "chophelper.h"

#ifndef PORT
  #define PORT 50001
#endif

#ifndef ADDRESS
  #define ADDRESS "127.0.0.1"
#endif

int sigint_received;

Client *server_connection;

void sigint_handler(int code);

void sigint_handler(int code) {
  debug_print("sigint_handler: received SIGINT, setting flag");
  sigint_received = 1;
}

int main(void) {
  // connect to locally hosted server
  int sock_fd = establish_server_connection(ADDRESS, PORT, &server_connection);

  // setup fd set for selecting
  int max_fd = sock_fd;
  fd_set all_fds, listen_fds;
  FD_ZERO(&all_fds);
  FD_SET(sock_fd, &all_fds);
  FD_SET(STDIN_FILENO, &all_fds);

  int run = 1;
  while(run) {

    // closing connections and freeing memory before the process ends
    if (sigint_received) {
      debug_print("main: caught SIGINT, exiting");
      exit(1);
    }

    // selecting
    listen_fds = all_fds;
    int nready = select(max_fd + 1, &listen_fds, NULL, NULL, NULL);
    if (nready < 0) {
      debug_print("select: error");
      exit(1);
    }

    // reading from server
    if (FD_ISSET(server_connection->socket_fd, &listen_fds)) {
      read_header(server_connection);

      // if escape
      if (is_client_status(server_connection, -1)) {
        exit(1);
        //FD_CLR(server_connection.socket_fd, &all_fds);
        //run = 0;
      }
    }

    // reading from stdin, parsing and sending to server
    int num_read;
    char buffer[265];
    if (FD_ISSET(STDIN_FILENO, &listen_fds)) {
      num_read = read(STDIN_FILENO, buffer, 265);
      if (num_read == 0) break;
      buffer[num_read] = '\0';

      // initializing packet struct
      Packet pack;
      reset_packet_struct(&pack);

      remove_newline(buffer, num_read);

      // setting values of header
      char header[PACKET_LEN] = {0};
      if (strcmp(buffer, "exit") == 0) {
        header[PACKET_STATUS] = ESCAPE;
      } else if (strcmp(buffer, "ping") == 0) {
        header[PACKET_STATUS] = ENQUIRY;
      } else if (strcmp(buffer, "sleep") == 0) {
        header[PACKET_STATUS] = IDLE;
      } else if (strcmp(buffer, "wake") == 0) {
        header[PACKET_STATUS] = WAKEUP;
      } else {
        send_str_to_client(server_connection, buffer);
        continue;
      }

      // copy into struct, send to client
      assemble_packet(&pack, header, NULL, 0);
      write_packet_to_client(server_connection, &pack);
    }
  }

  return 0;
}
