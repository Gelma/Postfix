/*++
/* NAME
/*	qmgr_entry 3
/* SUMMARY
/*	per-site queue entries
/* SYNOPSIS
/*	#include "qmgr.h"
/*
/*	QMGR_ENTRY *qmgr_entry_create(peer, message)
/*      QMGR_PEER *peer;
/*	QMGR_MESSAGE *message;
/*
/*	void	qmgr_entry_done(entry, which)
/*	QMGR_ENTRY *entry;
/*	int	which;
/*
/*	QMGR_ENTRY *qmgr_entry_select(queue)
/*	QMGR_QUEUE *queue;
/*
/*	void	qmgr_entry_unselect(queue, entry)
/*	QMGR_QUEUE *queue;
/*	QMGR_ENTRY *entry;
/* DESCRIPTION
/*	These routines add/delete/manipulate per-site message
/*	delivery requests.
/*
/*	qmgr_entry_create() creates an entry for the named peer and message,
/*      and appends the entry to the peer's list and its queue's todo list.
/*	Filling in and cleaning up the recipients is the responsibility
/*	of the caller.
/*
/*	qmgr_entry_done() discards a per-site queue entry.  The
/*	\fIwhich\fR argument is either QMGR_QUEUE_BUSY for an entry
/*	of the site's `busy' list (i.e. queue entries that have been
/*	selected for actual delivery), or QMGR_QUEUE_TODO for an entry
/*	of the site's `todo' list (i.e. queue entries awaiting selection
/*	for actual delivery).
/*
/*	qmgr_entry_done() discards its peer structure when the peer
/*      is not referenced anymore.
/*
/*	qmgr_entry_done() triggers cleanup of the per-site queue when
/*	the site has no pending deliveries, and the site is either
/*	alive, or the site is dead and the number of in-core queues
/*	exceeds a configurable limit (see qmgr_queue_done()).
/*
/*	qmgr_entry_done() triggers special action when the last in-core
/*	queue entry for a message is done with: either read more
/*	recipients from the queue file, delete the queue file, or move
/*	the queue file to the deferred queue; send bounce reports to the
/*	message originator (see qmgr_active_done()).
/*
/*	qmgr_entry_select() selects first entry from the named
/*	per-site queue's `todo' list for actual delivery. The entry is
/*	moved to the queue's `busy' list: the list of messages being
/*	delivered. The entry is also removed from its peer list.
/*
/*	qmgr_entry_unselect() takes the named entry off the named
/*	per-site queue's `busy' list and moves it to the queue's
/*	`todo' list. The entry is also appended to its peer list again.
/* DIAGNOSTICS
/*	Panic: interface violations, internal inconsistencies.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*
/*	Scheduler enhancements:
/*	Patrik Rak
/*	Modra 6
/*	155 00, Prague, Czech Republic
/*--*/

/* System library. */

#include <sys_defs.h>
#include <stdlib.h>
#include <time.h>

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <events.h>
#include <vstream.h>

/* Global library. */

#include <mail_params.h>

/* Application-specific. */

#include "qmgr.h"

/* qmgr_entry_select - select queue entry for delivery */

QMGR_ENTRY *qmgr_entry_select(QMGR_PEER *peer)
{
    QMGR_ENTRY *entry;
    QMGR_QUEUE *queue;

    if ((entry = peer->entry_list.next) != 0) {
	queue = entry->queue;
	QMGR_LIST_UNLINK(queue->todo, QMGR_ENTRY *, entry, queue_peers);
	queue->todo_refcount--;
	QMGR_LIST_APPEND(queue->busy, entry, queue_peers);
	queue->busy_refcount++;
	QMGR_LIST_UNLINK(peer->entry_list, QMGR_ENTRY *, entry, peer_peers);
	peer->job->selected_entries++;
    }
    return (entry);
}

/* qmgr_entry_unselect - unselect queue entry for delivery */

void    qmgr_entry_unselect(QMGR_ENTRY *entry)
{
    QMGR_PEER *peer = entry->peer;
    QMGR_QUEUE *queue = entry->queue;

    QMGR_LIST_UNLINK(queue->busy, QMGR_ENTRY *, entry, queue_peers);
    queue->busy_refcount--;
    QMGR_LIST_APPEND(queue->todo, entry, queue_peers);
    queue->todo_refcount++;
    QMGR_LIST_APPEND(peer->entry_list, entry, peer_peers);
    peer->job->selected_entries--;
}

/* qmgr_entry_done - dispose of queue entry */

