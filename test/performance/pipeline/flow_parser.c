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
#include "flow.h"
#include "work.h"

#define CONF_STR_NAME "name"
#define CONF_STR_INPUT "input"
#define CONF_STR_WORK "work"
#define CONF_STR_TYPE "type"
#define CONF_STR_OUTPUT "output"
#define CONF_STR_TIMER "timer"
#define CONF_STR_TIMEOUT_POOL "timeout_pool"
#define CONF_STR_TIMEOUT_NS "timeout_ns"

typedef struct {
	char *name;
	char *input;
	char *output;
	flow_t flow;
	work_param_t *work;
	uint32_t num;
} flow_parse_t;

typedef struct {
	flow_parse_t *flows;
	uint32_t num;
} flow_parses_t;

static flow_parses_t flows;

static odp_bool_t parse_work_entry(config_setting_t *cs, work_param_t *work)
{
	const char *val_str;
	long long val_ll;

	if (config_setting_lookup_string(cs, CONF_STR_TYPE, &val_str) == CONFIG_FALSE) {
		ODPH_ERR("No \"" CONF_STR_TYPE "\" found\n");
		return false;
	}

	work->type = strdup(val_str);

	if (work->type == NULL)
		ODPH_ABORT("Error allocating memory, aborting\n");

	if (strcmp(work->type, WORK_FORWARD) == 0) {
		if (config_setting_lookup_string(cs, CONF_STR_OUTPUT, &val_str) == CONFIG_FALSE) {
			ODPH_ERR("No \"" CONF_STR_OUTPUT "\" found\n");
			return false;
		}

		work->param.output = strdup(val_str);

		if (work->param.output == NULL)
			ODPH_ABORT("Error allocating memory, aborting\n");
	}

	if (strcmp(work->type, WORK_TIMEOUT_SOURCE) == 0) {
		if (config_setting_lookup_string(cs, CONF_STR_TIMER, &val_str) == CONFIG_FALSE) {
			ODPH_ERR("No \"" CONF_STR_TIMER "\" found\n");
			return false;
		}

		work->param.timer = strdup(val_str);

		if (work->param.timer == NULL)
			ODPH_ABORT("Error allocating memory, aborting\n");

		if (config_setting_lookup_string(cs, CONF_STR_TIMEOUT_POOL, &val_str)
		    == CONFIG_FALSE) {
			ODPH_ERR("No \"" CONF_STR_TIMEOUT_POOL "\" found\n");
			return false;
		}

		work->param.timeout_pool = strdup(val_str);

		if (work->param.timeout_pool == NULL)
			ODPH_ABORT("Error allocating memory, aborting\n");

		if (config_setting_lookup_int64(cs, CONF_STR_TIMEOUT_NS, &val_ll)
		    == CONFIG_FALSE) {
			ODPH_ERR("No \"" CONF_STR_TIMEOUT_NS "\" found\n");
			return false;
		}

		work->param.timeout_ns = val_ll;
	}

	return true;
}

static void free_work_entry(work_param_t *work)
{
	free(work->type);
	free(work->param.output);
}

static odp_bool_t parse_flow_entry(config_setting_t *cs, flow_parse_t *flow)
{
	const char *val_str;
	int num;
	config_setting_t *elem;
	work_param_t *work;

	if (config_setting_lookup_string(cs, CONF_STR_NAME, &val_str) == CONFIG_FALSE) {
		ODPH_ERR("No \"" CONF_STR_NAME "\" found\n");
		return false;
	}

	flow->name = strdup(val_str);

	if (flow->name == NULL)
		ODPH_ABORT("Error allocating memory, aborting\n");

	if (config_setting_lookup_string(cs, CONF_STR_INPUT, &val_str) == CONFIG_TRUE) {
		flow->input = strdup(val_str);

		if (flow->input == NULL)
			ODPH_ABORT("Error allocating memory, aborting\n");
	} else if (config_setting_lookup_string(cs, CONF_STR_OUTPUT, &val_str) == CONFIG_TRUE) {
		flow->output = strdup(val_str);

		if (flow->output == NULL)
			ODPH_ABORT("Error allocating memory, aborting\n");
	} else {
		ODPH_ERR("No \"" CONF_STR_INPUT "\" or \"" CONF_STR_OUTPUT "\" found\n");
		return false;
	}

	cs = config_setting_lookup(cs, CONF_STR_WORK);

	if (cs == NULL) {
		ODPH_ERR("No \"" CONF_STR_WORK "\" found\n");
		return false;
	}

	num = config_setting_length(cs);

	if (num == 0) {
		ODPH_ERR("No valid \"" CONF_STR_WORK "\" entries found\n");
		return false;
	}

	flow->work = calloc(1U, num * sizeof(*flow->work));

	if (flow->work == NULL)
		ODPH_ABORT("Error allocating memory, aborting\n");

	for (int i = 0; i < num; ++i) {
		elem = config_setting_get_elem(cs, i);

		if (elem == NULL) {
			ODPH_ERR("Unparsable \"" CONF_STR_WORK "\" entry (%d)\n", i);
			return false;
		}

		work = &flow->work[i];
		work->queue = flow->input != NULL ? flow->input : flow->output;

		if (!parse_work_entry(elem, work)) {
			ODPH_ERR("Invalid \"" CONF_STR_WORK "\" entry (%d)\n", i);
			free_work_entry(work);
			return false;
		}

		++flow->num;
	}

	return true;
}

