/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */
#ifndef MINITAP_INTERFACE_H
#define MINITAP_INTERFACE_H

// This calls execve() on the binary that sets up the TCP/IP
// stack. Need uid and gid cause we'll need to run in a user 
// namespace as 'fake root'. The third parameter can be empty
// if we do not want to set any specific network restriction
// via firewall
int RunTCPIP(uid_t uid, gid_t gid, std::string& rules);

#endif
