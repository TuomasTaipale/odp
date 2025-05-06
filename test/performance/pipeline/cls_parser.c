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

#define CONF_STR_COS "cos"
#define CONF_STR_PMR "pmr"
#define CONF_STR_NAME "name"
#define CONF_STR_ACTION "action"
#define CONF_STR_NUM_QS "num_queues"
#define CONF_STR_QUEUE "queue"
#define CONF_STR_TYPE "type"
#define CONF_STR_HASH_IPV4_UDP "hash_ipv4_udp"
#define CONF_STR_HASH_IPV4_TCP "hash_ipv4_tcp"
#define CONF_STR_HASH_IPV4 "hash_ipv4"
#define CONF_STR_HASH_IPV6_UDP "hash_ipv6_udp"
#define CONF_STR_HASH_IPV6_TCP "hash_ipv6_tcp"
#define CONF_STR_HASH_IPV6 "hash_ipv6"
#define CONF_STR_POOL "pool"
#define CONF_STR_DEFAULT "default"
#define CONF_STR_SRC_COS "src_cos"
#define CONF_STR_DST_COS "dst_cos"
#define CONF_STR_TERM "term"
#define CONF_STR_MATCH_VALUE "match_value"
#define CONF_STR_MATCH_MASK "match_mask"
#define CONF_STR_VAL_SZ "val_sz"
#define CONF_STR_OFFSET "offset"

#define DROP "drop"
#define ENQUEUE "enqueue"
#define PLAIN "plain"
#define SCHEDULED "schedule"
#define LEN "len"
#define ETH_0 "eth_0"
#define ETH_X "eth_x"
#define VLAN_0 "vlan_0"
#define VLAN_X "vlan_x"
#define VLAN_PCP "vlan_pcp"
#define DMAC "dmac"
#define IPPROTO "ipproto"
#define IP_DSCP "ip_dscp"
#define UDP_DPORT "udp_dport"
#define TCP_DPORT "tcp_dport"
#define UDP_SPORT "udp_sport"
#define TCP_SPORT "tcp_sport"
#define SIP_ADDR "sip_addr"
#define DIP_ADDR "dip_addr"
#define SIP6_ADDR "sip6_addr"
#define DIP6_ADDR "dip6_addr"
#define IPSEC_SPI "ipsec_spi"
#define LD_VNI "ld_vni"
#define CUSTOM_FRAME "custom_frame"
#define CUSTOM_L3 "custom_l3"
#define SCTP_SPORT "sctp_sport"
#define SCTP_DPORT "sctp_dport"

typedef struct {
	char *name;
	char *queue;
	char *pool;
	char *def;
	odp_cls_cos_param_t cos_param;
	odp_queue_param_t q_param;
	odp_cos_t cos;
	odp_pktio_t def_pktio;
} cos_parse_t;

typedef struct {
	char *name;
	char *src;
	char *dst;
	uint8_t *val_arr;
	uint8_t *mask_arr;
	odp_pmr_param_t param;
	odp_pmr_t pmr;
} pmr_parse_t;

typedef struct {
	cos_parse_t *coss;
	pmr_parse_t *pmrs;
	uint32_t num_cos;
	uint32_t num_pmr;
} cls_parses_t;

static cls_parses_t cls;

