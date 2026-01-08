/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */

#ifndef _FIREWALL_H
#define _FIREWALL_H

#define MAX_RULE_LENGTH 1024
#define MAX_RULES 100
#define RULES_OVERFLOW -5
#define MAX_ARGS 64

#include <string>
#include <iostream>


struct FirewallRules {
    char rules[MAX_RULES][MAX_RULE_LENGTH];
    size_t count;
    int max_connections = -1;
};

int set_firewall_rule(const char* rule, FirewallRules* fw_rules);
int set_max_connections(int max_connections, FirewallRules* fw_rules);
int reset_firewall_rules(FirewallRules* fw_rules);
void DumpRules(FirewallRules* fw_rules, std::string& filepath);


#endif
