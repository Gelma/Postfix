/*++
/* NAME
/*	sendmail 1
/* SUMMARY
/*	Postfix to Sendmail compatibility interface
/* SYNOPSIS
/*	\fBsendmail\fR [\fIoption ...\fR] [\fIrecipient ...\fR]
/*
/*	\fBmailq\fR
/*	\fBsendmail -bp\fR
/*
/*	\fBnewaliases\fR
/*	\fBsendmail -I\fR
/* DESCRIPTION
/*	The \fBsendmail\fR program implements the Postfix to Sendmail
/*	compatibility interface.
/*	For the sake of compatibility with existing applications, some
/*	Sendmail command-line options are recognized but silently ignored.
/*
/*	By default, \fBsendmail\fR reads a message from standard input
/*	until EOF or until it reads a line with only a \fB.\fR character,
/*	and arranges for delivery.  \fBsendmail\fR relies on the
/*	\fBpostdrop\fR(1) command to create a queue file in the \fBmaildrop\fR
/*	directory.
/*
/*	Specific command aliases are provided for other common modes of
/*	operation:
/* .IP \fBmailq\fR
/*	List the mail queue. Each entry shows the queue file ID, message
/*	size, arrival time, sender, and the recipients that still need to
/*	be delivered.  If mail could not be delivered upon the last attempt,
/*	the reason for failure is shown. This mode of operation is implemented
/*	by executing the \fBpostqueue\fR(1) command.
/* .IP \fBnewaliases\fR
/*	Initialize the alias database.  If no input file is specified (with
/*	the \fB-oA\fR option, see below), the program processes the file(s)
/*	specified with the \fBalias_database\fR configuration parameter.
/*	If no alias database type is specified, the program uses the type
/*	specified with the \fBdefault_database_type\fR configuration parameter.
/*	This mode of operation is implemented by running the \fBpostalias\fR(1)
/*	command.
/* .sp
/*	Note: it may take a minute or so before an alias database update
/*	becomes visible. Use the \fBpostfix reload\fR command to eliminate
/*	this delay.
/* .PP
/*	These and other features can be selected by specifying the
/*	appropriate combination of command-line options. Some features are
/*	controlled by parameters in the \fBmain.cf\fR configuration file.
/*
/*	The following options are recognized:
/* .IP "\fB-Am\fR (ignored)"
/* .IP "\fB-Ac\fR (ignored)"
/*	Postfix sendmail uses the same configuration file regardless of
/*	whether or not a message is an initial submission.
/* .IP "\fB-B \fIbody_type\fR"
/*	The message body MIME type: \fB7BIT\fR or \fB8BITMIME\fR.
/* .IP "\fB-C \fIconfig_file\fR (ignored :-)"
/*	The path name of the \fBsendmail.cf\fR file. Postfix configuration
/*	files are kept in \fB/etc/postfix\fR.
/* .IP "\fB-F \fIfull_name\fR
/*	Set the sender full name. This is used only with messages that
/*	have no \fBFrom:\fR message header.
/* .IP "\fB-G\fR (ignored)"
/*	Gateway (relay) submission, as opposed to initial user submission.
/* .IP \fB-I\fR
/*	Initialize alias database. See the \fBnewaliases\fR
/*	command above.
/* .IP "\fB-L \fIlabel\fR (ignored)"
/*	The logging label. Use the \fBsyslog_name\fR configuration
/*	parameter instead.
/* .IP "\fB-N \fIdsn\fR (ignored)"
/*	Delivery status notification control. Currently, Postfix does
/*	not implement \fBDSN\fR.
/* .IP "\fB-R \fIreturn_limit\fR (ignored)"
/*	Limit the size of bounced mail. Use the \fBbounce_size_limit\fR
/*	configuration parameter instead.
/* .IP "\fB-X \fIlog_file\fR (ignored)"
/*	Log mailer traffic. Use the \fBdebug_peer_list\fR and
/*	\fBdebug_peer_level\fR configuration parameters instead.
/* .IP "\fB-U\fR (ignored)"
/*	Initial user submission.
/* .IP \fB-V\fR
/*	Variable Envelope Return Path. Given an envelope sender address
/*	of the form \fIowner-listname\fR@\fIorigin\fR, each recipient
/*	\fIuser\fR@\fIdomain\fR receives mail with a personalized envelope
/*	sender address.
/* .sp
/*	By default, the personalized envelope sender address is
/*	\fIowner-listname\fB+\fIuser\fB=\fIdomain\fR@\fIorigin\fR. The default
/*	\fB+\fR and \fB=\fR characters are configurable with the
/*	\fBdefault_verp_delimiters\fR configuration parameter.
/* .IP \fB-V\fIxy\fR
/*	As \fB-V\fR, but uses \fIx\fR and \fIy\fR as the VERP delimiter
/*	characters, instead of the characters specified with the
/*	\fBdefault_verp_delimiters\fR configuration parameter.
/* .IP \fB-bd\fR
/*	Go into daemon mode. This mode of operation is implemented by
/*	executing the \fBpostfix start\fR command.
/* .IP \fB-bi\fR
/*	Initialize alias database. See the \fBnewaliases\fR
/*	command above.
/* .IP \fB-bm\fR
/*	Read mail from standard input and arrange for delivery.
/*	This is the default mode of operation.
/* .IP \fB-bp\fR
/*	List the mail queue. See the \fBmailq\fR command above.
/* .IP \fB-bs\fR
/*	Stand-alone SMTP server mode. Read SMTP commands from
/*	standard input, and write responses to standard output.
/*	In stand-alone SMTP server mode, UCE restrictions and
/*	access controls are disabled by default. To enable them,
/*	run the process as the \fBmail_owner\fR user.
/* .sp
/*	This mode of operation is implemented by running the
/*	\fBsmtpd\fR(8) daemon.
/* .IP "\fB-f \fIsender\fR"
/*	Set the envelope sender address. This is the address where
/*	delivery problems are sent to, unless the message contains an
/*	\fBErrors-To:\fR message header.
/* .IP "\fB-h \fIhop_count\fR (ignored)"
/*	Hop count limit. Use the \fBhopcount_limit\fR configuration
/*	parameter instead.
/* .IP "\fB-i\fR"
/*	When reading a message from standard input, don\'t treat a line
/*	with only a \fB.\fR character as the end of input.
/* .IP "\fB-m\fR (ignored)"
/*	Backwards compatibility.
/* .IP "\fB-n\fR (ignored)"
/*	Backwards compatibility.
/* .IP "\fB-oA\fIalias_database\fR"
/*	Non-default alias database. Specify \fIpathname\fR or
/*	\fItype\fR:\fIpathname\fR. See \fBpostalias\fR(1) for
/*	details.
/* .IP "\fB-o7\fR (ignored)"
/* .IP "\fB-o8\fR (ignored)"
/*	To send 8-bit or binary content, use an appropriate MIME encapsulation
/*	and specify the appropriate \fB-B\fR command-line option.
/* .IP "\fB-oi\fR"
/*	When reading a message from standard input, don\'t treat a line
/*	with only a \fB.\fR character as the end of input.
/* .IP "\fB-om\fR (ignored)"
/*	The sender is never eliminated from alias etc. expansions.
/* .IP "\fB-o \fIx value\fR (ignored)"
/*	Set option \fIx\fR to \fIvalue\fR. Use the equivalent
/*	configuration parameter in \fBmain.cf\fR instead.
/* .IP "\fB-r \fIsender\fR"
/*	Set the envelope sender address. This is the address where
/*	delivery problems are sent to, unless the message contains an
/*	\fBErrors-To:\fR message header.
/* .IP \fB-q\fR
/*	Attempt to deliver all queued mail. This is implemented by
/*	executing the \fBpostqueue\fR(1) command.
/* .IP "\fB-q\fIinterval\fR (ignored)"
/*	The interval between queue runs. Use the \fBqueue_run_delay\fR
/*	configuration parameter instead.
/* .IP \fB-qR\fIsite\fR
/*	Schedule immediate delivery of all mail that is queued for the named
/*	\fIsite\fR. This option accepts only \fIsite\fR names that are
/*	eligible for the "fast flush" service, and is implemented by
/*	executing the \fBpostqueue\fR(1) command.
/*	See \fBflush\fR(8) for more information about the "fast flush"
/*	service.
/* .IP \fB-qS\fIsite\fR
/*	This command is not implemented. Use the slower \fBsendmail -q\fR
/*	command instead.
/* .IP \fB-t\fR
/*	Extract recipients from message headers. This requires that no
/*	recipients be specified on the command line.
/* .IP \fB-v\fR
/*	Enable verbose logging for debugging purposes. Multiple \fB-v\fR
/*	options make the software increasingly verbose. For compatibility
/*	with mailx and other mail submission software, a single \fB-v\fR
/*	option produces no output.
/* SECURITY
/* .ad
/* .fi
/*	By design, this program is not set-user (or group) id. However,
/*	it must handle data from untrusted users or untrusted machines.
/*	Thus, the usual precautions need to be taken against malicious
/*	inputs.
/* DIAGNOSTICS
/*	Problems are logged to \fBsyslogd\fR(8) and to the standard error
/*	stream.
/* ENVIRONMENT
/* .ad
/* .fi
/* .IP \fBMAIL_CONFIG\fR
/*	Directory with Postfix configuration files.
/* .IP \fBMAIL_VERBOSE\fR
/*	Enable verbose logging for debugging purposes.
/* .IP \fBMAIL_DEBUG\fR
/*	Enable debugging with an external command, as specified with the
/*	\fBdebugger_command\fR configuration parameter.
/* FILES
/*	/var/spool/postfix, mail queue
/*	/etc/postfix, configuration files
/* CONFIGURATION PARAMETERS
/* .ad
/* .fi
/*	See the Postfix \fBmain.cf\fR file for syntax details and for
/*	default values. Use the \fBpostfix reload\fR command after a
/*	configuration change.
/* .IP \fBalias_database\fR
/*	Default alias database(s) for \fBnewaliases\fR. The default value
/*	for this parameter is system-specific.
/* .IP \fBbounce_size_limit\fR
/*	The amount of original message context that is sent along
/*	with a non-delivery notification.
/* .IP \fBdefault_database_type\fR
/*	Default alias etc. database type. On many UNIX systems the
/*	default type is either \fBdbm\fR or \fBhash\fR.
/* .IP \fBdebugger_command\fR
/*	Command that is executed after a Postfix daemon has initialized.
/* .IP \fBdebug_peer_level\fR
/*	Increment in verbose logging level when a remote host matches a
/*	pattern in the \fBdebug_peer_list\fR parameter.
/* .IP \fBdebug_peer_list\fR
/*	List of domain or network patterns. When a remote host matches
/*	a pattern, increase the verbose logging level by the amount
/*	specified in the \fBdebug_peer_level\fR parameter.
/* .IP \fBdefault_verp_delimiters\fR
/*	The VERP delimiter characters that are used when the \fB-V\fR
/*	command line option is specified without delimiter characters.
/* .IP \fBfast_flush_domains\fR
/*	List of domains that will receive "fast flush" service (default: all
/*	domains that this system is willing to relay mail to). This list
/*	specifies the domains that Postfix accepts in the SMTP \fBETRN\fR
/*	request and in the \fBsendmail -qR\fR command.
/* .IP \fBfork_attempts\fR
/*	Number of attempts to \fBfork\fR() a process before giving up.
/* .IP \fBfork_delay\fR
/*	Delay in seconds between successive \fBfork\fR() attempts.
/* .IP \fBhopcount_limit\fR
/*	Limit the number of \fBReceived:\fR message headers.
/* .IP \fBmail_owner\fR
/*	The owner of the mail queue and of most Postfix processes.
/* .IP \fBcommand_directory\fR
/*	Directory with Postfix support commands.
/* .IP \fBdaemon_directory\fR
/*	Directory with Postfix daemon programs.
/* .IP \fBqueue_directory\fR
/*	Top-level directory of the Postfix queue. This is also the root
/*	directory of Postfix daemons that run chrooted.
/* .IP \fBqueue_run_delay\fR
/*	The time between successive scans of the deferred queue.
/* .IP \fBverp_delimiter_filter\fR
/*	The characters that Postfix accepts as VERP delimiter characters.
/* SEE ALSO
/*	pickup(8) mail pickup daemon
/*	postsuper(1) queue maintenance
/*	postalias(1) maintain alias database
/*	postdrop(1) mail posting utility
/*	postfix(1) mail system control
/*	postqueue(1) mail queue control
/*	qmgr(8) queue manager
/*	smtpd(8) SMTP server
/*	flush(8) fast flush service
/*	syslogd(8) system logging
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
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>			/* remove() */
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <syslog.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <sysexits.h>

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <vstream.h>
#include <msg_vstream.h>
#include <msg_syslog.h>
#include <vstring_vstream.h>
#include <username.h>
#include <fullname.h>
#include <argv.h>
#include <safe.h>
#include <iostuff.h>
#include <stringops.h>
#include <set_ugid.h>
#include <connect.h>

