/*++
/* NAME
/*	bounce_log 3
/* SUMMARY
/*	bounce file API
/* SYNOPSIS
/*	#include <bounce_log.h>
/*
/*	typedef struct {
/* .in +4
/*	    /* public members... */
/*	    const char *recipient;
/*	    const char *status;
/*	    const char *text;
/* .in -4
/*	} BOUNCE_LOG;
/*
/*	BOUNCE_LOG *bounce_log_open(queue, id, flags, mode)
/*	const char *queue;
/*	const char *id;
/*	int	flags;
/*	int	mode;
/*
/*	BOUNCE_LOG *bounce_log_read(bp)
/*	BOUNCE_LOG *bp;
/*
/*	BOUNCE_LOG *bounce_log_delrcpt(bp)
/*	BOUNCE_LOG *bp;
/*
/*	void	bounce_log_rewind(bp)
/*	BOUNCE_LOG *bp;
/*
/*	BOUNCE_LOG *bounce_log_forge(recipient, dsn, why)
/*	const char *recipient;
/*	const char *status;
/*	const char *why;
/*
/*	void	bounce_log_close(bp)
/*	BOUNCE_LOG *bp;
/* DESCRIPTION
/*	This module implements a bounce/defer logfile API. Information
/*	is sanitized for control and non-ASCII characters.
/*
/*	bounce_log_open() opens the named bounce or defer logfile
/*	and returns a handle that must be used for further access.
/*	The result is a null pointer if the file cannot be opened.
/*	The caller is expected to inspect the errno code and deal
/*	with the problem.
/*
/*	bounce_log_read() reads the next record from the bounce or defer
/*	logfile (skipping over and warning about malformed data)
/*	and breaks out the recipient address, the recipient status
/*	and the text that explains why the recipient was undeliverable.
/*	bounce_log_read() returns a null pointer when no recipient was read,
/*	otherwise it returns its argument.
/*
/*	bounce_log_delrcpt() marks the last accessed recipient record as
/*	"deleted". This requires that the logfile is opened for update.
/*
/*	bounce_log_rewind() is a helper that seeks to the first recipient
/*	in an open bounce or defer logfile (skipping over recipients that
/*	are marked as done). The result is 0 in case of success, -1 in case
/*	of problems.
/*
/*	bounce_log_forge() forges one recipient status record
/*	without actually accessing a logfile. 
/*	The result cannot be used for any logfile access operation
/*	and must be disposed of by passing it to bounce_log_close().
/*
/*	bounce_log_close() closes an open bounce or defer logfile and
/*	releases memory for the specified handle. The result is non-zero
/*	in case of I/O errors.
/*
/*	Arguments:
/* .IP queue
/*	The bounce or defer queue name.
/* .IP id
/*	The message queue id of bounce or defer logfile. This
/*	file has the same name as the original message file.
/* .IP flags
/*	File open flags, as with open(2).
/* .IP more
/*	File permissions, as with open(2).
/* .PP
/*	Results:
/* .IP recipient
/*	The final recipient address.
/* .IP text
/*	The text that explains why the recipient was undeliverable.
/* .IP status
/*	String with DSN compatible status code (digit.digit.digit).
/* .PP
/*	Other fields will be added as the code evolves.
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
#include <string.h>
#include <ctype.h>
#include <unistd.h>

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <vstream.h>
#include <vstring.h>
#include <vstring_vstream.h>
#include <stringops.h>

/* Global library. */

#include <mail_queue.h>
#include <bounce_log.h>

/* Application-specific. */

#define STR(x)	vstring_str(x)

/* bounce_log_init - initialize structure */

static BOUNCE_LOG *bounce_log_init(VSTREAM *fp,
				           VSTRING *buf,
				           const char *recipient,
				           const char *status,
				           const char *text,
				           long offset)
{
    BOUNCE_LOG *bp;

    bp = (BOUNCE_LOG *) mymalloc(sizeof(*bp));
    bp->fp = fp;
    bp->buf = buf;
    bp->recipient = recipient;
    bp->status = status;
    bp->text = text;
    bp->offset = offset;
    return (bp);
}

