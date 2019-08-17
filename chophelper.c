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
            if (setup_client_struct(&(clients[i]), fd) < 0) {
                debug_print("setup_new_client: failed to setup client at index %d", i);
                break;
            }

            debug_print("setup_new_client: placed client at index %d", i);
            return fd;
        }
    }

    // close the connection since we can't store it
    close(fd);
    debug_print("setup_new_client: no empty space for client %d found, closing connection", fd);
    return -1;
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
    if (cli == NULL || all_fds == NULL) {
        debug_print("process_request: invalid arguments");
        return -1;
    }

    char header[PACKET_LEN] = {0};
    switch (cli->inc_flag) {
        case NULL_BYTE:
            read_header(cli, header);
            parse_header(cli, header);
            break;
        case IDLE:
            read_header(cli, header);
            parse_idle_header(cli, header);
            break;
        case CANCEL:
            break;
        default:
            break;
    }

    switch (cli->out_flag) {
        case NULL_BYTE:
            break;
        case CANCEL:
            break;
        default:
            break;
    }

    return 0;
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

    // TODO: this is very platform dependent
    debug_print("write_packet_to_client: wrote packet of style %d and data of %d bytes to client",
                *((int *) pack->head), bytes_written);
    return 0;
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
 * Receiving functions
 */

int read_header(Client *cli, char head[PACKET_LEN]) {
    // precondition for invalid argument
    if (cli == NULL || head == NULL) {
        debug_print("read_header: invalid argument");
        return -1;
    }

    // read packet from client
    int head_read = read(cli->socket_fd, head, PACKET_LEN);
    if (head_read != PACKET_LEN) {

        // in case read isn't perfect
        if (head_read < 0) {
            debug_print("read_header: failed to read header");
            return 1;
        } else if (head_read == 0) {
            debug_print("read_header: socket is closed");
            parse_escape(cli);
            return -1;
        } else {
            debug_print("read_header: received incomplete header");
            return 1;
        }
    }

    // TODO: this is very platform dependent
    debug_print("read_header: read a header of style %d", *((int *) head));
    return 0;
}

int parse_header(Client *cli, const char head[PACKET_LEN]) {
    // precondition for invalid arguments
    if (cli == NULL || head == NULL) {
        debug_print("parse_header: invalid arguments");
        return -1;
    }

    // parse the status
    int status;
    switch (head[PACKET_STATUS]) {
        case NULL_BYTE:
            debug_print("parse_header: received NULL header");
            break;

        case START_HEADER:
            debug_print("parse_header: received extended header");
            status = parse_long_header(cli, head[PACKET_CONTROL1]);
            break;

        case START_TEXT:
            debug_print("parse_header: received text header");
            status = parse_text(cli, head[PACKET_CONTROL1], head[PACKET_CONTROL2]);
            break;

        case ENQUIRY:
            debug_print("parse_header: received enquiry header");
            status = parse_enquiry(cli, head[PACKET_CONTROL1]);
            break;

        case ACKNOWLEDGE:
            debug_print("parse_header: received acknowledge header");
            status = parse_acknowledge(cli, head[PACKET_CONTROL1]);
            break;

        case WAKEUP:
            debug_print("parse_header: received wakeup header");
            status = parse_wakeup(cli);
            break;

        case NEG_ACKNOWLEDGE:
            debug_print("parse_header: received negative acknowledge header");
            status = parse_neg_acknowledge(cli, head[PACKET_CONTROL1]);
            break;

        case IDLE:
            debug_print("parse_header: received idle header");
            status = parse_idle(cli);
            break;

        case ESCAPE:
            debug_print("parse_header: received escape header");
            status = parse_escape(cli);
            break;

        default: // unsupported/invalid
            debug_print("parse_header: received invalid header");
            status = -1;
            break;

    }

    return status;
}

