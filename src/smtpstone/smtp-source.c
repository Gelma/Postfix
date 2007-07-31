/*++
/* NAME
/*	smtp-source 1
/* SUMMARY
/*	multi-threaded SMTP/LMTP test generator
/* SYNOPSIS
/* .fi
/*	\fBsmtp-source\fR [\fIoptions\fR] [\fBinet:\fR]\fIhost\fR[:\fIport\fR]
/*
/*	\fBsmtp-source\fR [\fIoptions\fR] \fBunix:\fIpathname\fR
/* DESCRIPTION
/*	\fBsmtp-source\fR connects to the named \fIhost\fR and TCP \fIport\fR
/*	(default: port 25)
/*	and sends one or more messages to it, either sequentially
/*	or in parallel. The program speaks either SMTP (default) or
/*	LMTP.
/*	Connections can be made to UNIX-domain and IPv4 or IPv6 servers.
/*	IPv4 and IPv6 are the default.
/*
/*	Note: this is an unsupported test program. No attempt is made
/*	to maintain compatibility between successive versions.
/*
/*	Arguments:
/* .IP \fB-4\fR
/*	Connect to the server with IPv4. This option has no effect when
/*	Postfix is built without IPv6 support.
/* .IP \fB-6\fR
/*	Connect to the server with IPv6. This option is not available when
/*	Postfix is built without IPv6 support.
/* .IP \fB-c\fR
/*	Display a running counter that is incremented each time
/*	an SMTP DATA command completes.
/* .IP "\fB-C \fIcount\fR"
/*	When a host sends RESET instead of SYN|ACK, try \fIcount\fR times
/*	before giving up. The default count is 1. Specify a larger count in
/*	order to work around a problem with TCP/IP stacks that send RESET
/*	when the listen queue is full.
/* .IP \fB-d\fR
/*	Don't disconnect after sending a message; send the next
/*	message over the same connection.
/* .IP "\fB-f \fIfrom\fR"
/*	Use the specified sender address (default: <foo@myhostname>).
/* .IP "\fB-l \fIlength\fR"
/*	Send \fIlength\fR bytes as message payload. The length does not
/*	include message headers.
/* .IP \fB-L\fR
/*	Speak LMTP rather than SMTP.
/* .IP "\fB-m \fImessage_count\fR"
/*	Send the specified number of messages (default: 1).
/* .IP "\fB-M \fImyhostname\fR"
/*	Use the specified hostname or [address] in the HELO command
/*	and in the default sender and recipient addresses, instead
/*	of the machine hostname.
/* .IP "\fB-N\fR"
/*	Prepend a non-repeating sequence number to each recipient
/*	address. This avoids the artificial 100% hit rate in the
/*	resolve and rewrite client caches and exercises the
/*	trivial-rewrite daemon, better approximating Postfix
/*	performance under real-life work-loads.
/* .IP \fB-o\fR
/*	Old mode: don't send HELO, and don't send message headers.
/* .IP "\fB-r \fIrecipient_count\fR"
/*	Send the specified number of recipients per transaction (default: 1).
/*	Recipient names are generated by prepending a number to the
/*	recipient address.
/* .IP "\fB-s \fIsession_count\fR"
/*	Run the specified number of SMTP sessions in parallel (default: 1).
/* .IP "\fB-S \fIsubject\fR"
/*	Send mail with the named subject line (default: none).
/* .IP "\fB-t \fIto\fR"
/*	Use the specified recipient address (default: <foo@myhostname>).
/* .IP "\fB-R \fIinterval\fR"
/*	Wait for a random period of time 0 <= n <= interval between messages.
/*	Suspending one thread does not affect other delivery threads.
/* .IP \fB-v\fR
/*	Make the program more verbose, for debugging purposes.
/* .IP "\fB-w \fIinterval\fR"
/*	Wait a fixed time between messages.
/*	Suspending one thread does not affect other delivery threads.
/* .IP [\fBinet:\fR]\fIhost\fR[:\fIport\fR]
/*	Connect via TCP to host \fIhost\fR, port \fIport\fR. The default
/*	port is \fBsmtp\fR.
/* .IP \fBunix:\fIpathname\fR
/*	Connect to the UNIX-domain socket at \fIpathname\fR.
/* BUGS
/*	No SMTP command pipelining support.
/* SEE ALSO
/*	smtp-sink(1), SMTP/LMTP message dump
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
#include <netinet/in.h>
#include <sys/un.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

/* Utility library. */

