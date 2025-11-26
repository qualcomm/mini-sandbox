#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <libgen.h>

#include "firewall.h"

FirewallTool Tool = NONE;

static FirewallTool get_firewall_tool_from_env() {
    const char* env = getenv("FIREWALL_TOOL");
    if (!env) {
        return NONE;
    }

    char* env_copy = strdup(env);
    if (!env_copy) {
        perror("strdup");
        return NONE;
    }

    char* base = basename(env_copy);

    FirewallTool result = NONE;
    if (strcmp(base, "iptables-nft") == 0) {
        result = IPTABLES_NFT;
    } else if (strcmp(base, "nft") == 0) {
        result = NFT;
    }

    free(env_copy);
    return result;
}



static FirewallTool which_nft_firewall() {
  if (Tool != NONE) {
      return Tool;
  }

  Tool = get_firewall_tool_from_env();
  if (Tool != NONE)
      return Tool;

  if (access("/usr/sbin/iptables-nft", X_OK) == 0) {
    Tool = IPTABLES_NFT;
  } else if (access("/usr/sbin/nft", X_OK) == 0) {
    Tool = NFT;
  } else
    Tool = NONE;
  return Tool;
}



int set_firewall_rule(const char *rule, FirewallRules *fw_rules) {
  FirewallTool tool = which_nft_firewall();
  if (tool == NONE)
      return ERR_FILE_NOT_FOUND;
  size_t rule_len = strlen(rule);
  if (rule_len >= MAX_RULE_LENGTH)
    return RULE_LEN_OVERFLOW;
  if (fw_rules->count >= MAX_RULES)
    return RULES_OVERFLOW;
  if (fw_rules->count == 0 && tool == IPTABLES_NFT) {
    // we add the DENY_ALL rule only if we're using iptables-nft . For nft
    // we just have the `policy drop` rule in the template
    memcpy(fw_rules->rules[fw_rules->count], DENY_ALL, sizeof(DENY_ALL));
    fw_rules->count++;
  }
  memcpy(fw_rules->rules[fw_rules->count], rule, rule_len);
  fw_rules->count++;

  return SUCCESS;
}

int reset_firewall_rules(FirewallRules *fw_rules) {
  fw_rules->count = 0;
  return SUCCESS;
}

int read_firewall_rules(const char *filepath, FirewallRules *fw_rules) {
  FILE *file = fopen(filepath, "r");
  if (!file) {
    perror("Error opening file");
    return ERR_FILE_NOT_FOUND;
  }

  fw_rules->count = 0;
  while (fgets(fw_rules->rules[fw_rules->count], MAX_RULE_LENGTH, file)) {
    if (strlen(fw_rules->rules[fw_rules->count]) < 2) // that is, \n only
      continue;
    fw_rules->rules[fw_rules->count]
                   [strcspn(fw_rules->rules[fw_rules->count], "\n")] = 0;
    fw_rules->count++;

    if (fw_rules->count >= MAX_RULES) {
      break;
    }
  }

  fclose(file);
  return SUCCESS;
}

int execute_firewall_rule(const char *rule) {
  pid_t pid = fork();

  if (pid < 0) {
    perror("Fork failed");
    return ERR_EXEC_FAILED;
  } else if (pid == 0) {
    char *args[MAX_ARGS];
    int arg_count = 0;
    args[arg_count++] = strdup("iptables-nft");
    char *rule_copy = strdup(rule);
    char *token = strtok(rule_copy, " ");
    while (token != NULL && arg_count < MAX_ARGS - 1) {
      args[arg_count++] = token;
      token = strtok(NULL, " ");
    }
    args[arg_count] = NULL;
    execvp("iptables-nft", args);
    exit(0);
  } else {

    int status;
    if (waitpid(pid, &status, 0) == -1) {
          perror("waitpid failed");
          exit(EXIT_FAILURE);
      }

      if (WIFEXITED(status)) {
          int child_exit_code = WEXITSTATUS(status);
          return child_exit_code;
      } else {
          fprintf(stderr, "Child did not exit normally\n");
          return ERR_EXEC_FAILED;
      }
  }
}