int parse_idle_header(Client *cli, const char head[PACKET_LEN]) {
    // precondition for invalid arguments
    if (cli == NULL || head == NULL) {
        debug_print("parse_idle_header: invalid arguments");
        return -1;
    }

    int status;
    switch (head[PACKET_STATUS]) {
        case NULL_BYTE:
            debug_print("parse_idle_header: received NULL header");
            break;

        case ENQUIRY:
            debug_print("parse_idle_header: received enquiry header");
            status = parse_enquiry(cli, head[PACKET_CONTROL1]);
            break;

        case WAKEUP:
            debug_print("parse_idle_header: received wakeup header");
            status = parse_wakeup(cli);
            break;

        case ESCAPE:
            debug_print("parse_idle_header: received escape header");
            status = parse_escape(cli);
            break;

        case ACKNOWLEDGE:
            debug_print("parse_idle_header: received acknowledge header");
            status = parse_acknowledge(cli, head[PACKET_CONTROL1]);
            break;

        default: // unsupported/invalid
            debug_print("parse_idle_header: received invalid header");
            status = -1;
            break;

    }

    return status;
}

int parse_long_header(Client *cli, const int control1) {
    return 0;
}

int parse_text(Client *cli, const int control1, const int control2) {
    // precondition for invalid arguments
    if (cli == NULL || control1 < 0 || control2 < 0) {
        debug_print("parse_text: invalid argument");
        return -1;
    }

    // translate names for readability
    int count = control1;
    int width = control2;

    // signals set to unknown length read
    if (control1 == 0 && control2 == 0) {
        int long_len;
        char *head = read_long_text(cli, &long_len);
        if (head == NULL) {
            debug_print("parse_text: failed long text parsing");
            if (long_len < 0) {
                return -1;
            } else {
                return 1;
            }
        }

        printf(recieve_len_header, cli->socket_fd, long_len, head);
        debug_print("parse_text: read long text section %d bytes wide", long_len);
        return 0;
    }

    int total = 0;
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
        total += bytes_read;

        // terminate message
        buffer[bytes_read] = '\0';

        // TODO: consume the message
        printf(recieve_header, cli->socket_fd, buffer);
    }

    debug_print("parse_text: read text section %d bytes wide", total);
    return 0;
}

char *read_long_text(Client *cli, int *len_ptr) {
    // precondition for invalid arguments
    if (cli == NULL || len_ptr == NULL) {
        debug_print("read_long_text: invalid arguments");
        *len_ptr = -1;
        return NULL;
    }

    // read as much as possible into regular buffer
    char buffer[TEXT_LEN];
    int readnum = read(cli->socket_fd, buffer, TEXT_LEN);
    if (readnum < 0) {
        debug_print("read_long_text: failed to read long text section");
        *len_ptr = 0;
        return NULL;
    }

    // check if new data contains a stop
    char *ptr;
    int stop_len;
    if (buf_contains_symbol(buffer, readnum, END_TEXT, &stop_len)) {
        // alloc heap to store the data
        ptr = (char *) malloc(stop_len);

        // move data onto heap, notify parent fn of length of heap data
        memmove(ptr, buffer, stop_len);
        *len_ptr = stop_len;
    } else {
        // recurse for rest of the data
        int sub_len;
        char *nptr = read_long_text(cli, &sub_len);

        // alloc memory to hold all the data
        ptr = (char *) malloc(readnum + sub_len);

        // move data from here in
        memmove(ptr, buffer, readnum);

        // move data from recursion in
        memmove(ptr + readnum, nptr, sub_len);

        // notify parent fn of length of heap data
        *len_ptr = readnum + sub_len;
    }

    return ptr;
}

