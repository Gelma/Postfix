/*++
/* NAME
/*	mail_stream 3
/* SUMMARY
/*	mail stream management
/* SYNOPSIS
/*	#include <mail_stream.h>
/*
/*	typedef struct {
/* .in +4
/*		VSTREAM	*stream;
/*		char	*id;
/*		private members...
/* .in -4
/*	} MAIL_STREAM;
/*
/*	MAIL_STREAM *mail_stream_file(queue, mode, class, service)
/*	const char *queue;
/*	int	mode;
/*	const char *class;
/*	const char *service;
/*
/*	MAIL_STREAM *mail_stream_service(class, service)
/*	const char *class;
/*	const char *service;
/*
/*	MAIL_STREAM *mail_stream_command(command)
/*	const char *command;
/*
/*	void	mail_stream_cleanup(info)
/*	MAIL_STREAM *info;
/*
/*	int	mail_stream_finish(info)
/*	MAIL_STREAM *info;
/* DESCRIPTION
/*	This module provides a generic interface to Postfix queue file
/*	format messages to file, to Postfix server, or to external command.
/*	The routines that open a stream return a handle with an initialized
/*	stream and queue id member. The handle is either given to a cleanup
/*	routine, to dispose of a failed request, or to a finish routine, to
/*	complete the request.
/*
/*	mail_stream_file() opens a mail stream to a newly-created file and
/*	arranges for trigger delivery at finish time. This call never fails.
/*	But it may take forever.
/*
/*	mail_stream_command() opens a mail stream to external command,
/*	and receives queue ID information from the command. The result
/*	is a null pointer when the initial handshake fails. The command
/*	is given to the shell only when necessary. At finish time, the
/*	command is expected to send a completion status.
/*
/*	mail_stream_service() opens a mail stream to Postfix service,
/*	and receives queue ID information from the command. The result
/*	is a null pointer when the initial handshake fails. At finish
/*	time, the daemon is expected to send a completion status.
/*
/*	mail_stream_cleanup() cancels the operation that was started with
/*	any of the mail_stream_xxx() routines, and destroys the argument.
/*	It is up to the caller to remove incomplete file objects.
/*
/*	mail_stream_finish() completes the operation that was started with
/*	any of the mail_stream_xxx() routines, and destroys the argument.
/*	The result is any of the status codes defined in <cleanup_user.h>.
/*	It is up to the caller to remove incomplete file objects.
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

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <vstring.h>
#include <vstream.h>
#include <stringops.h>

/* Global library. */

#include <cleanup_user.h>
#include <mail_proto.h>
#include <mail_queue.h>
#include <opened.h>
#include <mail_stream.h>

/* Application-specific. */

static VSTRING *id_buf;

#define FREE_AND_WIPE(free, arg) { if (arg) free(arg); arg = 0; }

/* mail_stream_cleanup - clean up after success or failure */

void    mail_stream_cleanup(MAIL_STREAM * info)
{
    FREE_AND_WIPE(info->close, info->stream);
    FREE_AND_WIPE(myfree, info->id);
    FREE_AND_WIPE(myfree, info->class);
    FREE_AND_WIPE(myfree, info->service);
    myfree((char *) info);
}

/* mail_stream_finish_file - finish file mail stream */

static int mail_stream_finish_file(MAIL_STREAM * info)
{
    int     status = 0;
    static char wakeup[] = {TRIGGER_REQ_WAKEUP};

    /*
     * Make sure the message makes it to file. Set the execute bit when no
     * write error was detected.
     */
    if (vstream_fflush(info->stream)
	|| fchmod(vstream_fileno(info->stream), 0700)
#ifdef HAS_FSYNC
	|| fsync(vstream_fileno(info->stream))
#endif
	)
	status = CLEANUP_STAT_WRITE;

    /*
     * Close the queue file and mark it as closed. Be prepared for
     * vstream_fclose() to fail even after vstream_fflush() and fsync()
     * reported no error. Reason: after a file is closed, some networked file
     * systems copy the file out to another machine. Running the queue on a
     * remote file system is not recommended, if only for performance
     * reasons.
     */
    if (info->close(info->stream))
	status = CLEANUP_STAT_WRITE;
    info->stream = 0;

    /*
     * When all is well, notify the next service that a new message has been
     * queued.
     */
    if (status == CLEANUP_STAT_OK)
	mail_trigger(info->class, info->service, wakeup, sizeof(wakeup));

    /*
     * Cleanup.
     */
    mail_stream_cleanup(info);
    return (status);
}

/* mail_stream_finish_ipc - finish IPC mail stream */

static int mail_stream_finish_ipc(MAIL_STREAM * info)
{
    int     status = CLEANUP_STAT_WRITE;

    /*
     * Receive the peer's completion status.
     */
    if (mail_scan(info->stream, "%d", &status) != 1)
	status = CLEANUP_STAT_WRITE;

    /*
     * Cleanup.
     */
    mail_stream_cleanup(info);
    return (status);
}

/* mail_stream_finish - finish action */

int     mail_stream_finish(MAIL_STREAM * info)
{
    return (info->finish(info));
}

/* mail_stream_file - destination is file */

MAIL_STREAM *mail_stream_file(const char *queue, const char *class,
			              const char *service)
{
    MAIL_STREAM *info;
    VSTREAM *stream;

    stream = mail_queue_enter(queue, 0600);
    if (msg_verbose)
	msg_info("open %s", VSTREAM_PATH(stream));

    info = (MAIL_STREAM *) mymalloc(sizeof(*info));
    info->stream = stream;
    info->finish = mail_stream_finish_file;
    info->close = vstream_fclose;
    info->id = mystrdup(basename(VSTREAM_PATH(stream)));
    info->class = mystrdup(class);
    info->service = mystrdup(service);
    return (info);
}

/* mail_stream_service - destination is service */

MAIL_STREAM *mail_stream_service(const char *class, const char *name)
{
    VSTREAM *stream;
    MAIL_STREAM *info;

    if (id_buf == 0)
	id_buf = vstring_alloc(10);

    stream = mail_connect_wait(class, name);
    if (mail_scan(stream, "%s", id_buf) != 1) {
	vstream_fclose(stream);
	return (0);
    } else {
	info = (MAIL_STREAM *) mymalloc(sizeof(*info));
	info->stream = stream;
	info->finish = mail_stream_finish_ipc;
	info->close = vstream_fclose;
	info->id = mystrdup(vstring_str(id_buf));
	info->class = 0;
	info->service = 0;
	return (info);
    }
}

/* mail_stream_command - destination is command */

MAIL_STREAM *mail_stream_command(const char *command)
{
    VSTREAM *stream;
    MAIL_STREAM *info;

    if (id_buf == 0)
	id_buf = vstring_alloc(10);

    /*
     * Treat fork() failure as a transient problem. Treat bad handshake as a
     * permanent error.
     */
    while ((stream = vstream_popen(command, O_RDWR)) == 0) {
	msg_warn("fork: %m");
	sleep(10);
    }
    if (mail_scan(stream, "%s", id_buf) != 1) {
	vstream_pclose(stream);
	return (0);
    } else {
	info = (MAIL_STREAM *) mymalloc(sizeof(*info));
	info->stream = stream;
	info->finish = mail_stream_finish_ipc;
	info->close = vstream_pclose;
	info->id = mystrdup(vstring_str(id_buf));
	info->class = 0;
	info->service = 0;
	return (info);
    }
}
