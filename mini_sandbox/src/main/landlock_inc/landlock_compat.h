/* 
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Purpose:
 *   Minimal, MIT-licensed Landlock userspace ABI compatibility layer.
 *   - Does NOT include or vendor <linux/landlock.h>
 *   - Declares the syscall ABI structs/constants needed by userspace
 *   - Provides syscall wrappers and runtime probing/masking helpers
 */

#ifndef LANDLOCK_COMPAT_H
#define LANDLOCK_COMPAT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>

#if defined(__linux__)
  #include <sys/syscall.h>
  #include <sys/prctl.h>
#endif

/* ---- ABI integer aliases (match kernel UAPI widths) ---- */
typedef uint64_t ll_u64;
typedef int32_t  ll_s32;

/* ---- Ruleset attribute (matches the ABI layout you referenced) ---- */
struct landlock_ruleset_attr {
  ll_u64 handled_access_fs;
  ll_u64 handled_access_net;
  ll_u64 scoped;
};

/* ---- landlock_create_ruleset flags ---- */
#define LANDLOCK_CREATE_RULESET_VERSION  (1U << 0)
#define LANDLOCK_CREATE_RULESET_ERRATA   (1U << 1)

/* ---- landlock_restrict_self flags (logging knobs) ---- */
#define LANDLOCK_RESTRICT_SELF_LOG_SAME_EXEC_OFF    (1U << 0)
#define LANDLOCK_RESTRICT_SELF_LOG_NEW_EXEC_ON      (1U << 1)
#define LANDLOCK_RESTRICT_SELF_LOG_SUBDOMAINS_OFF   (1U << 2)

/* ---- Rule types ---- */
enum landlock_rule_type {
  LANDLOCK_RULE_PATH_BENEATH = 1,
  LANDLOCK_RULE_NET_PORT     = 2,
};

/* ---- Rule attributes ---- */
#if defined(__GNUC__) || defined(__clang__)
  #define LL_PACKED __attribute__((packed))
#else
  #define LL_PACKED
#endif

struct landlock_path_beneath_attr {
  ll_u64 allowed_access;
  ll_s32 parent_fd;
} LL_PACKED;

struct landlock_net_port_attr {
  ll_u64 allowed_access;
  ll_u64 port; /* host endianness */
};

/* ---- Filesystem access rights ---- */
#define LANDLOCK_ACCESS_FS_EXECUTE      (1ULL << 0)
#define LANDLOCK_ACCESS_FS_WRITE_FILE   (1ULL << 1)
#define LANDLOCK_ACCESS_FS_READ_FILE    (1ULL << 2)
#define LANDLOCK_ACCESS_FS_READ_DIR     (1ULL << 3)
#define LANDLOCK_ACCESS_FS_REMOVE_DIR   (1ULL << 4)
#define LANDLOCK_ACCESS_FS_REMOVE_FILE  (1ULL << 5)
#define LANDLOCK_ACCESS_FS_MAKE_CHAR    (1ULL << 6)
#define LANDLOCK_ACCESS_FS_MAKE_DIR     (1ULL << 7)
#define LANDLOCK_ACCESS_FS_MAKE_REG     (1ULL << 8)
#define LANDLOCK_ACCESS_FS_MAKE_SOCK    (1ULL << 9)
#define LANDLOCK_ACCESS_FS_MAKE_FIFO    (1ULL << 10)
#define LANDLOCK_ACCESS_FS_MAKE_BLOCK   (1ULL << 11)
#define LANDLOCK_ACCESS_FS_MAKE_SYM     (1ULL << 12)
#define LANDLOCK_ACCESS_FS_REFER        (1ULL << 13)
#define LANDLOCK_ACCESS_FS_TRUNCATE     (1ULL << 14)
#define LANDLOCK_ACCESS_FS_IOCTL_DEV    (1ULL << 15)

/* ---- Network access rights (ABI v4+) ---- */
#define LANDLOCK_ACCESS_NET_BIND_TCP    (1ULL << 0)
#define LANDLOCK_ACCESS_NET_CONNECT_TCP (1ULL << 1)

/* ---- Scope flags (ABI v6+) ---- */
#define LANDLOCK_SCOPE_ABSTRACT_UNIX_SOCKET (1ULL << 0)
#define LANDLOCK_SCOPE_SIGNAL               (1ULL << 1)

/* ---- Minimal ABI-aware “supported mask” helpers ----
 * Notes:
 * - Landlock FS rights evolved across ABIs; these cutoffs are a pragmatic
 *   compatibility policy used by userspace libraries.
 * - You should still treat runtime failures as authoritative.
 */
static inline ll_u64 ll_supported_fs_mask_for_abi(int abi) {
  /* ABI v1: base FS flags up to MAKE_SYM */
  ll_u64 m = 0;
  if (abi >= 1) {
    m |= LANDLOCK_ACCESS_FS_EXECUTE
      |  LANDLOCK_ACCESS_FS_WRITE_FILE
      |  LANDLOCK_ACCESS_FS_READ_FILE
      |  LANDLOCK_ACCESS_FS_READ_DIR
      |  LANDLOCK_ACCESS_FS_REMOVE_DIR
      |  LANDLOCK_ACCESS_FS_REMOVE_FILE
      |  LANDLOCK_ACCESS_FS_MAKE_CHAR
      |  LANDLOCK_ACCESS_FS_MAKE_DIR
      |  LANDLOCK_ACCESS_FS_MAKE_REG
      |  LANDLOCK_ACCESS_FS_MAKE_SOCK
      |  LANDLOCK_ACCESS_FS_MAKE_FIFO
      |  LANDLOCK_ACCESS_FS_MAKE_BLOCK
      |  LANDLOCK_ACCESS_FS_MAKE_SYM;
  }
  /* ABI v2: REFER */
  if (abi >= 2) {
    m |= LANDLOCK_ACCESS_FS_REFER;
  }
  /* ABI v3: TRUNCATE */
  if (abi >= 3) {
    m |= LANDLOCK_ACCESS_FS_TRUNCATE;
  }
  /* ABI v5 (or later in some kernels): IOCTL_DEV (keep gated conservatively) */
  if (abi >= 5) {
    m |= LANDLOCK_ACCESS_FS_IOCTL_DEV;
  }
  return m;
}

