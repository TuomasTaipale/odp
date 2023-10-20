/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Nokia
 */

#ifndef ODP_QUEUE_POLL_JOB_H_
#define ODP_QUEUE_POLL_JOB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/api/hints.h>
#include <odp/api/queue.h>
#include <odp/api/ticketlock.h>

#include <odp_event_internal.h>

#include <sys/queue.h>

/* Status code for completed poll job. */
#define _QPJ_DONE -1
/* Status code for poll job that is still active but was not able to return events. */
#define _QPJ_KEEP -2

/* Job dequeue function, returns >=0 in case of successful dequeue, _QPJ_DONE in case of polling
 * can be stopped (error or resource otherwise not relevant anymore). */
typedef int (*poll_deq_fn_t)(odp_queue_t queue, _odp_event_hdr_t **event_hdr, int num, void *data);

typedef struct _odp_qpj_poll_job_s {
	TAILQ_ENTRY(_odp_qpj_poll_job_s) j;

	void *data;
	poll_deq_fn_t deq;
} _odp_qpj_poll_job_t;

/* Add poll jobs to a queue, returns 0 if job added successfully, <0 otherwise. */
typedef int (*poll_add_fn_t)(odp_queue_t queue, _odp_qpj_poll_job_t *job);

typedef struct {
	TAILQ_HEAD(_odp_qpj_poll_jobs_s, _odp_qpj_poll_job_s) head;

	odp_ticketlock_t lock;
	odp_atomic_u32_t num;
} _odp_qpj_poll_jobs_t;

/* Initialize jobs, to be called during queue initializtion. */
static inline void _odp_qpj_init_jobs(_odp_qpj_poll_jobs_t *jobs)
{
	TAILQ_INIT(&jobs->head);
	odp_ticketlock_init(&jobs->lock);
	odp_atomic_init_u32(&jobs->num, 0U);
}

/* Add poll job to active polling list */
static inline void _odp_qpj_add(_odp_qpj_poll_jobs_t *jobs, _odp_qpj_poll_job_t *job)
{
	odp_ticketlock_lock(&jobs->lock);
	TAILQ_INSERT_TAIL(&jobs->head, job, j);
	odp_atomic_inc_u32(&jobs->num);
	odp_ticketlock_unlock(&jobs->lock);
}

/* Remove poll job from active polling list */
static inline void _odp_qpj_rem(_odp_qpj_poll_jobs_t *jobs, _odp_qpj_poll_job_t *job)
{
	odp_ticketlock_lock(&jobs->lock);
	TAILQ_REMOVE(&jobs->head, job, j);
	odp_atomic_dec_u32(&jobs->num);
	odp_ticketlock_unlock(&jobs->lock);
}

/* Destroy jobs, to be called during queue teardown. */
static inline void _odp_qpj_destroy_jobs(_odp_qpj_poll_jobs_t *jobs)
{
	odp_ticketlock_lock(&jobs->lock);

	for (_odp_qpj_poll_job_t *job = jobs->head.tqh_first; job != NULL; job = job->j.tqe_next)
		TAILQ_REMOVE(&jobs->head, job, j);

	odp_atomic_store_u32(&jobs->num, 0U);
	odp_ticketlock_unlock(&jobs->lock);
}

/* Polls jobs of a queue, returns >0 in case events successfully polled, _QPJ_DONE or _QPJ_KEEP
 * otherwise. */
static inline int _odp_qpj_poll(_odp_qpj_poll_jobs_t *jobs, odp_queue_t queue,
				_odp_event_hdr_t **event_hdr, int num)
{
	int num_deq = 0, ret = _QPJ_KEEP;

	if (odp_likely(odp_atomic_load_u32(&jobs->num) == 0))
		return _QPJ_DONE;

	odp_ticketlock_lock(&jobs->lock);

	for (_odp_qpj_poll_job_t *job = jobs->head.tqh_first; job != NULL; job = job->j.tqe_next) {
		num_deq = job->deq(queue, event_hdr, num, job->data);

		if (num_deq == _QPJ_DONE) {
			TAILQ_REMOVE(&jobs->head, job, j);
			odp_atomic_dec_u32(&jobs->num);
			continue;
		} else {
			/* Job isn't removed, move to last to give other jobs a chance to get
			 * polled */
			TAILQ_REMOVE(&jobs->head, job, j);
			TAILQ_INSERT_TAIL(&jobs->head, job, j);
		}

		break;
	}

	odp_ticketlock_unlock(&jobs->lock);

	if (odp_atomic_load_u32(&jobs->num) == 0)
		ret = _QPJ_DONE;

	if (num_deq > 0)
		ret = num_deq;

	return ret;
}

static inline odp_bool_t _odp_qpj_has_jobs(_odp_qpj_poll_jobs_t *jobs)
{
	return odp_atomic_load_u32(&jobs->num) > 0U;
}

#ifdef __cplusplus
}
#endif

#endif
