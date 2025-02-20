/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2018 Linaro Limited
 * Copyright (c) 2024 Nokia
 */

/**
 * @example odp_packet_dump.c
 *
 * Packet dump example application which prints received packets to terminal
 *
 * @cond _ODP_HIDE_FROM_DOXYGEN_
 */

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <getopt.h>

#include <odp_api.h>
#include <odp/helper/odph_api.h>

#define MAX_PKTIOS        32
#define MAX_PKTIO_NAME    255
#define MAX_PKT_NUM       1024
#define MAX_FILTERS       32

typedef struct test_options_t {
	uint64_t num_packet;
	uint32_t data_offset;
	uint32_t data_len;
	int verbose;
	int num_pktio;
	char pktio_name[MAX_PKTIOS][MAX_PKTIO_NAME + 1];
	int num_filter_l3;
	int filter_l3[MAX_FILTERS];
	int num_filter_l4;
	int filter_l4[MAX_FILTERS];

} test_options_t;

typedef struct test_global_t {
	test_options_t opt;
	odp_pool_t pool;
	odp_atomic_u32_t stop;

	struct {
		odp_pktio_t pktio;
		int started;

	} pktio[MAX_PKTIOS];

} test_global_t;

static test_global_t test_global;

static void sig_handler(int signo)
{
	(void)signo;

	odp_atomic_store_u32(&test_global.stop, 1);
}

static void print_usage(void)
{
	printf("\n"
	       "Print received packets\n"
	       "\n"
	       "OPTIONS:\n"
	       "  -i, --interface <name>      Packet IO interfaces (comma-separated, no spaces)\n"
	       "  -n, --num_packet <number>   Exit after this many packets. Use 0 to run infinitely. Default 0.\n"
	       "  -o, --data_offset <number>  Data print start offset in bytes. Default 0.\n"
	       "  -l, --data_length <number>  Data print length in bytes. Default 0.\n"
	       "  --filter_l3 <type>          Print only packets with matching L3 type. Comma-separated\n"
	       "                              list (no spaces) of ODP L3 type values (e.g. value of ODP_PROTO_L3_TYPE_IPV4).\n"
	       "  --filter_l4 <type>          Print only packets with matching L4 type. Comma-separated\n"
	       "                              list (no spaces) of ODP L4 type values (e.g. value of ODP_PROTO_L4_TYPE_TCP).\n"
	       "  -v, --verbose               Print extra packet information.\n"
	       "  -h, --help                  Display help and exit.\n\n");
}

static int parse_int_list(char *str, int integer[], int max_num)
{
	int str_len, len;
	int i = 0;

	str_len = strlen(str);

	while (str_len > 0) {
		len = strcspn(str, ",");
		str[len] = 0;

		if (i == max_num) {
			ODPH_ERR("Maximum number of options is %i\n", max_num);
			return -1;
		}

		integer[i] = atoi(str);

		str_len -= len + 1;
		str     += len + 1;
		i++;
	}

	return i;
}

static int parse_options(int argc, char *argv[], test_global_t *global)
{
	int i, opt;
	char *name, *str;
	int len, str_len, num;

	const struct option longopts[] = {
		{"interface",   required_argument, NULL, 'i'},
		{"num_packet",  required_argument, NULL, 'n'},
		{"data_offset", required_argument, NULL, 'o'},
		{"data_length", required_argument, NULL, 'l'},
		{"verbose",     no_argument,       NULL, 'v'},
		{"help",        no_argument,       NULL, 'h'},
		{"filter_l3",   required_argument, NULL,  0 },
		{"filter_l4",   required_argument, NULL,  1 },
		{NULL, 0, NULL, 0}
	};
	const char *shortopts =  "+i:n:o:l:vh";
	int ret = 0;

	while (1) {
		opt = getopt_long(argc, argv, shortopts, longopts, NULL);

		if (opt == -1)
			break;	/* No more options */

		switch (opt) {
		case 0:
			/* --filter_l3 */
			num = parse_int_list(optarg, global->opt.filter_l3,
					     MAX_FILTERS);
			global->opt.num_filter_l3 = num;

			if (num < 0)
				ret = -1;
			break;
		case 1:
			/* --filter_l4 */
			num = parse_int_list(optarg, global->opt.filter_l4,
					     MAX_FILTERS);
			global->opt.num_filter_l4 = num;

			if (num < 0)
				ret = -1;
			break;
		case 'i':
			i = 0;
			str = optarg;
			str_len = strlen(str);

			while (str_len > 0) {
				len = strcspn(str, ",");
				str_len -= len + 1;

				if (i == MAX_PKTIOS) {
					ODPH_ERR("Too many interfaces\n");
					ret = -1;
					break;
				}

				if (len > MAX_PKTIO_NAME) {
					ODPH_ERR("Too long interface name %s\n", str);
					ret = -1;
					break;
				}

				name = global->opt.pktio_name[i];
				memcpy(name, str, len);
				str += len + 1;
				i++;
			}

			global->opt.num_pktio = i;

			break;
		case 'o':
			global->opt.data_offset = atoi(optarg);
			break;
		case 'l':
			global->opt.data_len = atoi(optarg);
			break;
		case 'n':
			global->opt.num_packet = atoll(optarg);
			break;
		case 'v':
			global->opt.verbose = 1;
			break;
		case 'h':
		default:
			print_usage();
			return -1;
		}
	}

	if (global->opt.num_pktio == 0) {
		ODPH_ERR("At least one pktio interface needed\n");
		ret = -1;
	}

	return ret;
}

