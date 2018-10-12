INSTALL_DIR=../../../build/install
BENCHMARK_UTILS_DIR=../../utils

RM=rm -rf

CXX=g++
CXXFLAGS=-g -O2 -Wall
LDFLAGS=-L$(INSTALL_DIR)/lib

INCLUDES=-I$(BENCHMARK_UTILS_DIR) \
-I$(INSTALL_DIR)/include \
-I$(INSTALL_DIR)/include/sbf \
-I$(INSTALL_DIR)/include/utils \
-I$(INSTALL_DIR)/include/config \
-I$(INSTALL_DIR)/include/logger \
-I$(INSTALL_DIR)/include/cdr \
-I$(INSTALL_DIR)/include/codec \
-I$(INSTALL_DIR)/include/codec/fields \
-I$(INSTALL_DIR)/include/gwc

LIBS=-lconfig \
-llogger \
-lcdr \
-levent_core \
-levent \
-levent_pthreads \
-lutils \
-lsbfcore \
-lsbfcommon \
-ldl \
-lgwc


all:
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c lse-millenium-test.cpp -o lse-test.o
	$(CXX) -o lse-test lse-test.o $(LDFLAGS) $(LIBS)

clean:
	$(RM) lse-test lse-test.o
