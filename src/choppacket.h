#ifndef _CHOPPACKET_H_
#define _CHOPPACKET_H_

#include "chopconst.h"

/*
 * Sending functions
 */

int write_dataless(struct client *cli, pack_head head, pack_stat status, pack_con1 control1, pack_con2 control2);

int write_datapack(struct client *cli, pack_head head, pack_stat status, pack_con1 control1, pack_con2 control2, const char *buf, int buflen);

int write_wordpack(struct client *cli, pack_head head, pack_stat status, pack_con1 control1, pack_con2 control2, unsigned long int value);

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

/*
 * Packet Utility functions
 */

int assemble_header(struct packet *pack, pack_head head, pack_stat status, pack_con1 control1, pack_con2 control2);

int assemble_body(struct buffer *buffer, const char *data, int len);

int append_buffer(struct packet *pack, int bufsize, struct buffer **out);

int packet_style(struct packet *pack);

#endif