static int open_pktios(test_global_t *global)
{
	odp_pool_param_t  pool_param;
	odp_pktio_param_t pktio_param;
	odp_pool_t pool;
	odp_pktio_capability_t pktio_capa;
	odp_pool_capability_t pool_capa;
	odp_pktio_t pktio;
	odp_pktio_config_t pktio_config;
	odp_pktin_queue_param_t pktin_param;
	char *name;
	int i, num_pktio;
	uint32_t num_pkt = MAX_PKT_NUM;

	num_pktio = global->opt.num_pktio;

	if (odp_pool_capability(&pool_capa)) {
		ODPH_ERR("Pool capability failed\n");
		return -1;
	}

	if (pool_capa.pkt.max_num < MAX_PKT_NUM)
		num_pkt = pool_capa.pkt.max_num;

	odp_pool_param_init(&pool_param);
	pool_param.pkt.num     = num_pkt;
	pool_param.type        = ODP_POOL_PACKET;

	pool = odp_pool_create("packet pool", &pool_param);

	global->pool = pool;

	if (pool == ODP_POOL_INVALID) {
		ODPH_ERR("Pool create failed\n");
		return -1;
	}

	odp_pktio_param_init(&pktio_param);
	pktio_param.in_mode  = ODP_PKTIN_MODE_SCHED;
	pktio_param.out_mode = ODP_PKTOUT_MODE_DISABLED;

	for (i = 0; i < num_pktio; i++)
		global->pktio[i].pktio = ODP_PKTIO_INVALID;

	/* Open and configure interfaces */
	for (i = 0; i < num_pktio; i++) {
		name  = global->opt.pktio_name[i];
		pktio = odp_pktio_open(name, pool, &pktio_param);

		if (pktio == ODP_PKTIO_INVALID) {
			ODPH_ERR("Pktio open failed for %s\n", name);
			return -1;
		}

		global->pktio[i].pktio = pktio;

		if (odp_pktio_capability(pktio, &pktio_capa)) {
			ODPH_ERR("Pktio capability failed for %s\n", name);
			return -1;
		}

		odp_pktio_print(pktio);

		odp_pktio_config_init(&pktio_config);
		pktio_config.pktin.bit.ts_all = pktio_capa.config.pktin.bit.ts_all;
		pktio_config.parser.layer = ODP_PROTO_LAYER_ALL;

		odp_pktio_config(pktio, &pktio_config);

		odp_pktin_queue_param_init(&pktin_param);

		pktin_param.queue_param.sched.prio  = odp_schedule_default_prio();
		pktin_param.queue_param.sched.sync  = ODP_SCHED_SYNC_ATOMIC;
		pktin_param.queue_param.sched.group = ODP_SCHED_GROUP_ALL;
		pktin_param.num_queues = 1;

		if (odp_pktin_queue_config(pktio, &pktin_param)) {
			ODPH_ERR("Pktin config failed for %s\n", name);
			return -1;
		}
	}

	return 0;
}

static int start_pktios(test_global_t *global)
{
	int i;

	for (i = 0; i < global->opt.num_pktio; i++) {
		if (odp_pktio_start(global->pktio[i].pktio)) {
			ODPH_ERR("Pktio start failed for %s\n", global->opt.pktio_name[i]);

			return -1;
		}

		global->pktio[i].started = 1;
	}

	return 0;
}

