#ifndef _MSG_H_INCLUDED_
#define _MSG_H_INCLUDED_

/*++
/* NAME
/*	msg 3h
/* SUMMARY
/*	diagnostics interface
/* SYNOPSIS
/*	#include "msg.h"
/* DESCRIPTION
/*	.nf

/*
 * External interface.
 */
typedef void (*MSG_CLEANUP_FN) (void);

extern int msg_verbose;

extern void msg_info(const char *,...);
extern void msg_warn(const char *,...);
extern void msg_error(const char *,...);
extern NORETURN msg_fatal(const char *,...);
extern NORETURN msg_panic(const char *,...);

extern int msg_error_limit(int);
extern void msg_error_clear(void);
extern MSG_CLEANUP_FN msg_cleanup(MSG_CLEANUP_FN);

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*      Wietse Venema
/*      IBM T.J. Watson Research
/*      P.O. Box 704
/*      Yorktown Heights, NY 10598, USA
/*--*/

#endif
