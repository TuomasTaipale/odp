#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <libconfig.h>
#include <odp_api.h>
#include <odp/helper/odph_api.h>

#include "common.h"
#include "config_parser.h"

#define CONF_STR_NAME "name"
#define CONF_STR_IFACE "iface"
#define CONF_STR_POOL "pool"
#define CONF_STR_INMODE "inmode"
#define CONF_STR_OUTMODE "outmode"
#define CONF_STR_NUM_IN_QS "num_in_queues"
#define CONF_STR_NUM_OUT_QS "num_out_queues"
#define CONF_STR_HASH_ENABLE "hash_enable"
#define CONF_STR_HASH_IPV4_UDP "hash_ipv4_udp"
#define CONF_STR_HASH_IPV4_TCP "hash_ipv4_tcp"
#define CONF_STR_HASH_IPV4 "hash_ipv4"
#define CONF_STR_HASH_IPV6_UDP "hash_ipv6_udp"
#define CONF_STR_HASH_IPV6_TCP "hash_ipv6_tcp"
#define CONF_STR_HASH_IPV6 "hash_ipv6"
#define CONF_STR_PROMISC "promisc"

#define QUEUED "queue"
#define SCHEDULED "schedule"

#define IDX_DELIM_CHAR '.'
#define IN_QS_STR ".in"
#define OUT_QS_STR ".out"

typedef struct {
	char *name;
	char *iface;
	char *pool;
	odp_pktio_param_t param;
	odp_pktin_queue_param_t in_param;
	odp_pktout_queue_param_t out_param;
	int promisc_mode;
	odp_pktio_t pktio;
	odp_queue_t in_queues[ODP_PKTIN_MAX_QUEUES];
	odp_queue_t out_queues[ODP_PKTOUT_MAX_QUEUES];
	odp_bool_t is_started;
} pktio_parse_t;

typedef struct {
	pktio_parse_t *pktios;
	uint32_t num;
} pktio_parses_t;

static pktio_parses_t pktios;

