#include <stdlib.h>
#include <errno.h>

#include "chopconst.h"
#include "chopdata.h"
#include "chopdebug.h"
#include "choppacket.h"
#include "chopsend.h"

/*
* Generic sending functions
*/

int write_dataless(struct client *cli, struct packet_header header) {
	// precondition for invalid arguments
	INVAL_CHECK(cli == NULL);

	// mark client outgoing flag with status
	cli->out_flag = header.status;

	// initalize packet
	struct packet *out;
	if (init_packet(&out) < 0) {
		DEBUG_PRINT("failed init packet");
		return -1;
	}

	// setting header value
	out->header = header;

	// write to client
	int ret = write_packet(cli, out);
	if (ret < 0) {
		DEBUG_PRINT("failed write");
		cli->inc_flag = CANCEL;
		cli->out_flag = CANCEL;
		return ret;
	}

	// destroy allocated packet
	if (destroy_packet(&out) < 0) {
		DEBUG_PRINT("failed packet destroy");
		return -1;
	}

	return ret;
}

int write_datapack(struct client *cli, struct packet_header header, const char *buf, size_t buf_len) {
	// check valid arguments
	INVAL_CHECK(cli == NULL || buf == NULL || buf_len < 1);

	// mark client outgoing flag with status
	cli->out_flag = header.status;

	// initialize packet
	struct packet *out;
	if (init_packet(&out) < 0) {
		DEBUG_PRINT("failed init packet");
		return -ENOMEM;
	}

	// setting header value
	out->header = header;

	// copy data into new packet with client-defined segments
	if (assemble_data(out, buf, buf_len, cli->window) < 0) {
		DEBUG_PRINT("failed data assembly");
		return -1;
	}

	// write to client
	if (write_packet(cli, out) < 0) {
		DEBUG_PRINT("failed write");
		cli->inc_flag = CANCEL;
		cli->out_flag = CANCEL;
		return -1;
	}

	// destroy allocated packet
	if (destroy_packet(&out) < 0) {
		DEBUG_PRINT("failed packet destroy");
		return -1;
	}

	return 0;
}

/*
 * Status-specific sending functions
 */

int send_text(struct client *target, const char *buf, size_t buf_len) {
	INVAL_CHECK(target == NULL || buf == NULL || buf_len < 1);
	// check if we can fit the whole text in one packet header, if not split it up
	// call write_datapack with the proper arguments

	// calculate how muany packets are needed to send all the data
	size_t packet_max = 8 * target->window;
	size_t packet_count = WRAP_DIV(buf_len, packet_max);

	// allocate array of pointers for each packet to be sent
	struct packet **packet_ptrs = calloc(packet_count, sizeof(struct packet *));
	if (packet_ptrs == NULL) {
		DEBUG_PRINT("failed allocating packet containers");
		return -ENOMEM;
	}

	// initialize, fill, and send each packet
	size_t sent = 0;
	for (size_t pack = 0; pack < packet_count; pack++) {
		struct packet **pack_ptr = packet_ptrs + pack;
		size_t partition = (packet_max > buf_len) ? buf_len : packet_max;

		// initialize packet
		if (init_packet(pack_ptr) < 0) {
			DEBUG_PRINT("failed allocating packet");
			return -errno;
		}

		// calculate data for packet, and set header values
		struct packet *out = (*pack_ptr);
		size_t segments = partition / target->window;
		struct packet_header header = {0, START_TEXT, segments, target->window};
		out->header = header;

		// copy data into new packet with client-defined segments
		if (assemble_data(out, buf + sent, partition, target->window) < 0) {
			DEBUG_PRINT("failed data assembly");
			return -errno;
		}

		// write to client
		if (write_packet(target, out) < 0) {
			DEBUG_PRINT("failed packet write");
			return -errno;
		}

		// destroy allocated packet
		if (destroy_packet(pack_ptr) < 0) {
			DEBUG_PRINT("failed packet destroy");
			return -errno;
		}

		// update tracker
		sent += partition;
	}

	DEBUG_PRINT("sent text in %zu packets", packet_count);
	return 0;
}

int send_enqu(struct client *target, pack_con1 type) {
	INVAL_CHECK(target == NULL || type > MAX_ENQUIRY);

	struct packet_header header = {0, ENQUIRY, type, 0};

	int ret;
	if (type == ENQUIRY_TIME) {
		header.control2 = sizeof(time_t);
		time_t current_time = time(NULL);
		ret = write_datapack(target, header, (const char *) &current_time, sizeof(time_t));
	} else {
		ret = write_dataless(target, header);
	}

	if (ret < 0) {
		DEBUG_PRINT("failed enquiry write");
		return -errno;
	}

	return 0;
}

int send_ackn(struct client *target, pack_con1 confirm) {
	INVAL_CHECK(target == NULL || confirm > MAX_STATUS || confirm < 0);

	struct packet_header header = {0, ACKNOWLEDGE, confirm, 0};
	if (write_dataless(target, header) < 0) {
		DEBUG_PRINT("failed acknowledge write");
		return -errno;
	}

	return 0;
}

int send_wake(struct client *target) {
	INVAL_CHECK(target == NULL);

	struct packet_header header = {0, WAKEUP, 0,0};
	if (write_dataless(target, header) < 0) {
		DEBUG_PRINT("failed wakeup write");
		return -errno;
	}

	return 0;
}

int send_neg_ackn(struct client *target, pack_con1 deny) {
	INVAL_CHECK(target == NULL || deny > MAX_STATUS || deny < 0);

	struct packet_header header = {0, NEG_ACKNOWLEDGE, deny, 0};
	if (write_dataless(target, header) < 0) {
		DEBUG_PRINT("failed negative acknowledge write");
		return -errno;
	}

	return 0;
}

int send_idle(struct client *target) {
	INVAL_CHECK(target == NULL);

	struct packet_header header = {0, IDLE, 0,0};
	if (write_dataless(target, header) < 0) {
		DEBUG_PRINT("failed idle write");
		return -errno;
	}

	return 0;
}

int send_exit(struct client *target) {
	INVAL_CHECK(target == NULL);

	struct packet_header header = {0, ESCAPE, 0,0};
	if (write_dataless(target, header) < 0) {
		DEBUG_PRINT("failed exit write");
		return -errno;
	}

	return 0;
}