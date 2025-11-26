# Internal only

This project create a binary whose only goal is to start a gVisor TCP/IP stack on top of a TUN device. To do that, the binary will create a network namespace and configured it accordingly (set up loopback, subnet, create TUN, assign address, etc.). After that other processing joining this network namespace will automatically run using the user-space gVisor network stack. 

See example usage in test


## Build

- `./install_deps.sh` (will install golang)
- `make`

Run tests with:
- `make test`
