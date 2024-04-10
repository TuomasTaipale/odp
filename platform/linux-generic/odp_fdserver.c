/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2016-2018 Linaro Limited
 */

/*
 * This file implements a file descriptor sharing server enabling
 * sharing of file descriptors between processes, regardless of fork time.
 *
 * File descriptors are process scoped, but they can be "sent and converted
 * on the fly" between processes using special unix domain socket ancillary
 * data.
 * The receiving process gets a file descriptor "pointing" to the same thing
 * as the one sent (but the value of the file descriptor itself may be different
 * from the one sent).
 * Because ODP applications are responsible for creating ODP threads (i.e.
 * pthreads or linux processes), ODP has no control on the order things happen:
 * Nothing prevent a thread A to fork B and C, and then C creating a pktio
 * which will be used by A and B to send/receive packets.
 * Assuming this pktio uses a file descriptor, the latter will need to be
 * shared between the processes, despite the "non convenient" fork time.
 * The shared memory allocator is likely to use this as well to be able to
 * share memory regardless of fork() time.
 * This server handles a table of {(context,key)<-> fd} pair, and is
 * interfaced by the following functions:
 *
 * _odp_fdserver_register_fd(context, key, fd_to_send);
 * _odp_fdserver_deregister_fd(context, key);
 * _odp_fdserver_lookup_fd(context, key);
 *
 * which are used to register/deregister or query for file descriptor based
 * on a context and key value couple, which has to be unique.
 *
 * Note again that the file descriptors stored here are local to this server
 * process and get converted both when registered or looked up.
 */

#include <odp_posix_extensions.h>
#include <odp_global_data.h>
#include <odp_init_internal.h>
#include <odp_debug_internal.h>
#include <odp_fdserver_internal.h>
#include <sys/prctl.h>
#include <signal.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/wait.h>

#define FDSERVER_SOCKPATH_MAXLEN 255
#define FDSERVER_SOCK_FORMAT "%s/%s/odp-%d-fdserver"
#define FDSERVER_SOCKDIR_FORMAT "%s/%s"
#define FDSERVER_DEFAULT_DIR "/dev/shm"
#define FDSERVER_BACKLOG 5

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

/* opens and returns a connected socket to the server */
static int get_socket(void)
{
	char sockpath[FDSERVER_SOCKPATH_MAXLEN];
	int s_sock; /* server socket */
	struct sockaddr_un remote;
	int len;

	/* construct the named socket path: */
	len = snprintf(sockpath, FDSERVER_SOCKPATH_MAXLEN, FDSERVER_SOCK_FORMAT,
		       odp_global_ro.shm_dir, odp_global_ro.uid,
		       odp_global_ro.main_pid);

	if (len >= FDSERVER_SOCKPATH_MAXLEN || len >= (int)sizeof(remote.sun_path)) {
		_ODP_ERR("path too long\n");
		return -1;
	}

	s_sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s_sock == -1) {
		_ODP_ERR("cannot connect to server: %s\n", strerror(errno));
		return -1;
	}

	remote.sun_family = AF_UNIX;
	strcpy(remote.sun_path, sockpath);
	len = strlen(remote.sun_path) + sizeof(remote.sun_family);
	while (connect(s_sock, (struct sockaddr *)&remote, len) == -1) {
		if (errno == EINTR)
			continue;
		_ODP_ERR("cannot connect to server: %s\n", strerror(errno));
		close(s_sock);
		return -1;
	}

	return s_sock;
}

/*
 * Client function:
 * Register a file descriptor to the server. Return -1 on error.
 */
int _odp_fdserver_register_fd(fd_server_context_e context, uint64_t key,
			      int fd_to_send)
{
	int s_sock; /* server socket */
	int res;
	int command;
	int fd;

	ODP_DBG_LVL(FD_DBG, "FD client register: pid=%d key=%" PRIu64 ", fd=%d\n",
		    getpid(), key, fd_to_send);

	s_sock = get_socket();
	if (s_sock < 0)
		return -1;

	res =  _odp_send_fdserver_msg(s_sock, FD_REGISTER_REQ, context, key, fd_to_send);
	if (res < 0) {
		_ODP_ERR("fd registration failure\n");
		close(s_sock);
		return -1;
	}

	res = _odp_recv_fdserver_msg(s_sock, &command, &context, &key, &fd);

	if ((res < 0) || (command != FD_REGISTER_ACK)) {
		_ODP_ERR("fd registration failure\n");
		close(s_sock);
		return -1;
	}

	close(s_sock);

	return 0;
}

/*
 * Client function:
 * Deregister a file descriptor from the server. Return -1 on error.
 */
int _odp_fdserver_deregister_fd(fd_server_context_e context, uint64_t key)
{
	int s_sock; /* server socket */
	int res;
	int command;
	int fd;

	ODP_DBG_LVL(FD_DBG, "FD client deregister: pid=%d key=%" PRIu64 "\n",
		    getpid(), key);

	s_sock = get_socket();
	if (s_sock < 0)
		return -1;

	res =  _odp_send_fdserver_msg(s_sock, FD_DEREGISTER_REQ, context, key, -1);
	if (res < 0) {
		_ODP_ERR("fd de-registration failure\n");
		close(s_sock);
		return -1;
	}

	res = _odp_recv_fdserver_msg(s_sock, &command, &context, &key, &fd);

	if ((res < 0) || (command != FD_DEREGISTER_ACK)) {
		_ODP_ERR("fd de-registration failure\n");
		close(s_sock);
		return -1;
	}

	close(s_sock);

	return 0;
}

