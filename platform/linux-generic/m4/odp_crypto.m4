# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2022 ARM Limited
# Copyright (c) 2022 Nokia
#

# ODP_CRYPTO
# ----------
# Compile in every crypto implementation whose dependencies are present. The
# implementation used at runtime is selected with the ODP_CRYPTO environment
# variable and defaults to null. Implementations whose libraries are missing
# are not built in and thus not registered as available at runtime.
#
# --with-crypto is kept for backward compatibility: when set to a specific
# implementation, that implementation is required (configure fails if its
# dependencies are missing). It no longer disables the other implementations.
AC_ARG_WITH([crypto],
            [AS_HELP_STRING([--with-crypto],
                            [require a specific crypto implementation to be built in]
                            [(openssl/armv8crypto/ipsecmb/null) (linux-generic)])],
            [], [with_crypto=auto])

AS_IF([test "x$with_crypto" = "xyes"], [with_crypto=openssl])
AS_IF([test "x$with_crypto" = "xno"], [with_crypto=null])

AS_IF([test "x$with_crypto" != "xauto" -a "x$with_crypto" != "xopenssl" -a "x$with_crypto" != "xarmv8crypto" -a "x$with_crypto" != "xipsecmb" -a "x$with_crypto" != "xnull"],
      [AC_MSG_ERROR([Invalid crypto implementation name])])

##########################################################################
# OpenSSL implementation
##########################################################################
# Available whenever the OpenSSL crypto library was found.
AS_IF([test "x$with_crypto" = "xopenssl" -a "x$have_openssl" != "x1"],
      [AC_MSG_ERROR([OpenSSL crypto requested but OpenSSL is not available])])

AC_DEFINE_UNQUOTED([_ODP_CRYPTO_OPENSSL], [$have_openssl],
                   [Define to 1 to build in the OpenSSL crypto implementation])

AC_CONFIG_COMMANDS_PRE([dnl
AM_CONDITIONAL([WITH_OPENSSL_CRYPTO], [test "x$have_openssl" = "x1"])
])

##########################################################################
# ARMv8 Crypto library implementation
##########################################################################
armv8crypto_support=no
PKG_CHECK_MODULES([AARCH64CRYPTO], [libAArch64crypto],
                  [armv8crypto_support=yes
                   AARCH64CRYPTO_PKG=", libAArch64crypto"
                   AC_SUBST([AARCH64CRYPTO_PKG])],
                  [armv8crypto_support=no])

AS_IF([test "x$with_crypto" = "xarmv8crypto" -a "x$armv8crypto_support" = "xno"],
      [AC_MSG_ERROR([ARMv8 crypto requested but libAArch64crypto is not available])])

AS_IF([test "x$armv8crypto_support" = "xyes"], [armv8crypto_def=1], [armv8crypto_def=0])
AC_DEFINE_UNQUOTED([_ODP_CRYPTO_ARMV8], [$armv8crypto_def],
                   [Define to 1 to build in the ARMv8 crypto implementation])

AC_CONFIG_COMMANDS_PRE([dnl
AM_CONDITIONAL([WITH_ARMV8_CRYPTO], [test "x$armv8crypto_support" = "xyes"])
])

##########################################################################
# Multi-buffer IPSec library implementation
##########################################################################
# ipsecmb_support is set by odp_ipsec_mb.m4 (included after this file).
AS_IF([test "x$with_crypto" = "xipsecmb"], [require_ipsecmb=yes], [require_ipsecmb=no])
AC_SUBST([require_ipsecmb])

AC_CONFIG_COMMANDS_PRE([dnl
AM_CONDITIONAL([WITH_IPSECMB_CRYPTO], [test "x$ipsecmb_support" = "xyes"])
])
