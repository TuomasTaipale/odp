#ifndef COMMON_H_
#define COMMON_H_

#define CPUMAP_DOMAIN "cpumap"
#define FLOW_DOMAIN "flows"
#define	PKTIO_DOMAIN "pktios"
#define	POOL_DOMAIN "pools"
#define	QUEUE_DOMAIN "queues"
#define TIMER_DOMAIN "timers"
#define	WORKER_DOMAIN "workers"

#define HIGH_PRIO 101
#define MED_PRIO (HIGH_PRIO + 1)
#define LOW_PRIO (MED_PRIO + 1)

#define WORK_COPY "copy"
#define WORK_FORWARD "forward"
#define WORK_SINK "sink"
#define WORK_TIMEOUT_SOURCE "timeout_source"

#endif
