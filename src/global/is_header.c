/*++
/* NAME
/*	is_header 3
/* SUMMARY
/*	message header classification
/* SYNOPSIS
/*	#include <is_header.h>
/*
/*	int	is_header(string)
/*	const char *string;
/* DESCRIPTION
/*	is_header() examines the given string and returns non-zero (true)
/*	when it begins with a mail header name + colon. This routine
/*	permits 8-bit data in header labels.
/* STANDARDS
/*	RFC 822 (ARPA Internet Text Messages)
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
#include <ctype.h>

/* Global library. */

#include "is_header.h"

/* is_header - determine if this can be a header line */

int     is_header(const char *str)
{
    const char *cp;
    int     c;

    /*
     * XXX RFC 2822 Section 4.5.2, Obsolete header fields: whitespace may
     * appear between header label and ":" (see: RFC 822, Section 3.4.2.).
     * 
     * The code below allows no such whitespace. This has never been a problem,
     * and therefore we're not inclined to add code for it.
     */
    for (cp = str; (c = *(unsigned char *) cp) != 0; cp++) {
	if (c == ':')
	    return (cp > str);
	if ( /* !ISASCII(c) || */ ISSPACE(c) || ISCNTRL(c))
	    break;
    }
    return (0);
}
