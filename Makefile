default: llama.cpp llm-ui

# run make in "llama.cpp" directory
LLAMA_DIR = "llama.cpp"
.PHONY: llama.cpp
llama.cpp: 
	$(MAKE) -C $(LLAMA_DIR)

EXEC      = llm-ui
SRC_FILES = src/mainframe.cpp src/config.cpp src/llm-ui.cpp src/model.cpp \
	src/webview.cpp src/utils.cpp src/loguru.cpp
O_FILES   = $(SRC_FILES:%.cpp=%.o)

CXX = g++ -std=c++20
CC = $(CXX)

DEBUG_LEVEL     = -g
EXTRA_CCFLAGS	= -DLOGURU_WITH_STREAMS=1 -Wall -Wextra -Wpedantic -Wno-multichar -Wno-unused-parameter
CPPFLAGS        = $(DEBUG_LEVEL) $(EXTRA_CCFLAGS)

CXXFLAGS=`wx-config --cxxflags` -I llama.cpp/ -I include/
LDLIBS=`wx-config --libs all` llama.cpp/ggml.o llama.cpp/common.o llama.cpp/k_quants.o

all: $(EXEC)

$(EXEC): $(O_FILES)
	$(CC) $(LDFLAGS) $(O_FILES) -o $@ $(LDLIBS)

clean:
	rm -vf $(O_FILES)

