/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Nokia
 */

#include <odp_queue_poll_job.h>

#include <string.h>

int _odp_qpj_reserve_ws(void)
{
	static uint8_t ws;

	return ws < __ODP_QPJ_MAX_WS ? ws++ : -1;
}

void _odp_qpj_init_wss(_odp_qpj_wss_t *wss)
{
	memset(wss, 0, sizeof(*wss));
	odp_ticketlock_init(&wss->lock);
}

void _odp_qpj_init_ws(_odp_qpj_ws_data_t *ws_data, _odp_qpj_deq_fn_t deq_fn, void *data,
		      uint8_t ws_idx, uint8_t slot_idx)
{
	ws_data->deq = deq_fn;
	ws_data->data = data;
	ws_data->ws_idx = ws_idx;
	ws_data->slot_idx = slot_idx;
}

void _odp_qpj_add(_odp_qpj_wss_t *wss, _odp_qpj_ws_data_t *data)
{
	_odp_qpj_ws_t *ws;
	_odp_qpj_ws_data_t **slot;

	odp_ticketlock_lock(&wss->lock);
	ws = &wss->wss[data->ws_idx];
	slot = &ws->slots[data->slot_idx];

	if (*slot != NULL) {
		odp_ticketlock_unlock(&wss->lock);
		return;
	}

	if (ws->num == 0U)
		++wss->num;

	*slot = data;
	++ws->num;
	odp_ticketlock_unlock(&wss->lock);
}

int _odp_qpj_poll(_odp_qpj_wss_t *wss, odp_queue_t queue, _odp_event_hdr_t **event_hdr, int num)
{
	_odp_qpj_ws_t *ws;
	int num_deq, num_tot = 0, ret;
	_odp_qpj_ws_data_t **slot;

	odp_ticketlock_lock(&wss->lock);

	if (wss->num == 0U) {
		odp_ticketlock_unlock(&wss->lock);
		return _ODP_QPJ_DONE;
	}

	ws = &wss->wss[wss->next++ % __ODP_QPJ_MAX_WS];

	for (uint8_t i = 0U; i < ws->num;) {
		if (num_tot == num)
			break;

		slot = &ws->slots[ws->next++ % __ODP_QPJ_MAX_WS_LEN];

		if (*slot == NULL)
			continue;

		++i;
		ret = (*slot)->deq(queue, event_hdr, num - num_tot, (*slot)->data, &num_deq);
		num_tot += num_deq;

		if (ret == _ODP_QPJ_DONE) {
			*slot = NULL;
			--ws->num;

			if (ws->num == 0U)
				--wss->num;
		}
	}

	ret = num_tot > 0 ? num_tot : wss->num == 0 ? _ODP_QPJ_DONE : _ODP_QPJ_KEEP;
	odp_ticketlock_unlock(&wss->lock);

	return ret;
}