static odp_bool_t parse_pktio_entry(config_setting_t *cs, pktio_parse_t *pktio)
{
	const char *val_str;
	int val_i;

	pktio->pktio = ODP_PKTIO_INVALID;
	odp_pktio_param_init(&pktio->param);
	/* No support for direct or TM modes for now, so our default is queued in/out */
	pktio->param.in_mode = ODP_PKTIN_MODE_QUEUE;
	pktio->param.out_mode = ODP_PKTOUT_MODE_QUEUE;
	odp_pktin_queue_param_init(&pktio->in_param);
	odp_pktout_queue_param_init(&pktio->out_param);

	if (config_setting_lookup_string(cs, CONF_STR_NAME, &val_str) == CONFIG_FALSE) {
		ODPH_ERR("No \"" CONF_STR_NAME "\" found\n");
		return false;
	}

	pktio->name = strdup(val_str);

	if (pktio->name == NULL)
		ODPH_ABORT("Error allocating memory, aborting\n");

	if (config_setting_lookup_string(cs, CONF_STR_IFACE, &val_str) == CONFIG_FALSE) {
		ODPH_ERR("No \"" CONF_STR_IFACE "\" found\n");
		return false;
	}

	pktio->iface = strdup(val_str);

	if (pktio->iface == NULL)
		ODPH_ABORT("Error allocating memory, aborting\n");

	if (config_setting_lookup_string(cs, CONF_STR_POOL, &val_str) == CONFIG_FALSE) {
		ODPH_ERR("No \"" CONF_STR_POOL "\" found\n");
		return false;
	}

	pktio->pool = strdup(val_str);

	if (pktio->pool == NULL)
		ODPH_ABORT("Error allocating memory, aborting\n");

	if (config_setting_lookup_string(cs, CONF_STR_INMODE, &val_str) == CONFIG_TRUE) {
		if (strcmp(val_str, SCHEDULED) == 0) {
			pktio->param.in_mode = ODP_PKTIN_MODE_SCHED;
		} else {
			ODPH_ERR("No valid \"" CONF_STR_INMODE "\" found\n");
			return false;
		}

		if (pktio->param.in_mode != ODP_PKTIN_MODE_QUEUE &&
		    pktio->param.in_mode != ODP_PKTIN_MODE_SCHED) {
			ODPH_ERR("Unsupported \"" CONF_STR_INMODE "\"\n");
			return false;
		}
	}

	if (config_setting_lookup_string(cs, CONF_STR_OUTMODE, &val_str) == CONFIG_TRUE) {
		if (strcmp(val_str, QUEUED) == 0) {
			pktio->param.out_mode = ODP_PKTOUT_MODE_QUEUE;
		} else {
			ODPH_ERR("No valid \"" CONF_STR_OUTMODE "\" found\n");
			return false;
		}

		if (pktio->param.out_mode != ODP_PKTOUT_MODE_QUEUE) {
			ODPH_ERR("Unsupported \"" CONF_STR_OUTMODE "\"\n");
			return false;
		}
	}

	if (config_setting_lookup_int(cs, CONF_STR_NUM_IN_QS, &val_i) == CONFIG_TRUE)
		pktio->in_param.num_queues = val_i;

	if (config_setting_lookup_int(cs, CONF_STR_NUM_OUT_QS, &val_i) == CONFIG_TRUE)
		pktio->out_param.num_queues = val_i;

	if (config_setting_lookup_int(cs, CONF_STR_HASH_ENABLE, &val_i) == CONFIG_TRUE)
		pktio->in_param.hash_enable = val_i;

	if (config_setting_lookup_int(cs, CONF_STR_HASH_IPV4_UDP, &val_i) == CONFIG_TRUE)
		pktio->in_param.hash_proto.proto.ipv4_udp = val_i;

	if (config_setting_lookup_int(cs, CONF_STR_HASH_IPV4_TCP, &val_i) == CONFIG_TRUE)
		pktio->in_param.hash_proto.proto.ipv4_tcp = val_i;

	if (config_setting_lookup_int(cs, CONF_STR_HASH_IPV4, &val_i) == CONFIG_TRUE)
		pktio->in_param.hash_proto.proto.ipv4 = val_i;

	if (config_setting_lookup_int(cs, CONF_STR_HASH_IPV6_UDP, &val_i) == CONFIG_TRUE)
		pktio->in_param.hash_proto.proto.ipv6_udp = val_i;

	if (config_setting_lookup_int(cs, CONF_STR_HASH_IPV6_TCP, &val_i) == CONFIG_TRUE)
		pktio->in_param.hash_proto.proto.ipv6_tcp = val_i;

	if (config_setting_lookup_int(cs, CONF_STR_HASH_IPV6, &val_i) == CONFIG_TRUE)
		pktio->in_param.hash_proto.proto.ipv6 = val_i;

	if (config_setting_lookup_int(cs, CONF_STR_PROMISC, &val_i) == CONFIG_TRUE)
		pktio->promisc_mode = val_i;

	return true;
}

static void free_pktio_entry(pktio_parse_t *pktio)
{
	if (pktio->pktio != ODP_PKTIO_INVALID) {
		if (pktio->is_started) {
			(void)odp_pktio_stop(pktio->pktio);
			pktio->is_started = false;
		}

		(void)odp_pktio_close(pktio->pktio);
	}

	free(pktio->name);
	free(pktio->iface);
	free(pktio->pool);
}

static odp_bool_t pktio_parser_init(config_t *config)
{
	config_setting_t *cs, *elem;
	int num;
	pktio_parse_t *pktio;

	cs = config_lookup(config, PKTIO_DOMAIN);

	if (cs == NULL)	{
		printf("Nothing to parse for \"" PKTIO_DOMAIN "\" domain\n");
		return true;
	}

	num = config_setting_length(cs);

	if (num == 0) {
		ODPH_ERR("No valid \"" PKTIO_DOMAIN "\" entries found\n");
		return false;
	}

	pktios.pktios = calloc(1U, num * sizeof(*pktios.pktios));

	if (pktios.pktios == NULL)
		ODPH_ABORT("Error allocating memory, aborting\n");

	for (int i = 0; i < num; ++i) {
		elem = config_setting_get_elem(cs, i);

		if (elem == NULL) {
			ODPH_ERR("Unparsable \"" PKTIO_DOMAIN "\" entry (%d)\n", i);
			return false;
		}

		pktio = &pktios.pktios[pktios.num];

		if (!parse_pktio_entry(elem, pktio)) {
			ODPH_ERR("Invalid \"" PKTIO_DOMAIN "\" entry (%d)\n", i);
			free_pktio_entry(pktio);
			return false;
		}

		++pktios.num;
	}

	return true;
}

