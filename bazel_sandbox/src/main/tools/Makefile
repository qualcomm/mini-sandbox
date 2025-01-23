# Define the compiler
CXX = g++

# Define the source files
SRCS = linux-sandbox.cc linux-sandbox-options.cc linux-sandbox-pid1.cc logging.cc process-tools.cc execution_statistics.pb.cc

# Define the include directory
INCLUDE_DIR = ../../../

# Define the output executable
TARGET = out/linux-sandbox

# Define the libraries
LIBS = -lm -lrt -lprotobuf -pthread

# Compilation rule
$(TARGET): $(SRCS)
	mkdir -p out/
	$(CXX) $(SRCS) -I$(INCLUDE_DIR) -o $(TARGET) $(LIBS)

# Clean rule to remove the executable
clean:
	rm -f $(TARGET)
