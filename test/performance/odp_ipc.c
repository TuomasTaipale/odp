#include "odp_ipc.h"

int odp_ipc_addr_to_message_multi(const void *addr[], odp_ipc_message_t msg[], int num)
{
	(void)addr;
	(void)msg;
	(void)num;

	return -1;
}

int odp_ipc_buffer_to_message_multi(const odp_buffer_t buf[], odp_ipc_message_t msg[], int num)
{
	(void)buf;
	(void)msg;
	(void)num;

	return -1;
}

int odp_ipc_packet_to_message_multi(const odp_packet_t pkt[], odp_ipc_message_t msg[], int num)
{
	(void)pkt;
	(void)msg;
	(void)num;

	return -1;
}

int odp_ipc_addr_from_message_multi(void *addr[], const odp_ipc_message_t msg[], int num)
{
	(void)addr;
	(void)msg;
	(void)num;

	return -1;
}

int odp_ipc_buffer_from_message_multi(odp_buffer_t buf[], const odp_ipc_message_t msg[], int num)
{
	(void)buf;
	(void)msg;
	(void)num;

	return -1;
}

int odp_ipc_packet_from_message_multi(odp_packet_t pkt[], const odp_ipc_message_t msg[], int num)
{
	(void)pkt;
	(void)msg;
	(void)num;

	return -1;
}

int odp_ipc_capability(odp_ipc_capability_t *capa)
{
	(void)capa;

	return -1;
}

void odp_ipc_param_init(odp_ipc_param_t *param)
{
	(void)param;
}

odp_ipc_t odp_ipc_open(const odp_ipc_param_t *param)
{
	(void)param;

	return NULL;
}

void odp_ipc_close(odp_ipc_t ipc)
{
	(void)ipc;
}

int odp_ipc_send(odp_ipc_t ipc, odp_ipc_format_t format, odp_ipc_message_t data[], uint32_t num,
		 odp_ipc_result_t *result)
{
	(void)ipc;
	(void)format;
	(void)data;
	(void)num;
	(void)result;

	return -1;
}

int odp_ipc_send_start(odp_ipc_t ipc, odp_ipc_format_t format, odp_ipc_message_t data[],
		       uint32_t num, const odp_ipc_compl_t *compl)
{
	(void)ipc;
	(void)format;
	(void)data;
	(void)num;
	(void)compl;

	return -1;
}

int odp_ipc_done(odp_ipc_t ipc, uint32_t compl_id, odp_ipc_result_t *result)
{
	(void)ipc;
	(void)compl_id;
	(void)result;

	return -1;
}

int odp_ipc_recv(odp_ipc_t ipc, odp_ipc_format_t format, odp_ipc_message_t data[], uint32_t num)
{
	(void)ipc;
	(void)format;
	(void)data;
	(void)num;

	return -1;
}
