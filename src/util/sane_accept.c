/*++
/* NAME
/*	sane_accept 3
/* SUMMARY
/*	sanitize accept() error returns
/* SYNOPSIS
/*	#include <sane_accept.h>
/*
/*	int	sane_accept(sock, buf, len)
/*	int	sock;
/*	struct sockaddr	*buf;
/*	SOCKADDR_SIZE *len;
/* DESCRIPTION
/*	sane_accept() implements the accept(2) socket call, and maps
/*	known harmless error results to EAGAIN.
/* BUGS
/*	Bizarre systems may have other harmless error results. Such
/*	systems encourage programers to ignore error results, and
/*	penalizes programmers who code defensively.
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
#include <sys/socket.h>
#include <errno.h>

/* Utility library. */

#include "msg.h"
#include "sane_accept.h"

/* sane_accept - sanitize accept() error returns */

int     sane_accept(int sock, struct sockaddr * sa, SOCKADDR_SIZE *len)
{
    static int accept_ok_errors[] = {
	EAGAIN,
	ECONNREFUSED,
	ECONNRESET,
	EHOSTDOWN,
	EHOSTUNREACH,
	EINTR,
	ENETDOWN,
	ENETUNREACH,
	ENOTCONN,
	EWOULDBLOCK,
	ENOBUFS,			/* HPUX11 */
	ECONNABORTED,
	0,
    };
    int     count;
    int     err;
    int     fd;

    /*
     * XXX Solaris 2.4 accept() returns EPIPE when a UNIX-domain client has
     * disconnected in the mean time. From then on, UNIX-domain sockets are
     * hosed beyond recovery. There is no point treating this as a beneficial
     * error result because the program would go into a tight loop.
     * 
     * XXX LINUX < 2.1 accept() wakes up before the three-way handshake is
     * complete, so it can fail with ECONNRESET and other "false alarm"
     * indications.
     * 
     * XXX FreeBSD 4.2-STABLE accept() returns ECONNABORTED when a UNIX-domain
     * client has disconnected in the mean time. The data that was sent with
     * connect() write() close() is lost, even though the write() and close()
     * reported successful completion. This was fixed shortly before FreeBSD
     * 4.3. However, other systems may make that same mistake again, so we're
     * adding a special warning.
     * 
     * XXX HP-UX 11 returns ENOBUFS when the client has disconnected in the mean
     * time.
     */
    if ((fd = accept(sock, sa, len)) < 0) {
	for (count = 0; (err = accept_ok_errors[count]) != 0; count++) {
	    if (errno == err) {
		errno = EAGAIN;
		break;
	    }
	}
    }
    return (fd);
}