static odp_bool_t parse_cos_entry(config_setting_t *cs, cos_parse_t *cos)
{
	const char *val_str;
	int val_i;

	cos->cos = ODP_COS_INVALID;
	cos->def_pktio = ODP_PKTIO_INVALID;
	odp_cls_cos_param_init(&cos->cos_param);
	odp_queue_param_init(&cos->q_param);

	if (config_setting_lookup_string(cs, CONF_STR_NAME, &val_str) == CONFIG_FALSE) {
		ODPH_ERR("No \"" CONF_STR_NAME "\" found\n");
		return false;
	}

	cos->name = strdup(val_str);

	if (cos->name == NULL)
		ODPH_ABORT("Error allocating memory, aborting\n");

	if (config_setting_lookup_string(cs, CONF_STR_ACTION, &val_str) == CONFIG_TRUE) {
		if (strcmp(val_str, DROP) == 0) {
			cos->cos_param.action = ODP_COS_ACTION_DROP;
		} else if (strcmp(val_str, ENQUEUE) == 0) {
			cos->cos_param.action = ODP_COS_ACTION_ENQUEUE;
		} else {
			ODPH_ERR("No valid \"" CONF_STR_ACTION "\" found\n");
			return false;
		}
	}

	if (config_setting_lookup_int(cs, CONF_STR_NUM_QS, &val_i) == CONFIG_TRUE)
		cos->cos_param.num_queue = val_i;

	if (cos->cos_param.num_queue == 1U) {
		if (config_setting_lookup_string(cs, CONF_STR_QUEUE, &val_str) == CONFIG_FALSE) {
			ODPH_ERR("No \"" CONF_STR_QUEUE "\" found\n");
			return false;
		}

		cos->queue = strdup(val_str);

		if (cos->queue == NULL)
			ODPH_ABORT("Error allocating memory, aborting\n");
	} else {
		if (config_setting_lookup_string(cs, CONF_STR_TYPE, &val_str) == CONFIG_TRUE) {
			if (strcmp(val_str, PLAIN) == 0) {
				cos->q_param.type = ODP_QUEUE_TYPE_PLAIN;
			} else if (strcmp(val_str, SCHEDULED) == 0) {
				cos->q_param.type = ODP_QUEUE_TYPE_SCHED;
			} else {
				ODPH_ERR("No valid \"" CONF_STR_TYPE "\" found\n");
				return false;
			}
		}

		if (config_setting_lookup_int(cs, CONF_STR_HASH_IPV4_UDP, &val_i) == CONFIG_TRUE)
			cos->cos_param.hash_proto.proto.ipv4_udp = val_i;

		if (config_setting_lookup_int(cs, CONF_STR_HASH_IPV4_TCP, &val_i) == CONFIG_TRUE)
			cos->cos_param.hash_proto.proto.ipv4_tcp = val_i;

		if (config_setting_lookup_int(cs, CONF_STR_HASH_IPV4, &val_i) == CONFIG_TRUE)
			cos->cos_param.hash_proto.proto.ipv4 = val_i;

		if (config_setting_lookup_int(cs, CONF_STR_HASH_IPV6_UDP, &val_i) == CONFIG_TRUE)
			cos->cos_param.hash_proto.proto.ipv6_udp = val_i;

		if (config_setting_lookup_int(cs, CONF_STR_HASH_IPV6_TCP, &val_i) == CONFIG_TRUE)
			cos->cos_param.hash_proto.proto.ipv6_tcp = val_i;

		if (config_setting_lookup_int(cs, CONF_STR_HASH_IPV6, &val_i) == CONFIG_TRUE)
			cos->cos_param.hash_proto.proto.ipv6 = val_i;
	}

	if (config_setting_lookup_string(cs, CONF_STR_POOL, &val_str) == CONFIG_TRUE) {
		cos->pool = strdup(val_str);

		if (cos->pool == NULL)
			ODPH_ABORT("Error allocating memory, aborting\n");
	}

	if (config_setting_lookup_string(cs, CONF_STR_DEFAULT, &val_str) == CONFIG_TRUE) {
		cos->def = strdup(val_str);

		if (cos->def == NULL)
			ODPH_ABORT("Error allocating memory, aborting\n");
	}

	return true;
}

static void free_cos_entry(cos_parse_t *cos)
{
	free(cos->name);
	free(cos->queue);
	free(cos->pool);
	free(cos->def);

	if (cos->def_pktio != ODP_PKTIO_INVALID)
		(void)odp_pktio_default_cos_set(cos->def_pktio, ODP_COS_INVALID);

	if (cos->cos != ODP_COS_INVALID)
		(void)odp_cos_destroy(cos->cos);
}

