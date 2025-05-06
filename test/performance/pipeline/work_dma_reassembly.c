#include <stdint.h>
#include <stdlib.h>

#include <odp_api.h>
#include <odp/helper/odph_api.h>

#include "common.h"
#include "config_parser.h"
#include "work.h"

#define CONF_STR_SESSION "dma_session"
#define CONF_STR_COMPL_Q "dma_completion_queue"
#define CONF_STR_DST_POOL "destination_pool"
#define CONF_STR_COMPL_POOL "completion_pool"
#define CONF_STR_INFLIGHT "inflight_stash"
#define CONF_STR_NUM_FRAG "num_fragments"
#define CONF_STR_OFF "data_offset"
#define CONF_STR_LSO_ENABLE "enable_lso"
#define CONF_STR_PKTIO "pktio"
#define CONF_STR_PAYLOAD_LEN "payload_len"
#define CONF_STR_NUM_DIR "num_dir_qs"
#define CONF_STR_OUT_QS "output_queues"
#define CONF_STR_QS "fragment_queues"

#define WORK_DMA_SEGMENTATION "dma_segmentation"
#define WORK_DMA_COMPLETION "dma_completion"

typedef struct {
	odp_ticketlock_t lock;
	odp_queue_t q;
	odp_atomic_u32_t num_pending;
} reassembly_context_t;

typedef struct {
	reassembly_context_t *ctxs;

	union {
		odp_dma_t dma;
		odp_dma_t *dmas;
	};

	odp_queue_t compl_q;
	odp_pool_t dst_pool;
	odp_pool_t compl_pool;
	odp_stash_t inflight;
	uint32_t num_ctx;
	uint32_t num_dmas;
	uint32_t offset;
	uint32_t num_frag;
} work_reassembly_data_t;

typedef struct {
	odp_packet_t *src;
	odp_packet_t dst;
	odp_stash_t inflight;
	uint32_t num;
} trs_t;

typedef struct work_compl_data_t {
	union {
		odp_pktout_queue_t *dir_qs;
		odp_queue_t *qs;
	};

	odp_pktio_t pktio;
	odp_packet_lso_opt_t opt;
	uint32_t num;
} work_compl_data_t;

static work_reassembly_data_t common;
static work_compl_data_t compl;

static int work_dma_completion(uintptr_t data, odp_event_t ev[], int num, work_stats_t *stats)
{
	odp_event_t e;
	odp_dma_compl_t c_e;
	odp_dma_result_t res;
	trs_t *trs;
	work_compl_data_t *priv = (work_compl_data_t *)data;

	for (int i = 0; i < num; ++i) {
		e = ev[i];

		if (odp_unlikely(odp_event_type(e) != ODP_EVENT_DMA_COMPL))
			ODPH_ABORT("Invalid DMA completion event received, aborting\n");

		c_e = odp_dma_compl_from_event(e);
		odp_dma_compl_result(c_e, &res);

		if (odp_unlikely(!res.success))
			ODPH_ABORT("DMA transfer failed, aborting\n");

		trs = res.user_ptr;

		if (priv->opt.lso_profile == ODP_LSO_PROFILE_INVALID) {
			if (odp_unlikely(odp_queue_enq(priv->qs[(odp_thread_id() - 1) % priv->num],
						       odp_packet_to_event(trs->dst)) < 0))
				ODPH_ABORT("Error enqueueing, aborting\n");
		} else {
			if (odp_unlikely(odp_pktout_send_lso(priv->dir_qs[(odp_thread_id() - 1) %
							     priv->num],
							     &trs->dst, 1U, &priv->opt) != 1))
				ODPH_ABORT("Error sending, aborting\n");
		}

		odp_packet_free_multi(trs->src, trs->num);

		if (odp_unlikely(odp_stash_put_ptr(trs->inflight, (uintptr_t *)&trs, 1) != 1))
			ODPH_ABORT("Error putting to stash, aborting\n");

		odp_dma_compl_free(c_e);
		++stats->data1;
	}

	return num;
}

