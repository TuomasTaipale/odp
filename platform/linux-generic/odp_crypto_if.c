/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026 Nokia
 */

#include <odp/autoheader_internal.h>
#include <odp/api/crypto.h>

#include <odp_crypto_if.h>
#include <odp_debug_internal.h>
#include <odp_init_internal.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CRYPTO_IMPL 8

static const crypto_fn_t *crypto_impl[MAX_CRYPTO_IMPL];
static int num_crypto_impl;

const crypto_fn_t *_odp_crypto_fn;

static void register_impl(const crypto_fn_t *fn)
{
	if (num_crypto_impl >= MAX_CRYPTO_IMPL) {
		_ODP_ERR("Too many crypto implementations\n");
		return;
	}

	crypto_impl[num_crypto_impl++] = fn;
}

/* Populate the array of available implementations. Each backend is built in
 * only when its dependencies were found at configure time. */
static void register_impls(void)
{
	register_impl(&_odp_crypto_null_fn);

#if _ODP_CRYPTO_OPENSSL
	register_impl(&_odp_crypto_openssl_fn);
#endif
#if _ODP_CRYPTO_ARMV8
	register_impl(&_odp_crypto_armv8_fn);
#endif
#if _ODP_CRYPTO_IPSECMB
	register_impl(&_odp_crypto_ipsecmb_fn);
#endif
}

int _odp_crypto_init_global(void)
{
	const char *name = getenv("ODP_CRYPTO");

	if (name == NULL || !strcmp(name, "default"))
		name = "null";

	register_impls();

	for (int i = 0; i < num_crypto_impl; i++) {
		if (!strcmp(crypto_impl[i]->name, name)) {
			_odp_crypto_fn = crypto_impl[i];
			break;
		}
	}

	if (_odp_crypto_fn == NULL) {
		_ODP_ERR("Crypto implementation '%s' not available\n", name);
		return -1;
	}

	_ODP_PRINT("Using crypto '%s'\n", _odp_crypto_fn->name);

	return _odp_crypto_fn->init_global();
}

int _odp_crypto_term_global(void)
{
	return _odp_crypto_fn->term_global();
}

int _odp_crypto_init_local(void)
{
	return _odp_crypto_fn->init_local();
}

int _odp_crypto_term_local(void)
{
	return _odp_crypto_fn->term_local();
}

int odp_crypto_capability(odp_crypto_capability_t *capa)
{
	return _odp_crypto_fn->capability(capa);
}

int odp_crypto_cipher_capability(odp_cipher_alg_t cipher,
				 odp_crypto_cipher_capability_t dst[],
				 int num_copy)
{
	return _odp_crypto_fn->cipher_capability(cipher, dst, num_copy);
}

int odp_crypto_auth_capability(odp_auth_alg_t auth,
			       odp_crypto_auth_capability_t dst[], int num_copy)
{
	return _odp_crypto_fn->auth_capability(auth, dst, num_copy);
}

int odp_crypto_session_create(const odp_crypto_session_param_t *param,
			      odp_crypto_session_t *session_out,
			      odp_crypto_ses_create_err_t *status)
{
	return _odp_crypto_fn->session_create(param, session_out, status);
}

int odp_crypto_session_destroy(odp_crypto_session_t session)
{
	return _odp_crypto_fn->session_destroy(session);
}

void odp_crypto_session_print(odp_crypto_session_t hdl)
{
	_odp_crypto_fn->session_print(hdl);
}

int odp_crypto_op(const odp_packet_t pkt_in[], odp_packet_t pkt_out[],
		  const odp_crypto_packet_op_param_t param[], int num_pkt)
{
	return _odp_crypto_fn->op(pkt_in, pkt_out, param, num_pkt);
}

int odp_crypto_op_enq(const odp_packet_t pkt_in[],
		      const odp_packet_t pkt_out[],
		      const odp_crypto_packet_op_param_t param[], int num_pkt)
{
	return _odp_crypto_fn->op_enq(pkt_in, pkt_out, param, num_pkt);
}

void odp_crypto_session_param_init(odp_crypto_session_param_t *param)
{
	memset(param, 0, sizeof(odp_crypto_session_param_t));
	param->op_type = ODP_CRYPTO_OP_TYPE_BASIC;
}

uint64_t odp_crypto_session_to_u64(odp_crypto_session_t hdl)
{
	return (uint64_t)hdl;
}
