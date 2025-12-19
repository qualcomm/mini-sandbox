CXX := $(shell command -v clang++ 2>/dev/null)
ifeq ($(CXX),)
  CXX := g++
endif
AR = ar
CP = cp
RANLIB = ranlib


VERSION ?= test
FLAGS := -Wall -fPIE -O3 -DVERSION=\"$(VERSION)\"
STD := -std=c++17
LDFLAGS :=
LDL := -ldl

ifeq (4.3,$(firstword $(sort $(MAKE_VERSION) 4.3)))
HAS_FILESYSTEM += $(shell echo "#include <filesystem> \nint main() {}" | $(CXX) $(STD) -x c++ - -o /dev/null 2> /dev/null && echo "yes" || echo "no")
else
HAS_FILESYSTEM += $(shell echo "\#include <filesystem> \nint main() {}" | $(CXX) $(STD) -x c++ - -o /dev/null 2> /dev/null && echo "yes" || echo "no")
endif

ifeq ($(HAS_FILESYSTEM),no)
        LDFLAGS += -lstdc++fs
endif

INCLUDE_DIR = ../../../
MINITAP ?= ../../../../minitap
MINITAP_OUT = $(MINITAP)/out
MINISANDBOX := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
LIBMINITAP = $(MINITAP_OUT)/libminitap.a
MINITAP_BIN = $(MINITAP_OUT)/minitap

SRCS = linux-sandbox.cc linux-sandbox-options.cc linux-sandbox-pid1.cc logging.cc process-tools.cc docker-support.cc linux-sandbox-api.cc error-handling.cc
CLI_SRCS = linux-sandbox-main.cc $(SRCS) 
MINITAP_CLI_SRCS = $(CLI_SRCS) firewall.cc minitap-interface.cc
MINITAP_LIB_SRCS = $(SRCS) firewall.cc minitap-interface.cc

OUT_DIR = out
BUILD_mini_sandbox = $(OUT_DIR)/.mini-sandbox
BUILD_libmini_sandbox = $(OUT_DIR)/.libmini-sandbox
BUILD_mini_tapbox = $(OUT_DIR)/.mini-tapbox
BUILD_libmini_tapbox = $(OUT_DIR)/.libmini-tapbox

OBJS_mini_sandbox = $(addprefix $(BUILD_mini_sandbox)/, $(CLI_SRCS:.cc=.o))
OBJS_libmini_sandbox = $(addprefix $(BUILD_libmini_sandbox)/, $(SRCS:.cc=.o))
OBJS_mini_tapbox = $(addprefix $(BUILD_mini_tapbox)/, $(MINITAP_CLI_SRCS:.cc=.o))
OBJS_libmini_tapbox = $(addprefix $(BUILD_libmini_tapbox)/, $(MINITAP_LIB_SRCS:.cc=.o))

BIN_mini_sandbox = $(OUT_DIR)/mini-sandbox
LIB_mini_sandbox = $(OUT_DIR)/libmini-sandbox
BIN_mini_tapbox = $(OUT_DIR)/mini-tapbox
LIB_mini_tapbox = $(OUT_DIR)/libmini-tapbox

.PHONY: all clean mini-sandbox libmini-sandbox mini-tapbox libmini-tapbox $(MINITAP_BIN)

all: mini-sandbox libmini-sandbox mini-tapbox libmini-tapbox

mini-sandbox: $(BIN_mini_sandbox)

$(BIN_mini_sandbox): $(OBJS_mini_sandbox)
	$(CXX) $(STD) $(CXXFLAGS) $(FLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD_mini_sandbox)/%.o: %.cc | $(BUILD_mini_sandbox)
	$(CXX) $(STD) $(CXXFLAGS) $(FLAGS) -I$(INCLUDE_DIR) -c $< -o $@

$(BUILD_mini_sandbox):
	mkdir -p $@

mini-tapbox: $(BIN_mini_tapbox)

$(BIN_mini_tapbox): $(OBJS_mini_tapbox) $(MINITAP_BIN)
	$(CP) $(MINITAP_BIN) $(OUT_DIR)
	$(CXX) $(STD) $(CXXFLAGS)  $(FLAGS) $(OBJS_mini_tapbox) -o $@ $(LDFLAGS) $(LDL)


$(MINITAP_BIN): 
	make -B -C $(MINITAP) bin
	$(CP) $(MINITAP_BIN) $(OUT_DIR)

$(BUILD_mini_tapbox)/%.o: %.cc | $(BUILD_mini_tapbox)
	$(CXX) $(STD) $(CXXFLAGS) $(FLAGS) -DMINITAP -I$(MINITAP_OUT) -I$(INCLUDE_DIR) -c $< -o $@

$(BUILD_mini_tapbox):
	mkdir -p $@

libmini-sandbox: $(LIB_mini_sandbox).a $(LIB_mini_sandbox).so

$(LIB_mini_sandbox).so: $(OBJS_libmini_sandbox)
	$(CXX) $(STD) $(FLAGS) $(CXXFLLAGS) -shared -o $@ $(OBJS_libmini_sandbox) $(LDFLAGS)

$(LIB_mini_sandbox).a: $(OBJS_libmini_sandbox)
	$(AR) rcs $@ $^
	$(RANLIB) $@

$(BUILD_libmini_sandbox)/%.o: %.cc | $(BUILD_libmini_sandbox)
	$(CXX) $(STD) $(FLAGS) $(CXXFLAGS) -DLIBMINISANDBOX -I$(INCLUDE_DIR) -fPIC -c $< -o $@

$(BUILD_libmini_sandbox):
	mkdir -p $@

libmini-tapbox: $(LIB_mini_tapbox).a $(LIB_mini_tapbox).so

$(LIB_mini_tapbox).a: $(OBJS_libmini_tapbox) $(MINITAP_BIN)
	$(AR) r $@ $(OBJS_libmini_tapbox)
	$(RANLIB) $@

$(LIB_mini_tapbox).so: $(OBJS_libmini_tapbox) $(MINITAP_BIN)
	$(CXX) $(STD) $(FLAGS) $(CXXFLLAGS) -shared -o $@ $(OBJS_libmini_tapbox) $(LDFLAGS)

$(BUILD_libmini_tapbox)/%.o: %.cc | $(BUILD_libmini_tapbox)
	$(CXX) $(STD) $(FLAGS) $(CXXFLAGS) -DLIBMINISANDBOX -DMINITAP -I$(INCLUDE_DIR) -I$(MINITAP_OUT) -fPIC -c $< -o $@

$(BUILD_libmini_tapbox):
	mkdir -p $@


clean:
	rm -rf $(OUT_DIR)
