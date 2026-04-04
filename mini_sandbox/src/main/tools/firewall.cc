/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <libgen.h>

#include "src/main/tools/firewall.h"
#include "src/main/tools/constants.h"



int SetMaxConnections(int max_connections, FirewallRules* fw_rules) {
  fw_rules->mode = FirewallMode::FirewallMaxConnections;
  fw_rules->max_connections = max_connections;
  return 0;
}


int SetFirewallRule(const char *rule, FirewallRules *fw_rules) {
  if (fw_rules->count > MAX_RULES) 
    return RULES_OVERFLOW;
  
  size_t rule_len = strlen(rule);
  if (rule_len > MAX_RULE_LENGTH)
    return RULES_OVERFLOW;
  
  if (fw_rules->mode == FirewallMode::FirewallDisabled)
    return RULES_CONFIG_ALREADY_SET;


  fw_rules->mode = FirewallMode::FirewallEnabled;
  memcpy(fw_rules->rules[fw_rules->count], rule, rule_len);
  fw_rules->count++;
  return 0;
}

int SetFirewallPort(uint16_t port, FirewallRules* fw_rules) {
  if (fw_rules->ports_count > MAX_RULES)
    return RULES_OVERFLOW;

  if (fw_rules->mode == FirewallMode::FirewallDisabled)
    return RULES_CONFIG_ALREADY_SET;

  fw_rules->mode = FirewallMode::FirewallEnabled;
  fw_rules->ports[fw_rules->ports_count] = port;
  fw_rules->ports_count++;
  return 0;
}

int ResetFirewallRules(FirewallRules *fw_rules) {
  if (fw_rules->mode == FirewallMode::FirewallEnabled)
    return RULES_CONFIG_ALREADY_SET;
  fw_rules->mode = FirewallMode::FirewallDisabled;
  fw_rules->count = 0; 
  return 0;
}


void SetPorts(FirewallRules* fw_rules) {
  if (fw_rules->ports_count != 0)
    return;
  for (int i = 0; i < DEFAULT_PORTS; i++) {
    fw_rules->ports[i] = kDefaultPorts[i];
    fw_rules->ports_count += 1;
  }
}




// This method dumps the policy that the minitap backend binary
// will use to setup the firewall rules and, if specified, 
// the max number of connections
// The two policies are exclusive, i.e., either you set up
// a maximum number of connections OR you set up the firewall 
// rules, at least for now.
void DumpRules(FirewallRules* fw_rules, std::string& filepath) { 

  if (fw_rules == NULL || filepath.empty()) {
    return;
  }

  FILE* file = fopen(filepath.c_str(), "w");
  if (file == NULL) {
    perror("Failed to open file");
    return;
  }

  // The first line stores the number of connections allowed in the sandbox. 
  // A negative number means the value will be ignored
  fprintf(file, "%d\n", fw_rules->max_connections);

  // if max_connections >= 0 we only enforce the `max_connections` policy. 
  // if max_connections < 0 (which is the default value) we look at the firewall
  // rules and dump them
  if (fw_rules->max_connections < 0) {

  // Then we dump all the IP addresses/ domain names / subnets we wanna 
  // allow in the new network
    for (size_t i = 0; i < fw_rules->count; ++i) {
      if (fw_rules->rules[i][0] != '\0') {
        fprintf(file, "%s\n", fw_rules->rules[i]);
      }
    }
  }

  fclose(file);  
}
