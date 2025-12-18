/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */

#ifndef _FIREWALL_H
#define _FIREWALL_H

#define MAX_RULE_LENGTH 1024
#define MAX_RULES 100
#define SUCCESS 0
#define ERR_FILE_NOT_FOUND -1
#define ERR_PARSE_FAILED -2
#define ERR_EXEC_FAILED -3
#define RULE_LEN_OVERFLOW -4
#define RULES_OVERFLOW -5
#define MAX_ARGS 64

#define DENY_ALL "-A OUTPUT -j DROP" 
#define NFT_RULESET "/tmp/nftables_ruleset.nft"

#define MAX_IP_LEN 4*4+3
#define IP_RULE_FMT "-A OUTPUT -d %s -j ACCEPT"
#define MAX_IP_RULE_LEN MAX_IP_LEN+sizeof(IP_RULE_FMT) 

#define IP_RULE_FMT_NFT "ip daddr %s accept"
#define MAX_IP_RULE_LEN_NFT MAX_IP_LEN + sizeof(IP_RULE_FMT_NFT)

#include <string>
#include <iostream>

typedef enum {
    IPTABLES_NFT,
    NFT,
    NONE
} FirewallTool;

struct FirewallRules {
    char rules[MAX_RULES][MAX_RULE_LENGTH];
    size_t count;
    int max_connections = -1;
};

int read_firewall_rules(const char* filepath, FirewallRules* fw_rules);
int set_firewall_rule(const char* rule, FirewallRules* fw_rules);
int set_max_connections(int max_connections, FirewallRules* fw_rules);
#ifdef __cplusplus
extern "C" {
#endif
int execute_firewall_rule(const char* rule);
void execute_firewall_rules(FirewallRules* fw_rules);
char* format_rule_from_ipv4(const char* ip);
#ifdef __cplusplus
}
#endif

int reset_firewall_rules(FirewallRules* fw_rules);
void DumpRules(FirewallRules* fw_rules, std::string& filepath);


#endif
