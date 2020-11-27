#ifndef __CHOPDEBUG_H__
#define __CHOPDEBUG_H__

#include <unistd.h>
#include <pthread.h>

#include "chopconst.h"

/*
 * Printing Constants
 */

static const int debug_fd = STDERR_FILENO;
static const int msg_fd = STDOUT_FILENO;
static const char dbg_head[] = "[DEBUG][%lu][%s]: ";
static const char dbg_err[] = "[ERROR %d][%lu][%s]: {%s}, ";
static const char dbg_pack[] = "PACKET{%hhu:%hhu:%hhu:%hhu}";
static const char msg_tail[] = "\n";

static const char recv_text_start[] = "\"";
static const char recv_text_seg[] = "%.*s";
static const char recv_text_end[] = "\"\n";

static const char acpt_text[] = "Connected";

static const char recv_ping_norm[] = "ENQUIRY";
static const char recv_ping_send[] = "ENQURIY Requested";
static const char recv_ping_time[] = "Time ENQUIRY \"%ld\"";
static const char recv_ping_time_send[] = "Time ENQUIRY Requested";

static const char ackn_text[] = "%s Confirmed";

static const char wakeup_text[] = "Requesting Wakeup";

static const char neg_ackn_text[] = "%s Refused";

static const char idle_text[] = "Requesting Idle";

static const char err_text[] = "ERROR %d\"%s\"";

static const char esc_text[] = "Requesting disconnect";
static const char esc_confirm[] = "Connection closed";

/*
 * Printing Control Variable
 */

extern int header_type;
pthread_mutex_t print_lock;

#define INIT_SERVER_PRINT {header_type = 0; pthread_mutex_init(&print_lock, NULL);}
#define INIT_CLIENT_PRINT {header_type = 1; pthread_mutex_init(&print_lock, NULL);}

/*
 * Lock outside of print calls, in case of long-winded prints
 * Allows calling print multiple times without interruption
 */
#define LOCK_PRINT pthread_mutex_lock(&print_lock);
#define UNLOCK_PRINT pthread_mutex_unlock(&print_lock);
#define PRINTLOCK_BLOCK(args) pthread_mutex_lock(&print_lock);{args}pthread_mutex_unlock(&print_lock);

/*
 * Printing Functions
 */

void no_operation();

/*
 * Prints requested format string into stderr, prefixing properly
 */
void debug_print(const char *function, const char *format, ...);

void message_print(int socketfd, const char *format, ...);

int print_packet(struct client *client, struct packet *pack);

int print_text(struct client *client, struct packet *pack);

int print_enquiry(struct client *client, struct packet *pack);

int print_time(struct client *client, struct packet *pack);

int print_acknowledge(struct client *client, struct packet *pack);

int print_wakeup(struct client *client, struct packet *pack);

int print_neg_acknowledge(struct client *client, struct packet *pack);

int print_idle(struct client *client, struct packet *pack);

int print_error(struct client *client, struct packet *pack);

int print_escape(struct client *client, struct packet *pack);

/*
 * Printing Utilities
 */

const char *stat_to_str(pack_stat status);

const char *enq_cont_to_str(pack_con1 control1);

const char *msg_header();

/*
 * Printing Macros
 */

// Don't do anything in non-debug builds
#ifndef NDEBUG
#define DEBUG_PRINT(fmt, args...) debug_print(__FUNCTION__, fmt, ## args)
#else
#define DEBUG_PRINT(fmt, args...) no_operation(fmt, ## args)
#endif

#define MESSAGE_PRINT(id, fmt, args...) message_print(id, fmt, ## args)

// Don't print in non-debug builds
#ifndef NDEBUG
#define INVAL_CHECK(args) if (args) {DEBUG_PRINT("invalid arguments"); return -EINVAL;}
#else
#define INVAL_CHECK(args) if (args) {return -EINVAL;}
#endif

#endif
