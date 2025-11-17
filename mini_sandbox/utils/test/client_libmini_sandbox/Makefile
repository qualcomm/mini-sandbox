SCRIPT_DIR = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

ifdef PACKAGE_DIR
	MINI = $(PACKAGE_DIR)/include/
	LIBS = $(PACKAGE_DIR)/lib/libmini-sandbox.a
else
	MINI = $(SCRIPT_DIR)/../../../src/main/tools/
	LIBS = $(MINI)/out/libmini-sandbox.a
endif

CXX = clang++
FLAGS = -DLIBMINISANDBOX -g -lpthread -Wno-writable-strings -std=c++17
LDFLAGS ?= ""

ifeq (4.3,$(firstword $(sort $(MAKE_VERSION) 4.3)))
HAS_FILESYSTEM += $(shell echo "#include <filesystem> \nint main() {}" | $(CXX) $(STD) -x c++ - -o /dev/null 2> /dev/null && echo "yes" || echo "no")
else
HAS_FILESYSTEM += $(shell echo "\#include <filesystem> \nint main() {}" | $(CXX) $(STD) -x c++ - -o /dev/null 2> /dev/null && echo "yes" || echo "no")
endif

ifeq ($(HAS_FILESYSTEM),no)
        LDFLAGS += -lstdc++fs
endif

TARGET_CXX = client_cxx.bin
TARGET_CC = client_c.bin
TARGET_DEFAULT = client_cxx_default.bin
TARGET_CUSTOM = client_cxx_custom.bin
TARGET_HERMETIC = client_cxx_hermetic.bin
TARGET_DEFAULT_WRITE_PARENTS = client_cxx_default_write_parents.bin
TARGET = $(TARGET_CXX) $(TARGET_CC) $(TARGET_DEFAULT) $(TARGET_DEFAULT_WRITE_PARENTS) $(TARGET_CUSTOM) $(TARGET_HERMETIC)
SRC_CXX = client.cc



all: $(TARGET)

$(TARGET): $(SRC_CXX)
	$(CXX) $(FLAGS) $(CXXFLAGS) -I$(MINI) $(SRC_CXX) $(LIBS) -o $(TARGET_CXX) $(LDFLAGS)
	$(CXX) $(FLAGS) $(CXXFLAGS) -DDEFAULT -I$(MINI) $(SRC_CXX) $(LIBS) -o $(TARGET_DEFAULT) $(LDFLAGS)
	$(CXX) $(FLAGS) $(CXXFLAGS) -DDEFAULT -DDEFAULT_WRITE_PARENTS -I$(MINI) $(SRC_CXX) $(LIBS) -o $(TARGET_DEFAULT_WRITE_PARENTS) $(LDFLAGS)
	$(CXX) $(FLAGS) $(CXXFLAGS) -DCUSTOM -I$(MINI) $(SRC_CXX) $(LIBS) -o $(TARGET_CUSTOM) $(LDFLAGS)
	$(CXX) $(FLAGS) $(CXXFLAGS) -DHERMETIC -I$(MINI) $(SRC_CXX) $(LIBS) -o $(TARGET_HERMETIC) $(LDFLAGS)
	$(CXX) $(FLAGS) $(CFLAGS) -I$(MINI) $(SRC_CXX) $(LIBS) -o $(TARGET_CC) $(LDFLAGS)

clean:
	rm -f $(TARGET)

