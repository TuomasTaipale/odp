#include <stdint.h>
#include <stdlib.h>

#include <odp_api.h>
#include <odp/helper/odph_api.h>

#include "common.h"
#include "config_parser.h"
#include "work.h"

#define CONF_DMA_SESSION "dma_session"
#define CONF_STR_QS "reassembly_queues"

#define WORK_DMA_REASSEMBLY "dma_reassembly"
#define WORK_DMA_SEGMENTATION "dma_segmentation"

typedef struct {
	odp_atomic_u32_t num_pending;
	odp_queue_t q;
} reassembly_context_t;

typedef struct {
	uint32_t num;
	odp_dma_t dma;
	reassembly_context_t *ctxs;
} work_reassembly_data_t;

static work_reassembly_data_t common;

static int work_dma_reassembly(uintptr_t data ODP_UNUSED, odp_event_t ev[] ODP_UNUSED,
			       int num ODP_UNUSED, work_stats_t *stats ODP_UNUSED)
{
	return 0;
}

static void work_dma_reassembly_init(const work_param_t *param ODP_UNUSED,
				     work_init_t *init ODP_UNUSED)
{
	const char *val_str;

	if (param->param == NULL)
		ODPH_ABORT("No parameters available\n");

	if (config_setting_length(param->param) != 1)
		ODPH_ABORT("No valid parameters available\n");

	val_str = config_setting_get_string_elem(param->param, 0);

	if (val_str == NULL)
		ODPH_ABORT("No \"" CONF_DMA_SESSION "\"found\n");

	common.dma = (odp_dma_t)config_parser_get(DMA_DOMAIN, val_str);
	init->fn = work_dma_reassembly;
	init->data = (uintptr_t)&common;
}

static void work_dma_reassembly_print(const char *queue ODP_UNUSED,
				      const work_stats_t *stats ODP_UNUSED)
{
}

static void work_dma_reassembly_destroy(uintptr_t data ODP_UNUSED)
{
}

WORK_AUTOREGISTER(WORK_DMA_REASSEMBLY, work_dma_reassembly_init, work_dma_reassembly_print,
		  work_dma_reassembly_destroy)

static int work_dma_segmentation(uintptr_t data ODP_UNUSED, odp_event_t ev[] ODP_UNUSED,
				 int num ODP_UNUSED, work_stats_t *stats ODP_UNUSED)
{
	return 0;
}

static void work_dma_segmentation_init(const work_param_t *param ODP_UNUSED,
				       work_init_t *init ODP_UNUSED)
{
	const char *val_str;
	config_setting_t *elem;
	int num;
	reassembly_context_t *ctx;

	if (param->param == NULL)
		ODPH_ABORT("No parameters available\n");

	if (common.ctxs == NULL) {
		if (config_setting_length(param->param) != 1)
			ODPH_ABORT("No valid parameters available\n");

		elem = config_setting_get_elem(param->param, 0);

		if (elem == NULL)
			ODPH_ABORT("No \"" CONF_STR_QS "\"found\n");

		num = config_setting_length(elem);

		if (num == 0)
			ODPH_ABORT("No valid \"" CONF_STR_QS "\"found\n");

		common.ctxs = calloc(1U, num * sizeof(*common.ctxs));

		if (common.ctxs == NULL)
			ODPH_ABORT("Error allocating memory, aborting\n");

		for (int i = 0; i < num; ++i) {
			ctx = &common.ctxs[i];
			odp_atomic_init_u32(&ctx->num_pending, 0U);
			val_str = config_setting_get_string_elem(elem, i);
			ctx->q = (odp_queue_t)config_parser_get(QUEUE_DOMAIN, val_str);
		}

		common.num = num;
	}

	init->fn = work_dma_segmentation;
	init->data = (uintptr_t)&common;
}

static void work_dma_segmentation_print(const char *queue ODP_UNUSED,
					const work_stats_t *stats ODP_UNUSED)
{
}

static void work_dma_segmentation_destroy(uintptr_t data ODP_UNUSED)
{
}

WORK_AUTOREGISTER(WORK_DMA_SEGMENTATION, work_dma_segmentation_init, work_dma_segmentation_print,
		  work_dma_segmentation_destroy)
