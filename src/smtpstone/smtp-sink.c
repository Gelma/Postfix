/*++
/* NAME
/*	smtp-sink 8
/* SUMMARY
/*	multi-threaded SMTP/LMTP test server
/* SYNOPSIS
/* .fi
/*	\fBsmtp-sink\fR [\fIoptions\fR] [\fBinet:\fR][\fIhost\fR]:\fIport\fR
/*	\fIbacklog\fR
/*
/*	\fBsmtp-sink\fR [\fIoptions\fR] \fBunix:\fR\fIpathname\fR \fIbacklog\fR
/* DESCRIPTION
/*	\fIsmtp-sink\fR listens on the named host (or address) and port.
/*	It takes SMTP messages from the network and throws them away.
/*	The purpose is to measure SMTP client performance, not protocol
/*	compliance.
/*	Connections can be accepted on IPV4 endpoints or UNIX-domain sockets.
/*	IPV4 is the default.
/*	This program is the complement of the \fIsmtp-source\fR program.
/*
/*	Arguments:
/* .IP \fB-c\fR
/*	Display a running counter that is updated whenever an SMTP
/*	QUIT command is executed.
/* .IP \fB-L\fR
/*	Speak LMTP rather than SMTP.
/* .IP "\fB-n \fIcount\fR"
/*	Terminate after \fIcount\fR sessions. This is for testing purposes.
/* .IP \fB-p\fR
/*	Disable ESMTP command pipelining.
/* .IP \fB-P\fR
/*	Change the server greeting so that it appears to come through
/*	a CISCO PIX system.
/* .IP \fB-v\fR
/*	Show the SMTP conversations.
/* .IP "\fB-w \fIdelay\fR"
/*	Wait \fIdelay\fR seconds before responding to a DATA command.
/* .IP [\fBinet:\fR][\fIhost\fR]:\fIport\fR
/*	Listen on network interface \fIhost\fR (default: any interface)
/*	TCP port \fIport\fR. Both \fIhost\fR and \fIport\fR may be
/*	specified in numeric or symbolic form.
/* .IP \fBunix:\fR\fIpathname\fR
/*	Listen on the UNIX-domain socket at \fIpathname\fR.
/* .IP \fIbacklog\fR
/*	The maximum length the queue of pending connections,
/*	as defined by the listen(2) call.
/* SEE ALSO
/*	smtp-source, SMTP/LMTP test message generator
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/

/* System library. */

#include <sys_defs.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#ifdef STRCASECMP_IN_STRINGS_H
#include <strings.h>
#endif

/* Utility library. */

#include <msg.h>
#include <vstring.h>
#include <vstream.h>
#include <vstring_vstream.h>
#include <get_hostname.h>
#include <listen.h>
#include <events.h>
#include <mymalloc.h>
#include <iostuff.h>
#include <msg_vstream.h>

/* Global library. */

#include <smtp_stream.h>

/* Application-specific. */

typedef struct SINK_STATE {
    VSTREAM *stream;
    VSTRING *buffer;
    int     data_state;
    int     (*read) (struct SINK_STATE *);
    int     rcpts;
} SINK_STATE;

#define ST_ANY			0
#define ST_CR			1
#define ST_CR_LF		2
#define ST_CR_LF_DOT		3
#define ST_CR_LF_DOT_CR		4
#define ST_CR_LF_DOT_CR_LF	5

static int var_tmout;
static int var_max_line_length = 2048;
static char *var_myhostname;
static int command_read(SINK_STATE *);
static int data_read(SINK_STATE *);
static void disconnect(SINK_STATE *);
static int count;
static int counter;
static int max_count;
static int disable_pipelining;
static int fixed_delay;
static int enable_lmtp;
static int pretend_pix;

/* ehlo_response - respond to EHLO command */

static void ehlo_response(SINK_STATE *state)
{
    smtp_printf(state->stream, "250-%s", var_myhostname);
    if (!disable_pipelining)
	smtp_printf(state->stream, "250-PIPELINING");
    smtp_printf(state->stream, "250 8BITMIME");
}

/* ok_response - send 250 OK */

static void ok_response(SINK_STATE *state)
{
    smtp_printf(state->stream, "250 Ok");
}

/* mail_response - reset recipient count, send 250 OK */

static void mail_response(SINK_STATE *state)
{
    state->rcpts = 0;
    ok_response(state);
}

/* rcpt_response - bump recipient count, send 250 OK */

static void rcpt_response(SINK_STATE *state)
{
    state->rcpts++;
    ok_response(state);
}

/* data_response - respond to DATA command */

static void data_response(SINK_STATE *state)
{
    state->data_state = ST_CR_LF;
    smtp_printf(state->stream, "354 End data with <CR><LF>.<CR><LF>");
    state->read = data_read;
}

/* data_event - delayed response to DATA command */

static void data_event(int unused_event, char *context)
{
    SINK_STATE *state = (SINK_STATE *) context;

    data_response(state);
}

/* dot_response - response to . command */

static void dot_response(SINK_STATE *state)
{
    if (enable_lmtp) {
	while (state->rcpts-- > 0)	/* XXX this could block */
	    ok_response(state);
    } else {
	ok_response(state);
    }
}