static int stop_pktios(test_global_t *global)
{
	odp_pktio_t pktio;
	int i, ret = 0;

	for (i = 0; i < global->opt.num_pktio; i++) {
		pktio = global->pktio[i].pktio;

		if (pktio == ODP_PKTIO_INVALID || global->pktio[i].started == 0)
			continue;

		if (odp_pktio_stop(pktio)) {
			ODPH_ERR("Pktio stop failed for %s\n", global->opt.pktio_name[i]);
			ret = -1;
		}
	}

	return ret;
}

static void empty_queues(void)
{
	odp_event_t ev;
	uint64_t wait_time = odp_schedule_wait_time(ODP_TIME_SEC_IN_NS / 2);

	/* Drop all events from all queues */
	while (1) {
		ev = odp_schedule(NULL, wait_time);

		if (ev == ODP_EVENT_INVALID)
			break;

		odp_event_free(ev);
	}
}

static int close_pktios(test_global_t *global)
{
	odp_pktio_t pktio;
	odp_pool_t pool;
	int i, ret = 0;

	for (i = 0; i < global->opt.num_pktio; i++) {
		pktio = global->pktio[i].pktio;

		if (pktio == ODP_PKTIO_INVALID)
			continue;

		if (odp_pktio_close(pktio)) {
			ODPH_ERR("Pktio close failed for %s\n", global->opt.pktio_name[i]);
			ret = -1;
		}
	}

	pool = global->pool;

	if (pool == ODP_POOL_INVALID)
		return ret;

	if (odp_pool_destroy(pool)) {
		ODPH_ERR("Pool destroy failed\n");
		ret = -1;
	}

	return ret;
}

