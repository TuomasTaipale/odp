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

/* Status code for active poll job. */
#define _ODP_QPJ_KEEP 0
/* Status code for completed poll job. */
#define _ODP_QPJ_DONE -1

#define __ODP_QPJ_MAX_WS 2U
#define __ODP_QPJ_MAX_WS_LEN 32U

/* Job dequeue function, returns _ODP_QPJ_KEEP or _ODP_QPJ_DONE and sets 'num_out' based on how
 * many events were dequed from different jobs (>=0). */
typedef int (*_odp_qpj_deq_fn_t)(odp_queue_t queue, _odp_event_hdr_t **event_hdr, int num,
				 void *data, int *num_out);

typedef struct {
	_odp_qpj_deq_fn_t deq;
	void *data;
	uint8_t ws_idx;
	uint8_t slot_idx;
} _odp_qpj_ws_data_t;

typedef struct {
	_odp_qpj_ws_data_t *slots[__ODP_QPJ_MAX_WS_LEN];
	uint8_t next;
	uint8_t num;
} _odp_qpj_ws_t;

/* Add poll jobs to a queue, returns 0 if job added successfully, <0 otherwise. */
typedef int (*_odp_qpj_add_fn_t)(odp_queue_t queue, _odp_qpj_ws_data_t *data);

typedef struct {
	odp_ticketlock_t lock;
	_odp_qpj_ws_t wss[__ODP_QPJ_MAX_WS];
	uint8_t next;
	uint8_t num;
} _odp_qpj_wss_t;

/* Reserve work slot for module, returns >=0 if successfully able to reserve slot, <0 otherwise.
 * Modules requiring a work slot should call this function only **once**. */
int _odp_qpj_reserve_ws(void);

/* Initialize work slots, to be called during queue initializtion. */
void _odp_qpj_init_wss(_odp_qpj_wss_t *wss);

/* Initialize a work slot, to be called to initialize a to-be-added poll job. */
void _odp_qpj_init_ws(_odp_qpj_ws_data_t *ws_data, _odp_qpj_deq_fn_t deq_fn, void *data,
		      uint8_t ws_idx, uint8_t slot_idx);

/* Add poll job to active polling list. */
void _odp_qpj_add(_odp_qpj_wss_t *wss, _odp_qpj_ws_data_t *data);

/* Polls jobs of a queue, returns >0 in case events successfully polled, _ODP_QPJ_KEEP or
 * _ODP_QPJ_DONE otherwise. */
int _odp_qpj_poll(_odp_qpj_wss_t *wss, odp_queue_t queue, _odp_event_hdr_t **event_hdr, int num);

static inline odp_bool_t _odp_qpj_has_jobs(_odp_qpj_wss_t *wss)
{
	odp_bool_t has_jobs;

	odp_ticketlock_lock(&wss->lock);
	has_jobs = wss->num > 0U;
	odp_ticketlock_unlock(&wss->lock);

	return has_jobs;
}

static inline odp_bool_t _odp_qpj_is_err(int ret)
{
	return ret == _ODP_QPJ_KEEP || ret == _ODP_QPJ_DONE;
}

#ifdef __cplusplus
}
#endif

#endif
