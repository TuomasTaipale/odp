#!/bin/bash
set -e

echo 1500 | tee /proc/sys/vm/nr_hugepages
mkdir -p /mnt/huge
mount -t hugetlbfs nodev /mnt/huge

"`dirname "$0"`"/build_${ARCH}.sh

cd "$(dirname "$0")"/../..

# Crypto defaults to the null implementation at runtime. Select the OpenSSL
# implementation when it was built in, so that crypto and IPsec tests keep
# exercising real algorithms. An externally set ODP_CRYPTO is respected.
if [ -z "${ODP_CRYPTO}" ] && \
   grep -q "define _ODP_CRYPTO_OPENSSL 1" include/odp/autoheader_internal.h 2>/dev/null; then
	export ODP_CRYPTO=openssl
fi

# Ignore possible failures there because these tests depends on measurements
# and systems might differ in performance.
export CI="true"
make check

umount /mnt/huge