/*
 * client function:
 * lookup a file descriptor from the server. return -1 on error,
 * or the file descriptor on success (>=0).
 */
int _odp_fdserver_lookup_fd(fd_server_context_e context, uint64_t key)
{
	int s_sock; /* server socket */
	int res;
	int command;
	int fd;

	s_sock = get_socket();
	if (s_sock < 0)
		return -1;

	res =  _odp_send_fdserver_msg(s_sock, FD_LOOKUP_REQ, context, key, -1);
	if (res < 0) {
		_ODP_ERR("fd lookup failure\n");
		close(s_sock);
		return -1;
	}

	res = _odp_recv_fdserver_msg(s_sock, &command, &context, &key, &fd);

	if ((res < 0) || (command != FD_LOOKUP_ACK)) {
		_ODP_ERR("fd lookup failure\n");
		close(s_sock);
		return -1;
	}

	close(s_sock);
	_ODP_DBG("FD client lookup: pid=%d, key=%" PRIu64 ", fd=%d\n",
		 getpid(), key, fd);

	return fd;
}

/*
 * request server termination:
 */
static int stop_server(void)
{
	int s_sock; /* server socket */
	int res;

	ODP_DBG_LVL(FD_DBG, "FD sending server stop request\n");

	s_sock = get_socket();
	if (s_sock < 0)
		return -1;

	res =  _odp_send_fdserver_msg(s_sock, FD_SERVERSTOP_REQ, 0, 0, -1);
	if (res < 0) {
		_ODP_ERR("fd stop request failure\n");
		close(s_sock);
		return -1;
	}

	close(s_sock);

	return 0;
}

/*
 * Create a unix domain socket and fork a process to listen to incoming
 * requests.
 */
int _odp_fdserver_init_global(void)
{
	char sockpath[FDSERVER_SOCKPATH_MAXLEN];
	int sock;
	struct sockaddr_un local;
	pid_t server_pid;
	int len, res;

	snprintf(sockpath, FDSERVER_SOCKPATH_MAXLEN, FDSERVER_SOCKDIR_FORMAT,
		 odp_global_ro.shm_dir,
		 odp_global_ro.uid);

	mkdir(sockpath, 0744);

	/* construct the server named socket path: */
	len = snprintf(sockpath, FDSERVER_SOCKPATH_MAXLEN, FDSERVER_SOCK_FORMAT,
		       odp_global_ro.shm_dir, odp_global_ro.uid,
		       odp_global_ro.main_pid);

	if (len >= FDSERVER_SOCKPATH_MAXLEN || len >= (int)sizeof(local.sun_path)) {
		_ODP_ERR("path too long\n");
		return -1;
	}

	/* create UNIX domain socket: */
	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock == -1) {
		_ODP_ERR("socket() failed: %s\n", strerror(errno));
		return -1;
	}

	/* remove previous named socket if it already exists: */
	unlink(sockpath);

	/* bind to new named socket: */
	local.sun_family = AF_UNIX;
	memcpy(local.sun_path, sockpath, sizeof(local.sun_path));
	local.sun_path[sizeof(local.sun_path) - 1] = '\0';

	res = bind(sock, (struct sockaddr *)&local, sizeof(struct sockaddr_un));
	if (res == -1) {
		_ODP_ERR("bind() failed: %s\n", strerror(errno));
		close(sock);
		return -1;
	}

	/* listen for incoming connections: */
	if (listen(sock, FDSERVER_BACKLOG) == -1) {
		_ODP_ERR("listen() failed: %s\n", strerror(errno));
		close(sock);
		return -1;
	}

	/* fork a server process: */
	server_pid = fork();
	if (server_pid == -1) {
		_ODP_ERR("Could not fork!\n");
		close(sock);
		return -1;
	}

	if (server_pid == 0) { /*child */
		/* duplicate the server socket to something easily resolvable */
		dup2(sock, STDIN_FILENO);
		close(sock);
		execl(FDSERVER_PATH, FDSERVER_PATH, (char *)NULL);
	}

	/* parent */
	odp_global_ro.fdserver_pid = server_pid;
	close(sock);
	return 0;
}

/*
 * Terminate the server
 */
int _odp_fdserver_term_global(void)
{
	int status;
	pid_t pid;
	char sockpath[FDSERVER_SOCKPATH_MAXLEN];

	/* close fdserver and wait for it to terminate */
	if (stop_server()) {
		_ODP_ERR("Server stop failed\n");
		return -1;
	}

	_ODP_DBG("Waiting for fdserver (%i) to stop\n", odp_global_ro.fdserver_pid);
	pid = waitpid(odp_global_ro.fdserver_pid, &status, 0);

	if (pid != odp_global_ro.fdserver_pid)
		_ODP_ERR("Failed to wait for fdserver\n");

	/* construct the server named socket path: */
	snprintf(sockpath, FDSERVER_SOCKPATH_MAXLEN, FDSERVER_SOCK_FORMAT,
		 odp_global_ro.shm_dir,
		 odp_global_ro.uid,
		 odp_global_ro.main_pid);

	/* delete the UNIX domain socket: */
	unlink(sockpath);

	/* delete shm files directory */
	snprintf(sockpath, FDSERVER_SOCKPATH_MAXLEN, FDSERVER_SOCKDIR_FORMAT,
		 odp_global_ro.shm_dir,
		 odp_global_ro.uid);
	rmdir(sockpath);

	return 0;
}
