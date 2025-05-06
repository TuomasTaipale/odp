#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <inttypes.h>
#include <stdlib.h>

#include <odp_api.h>
#include <odp/helper/odph_api.h>
#include <signal.h>

#include "common.h"
#include "config_parser.h"
#include "cpumap.h"
#include "flow.h"
#include "orchestrator.h"
#include "worker.h"

#define MAX_WORKERS (ODP_THREAD_COUNT_MAX - 1)

typedef struct orchestrator_s orchestrator_t;

typedef struct {
	uint64_t num_unhandled_in;
	uint64_t num_unhandled_out;
} stats_t;

typedef struct ODP_ALIGNED_CACHE {
	odp_queue_t *inputs;
	odp_queue_t *outputs;
	orchestrator_t *prog_config;
	worker_t *worker;
	stats_t stats;
} orchestrator_worker_t;

typedef struct orchestrator_s {
	odph_thread_t thrs[MAX_WORKERS];
	orchestrator_worker_t worker[MAX_WORKERS];
	odp_instance_t instance;
	odp_shm_t shm;
	odp_atomic_u32_t is_running;
	odp_barrier_t init_barrier;
	odp_barrier_t term_barrier;
	uint32_t num_workers;
} orchestrator_t;

typedef int (*worker_fn_t)(void *);

static orchestrator_t *config;

static odp_instance_t init_odp(void)
{
	odp_instance_t instance;

	if (odp_init_global(&instance, NULL, NULL) < 0)
		ODPH_ABORT("ODP global init failed, aborting\n");

	if (odp_init_local(instance, ODP_THREAD_CONTROL) < 0)
		ODPH_ABORT("ODP local init failed, aborting\n");

	return instance;
}

static void term_odp(odp_instance_t instance)
{
	if (odp_term_local() < 0)
		ODPH_ABORT("ODP local terminate failed, aborting\n");

	if (odp_term_global(instance) < 0)
		ODPH_ABORT("ODP global terminate failed, aborting\n");
}

static void terminate(int signal ODP_UNUSED)
{
	odp_atomic_store_u32(&config->is_running, 0U);
}

static void setup_signals(void)
{
	struct sigaction action = { .sa_handler = terminate };

	if (sigemptyset(&action.sa_mask) == -1 || sigaddset(&action.sa_mask, SIGINT) == -1 ||
	    sigaddset(&action.sa_mask, SIGTERM) == -1 ||
	    sigaddset(&action.sa_mask, SIGHUP) == -1 || sigaction(SIGINT, &action, NULL) == -1 ||
	    sigaction(SIGTERM, &action, NULL) == -1 || sigaction(SIGHUP, &action, NULL) == -1)
		ODPH_ABORT("Error installing signal handler, aborting\n");
}

odp_bool_t orchestrator_init(void)
{
	odp_instance_t instance = init_odp();
	odp_shm_t shm;

	shm = odp_shm_reserve("orchestrator", sizeof(orchestrator_t), ODP_CACHE_LINE_SIZE, 0U);

	if (shm == ODP_SHM_INVALID) {
		ODPH_ERR("Error reserving shared memory\n");
		term_odp(instance);
		return false;
	}

	config = odp_shm_addr(shm);

	if (config == NULL) {
		ODPH_ERR("Error resolving shared memory address\n");
		odp_shm_free(shm);
		term_odp(instance);
		return false;
	}

	if (odp_schedule_config(NULL) < 0) {
		ODPH_ERR("Error configuring scheduler\n");
		odp_shm_free(shm);
		term_odp(instance);
		return false;
	}

	config->instance = instance;
	config->shm = shm;
	odp_atomic_init_u32(&config->is_running, 1U);
	setup_signals();

	return true;
}

static void drain_events(void)
{
	while (true) {
		odp_event_t  ev;

		ev = odp_schedule(NULL, ODP_SCHED_NO_WAIT);

		if (ev == ODP_EVENT_INVALID)
			break;

		odp_event_free(ev);
	}
}

static int schedule_and_handle(void *args)
{
	orchestrator_worker_t *worker = args;
	odp_atomic_u32_t *is_running = &worker->prog_config->is_running;
	const uint32_t burst_size = 32, num_out = worker->worker->num_out;
	odp_event_t evs[burst_size];
	int num_recv, num_procd;
	odp_queue_t input, output;
	stats_t *stats = &worker->stats;

	odp_barrier_wait(&worker->prog_config->init_barrier);

	while (odp_atomic_load_u32(is_running)) {
		num_recv = odp_schedule_multi_no_wait(&input, evs, burst_size);

		if (num_recv > 0) {
			num_procd = flow_issue(F_IN, odp_queue_context(input), evs, num_recv);

			if (odp_unlikely(num_procd < num_recv)) {
				odp_event_free_multi(&evs[num_procd], num_recv - num_procd);
				++stats->num_unhandled_in;
			}
		}

		for (uint32_t i = 0U; i < num_out; ++i) {
			output = worker->outputs[i];
			num_recv = flow_issue(F_OUT, odp_queue_context(output), evs, burst_size);

			if (num_recv == 0U)
				continue;

			num_procd = odp_queue_enq_multi(output, evs, num_recv);

			if (odp_unlikely(num_procd < 0))
				ODPH_ABORT("Error enqueueing, aborting\n");

			if (odp_unlikely(num_procd < num_recv)) {
				odp_event_free_multi(&evs[num_procd], num_recv - num_procd);
				++stats->num_unhandled_out;
			}
		}
	}

	odp_schedule_pause();
	drain_events();
	odp_barrier_wait(&worker->prog_config->term_barrier);
	odp_schedule_resume();
	drain_events();

	return 0;
}

