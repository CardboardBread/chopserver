#ifndef CHOPSEND_H__
#define CHOPSEND_H__

#include "chopconst.h"

int send_text(struct client *target, const char *buf, size_t buf_len);

int send_enqu(struct client *target, pack_con1 type);

int send_ackn(struct client *target, struct packet *source, pack_con1 confirm);

int send_wake(struct client *target);

int send_neg_ackn(struct client *target, struct packet *source, pack_con1 deny);

int send_idle(struct client *target);

int send_exit(struct client *target);

#endif // CHOPSEND_H__
