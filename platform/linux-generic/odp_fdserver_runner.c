/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2016-2018 Linaro Limited
 * Copyright (c) 2024 Nokia
 */

#include <odp_posix_extensions.h>
#include <odp_fdserver_internal.h>

#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static fdentry_t *fd_table;
static int fd_table_nb_entries;

/*
 * server function
 * receive a client request and handle it.
 * Always returns 0 unless a stop request is received.
 */
static int handle_request(int client_sock)
{
	int command = 0;
	fd_server_context_e context;
	uint64_t key = 0;
	int fd = -1;
	int i;

	memset(&context, 0, sizeof(context));
	/* get a client request: */
	_odp_recv_fdserver_msg(client_sock, &command, &context, &key, &fd);
	switch (command) {
	case FD_REGISTER_REQ:
		if ((fd < 0) || (context >= FD_SRV_CTX_END)) {
			_odp_send_fdserver_msg(client_sock, FD_REGISTER_NACK, FD_SRV_CTX_NA, 0,
					       -1);
			return 0;
		}

		/* store the file descriptor in table: */
		if (fd_table_nb_entries < FDSERVER_MAX_ENTRIES) {
			fd_table[fd_table_nb_entries].context = context;
			fd_table[fd_table_nb_entries].key     = key;
			fd_table[fd_table_nb_entries++].fd    = fd;
		} else {
			_odp_send_fdserver_msg(client_sock, FD_REGISTER_NACK, FD_SRV_CTX_NA, 0,
					       -1);
			return 0;
		}

		_odp_send_fdserver_msg(client_sock, FD_REGISTER_ACK, FD_SRV_CTX_NA, 0, -1);
		break;

	case FD_LOOKUP_REQ:
		if (context >= FD_SRV_CTX_END) {
			_odp_send_fdserver_msg(client_sock, FD_LOOKUP_NACK, FD_SRV_CTX_NA, 0, -1);
			return 0;
		}

		/* search key in table and sent reply: */
		for (i = 0; i < fd_table_nb_entries; i++) {
			if ((fd_table[i].context == context) &&
			    (fd_table[i].key == key)) {
				fd = fd_table[i].fd;
				_odp_send_fdserver_msg(client_sock, FD_LOOKUP_ACK, context, key,
						       fd);
				return 0;
			}
		}

		/* context+key not found... send nack */
		_odp_send_fdserver_msg(client_sock, FD_LOOKUP_NACK, context, key, -1);
		break;

	case FD_DEREGISTER_REQ:
		if (context >= FD_SRV_CTX_END) {
			_odp_send_fdserver_msg(client_sock, FD_DEREGISTER_NACK, FD_SRV_CTX_NA, 0,
					       -1);
			return 0;
		}

		/* search key in table and remove it if found, and reply: */
		for (i = 0; i < fd_table_nb_entries; i++) {
			if ((fd_table[i].context == context) &&
			    (fd_table[i].key == key)) {
				close(fd_table[i].fd);
				fd_table[i] = fd_table[--fd_table_nb_entries];
				_odp_send_fdserver_msg(client_sock, FD_DEREGISTER_ACK, context,
						       key, -1);
				return 0;
			}
		}

		/* key not found... send nack */
		_odp_send_fdserver_msg(client_sock, FD_DEREGISTER_NACK, context, key, -1);
		break;

	case FD_SERVERSTOP_REQ:
		return 1;

	default:
		break;
	}
	return 0;
}

/*
 * server function
 * loop forever, handling client requests one by one
 */
static void wait_requests(int sock)
{
	int c_socket; /* client connection */
	unsigned int addr_sz;
	struct sockaddr_un remote;

	for (;;) {
		addr_sz = sizeof(remote);
		c_socket = accept(sock, (struct sockaddr *)&remote, &addr_sz);
		if (c_socket == -1) {
			if (errno == EINTR)
				continue;

			return;
		}

		if (handle_request(c_socket))
			break;
		close(c_socket);
	}
	close(c_socket);
}

int main(int argc ODP_UNUSED, char **argv)
{
	sigset_t sigset;
	struct sigaction action;
	int res;
	int sock = (int)(uintptr_t)argv[1];

	sigfillset(&sigset);
	/* undefined if these are ignored, as per POSIX */
	sigdelset(&sigset, SIGFPE);
	sigdelset(&sigset, SIGILL);
	sigdelset(&sigset, SIGSEGV);
	/* can not be masked */
	sigdelset(&sigset, SIGKILL);
	sigdelset(&sigset, SIGSTOP);
	/* these we want to handle */
	sigdelset(&sigset, SIGTERM);
	if (sigprocmask(SIG_SETMASK, &sigset, NULL) == -1)
		return EXIT_FAILURE;

	/* set default handlers for those signals we can handle */
	memset(&action, 0, sizeof(action));
	action.sa_handler = SIG_DFL;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	sigaction(SIGFPE, &action, NULL);
	sigaction(SIGILL, &action, NULL);
	sigaction(SIGSEGV, &action, NULL);
	sigaction(SIGTERM, &action, NULL);

	/* TODO: pin the server on appropriate service cpu mask */
	/* when (if) we can agree on the usage of service mask  */

	/* request to be killed if parent dies, hence avoiding  */
	/* orphans being "adopted" by the init process...	*/
	prctl(PR_SET_PDEATHSIG, SIGTERM);

	res = setsid();
	if (res == -1)
		return EXIT_FAILURE;

	/* allocate the space for the file descriptor<->key table: */
	fd_table = malloc(FDSERVER_MAX_ENTRIES * sizeof(fdentry_t));
	if (!fd_table)
		return EXIT_FAILURE;

	/* wait for clients requests */
	wait_requests(sock); /* Returns when server is stopped  */
	close(sock);

	/* release the file descriptor table: */
	free(fd_table);

	return EXIT_SUCCESS;
}
