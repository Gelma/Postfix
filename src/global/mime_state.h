#ifndef _MIME_STATE_H_INCLUDED_
#define _MIME_STATE_H_INCLUDED_

/*++
/* NAME
/*	mime_state 3h
/* SUMMARY
/*	MIME parser state engine
/* SYNOPSIS
/*	#include "mime_state.h"
 DESCRIPTION
 .nf

 /*
  * Utility library.
  */
#include <vstring.h>

 /*
  * Global library.
  */
#include <header_opts.h>

 /*
  * External interface. All MIME_STATE structure members are private.
  */
typedef struct MIME_STATE MIME_STATE;
typedef void (*MIME_STATE_HEAD_OUT) (void *, int, HEADER_OPTS *, VSTRING *, off_t);
typedef void (*MIME_STATE_BODY_OUT) (void *, int, const char *, int, off_t);
typedef void (*MIME_STATE_ANY_END) (void *);
typedef void (*MIME_STATE_ERR_PRINT) (void *, int, const char *);

extern MIME_STATE *mime_state_alloc(int, MIME_STATE_HEAD_OUT, MIME_STATE_ANY_END, MIME_STATE_BODY_OUT, MIME_STATE_ANY_END, MIME_STATE_ERR_PRINT, void *);
extern int mime_state_update(MIME_STATE *, int, const char *, int);
extern MIME_STATE *mime_state_free(MIME_STATE *);
extern const char *mime_state_error(int);

 /*
  * Processing options.
  */
#define MIME_OPT_NONE				(0)
#define MIME_OPT_DOWNGRADE			(1<<0)
#define MIME_OPT_REPORT_8BIT_IN_7BIT_BODY	(1<<1)
#define MIME_OPT_REPORT_8BIT_IN_HEADER		(1<<2)
#define MIME_OPT_REPORT_ENCODING_DOMAIN		(1<<3)
#define MIME_OPT_RECURSE_ALL_MESSAGE		(1<<4)
#define MIME_OPT_REPORT_TRUNC_HEADER		(1<<5)
#define MIME_OPT_DISABLE_MIME			(1<<6)
#define MIME_OPT_REPORT_NESTING			(1<<7)

 /*
  * Body encoding domains.
  */
#define MIME_ENC_7BIT	(7)
#define MIME_ENC_8BIT	(8)
#define MIME_ENC_BINARY	(9)

 /*
  * Processing errors, not necessarily lethal.
  */
#define MIME_ERR_NESTING		(1<<0)
#define MIME_ERR_TRUNC_HEADER		(1<<1)
#define MIME_ERR_8BIT_IN_HEADER		(1<<2)
#define MIME_ERR_8BIT_IN_7BIT_BODY	(1<<3)
#define MIME_ERR_ENCODING_DOMAIN	(1<<4)

 /*
  * Header classes. Look at the header_opts argument to find out if something
  * is a MIME header in a primary or nested section.
  */
#define MIME_HDR_PRIMARY	(1)	/* initial headers */
#define MIME_HDR_MULTIPART	(2)	/* headers after multipart boundary */
#define MIME_HDR_NESTED		(3)	/* attached message initial headers */

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

#endif
