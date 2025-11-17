#!/bin/bash

if [[ -n "$1" ]]; then
	export BASE="$1"
else
	export BASE=$HOME
fi

#export BASE="/local/mnt/workspace"

TOP_LVL="$BASE/top-level"
WD="$TOP_LVL/working-dir"
mkdir -p $WD
echo "mini-sandbox-test" > "$BASE/mini-sandbox-test"
echo "mini-sandbox-test" > "$TOP_LVL/mini-sandbox-test"

pushd $WD || {
	echo "Failed to change dir"
	exit 1
}


mini-sandbox -x -- /bin/bash << 'EOF'

TOP_LVL="$BASE/top-level"

check_last_command() {
    R=$?
    if [ $R -ne 0 ]; then
        echo "Error: Last command failed."
        exit 1
    else
        echo "Success: Last command succeeded."
    fi
}


check_last_command_failed() {
    if [ $? -ne 0 ]; then
        echo "Success: Last command failed (As Expected)."
        return 0
    else
        echo "Error: Last command succeeded but should have failed."
	exit 1
    fi
}


check_ls_count() {
    local count="$1"
    local threshold="$2"

    if [ "$count" -gt "$threshold" ]; then
        echo "Success: Directory contains more than $threshold items ($count)."
        return 0
    else
        echo "Error: Directory contains $count items, which is not more than $threshold."
        exit 1
    fi
}


echo -e "\nTest writing in parent folder $PWD/../"
echo "sandbox-test" > $PWD/../test.txt
check_last_command

echo -e "\nTest writing in $BASE/test.txt folder overlayfs"
echo "sandbox-test" > $BASE/test.txt
check_last_command

echo -e "\nTest reading from $BASE"

if [[ "$BASE" == "$HOME" ]]; then
	ls $BASE/mini-sandbox-test
	check_last_command_failed
else
	ls $BASE/mini-sandbox-test
	check_last_command
fi

echo -e "\nTest reading from $TOP_LVL (top level) dir"
ls $TOP_LVL/mini-sandbox-test
check_last_command

EOF


RES=$?
if [[ $RES -ne 0 ]]; then
    echo "Something went wrong inside the sandbox. Exiting"
    popd
    exit $RES
fi



check_last_command() {
    if [ $? -ne 0 ]; then
        echo "Error: Last command failed."
        exit 1
    else
        echo "Success: Last command succeeded."
    fi
}


check_last_command_failed() {
    if [ $? -ne 0 ]; then
        echo "Success: Last command failed (As Expected)."
        return 0
    else
        echo "Error: Last command succeeded but should have failed."
        popd
	exit 1
    fi
}


check_ls_count() {
    local count="$1"
    local threshold="$2"

    if [ "$count" -gt "$threshold" ]; then
        echo "Success: Directory contains more than $threshold items ($count)."
        return 0
    else
        echo "Error: Directory contains $count items, which is not more than $threshold."
        exit 1
    fi
}

echo -e "\nCheck that overlayfs has not gone through in parent folders"
ls $PWD/../test.txt
check_last_command_failed

echo -e "\nCheck that overlayfs has not gone through in $BASE folder"
ls $BASE/test.txt
check_last_command_failed

echo -e "\nAll Good!\nDone going back to original folder"
popd
exit 0