static void free_flow_entry(flow_parse_t *flow)
{
	free(flow->name);
	flow_destroy_flow(flow->flow);

	for (uint32_t i = 0U; i < flow->num; ++i)
		free_work_entry(&flow->work[i]);

	free(flow->work);
}

static odp_bool_t flow_parser_init(config_t *config)
{
	config_setting_t *cs, *elem;
	int num;
	flow_parse_t *flow;

	cs = config_lookup(config, FLOW_DOMAIN);

	if (cs == NULL)	{
		printf("Nothing to parse for \"" FLOW_DOMAIN "\" domain\n");
		return true;
	}

	num = config_setting_length(cs);

	if (num == 0) {
		ODPH_ERR("No valid \"" FLOW_DOMAIN "\" entries found\n");
		return false;
	}

	flows.flows = calloc(1U, num * sizeof(*flows.flows));

	if (flows.flows == NULL)
		ODPH_ABORT("Error allocating memory, aborting\n");

	for (int i = 0; i < num; ++i) {
		elem = config_setting_get_elem(cs, i);

		if (elem == NULL) {
			ODPH_ERR("Unparsable \"" FLOW_DOMAIN "\" entry (%d)\n", i);
			return false;
		}

		flow = &flows.flows[flows.num];

		if (!parse_flow_entry(elem, flow)) {
			ODPH_ERR("Invalid \"" FLOW_DOMAIN "\" entry (%d)\n", i);
			free_flow_entry(flow);
			return false;
		}

		++flows.num;
	}

	return true;
}

static odp_bool_t flow_parser_deploy(void)
{
	flow_parse_t *parse;
	char *name;
	odp_queue_t queue;
	flow_t flow;
	work_t *work;

	printf("\n*** " FLOW_DOMAIN " resources ***\n");

	for (uint32_t i = 0U; i < flows.num; ++i) {
		parse = &flows.flows[i];
		work = calloc(1U, parse->num * sizeof(*work));

		if (work == NULL)
			ODPH_ABORT("Error allocating memory, aborting\n");

		for (uint32_t j = 0U; j < parse->num; ++j)
			work[j] = work_create_work(&parse->work[j]);

		name = parse->input != NULL ? parse->input : parse->output;
		queue = (odp_queue_t)config_parser_get_resource(QUEUE_DOMAIN, name);
		flow = odp_queue_context(queue);

		if (flow == NULL) {
			parse->flow = flow_create_flow(name);

			if (parse->input != NULL)
				(void)flow_add_input(parse->flow, work, parse->num);
			else
				(void)flow_add_output(parse->flow, work, parse->num);

			if (odp_queue_context_set(queue, parse->flow, sizeof(parse->flow)) < 0) {
				ODPH_ERR("Error setting queue context\n");
				return false;
			}
		} else {
			if (parse->input != NULL &&
			    !flow_add_input(flow, work, parse->num)) {
				ODPH_ERR("Error setting input flow\n");

				for (uint32_t j = 0U; j < parse->num; ++j) {
					work_destroy_work(work[j]);
					free(work);
				}

				return false;
			} else if (!flow_add_output(flow, work, parse->num)) {
				ODPH_ERR("Error setting output flow\n");

				for (uint32_t j = 0U; j < parse->num; ++j) {
					work_destroy_work(work[j]);
					free(work);
				}

				return false;
			}
		}
	}

	for (uint32_t i = 0U; i < flows.num; ++i) {
		parse = &flows.flows[i];
		printf("\nname: %s\n"
		       "info:\n", parse->name);

		if (parse->input != NULL) {
			printf("  type:  input\n"
			       "  queue: %s\n"
			       "  work:\n", parse->input);

			for (uint32_t j = 0U; j < parse->num; ++j)
				printf("    %s\n", parse->work[j].type);
		} else {
			printf("  type:  output\n"
			       "  queue: %s\n"
			       "  work:\n", parse->output);

			for (uint32_t j = 0U; j < parse->num; ++j)
				printf("    %s\n", parse->work[j].type);
		}
	}

	return true;
}

static void flow_parser_destroy(void)
{
	for (uint32_t i = 0U; i < flows.num; ++i)
		free_flow_entry(&flows.flows[i]);

	free(flows.flows);
}

static uintptr_t flow_parser_get_resource(const char *resource)
{
	flow_parse_t *parse;
	flow_t flow = NULL;

	for (uint32_t i = 0U; i < flows.num; ++i) {
		parse = &flows.flows[i];

		if (strcmp(parse->name, resource) != 0)
			continue;

		flow = parse->flow;
		break;
	}

	if (flow == NULL)
		ODPH_ABORT("No resource found (%s), aborting\n", resource);

	return (uintptr_t)flow;
}

CONFIG_PARSER_AUTOREGISTER(LOW_PRIO, FLOW_DOMAIN, flow_parser_init, flow_parser_deploy,
			   flow_parser_destroy, flow_parser_get_resource)
