CXX = clang++
CC = clang

FLAGS = -DLIBMINISANDBOX -g -lpthread -Wno-writable-strings
SCRIPT_DIR = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
MINI = $(SCRIPT_DIR)/../../../src/main/tools/
LIBS = $(MINI)/out/libmini-sandbox.a

# List of source files
SRC_CXX_LIST = check_only_one_root.cc  check_validate_path.cc only_one_log.cc check_overlay_enabled.cc call_default_after_start.cc call_start_twice.cc

# Derive target names from source files
TARGETS = $(SRC_CXX_LIST:.cc=.bin)

all: $(TARGETS)

# Pattern rule to build each binary
%.bin: %.cc
	$(CXX) $(FLAGS) $(CXXFLAGS) -I$(MINI) $< $(LIBS) -o $@

clean:
	rm -f $(TARGETS)

