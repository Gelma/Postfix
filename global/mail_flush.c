/*++
/* NAME
/*	mail_flush 3
/* SUMMARY
/*	flush backed up mail
/* SYNOPSIS
/*	#include <mail_flush.h>
/*
/*	int	mail_flush_deferred()
/*
/*	int	mail_flush_site(site)
/*	const char *site;
/* DESCRIPTION
/*	This module triggers delivery of backed up mail.
/*
/*	mail_flush_deferred() triggers delivery of all deferred
/*	or incoming mail.
/*
/*	mail_flush_site() triggers delivery of all mail queued for
/*	the named site. This routine may degenerate into a
/*	mail_flush_deferred() call.
/* DIAGNOSTICS
/*	The result is 0 in case of success, -1 in case of failure.
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

#include "sys_defs.h"

/* Utility library. */

/* Global library. */

#include <mail_proto.h>
#include <mail_flush.h>

/* mail_flush_deferred - flush deferred queue */

int     mail_flush_deferred(void)
{
    static char qmgr_trigger[] = {
	QMGR_REQ_FLUSH_DEAD,		/* all hosts, all transports */
	QMGR_REQ_SCAN_ALL,		/* all time stamps */
	QMGR_REQ_SCAN_DEFERRED,		/* scan deferred queue */
	QMGR_REQ_SCAN_INCOMING,		/* scan incoming queue */
    };

    /*
     * Trigger the flush queue service.
     */
    return (mail_trigger(MAIL_CLASS_PUBLIC, MAIL_SERVICE_QUEUE,
			 qmgr_trigger, sizeof(qmgr_trigger)));
}

/* mail_flush_site - flush deferred mail for site */

int     mail_flush_site(const char *unused_site)
{

    /*
     * Until we have dedicated per-site queues, this call will degenerate
     * into a mail_flush_deferred() call.
     */
    return (mail_flush_deferred());
}
