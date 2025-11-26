
CXX = clang++
FLAGS = -DLIBMINISANDBOX -g -lpthread -Wno-writable-strings
SCRIPT_DIR = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
MINI = $(SCRIPT_DIR)/../../../src/main/tools/
TARGET = client.bin
SRC = test_case_ret_child.c 
LIBS = $(MINI)/out/libmini-tapbox.a

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(FLAGS) $(CXXFLAGS) -I$(MINI) $(SRC) $(LIBS) -o $(TARGET)

clean:
	rm -f $(TARGET)

