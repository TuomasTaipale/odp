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
#define CONF_STR_TYPE "type"

#define PLAIN "plain"
#define SCHEDULED "schedule"

typedef struct {
	char *name;
	char *ext;
	odp_queue_param_t param;
	odp_queue_t queue;
} queue_parse_t;

typedef struct {
	queue_parse_t *queues;
	uint32_t num;
} queue_parses_t;

static queue_parses_t queues;

static odp_bool_t parse_queue_entry(config_setting_t *cs, queue_parse_t *queue)
{
	const char *val_str;

	queue->queue = ODP_QUEUE_INVALID;
	odp_queue_param_init(&queue->param);

	if (config_setting_lookup_string(cs, CONF_STR_NAME, &val_str) == CONFIG_FALSE) {
		ODPH_ERR("No \"" CONF_STR_NAME "\" found\n");
		return false;
	}

	queue->name = strdup(val_str);

	if (queue->name == NULL)
		ODPH_ABORT("Error allocating memory, aborting\n");

	if (config_setting_lookup_string(cs, CONF_STR_TYPE, &val_str) == CONFIG_TRUE) {
		if (strcmp(val_str, PLAIN) == 0) {
			queue->param.type = ODP_QUEUE_TYPE_PLAIN;
		} else if (strcmp(val_str, SCHEDULED) == 0) {
			queue->param.type = ODP_QUEUE_TYPE_SCHED;
		} else	{
			queue->ext = strdup(val_str);

			if (queue->ext == NULL)
				ODPH_ABORT("Error allocating memory, aborting\n");
		}
	}

	return true;
}

static void free_queue_entry(queue_parse_t *queue)
{
	free(queue->name);
	free(queue->ext);

	if (queue->queue != ODP_QUEUE_INVALID)
		(void)odp_queue_destroy(queue->queue);
}

static odp_bool_t queue_parser_init(config_t *config)
{
	config_setting_t *cs, *elem;
	int num;
	queue_parse_t *queue;

	cs = config_lookup(config, QUEUE_DOMAIN);

	if (cs == NULL)	{
		printf("Nothing to parse for \"" QUEUE_DOMAIN "\" domain\n");
		return true;
	}

	num = config_setting_length(cs);

	if (num == 0) {
		ODPH_ERR("No valid \"" QUEUE_DOMAIN "\" entries found\n");
		return false;
	}

	queues.queues = calloc(1U, num * sizeof(*queues.queues));

	if (queues.queues == NULL)
		ODPH_ABORT("Error allocating memory, aborting\n");

	for (int i = 0; i < num; ++i) {
		elem = config_setting_get_elem(cs, i);

		if (elem == NULL) {
			ODPH_ERR("Unparsable \"" QUEUE_DOMAIN "\" entry (%d)\n", i);
			return false;
		}

		queue = &queues.queues[queues.num];

		if (!parse_queue_entry(elem, queue)) {
			ODPH_ERR("Invalid \"" QUEUE_DOMAIN "\" entry (%d)\n", i);
			free_queue_entry(queue);
			return false;
		}

		++queues.num;
	}

	return true;
}

static odp_bool_t queue_parser_deploy(void)
{
	queue_parse_t *queue;

	printf("\n*** " QUEUE_DOMAIN " resources ***\n");

	for (uint32_t i = 0U; i < queues.num; ++i) {
		queue = &queues.queues[i];

		if (queue->ext != NULL)
			continue;

		queue->queue = odp_queue_create(queue->name, &queue->param);

		if (queue->queue == ODP_QUEUE_INVALID) {
			ODPH_ERR("Error creating queue (%s)\n", queue->name);
			return false;
		}

		printf("\nname: %s\n"
		       "info:\n", queue->name);
		odp_queue_print(queue->queue);
	}

	return true;
}

static void queue_parser_destroy(void)
{
	for (uint32_t i = 0U; i < queues.num; ++i)
		free_queue_entry(&queues.queues[i]);

	free(queues.queues);
}

static uintptr_t queue_parser_get_resource(const char *resource)
{
	queue_parse_t *parse;
	odp_queue_t queue = ODP_QUEUE_INVALID;

	for (uint32_t i = 0U; i < queues.num; ++i) {
		parse = &queues.queues[i];

		if (strcmp(parse->name, resource) != 0)
			continue;

		if (parse->ext != NULL)
			queue = (odp_queue_t)config_parser_get(parse->ext, resource);
		else
			queue = parse->queue;

		break;
	}

	if (queue == ODP_QUEUE_INVALID)
		ODPH_ABORT("No resource found (%s), aborting\n", resource);

	return (uintptr_t)queue;
}

CONFIG_PARSER_AUTOREGISTER(HIGH_PRIO, QUEUE_DOMAIN, queue_parser_init, queue_parser_deploy,
			   queue_parser_destroy, queue_parser_get_resource)