static odp_bool_t parse_pmr_entry(config_setting_t *cs, pmr_parse_t *pmr)
{
	const char *val_str;
	int val_i, num1, num2;
	config_setting_t *elem1, *elem2;
	uint8_t *val_arr, *mask_arr;

	pmr->pmr = ODP_PMR_INVALID;
	odp_cls_pmr_param_init(&pmr->param);

	if (config_setting_lookup_string(cs, CONF_STR_NAME, &val_str) == CONFIG_FALSE) {
		ODPH_ERR("No \"" CONF_STR_NAME "\" found\n");
		return false;
	}

	pmr->name = strdup(val_str);

	if (pmr->name == NULL)
		ODPH_ABORT("Error allocating memory, aborting\n");

	if (config_setting_lookup_string(cs, CONF_STR_SRC_COS, &val_str) == CONFIG_FALSE) {
		ODPH_ERR("No \"" CONF_STR_SRC_COS "\" found\n");
		return false;
	}

	pmr->src = strdup(val_str);

	if (pmr->src == NULL)
		ODPH_ABORT("Error allocating memory, aborting\n");

	if (config_setting_lookup_string(cs, CONF_STR_DST_COS, &val_str) == CONFIG_FALSE) {
		ODPH_ERR("No \"" CONF_STR_DST_COS "\" found\n");
		return false;
	}

	pmr->dst = strdup(val_str);

	if (pmr->dst == NULL)
		ODPH_ABORT("Error allocating memory, aborting\n");

	if (config_setting_lookup_string(cs, CONF_STR_TERM, &val_str) == CONFIG_FALSE) {
		ODPH_ERR("No \"" CONF_STR_TERM "\" found\n");
		return false;
	}

	if (strcmp(val_str, LEN) == 0) {
		pmr->param.term = ODP_PMR_LEN;
	} else if (strcmp(val_str, ETH_0) == 0) {
		pmr->param.term = ODP_PMR_ETHTYPE_0;
	} else if (strcmp(val_str, ETH_X) == 0) {
		pmr->param.term = ODP_PMR_ETHTYPE_X;
	} else if (strcmp(val_str, VLAN_0) == 0) {
		pmr->param.term = ODP_PMR_VLAN_ID_0;
	} else if (strcmp(val_str, VLAN_X) == 0) {
		pmr->param.term = ODP_PMR_VLAN_ID_X;
	} else if (strcmp(val_str, VLAN_PCP) == 0) {
		pmr->param.term = ODP_PMR_VLAN_PCP_0;
	} else if (strcmp(val_str, DMAC) == 0) {
		pmr->param.term = ODP_PMR_DMAC;
	} else if (strcmp(val_str, IPPROTO) == 0) {
		pmr->param.term = ODP_PMR_IPPROTO;
	} else if (strcmp(val_str, IP_DSCP) == 0) {
		pmr->param.term = ODP_PMR_IP_DSCP;
	} else if (strcmp(val_str, UDP_DPORT) == 0) {
		pmr->param.term = ODP_PMR_UDP_DPORT;
	} else if (strcmp(val_str, TCP_DPORT) == 0) {
		pmr->param.term = ODP_PMR_TCP_DPORT;
	} else if (strcmp(val_str, UDP_SPORT) == 0) {
		pmr->param.term = ODP_PMR_UDP_SPORT;
	} else if (strcmp(val_str, TCP_SPORT) == 0) {
		pmr->param.term = ODP_PMR_TCP_SPORT;
	} else if (strcmp(val_str, SIP_ADDR) == 0) {
		pmr->param.term = ODP_PMR_SIP_ADDR;
	} else if (strcmp(val_str, DIP_ADDR) == 0) {
		pmr->param.term = ODP_PMR_DIP_ADDR;
	} else if (strcmp(val_str, SIP6_ADDR) == 0) {
		pmr->param.term = ODP_PMR_SIP6_ADDR;
	} else if (strcmp(val_str, DIP6_ADDR) == 0) {
		pmr->param.term = ODP_PMR_DIP6_ADDR;
	} else if (strcmp(val_str, IPSEC_SPI) == 0) {
		pmr->param.term = ODP_PMR_IPSEC_SPI;
	} else if (strcmp(val_str, LD_VNI) == 0) {
		pmr->param.term = ODP_PMR_LD_VNI;
	} else if (strcmp(val_str, CUSTOM_FRAME) == 0) {
		pmr->param.term = ODP_PMR_CUSTOM_FRAME;
	} else if (strcmp(val_str, CUSTOM_L3) == 0) {
		pmr->param.term = ODP_PMR_CUSTOM_L3;
	} else if (strcmp(val_str, SCTP_SPORT) == 0) {
		pmr->param.term = ODP_PMR_SCTP_SPORT;
	} else if (strcmp(val_str, SCTP_DPORT) == 0) {
		pmr->param.term = ODP_PMR_SCTP_DPORT;
	} else {
		ODPH_ERR("No valid \"" CONF_STR_TERM "\" found\n");
		return false;
	}

	elem1 = config_setting_lookup(cs, CONF_STR_MATCH_VALUE);

	if (elem1 == NULL) {
		ODPH_ERR("No \"" CONF_STR_MATCH_VALUE "\" entries found\n");
		return false;
	}

	num1 = config_setting_length(cs);

	if (num1 == 0) {
		ODPH_ERR("No valid \"" CONF_STR_MATCH_VALUE "\" entries found\n");
		return false;
	}

	elem2 = config_setting_lookup(cs, CONF_STR_MATCH_MASK);

	if (elem2 == NULL) {
		ODPH_ERR("No \"" CONF_STR_MATCH_MASK "\" entries found\n");
		return false;
	}

	num2 = config_setting_length(cs);

	if (num2 == 0) {
		ODPH_ERR("No valid \"" CONF_STR_MATCH_MASK "\" entries found\n");
		return false;
	}

	val_arr = calloc(1U, num1 * sizeof(*val_arr));

	if (val_arr == NULL)
		ODPH_ABORT("Error allocating memory, aborting\n");

	pmr->val_arr = val_arr;
	mask_arr = calloc(1U, num2 * sizeof(*mask_arr));

	if (mask_arr == NULL)
		ODPH_ABORT("Error allocating memory, aborting\n");

	pmr->mask_arr = mask_arr;

	for (int i = 0; i < num1; ++i)
		val_arr[i] = (uint8_t)config_setting_get_int_elem(elem1, i);

	for (int i = 0; i < num2; ++i)
		mask_arr[i] = (uint8_t)config_setting_get_int_elem(elem2, i);

	pmr->param.match.value = val_arr;
	pmr->param.match.mask = mask_arr;

	if (config_setting_lookup_int(cs, CONF_STR_VAL_SZ, &val_i) == CONFIG_FALSE) {
		ODPH_ERR("No \"" CONF_STR_VAL_SZ "\" found\n");
		return false;
	}

	pmr->param.val_sz = val_i;

	if (pmr->param.term == ODP_PMR_CUSTOM_FRAME || pmr->param.term == ODP_PMR_CUSTOM_L3) {
		if (config_setting_lookup_int(cs, CONF_STR_OFFSET, &val_i) == CONFIG_FALSE) {
			ODPH_ERR("No \"" CONF_STR_OFFSET "\" found\n");
			return false;
		}
	}

	pmr->param.offset = val_i;

	return true;
}

