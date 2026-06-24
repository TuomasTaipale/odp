/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026 Nokia
 */

#ifndef ODP_CRYPTO_IF_H_
#define ODP_CRYPTO_IF_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/api/crypto.h>
#include <odp/api/packet.h>

/* Interface that each crypto implementation provides. The selected
 * implementation backs the public odp_crypto_*() API at runtime. */
typedef struct crypto_fn_t {
	const char *name;

	int (*init_global)(void);
	int (*term_global)(void);
	int (*init_local)(void);
	int (*term_local)(void);

	int (*capability)(odp_crypto_capability_t *capa);
	int (*cipher_capability)(odp_cipher_alg_t cipher,
				 odp_crypto_cipher_capability_t dst[],
				 int num_copy);
	int (*auth_capability)(odp_auth_alg_t auth,
			       odp_crypto_auth_capability_t dst[],
			       int num_copy);

	int (*session_create)(const odp_crypto_session_param_t *param,
			      odp_crypto_session_t *session_out,
			      odp_crypto_ses_create_err_t *status);
	int (*session_destroy)(odp_crypto_session_t session);
	void (*session_print)(odp_crypto_session_t hdl);

	int (*op)(const odp_packet_t pkt_in[], odp_packet_t pkt_out[],
		  const odp_crypto_packet_op_param_t param[], int num_pkt);
	int (*op_enq)(const odp_packet_t pkt_in[], const odp_packet_t pkt_out[],
		      const odp_crypto_packet_op_param_t param[], int num_pkt);

} crypto_fn_t;

/* The selected crypto implementation. Set during global init. */
extern const crypto_fn_t *_odp_crypto_fn;

/* Implementations provided by the compiled-in backends. Only those whose
 * dependencies were available at build time are defined and linked in. */
extern const crypto_fn_t _odp_crypto_null_fn;
extern const crypto_fn_t _odp_crypto_openssl_fn;
extern const crypto_fn_t _odp_crypto_armv8_fn;
extern const crypto_fn_t _odp_crypto_ipsecmb_fn;

#ifdef __cplusplus
}
#endif

#endif
