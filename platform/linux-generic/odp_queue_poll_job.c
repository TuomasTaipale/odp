/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Nokia
 */

#include <odp_queue_poll_job.h>

int _odp_qpj_reserve_ws(void)
{
	static uint8_t ws;

	return ws < __ODP_QPJ_MAX_WS ? ws++ : -1;
}

void _odp_qpj_init_wss(_odp_qpj_wss_t *wss)
{
	_odp_qpj_ws_t *ws;

	odp_ticketlock_init(&wss->lock);

	for (uint32_t i = 0U; i < __ODP_QPJ_MAX_WS; ++i) {
		ws = &wss->wss[i];
		TAILQ_INIT(&ws->slots);
		ws->slot_mask = 0U;
		ws->num = 0U;
	}

	wss->num = 0U;
	wss->next = 0U;
}

void _odp_qpj_init_ws(_odp_qpj_ws_data_t *ws_data, _odp_qpj_deq_fn_t deq_fn, void *data,
		      uint8_t ws_idx, uint8_t slot_idx)
{
	ws_data->deq = deq_fn;
	ws_data->data = data;
	ws_data->ws_idx = ws_idx;
	ws_data->slot_idx = slot_idx;
}