int parse_enquiry(Client *cli, const int control1) {
    if (cli == NULL || control1 < 0) {
        debug_print("parse_enquiry: invalid arguments");
        return -1;
    }

    switch(control1) {
      case 0:
        // just return an acknowledge
      case 1:
        // return a enquiry signal 0
      case 2:
        // parse time in data section, return acknowledge of time ping
      default:
        debug_print("parse_enquiry: invalid/unsupported control signal sent");
        return -1;
    }

    // initializing packet struct
    Packet pack;
    reset_packet_struct(&pack);

    // setting values of header
    char header[PACKET_LEN] = {0};
    header[PACKET_STATUS] = ACKNOWLEDGE;
    header[PACKET_CONTROL1] = ENQUIRY;

    debug_print("parse_enquiry: returning ping to client %d", cli->socket_fd);

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

    switch (control1) {
        case ENQUIRY:
            // TODO: ping was received
            debug_print("parse_acknowledge: client %d returned ping", cli->socket_fd);
            printf("pong\n");
            break;
        case WAKEUP:
            // TODO: the sender says it has woken up
            debug_print("parse_acknowledge: client %d successfully woken up", cli->socket_fd);
            cli->inc_flag = NULL_BYTE;
            break;
        case IDLE:
            // TODO: the sender says it has gone asleep
            debug_print("parse_acknowledge: client %d has begun sleeping", cli->socket_fd);
            cli->inc_flag = IDLE;
            break;
        case ESCAPE:
            // TOOD: the sender knows you're stopping
            debug_print("parse_acknowledge: client %d stop request accepted", cli->socket_fd);
            // marking this client as closed
            cli->inc_flag = CANCEL;
            cli->out_flag = CANCEL;
            break;
    }

    return 0;
}

int parse_wakeup(Client *cli) {
    if (cli == NULL) {
        debug_print("parse_wakeup: invalid arguments");
        return -1;
    }

    // initializing packet struct
    Packet pack;
    reset_packet_struct(&pack);

    // setting values of header
    char header[PACKET_LEN] = {0};
    if (cli->inc_flag == IDLE) {
        header[PACKET_STATUS] = ACKNOWLEDGE;
        header[PACKET_CONTROL1] = WAKEUP;

        // demark incoming client flag
        cli->inc_flag = NULL_BYTE;
        debug_print("parse_wakeup: marking client %d as awake", cli->socket_fd);
    } else {
        header[PACKET_STATUS] = NEG_ACKNOWLEDGE;
        header[PACKET_CONTROL1] = WAKEUP;

        debug_print("parse_wakeup: client %d is not asleep, refusing wake-up request", cli->socket_fd);
    }

    // copy into struct, send to client
    assemble_packet(&pack, header, NULL, 0);
    write_packet_to_client(cli, &pack);

    return 0;
}

int parse_neg_acknowledge(Client *cli, const int control1) {
    // precondition for invalid argument
    if (cli == NULL || control1 < 0) {
        debug_print("parse_neg_acknowledge: invalid arguments");
        return -1;
    }

    switch (control1) {
        case START_TEXT:
            // TODO: text arguments are invalid/message is invalid
            debug_print("parse_neg_acknowledge: client %d refused text packet", cli->socket_fd);
            break;
        case ENQUIRY:
            // TODO: ping refused
            debug_print("parse_neg_acknowledge: client %d refused ping request", cli->socket_fd);
            break;
        case WAKEUP:
            // TODO: sender is already awake/cannot wakeup
            debug_print("parse_neg_acknowledge: client %d refused wake-up request", cli->socket_fd);
            break;
        case IDLE:
            // TODO: sender is already sleeping/cannot sleep
            debug_print("parse_neg_acknowledge: client %d refused idle request", cli->socket_fd);
            break;
        case ESCAPE:
            // TODO: you cannot disconnect
            debug_print("parse_neg_acknowledge: client %d refused disconnect request", cli->socket_fd);
            break;
    }

    return 0;
}

int parse_idle(Client *cli) {
    // precondition for invalid argument
    if (cli == NULL) {
        debug_print("parse_idle: invalid arguments");
        return -1;
    }

    // initializing packet struct
    Packet pack;
    reset_packet_struct(&pack);

    // setting values of header
    char header[PACKET_LEN] = {0};
    if (cli->inc_flag == NULL_BYTE) {
        header[PACKET_STATUS] = ACKNOWLEDGE;
        header[PACKET_CONTROL1] = IDLE;

        // mark client as sleeping on both channels
        cli->inc_flag = IDLE;
        cli->out_flag = IDLE;
        debug_print("parse_idle: marking client %d as sleeping", cli->socket_fd);
    } else {
        header[PACKET_STATUS] = NEG_ACKNOWLEDGE;
        header[PACKET_CONTROL1] = IDLE;

        debug_print("parse_idle: client %d is busy, refusing sleep request", cli->socket_fd);
    }

    // copy into struct, send to client
    assemble_packet(&pack, header, NULL, 0);
    write_packet_to_client(cli, &pack);

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

    debug_print("parse_escape: shutting down connection to client %d", cli->socket_fd);

    // copy into struct, send to client
    assemble_packet(&pack, header, NULL, 0);
    write_packet_to_client(cli, &pack);

    // marking this client as closed
    cli->inc_flag = CANCEL;
    cli->out_flag = CANCEL;

    return 0;
}