#include <msg.h>
#include <msg_vstream.h>
#include <vstring.h>
#include <vstream.h>
#include <vstring_vstream.h>
#include <get_hostname.h>
#include <split_at.h>
#include <connect.h>
#include <mymalloc.h>
#include <events.h>
#include <iostuff.h>
#include <sane_connect.h>
#include <host_port.h>
#include <myaddrinfo.h>
#include <inet_proto.h>
#include <valid_hostname.h>
#include <valid_mailhost_addr.h>

/* Global library. */

#include <smtp_stream.h>
#include <mail_date.h>
#include <mail_version.h>

/* Application-specific. */

 /*
  * Per-session data structure with state.
  * 
  * This software can maintain multiple parallel connections to the same SMTP
  * server. However, it makes no more than one connection request at a time
  * to avoid overwhelming the server with SYN packets and having to back off.
  * Back-off would screw up the benchmark. Pending connection requests are
  * kept in a linear list.
  */
typedef struct SESSION {
    int     xfer_count;			/* # of xfers in session */
    int     rcpt_done;			/* # of recipients done */
    int     rcpt_count;			/* # of recipients to go */
    VSTREAM *stream;			/* open connection */
    int     connect_count;		/* # of connect()s to retry */
    struct SESSION *next;		/* connect() queue linkage */
} SESSION;

static SESSION *last_session;		/* connect() queue tail */

 /*
  * Structure with broken-up SMTP server response.
  */
typedef struct {			/* server response */
    int     code;			/* status */
    char   *str;			/* text */
    VSTRING *buf;			/* origin of text */
} RESPONSE;

static VSTRING *buffer;
static int var_line_limit = 10240;
static int var_timeout = 300;
static const char *var_myhostname;
static int session_count;
static int message_count = 1;
static struct sockaddr_storage ss;

#undef sun
static struct sockaddr_un sun;
static struct sockaddr *sa;
static int sa_length;
static int recipients = 1;
static char *defaddr;
static char *recipient;
static char *sender;
static char *message_data;
static int message_length;
static int disconnect = 1;
static int count = 0;
static int counter = 0;
static int send_helo_first = 1;
static int send_headers = 1;
static int connect_count = 1;
static int random_delay = 0;
static int fixed_delay = 0;
static int talk_lmtp = 0;
static char *subject = 0;
static int number_rcpts = 0;

static void enqueue_connect(SESSION *);
static void start_connect(SESSION *);
static void connect_done(int, char *);
static void read_banner(int, char *);
static void send_helo(SESSION *);
static void helo_done(int, char *);
static void send_mail(SESSION *);
static void mail_done(int, char *);
static void send_rcpt(int, char *);
static void rcpt_done(int, char *);
static void send_data(int, char *);
static void data_done(int, char *);
static void dot_done(int, char *);
static void send_quit(SESSION *);
static void quit_done(int, char *);

/* random_interval - generate a random value in 0 .. (small) interval */

static int random_interval(int interval)
{
    return (rand() % (interval + 1));
}

/* command - send an SMTP command */

static void command(VSTREAM *stream, char *fmt,...)
{
    VSTRING *buf;
    va_list ap;

    /*
     * Optionally, log the command before actually sending, so we can see
     * what the program is trying to do.
     */
    if (msg_verbose) {
	buf = vstring_alloc(100);
	va_start(ap, fmt);
	vstring_vsprintf(buf, fmt, ap);
	va_end(ap);
	msg_info("%s", vstring_str(buf));
	vstring_free(buf);
    }
    va_start(ap, fmt);
    smtp_vprintf(stream, fmt, ap);
    va_end(ap);
    smtp_flush(stream);
}

/* socket_error - look up and reset the last socket error */

static int socket_error(int sock)
{
    int     error;
    SOCKOPT_SIZE error_len;

    /*
     * Some Solaris 2 versions have getsockopt() itself return the error,
     * instead of returning it via the parameter list.
     */
    error = 0;
    error_len = sizeof(error);
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (char *) &error, &error_len) < 0)
	return (-1);
    if (error) {
	errno = error;
	return (-1);
    }

    /*
     * No problems.
     */
    return (0);
}

