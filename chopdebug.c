#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

#include "chopdebug.h"

int header_type = 0;
static const char *all_headers[] = {"[CLIENT %d]", "[SERVER %d]"};

/*
 * Header-to-String Section
 */

/*static const int packet_id_len = 4;
static const char *packet_id[] = {
"PACKET_HEAD",
"PACKET_STATUS",
"PACKET_CONTROL1",
"PACKET_CONTROL2"};*/

static const int status_str_len = 32;
static const char *status_str[] = {
"NULL_BYTE",
"START_HEADER",
"START_TEXT",
"END_TEXT",
"END_TRANSMISSION",
"ENQURIY",
"ACKNOWLEDGE",
"WAKEUP",
"8",
"9",
"10",
"11",
"12",
"13",
"SHIFT_OUT",
"SHIFT_IN",
"START_DATA",
"CONTROL_ONE",
"CONTROL_TWO",
"CONTROL_THREE",
"CONTROL_FOUR",
"NEG_ACKNOWLEDGE",
"IDLE",
"END_TRANSMISSION_BLOCK",
"CANCEL",
"END_OF_MEDIUM",
"SUBSTITUTE",
"ESCAPE",
"FILE_SEPARATOR",
"GROUP_SEPARATOR",
"RECORD_SEPARATOR",
"UNIT_SEPARATOR"};

static const int enquiry_str_len = 4;
static const char *enquiry_str[] = {
"ENQUIRY_NORMAL",
"ENQUIRY_RETURN",
"ENQUIRY_TIME",
"ENQUIRY_RTIME"};

// errno is preserved through this function,
// it will not change between this function calling and returning.
void _debug_print(const char *function, const char *format, ...) {
  // check valid arguments
  if (format == NULL) {
    return;
  }

  // saving errno
  int errsav = errno;

  // capturing variable argument list
  va_list args;
  va_start(args, format);

  // printing argument
  dprintf(debug_fd, dbg_fcn_head, function);
  vdprintf(debug_fd, format, args);
  dprintf(debug_fd, dbg_fcn_tail);

  // in case errno is nonzero
  if (errsav > 0) {
    dprintf(debug_fd, dbg_err, errsav, strerror(errsav));
  }

  // cleaning up
  va_end(args);
  errno = errsav;
  return;
}

const char *stat_to_str(char status) {
	if (status < 0) {
		return NULL;
	}

	int index = (int) status;

	if (index > status_str_len) {
		return NULL;
	}

	return status_str[index];
}

const char *enq_cont_to_str(char control1) {
	if (control1 < 0) {
		return NULL;
	}

	int index = (int) control1;

	if (index > enquiry_str_len) {
		return NULL;
	}

	return enquiry_str[index];
}

const char *msg_header() {
	return all_headers[header_type];
}