static void work_dma_completion_init(const work_param_t *param, work_init_t *init)
{
	int num, val_i;
	const char *val_str;
	odp_lso_profile_param_t l_param;
	config_setting_t *elem;

	if (param->param == NULL)
		ODPH_ABORT("No parameters available\n");

	if (compl.dir_qs == NULL) {
		num = config_setting_length(param->param);

		if (num != 5 && num != 2)
			ODPH_ABORT("No valid parameters available\n");

		val_i = config_setting_get_int_elem(param->param, 0);

		if (val_i == -1)
			ODPH_ABORT("No \"" CONF_STR_LSO_ENABLE "\" found\n");

		compl.opt.lso_profile = ODP_LSO_PROFILE_INVALID;

		if (val_i) {
			/* Fragment the DMA assembled packet with LSO */
			val_str = config_setting_get_string_elem(param->param, 1);

			if (val_str == NULL)
				ODPH_ABORT("No \"" CONF_STR_PKTIO "\" found\n");

			compl.pktio = (odp_pktio_t)config_parser_get(PKTIO_DOMAIN, val_str);
			val_i = config_setting_get_int_elem(param->param, 2);

			if (val_i == -1)
				ODPH_ABORT("No \"" CONF_STR_OFF "\" found\n");

			compl.opt.payload_offset = val_i;
			val_i = config_setting_get_int_elem(param->param, 3);

			if (val_i == -1)
				ODPH_ABORT("No \"" CONF_STR_PAYLOAD_LEN "\" found\n");

			compl.opt.max_payload_len = val_i;
			val_i = config_setting_get_int_elem(param->param, 4);

			if (val_i == -1)
				ODPH_ABORT("No \"" CONF_STR_NUM_DIR "\" found\n");

			compl.num = val_i;
			compl.dir_qs = calloc(1U, compl.num * sizeof(*compl.dir_qs));

			if (compl.dir_qs == NULL)
				ODPH_ABORT("Error allocating memory, aborting\n");

			if (odp_pktout_queue(compl.pktio, compl.dir_qs, compl.num) !=
			    (int)compl.num)
				ODPH_ABORT("No valid \"" CONF_STR_OUT_QS "\" found\n");

			odp_lso_profile_param_init(&l_param);
			l_param.lso_proto = ODP_LSO_PROTO_CUSTOM;
			compl.opt.lso_profile = odp_lso_profile_create(compl.pktio, &l_param);

			if (compl.opt.lso_profile == ODP_LSO_PROFILE_INVALID)
				ODPH_ABORT("Error creating LSO profile\n");
		} else {
			elem = config_setting_get_elem(param->param, 1);

			if (elem == NULL)
				ODPH_ABORT("No \"" CONF_STR_OUT_QS "\" found\n");

			num = config_setting_length(elem);

			if (num == 0)
				ODPH_ABORT("No valid \"" CONF_STR_OUT_QS "\" found\n");

			compl.qs = calloc(1U, num * sizeof(*compl.qs));

			if (compl.qs == NULL)
				ODPH_ABORT("Error allocating memory, aborting\n");

			for (int i = 0; i < num; ++i) {
				val_str = config_setting_get_string_elem(elem, i);
				compl.qs[i] = (odp_queue_t)config_parser_get(QUEUE_DOMAIN,
									     val_str);
			}

			compl.num = num;
		}
	}

	init->fn = work_dma_completion;
	init->data = (uintptr_t)&compl;
}

static void work_dma_completion_print(const char *queue, const work_stats_t *stats)
{
	printf("\n%s:\n"
	       "  work:              %s\n"
	       "  lso:               %s\n"
	       "  packets forwarded: %" PRIu64 "\n", queue, WORK_DMA_COMPLETION,
	       compl.opt.lso_profile != ODP_LSO_PROFILE_INVALID ? "enabled" : "disabled",
	       stats->data1);
}

static void work_dma_completion_destroy(uintptr_t data)
{
	work_compl_data_t *priv = (work_compl_data_t *)data;

	if (priv->dir_qs == NULL)
		return;

	if (priv->opt.lso_profile != ODP_LSO_PROFILE_INVALID)
		(void)odp_lso_profile_destroy(priv->opt.lso_profile);

	free(priv->dir_qs);
	priv->dir_qs = NULL;
}

WORK_AUTOREGISTER(WORK_DMA_COMPLETION, work_dma_completion_init, work_dma_completion_print,
		  work_dma_completion_destroy)