/* response - read and process SMTP server response */

static RESPONSE *response(VSTREAM *stream, VSTRING *buf)
{
    static RESPONSE rdata;
    int     more;
    char   *cp;

    /*
     * Initialize the response data buffer. Defend against a denial of
     * service attack by limiting the amount of multi-line text that we are
     * willing to store.
     */
    if (rdata.buf == 0) {
	rdata.buf = vstring_alloc(100);
	vstring_ctl(rdata.buf, VSTRING_CTL_MAXLEN, (ssize_t) var_line_limit, 0);
    }

    /*
     * Censor out non-printable characters in server responses. Concatenate
     * multi-line server responses. Separate the status code from the text.
     * Leave further parsing up to the application.
     */
#define BUF ((char *) vstring_str(buf))
    VSTRING_RESET(rdata.buf);
    for (;;) {
	smtp_get(buf, stream, var_line_limit);
	for (cp = BUF; *cp != 0; cp++)
	    if (!ISPRINT(*cp) && !ISSPACE(*cp))
		*cp = '?';
	cp = BUF;
	if (msg_verbose)
	    msg_info("<<< %s", cp);
	while (ISDIGIT(*cp))
	    cp++;
	rdata.code = (cp - BUF == 3 ? atoi(BUF) : 0);
	if ((more = (*cp == '-')) != 0)
	    cp++;
	while (ISSPACE(*cp))
	    cp++;
	vstring_strcat(rdata.buf, cp);
	if (more == 0)
	    break;
	VSTRING_ADDCH(rdata.buf, '\n');
    }
    VSTRING_TERMINATE(rdata.buf);
    rdata.str = vstring_str(rdata.buf);
    return (&rdata);
}

/* exception_text - translate exceptions from the smtp_stream module */

static char *exception_text(int except)
{
    switch (except) {
	case SMTP_ERR_EOF:
	return ("lost connection");
    case SMTP_ERR_TIME:
	return ("timeout");
    default:
	msg_panic("exception_text: unknown exception %d", except);
    }
    /* NOTREACHED */
}

/* startup - connect to server but do not wait */

static void startup(SESSION *session)
{
    if (message_count-- <= 0) {
	myfree((char *) session);
	session_count--;
	return;
    }
    if (session->stream == 0) {
	enqueue_connect(session);
    } else {
	send_mail(session);
    }
}

/* start_event - invoke startup from timer context */

static void start_event(int unused_event, char *context)
{
    SESSION *session = (SESSION *) context;

    startup(session);
}

/* start_another - start another session */

static void start_another(SESSION *session)
{
    if (random_delay > 0) {
	event_request_timer(start_event, (char *) session,
			    random_interval(random_delay));
    } else if (fixed_delay > 0) {
	event_request_timer(start_event, (char *) session, fixed_delay);
    } else {
	startup(session);
    }
}

/* enqueue_connect - queue a connection request */

static void enqueue_connect(SESSION *session)
{
    session->next = 0;
    if (last_session == 0) {
	last_session = session;
	start_connect(session);
    } else {
	last_session->next = session;
	last_session = session;
    }
}

/* dequeue_connect - connection request completed */

static void dequeue_connect(SESSION *session)
{
    if (session == last_session) {
	if (session->next != 0)
	    msg_panic("dequeue_connect: queue ends after last");
	last_session = 0;
    } else {
	if (session->next == 0)
	    msg_panic("dequeue_connect: queue ends before last");
	start_connect(session->next);
    }
}

/* fail_connect - handle failed startup */

static void fail_connect(SESSION *session)
{
    if (session->connect_count-- == 1)
	msg_fatal("connect: %m");
    msg_warn("connect: %m");
    event_disable_readwrite(vstream_fileno(session->stream));
    vstream_fclose(session->stream);
    session->stream = 0;
#ifdef MISSING_USLEEP
    doze(10);
#else
    usleep(10);
#endif
    start_connect(session);
}

/* start_connect - start TCP handshake */

