#ifndef ORCHESTRATOR_H_
#define ORCHESTRATOR_H_

#include <odp_api.h>

odp_bool_t orchestrator_init(void);

void orchestrator_deploy(void);

void orchestrator_destroy(void);

#endif
