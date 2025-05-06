include $(top_srcdir)/test/Makefile.inc

if LIBCONFIG
AM_CFLAGS += \
	$(LIBCONFIG_CFLAGS) \
	-I$(top_srcdir)/test/performance/pipeline

bin_PROGRAMS = odp_pipeline

odp_pipeline_SOURCES = \
	common.h \
	config_parser.h \
	config_parser.c \
	cpumap.h \
	cpumap_parser.c \
	flow.h \
	flow.c \
	flow_parser.c \
	helpers.h \
	main.c \
	orchestrator.h \
	orchestrator.c \
	pktio_parser.c \
	pool_parser.c \
	queue_parser.c \
	timer_parser.c \
	work.h \
	work.c \
	work_copy.c \
	work_forward.c \
	work_sink.c \
	work_timeout_source.c \
	worker.h \
	worker_parser.c

endif