static void start_connect(SESSION *session)
{
    int     fd;
    struct linger linger;

    /*
     * Some systems don't set the socket error when connect() fails early
     * (loopback) so we must deal with the error immediately, rather than
     * retrieving it later with getsockopt(). We can't use MSG_PEEK to
     * distinguish between server disconnect and connection refused.
     */
    if ((fd = socket(sa->sa_family, SOCK_STREAM, 0)) < 0)
	msg_fatal("socket: %m");
    (void) non_blocking(fd, NON_BLOCKING);
    linger.l_onoff = 1;
    linger.l_linger = 0;
    if (setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *) &linger,
		   sizeof(linger)) < 0)
	msg_warn("setsockopt SO_LINGER %d: %m", linger.l_linger);
    session->stream = vstream_fdopen(fd, O_RDWR);
    event_enable_write(fd, connect_done, (char *) session);
    smtp_timeout_setup(session->stream, var_timeout);
    if (sane_connect(fd, sa, sa_length) < 0 && errno != EINPROGRESS)
	fail_connect(session);
}

/* connect_done - send message sender info */

static void connect_done(int unused_event, char *context)
{
    SESSION *session = (SESSION *) context;
    int     fd = vstream_fileno(session->stream);

    /*
     * Try again after some delay when the connection failed, in case they
     * run a Mickey Mouse protocol stack.
     */
    if (socket_error(fd) < 0) {
	fail_connect(session);
    } else {
	non_blocking(fd, BLOCKING);
	/* Disable write events. */
	event_disable_readwrite(fd);
	event_enable_read(fd, read_banner, (char *) session);
	dequeue_connect(session);
	/* Avoid poor performance when TCP MSS > VSTREAM_BUFSIZE. */
	if (sa->sa_family == AF_INET
#ifdef AF_INET6
	    || sa->sa_family == AF_INET6
#endif
	    )
	    vstream_tweak_tcp(session->stream);
    }
}

/* read_banner - receive SMTP server greeting */

static void read_banner(int unused_event, char *context)
{
    SESSION *session = (SESSION *) context;
    RESPONSE *resp;
    int     except;

    /*
     * Prepare for disaster.
     */
    if ((except = vstream_setjmp(session->stream)) != 0)
	msg_fatal("%s while reading server greeting", exception_text(except));

    /*
     * Read and parse the server's SMTP greeting banner.
     */
    if (((resp = response(session->stream, buffer))->code / 100) != 2)
	msg_fatal("bad startup: %d %s", resp->code, resp->str);

    /*
     * Send helo or send the envelope sender address.
     */
    if (send_helo_first)
	send_helo(session);
    else
	send_mail(session);
}

/* send_helo - send hostname */

static void send_helo(SESSION *session)
{
    int     except;
    const char *NOCLOBBER protocol = (talk_lmtp ? "LHLO" : "HELO");

    /*
     * Send the standard greeting with our hostname
     */
    if ((except = vstream_setjmp(session->stream)) != 0)
	msg_fatal("%s while sending %s", exception_text(except), protocol);

    command(session->stream, "%s %s", protocol, var_myhostname);

    /*
     * Prepare for the next event.
     */
    event_enable_read(vstream_fileno(session->stream), helo_done, (char *) session);
}

/* helo_done - handle HELO response */

static void helo_done(int unused_event, char *context)
{
    SESSION *session = (SESSION *) context;
    RESPONSE *resp;
    int     except;
    const char *protocol = (talk_lmtp ? "LHLO" : "HELO");

    /*
     * Get response to HELO command.
     */
    if ((except = vstream_setjmp(session->stream)) != 0)
	msg_fatal("%s while sending %s", exception_text(except), protocol);

    if ((resp = response(session->stream, buffer))->code / 100 != 2)
	msg_fatal("%s rejected: %d %s", protocol, resp->code, resp->str);

    send_mail(session);
}

/* send_mail - send envelope sender */

static void send_mail(SESSION *session)
{
    int     except;

    /*
     * Send the envelope sender address.
     */
    if ((except = vstream_setjmp(session->stream)) != 0)
	msg_fatal("%s while sending sender", exception_text(except));

    command(session->stream, "MAIL FROM:<%s>", sender);

    /*
     * Prepare for the next event.
     */
    event_enable_read(vstream_fileno(session->stream), mail_done, (char *) session);
}

/* mail_done - handle MAIL response */

static void mail_done(int unused, char *context)
{
    SESSION *session = (SESSION *) context;
    RESPONSE *resp;
    int     except;

    /*
     * Get response to MAIL command.
     */
    if ((except = vstream_setjmp(session->stream)) != 0)
	msg_fatal("%s while sending sender", exception_text(except));

    if ((resp = response(session->stream, buffer))->code / 100 != 2)
	msg_fatal("sender rejected: %d %s", resp->code, resp->str);

    session->rcpt_count = recipients;
    session->rcpt_done = 0;
    send_rcpt(unused, context);
}