/* Global library. */

#include <mail_queue.h>
#include <mail_proto.h>
#include <mail_params.h>
#include <record.h>
#include <rec_type.h>
#include <rec_streamlf.h>
#include <mail_conf.h>
#include <cleanup_user.h>
#include <mail_task.h>
#include <mail_run.h>
#include <debug_process.h>
#include <tok822.h>
#include <mail_flush.h>
#include <mail_stream.h>
#include <verp_sender.h>

/* Application-specific. */

 /*
  * Modes of operation.
  */
#define SM_MODE_ENQUEUE		1	/* delivery mode */
#define SM_MODE_NEWALIAS	2	/* initialize alias database */
#define SM_MODE_MAILQ		3	/* list mail queue */
#define SM_MODE_DAEMON		4	/* daemon mode */
#define SM_MODE_USER		5	/* user (stand-alone) mode */
#define SM_MODE_FLUSHQ		6	/* user (stand-alone) mode */

 /*
  * Flag parade.
  */
#define SM_FLAG_AEOF	(1<<0)		/* archaic EOF */

#define SM_FLAG_DEFAULT	(SM_FLAG_AEOF)

 /*
  * VERP support.
  */
char   *verp_delims;

 /*
  * Silly little macros (SLMs).
  */
#define STR	vstring_str

