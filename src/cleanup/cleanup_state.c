/*++
/* NAME
/*	cleanup_state 3
/* SUMMARY
/*	per-message state variables
/* SYNOPSIS
/*	#include "cleanup.h"
/*
/*	CLEANUP_STATE *cleanup_state_alloc(void)
/*
/*	void	cleanup_state_free(state)
/*	CLEANUP_STATE *state;
/* DESCRIPTION
/*	This module maintains about two dozen state variables
/*	that are used by many routines in the course of processing one
/*	message.
/*
/*	cleanup_state_alloc() initializes the per-message state variables.
/*
/*	cleanup_state_free() cleans up.
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

/* Utility library. */

#include <mymalloc.h>
#include <vstring.h>
#include <argv.h>

/* Global library. */

#include <been_here.h>
#include <mail_params.h>

/* Application-specific. */

#include "cleanup.h"

/* cleanup_state_alloc - initialize global state */

CLEANUP_STATE *cleanup_state_alloc(void)
{
    CLEANUP_STATE *state = (CLEANUP_STATE *) mymalloc(sizeof(*state));

    state->temp1 = vstring_alloc(10);
    state->temp2 = vstring_alloc(10);
    state->dst = 0;
    state->handle = 0;
    state->queue_id = 0;
    state->time = 0;
    state->fullname = 0;
    state->sender = 0;
    state->from = 0;
    state->resent_from = 0;
    state->recip = 0;
    state->return_receipt = 0;
    state->errors_to = 0;
    state->flags = 0;
    state->errs = 0;
    state->err_mask = 0;
    state->header_buf = vstring_alloc(100);
    state->headers_seen = 0;
    state->long_header = 0;
    state->hop_count = 0;
    state->recipients = argv_alloc(2);
    state->resent_recip = argv_alloc(2);
    state->resent = "";
    state->dups = been_here_init(var_dup_filter_limit, BH_FLAG_FOLD);
    state->warn_time = 0;
    state->action = cleanup_envelope;
    state->mesg_offset = -1;
    state->data_offset = -1;
    state->xtra_offset = -1;
    state->end_seen = 0;
    state->rcpt_count = 0;
    state->reason = 0;
    return (state);
}

/* cleanup_state_free - destroy global state */

void    cleanup_state_free(CLEANUP_STATE *state)
{
    vstring_free(state->temp1);
    vstring_free(state->temp2);
    if (state->fullname)
	myfree(state->fullname);
    if (state->sender)
	myfree(state->sender);
    if (state->from)
	myfree(state->from);
    if (state->resent_from)
	myfree(state->resent_from);
    if (state->recip)
	myfree(state->recip);
    if (state->return_receipt)
	myfree(state->return_receipt);
    if (state->errors_to)
	myfree(state->errors_to);
    vstring_free(state->header_buf);
    argv_free(state->recipients);
    argv_free(state->resent_recip);
    if (state->queue_id)
	myfree(state->queue_id);
    been_here_free(state->dups);
    if (state->reason)
	myfree(state->reason);
    myfree((char *) state);
}
