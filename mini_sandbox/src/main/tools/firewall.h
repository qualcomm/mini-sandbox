/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */

#ifndef _FIREWALL_H
#define _FIREWALL_H

#define MAX_RULE_LENGTH 1024
#define MAX_RULES 100
#define RULES_ERR -1
#define RULES_OVERFLOW -2
#define RULES_CONFIG_ALREADY_SET -3


#define MAX_ARGS 64

#include <string>
#include <iostream>
#include <cstdint>


enum class FirewallMode : int {
  FirewallUninitialized   = 0,
  FirewallEnabled         = 1,
  FirewallDisabled        = 2,
  FirewallMaxConnections  = 3
};


struct FirewallRules {
    char rules[MAX_RULES][MAX_RULE_LENGTH];
    size_t count = 0;
    uint16_t ports[MAX_RULES];
    size_t ports_count = 0;
    int max_connections = -1;
    FirewallMode mode = FirewallMode::FirewallUninitialized;
};

int SetFirewallRule(const char* rule, FirewallRules* fw_rules);
int SetFirewallPort(uint16_t port, FirewallRules* fw_rules);
int SetMaxConnections(int max_connections, FirewallRules* fw_rules);
int ResetFirewallRules(FirewallRules* fw_rules);
void DumpRules(FirewallRules* fw_rules, std::string& filepath);
void SetPorts(FirewallRules* fw_rules);

#endif