static int poll_and_handle(void *args)
{
	orchestrator_worker_t *worker = args;
	odp_atomic_u32_t *is_running = &worker->prog_config->is_running;
	odp_queue_t input;
	const uint32_t burst_size = 32, num_in = worker->worker->num_in,
	num_out = worker->worker->num_out;
	odp_event_t evs[burst_size];
	int num_recv, num_procd;
	odp_queue_t output;
	stats_t *stats = &worker->stats;

	odp_barrier_wait(&worker->prog_config->init_barrier);

	while (odp_atomic_load_u32(is_running)) {
		for (uint32_t i = 0U; i < num_in; ++i) {
			input = worker->inputs[i];
			num_recv = odp_queue_deq_multi(input, evs, burst_size);

			if (odp_unlikely(num_recv < 0))
				ODPH_ABORT("Error dequeuing, aborting\n");

			if (num_recv == 0)
				continue;

			num_procd = flow_issue(F_IN, odp_queue_context(input), evs, num_recv);

			if (odp_unlikely(num_procd < num_recv)) {
				odp_event_free_multi(&evs[num_procd], num_recv - num_procd);
				++stats->num_unhandled_in;
			}
		}

		for (uint32_t i = 0U; i < num_out; ++i) {
			output = worker->outputs[i];
			num_recv = flow_issue(F_OUT, odp_queue_context(output), evs, burst_size);

			if (num_recv == 0U)
				continue;

			num_procd = odp_queue_enq_multi(output, evs, num_recv);

			if (odp_unlikely(num_procd < 0))
				ODPH_ABORT("Error enqueueing, aborting\n");

			if (odp_unlikely(num_procd < num_recv)) {
				odp_event_free_multi(&evs[num_procd], num_recv - num_procd);
				++stats->num_unhandled_out;
			}
		}
	}

	odp_barrier_wait(&worker->prog_config->term_barrier);

	return 0;
}

static worker_fn_t get_worker(orchestrator_worker_t *orch)
{
	worker_t *worker = orch->worker;
	odp_queue_t *outputs, *inputs;

	if (worker->num_out > 0U) {
		outputs = calloc(1U, worker->num_out * sizeof(*outputs));

		if (outputs == NULL)
			ODPH_ABORT("Error allocating memory, aborting\n");

		for (uint32_t i = 0U; i < worker->num_out; ++i)
			outputs[i] = (odp_queue_t)config_parser_get_resource(QUEUE_DOMAIN,
									     worker->outputs[i]);

		orch->outputs = outputs;
	}

	if (worker->type == WT_SCHED)
		return schedule_and_handle;

	if (worker->num_in > 0U) {
		inputs = calloc(1U, worker->num_in * sizeof(*inputs));

		if (inputs == NULL)
			ODPH_ABORT("Error allocating memory, aborting\n");

		for (uint32_t i = 0U; i < worker->num_in; ++i)
			inputs[i] = (odp_queue_t)config_parser_get_resource(QUEUE_DOMAIN,
									worker->inputs[i]);

		orch->inputs = inputs;
	}

	return poll_and_handle;
}

static void print_stats(void)
{
	orchestrator_worker_t *worker;

	printf("\n*** pipeline finished ***\n");

	for (uint32_t i = 0U; i < config->num_workers; ++i) {
		worker = &config->worker[i];
		printf("\n%s:\n"
		       "  unhandled input events:  %" PRIu64 "\n"
		       "  unhandled output events: %" PRIu64 "\n", worker->worker->name,
		       worker->stats.num_unhandled_in, worker->stats.num_unhandled_out);
	}
}

void orchestrator_deploy(void)
{
	cpumap_t *map = (cpumap_t *)config_parser_get_resource(CPUMAP_DOMAIN, NULL);
	odph_thread_common_param_t common;
	const int num = map->num;
	odph_thread_param_t params[num], *param;
	orchestrator_worker_t *worker;

	odp_barrier_init(&config->init_barrier, num + 1);
	odp_barrier_init(&config->term_barrier, num + 1);
	odph_thread_common_param_init(&common);
	common.instance = config->instance;
	common.cpumask = &map->cpumask;

	for (int i = 0; i < num; ++i) {
		worker = &config->worker[i];
		worker->prog_config = config;
		worker->worker = (worker_t *)config_parser_get_resource(WORKER_DOMAIN,
									map->workers[i]);
		param = &params[i];
		odph_thread_param_init(param);
		param->start = get_worker(worker);
		param->thr_type = ODP_THREAD_WORKER;
		param->arg = worker;
	}

	if (odph_thread_create(config->thrs, &common, params, num) != num)
		ODPH_ABORT("Error launching worker threads, aborting\n");

	config->num_workers = num;
	odp_barrier_wait(&config->init_barrier);
	odp_barrier_wait(&config->term_barrier);
	print_stats();
}

void orchestrator_destroy(void)
{
	odp_instance_t instance = config->instance;

	for (uint32_t i = 0U; i < config->num_workers; ++i) {
		free(config->worker[i].inputs);
		free(config->worker[i].outputs);
	}

	odp_shm_free(config->shm);
	term_odp(instance);
}
