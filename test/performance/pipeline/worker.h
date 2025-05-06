#ifndef WORKER_H_
#define WORKER_H_

#include <stdint.h>

typedef enum {
	WT_PLAIN,
	WT_SCHED
} worker_type_t;

typedef struct {
	char *name;
	char **inputs;
	char **outputs;
	uint32_t num_in;
	uint32_t num_out;
	worker_type_t type;
} worker_t;

#endif
