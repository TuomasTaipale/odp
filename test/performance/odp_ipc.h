#ifndef ODP_API_IPC_H_
#define ODP_API_IPC_H_

#include <inttypes.h>

#include <odp_api.h>

#define ODP_IPC_FORMAT_ADDR   0x1U
#define ODP_IPC_FORMAT_BUFFER 0x2U
#define ODP_IPC_FORMAT_PACKET 0x4U

#define ODP_IPC_SYNC        0x1U
#define	ODP_IPC_ASYNC_POLL  0x2U
#define	ODP_IPC_ASYNC_EVENT 0x4U

#define ODP_IPC_DIRECT 0x1U
#define ODP_IPC_SCHED  0x2U

#define ODP_IPC_SUCCESS 0

/**
 * TODO
 */
typedef void *odp_ipc_message_t;

/**
 * TODO
 */
typedef uint32_t odp_ipc_format_t;

/**
 * TODO
 */
typedef uint32_t odp_ipc_compl_mode_t;

/**
 * TODO
 */
typedef uint32_t odp_ipc_input_mode_t;

/**
 * TODO
 */
typedef struct {
	odp_ipc_format_t format_mask;
	odp_ipc_compl_mode_t compl_mode_mask;
	odp_ipc_input_mode_t input_mode_mask;
	uint32_t max_compl_id;
	uint32_t mtu;
} odp_ipc_capability_t;

/**
 * TODO
 */
typedef struct {
	odp_ipc_format_t format_mask;
	odp_ipc_compl_mode_t compl_mode_mask;
	odp_ipc_input_mode_t input_mode;

	struct {
		void *pool;
		int (*alloc_fn)(void *pool, void *data[], int num);
		void (*free_fn)(void *pool, const void *data[], int num);
	} addr;

	struct {
		odp_pool_t pool;
	} buf;

	struct {
		odp_pool_t pool;
		uint32_t mtu;
	} pkt;
} odp_ipc_param_t;

/**
 * TODO
 */
typedef void *odp_ipc_t;

/**
 * TODO
 */
typedef struct {
	odp_ipc_compl_mode_t mode;
	void *user_ptr;

	union {
		struct {
			odp_event_t event;
			odp_queue_t queue;
		};

		uint32_t compl_id;
	};
} odp_ipc_compl_t;

/**
 * TODO
 */
typedef struct {
	int err_code;
	void *user_ptr;
} odp_ipc_result_t;

/**
 * TODO
 */
int odp_ipc_addr_to_message_multi(const void *addr[], odp_ipc_message_t msg[], int num);

/**
 * TODO
 */
int odp_ipc_buffer_to_message_multi(const odp_buffer_t buf[], odp_ipc_message_t msg[], int num);

/**
 * TODO
 */
int odp_ipc_packet_to_message_multi(const odp_packet_t pkt[], odp_ipc_message_t msg[], int num);

/**
 * TODO
 */
int odp_ipc_addr_from_message_multi(void *addr[], const odp_ipc_message_t msg[], int num);

/**
 * TODO
 */
int odp_ipc_buffer_from_message_multi(odp_buffer_t buf[], const odp_ipc_message_t msg[], int num);

/**
 * TODO
 */
int odp_ipc_packet_from_message_multi(odp_packet_t pkt[], const odp_ipc_message_t msg[], int num);

/**
 * TODO
 */
int odp_ipc_capability(odp_ipc_capability_t *capa);

/**
 * TODO
 */
void odp_ipc_param_init(odp_ipc_param_t *param);

/**
 * TODO
 */
odp_ipc_t odp_ipc_open(const odp_ipc_param_t *param);

/**
 * TODO
 */
void odp_ipc_close(odp_ipc_t ipc);

/**
 * TODO
 */
int odp_ipc_send(odp_ipc_t ipc, odp_ipc_format_t format, odp_ipc_message_t data[], uint32_t num,
		 odp_ipc_result_t *result);

/**
 * TODO
 */
int odp_ipc_send_start(odp_ipc_t ipc, odp_ipc_format_t format, odp_ipc_message_t data[],
		       uint32_t num, const odp_ipc_compl_t *compl);

/**
 * TODO
 */
int odp_ipc_done(odp_ipc_t ipc, uint32_t compl_id, odp_ipc_result_t *result);

/**
 * TODO
 */
int odp_ipc_recv(odp_ipc_t ipc, odp_ipc_format_t format, odp_ipc_message_t data[], uint32_t num);

#endif
