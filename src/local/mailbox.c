/*++
/* NAME
/*	mailbox 3
/* SUMMARY
/*	mailbox delivery
/* SYNOPSIS
/*	#include "local.h"
/*
/*	int	deliver_mailbox(state, usr_attr, statusp)
/*	LOCAL_STATE state;
/*	USER_ATTR usr_attr;
/*	int	*statusp;
/* DESCRIPTION
/*	deliver_mailbox() delivers to mailbox, with duplicate
/*	suppression. The default is direct mailbox delivery to
/*	/var/[spool/]mail/\fIuser\fR; when a \fIhome_mailbox\fR
/*	has been configured, mail is delivered to ~/$\fIhome_mailbox\fR;
/*	and when a \fImailbox_command\fR has been configured, the message
/*	is piped into the command instead.
/*
/*	A zero result means that the named user was not found.
/*
/*	Arguments:
/* .IP state
/*	The attributes that specify the message, recipient and more.
/*	Attributes describing alias, include or forward expansion.
/*	A table with the results from expanding aliases or lists.
/* .IP usr_attr
/*	Attributes describing user rights and environment.
/* .IP statusp
/*	Delivery status: see below.
/* DIAGNOSTICS
/*	The message delivery status is non-zero when delivery should be tried
/*	again.
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
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/* Utility library. */

#include <msg.h>
#include <htable.h>
#include <vstring.h>
#include <vstream.h>
#include <mymalloc.h>
#include <stringops.h>
#include <set_eugid.h>

/* Global library. */

#include <mail_copy.h>
#include <defer.h>
#include <sent.h>
#include <mypwd.h>
#include <been_here.h>
#include <mail_params.h>
#include <deliver_pass.h>
#include <mbox_open.h>

#ifndef EDQUOT
#define EDQUOT EFBIG
#endif

/* Application-specific. */

#include "local.h"
#include "biff_notify.h"

#define YES	1
#define NO	0

/* deliver_mailbox_file - deliver to recipient mailbox */

static int deliver_mailbox_file(LOCAL_STATE state, USER_ATTR usr_attr)
{
    char   *myname = "deliver_mailbox_file";
    char   *spool_dir;
    char   *mailbox;
    VSTRING *why;
    MBOX   *mp;
    int     status;
    int     copy_flags;
    VSTRING *biff;
    long    end;
    struct stat st;
    uid_t   spool_uid;
    gid_t   spool_gid;
    uid_t   chown_uid;
    gid_t   chown_gid;

    /*
     * Make verbose logging easier to understand.
     */
    state.level++;
    if (msg_verbose)
	MSG_LOG_STATE(myname, state);

    /*
     * Initialize. Assume the operation will fail. Set the delivered
     * attribute to reflect the final recipient.
     */
    if (vstream_fseek(state.msg_attr.fp, state.msg_attr.offset, SEEK_SET) < 0)
	msg_fatal("seek message file %s: %m", VSTREAM_PATH(state.msg_attr.fp));
    state.msg_attr.delivered = state.msg_attr.recipient;
    status = -1;
    why = vstring_alloc(100);
    if (*var_home_mailbox) {
	spool_dir = 0;
	mailbox = concatenate(usr_attr.home, "/", var_home_mailbox, (char *) 0);
    } else {
	spool_dir = var_mail_spool_dir;
	mailbox = concatenate(spool_dir, "/", state.msg_attr.user, (char *) 0);
    }

    /*
     * Mailbox delivery with least privilege. As long as we do not use root
     * privileges this code may also work over NFS.
     * 
     * If delivering to the recipient's home directory, perform all operations
     * (including file locking) as that user (Mike Muuss, Army Research
     * Laboratory, USA).
     * 
     * If delivering to the mail spool directory, and the spool directory is
     * world-writable, deliver as the recipient; if the spool directory is
     * group-writable, use the recipient user id and the mail spool group id.
     * 
     * Otherwise, use root privileges and chown the mailbox.
     */
    if (spool_dir == 0
	|| stat(spool_dir, &st) < 0
	|| (st.st_mode & S_IWOTH) != 0) {
	spool_uid = usr_attr.uid;
	spool_gid = usr_attr.gid;
    } else if ((st.st_mode & S_IWGRP) != 0) {
	spool_uid = usr_attr.uid;
	spool_gid = st.st_gid;
    } else {
	spool_uid = 0;
	spool_gid = 0;
    }
    if (spool_uid == usr_attr.uid) {
	chown_uid = -1;
	chown_gid = -1;
    } else {
	chown_uid = usr_attr.uid;
	chown_gid = usr_attr.gid;
    }
    if (msg_verbose)
	msg_info("spool_uid/gid %ld/%ld chown_uid/gid %ld/%ld",
		 (long) spool_uid, (long) spool_gid,
		 (long) chown_uid, (long) chown_gid);

    /*
     * Lock the mailbox and open/create the mailbox file. Depending on the
     * type of locking used, we lock first or we open first.
     * 
     * Write the file as the recipient, so that file quota work.
     */
    copy_flags = MAIL_COPY_MBOX;
    if ((local_deliver_hdr_mask & DELIVER_HDR_FILE) == 0)
	copy_flags &= ~MAIL_COPY_DELIVERED;

    set_eugid(spool_uid, spool_gid);
    mp = mbox_open(mailbox, O_APPEND | O_WRONLY | O_CREAT,
		   S_IRUSR | S_IWUSR, &st, chown_uid, chown_gid,
		   local_mbox_lock_mask, why);
    if (mp != 0) {
	if (spool_uid != usr_attr.uid || spool_gid != usr_attr.gid)
	    set_eugid(usr_attr.uid, usr_attr.gid);
	if (S_ISREG(st.st_mode) == 0) {
	    vstream_fclose(mp->fp);
	    vstring_sprintf(why, "destination is not a regular file");
	    errno = 0;
	} else {
	    end = vstream_fseek(mp->fp, (off_t) 0, SEEK_END);
	    status = mail_copy(COPY_ATTR(state.msg_attr), mp->fp,
			       copy_flags, "\n", why);
	}
	if (spool_uid != usr_attr.uid || spool_gid != usr_attr.gid)
	    set_eugid(spool_uid, spool_gid);
	mbox_release(mp);
    }
    set_eugid(var_owner_uid, var_owner_gid);

    /*
     * As the mail system, bounce, defer delivery, or report success.
     */
    if (status != 0) {
	status = (errno == EAGAIN || errno == ENOSPC ?
		  defer_append : bounce_append)
	    (BOUNCE_FLAG_KEEP, BOUNCE_ATTR(state.msg_attr),
	     "cannot access mailbox %s for user %s. %s",
	     mailbox, state.msg_attr.user, vstring_str(why));
    } else {
	sent(SENT_ATTR(state.msg_attr), "mailbox");
	if (var_biff) {
	    biff = vstring_alloc(100);
	    vstring_sprintf(biff, "%s@%ld", usr_attr.logname, (long) end);
	    biff_notify(vstring_str(biff), VSTRING_LEN(biff) + 1);
	    vstring_free(biff);
	}
    }

    /*
     * Clean up.
     */
    myfree(mailbox);
    vstring_free(why);
    return (status);
}

