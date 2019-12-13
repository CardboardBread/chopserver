#ifndef __CHOPDEBUG_H__
#define __CHOPDEBUG_H__

#include <unistd.h>

#define DEBUG 1 // TODO: remove this and place in makefile for compilation-time decision to debug

/*
 * Printing Constants
 */

static const int debug_fd = STDERR_FILENO;
static const char dbg_head[] = "[DEBUG]: ";
static const char dbg_fcn_head[] = "[DEBUG][%s]: ";
static const char dbg_fcn_tail[] = "\n";
static const char dbg_err[] = "[ERRNO %d]: %s\n";

extern int header_type;
static const char msg_tail[] = "\n";

static const char recv_text_start[] = ": \"";
static const char recv_text_seg[] = "%.*s";
static const char recv_text_end[] = "\"\n";

static const char recv_ping_norm[] = " ENQUIRY\n";
static const char recv_ping_send[] = " ENQURIY Requested\n";
static const char recv_ping_time[] = " Time ENQUIRY: \"%ld\"\n";
static const char recv_ping_time_send[] = " Time ENQUIRY Requested\n";

static const char ackn_text[] = " %s Confirmed\n";

static const char wakeup_text[] = " Requesting Wakeup\n";

static const char neg_ackn_text[] = " %s Refused\n";

static const char idle_text[] = " Requesting Idle\n";

static const char esc_text[] = " Requesting Disconnect\n";

/*
 * Prints requested format string into stderr, prefixing properly
 */
void _debug_print(const char *format, const char *function, ...);

#ifdef DEBUG
#define DEBUG_PRINT(fmt, args...) _debug_print(__FUNCTION__, fmt, ## args)
#else
#define DEBUG_PRINT(fmt, args...) {} /* Don't do anything in release builds */
#endif

const char *stat_to_str(char status);

const char *enq_cont_to_str(char control1);

const char *pack_id_to_str(int id);

const char *msg_header();

#endif
