/*++
/* NAME
/*	cleanup_map11 3
/* SUMMARY
/*	one-to-one mapping
/* SYNOPSIS
/*	#include <cleanup.h>
/*
/*	void	cleanup_map11_external(addr, maps, propagate)
/*	VSTRING	*addr;
/*	MAPS	*maps;
/*	int	propagate;
/*
/*	void	cleanup_map11_internal(addr, maps, propagate)
/*	VSTRING	*addr;
/*	MAPS	*maps;
/*	int	propagate;
/*
/*	void	cleanup_map11_tree(tree, maps, propagate)
/*	TOK822	*tree;
/*	MAPS	*maps;
/*	int	propagate;
/* DESCRIPTION
/*	This module performs one-to-one map lookups.
/*
/*	If an address has a mapping, the lookup result is
/*	subjected to another iteration of rewriting and mapping.
/*	Recursion continues until an address maps onto itself,
/*	or until an unreasonable recursion level is reached.
/*	An unmatched address extension is propagated when 
/*	\fIpropagate\fR is non-zero.
/*
/*	cleanup_map11_external() looks up the external (quoted) string
/*	form of an address in the maps specified via the \fImaps\fR argument.
/*
/*	cleanup_map11_internal() is a wrapper around the
/*	cleanup_map11_external() routine that transforms from
/*	internal (quoted) string form to external form and back.
/*
/*	cleanup_map11_tree() is a wrapper around the
/*	cleanup_map11_external() routine that transforms from
/*	internal parse tree form to external form and back.
/* DIAGNOSTICS
/*	Recoverable errors: the global \fIcleanup_errs\fR flag is updated.
/* SEE ALSO
/*	mail_addr_find(3) address lookups
/*	mail_addr_map(3) address mappings
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

#ifdef STRCASECMP_IN_STRINGS_H
#include <strings.h>
#endif

/* Utility library. */

#include <msg.h>
#include <vstring.h>
#include <dict.h>
#include <mymalloc.h>

/* Global library. */

#include <cleanup_user.h>
#include <mail_addr_map.h>
#include <quote_822_local.h>

/* Application-specific. */

#include "cleanup.h"

#define STR		vstring_str
#define MAX_RECURSION	10

/* cleanup_map11_external - one-to-one table lookups */

void    cleanup_map11_external(VSTRING *addr, MAPS *maps, int propagate)
{
    int     count;
    int     expand_to_self;
    ARGV   *new_addr;
    char   *saved_addr;

    /*
     * Produce sensible output even in the face of a recoverable error. This
     * simplifies error recovery considerably because we can do delayed error
     * checking in one place, instead of having error handling code all over
     * the place.
     */
    for (count = 0; count < MAX_RECURSION; count++) {
	if ((new_addr = mail_addr_map(maps, STR(addr), propagate)) != 0) {
	    if (new_addr->argc > 1)
		msg_warn("%s: multi-valued %s entry for %s",
			 cleanup_queue_id, maps->title, STR(addr));
	    saved_addr = mystrdup(STR(addr));
	    vstring_strcpy(addr, new_addr->argv[0]);
	    expand_to_self = !strcasecmp(saved_addr, STR(addr));
	    myfree(saved_addr);
	    argv_free(new_addr);
	    if (expand_to_self)
		return;
	} else if (dict_errno != 0) {
	    msg_warn("%s: %s map lookup problem for %s",
		     cleanup_queue_id, maps->title, STR(addr));
	    cleanup_errs |= CLEANUP_STAT_WRITE;
	    return;
	} else {
	    return;
	}
    }
    msg_warn("%s: unreasonable %s map nesting for %s",
	     cleanup_queue_id, maps->title, STR(addr));
}

/* cleanup_map11_tree - rewrite address node */

void    cleanup_map11_tree(TOK822 *tree, MAPS *maps, int propagate)
{
    VSTRING *temp = vstring_alloc(100);

    /*
     * Produce sensible output even in the face of a recoverable error. This
     * simplifies error recovery considerably because we can do delayed error
     * checking in one place, instead of having error handling code all over
     * the place.
     */
    tok822_externalize(temp, tree->head, TOK822_STR_DEFL);
    cleanup_map11_external(temp, maps, propagate);
    tok822_free_tree(tree->head);
    tree->head = tok822_scan(STR(temp), &tree->tail);
    vstring_free(temp);
}

/* cleanup_map11_internal - rewrite address internal form */

void    cleanup_map11_internal(VSTRING *addr, MAPS *maps, int propagate)
{
    VSTRING *temp = vstring_alloc(100);

    /*
     * Produce sensible output even in the face of a recoverable error. This
     * simplifies error recovery considerably because we can do delayed error
     * checking in one place, instead of having error handling code all over
     * the place.
     */
    quote_822_local(temp, STR(addr));
    cleanup_map11_external(temp, maps, propagate);
    unquote_822_local(addr, STR(temp));
    vstring_free(temp);
}