static void free_pmr_entry(pmr_parse_t *pmr)
{
	free(pmr->name);
	free(pmr->src);
	free(pmr->dst);
	free(pmr->val_arr);
	free(pmr->mask_arr);

	if (pmr->pmr != ODP_PMR_INVALID)
		(void)odp_cls_pmr_destroy(pmr->pmr);
}

static odp_bool_t classifier_parser_init(config_t *config)
{
	config_setting_t *cs, *elem1, *elem2, *tmp;
	int num1, num2;
	cos_parse_t *cos;
	pmr_parse_t *pmr;

	cs = config_lookup(config, CLASSIFICATION_DOMAIN);

	if (cs == NULL)	{
		printf("Nothing to parse for \"" CLASSIFICATION_DOMAIN "\" domain\n");
		return true;
	}

	elem1 = config_setting_lookup(cs, CONF_STR_COS);

	if (elem1 == NULL) {
		ODPH_ERR("No \"" CONF_STR_COS "\" entries found\n");
		return false;
	}

	num1 = config_setting_length(elem1);

	if (num1 == 0) {
		ODPH_ERR("No valid \"" CONF_STR_COS "\" entries found\n");
		return false;
	}

	elem2 = config_setting_lookup(cs, CONF_STR_PMR);

	if (elem2 == NULL) {
		ODPH_ERR("No \"" CONF_STR_PMR "\" entries found\n");
		return false;
	}

	num2 = config_setting_length(elem2);

	if (num2 == 0) {
		ODPH_ERR("No valid \"" CONF_STR_PMR "\" entries found\n");
		return false;
	}

	cls.coss = calloc(1U, num1 * sizeof(*cls.coss));

	if (cls.coss == NULL)
		ODPH_ABORT("Error allocating memory, aborting\n");

	for (int i = 0; i < num1; ++i) {
		tmp = config_setting_get_elem(elem1, i);

		if (tmp == NULL) {
			ODPH_ERR("Unparsable \"" CONF_STR_COS "\" entry (%d)\n", i);
			return false;
		}

		cos = &cls.coss[cls.num_cos];

		if (!parse_cos_entry(tmp, cos)) {
			ODPH_ERR("Invalid \"" CONF_STR_COS "\" entry (%d)\n", i);
			free_cos_entry(cos);
			return false;
		}

		++cls.num_cos;
	}

	cls.pmrs = calloc(1U, num2 * sizeof(*cls.pmrs));

	if (cls.pmrs == NULL)
		ODPH_ABORT("Error allocating memory, aborting\n");

	for (int i = 0; i < num2; ++i) {
		tmp = config_setting_get_elem(elem2, i);

		if (tmp == NULL) {
			ODPH_ERR("Unparsable \"" CONF_STR_PMR "\" entry (%d)\n", i);
			return false;
		}

		pmr = &cls.pmrs[cls.num_pmr];

		if (!parse_pmr_entry(tmp, pmr)) {
			ODPH_ERR("Invalid \"" CONF_STR_PMR "\" entry (%d)\n", i);
			free_pmr_entry(pmr);
			return false;
		}

		++cls.num_pmr;
	}

	return true;
}