static int setup_nft_file(FILE **fp) {
  *fp = fopen(NFT_RULESET, "w");
  if (*fp == NULL) {
    perror("Failed to open file");
    return ERR_FILE_NOT_FOUND;
  }
  fprintf(*fp, "table ip pasta {\n");
  fprintf(*fp, "    chain output {\n");
  fprintf(*fp, "        type filter hook output priority 0;\n");
  fprintf(*fp, "        policy drop;\n");
  return SUCCESS;
}

static int write_firewall_rule(FILE **fp, const char *rule) {
  if (*fp != NULL) {
    fprintf(*fp, "		%s;\n", rule);
    return SUCCESS;
  }
  return ERR_FILE_NOT_FOUND;
}

static void close_nft_file(FILE **fp) {
  fprintf(*fp, "    }\n");
  fprintf(*fp, "}\n");
  fclose(*fp);
}

static void execute_nft(const char *path) {
  if (path == NULL) {
    fprintf(stderr, "Error: ruleset path is NULL.\n");
    exit(EXIT_FAILURE);
  }

  pid_t pid = fork();
  if (pid < 0) {
    perror("Fork failed");
    return;
  } else if (pid == 0) {
    const char *nft_path = "/usr/sbin/nft";
    // char nft_bin[4];
    // strncpy(nft_bin, "nft", sizeof("nft"));
    char *const args[] = {(char *)"nft", (char *)"-f", (char *)path, NULL};
    execv(nft_path, args);
    perror("execv failed");
    exit(EXIT_FAILURE);
  } else {
    int status;
    if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid failed");
            exit(EXIT_FAILURE);
    }

    if (WIFEXITED(status)) {
        // Child exited normally, get its exit status
        return; // Exit parent with the same code
    } else {
        // Child did not exit normally
        fprintf(stderr, "Child did not exit normally\n");
        exit(EXIT_FAILURE);
    }
  }
}

void execute_firewall_rules(FirewallRules *fw_rules) {
  FILE *fp = NULL;
  if (fw_rules->count == 0)
    return;

  FirewallTool tool = which_nft_firewall();
  if (tool == NONE) {
    printf("Warning: no nft based firewall (iptables-nft or nft). Couldnt set "
           "up firewall in namespace\n");
    return;
  }

  if (tool == NFT) {
    int res = setup_nft_file(&fp);
    if (res < 0)
      return;
  }

  for (int i = fw_rules->count - 1; i >= 0; i--) {
    int result;

    if (tool == IPTABLES_NFT)
      result = execute_firewall_rule(fw_rules->rules[i]);
    else if (tool == NFT)
      result = write_firewall_rule(&fp, fw_rules->rules[i]);
    else
      result = ERR_FILE_NOT_FOUND;

    if (result != SUCCESS) {
      fprintf(stderr, "Failed to setup rule: %s\n", fw_rules->rules[i]);
    }
  }

  if (tool == NFT) {
    close_nft_file(&fp);
    execute_nft(NFT_RULESET);
  }
}

char* format_rule_from_ipv4(const char* ip) {
  FirewallTool tool = which_nft_firewall();
  char* ip_rule = NULL;
  if (tool == NONE) {
      fprintf(stderr, "Could not format the firewall rule cause tool==NONE\n");
      return NULL;
  }
  else if (tool == NFT) {
    ip_rule = (char*)malloc(MAX_IP_RULE_LEN_NFT + 1);
    if (ip_rule == NULL) {
      return NULL;
    }
    snprintf(ip_rule, MAX_IP_RULE_LEN_NFT, IP_RULE_FMT_NFT, ip);
  }
  else {
    ip_rule = (char*)malloc(MAX_IP_RULE_LEN + 1);
    if (ip_rule == NULL) {
      return NULL;
    }
    snprintf(ip_rule, MAX_IP_RULE_LEN, IP_RULE_FMT, ip);
  }
  return ip_rule;
}


void DumpRules(FirewallRules* fw_rules, std::string& filepath) {

    if (fw_rules == NULL || filepath.empty()) {
        return;
    }

    FILE* file = fopen(filepath.c_str(), "w");
    if (file == NULL) {
        perror("Failed to open file");
        return;
    }

    for (size_t i = 0; i < fw_rules->count; ++i) {
        if (fw_rules->rules[i] != NULL) {
            fprintf(file, "%s\n", fw_rules->rules[i]);
        }
    }

    fclose(file);
  
}