/* send_rcpt - send recipient address */

static void send_rcpt(int unused_event, char *context)
{
    SESSION *session = (SESSION *) context;
    int     except;

    /*
     * Send envelope recipient address.
     */
    if ((except = vstream_setjmp(session->stream)) != 0)
	msg_fatal("%s while sending recipient", exception_text(except));

    if (session->rcpt_count > 1 || number_rcpts > 0)
	command(session->stream, "RCPT TO:<%d%s>",
		number_rcpts ? number_rcpts++ : session->rcpt_count,
		recipient);
    else
	command(session->stream, "RCPT TO:<%s>", recipient);
    session->rcpt_count--;
    session->rcpt_done++;

    /*
     * Prepare for the next event.
     */
    event_enable_read(vstream_fileno(session->stream), rcpt_done, (char *) session);
}

/* rcpt_done - handle RCPT completion */

static void rcpt_done(int unused, char *context)
{
    SESSION *session = (SESSION *) context;
    RESPONSE *resp;
    int     except;

    /*
     * Get response to RCPT command.
     */
    if ((except = vstream_setjmp(session->stream)) != 0)
	msg_fatal("%s while sending recipient", exception_text(except));

    if ((resp = response(session->stream, buffer))->code / 100 != 2)
	msg_fatal("recipient rejected: %d %s", resp->code, resp->str);

    /*
     * Send another RCPT command or send DATA.
     */
    if (session->rcpt_count > 0)
	send_rcpt(unused, context);
    else
	send_data(unused, context);
}

/* send_data - send DATA command */

static void send_data(int unused_event, char *context)
{
    SESSION *session = (SESSION *) context;
    int     except;

    /*
     * Request data transmission.
     */
    if ((except = vstream_setjmp(session->stream)) != 0)
	msg_fatal("%s while sending DATA command", exception_text(except));
    command(session->stream, "DATA");

    /*
     * Prepare for the next event.
     */
    event_enable_read(vstream_fileno(session->stream), data_done, (char *) session);
}

/* data_done - send message content */

static void data_done(int unused_event, char *context)
{
    SESSION *session = (SESSION *) context;
    RESPONSE *resp;
    int     except;
    static const char *mydate;
    static int mypid;

    /*
     * Get response to DATA command.
     */
    if ((except = vstream_setjmp(session->stream)) != 0)
	msg_fatal("%s while sending DATA command", exception_text(except));
    if ((resp = response(session->stream, buffer))->code != 354)
	msg_fatal("data %d %s", resp->code, resp->str);

    /*
     * Send basic header to keep mailers that bother to examine them happy.
     */
    if (send_headers) {
	if (mydate == 0) {
	    mydate = mail_date(time((time_t *) 0));
	    mypid = getpid();
	}
	smtp_printf(session->stream, "From: <%s>", sender);
	smtp_printf(session->stream, "To: <%s>", recipient);
	smtp_printf(session->stream, "Date: %s", mydate);
	smtp_printf(session->stream, "Message-Id: <%04x.%04x.%04x@%s>",
		    mypid, vstream_fileno(session->stream), message_count, var_myhostname);
	if (subject)
	    smtp_printf(session->stream, "Subject: %s", subject);
	smtp_fputs("", 0, session->stream);
    }

    /*
     * Send some garbage.
     */
    if ((except = vstream_setjmp(session->stream)) != 0)
	msg_fatal("%s while sending message", exception_text(except));
    if (message_length == 0) {
	smtp_fputs("La de da de da 1.", 17, session->stream);
	smtp_fputs("La de da de da 2.", 17, session->stream);
	smtp_fputs("La de da de da 3.", 17, session->stream);
	smtp_fputs("La de da de da 4.", 17, session->stream);
    } else {

	/*
	 * XXX This may cause the process to block with message content
	 * larger than VSTREAM_BUFIZ bytes.
	 */
	smtp_fputs(message_data, message_length, session->stream);
    }

    /*
     * Send end of message and process the server response.
     */
    command(session->stream, ".");

    /*
     * Update the running counter.
     */
    if (count) {
	counter++;
	vstream_printf("%d\r", counter);
	vstream_fflush(VSTREAM_OUT);
    }

    /*
     * Prepare for the next event.
     */
    event_enable_read(vstream_fileno(session->stream), dot_done, (char *) session);
}