static inline ll_u64 ll_supported_net_mask_for_abi(int abi) {
  ll_u64 m = 0;
  if (abi >= 4) {
    m |= LANDLOCK_ACCESS_NET_BIND_TCP | LANDLOCK_ACCESS_NET_CONNECT_TCP;
  }
  return m;
}

static inline ll_u64 ll_supported_scope_mask_for_abi(int abi) {
  ll_u64 m = 0;
  if (abi >= 6) {
    m |= LANDLOCK_SCOPE_ABSTRACT_UNIX_SOCKET | LANDLOCK_SCOPE_SIGNAL;
  }
  return m;
}

/* ---- Syscall number bridging (Ubuntu18/glibc may miss SYS_landlock_*) ---- */
#if !defined(__linux__)
  /* Non-Linux: compile stubs that behave like “not supported”. */
  #define LL_HAVE_LANDLOCK_SYSCALLS 0
#else
  #define LL_HAVE_LANDLOCK_SYSCALLS 1
#endif

#if LL_HAVE_LANDLOCK_SYSCALLS

  #ifndef SYS_landlock_create_ruleset
    #ifdef __NR_landlock_create_ruleset
      #define SYS_landlock_create_ruleset __NR_landlock_create_ruleset
    #endif
  #endif

  #ifndef SYS_landlock_add_rule
    #ifdef __NR_landlock_add_rule
      #define SYS_landlock_add_rule __NR_landlock_add_rule
    #endif
  #endif

  #ifndef SYS_landlock_restrict_self
    #ifdef __NR_landlock_restrict_self
      #define SYS_landlock_restrict_self __NR_landlock_restrict_self
    #endif
  #endif

#endif /* LL_HAVE_LANDLOCK_SYSCALLS */

/* ---- Raw syscall wrappers ---- */

static inline int ll_landlock_create_ruleset(
    const struct landlock_ruleset_attr* attr, size_t size, uint32_t flags)
{
#if LL_HAVE_LANDLOCK_SYSCALLS && defined(SYS_landlock_create_ruleset)
  return (int)syscall(SYS_landlock_create_ruleset, attr, size, flags);
#else
  (void)attr; (void)size; (void)flags;
  errno = ENOSYS;
  return -1;
#endif
}

static inline int ll_landlock_add_rule(
    int ruleset_fd, enum landlock_rule_type rule_type, const void* rule_attr, uint32_t flags)
{
#if LL_HAVE_LANDLOCK_SYSCALLS && defined(SYS_landlock_add_rule)
  return (int)syscall(SYS_landlock_add_rule, ruleset_fd, rule_type, rule_attr, flags);
#else
  (void)ruleset_fd; (void)rule_type; (void)rule_attr; (void)flags;
  errno = ENOSYS;
  return -1;
#endif
}

static inline int ll_landlock_restrict_self(int ruleset_fd, uint32_t flags)
{
#if LL_HAVE_LANDLOCK_SYSCALLS && defined(SYS_landlock_restrict_self)
  return (int)syscall(SYS_landlock_restrict_self, ruleset_fd, flags);
#else
  (void)ruleset_fd; (void)flags;
  errno = ENOSYS;
  return -1;
#endif
}

/* ---- Runtime probing helpers ---- */

static inline int ll_landlock_probe_abi(void) {
  /* Returns highest supported ABI version, or -1 with errno (ENOSYS/EOPNOTSUPP/...) */
  return ll_landlock_create_ruleset(NULL, 0, LANDLOCK_CREATE_RULESET_VERSION);
}

static inline int ll_landlock_probe_errata(void) {
  /* Returns errata bitmask for current ABI, or -1 with errno */
  return ll_landlock_create_ruleset(NULL, 0, LANDLOCK_CREATE_RULESET_ERRATA);
}

/* A convenience helper used by unprivileged callers before restrict_self:
 * landlock_restrict_self requires no_new_privs or CAP_SYS_ADMIN.
 */
static inline int ll_landlock_enable_no_new_privs(void) {
#if defined(__linux__) && defined(PR_SET_NO_NEW_PRIVS)
  return prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
#else
  errno = ENOSYS;
  return -1;
#endif
}

/* Build a ruleset_attr masked to what the running kernel can support for a given ABI.
 * You still pass sizeof(struct landlock_ruleset_attr) and let the kernel validate.
 */
static inline struct landlock_ruleset_attr ll_landlock_ruleset_attr_masked(
    int abi, ll_u64 want_fs, ll_u64 want_net, ll_u64 want_scoped)
{
  struct landlock_ruleset_attr a;
  a.handled_access_fs  = want_fs    & ll_supported_fs_mask_for_abi(abi);
  a.handled_access_net = want_net   & ll_supported_net_mask_for_abi(abi);
  a.scoped             = want_scoped & ll_supported_scope_mask_for_abi(abi);
  return a;
}

#ifdef __cplusplus
}
#endif

#endif /* LANDLOCK_COMPAT_H */
