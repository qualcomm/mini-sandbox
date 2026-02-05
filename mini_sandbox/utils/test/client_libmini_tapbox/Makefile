
SCRIPT_DIR = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

ifdef PACKAGE_DIR
        MINI = $(PACKAGE_DIR)/include/
        LIBS = $(PACKAGE_DIR)/lib/libmini-tapbox.a
        SO_PATH = $(PACKAGE_DIR)/lib/
else
        MINI = $(SCRIPT_DIR)/../../../src/main/tools/
        LIBS = $(MINI)/out/libmini-tapbox.a
        SO_PATH = $(MINI)/out/
endif

CXX := $(shell command -v clang++ 2>/dev/null)
CC := $(shell command -v clang 2>/dev/null)
ifeq ($(CXX),)
  CXX := g++
  CC := gcc
endif

FLAGS = -DMINITAP -DLIBMINISANDBOX -g -Wno-writable-strings -std=c++17

LDFLAGS :=
LDFLAGS += -lpthread -ldl

ifeq (4.3,$(firstword $(sort $(MAKE_VERSION) 4.3)))
HAS_FILESYSTEM += $(shell echo "#include <filesystem> \nint main() {}" | $(CXX) $(STD) -x c++ - -o /dev/null 2> /dev/null && echo "yes" || echo "no")
else
HAS_FILESYSTEM += $(shell echo "\#include <filesystem> \nint main() {}" | $(CXX) $(STD) -x c++ - -o /dev/null 2> /dev/null && echo "yes" || echo "no")
endif

ifeq ($(HAS_FILESYSTEM),no)
        LDFLAGS += -lstdc++fs
endif

TARGET_A = client.bin
TARGET_SO = client.so.bin
TARGET_MAX_CONN = client_max_connection.bin
SRC = client.cc connect_with_timeout.cc
SO_NAME = mini-tapbox

all: $(TARGET_A) $(TARGET_SO) $(TARGET_MAX_CONN)

$(TARGET_MAX_CONN): $(SRC)
	$(CXX) $(FLAGS) -DMAX_CONN $(CXXFLAGS) -I$(MINI) $(SRC) $(LIBS) -o $(TARGET_MAX_CONN) $(LDFLAGS)

$(TARGET_A): $(SRC)
	$(CXX) $(FLAGS) $(CXXFLAGS) -I$(MINI) $(SRC) $(LIBS) -o $(TARGET_A) $(LDFLAGS)

$(TARGET_SO): $(SRC)
	$(CXX) $(FLAGS) $(CXXFLAGS) -I$(MINI) $(SRC) -L$(SO_PATH) -l$(SO_NAME) -o $(TARGET_SO) $(LDFLAGS)

clean:
	rm -f *.bin *.o