/* enqueue - post one message */

static void enqueue(const int flags, const char *encoding, const char *sender,
		            const char *full_name, char **recipients)
{
    VSTRING *buf;
    VSTREAM *dst;
    char   *saved_sender;
    char  **cpp;
    int     type;
    char   *start;
    int     skip_from_;
    TOK822 *tree;
    TOK822 *tp;
    enum {
	STRIP_CR_DUNNO, STRIP_CR_DO, STRIP_CR_DONT
    }       strip_cr;
    MAIL_STREAM *handle;
    char   *postdrop_command;
    uid_t   uid = getuid();
    int     status;
    int     naddr;
    int     prev_type;

    /*
     * Initialize.
     */
    buf = vstring_alloc(100);

    /*
     * Stop run-away process accidents by limiting the queue file size. This
     * is not a defense against DOS attack.
     */
    if (var_message_limit > 0 && get_file_limit() > var_message_limit)
	set_file_limit((off_t) var_message_limit);

    /*
     * The sender name is provided by the user. In principle, the mail pickup
     * service could deduce the sender name from queue file ownership, but:
     * pickup would not be able to run chrooted, and it may not be desirable
     * to use login names at all.
     */
    if (sender != 0) {
	VSTRING_RESET(buf);
	VSTRING_TERMINATE(buf);
	tree = tok822_parse(sender);
	for (naddr = 0, tp = tree; tp != 0; tp = tp->next)
	    if (tp->type == TOK822_ADDR && naddr++ == 0)
		tok822_internalize(buf, tp->head, TOK822_STR_DEFL);
	tok822_free_tree(tree);
	saved_sender = mystrdup(STR(buf));
	if (naddr > 1)
	    msg_warn("-f option specified malformed sender: %s", sender);
    } else {
	if ((sender = username()) == 0)
	    msg_fatal_status(EX_OSERR, "no login name found for user ID %lu",
			     (unsigned long) uid);
	saved_sender = mystrdup(sender);
    }

    /*
     * Let the postdrop command open the queue file for us, and sanity check
     * the content. XXX Make postdrop a manifest constant.
     */
    errno = 0;
    postdrop_command = concatenate(var_command_dir, "/postdrop -r",
			      msg_verbose ? " -v" : (char *) 0, (char *) 0);
    if ((handle = mail_stream_command(postdrop_command)) == 0)
	msg_fatal_status(EX_UNAVAILABLE, "%s(%ld): unable to execute %s: %m",
			 saved_sender, (long) uid, postdrop_command);
    myfree(postdrop_command);
    dst = handle->stream;

    /*
     * First, write envelope information to the output stream.
     * 
     * For sendmail compatibility, parse each command-line recipient as if it
     * were an RFC 822 message header; some MUAs specify comma-separated
     * recipient lists; and some MUAs even specify "word word <address>".
     * 
     * Sort-uniq-ing the recipient list is done after address canonicalization,
     * before recipients are written to queue file. That's cleaner than
     * having the queue manager nuke duplicate recipient status records.
     * 
     * XXX Should limit the size of envelope records.
     */
    if (full_name || (full_name = fullname()) != 0)
	rec_fputs(dst, REC_TYPE_FULL, full_name);
    rec_fputs(dst, REC_TYPE_FROM, saved_sender);
    if (verp_delims && *saved_sender == 0)
	msg_fatal_status(EX_USAGE,
			 "-V option requires non-null sender address");
    if (encoding)
	rec_fprintf(dst, REC_TYPE_ATTR, "%s=%s", MAIL_ATTR_ENCODING, encoding);
    if (verp_delims)
	rec_fputs(dst, REC_TYPE_VERP, verp_delims);
    if (recipients) {
	for (cpp = recipients; *cpp != 0; cpp++) {
	    tree = tok822_parse(*cpp);
	    for (tp = tree; tp != 0; tp = tp->next) {
		if (tp->type == TOK822_ADDR) {
		    tok822_internalize(buf, tp->head, TOK822_STR_DEFL);
		    if (REC_PUT_BUF(dst, REC_TYPE_RCPT, buf) < 0)
			msg_fatal_status(EX_TEMPFAIL,
				    "%s(%ld): error writing queue file: %m",
					 saved_sender, (long) uid);
		}
	    }
	    tok822_free_tree(tree);
	}
    }

    /*
     * Append the message contents to the queue file. Write chunks of at most
     * 1kbyte. Internally, we use different record types for data ending in
     * LF and for data that doesn't, so we can actually be binary transparent
     * for local mail. Unfortunately, SMTP has no record continuation
     * convention, so there is no guarantee that arbitrary data will be
     * delivered intact via SMTP. Strip leading From_ lines. For the benefit
     * of UUCP environments, also get rid of leading >>>From_ lines.
     */
    rec_fprintf(dst, REC_TYPE_MESG, REC_TYPE_MESG_FORMAT, 0L);
    skip_from_ = 1;
    strip_cr = STRIP_CR_DUNNO;
    for (prev_type = 0; (type = rec_streamlf_get(VSTREAM_IN, buf, var_line_limit))
	 != REC_TYPE_EOF; prev_type = type) {
	if (strip_cr == STRIP_CR_DUNNO && type == REC_TYPE_NORM) {
	    if (VSTRING_LEN(buf) > 0 && vstring_end(buf)[-1] == '\r')
		strip_cr = STRIP_CR_DO;
	    else
		strip_cr = STRIP_CR_DONT;
	}
	if (skip_from_) {
	    if (type == REC_TYPE_NORM) {
		start = vstring_str(buf);
		if (strncmp(start + strspn(start, ">"), "From ", 5) == 0)
		    continue;
	    }
	    skip_from_ = 0;
	}
	if (strip_cr == STRIP_CR_DO && type == REC_TYPE_NORM)
	    if (VSTRING_LEN(buf) > 0 && vstring_end(buf)[-1] == '\r')
		vstring_truncate(buf, VSTRING_LEN(buf) - 1);
	if ((flags & SM_FLAG_AEOF) && prev_type != REC_TYPE_CONT
	    && VSTRING_LEN(buf) == 1 && *STR(buf) == '.')
	    break;
	if (REC_PUT_BUF(dst, type, buf) < 0)
	    msg_fatal_status(EX_TEMPFAIL,
			     "%s(%ld): error writing queue file: %m",
			     saved_sender, (long) uid);
    }

    /*
     * Append an empty section for information extracted from message
     * headers. Header parsing is done by the cleanup service.
     */
    rec_fputs(dst, REC_TYPE_XTRA, "");

    /*
     * Identify the end of the queue file.
     */
    rec_fputs(dst, REC_TYPE_END, "");

    /*
     * Make sure that the message makes it to the file system. Once we have
     * terminated with successful exit status we cannot lose the message due
     * to "frivolous reasons". If all goes well, prevent the run-time error
     * handler from removing the file.
     */
    if (vstream_ferror(VSTREAM_IN))
	msg_fatal_status(EX_DATAERR, "%s(%ld): error reading input: %m",
			 saved_sender, (long) uid);
    if ((status = mail_stream_finish(handle, (VSTRING *) 0)) != 0)
	msg_fatal_status((status & CLEANUP_STAT_BAD) ? EX_SOFTWARE :
			 (status & CLEANUP_STAT_WRITE) ? EX_TEMPFAIL :
			 EX_UNAVAILABLE, "%s(%ld): %s", saved_sender,
			 (long) uid, cleanup_strerror(status));

    /*
     * Cleanup. Not really necessary as we're about to exit, but good for
     * debugging purposes.
     */
    vstring_free(buf);
    myfree(saved_sender);
}

