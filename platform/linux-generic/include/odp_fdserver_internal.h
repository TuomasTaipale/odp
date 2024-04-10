/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2016-2018 Linaro Limited
 * Copyright (c) 2024 Nokia
 */

#ifndef _FD_SERVER_INTERNAL_H
#define _FD_SERVER_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <odp_debug_internal.h>
#include <odp_config_internal.h>

#include <stdint.h>
#include <string.h>
#include <sys/socket.h>

/* Debug level for the FD server */
#define FD_DBG  3
/* define the tables of file descriptors handled by this server: */
#define FDSERVER_MAX_ENTRIES (CONFIG_SHM_BLOCKS + CONFIG_INTERNAL_SHM_BLOCKS)
/* possible commands: */
#define FD_REGISTER_REQ		1  /* client -> server */
#define FD_REGISTER_ACK		2  /* server -> client */
#define FD_REGISTER_NACK	3  /* server -> client */
#define FD_LOOKUP_REQ		4  /* client -> server */
#define FD_LOOKUP_ACK		5  /* server -> client */
#define FD_LOOKUP_NACK		6  /* server -> client */
#define FD_DEREGISTER_REQ	7  /* client -> server */
#define FD_DEREGISTER_ACK	8  /* server -> client */
#define FD_DEREGISTER_NACK	9  /* server -> client */
#define FD_SERVERSTOP_REQ	10 /* client -> server (stops) */

/*
 * the following enum defines the different contexts by which the
 * FD server may be used: In the FD server, the keys used to store/retrieve
 * a file descriptor are actually context based:
 * Both the context and the key are stored at fd registration time,
 * and both the context and the key are used to retrieve a fd.
 * In other words a context identifies a FD server usage, so that different
 * unrelated fd server users do not have to guarantee key unicity between
 * them.
 */
typedef enum fd_server_context {
	FD_SRV_CTX_NA,  /* Not Applicable   */
	FD_SRV_CTX_ISHM,
	FD_SRV_CTX_END, /* upper enum limit */
} fd_server_context_e;

typedef struct fdentry_s {
	fd_server_context_e context;
	uint64_t key;
	int  fd;
} fdentry_t;

/*
 * define the message struct used for communication between client and server
 * (this single message is used in both direction)
 * The file descriptors are sent out of band as ancillary data for conversion.
 */
typedef struct fd_server_msg {
	int command;
	fd_server_context_e context;
	uint64_t key;
} fdserver_msg_t;

int _odp_fdserver_register_fd(fd_server_context_e context, uint64_t key,
			      int fd);
int _odp_fdserver_deregister_fd(fd_server_context_e context, uint64_t key);
int _odp_fdserver_lookup_fd(fd_server_context_e context, uint64_t key);
/*
 * Client and server function:
 * Send a fdserver_msg, possibly including a file descriptor, on the socket
 * This function is used both by:
 * -the client (sending a FD_REGISTER_REQ with a file descriptor to be shared,
 *  or FD_LOOKUP_REQ/FD_DEREGISTER_REQ without a file descriptor)
 * -the server (sending FD_REGISTER_ACK/NACK, FD_LOOKUP_NACK,
 *  FD_DEREGISTER_ACK/NACK... without a fd or a
 *  FD_LOOKUP_ACK with a fd)
 * This function make use of the ancillary data (control data) to pass and
 * convert file descriptors over UNIX sockets
 * Return -1 on error, 0 on success.
 */
static inline int _odp_send_fdserver_msg(int sock, int command, fd_server_context_e context,
					 uint64_t key, int fd_to_send)
{
	struct msghdr socket_message;
	struct iovec io_vector[1]; /* one msg frgmt only */
	struct cmsghdr *control_message = NULL;
	int *fd_location;
	fdserver_msg_t msg;
	int res;

	char ancillary_data[CMSG_SPACE(sizeof(int))];

	/* prepare the register request body (single fragment): */
	msg.command = command;
	msg.context = context;
	msg.key = key;
	io_vector[0].iov_base = &msg;
	io_vector[0].iov_len = sizeof(fdserver_msg_t);

	/* initialize socket message */
	memset(&socket_message, 0, sizeof(struct msghdr));
	socket_message.msg_iov = io_vector;
	socket_message.msg_iovlen = 1;

	if (fd_to_send >= 0) {
		/* provide space for the ancillary data */
		memset(ancillary_data, 0, CMSG_SPACE(sizeof(int)));
		socket_message.msg_control = ancillary_data;
		socket_message.msg_controllen = CMSG_SPACE(sizeof(int));

		/* initialize a single ancillary data element for fd passing */
		control_message = CMSG_FIRSTHDR(&socket_message);
		control_message->cmsg_level = SOL_SOCKET;
		control_message->cmsg_type = SCM_RIGHTS;
		control_message->cmsg_len = CMSG_LEN(sizeof(int));
		fd_location = (int *)(void *)CMSG_DATA(control_message);
		*fd_location = fd_to_send;
	}
	res = sendmsg(sock, &socket_message, 0);
	if (res < 0)
		return -1;

	return 0;
}

/*
 * Client and server function
 * Receive a fdserver_msg, possibly including a file descriptor, on the
 * given socket.
 * This function is used both by:
 * -the server (receiving a FD_REGISTER_REQ with a file descriptor to be shared,
 *  or FD_LOOKUP_REQ, FD_DEREGISTER_REQ without a file descriptor)
 * -the client (receiving FD_REGISTER_ACK...without a fd or a FD_LOOKUP_ACK with
 * a fd)
 * This function make use of the ancillary data (control data) to pass and
 * convert file descriptors over UNIX sockets.
 * Return -1 on error, 0 on success.
 */
static inline int _odp_recv_fdserver_msg(int sock, int *command, fd_server_context_e *context,
					 uint64_t *key, int *recvd_fd)
{
	struct msghdr socket_message;
	struct iovec io_vector[1]; /* one msg frgmt only */
	struct cmsghdr *control_message = NULL;
	int *fd_location;
	fdserver_msg_t msg;
	char ancillary_data[CMSG_SPACE(sizeof(int))];

	memset(&socket_message, 0, sizeof(struct msghdr));
	memset(ancillary_data, 0, CMSG_SPACE(sizeof(int)));

	/* setup a place to fill in message contents */
	io_vector[0].iov_base = &msg;
	io_vector[0].iov_len = sizeof(fdserver_msg_t);
	socket_message.msg_iov = io_vector;
	socket_message.msg_iovlen = 1;

	/* provide space for the ancillary data */
	socket_message.msg_control = ancillary_data;
	socket_message.msg_controllen = CMSG_SPACE(sizeof(int));

	/* receive the message */
	if (recvmsg(sock, &socket_message, MSG_CMSG_CLOEXEC) < 0)
		return -1;

	*command = msg.command;
	*context = msg.context;
	*key = msg.key;

	/* grab the converted file descriptor (if any) */
	*recvd_fd = -1;

	if ((socket_message.msg_flags & MSG_CTRUNC) == MSG_CTRUNC)
		return 0;

	/* iterate ancillary elements to find the file descriptor: */
	for (control_message = CMSG_FIRSTHDR(&socket_message);
	     control_message != NULL;
	     control_message = CMSG_NXTHDR(&socket_message, control_message)) {
		if ((control_message->cmsg_level == SOL_SOCKET) &&
		    (control_message->cmsg_type == SCM_RIGHTS)) {
			fd_location = (int *)(void *)CMSG_DATA(control_message);
			*recvd_fd = *fd_location;
			break;
		}
	}

	return 0;
}

#ifdef __cplusplus
}
#endif

#endif
