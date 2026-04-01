/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */


#ifndef SRC_MAIN_TOOLS_CONSTANTS_H_
#define SRC_MAIN_TOOLS_CONSTANTS_H_

// The isolation mode that it's going to be used if this env var is specified, 
// "namespaces", "landlock" or "capabilities"
inline constexpr const char kIsolationModeEnv[] = "MINI_SANDBOX_ISOLATION_MODE";

// The path of minitap binary in case it's not in PATH or ./ or in the folder of the shared
//object
inline constexpr const char kMiniTapBinaryEnv[] = "MINI_SANDBOX_TAP_BINARY";

// The following env variables are helpers that tell mini-sandbox if we are running in 
// unprivileged or privileged Docker container
inline constexpr const char kDockerUnprivilegedEnv[] = "MINI_SANDBOX_DOCKER_UNPRIVILEGED";
inline constexpr const char kDockerPrivilegedEnv[] = "MINI_SANDBOX_DOCKER_PRIVILEGED";

// Internal env to check if we are running inside mini-sandbox
inline constexpr const char kInternalMiniSandboxEnv[] = "__INTERNAL_MINI_SANDBOX_ON";

// Some constants values used in the code
inline constexpr const char kInitStatus[] = "1\n";
inline constexpr const char kDev[] = "/dev";
inline constexpr const char kProc[] = "/proc";
inline constexpr const char kDevPts[] = "/dev/pts";
inline constexpr const char kTmp[] = "/tmp";
inline constexpr const char kDockerPath[] = "/.dockerenv";
inline constexpr const char kMounts[] = "/proc/self/mounts";

inline const char *devs[] = {"/dev/null", "/dev/random", "/dev/urandom", "/dev/zero",
                      "/dev/full", "/dev/tty", "/dev/console", NULL };

#define DEV_LINKS 4
static const struct {
    const char *link_path;
    const char *target;
} links[DEV_LINKS] = {
    { "dev/fd",     "/proc/self/fd"   },
    { "dev/stdin",  "/proc/self/fd/0" },
    { "dev/stdout", "/proc/self/fd/1" },
    { "dev/stderr", "/proc/self/fd/2" },
};


enum class MiniSbxExecMode {
  CLI,
  LIB,
};

#define DEFAULT_PORTS 3
inline constexpr const uint16_t kDefaultPorts[3] = { 80, 443, 53 };
#endif
