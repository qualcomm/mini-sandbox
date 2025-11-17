#!/bin/bash

mini-sandbox -x -- /bin/bash << 'EOF'
exit 1
EOF

if [ $? -ne 1 ]; then
	echo "Failed"
	exit 1;
else
	echo "Success"
fi
