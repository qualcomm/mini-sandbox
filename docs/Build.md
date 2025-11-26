# Build the project and the library

We support 4 different flavors of builds, as listed here. Before each `make` command we recommend to run a `make clean` to make sure that previous out-dated objects have been removed. Also, before running the build `cd mini_sandbox/src/main/tools`

### Vanilla build CLI

- `make mini-sandbox`

### TAP mode enabled CLI

- `export MINITAP=path/to/minitap/repository` (minitap gets shipped as submodule, this is only needed for custom builds)
- `make mini-tapbox`

### libmini-sandbox

- `make libmini-sandbox`

### libmini-sandbox + Firewall APIs

- `export MINITAP=path/to/minitap/repository` (minitap gets shipped as submodule, this is only needed for custom builds)
- `make libmini-tapbox`

# Compatibility and support 

The project has been tested on Docker Ubuntu 18.04 until Ubuntu 24.04 and is known to work on those systems. However, in Ubuntu 18.04, users that want to use the static library (`libmini-sandbox`) will have to link with `-lstdc++fs`. Also, in Ubuntu 18.04 the `iptables-nft` frontend that we currently rely on to set up the firewall rules does not exist so we have to use the raw `nft` with the consequence that rules have to be specified according to that format (see example in `utils/firewall_example_rules/rules_nft.bak`.

For the compiler, the project is known to build with clang-6 up to clang-19 as well as different version of gcc/g++ from 7.5 up to 13.3.0