/* dot_done - send QUIT */

static void dot_done(int unused_event, char *context)
{
    SESSION *session = (SESSION *) context;
    RESPONSE *resp;
    int     except;

    /*
     * Get response to "." command.
     */
    if ((except = vstream_setjmp(session->stream)) != 0)
	msg_fatal("%s while sending message", exception_text(except));
    do {					/* XXX this could block */
	if ((resp = response(session->stream, buffer))->code / 100 != 2)
	    msg_fatal("data %d %s", resp->code, resp->str);
    } while (talk_lmtp && --session->rcpt_done > 0);
    session->xfer_count++;

    /*
     * Say goodbye or send the next message.
     */
    if (disconnect || message_count < 1) {
	send_quit(session);
    } else {
	event_disable_readwrite(vstream_fileno(session->stream));
	start_another(session);
    }
}

/* send_quit - send QUIT command */

static void send_quit(SESSION *session)
{
    command(session->stream, "QUIT");
    event_enable_read(vstream_fileno(session->stream), quit_done, (char *) session);
}

/* quit_done - disconnect */

static void quit_done(int unused_event, char *context)
{
    SESSION *session = (SESSION *) context;

    (void) response(session->stream, buffer);
    event_disable_readwrite(vstream_fileno(session->stream));
    vstream_fclose(session->stream);
    session->stream = 0;
    start_another(session);
}

/* usage - explain */

static void usage(char *myname)
{
    msg_fatal("usage: %s -cdLNov -s sess -l msglen -m msgs -C count -M myhostname -f from -t to -r rcptcount -R delay -w delay host[:port]", myname);
}

MAIL_VERSION_STAMP_DECLARE;

/* main - parse JCL and start the machine */