void    qmgr_entry_done(QMGR_ENTRY *entry, int which)
{
    QMGR_QUEUE *queue = entry->queue;
    QMGR_MESSAGE *message = entry->message;
    QMGR_PEER *peer = entry->peer;
    QMGR_JOB *sponsor,
           *job = peer->job;
    QMGR_TRANSPORT *transport = job->transport;

    /*
     * Take this entry off the in-core queue.
     */
    if (entry->stream != 0)
	msg_panic("qmgr_entry_done: file is open");
    if (which == QMGR_QUEUE_BUSY) {
	QMGR_LIST_UNLINK(queue->busy, QMGR_ENTRY *, entry, queue_peers);
	queue->busy_refcount--;
    } else if (which == QMGR_QUEUE_TODO) {
	QMGR_LIST_UNLINK(peer->entry_list, QMGR_ENTRY *, entry, peer_peers);
	job->selected_entries++;
	QMGR_LIST_UNLINK(queue->todo, QMGR_ENTRY *, entry, queue_peers);
	queue->todo_refcount--;
    } else {
	msg_panic("qmgr_entry_done: bad queue spec: %d", which);
    }

    /*
     * Decrease the in-core recipient counts and free the recipient list and
     * the structure itself.
     */
    job->rcpt_count -= entry->rcpt_list.len;
    message->rcpt_count -= entry->rcpt_list.len;
    qmgr_recipient_count -= entry->rcpt_list.len;
    qmgr_rcpt_list_free(&entry->rcpt_list);
    myfree((char *) entry);

    /*
     * Make sure that the transport of any retired or finishing job that
     * donated recipient slots to this message gets them back first. Then, if
     * possible, pass the remaining unused recipient slots to the next job on
     * the job list.
     */
    for (sponsor = message->job_list.next; sponsor; sponsor = sponsor->message_peers.next) {
	if (sponsor->rcpt_count >= sponsor->rcpt_limit || sponsor == job)
	    continue;
	if (sponsor->stack_level < 0 || message->rcpt_offset == 0)
	    qmgr_job_move_limits(sponsor);
    }
    if (message->rcpt_offset == 0) {
	qmgr_job_move_limits(job);
    }

    /*
     * If the queue was blocking some of the jobs on the job list, check if
     * the concurrency limit has lifted. If there are still some pending
     * deliveries, give it a try and unmark all transport blockers at once.
     * The qmgr_job_entry_select() will do the rest. In either case make sure
     * the queue is not marked as a blocker anymore, with extra handling of
     * queues which were declared dead.
     * 
     * Note that changing the blocker status also affects the candidate cache.
     * Most of the cases would be automatically recognized by the current job
     * change, but we play safe and reset the cache explicitly below.
     * 
     * Keeping the transport blocker tag odd is an easy way to make sure the tag
     * never matches jobs that are not explicitly marked as blockers.
     */
    if (queue->blocker_tag == transport->blocker_tag) {
	if (queue->window > queue->busy_refcount && queue->todo.next != 0) {
	    transport->blocker_tag += 2;
	    transport->job_current = transport->job_list.next;
	    transport->candidate_cache_current = 0;
	}
	if (queue->window > queue->busy_refcount || queue->window == 0)
	    queue->blocker_tag = 0;
    }

    /*
     * When there are no more entries for this peer, discard the peer
     * structure.
     */
    peer->refcount--;
    if (peer->refcount == 0)
	qmgr_peer_free(peer);

    /*
     * When the in-core queue for this site is empty and when this site is
     * not dead, discard the in-core queue. When this site is dead, but the
     * number of in-core queues exceeds some threshold, get rid of this
     * in-core queue anyway, in order to avoid running out of memory.
     */
    if (queue->todo.next == 0 && queue->busy.next == 0) {
	if (queue->window == 0 && qmgr_queue_count > 2 * var_qmgr_rcpt_limit)
	    qmgr_queue_unthrottle(queue);
	if (queue->window > 0)
	    qmgr_queue_done(queue);
    }

    /*
     * Update the in-core message reference count. When the in-core message
     * structure has no more references, dispose of the message.
     */
    message->refcount--;
    if (message->refcount == 0)
	qmgr_active_done(message);
}

/* qmgr_entry_create - create queue todo entry */

QMGR_ENTRY *qmgr_entry_create(QMGR_PEER *peer, QMGR_MESSAGE *message)
{
    QMGR_ENTRY *entry;
    QMGR_QUEUE *queue = peer->queue;

    /*
     * Sanity check.
     */
    if (queue->window == 0)
	msg_panic("qmgr_entry_create: dead queue: %s", queue->name);

    entry = (QMGR_ENTRY *) mymalloc(sizeof(QMGR_ENTRY));
    entry->stream = 0;
    entry->message = message;
    qmgr_rcpt_list_init(&entry->rcpt_list);
    message->refcount++;
    entry->peer = peer;
    QMGR_LIST_APPEND(peer->entry_list, entry, peer_peers);
    peer->refcount++;
    entry->queue = queue;
    QMGR_LIST_APPEND(queue->todo, entry, queue_peers);
    queue->todo_refcount++;
    return (entry);
}