static void transfer(work_reassembly_data_t *data, trs_t *trs, odp_event_t src_ev[])
{
	odp_dma_transfer_param_t trs_p;
	odp_dma_compl_param_t compl_p;
	odp_dma_seg_t src[data->num_frag], *seg, dst;
	uint32_t dst_len = 0U;
	odp_dma_compl_t compl_ev;
	int ret = 0;

	odp_dma_transfer_param_init(&trs_p);
	odp_dma_compl_param_init(&compl_p);

	for (uint32_t i = 0U; i < data->num_frag; ++i) {
		seg = &src[i];
		seg->packet = odp_packet_from_event(src_ev[i]);
		seg->len = odp_packet_len(seg->packet);
		seg->offset = 0U;
		trs->src[i] = seg->packet;
		dst_len += seg->len;
	}

	dst.packet = odp_packet_alloc(data->dst_pool, dst_len);

	if (odp_unlikely(dst.packet == ODP_PACKET_INVALID))
		ODPH_ABORT("Error allocating, aborting\n");

	dst.len = dst_len;
	dst.offset = 0U;
	trs->dst = dst.packet;
	trs->inflight = data->inflight;
	trs->num = data->num_frag;
	trs_p.src_format = ODP_DMA_FORMAT_PACKET;
	trs_p.dst_format = ODP_DMA_FORMAT_PACKET;
	trs_p.num_src = data->num_frag;
	trs_p.num_dst = 1U;
	trs_p.src_seg = src;
	trs_p.dst_seg = &dst;
	compl_p.compl_mode = ODP_DMA_COMPL_EVENT;
	compl_ev = odp_dma_compl_alloc(data->compl_pool);

	if (odp_unlikely(compl_ev == ODP_DMA_COMPL_INVALID))
		ODPH_ABORT("Error allocating, aborting\n");

	compl_p.event = odp_dma_compl_to_event(compl_ev);
	compl_p.queue = data->compl_q;
	compl_p.user_ptr = trs;

	do {
		ret = odp_dma_transfer_start(data->dmas[(odp_thread_id() - 1) % data->num_dmas],
					     &trs_p, &compl_p);
	} while (ret == 0);

	if (odp_unlikely(ret < 0))
		ODPH_ABORT("Error starting DMA, aborting\n");
}

static int work_dma_segmentation(uintptr_t data, odp_event_t ev[], int num, work_stats_t *stats)
{
	work_reassembly_data_t *priv = (work_reassembly_data_t *)data;
	int i;
	odp_event_t e;
	odp_packet_t pkt;
	uint8_t *pkt_data, flow_idx;
	reassembly_context_t *ctx;
	uint32_t num_pending;
	trs_t *trs;

	for (i = 0; i < num; ++i) {
		e = ev[i];

		if (odp_event_type(e) != ODP_EVENT_PACKET)
			break;

		pkt = odp_packet_from_event(e);
		pkt_data = odp_packet_data(pkt);
		flow_idx = *(pkt_data + priv->offset);
		ctx = &priv->ctxs[flow_idx % priv->num_ctx];

		if (odp_unlikely(odp_queue_enq(ctx->q, e) < 0))
			ODPH_ABORT("Error enqueueing, aborting\n");

		++stats->data1;
		num_pending = odp_atomic_fetch_inc_u32(&ctx->num_pending);

		if (num_pending + 1U >= priv->num_frag) {
			if (odp_ticketlock_trylock(&ctx->lock) == 0)
				continue;

			if (odp_atomic_load_u32(&ctx->num_pending) < priv->num_frag) {
				odp_ticketlock_unlock(&ctx->lock);
				continue;
			}

			odp_event_t q_ev[priv->num_frag];

			if (odp_unlikely(odp_stash_get_ptr(priv->inflight, (uintptr_t *)&trs, 1)
					 != 1))
				ODPH_ABORT("Error getting from stash, aborting\n");

			if (odp_unlikely(odp_queue_deq_multi(ctx->q, q_ev, priv->num_frag)
					 != (int)priv->num_frag))
				ODPH_ABORT("Error dequeuing, aborting\n");

			odp_atomic_sub_u32(&ctx->num_pending, priv->num_frag);
			odp_ticketlock_unlock(&ctx->lock);
			transfer(priv, trs, q_ev);
		}
	}

	return i;
}