int     main(int argc, char **argv)
{
    SESSION *session;
    char   *host;
    char   *port;
    char   *path;
    int     path_len;
    int     sessions = 1;
    int     ch;
    int     i;
    char   *buf;
    const char *parse_err;
    struct addrinfo *res;
    int     aierr;
    const char *protocols = INET_PROTO_NAME_ALL;
    INET_PROTO_INFO *proto_info;

    /*
     * Fingerprint executables and core dumps.
     */
    MAIL_VERSION_STAMP_ALLOCATE;

    signal(SIGPIPE, SIG_IGN);
    msg_vstream_init(argv[0], VSTREAM_ERR);

    /*
     * Parse JCL.
     */
    while ((ch = GETOPT(argc, argv, "46cC:df:l:Lm:M:Nor:R:s:S:t:vw:")) > 0) {
	switch (ch) {
	case '4':
	    protocols = INET_PROTO_NAME_IPV4;
	    break;
	case '6':
	    protocols = INET_PROTO_NAME_IPV6;
	    break;
	case 'c':
	    count++;
	    break;
	case 'C':
	    if ((connect_count = atoi(optarg)) <= 0)
		msg_fatal("bad connection count: %s", optarg);
	    break;
	case 'd':
	    disconnect = 0;
	    break;
	case 'f':
	    sender = optarg;
	    break;
	case 'l':
	    if ((message_length = atoi(optarg)) <= 0)
		msg_fatal("bad message length: %s", optarg);
	    message_data = mymalloc(message_length);
	    memset(message_data, 'X', message_length);
	    for (i = 80; i < message_length; i += 80) {
		message_data[i - 80] = "0123456789"[(i / 80) % 10];
		message_data[i - 2] = '\r';
		message_data[i - 1] = '\n';
	    }
	    break;
	case 'L':
	    talk_lmtp = 1;
	    break;
	case 'm':
	    if ((message_count = atoi(optarg)) <= 0)
		msg_fatal("bad message count: %s", optarg);
	    break;
	case 'M':
	    if (*optarg == '[') {
		if (!valid_mailhost_literal(optarg, DO_GRIPE))
		    msg_fatal("bad address literal: %s", optarg);
	    } else {
		if (!valid_hostname(optarg, DO_GRIPE))
		    msg_fatal("bad hostname: %s", optarg);
	    }
	    var_myhostname = optarg;
	    break;
	case 'N':
	    number_rcpts = 1;
	    break;
	case 'o':
	    send_helo_first = 0;
	    send_headers = 0;
	    break;
	case 'r':
	    if ((recipients = atoi(optarg)) <= 0)
		msg_fatal("bad recipient count: %s", optarg);
	    break;
	case 'R':
	    if (fixed_delay > 0)
		msg_fatal("do not use -w and -R options at the same time");
	    if ((random_delay = atoi(optarg)) <= 0)
		msg_fatal("bad random delay: %s", optarg);
	    break;
	case 's':
	    if ((sessions = atoi(optarg)) <= 0)
		msg_fatal("bad session count: %s", optarg);
	    break;
	case 'S':
	    subject = optarg;
	    break;
	case 't':
	    recipient = optarg;
	    break;
	case 'v':
	    msg_verbose++;
	    break;
	case 'w':
	    if (random_delay > 0)
		msg_fatal("do not use -w and -R options at the same time");
	    if ((fixed_delay = atoi(optarg)) <= 0)
		msg_fatal("bad fixed delay: %s", optarg);
	    break;
	default:
	    usage(argv[0]);
	}
    }
    if (argc - optind != 1)
	usage(argv[0]);

    if (random_delay > 0)
	srand(getpid());

    /*
     * Translate endpoint address to internal form.
     */
    proto_info = inet_proto_init("protocols", protocols);
    if (strncmp(argv[optind], "unix:", 5) == 0) {
	path = argv[optind] + 5;
	path_len = strlen(path);
	if (path_len >= (int) sizeof(sun.sun_path))
	    msg_fatal("unix-domain name too long: %s", path);
	memset((char *) &sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
#ifdef HAS_SUN_LEN
	sun.sun_len = path_len + 1;
#endif
	memcpy(sun.sun_path, path, path_len);
	sa = (struct sockaddr *) & sun;
	sa_length = sizeof(sun);
    } else {
	if (strncmp(argv[optind], "inet:", 5) == 0)
	    argv[optind] += 5;
	buf = mystrdup(argv[optind]);
	if ((parse_err = host_port(buf, &host, (char *) 0, &port, "smtp")) != 0)
	    msg_fatal("%s: %s", argv[optind], parse_err);
	if ((aierr = hostname_to_sockaddr(host, port, SOCK_STREAM, &res)) != 0)
	    msg_fatal("%s: %s", argv[optind], MAI_STRERROR(aierr));
	myfree(buf);
	sa = (struct sockaddr *) & ss;
	if (res->ai_addrlen > sizeof(ss))
	    msg_fatal("address length %d > buffer length %d",
		      (int) res->ai_addrlen, (int) sizeof(ss));
	memcpy((char *) sa, res->ai_addr, res->ai_addrlen);
	sa_length = res->ai_addrlen;
#ifdef HAS_SA_LEN
	sa->sa_len = sa_length;
#endif
	freeaddrinfo(res);
    }

    /*
     * Make sure the SMTP server cannot run us out of memory by sending
     * never-ending lines of text.
     */
    if (buffer == 0) {
	buffer = vstring_alloc(100);
	vstring_ctl(buffer, VSTRING_CTL_MAXLEN, (ssize_t) var_line_limit, 0);
    }

    /*
     * Make sure we have sender and recipient addresses.
     */
    if (var_myhostname == 0)
	var_myhostname = get_hostname();
    if (sender == 0 || recipient == 0) {
	vstring_sprintf(buffer, "foo@%s", var_myhostname);
	defaddr = mystrdup(vstring_str(buffer));
	if (sender == 0)
	    sender = defaddr;
	if (recipient == 0)
	    recipient = defaddr;
    }

    /*
     * Start sessions.
     */
    while (sessions-- > 0) {
	session = (SESSION *) mymalloc(sizeof(*session));
	session->stream = 0;
	session->xfer_count = 0;
	session->connect_count = connect_count;
	session->next = 0;
	session_count++;
	startup(session);
    }
    for (;;) {
	event_loop(-1);
	if (session_count <= 0 && message_count <= 0) {
	    if (count) {
		VSTREAM_PUTC('\n', VSTREAM_OUT);
		vstream_fflush(VSTREAM_OUT);
	    }
	    exit(0);
	}
    }
}