/* quit_response - respond to QUIT command */

static void quit_response(SINK_STATE *state)
{
    smtp_printf(state->stream, "221 Bye");
    if (count) {
	counter++;
	vstream_printf("%d\r", counter);
	vstream_fflush(VSTREAM_OUT);
    }
}

/* data_read - read data from socket */

static int data_read(SINK_STATE *state)
{
    int     ch;
    struct data_trans {
	int     state;
	int     want;
	int     next_state;
    };
    static struct data_trans data_trans[] = {
	ST_ANY, '\r', ST_CR,
	ST_CR, '\n', ST_CR_LF,
	ST_CR_LF, '.', ST_CR_LF_DOT,
	ST_CR_LF_DOT, '\r', ST_CR_LF_DOT_CR,
	ST_CR_LF_DOT_CR, '\n', ST_CR_LF_DOT_CR_LF,
    };
    struct data_trans *dp;

    /*
     * A read may result in EOF, but is never supposed to time out - a time
     * out means that we were trying to read when no data was available.
     */
    for (;;) {
	if ((ch = VSTREAM_GETC(state->stream)) == VSTREAM_EOF)
	    return (-1);
	for (dp = data_trans; dp->state != state->data_state; dp++)
	     /* void */ ;

	/*
	 * Try to match the current character desired by the state machine.
	 * If that fails, try to restart the machine with a match for its
	 * first state.  This covers the case of a CR/LF/CR/LF sequence
	 * (empty line) right before the end of the message data.
	 */
	if (ch == dp->want)
	    state->data_state = dp->next_state;
	else if (ch == data_trans[0].want)
	    state->data_state = data_trans[0].next_state;
	else
	    state->data_state = ST_ANY;
	if (state->data_state == ST_CR_LF_DOT_CR_LF) {
	    if (msg_verbose)
		msg_info(".");
	    dot_response(state);
	    state->read = command_read;
	    state->data_state = ST_ANY;
	    break;
	}

	/*
	 * We must avoid blocking I/O, so get out of here as soon as both the
	 * VSTREAM and kernel read buffers dry up.
	 */
	if (vstream_peek(state->stream) <= 0
	    && peekfd(vstream_fileno(state->stream)) <= 0)
	    return (0);
    }
    return (0);
}

 /*
  * The table of all SMTP commands that we can handle.
  */
typedef struct SINK_COMMAND {
    char   *name;
    void    (*response) (SINK_STATE *);
} SINK_COMMAND;

static SINK_COMMAND command_table[] = {
    "helo", ok_response,
    "ehlo", ehlo_response,
    "lhlo", ehlo_response,
    "mail", mail_response,
    "rcpt", rcpt_response,
    "data", data_response,
    "rset", ok_response,
    "noop", ok_response,
    "vrfy", ok_response,
    "quit", quit_response,
    0,
};

/* command_read - talk the SMTP protocol, server side */

static int command_read(SINK_STATE *state)
{
    char   *command;
    SINK_COMMAND *cmdp;
    int     ch;
    struct cmd_trans {
	int     state;
	int     want;
	int     next_state;
    };
    static struct cmd_trans cmd_trans[] = {
	ST_ANY, '\r', ST_CR,
	ST_CR, '\n', ST_CR_LF,
    };
    struct cmd_trans *cp;

    /*
     * A read may result in EOF, but is never supposed to time out - a time
     * out means that we were trying to read when no data was available.
     */
    for (;;) {
	if ((ch = VSTREAM_GETC(state->stream)) == VSTREAM_EOF)
	    return (-1);

	/*
	 * Sanity check. We don't want to store infinitely long commands.
	 */
	if (VSTRING_LEN(state->buffer) >= var_max_line_length) {
	    msg_warn("command line too long");
	    return (-1);
	}
	VSTRING_ADDCH(state->buffer, ch);

	/*
	 * Try to match the current character desired by the state machine.
	 * If that fails, try to restart the machine with a match for its
	 * first state.
	 */
	for (cp = cmd_trans; cp->state != state->data_state; cp++)
	     /* void */ ;
	if (ch == cp->want)
	    state->data_state = cp->next_state;
	else if (ch == cmd_trans[0].want)
	    state->data_state = cmd_trans[0].next_state;
	else
	    state->data_state = ST_ANY;
	if (state->data_state == ST_CR_LF)
	    break;

	/*
	 * We must avoid blocking I/O, so get out of here as soon as both the
	 * VSTREAM and kernel read buffers dry up.
	 */
	if (vstream_peek(state->stream) <= 0
	    && peekfd(vstream_fileno(state->stream)) <= 0)
	    return (0);
    }

    /*
     * Properly terminate the result, and reset the buffer write pointer for
     * reading the next command. This is ugly, but not as ugly as trying to
     * deal with all the early returns below.
     */
    vstring_truncate(state->buffer, VSTRING_LEN(state->buffer) - 2);
    VSTRING_TERMINATE(state->buffer);
    state->data_state = ST_ANY;
    VSTRING_RESET(state->buffer);

    /*
     * Got a complete command line. Parse it.
     */
    if ((command = strtok(vstring_str(state->buffer), " \t")) == 0) {
	smtp_printf(state->stream, "500 Error: unknown command");
	return (0);
    }
    if (msg_verbose)
	msg_info("%s", command);
    for (cmdp = command_table; cmdp->name != 0; cmdp++)
	if (strcasecmp(command, cmdp->name) == 0)
	    break;
    if (cmdp->name == 0) {
	smtp_printf(state->stream, "500 Error: unknown command");
	return (0);
    }
    if (cmdp->response == data_response && fixed_delay > 0) {
	event_request_timer(data_event, (char *) state, fixed_delay);
    } else {
	cmdp->response(state);
	if (cmdp->response == quit_response)
	    return (-1);
    }
    return (0);
}

