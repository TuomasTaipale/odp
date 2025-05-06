#include <odp_api.h>

#include "common.h"
#include "work.h"

#define WORK_COPY "copy"

static int work_copy(uintptr_t data ODP_UNUSED, odp_event_t *ev, int num, work_stats_t *stats)
{
	odp_packet_t pkt, pkt_cp;

	if (odp_event_type(ev[0]) == ODP_EVENT_PACKET) {
		for (int i = 0U; i < num; ++i) {
			pkt = odp_packet_from_event(ev[i]);
			pkt_cp = odp_packet_copy(pkt, odp_packet_pool(pkt));

			if (pkt_cp == ODP_PACKET_INVALID)
				continue;

			odp_packet_free(pkt);
			ev[i] = odp_packet_to_event(pkt_cp);
			++stats->data1;
		}
	}

	return 0;
}

static void work_copy_init(const work_param_t *param ODP_UNUSED, work_init_t *init)
{
	init->fn = work_copy;
}

static void work_copy_print(const char *queue, const work_stats_t *stats)
{
	printf("\n%s:\n"
	       "  work:           %s\n"
	       "  packets copied: %" PRIu64 "\n", queue, WORK_COPY, stats->data1);
}

static void work_copy_destroy(uintptr_t data ODP_UNUSED)
{
}

WORK_AUTOREGISTER(WORK_COPY, work_copy_init, work_copy_print, work_copy_destroy)