/* main - the main program */

int     main(int argc, char **argv)
{
    static char *full_name = 0;		/* sendmail -F */
    struct stat st;
    char   *slash;
    int     extract_recipients = 0;	/* sendmail -t, almost */
    char   *sender = 0;			/* sendmail -f */
    int     c;
    int     fd;
    int     mode;
    ARGV   *ext_argv;
    int     debug_me = 0;
    int     err;
    int     n;
    int     flags = SM_FLAG_DEFAULT;
    char   *site_to_flush = 0;
    char   *encoding = 0;

    /*
     * Be consistent with file permissions.
     */
    umask(022);

    /*
     * To minimize confusion, make sure that the standard file descriptors
     * are open before opening anything else. XXX Work around for 44BSD where
     * fstat can return EBADF on an open file descriptor.
     */
    for (fd = 0; fd < 3; fd++)
	if (fstat(fd, &st) == -1
	    && (close(fd), open("/dev/null", O_RDWR, 0)) != fd)
	    msg_fatal_status(EX_UNAVAILABLE, "open /dev/null: %m");

    /*
     * The CDE desktop calendar manager leaks a parent file descriptor into
     * the child process. For the sake of sendmail compatibility we have to
     * close the file descriptor otherwise mail notification will hang.
     */
    for ( /* void */ ; fd < 100; fd++)
	(void) close(fd);

    /*
     * Process environment options as early as we can. We might be called
     * from a set-uid (set-gid) program, so be careful with importing
     * environment variables.
     */
    if (safe_getenv(CONF_ENV_VERB))
	msg_verbose = 1;
    if (safe_getenv(CONF_ENV_DEBUG))
	debug_me = 1;

    /*
     * Initialize. Set up logging, read the global configuration file and
     * extract configuration information. Set up signal handlers so that we
     * can clean up incomplete output.
     */
    if ((slash = strrchr(argv[0], '/')) != 0)
	argv[0] = slash + 1;
    msg_vstream_init(argv[0], VSTREAM_ERR);
    msg_syslog_init(mail_task("sendmail"), LOG_PID, LOG_FACILITY);
    set_mail_conf_str(VAR_PROCNAME, var_procname = mystrdup(argv[0]));

    /*
     * Some sites mistakenly install Postfix sendmail as set-uid root. Drop
     * set-uid privileges only when root, otherwise some systems will not
     * reset the saved set-userid, which would be a security vulnerability.
     */
    if (geteuid() == 0 && getuid() != 0) {
	msg_warn("the Postfix sendmail command has set-uid root file permissions");
	msg_warn("or the command is run from a set-uid root process");
	msg_warn("the Postfix sendmail command must be installed without set-uid root file permissions");
	set_ugid(getuid(), getgid());
    }

    /*
     * Further initialization...
     */
    mail_conf_read();
    if (chdir(var_queue_dir))
	msg_fatal_status(EX_UNAVAILABLE, "chdir %s: %m", var_queue_dir);

    signal(SIGPIPE, SIG_IGN);

    /*
     * Optionally start the debugger on ourself. This must be done after
     * reading the global configuration file, because that file specifies
     * what debugger command to execute.
     */
    if (debug_me)
	debug_process();

    /*
     * The default mode of operation is determined by the process name. It
     * can, however, be changed via command-line options (for example,
     * "newaliases -bp" will show the mail queue).
     */
    if (strcmp(argv[0], "mailq") == 0) {
	mode = SM_MODE_MAILQ;
    } else if (strcmp(argv[0], "newaliases") == 0) {
	mode = SM_MODE_NEWALIAS;
    } else if (strcmp(argv[0], "smtpd") == 0) {
	mode = SM_MODE_DAEMON;
    } else {
	mode = SM_MODE_ENQUEUE;
    }

    /*
     * Parse JCL. Sendmail has been around for a long time, and has acquired
     * a large number of options in the course of time. Some options such as
     * -q are not parsable with GETOPT() and get special treatment.
     */
#define OPTIND  (optind > 0 ? optind : 1)

    while (argv[OPTIND] != 0) {
	if (strcmp(argv[OPTIND], "-q") == 0) {
	    if (mode == SM_MODE_DAEMON)
		msg_warn("ignoring -q option in daemon mode");
	    else
		mode = SM_MODE_FLUSHQ;
	    optind++;
	    continue;
	}
	if (strcmp(argv[OPTIND], "-V") == 0) {
	    verp_delims = var_verp_delims;
	    optind++;
	    continue;
	}
	if ((c = GETOPT(argc, argv, "A:B:C:F:GIL:N:R:UV:X:b:ce:f:h:imno:p:r:q:tvx")) <= 0)
	    break;
	switch (c) {
	default:
	    if (msg_verbose)
		msg_info("-%c option ignored", c);
	    break;
	case 'n':
	    msg_fatal_status(EX_USAGE, "-%c option not supported", c);
	case 'B':
	    if (strcmp(optarg, "8BITMIME") == 0)/* RFC 1652 */
		encoding = MAIL_ATTR_ENC_8BIT;
	    else if (strcmp(optarg, "7BIT") == 0)	/* RFC 1652 */
		encoding = MAIL_ATTR_ENC_7BIT;
	    else
		msg_fatal_status(EX_USAGE, "-B option needs 8BITMIME or 7BIT");
	    break;
	case 'F':				/* full name */
	    full_name = optarg;
	    break;
	case 'I':				/* newaliases */
	    mode = SM_MODE_NEWALIAS;
	    break;
	case 'V':				/* VERP */
	    if (verp_delims_verify(optarg) != 0)
		msg_fatal_status(EX_USAGE, "-V requires two characters from %s",
				 var_verp_filter);
	    verp_delims = optarg;
	    break;
	case 'b':
	    switch (*optarg) {
	    default:
		msg_fatal_status(EX_USAGE, "unsupported: -%c%c", c, *optarg);
	    case 'd':				/* daemon mode */
		if (mode == SM_MODE_FLUSHQ)
		    msg_warn("ignoring -q option in daemon mode");
		mode = SM_MODE_DAEMON;
		break;
	    case 'i':				/* newaliases */
		mode = SM_MODE_NEWALIAS;
		break;
	    case 'm':				/* deliver mail */
		mode = SM_MODE_ENQUEUE;
		break;
	    case 'p':				/* mailq */
		mode = SM_MODE_MAILQ;
		break;
	    case 's':				/* stand-alone mode */
		mode = SM_MODE_USER;
		break;
	    }
	    break;
	case 'f':
	    sender = optarg;
	    break;
	case 'i':
	    flags &= ~SM_FLAG_AEOF;
	    break;
	case 'o':
	    switch (*optarg) {
	    default:
		if (msg_verbose)
		    msg_info("-%c%c option ignored", c, *optarg);
		break;
	    case 'A':
		if (optarg[1] == 0)
		    msg_fatal_status(EX_USAGE, "-oA requires pathname");
		myfree(var_alias_db_map);
		var_alias_db_map = mystrdup(optarg + 1);
		set_mail_conf_str(VAR_ALIAS_DB_MAP, var_alias_db_map);
		break;
	    case '7':
	    case '8':
		break;
	    case 'i':
		flags &= ~SM_FLAG_AEOF;
		break;
	    case 'm':
		break;
	    }
	    break;
	case 'r':				/* obsoleted by -f */
	    sender = optarg;
	    break;
	case 'q':
	    if (ISDIGIT(optarg[0])) {
		if (mode == SM_MODE_DAEMON) {
		    if (msg_verbose)
			msg_info("-%c%s option ignored", c, optarg);

		}
	    } else if (optarg[0] == 'R') {
		site_to_flush = optarg + 1;
		if (*site_to_flush == 0)
		    msg_fatal_status(EX_USAGE, "specify: -qRsitename");
	    } else {
		msg_fatal_status(EX_USAGE, "-q%c is not implemented",
				 optarg[0]);
	    }
	    break;
	case 't':
	    extract_recipients = 1;
	    break;
	case 'v':
	    msg_verbose++;
	    break;
	case '?':
	    msg_fatal_status(EX_USAGE, "usage: %s [options]", argv[0]);
	}
    }

    /*
     * Workaround: produce no output when verbose delivery is requested in
     * mail.rc.
     */
    if (msg_verbose > 0)
	msg_verbose--;

    /*
     * Look for conflicting options and arguments.
     */
    if (extract_recipients && mode != SM_MODE_ENQUEUE)
	msg_fatal_status(EX_USAGE, "-t can be used only in delivery mode");

    if (site_to_flush && mode != SM_MODE_ENQUEUE)
	msg_fatal_status(EX_USAGE, "-qR can be used only in delivery mode");

    if (extract_recipients && argv[OPTIND])
	msg_fatal_status(EX_USAGE,
			 "cannot handle command-line recipients with -t");

    /*
     * Start processing. Everything is delegated to external commands.
     */
    switch (mode) {
    default:
	msg_panic("unknown operation mode: %d", mode);
	/* NOTREACHED */
    case SM_MODE_ENQUEUE:
	if (site_to_flush == 0) {
	    enqueue(flags, encoding, sender, full_name, argv + OPTIND);
	    exit(0);
	}
	if (argv[OPTIND])
	    msg_fatal_status(EX_USAGE, "flush site requires no recipient");
	ext_argv = argv_alloc(2);
	argv_add(ext_argv, "postqueue", "-s", site_to_flush, (char *) 0);
	for (n = 0; n < msg_verbose; n++)
	    argv_add(ext_argv, "-v", (char *) 0);
	argv_terminate(ext_argv);
	mail_run_replace(var_command_dir, ext_argv->argv);
	/* NOTREACHED */
	break;
    case SM_MODE_MAILQ:
	if (argv[OPTIND])
	    msg_fatal_status(EX_USAGE,
			     "display queue mode requires no recipient");
	ext_argv = argv_alloc(2);
	argv_add(ext_argv, "postqueue", "-p", (char *) 0);
	for (n = 0; n < msg_verbose; n++)
	    argv_add(ext_argv, "-v", (char *) 0);
	argv_terminate(ext_argv);
	mail_run_replace(var_command_dir, ext_argv->argv);
	/* NOTREACHED */
    case SM_MODE_FLUSHQ:
	if (argv[OPTIND])
	    msg_fatal_status(EX_USAGE,
			     "flush queue mode requires no recipient");
	ext_argv = argv_alloc(2);
	argv_add(ext_argv, "postqueue", "-f", (char *) 0);
	for (n = 0; n < msg_verbose; n++)
	    argv_add(ext_argv, "-v", (char *) 0);
	argv_terminate(ext_argv);
	mail_run_replace(var_command_dir, ext_argv->argv);
	/* NOTREACHED */
    case SM_MODE_DAEMON:
	if (argv[OPTIND])
	    msg_fatal_status(EX_USAGE, "daemon mode requires no recipient");
	ext_argv = argv_alloc(2);
	argv_add(ext_argv, "postfix", (char *) 0);
	for (n = 0; n < msg_verbose; n++)
	    argv_add(ext_argv, "-v", (char *) 0);
	argv_add(ext_argv, "start", (char *) 0);
	argv_terminate(ext_argv);
	err = (mail_run_background(var_command_dir, ext_argv->argv) < 0);
	argv_free(ext_argv);
	exit(err);
	break;
    case SM_MODE_NEWALIAS:
	if (argv[OPTIND])
	    msg_fatal_status(EX_USAGE,
			 "alias initialization mode requires no recipient");
	if (*var_alias_db_map == 0)
	    return (0);
	ext_argv = argv_alloc(2);
	argv_add(ext_argv, "postalias", (char *) 0);
	for (n = 0; n < msg_verbose; n++)
	    argv_add(ext_argv, "-v", (char *) 0);
	argv_split_append(ext_argv, var_alias_db_map, ", \t\r\n");
	argv_terminate(ext_argv);
	mail_run_replace(var_command_dir, ext_argv->argv);
	/* NOTREACHED */
    case SM_MODE_USER:
	if (argv[OPTIND])
	    msg_fatal_status(EX_USAGE,
			     "stand-alone mode requires no recipient");
	ext_argv = argv_alloc(2);
	argv_add(ext_argv, "smtpd", "-S", (char *) 0);
	for (n = 0; n < msg_verbose; n++)
	    argv_add(ext_argv, "-v", (char *) 0);
	argv_terminate(ext_argv);
	mail_run_replace(var_daemon_dir, ext_argv->argv);
	/* NOTREACHED */
    }
}
