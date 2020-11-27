#ifndef _CHOPPACKET_H_
#define _CHOPPACKET_H_

#include "chopconst.h"

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