/* bounce_log_open - open bounce read stream */

BOUNCE_LOG *bounce_log_open(const char *queue_name, const char *queue_id,
			            int flags, int mode)
{
    VSTREAM *fp;

#define STREQ(x,y)	(strcmp((x),(y)) == 0)

    /*
     * TODO: peek at the first byte to see if this is an old-style log
     * (<recipient>: text) or a new-style extensible log with multiple
     * attributes per recipient.
     */
    if ((fp = mail_queue_open(queue_name, queue_id, flags, mode)) == 0) {
	return (0);
    } else {
	return (bounce_log_init(fp,		/* stream */
				vstring_alloc(100),	/* buffer */
				(const char *) 0,	/* recipient */
				STREQ(queue_name, MAIL_QUEUE_DEFER) ?
				"4.0.0" : "5.0.0",	/* status */
				(const char *) 0,	/* text */
				0));		/* offset */
    }
}

/* bounce_log_read - read one record from bounce log file */

BOUNCE_LOG *bounce_log_read(BOUNCE_LOG *bp)
{
    char   *recipient;
    char   *text;
    char   *cp;

    while ((bp->offset = vstream_ftell(bp->fp)),
	   (vstring_get_nonl(bp->buf, bp->fp) != VSTREAM_EOF)) {

	if (STR(bp->buf)[0] == 0)
	    continue;

	/*
	 * Sanitize.
	 */
	cp = printable(STR(bp->buf), '?');

	/*
	 * Skip over deleted recipients.
	 */
	if (*cp == BOUNCE_LOG_STAT_DELETED)
	    continue;

	/*
	 * Find the recipient address.
	 */
	if (*cp != '<') {
	    msg_warn("%s: malformed record: %.30s...",
		     VSTREAM_PATH(bp->fp), cp);
	    continue;
	}
	recipient = cp + 1;
	if ((cp = strstr(recipient, ">: ")) == 0) {
	    msg_warn("%s: malformed record: %.30s...",
		     VSTREAM_PATH(bp->fp), cp);
	    continue;
	}
	*cp = 0;
	bp->recipient = *recipient ? recipient : "(MAILER-DAEMON)";

	/*
	 * Find the text that explains why mail was not deliverable.
	 */
	text = cp + 2;
	while (*text && ISSPACE(*text))
	    text++;
	bp->text = text;

	return (bp);
    }
    return (0);
}

/* bounce_log_delrcpt - mark recipient record as deleted */

BOUNCE_LOG *bounce_log_delrcpt(BOUNCE_LOG *bp)
{
    long    current_offset;

    current_offset = vstream_ftell(bp->fp);
    if (vstream_fseek(bp->fp, bp->offset, SEEK_SET) < 0)
	msg_fatal("bounce logfile %s seek error: %m", VSTREAM_PATH(bp->fp));
    VSTREAM_PUTC(BOUNCE_LOG_STAT_DELETED, bp->fp);
    if (vstream_fseek(bp->fp, current_offset, SEEK_SET) < 0)
	msg_fatal("bounce logfile %s seek error: %m", VSTREAM_PATH(bp->fp));
    return (bp);
}

/* bounce_log_forge - forge one recipient status record */

BOUNCE_LOG *bounce_log_forge(const char *recipient, const char *status,
			             const char *text)
{
    return (bounce_log_init((VSTREAM *) 0, (VSTRING *) 0,
			    recipient, status, text, 0));
}

/* bounce_log_close - close bounce reader stream */

int     bounce_log_close(BOUNCE_LOG *bp)
{
    int     ret;

    if (bp->fp)
	ret = vstream_fclose(bp->fp);
    else
	ret = 0;
    if (bp->buf)
	vstring_free(bp->buf);
    myfree((char *) bp);
    return (ret);
}