static odp_bool_t classifier_parser_deploy(void)
{
	cos_parse_t *cos;
	odp_pktio_t pktio;
	pmr_parse_t *pmr;
	odp_cos_t src, dst;

	printf("\n*** " CLASSIFICATION_DOMAIN " resources ***\n");

	for (uint32_t i = 0U; i < cls.num_cos; ++i) {
		cos = &cls.coss[i];

		if (cos->queue != NULL)
			cos->cos_param.queue = (odp_queue_t)config_parser_get(QUEUE_DOMAIN,
									      cos->queue);

		if (cos->pool != NULL)
			cos->cos_param.pool = (odp_pool_t)config_parser_get(POOL_DOMAIN,
									    cos->pool);

		cos->cos = odp_cls_cos_create(cos->name, &cos->cos_param);

		if (cos->cos == ODP_COS_INVALID) {
			ODPH_ERR("Error creating CoS (%s)\n", cos->name);
			return false;
		}

		if (cos->def != NULL) {
			pktio = (odp_pktio_t)config_parser_get(PKTIO_DOMAIN, cos->def);

			if (odp_pktio_default_cos_set(pktio, cos->cos) < 0) {
				ODPH_ERR("Error setting default CoS (%s)\n", cos->name);
				return false;
			}

			cos->def_pktio = pktio;
		}

		printf("\nname: %s\n", cos->name);
	}

	for (uint32_t i = 0U; i < cls.num_pmr; ++i) {
		pmr = &cls.pmrs[i];
		src = (odp_cos_t)config_parser_get(CLASSIFICATION_DOMAIN, pmr->src);
		dst = (odp_cos_t)config_parser_get(CLASSIFICATION_DOMAIN, pmr->dst);
		pmr->pmr = odp_cls_pmr_create(&pmr->param, 1, src, dst);

		if (pmr->pmr == ODP_PMR_INVALID) {
			ODPH_ERR("Error creating PMR (%s)\n", pmr->name);
			return false;
		}

		printf("\nname: %s\n", pmr->name);
	}

	odp_cls_print_all();

	return true;
}

static void classifier_parser_destroy(void)
{
	for (uint32_t i = 0U; i < cls.num_pmr; ++i)
		free_pmr_entry(&cls.pmrs[i]);

	for (uint32_t i = 0U; i < cls.num_cos; ++i)
		free_cos_entry(&cls.coss[i]);

	free(cls.coss);
}

static uintptr_t classifier_parser_get_resource(const char *resource)
{
	cos_parse_t *parse;
	odp_cos_t cos = ODP_COS_INVALID;

	for (uint32_t i = 0U; i < cls.num_cos; ++i) {
		parse = &cls.coss[i];

		if (strcmp(parse->name, resource) != 0)
			continue;

		cos = parse->cos;
		break;
	}

	if (cos == ODP_COS_INVALID)
		ODPH_ABORT("No resource found (%s), aborting\n", resource);

	return (uintptr_t)cos;
}

CONFIG_PARSER_AUTOREGISTER(LOW_PRIO, CLASSIFICATION_DOMAIN, classifier_parser_init,
			   classifier_parser_deploy, classifier_parser_destroy,
			   classifier_parser_get_resource)