static odp_bool_t pktio_parser_deploy(void)
{
	pktio_parse_t *pktio;

	printf("\n*** " PKTIO_DOMAIN " resources ***\n");

	for (uint32_t i = 0U; i < pktios.num; ++i) {
		pktio = &pktios.pktios[i];
		pktio->pktio = odp_pktio_open(pktio->iface,
					      (odp_pool_t)config_parser_get_resource(POOL_DOMAIN,
										     pktio->pool),
					      &pktio->param);

		if (pktio->pktio == ODP_PKTIO_INVALID) {
			ODPH_ERR("Error opening packet I/O (%s)\n", pktio->name);
			return false;
		}

		if (odp_pktin_queue_config(pktio->pktio, &pktio->in_param) < 0) {
			ODPH_ERR("Error configuring packet I/O input queues (%s)\n", pktio->name);
			return false;
		}

		if (odp_pktout_queue_config(pktio->pktio, &pktio->out_param) < 0) {
			ODPH_ERR("Error configuring packet I/O output queues (%s)\n", pktio->name);
			return false;
		}

		if (odp_pktin_event_queue(pktio->pktio, pktio->in_queues,
					  pktio->in_param.num_queues)
		    != (int)pktio->in_param.num_queues) {
			ODPH_ERR("Error querying packet I/O input event queues (%s)\n",
				 pktio->name);
			return false;
		}

		if (odp_pktout_event_queue(pktio->pktio, pktio->out_queues,
					   pktio->out_param.num_queues)
		    != (int)pktio->out_param.num_queues) {
			ODPH_ERR("Error querying packet I/O output event queues (%s)\n",
				 pktio->name);
			return false;
		}

		if (odp_pktio_promisc_mode_set(pktio->pktio, pktio->promisc_mode) < 0) {
			ODPH_ERR("Error setting promiscuous mode (%s)\n", pktio->name);
			return false;
		}

		if (odp_pktio_start(pktio->pktio) < 0) {
			ODPH_ERR("Error starting packet I/O (%s)\n", pktio->name);
			return false;
		}

		pktio->is_started = true;
		printf("\nname: %s\n"
		       "info:\n", pktio->name);
		odp_pktio_print(pktio->pktio);
	}

	return true;
}

static void pktio_parser_destroy(void)
{
	for (uint32_t i = 0U; i < pktios.num; ++i)
		free_pktio_entry(&pktios.pktios[i]);

	free(pktios.pktios);
}

static uintptr_t pktio_parser_get_resource(const char *resource)
{
	pktio_parse_t *parse;
	char *tmp1, *tmp2;
	uint32_t idx;

	for (uint32_t i = 0U; i < pktios.num; ++i) {
		parse = &pktios.pktios[i];

		if (strcmp(parse->name, resource) == 0)
			return (uintptr_t)parse->pktio;

		tmp1 = strchr(resource, IDX_DELIM_CHAR);

		if (tmp1 == NULL)
			continue;

		if (strncmp(parse->name, resource, tmp1 - resource) != 0)
			continue;

		tmp2 = strchr(tmp1 + 1, IDX_DELIM_CHAR);

		if (tmp2 == NULL)
			continue;

		if (strncmp(IN_QS_STR, tmp1, tmp2 - tmp1) == 0) {
			idx = atoi(tmp2 + 1);

			if (idx >= parse->in_param.num_queues)
				ODPH_ABORT("Invalid packet I/O input queue index (%s: %d)",
					   parse->name, idx);

			return (uintptr_t)parse->in_queues[idx];
		} else if (strncmp(OUT_QS_STR, tmp1, tmp2 - tmp1) == 0) {
			idx = atoi(tmp2 + 1);

			if (idx >= parse->out_param.num_queues)
				ODPH_ABORT("Invalid packet I/O output queue index (%s: %d)",
					   parse->name, idx);

			return (uintptr_t)parse->out_queues[idx];
		}
	}

	ODPH_ABORT("No resource found (%s), aborting\n", resource);
}

CONFIG_PARSER_AUTOREGISTER(MED_PRIO, PKTIO_DOMAIN, pktio_parser_init, pktio_parser_deploy,
			   pktio_parser_destroy, pktio_parser_get_resource)
