#!/bin/bash
##
## Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
## SPDX-License-Identifier: MIT
##

# Validates the -c flag, which forces the capabilities sandbox instead of the
# namespace based one (same effect as MINI_SANDBOX_DOCKER_UNPRIVILEGED=1).
#
# This test has to pass on Ubuntu 18, Ubuntu 24, openSUSE and Debian, running
# either as root (as the CI containers do) or as a normal user. Everything that
# depends on the environment is therefore detected at run time and skipped when
# it does not apply, so only checks that are meaningful everywhere can fail the
# run. The portable assertions are the NoNewPrivs flag and the reduction of the
# capability bounding set, both read from /proc/self/status of the sandboxed
# process.


# Reads one field of /proc/self/status as seen by the sandboxed process.
# $1 is the field name, the remaining arguments are passed to mini-sandbox.
status_field_in_sandbox() {
    local field="$1"
    shift
    mini-sandbox "$@" -- grep "^${field}:" /proc/self/status 2>/dev/null \
        | cut -f2 | tail -n 1
}


# Reads one field of /proc/self/status for the current process.
status_field() {
    grep "^${1}:" /proc/self/status | cut -f2 | tail -n 1
}


# Tests one bit of a capability mask as printed by /proc/self/status.
# Only the low 32 capabilities are supported, which is enough for the
# capabilities checked here and avoids overflowing the shell arithmetic on
# masks such as 000001ffffffffff.
cap_bit_is_set() {
    local mask="$1"
    local bit="$2"
    local low=$(( 0x${mask: -8} ))
    (( ( low >> bit ) & 1 ))
}


check_cap_cleared() {
    local mask="$1"
    local bit="$2"
    local name="$3"

    if cap_bit_is_set "$mask" "$bit"; then
        echo "Error: $name (bit $bit) is still present in $mask."
        exit 1
    fi
    echo "Success: $name (bit $bit) has been dropped."
}


echo -e "\nTest that -c is accepted and reports the capabilities sandbox"
NNP_WITH_C="$(status_field_in_sandbox NoNewPrivs -c)"
if [ "$NNP_WITH_C" != "1" ]; then
    echo "Error: expected NoNewPrivs 1 with -c, got '${NNP_WITH_C}'."
    exit 1
fi
echo "Success: -c sets NoNewPrivs to 1."


echo -e "\nTest that -c wins over MINI_SANDBOX_DOCKER_PRIVILEGED"
# The CI containers export MINI_SANDBOX_DOCKER_PRIVILEGED=1, so the flag has to
# take precedence over it or the capabilities sandbox could never be tested.
NNP_PRIV="$(MINI_SANDBOX_DOCKER_PRIVILEGED=1 status_field_in_sandbox NoNewPrivs -c)"
if [ "$NNP_PRIV" != "1" ]; then
    echo "Error: -c did not override MINI_SANDBOX_DOCKER_PRIVILEGED, NoNewPrivs is '${NNP_PRIV}'."
    exit 1
fi
echo "Success: -c overrides MINI_SANDBOX_DOCKER_PRIVILEGED."


echo -e "\nTest that -c and MINI_SANDBOX_DOCKER_UNPRIVILEGED are equivalent"
NNP_ENV="$(MINI_SANDBOX_DOCKER_UNPRIVILEGED=1 status_field_in_sandbox NoNewPrivs)"
if [ "$NNP_ENV" != "$NNP_WITH_C" ]; then
    echo "Error: env variable gave NoNewPrivs '${NNP_ENV}' but -c gave '${NNP_WITH_C}'."
    exit 1
fi
echo "Success: the flag and the environment variable behave the same."


echo -e "\nTest that -c can be combined with the other flags"
NNP_COMBINED="$(status_field_in_sandbox NoNewPrivs -c -x)"
if [ "$NNP_COMBINED" != "1" ]; then
    echo "Error: expected NoNewPrivs 1 with '-c -x', got '${NNP_COMBINED}'."
    exit 1
fi
echo "Success: -c works together with -x."


echo -e "\nTest that the ambient capability set is empty"
AMB="$(status_field_in_sandbox CapAmb -c)"
case "$AMB" in
    "" )
        echo "Skipping: this kernel does not report CapAmb."
        ;;
    *[!0]* )
        echo "Error: ambient capabilities are not empty ($AMB)."
        exit 1
        ;;
    * )
        echo "Success: ambient capabilities are empty."
        ;;
