#ifndef _CHOPPACKET_H_
#define _CHOPPACKET_H_

#include "chopconst.h"

/*
 * Sending functions
 */

int write_packet_to_client(struct client *cli, struct packet *pack);

int write_dataless(struct client *cli, const pack_head head, const pack_stat status, const pack_con1 control1, const pack_con2 control2);

int write_datapack(struct client *cli, const pack_head head, const pack_stat status, const pack_con1 control1, const pack_con2 control2, const char *buf, const int buflen);

int write_wordpack(struct client *cli, const pack_head head, const pack_stat status, const pack_con1 control1, const pack_con2 control2, unsigned long int value);

/*
 * Receiving functions
 */

int read_header(struct client *cli, struct packet *pack);

int parse_header(struct client *cli, struct packet *pack);

int parse_long_header(struct client *cli, struct packet *pack);

int parse_text(struct client *cli, struct packet *pack);

char *read_long_text(struct client *cli, struct packet *pack, int *len_ptr, int *buffers);

int parse_enquiry(struct client *cli, struct packet *pack);

int parse_acknowledge(struct client *cli, struct packet *pack);

int parse_wakeup(struct client *cli, struct packet *pack);

int parse_neg_acknowledge(struct client *cli, struct packet *pack);

int parse_idle(struct client *cli, struct packet *pack);

int parse_escape(struct client *cli, struct packet *pack);

/*
 * Packet Utility functions
 */

int assemble_header(struct packet *pack, int head, int status, int control1, int control2);

int split_header(struct packet *pack, const char *header);

int assemble_body(struct buffer *buffer, const char *data, const int len);

int append_buffer(struct packet *pack, const int bufsize, struct buffer **out);

int packet_style(struct packet *pack);

int print_text(struct client *client, struct packet *pack);

int print_enquiry(struct client *client, struct packet *pack);

int print_time(struct client *client, struct packet *pack);

int print_acknowledge(struct client *client, struct packet *pack);

int print_wakeup(struct client *client, struct packet *pack);

int print_neg_acknowledge(struct client *client, struct packet *pack);

int print_idle(struct client *client, struct packet *pack);

int print_escape(struct client *client, struct packet *pack);

#endif