static void work_dma_segmentation_init(const work_param_t *param, work_init_t *init)
{
	int val_i, num;
	config_setting_t *elem;
	reassembly_context_t *ctx;
	const char *val_str;
	trs_t *trs;

	if (param->param == NULL)
		ODPH_ABORT("No parameters available\n");

	if (common.ctxs == NULL) {
		if (config_setting_length(param->param) != 8)
			ODPH_ABORT("No valid parameters available\n");

		val_i = config_setting_get_int_elem(param->param, 0);

		if (val_i == -1)
			ODPH_ABORT("No \"" CONF_STR_OFF "\" found\n");

		common.offset = val_i;
		elem = config_setting_get_elem(param->param, 1);

		if (elem == NULL)
			ODPH_ABORT("No \"" CONF_STR_QS "\" found\n");

		num = config_setting_length(elem);

		if (num == 0)
			ODPH_ABORT("No valid \"" CONF_STR_QS "\" found\n");

		common.ctxs = calloc(1U, num * sizeof(*common.ctxs));

		if (common.ctxs == NULL)
			ODPH_ABORT("Error allocating memory, aborting\n");

		for (int i = 0; i < num; ++i) {
			ctx = &common.ctxs[i];
			odp_atomic_init_u32(&ctx->num_pending, 0U);
			odp_ticketlock_init(&ctx->lock);
			val_str = config_setting_get_string_elem(elem, i);
			ctx->q = (odp_queue_t)config_parser_get(QUEUE_DOMAIN, val_str);
		}

		common.num_ctx = num;
		elem = config_setting_get_elem(param->param, 2);

		if (elem == NULL)
			ODPH_ABORT("No \"" CONF_STR_SESSION "\" found\n");

		num = config_setting_length(elem);

		if (num == 0)
			ODPH_ABORT("No valid \"" CONF_STR_SESSION "\" found\n");

		common.dmas = calloc(1U, num * sizeof(*common.dmas));

		if (common.dmas == NULL)
			ODPH_ABORT("Error allocating memory, aborting\n");

		for (int i = 0; i < num; ++i) {
			val_str = config_setting_get_string_elem(elem, i);
			common.dmas[i] = (odp_dma_t)config_parser_get(DMA_DOMAIN, val_str);
		}

		common.num_dmas = num;
		val_str = config_setting_get_string_elem(param->param, 3);

		if (val_str == NULL)
			ODPH_ABORT("No \"" CONF_STR_COMPL_Q "\" found\n");

		common.compl_q = (odp_queue_t)config_parser_get(QUEUE_DOMAIN, val_str);
		val_str = config_setting_get_string_elem(param->param, 4);

		if (val_str == NULL)
			ODPH_ABORT("No \"" CONF_STR_DST_POOL "\" found\n");

		common.dst_pool = (odp_pool_t)config_parser_get(POOL_DOMAIN, val_str);
		val_str = config_setting_get_string_elem(param->param, 5);

		if (val_str == NULL)
			ODPH_ABORT("No \"" CONF_STR_COMPL_POOL "\" found\n");

		common.compl_pool = (odp_pool_t)config_parser_get(POOL_DOMAIN, val_str);
		val_str = config_setting_get_string_elem(param->param, 6);

		if (val_str == NULL)
			ODPH_ABORT("No \"" CONF_STR_INFLIGHT "\" found\n");

		common.inflight = (odp_stash_t)config_parser_get(STASH_DOMAIN, val_str);
		val_i = config_setting_get_int_elem(param->param, 7);

		if (val_i == -1)
			ODPH_ABORT("No \"" CONF_STR_NUM_FRAG "\" found\n");

		common.num_frag = val_i;

		while (true) {
			trs = calloc(1U, sizeof(*trs));

			if (trs == NULL)
				ODPH_ABORT("Error allocating memory, aborting\n");

			trs->src = malloc(common.num_frag * sizeof(*trs->src));

			if (trs->src == NULL)
				ODPH_ABORT("Error allocating memory, aborting\n");

			if (odp_stash_put_ptr(common.inflight, (uintptr_t *)&trs, 1) != 1) {
				free(trs->src);
				free(trs);
				break;
			}
		}
	}

	init->fn = work_dma_segmentation;
	init->data = (uintptr_t)&common;
}

static void work_dma_segmentation_print(const char *queue, const work_stats_t *stats)
{
	printf("\n%s:\n"
	       "  work:              %s\n"
	       "  segments enqueued: %" PRIu64 "\n", queue, WORK_DMA_SEGMENTATION, stats->data1);
}

static void work_dma_segmentation_destroy(uintptr_t data)
{
	work_reassembly_data_t *priv = (work_reassembly_data_t *)data;
	odp_event_t ev;
	trs_t *trs;

	if (priv->ctxs == NULL)
		return;

	while (true) {
		if (odp_stash_get_ptr(priv->inflight, (uintptr_t *)&trs, 1) != 1)
			break;

		free(trs->src);
		free(trs);
	}

	for (uint32_t i = 0U; i < priv->num_ctx; ++i) {
		while (true) {
			ev = odp_queue_deq(priv->ctxs[i].q);

			if (ev == ODP_EVENT_INVALID)
				break;

			odp_event_free(ev);
		}
	}

	free(priv->ctxs);
	free(priv->dmas);
	priv->ctxs = NULL;
}

WORK_AUTOREGISTER(WORK_DMA_SEGMENTATION, work_dma_segmentation_init,
		  work_dma_segmentation_print, work_dma_segmentation_destroy)