/* deliver_mailbox - deliver to recipient mailbox */

int     deliver_mailbox(LOCAL_STATE state, USER_ATTR usr_attr, int *statusp)
{
    char   *myname = "deliver_mailbox";
    int     status;
    struct mypasswd *mbox_pwd;
    char   *path;

    /*
     * Make verbose logging easier to understand.
     */
    state.level++;
    if (msg_verbose)
	MSG_LOG_STATE(myname, state);

    /*
     * DUPLICATE ELIMINATION
     * 
     * Don't come here more than once, whether or not the recipient exists.
     */
    if (been_here(state.dup_filter, "mailbox %s", state.msg_attr.user))
	return (YES);

    /*
     * Delegate mailbox delivery to another message transport.
     */
    if (*var_mailbox_transport) {
	*statusp = deliver_pass(MAIL_CLASS_PRIVATE, var_mailbox_transport,
			      state.request, state.msg_attr.recipient, -1L);
	return (YES);
    }

    /*
     * Skip delivery when this recipient does not exist.
     */
    if ((mbox_pwd = mypwnam(state.msg_attr.user)) == 0)
	return (NO);

    /*
     * No early returns or we have a memory leak.
     */

    /*
     * DELIVERY RIGHTS
     * 
     * Use the rights of the recipient user.
     */
    SET_USER_ATTR(usr_attr, mbox_pwd, state.level);

    /*
     * Deliver to mailbox or to external command.
     */
#define LAST_CHAR(s) (s[strlen(s) - 1])

    if (*var_mailbox_command)
	status = deliver_command(state, usr_attr, var_mailbox_command);
    else if (*var_home_mailbox && LAST_CHAR(var_home_mailbox) == '/') {
	path = concatenate(usr_attr.home, "/", var_home_mailbox, (char *) 0);
	status = deliver_maildir(state, usr_attr, path);
	myfree(path);
    } else
	status = deliver_mailbox_file(state, usr_attr);

    /*
     * Cleanup.
     */
    mypwfree(mbox_pwd);
    *statusp = status;
    return (YES);
}
