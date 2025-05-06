#ifndef CPUMAP_H_
#define CPUMAP_H_

#include <stdint.h>

#include <odp_api.h>

typedef struct {
	char **workers;
	uint32_t num;
	odp_cpumask_t cpumask;
} cpumap_t;

#endif
