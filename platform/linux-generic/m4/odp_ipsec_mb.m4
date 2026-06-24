# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2022 ARM Limited
#

#########################################################################
# Check for libIPSec_MB availability
#########################################################################
ipsecmb_support=no
AC_CHECK_HEADERS([ipsec-mb.h],
        [AC_CHECK_LIB([IPSec_MB], [init_mb_mgr_auto], [ipsecmb_support=yes],
                [ipsecmb_support=no])],
        [ipsecmb_support=no])

AS_IF([test "x$require_ipsecmb" = "xyes" -a "x$ipsecmb_support" = "xno"],
      [AC_MSG_ERROR([IPSec MB library not found on this platform])])

if test "x$ipsecmb_support" = "xyes"; then
        IPSEC_MB_LIBS="-lIPSec_MB"
        ipsecmb_def=1
else
        IPSEC_MB_LIBS=""
        ipsecmb_def=0
fi

AC_SUBST([IPSEC_MB_LIBS])

AC_DEFINE_UNQUOTED([_ODP_CRYPTO_IPSECMB], [$ipsecmb_def],
                   [Define to 1 to build in the IPSec MB crypto implementation])
