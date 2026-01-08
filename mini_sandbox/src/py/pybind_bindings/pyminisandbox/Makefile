PYTHON_INCLUDES := $(shell python3 -m pybind11 --includes)
CXX := clang++

CXXFLAGS := -O3 -Wall -shared -std=c++17 -fPIC $(PYTHON_INCLUDES) -I./
SRC_DIR := ../../main/tools/
INCLUDE := ../../../

PYMINISANDBOX = ./pyminisandbox.so
PYMINITAPBOX = ./pyminitapbox.so

LIBMINISANDBOX := ../../main/tools/out/libmini-sandbox.a
LIBMINITAPBOX := ../../main/tools/out/libmini-tapbox.a


.PHONY: pyminisandbox pyminitapbox clean



check_libminisandbox:
	@if [ ! -f $(LIBMINISANDBOX) ]; then \
        	echo "You should first build libmini-sandbox.a (make -C ../../main/tools/ libmini-sandbox)"; \
        	exit 1; \
    	fi

check_libminitapbox:
	@if [ ! -f $(LIBMINITAPBOX) ]; then \
        	echo "You should first build libmini-tapbox.a (make -C ../../main/tools/ libmini-tapbox)"; \
        	exit 1; \
    	fi


pyminisandbox: $(PYMINISANDBOX)

$(PYMINISANDBOX): bindings.cpp check_libminisandbox
	$(CXX) $(CXXFLAGS) -I$(INCLUDE) -I$(SRC_DIR) -DLIBMINISANDBOX bindings.cpp $(LIBMINISANDBOX) -o $@

pyminitapbox: $(PYMINITAPBOX)

$(PYMINITAPBOX): bindings.cpp check_libminitapbox
	$(CXX) $(CXXFLAGS) -I$(INCLUDE) -I$(SRC_DIR) -DLIBMINISANDBOX -DMINITAP bindings.cpp $(LIBMINITAPBOX) -o $@


	

clean:
	rm -f $(PYMINISANDBOX) $(PYMINITAPBOX)