esac


echo -e "\nTest that the capability bounding set is reduced"
# Shrinking the bounding set needs CAP_SETPCAP, so this only applies when the
# caller is privileged. That is the case in the CI containers, which run as
# root, but not for a normal user on a developer machine.
CAP_SETPCAP_BIT=8
CAP_EFF_OUTSIDE="$(status_field CapEff)"
if cap_bit_is_set "$CAP_EFF_OUTSIDE" "$CAP_SETPCAP_BIT"; then
    BND="$(status_field_in_sandbox CapBnd -c)"
    if [ -z "$BND" ]; then
        echo "Error: could not read CapBnd from the sandboxed process."
        exit 1
    fi
    echo "Bounding set inside the sandbox: $BND"
    check_cap_cleared "$BND" 21 "CAP_SYS_ADMIN"
    check_cap_cleared "$BND" 7  "CAP_SETUID"
    check_cap_cleared "$BND" 8  "CAP_SETPCAP"
    check_cap_cleared "$BND" 13 "CAP_NET_RAW"
else
    echo "Skipping: CAP_SETPCAP not held (CapEff $CAP_EFF_OUTSIDE), the bounding set cannot be shrunk."
fi


echo -e "\nTest that NoNewPrivs is inherited by the children of the sandboxed process"
mini-sandbox -c -- /bin/bash << 'EOF'

# /proc/self/status is tab separated, so cut avoids any quoting problem when the
# same check has to be nested inside another shell.
CHECK='test "$(grep ^NoNewPrivs: /proc/self/status | cut -f2)" = 1'

check_nnp() {
    local where="$1"
    shift
    if "$@"; then
        echo "Success: NoNewPrivs is set $where."
    else
        echo "Error: NoNewPrivs is not set $where."
        exit 1
    fi
}

check_nnp "in the sandboxed shell" /bin/sh -c "$CHECK"

# The flag has to survive fork and exec, otherwise a child could regain
# privileges even though the first process was confined.
check_nnp "in a child process" /bin/sh -c "/bin/sh -c '$CHECK'"
check_nnp "in a grandchild process" /bin/sh -c "/bin/sh -c \"/bin/sh -c '$CHECK'\""
EOF

if [ $? -ne 0 ]; then
    echo "Error: the checks inside the sandbox failed."
    exit 1
fi


echo -e "\nTest that the exit code is propagated in capabilities mode"
mini-sandbox -c -- /bin/bash << 'EOF'
exit 7
EOF

if [ $? -ne 7 ]; then
    echo "Error: exit code was not propagated through the capabilities sandbox."
    exit 1
fi
echo "Success: exit code propagated."


echo -e "\nTest that the sandbox without -c is not in capabilities mode"
# Only meaningful where the namespace sandbox is actually available. When user
# namespaces are missing mini-sandbox already falls back to the capabilities
# sandbox on its own, and then there is nothing for -c to change.
NNP_BASELINE="$(status_field_in_sandbox NoNewPrivs)"
if [ "$NNP_BASELINE" = "0" ]; then
    echo "Success: without -c NoNewPrivs is 0, so -c is what enables the capabilities sandbox."
else
    echo "Skipping: this environment already defaults to the capabilities sandbox (NoNewPrivs ${NNP_BASELINE})."
fi


echo -e "\nTest that a setuid binary cannot be used to escalate"
# Informational only. For a normal user "sudo -n" fails whether or not the flag
# is set, so the message is the only signal, and its wording depends on the sudo
# version. Root does not go through a setuid transition at all.
if [ "$(id -u)" -eq 0 ]; then
    echo "Skipping: running as root, no setuid transition to block."
elif [ -z "$(command -v sudo)" ]; then
    echo "Skipping: sudo is not installed."
else
    SUDO_OUT="$(mini-sandbox -c -- sudo -n true 2>&1)"
    if echo "$SUDO_OUT" | grep -qi "new privileges"; then
        echo "Success: sudo refused to run because of the no-new-privs flag."
    else
        echo "Note: sudo did not report the no-new-privs flag, output was: ${SUDO_OUT}"
    fi
fi


echo -e "\nAll done"
