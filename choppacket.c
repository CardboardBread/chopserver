
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

int parse_long_header(Client *cli, const int control1) {
    // precondition for invalid arguments
    if (cli == NULL || control1 < 0) {
        debug_print("parse_long_header: invalid arguments");
        return -1;
    }

    int packetcount = control1;
    debug_print("parse_long_header: long header contains %d packets", packetcount);

    int status = 0;
    char header[PACKET_LEN] = {0};
    for (int i = 0; i < packetcount; i++) {
        status = read_header(cli, header);
        if (status) break;
        status = parse_header(cli, header);
        if (status) break;
    }

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
 * Packet Utility Functions
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
