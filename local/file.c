/*++
/* NAME
/*	file 3
/* SUMMARY
/*	mail delivery to arbitrary file
/* SYNOPSIS
/*	#include "local.h"
/*
/*	int	deliver_file(state, usr_attr, path)
/*	LOCAL_STATE state;
/*	USER_ATTR usr_attr;
/*	char	*path;
/* DESCRIPTION
/*	deliver_file() appends a message to a file, UNIX mailbox format,
/*	or qmail maildir format,
/*	with duplicate suppression. It will deliver only to non-executable
/*	regular files.
/*
/*	Arguments:
/* .IP state
/*	The attributes that specify the message, recipient and more.
/*	Attributes describing alias, include or forward expansion.
/*	A table with the results from expanding aliases or lists.
/* .IP usr_attr
/*	Attributes describing user rights and environment information.
/* .IP path
/*	The file to deliver to. If the name ends in '/', delivery is done
/*	in qmail maildir format, otherwise delivery is done in UNIX mailbox
/*	format.
/* DIAGNOSTICS
/*	deliver_file() returns non-zero when delivery should be tried again.
/* SEE ALSO
/*	defer(3)
/*	bounce(3)
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
#include <fcntl.h>
#include <errno.h>

/* Utility library. */

#include <msg.h>
#include <htable.h>
#include <vstring.h>
#include <vstream.h>
#include <deliver_flock.h>
#include <open_as.h>

/* Global library. */

#include <mail_copy.h>
#include <bounce.h>
#include <defer.h>
#include <sent.h>
#include <been_here.h>
#include <mail_params.h>
#include <dot_lockfile_as.h>

/* Application-specific. */

#include "local.h"

#define STR	vstring_str

/* deliver_file - deliver to file */

int     deliver_file(LOCAL_STATE state, USER_ATTR usr_attr, char *path)
{
    char   *myname = "deliver_file";
    struct stat st;
    int     fd;
    VSTREAM *dst;
    VSTRING *why;
    int     status;
    int     copy_flags;

#ifdef USE_DOT_LOCK
    int     lock = -1;

#endif

    /*
     * Make verbose logging easier to understand.
     */
    state.level++;
    if (msg_verbose)
	MSG_LOG_STATE(myname, state);

    /*
     * DUPLICATE ELIMINATION
     * 
     * Skip this file if it was already delivered to as this user.
     */
    if (been_here(state.dup_filter, "file %d %s", usr_attr.uid, path))
	return (0);

    /*
     * DELIVERY POLICY
     * 
     * Do we allow delivery to files?
     */
    if ((local_file_deliver_mask & state.msg_attr.exp_type) == 0)
	return (bounce_append(BOUNCE_FLAG_KEEP, BOUNCE_ATTR(state.msg_attr),
			      "mail to file is restricted"));

    /*
     * DELIVERY RIGHTS
     * 
     * Use a default uid/gid when none are given.
     */
    if (usr_attr.uid == 0 && (usr_attr.uid = var_default_uid) == 0)
	msg_panic("privileged default user id");
    if (usr_attr.gid == 0 && (usr_attr.gid = var_default_gid) == 0)
	msg_panic("privileged default group id");

    /*
     * If the name ends in /, use maildir-style delivery instead.
     */
    if (path[strlen(path) - 1] == '/')
	return (deliver_maildir(state, usr_attr, path));

    /*
     * Deliver. From here on, no early returns or we have a memory leak.
     */
    if (msg_verbose)
	msg_info("deliver_file (%d,%d): %s", usr_attr.uid, usr_attr.gid, path);
    if (vstream_fseek(state.msg_attr.fp, state.msg_attr.offset, SEEK_SET) < 0)
	msg_fatal("seek queue file %s: %m", state.msg_attr.queue_id);
    why = vstring_alloc(100);

    /*
     * Open or create the file, lock it, and append the message. Open the
     * file as the specified user. XXX Since we cannot set a lockfile before
     * creating the destination, there is a small chance that we truncate an
     * existing file.
     */
    copy_flags = MAIL_COPY_MBOX;
    if ((local_deliver_hdr_mask & DELIVER_HDR_FILE) == 0)
	copy_flags &= ~MAIL_COPY_DELIVERED;

#define FOPEN_AS(p,u,g) ( \
    (fd = open_as(p,O_APPEND|O_CREAT|O_WRONLY,0600,u,g)) >= 0 ? \
	vstream_fdopen(fd, O_WRONLY) : 0)

    if ((dst = FOPEN_AS(path, usr_attr.uid, usr_attr.gid)) == 0) {
	status = bounce_append(BOUNCE_FLAG_KEEP, BOUNCE_ATTR(state.msg_attr),
			       "cannot open destination file %s: %m", path);
    } else if (fstat(vstream_fileno(dst), &st) < 0) {
	vstream_fclose(dst);
	status = defer_append(BOUNCE_FLAG_KEEP, BOUNCE_ATTR(state.msg_attr),
			      "cannot fstat file %s: %m", path);
    } else if (S_ISREG(st.st_mode) && deliver_flock(vstream_fileno(dst), why) < 0) {
	vstream_fclose(dst);
	status = defer_append(BOUNCE_FLAG_KEEP, BOUNCE_ATTR(state.msg_attr),
			      "cannot lock destination file %s: %s",
			      path, STR(why));
    } else if (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
	vstream_fclose(dst);
	status = bounce_append(BOUNCE_FLAG_KEEP, BOUNCE_ATTR(state.msg_attr),
			       "executable destination file %s", path);
#ifdef USE_DOT_LOCK
    } else if ((lock = dot_lockfile_as(path, why, usr_attr.uid, usr_attr.gid)) < 0
	       && errno == EEXIST) {
	vstream_fclose(dst);
	status = defer_append(BOUNCE_FLAG_KEEP, BOUNCE_ATTR(state.msg_attr),
			      "cannot append destination file %s: %s",
			      path, STR(why));
#endif
    } else if (mail_copy(COPY_ATTR(state.msg_attr), dst, S_ISREG(st.st_mode) ?
		      copy_flags : (copy_flags & ~MAIL_COPY_TOFILE), why)) {
	status = defer_append(BOUNCE_FLAG_KEEP, BOUNCE_ATTR(state.msg_attr),
			      "cannot append destination file %s: %s",
			      path, STR(why));
    } else {
	status = sent(SENT_ATTR(state.msg_attr), "%s", path);
    }

    /*
     * Clean up.
     */
#ifdef USE_DOT_LOCK
    if (lock == 0)
	dot_unlockfile_as(path, usr_attr.uid, usr_attr.gid);
#endif
    vstring_free(why);
    return (status);
}
