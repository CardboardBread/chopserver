#ifndef _CHOPPACKET_H_
#define _CHOPPACKET_H_

#include "chopconst.h"

/*
 * Sending functions
 */

int write_dataless(struct client *cli, struct packet_header header);

int write_datapack(struct client *cli, struct packet_header header, const char *buf, size_t buf_len);

int write_wordpack(struct client *cli, struct packet_header header, unsigned long int value);

/*
 * Receiving functions
 */

int parse_header(struct client *cli, struct packet *pack);

int parse_long_header(struct client *cli, struct packet *pack);

int parse_text(struct client *cli, struct packet *pack);

int read_long_text(struct client *cli, struct packet *pack);

int parse_enquiry(struct client *cli, struct packet *pack);

int parse_acknowledge(struct client *cli, struct packet *pack);

int parse_wakeup(struct client *cli, struct packet *pack);

int parse_neg_acknowledge(struct client *cli, struct packet *pack);

int parse_idle(struct client *cli, struct packet *pack);

int parse_error(struct client *cli, struct packet *pack);

int parse_escape(struct client *cli, struct packet *pack);

#endif
