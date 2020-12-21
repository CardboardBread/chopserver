#include <stdlib.h>
#include <errno.h>

#include "chopconst.h"
#include "chopdata.h"
#include "chopdebug.h"
#include "chopsend.h"

#include "hashtable.h"

/*
 * An interrupt kind of function, for any code that needs to run right before a
 * packet is sent. Scope is local to this source file since it should only be
 * called by local functions which can properly interpret the arguments of
 * outside callers.
 */
int send_packet(struct client *cli, struct packet *out) {
	INVAL_CHECK(cli == NULL || out == NULL);

	// do not register response packets
	if (out->header.status != ACKNOWLEDGE || out->header.status != NEG_ACKNOWLEDGE) {

		// set head with current send depth
		out->header.head = cli->send_depth;

		// convert pairing elements to hash types
		hash_value val = (hash_value) out;
		hash_key key = (hash_key) cli->send_depth;

		// place outgoing packet in table
		if (!table_put(cli->pair_table, key, val)) {
			DEBUG_PRINT("failed packet pair register");
			return -errno;
		}

		// next packet sent should have a different send depth
		cli->send_depth++;

		DEBUG_PRINT("registered packet %d under exchange %u", packet_style(out), key);
	}

	return write_packet(cli, out);
}

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
	if (create_packet(&out) < 0) {
		DEBUG_PRINT("failed init packet");
		return -1;
	}

	// setting header value
	out->header = header;

	// write to client
	int ret = send_packet(cli, out);
	if (ret < 0) {
		DEBUG_PRINT("failed write");
		cli->inc_flag = CANCEL;
		cli->out_flag = CANCEL;
		return ret;
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
	if (create_packet(&out) < 0) {
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
	if (send_packet(cli, out) < 0) {
		DEBUG_PRINT("failed write");
		cli->inc_flag = CANCEL;
		cli->out_flag = CANCEL;
		return -1;
	}

	return 0;
}

/*
 * Status-specific sending functions
 */

/*
 * Will allocate all required packets and fill them all before sending them, in
 * order to keep them sent as close to each other as possible, since sending
 * packets as soon as we make them may invite some delay between each packet
 * required to sent the entire given text.
 */
int send_text(struct client *target, const char *buf, size_t buf_len) {
	INVAL_CHECK(target == NULL || buf == NULL || buf_len < 1);
	// check if we can fit the whole text in one packet header, if not split it up
	// call write_datapack with the proper arguments

	// calculate how muany packets are needed to send all the data
	size_t packet_max = pack_con1_max;
	size_t packet_count = WRAP_DIV(buf_len, packet_max);

	// allocate array of pointers for each packet to be sent
	struct packet **packet_ptrs = calloc(packet_count, sizeof(struct packet *));
	if (packet_ptrs == NULL) {
		DEBUG_PRINT("failed allocating packet containers");
		return -ENOMEM;
	}

	// initialize and fill each packet
	size_t sent = 0;
	for (size_t pack = 0; pack < packet_count; pack++) {
		size_t partition = EXPECT(packet_max, buf_len);

		// initialize packet
		struct packet **pack_ptr = packet_ptrs + pack;
		if (create_packet(pack_ptr) < 0) {
			DEBUG_PRINT("failed allocating packet");
			return -errno;
		}

		// copy data into new packet with client-defined segments
		struct packet *out = (*pack_ptr);
		int segments = assemble_data(out, buf + sent, partition, target->window);
		if (segments < 0) {
			DEBUG_PRINT("failed data assembly");
			return -errno;
		}

		// set header values post-assembly
		struct packet_header header = {0, START_TEXT, partition, 0};
		out->header = header;

		// update tracker
		sent += partition;
		buf_len -= partition;
	}

	// send each packet
	for (size_t pack = 0; pack < packet_count; pack++) {
		struct packet **pack_ptr = packet_ptrs + pack;
		struct packet *out = (*pack_ptr);

		// write to client
		if (send_packet(target, out) < 0) {
			DEBUG_PRINT("failed packet write");
			return -errno;
		}
	}

	// free array of packets
	free(packet_ptrs);
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

int send_ackn(struct client *target, struct packet *source, pack_con1 confirm) {
	INVAL_CHECK(target == NULL || source == NULL || confirm > MAX_STATUS || confirm < 0);

	struct packet_header header = {source->header.head, ACKNOWLEDGE, confirm, 0};
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

int send_neg_ackn(struct client *target, struct packet *source, pack_con1 deny) {
	INVAL_CHECK(target == NULL || deny > MAX_STATUS || deny < 0);

	struct packet_header header = {source->header.head, NEG_ACKNOWLEDGE, deny, 0};
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