/* read_event - handle command or data read events */

static void read_event(int unused_event, char *context)
{
    SINK_STATE *state = (SINK_STATE *) context;

    do {
	switch (vstream_setjmp(state->stream)) {

	default:
	    msg_panic("unknown error reading input");

	case SMTP_ERR_TIME:
	    msg_panic("attempt to read non-readable socket");
	    /* NOTREACHED */

	case SMTP_ERR_EOF:
	    msg_warn("lost connection");
	    disconnect(state);
	    return;

	case 0:
	    if (state->read(state) < 0) {
		if (msg_verbose)
		    msg_info("disconnect");
		disconnect(state);
		return;
	    }
	}
    } while (vstream_peek(state->stream) > 0);
}

/* disconnect - handle disconnection events */

static void disconnect(SINK_STATE *state)
{
    event_disable_readwrite(vstream_fileno(state->stream));
    vstream_fclose(state->stream);
    vstring_free(state->buffer);
    myfree((char *) state);
    if (max_count > 0 && counter >= max_count)
	exit(0);
}

/* connect_event - handle connection events */

static void connect_event(int unused_event, char *context)
{
    int     sock = CAST_CHAR_PTR_TO_INT(context);
    struct sockaddr sa;
    SOCKADDR_SIZE len = sizeof(sa);
    SINK_STATE *state;
    int     fd;

    if ((fd = accept(sock, &sa, &len)) >= 0) {
	if (msg_verbose)
	    msg_info("connect (%s)",
#ifdef AF_LOCAL
		     sa.sa_family == AF_LOCAL ? "AF_LOCAL" :
#else
		     sa.sa_family == AF_UNIX ? "AF_UNIX" :
#endif
		     sa.sa_family == AF_INET ? "AF_INET" :
#ifdef AF_INET6
		     sa.sa_family == AF_INET6 ? "AF_INET6" :
#endif
		     "unknown protocol family");
	non_blocking(fd, NON_BLOCKING);
	state = (SINK_STATE *) mymalloc(sizeof(*state));
	state->stream = vstream_fdopen(fd, O_RDWR);
	state->buffer = vstring_alloc(1024);
	state->read = command_read;
	state->data_state = ST_ANY;
	smtp_timeout_setup(state->stream, var_tmout);
if (pretend_pix)
	smtp_printf(state->stream, "220 ********");
else
	smtp_printf(state->stream, "220 %s ESMTP", var_myhostname);
	event_enable_read(fd, read_event, (char *) state);
    }
}

/* usage - explain */

static void usage(char *myname)
{
    msg_fatal("usage: %s [-cLpPv] [-n count] [-w delay] [host]:port backlog", myname);
}

int     main(int argc, char **argv)
{
    int     sock;
    int     backlog;
    int     ch;

    /*
     * Initialize diagnostics.
     */
    msg_vstream_init(argv[0], VSTREAM_ERR);

    /*
     * Parse JCL.
     */
    while ((ch = GETOPT(argc, argv, "cLn:pPvw:")) > 0) {
	switch (ch) {
	case 'c':
	    count++;
	    break;
	case 'L':
	    enable_lmtp = 1;
	    break;
	case 'n':
	    max_count = atoi(optarg);
	    break;
	case 'p':
	    disable_pipelining = 1;
	    break;
	case 'P':
	    pretend_pix=1;
	    break;
	case 'v':
	    msg_verbose++;
	    break;
	case 'w':
	    if ((fixed_delay = atoi(optarg)) <= 0)
		usage(argv[0]);
	    break;
	default:
	    usage(argv[0]);
	}
    }
    if (argc - optind != 2)
	usage(argv[0]);
    if ((backlog = atoi(argv[optind + 1])) <= 0)
	usage(argv[0]);

    /*
     * Initialize.
     */
    var_myhostname = "smtp-sink";
    if (strncmp(argv[optind], "unix:", 5) == 0) {
	sock = unix_listen(argv[optind] + 5, backlog, BLOCKING);
    } else {
	if (strncmp(argv[optind], "inet:", 5) == 0)
	    argv[optind] += 5;
	sock = inet_listen(argv[optind], backlog, BLOCKING);
    }

    /*
     * Start the event handler.
     */
    event_enable_read(sock, connect_event, CAST_INT_TO_CHAR_PTR(sock));
    for (;;)
	event_loop(-1);
}
