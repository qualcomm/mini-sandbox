
SCRIPT_DIR = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

ifdef PACKAGE_DIR
        MINI = $(PACKAGE_DIR)/include/
        LIBS = $(PACKAGE_DIR)/lib/libmini-sandbox.a
else
        MINI = $(SCRIPT_DIR)/../../../src/main/tools/
        LIBS = $(MINI)/out/libmini-sandbox.a
endif

CXX := $(shell command -v clang++ 2>/dev/null)
CC := $(shell command -v clang 2>/dev/null)
ifeq ($(CXX),)
  CXX := g++
  CC := gcc
endif


COMMON_FLAGS = -DLIBMINISANDBOX -g -lpthread -Wno-writable-strings
FLAGS = $(COMMON_FLAGS) -std=c++17
LDFLAGS := 

ifeq (4.3,$(firstword $(sort $(MAKE_VERSION) 4.3)))
HAS_FILESYSTEM += $(shell echo "#include <filesystem> \nint main() {}" | $(CXX) $(STD) -x c++ - -o /dev/null 2> /dev/null && echo "yes" || echo "no")
else
HAS_FILESYSTEM += $(shell echo "\#include <filesystem> \nint main() {}" | $(CXX) $(STD) -x c++ - -o /dev/null 2> /dev/null && echo "yes" || echo "no")
endif

ifeq ($(HAS_FILESYSTEM),no)
        LDFLAGS += -lstdc++fs
endif

# List of source files
SRC_CXX_LIST = check_only_one_root.cc  check_validate_path.cc only_one_log.cc check_overlay_enabled.cc call_default_after_start.cc call_start_twice.cc check_runtime_init_error.cc check_exit_from_user.cc check_runtime_init_error_readonly.cc check_runtime_init_error_custom.cc

# Derive target names from source files
TARGETS = $(SRC_CXX_LIST:.cc=.bin)

all: $(TARGETS)

# Pattern rule to build each binary
%.bin: %.cc
	$(CXX) $(FLAGS) $(CXXFLAGS) -I$(MINI) $< $(LIBS) -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGETS)