static void print_mac_addr(uint8_t *addr)
{
	printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
	       addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

static void print_ipv4_addr(uint8_t *addr)
{
	printf("%u.%u.%u.%u\n",
	       addr[0], addr[1], addr[2], addr[3]);
}

static void print_port(uint8_t *ptr)
{
	uint16_t *port = (uint16_t *)(uintptr_t)ptr;

	printf("%u\n", odp_be_to_cpu_16(*port));
}

static void print_data(odp_packet_t pkt, uint32_t offset, uint32_t len)
{
	const uint32_t bytes_per_row = 16;
	const uint32_t num_char = 1 + (bytes_per_row * 3) + 1;
	uint8_t data[bytes_per_row];
	char row[num_char];
	uint32_t copy_len, i, j;
	uint32_t data_len = odp_packet_len(pkt);

	if (offset > data_len)
		return;

	if (offset + len > data_len)
		len = data_len - offset;

	while (len) {
		i = 0;

		if (len > bytes_per_row)
			copy_len = bytes_per_row;
		else
			copy_len = len;

		odp_packet_copy_to_mem(pkt, offset, copy_len, data);

		i += snprintf(&row[i], num_char - i, " ");

		for (j = 0; j < copy_len; j++)
			i += snprintf(&row[i], num_char - i, " %02x", data[j]);

		row[i] = 0;
		printf("%s\n", row);

		len    -= copy_len;
		offset += copy_len;
	}
}

static int print_packet(test_global_t *global, odp_packet_t pkt,
			uint64_t num_packet)
{
	odp_pktio_t pktio = odp_packet_input(pkt);
	odp_pktio_info_t pktio_info;
	odp_time_t pktio_time, time;
	uint64_t sec, nsec;
	uint32_t offset;
	int i, type, match;
	int num_filter_l3 = global->opt.num_filter_l3;
	int num_filter_l4 = global->opt.num_filter_l4;
	uint8_t *data = odp_packet_data(pkt);
	uint32_t seg_len = odp_packet_seg_len(pkt);
	uint32_t l2_offset = odp_packet_l2_offset(pkt);
	uint32_t l3_offset = odp_packet_l3_offset(pkt);
	uint32_t l4_offset = odp_packet_l4_offset(pkt);
	int tcp  = odp_packet_has_tcp(pkt);
	int udp  = odp_packet_has_udp(pkt);
	int sctp = odp_packet_has_sctp(pkt);
	int icmp = odp_packet_has_icmp(pkt);
	int ipv4 = odp_packet_has_ipv4(pkt);

	if (odp_packet_has_ts(pkt)) {
		pktio_time = odp_pktio_time(pktio, NULL);
		time = odp_packet_ts(pkt);
	} else {
		time = odp_time_local();
		pktio_time = ODP_TIME_NULL;
	}

	/* Filter based on L3 type */
	if (num_filter_l3) {
		match = 0;

		for (i = 0; i < num_filter_l3; i++) {
			type = global->opt.filter_l3[i];

			if (type == odp_packet_l3_type(pkt)) {
				match = 1;
				break;
			}
		}

		if (!match)
			return 0;
	}

	/* Filter based on L4 type */
	if (num_filter_l4) {
		match = 0;

		for (i = 0; i < num_filter_l4; i++) {
			type = global->opt.filter_l4[i];

			if (type == odp_packet_l4_type(pkt)) {
				match = 1;
				break;
			}
		}

		if (!match)
			return 0;
	}

	nsec  = odp_time_to_ns(time);
	sec   = nsec / ODP_TIME_SEC_IN_NS;
	nsec  = nsec - (sec * ODP_TIME_SEC_IN_NS);

	if (odp_pktio_info(pktio, &pktio_info)) {
		ODPH_ERR("Pktio info failed\n");
		return -1;
	}

	printf("PACKET [%" PRIu64 "]\n", num_packet);
	printf("  time:            %" PRIu64 ".%09" PRIu64 " sec\n", sec, nsec);

	if (odp_time_cmp(pktio_time, ODP_TIME_NULL)) {
		nsec  = odp_time_to_ns(pktio_time);
		sec   = nsec / ODP_TIME_SEC_IN_NS;
		nsec  = nsec - (sec * ODP_TIME_SEC_IN_NS);
		printf("  pktio time:      %" PRIu64 ".%09" PRIu64 " sec\n", sec, nsec);
		printf("  input delay:     %" PRIu64 " nsec\n", odp_time_diff_ns(pktio_time, time));
	}

	printf("  interface name:  %s\n", pktio_info.name);
	printf("  packet length:   %u bytes\n", odp_packet_len(pkt));

	/* L2 */
	if (odp_packet_has_eth(pkt)) {
		uint8_t *eth = data + l2_offset;

		printf("  Ethernet offset: %u bytes\n", l2_offset);
		if (l2_offset + 6 <= seg_len) {
			printf("    dst address:   ");
			print_mac_addr(eth);
		}

		if (l2_offset + 12 <= seg_len) {
			printf("    src address:   ");
			print_mac_addr(eth + 6);
		}

		/* VLAN */
		if (odp_packet_has_vlan(pkt)) {
			int qinq = odp_packet_has_vlan_qinq(pkt);
			uint16_t *tpid = (uint16_t *)(uintptr_t)(eth + 12);
			uint16_t *tci  = tpid + 1;

			if (qinq)
				printf("  VLAN (outer):\n");
			else
				printf("  VLAN:\n");

			if (l2_offset + 14 <= seg_len) {
				printf("    TPID:          0x%04x\n",
				       odp_be_to_cpu_16(*tpid));
			}

			if (l2_offset + 16 <= seg_len) {
				printf("    TCI:           0x%04x (VID: %u)\n",
				       odp_be_to_cpu_16(*tci),
				       odp_be_to_cpu_16(*tci) & 0x0fff);
			}

			if (qinq) {
				printf("  VLAN (inner):\n");
				tpid += 2;
				tci  += 2;

				if (l2_offset + 18 <= seg_len) {
					printf("    TPID:          0x%04x\n",
					       odp_be_to_cpu_16(*tpid));
				}

				if (l2_offset + 20 <= seg_len) {
					printf("    TCI:           0x%04x (VID: %u)\n",
					       odp_be_to_cpu_16(*tci),
					       odp_be_to_cpu_16(*tci) & 0x0fff);
				}
			}
		}

	} else if (odp_packet_has_l2(pkt)) {
		printf("  L2 (%i) offset:   %u bytes\n",
		       odp_packet_l2_type(pkt), l2_offset);
	}

	/* L3 */
	if (ipv4) {
		printf("  IPv4 offset:     %u bytes\n", l3_offset);
		offset = l3_offset + 12;
		if (offset + 4 <= seg_len) {
			printf("    src address:   ");
			print_ipv4_addr(data + offset);
		}

		offset = l3_offset + 16;
		if (offset + 4 <= seg_len) {
			printf("    dst address:   ");
			print_ipv4_addr(data + offset);
		}
	} else if (odp_packet_has_ipv6(pkt)) {
		printf("  IPv6 offset:     %u bytes\n", l3_offset);
	} else if (odp_packet_has_l3(pkt)) {
		printf("  L3 (%i) offset:   %u bytes\n",
		       odp_packet_l3_type(pkt), l3_offset);
	}

	/* L4 */
	if (tcp || udp || sctp) {
		if (tcp)
			printf("  TCP offset:      %u bytes\n", l4_offset);
		else if (udp)
			printf("  UDP offset:      %u bytes\n", l4_offset);
		else
			printf("  SCTP offset:     %u bytes\n", l4_offset);

		offset = l4_offset;
		if (offset + 2 <= seg_len) {
			printf("    src port:      ");
			print_port(data + offset);
		}

		offset = l4_offset + 2;
		if (offset + 2 <= seg_len) {
			printf("    dst port:      ");
			print_port(data + offset);
		}
	} else if (icmp) {
		printf("  ICMP offset:     %u bytes\n", l4_offset);
		if (ipv4) {
			uint32_t len;
			uint8_t *u8 = odp_packet_l4_ptr(pkt, &len);

			if (u8 && len >= 2) {
				printf("    type:          %u\n", u8[0]);
				printf("    code:          %u\n", u8[1]);
			}
		}
	} else if (odp_packet_has_l4(pkt)) {
		printf("  L4 (%i) offset:   %u bytes\n",
		       odp_packet_l4_type(pkt), l4_offset);
	}

	/* User defined data range */
	if (global->opt.data_len)
		print_data(pkt, global->opt.data_offset, global->opt.data_len);

	if (global->opt.verbose)
		odp_packet_print(pkt);

	printf("\n");

	return 1;
}

static int receive_packets(test_global_t *global)
{
	odp_event_t ev;
	odp_packet_t pkt;
	int printed;
	uint64_t num_packet = 0;

	while (!odp_atomic_load_u32(&global->stop)) {
		ev = odp_schedule(NULL, ODP_SCHED_NO_WAIT);

		if (ev == ODP_EVENT_INVALID)
			continue;

		if (odp_event_type(ev) != ODP_EVENT_PACKET) {
			ODPH_ERR("Bad event type: %i\n", odp_event_type(ev));
			odp_event_free(ev);
			continue;
		}

		pkt = odp_packet_from_event(ev);

		printed = print_packet(global, pkt, num_packet);

		odp_packet_free(pkt);

		if (odp_unlikely(printed < 0))
			return -1;

		if (!printed)
			continue;

		num_packet++;
		if (global->opt.num_packet &&
		    num_packet >= global->opt.num_packet)
			break;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	odp_instance_t instance;
	test_global_t *global;
	int ret = 0;

	global = &test_global;
	memset(global, 0, sizeof(test_global_t));
	odp_atomic_init_u32(&global->stop, 0);

	signal(SIGINT, sig_handler);

	if (parse_options(argc, argv, global))
		return -1;

	/* Init ODP before calling anything else */
	if (odp_init_global(&instance, NULL, NULL)) {
		ODPH_ERR("Global init failed\n");
		return -1;
	}

	/* Init this thread */
	if (odp_init_local(instance, ODP_THREAD_CONTROL)) {
		ODPH_ERR("Local init failed\n");
		return -1;
	}

	global->pool = ODP_POOL_INVALID;

	odp_schedule_config(NULL);

	odp_sys_info_print();

	if (open_pktios(global)) {
		ODPH_ERR("Pktio open failed\n");
		return -1;
	}

	if (start_pktios(global)) {
		ODPH_ERR("Pktio start failed\n");
		return -1;
	}

	if (receive_packets(global)) {
		ODPH_ERR("Packet receive failed\n");
		return -1;
	}

	if (stop_pktios(global)) {
		ODPH_ERR("Pktio stop failed\n");
		return -1;
	}

	empty_queues();

	if (close_pktios(global)) {
		ODPH_ERR("Pktio close failed\n");
		return -1;
	}

	if (odp_term_local()) {
		ODPH_ERR("Term local failed\n");
		return -1;
	}

	if (odp_term_global(instance)) {
		ODPH_ERR("Term global failed\n");
		return -1;
	}

	return ret;
}
