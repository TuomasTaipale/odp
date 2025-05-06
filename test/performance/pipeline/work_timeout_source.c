#include <stdint.h>
#include <stdlib.h>

#include <odp_api.h>
#include <odp/helper/odph_api.h>

#include "common.h"
#include "config_parser.h"
#include "work.h"

typedef struct {
	odp_timer_pool_t tmr_pool;
	odp_pool_t tmo_pool;
	odp_queue_t queue;
	uint64_t timeout_ns;
	odp_timer_t tmr;
} work_timeout_source_data_t;

static int work_timeout_source(uintptr_t data, odp_event_t *ev ODP_UNUSED, int num ODP_UNUSED,
			       work_stats_t *stats)
{
	int ret;

	work_timeout_source_data_t *priv = (work_timeout_source_data_t *)data;
	odp_timeout_t tmo;
	odp_timer_start_t start;

	if (priv->tmr == ODP_TIMER_INVALID) {
		priv->tmr = odp_timer_alloc(priv->tmr_pool, priv->queue, NULL);

		if (priv->tmr == ODP_TIMER_INVALID)
			ODPH_ABORT("Error allocating timer, aborting\n");
	}

	tmo = odp_timeout_alloc(priv->tmo_pool);

	if (tmo == ODP_TIMEOUT_INVALID)
		return 0;

	start.tick_type = ODP_TIMER_TICK_REL;
	start.tick = odp_timer_ns_to_tick(priv->tmr_pool, priv->timeout_ns);
	start.tmo_ev = odp_timeout_to_event(tmo);
	ret = odp_timer_start(priv->tmr, &start);

	if (ret == ODP_TIMER_FAIL)
		ODPH_ABORT("Error arming timer, aborting\n");

	if (ret == ODP_TIMER_TOO_NEAR)
		++stats->data1;
	else if (ret == ODP_TIMER_TOO_FAR)
		++stats->data2;
	else
		++stats->data3;

	return 0;
}

static void work_timeout_source_init(const work_param_t *param, work_init_t *init)
{
	work_timeout_source_data_t *data = calloc(1U, sizeof(*data));

	if (data == NULL)
		ODPH_ABORT("Error allocating memory, aborting\n");

	init->fn = work_timeout_source;
	data->tmr_pool = (odp_timer_pool_t)config_parser_get_resource(TIMER_DOMAIN,
								      param->param.timer);
	data->tmo_pool = (odp_pool_t)config_parser_get_resource(POOL_DOMAIN,
								param->param.timeout_pool);
	data->queue = (odp_queue_t)config_parser_get_resource(QUEUE_DOMAIN, param->queue);
	data->timeout_ns = param->param.timeout_ns;
	data->tmr = ODP_TIMER_INVALID;
	init->data = (uintptr_t)data;
}

static void work_timeout_source_print(const char *queue, const work_stats_t *stats)
{
	printf("\n%s:\n"
	       "  work:           %s\n"
	       "  timer too near: %" PRIu64 "\n"
	       "  timer too far:  %" PRIu64 "\n"
	       "  timer arms:     %" PRIu64 "\n", queue, WORK_TIMEOUT_SOURCE, stats->data1,
	       stats->data2, stats->data3);
}

static void work_timeout_source_destroy(uintptr_t data)
{
	work_timeout_source_data_t *priv = (work_timeout_source_data_t *)data;
	odp_event_t ev;
	int ret;

	ret = odp_timer_cancel(priv->tmr, &ev);

	if (ret == ODP_TIMER_SUCCESS) {
		odp_timer_free(priv->tmr);
		odp_event_free(ev);
	}

	free(priv);
}

WORK_AUTOREGISTER(WORK_TIMEOUT_SOURCE, work_timeout_source_init, work_timeout_source_print,
		  work_timeout_source_destroy)
