#ifndef _IOSTUFF_H_INCLUDED_
#define _IOSTUFF_H_INCLUDED_

/*++
/* NAME
/*	iostuff 3h
/* SUMMARY
/*	miscellaneous I/O primitives
/* SYNOPSIS
/*	#include <iostuff.h>
/* DESCRIPTION

 /*
  * External interface.
  */
extern int non_blocking(int, int);
extern int close_on_exec(int, int);
extern int open_limit(int);
extern int readable(int);
extern int writable(int);
extern off_t get_file_limit(void);
extern void set_file_limit(off_t);
extern int peekfd(int);
extern int read_wait(int, int);
extern int write_wait(int, int);
extern int write_buf(int, const char *, int, int);
extern void doze(unsigned);

#define BLOCKING	0
#define NON_BLOCKING	1

#define CLOSE_ON_EXEC	1
#define PASS_ON_EXEC	0

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*      Wietse Venema
/*      IBM T.J. Watson Research
/*      P.O. Box 704
/*      Yorktown Heights, NY 10598, USA
/* CREATION DATE
/*	Sat Jan 25 16:54:13 EST 1997
/*--*/

#endif
