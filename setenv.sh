#!/bin/bash

script_path=$(dirname "$(realpath "${BASH_SOURCE[0]}")")
sandbox_path="$script_path/mini_sandbox/src/main/tools/out"
echo "Adding $sandbox_path into PATH"
PATH=$sandbox_path:$PATH