/*
 * Data Layer functions
 */

int remove_newline(char *buf, int len) {
    // Precondition for invalid arguments
    if (buf == NULL || len < 2) {
        debug_print("remove_newline: invalid arguments");
        return -1;
    }

    int index;
    for (index = 1; index < len; index++) {
        // network newline
        if (buf[index - 1] == '\r' && buf[index] == '\n') {
            debug_print("remove_newline: network newline at %d", index);
            buf[index - 1] = '\0';
            buf[index] = '\0';
            return index;
        }

        // unix newline
        if (buf[index - 1] != '\r' && buf[index] == '\n') {
            debug_print("remove_newline: unix newline at %d", index);
            buf[index] = '\0';
            return index;
        }
    }

    // nothing found
    debug_print("remove_newline: no newline found");
    return -1;
}

/*
 * Utility Functions
 */

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

int is_client_status(Client *cli, const int status) {
    // precondition for invalid arguments
    if (cli == NULL || status < 0) {
        debug_print("is_client_status: invalid arguments");
        return 0;
    }

    return (cli->inc_flag == status && cli->out_flag == status);
}

int buf_contains_symbol(const char *buf, const int buf_len, int symbol, int *symbol_index) {
    // precondition for invalid arguments
    if (buf == NULL || buf_len < 0) {
        debug_print("buf_contains_stop: invalid arguments");
        return 0;
    }

    // buffer is empty
    if (buf_len == 0) {
        if (symbol_index != NULL) *symbol_index = 0;
        return 0;
    }

    // search entire buffer
    for (int i = 0; i < buf_len; i++) {
        if (buf[i] == symbol) {
            if (symbol_index != NULL) *symbol_index = i;
            return 1;
        }
    }

    // no symbol found
    if (symbol_index != NULL) *symbol_index = 0;
    return 0;
}

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

int setup_client_struct(Client **client, int socket_fd) {
    // precondition for invalid arguments
    if (client == NULL) {
        debug_print("setup_client_struct: invalid arguments");
        return -1;
    }

    // initialize struct memory
    *client = (Client *) malloc(sizeof(Client));
    (*client)->buffer = (Buffer *) malloc(sizeof(Buffer));
    reset_client_struct(*client);

    // copy in values
    if (socket_fd >= MIN_FD) (*client)->socket_fd = socket_fd;

    return 0;
}

int setup_packet_struct(Packet **packet, char *head, char *data) {
    // precondition for invalid arguments
    if (packet == NULL) {
        debug_print("setup_packet_struct: invalid arguments");
        return -1;
    }

    // initialize struct memory
    *packet = (Packet *) malloc(sizeof(Packet));

    // copy in header
    if (head != NULL) memmove((*packet)->head, head, PACKET_LEN);

    // copy in data
    if (data != NULL) memmove((*packet)->buf, data, TEXT_LEN);

    return 0;
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

int destroy_client_struct(Client **client) {
    if (client == NULL) {
        debug_print("destroy_client_struct: invalid arguments");
        return -1;
    }

    // variable for clarity
    Client *temp = *client;

    // closing the socket
    int sav = temp->socket_fd;
    close(temp->socket_fd);

    // freeing heap memory
    free(temp->buffer);
    free(temp);

    // moving pointers off the heap
    *client = NULL;
    temp = NULL;

    debug_print("destroy_client_struct: destroyed client %d", sav);
    return 0;
}
