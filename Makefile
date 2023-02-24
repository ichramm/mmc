##
## MMC
CURRENT?=unit4e111

Target=release

CC      ?=clang
CXXC    ?=clang++
CFLAGS   :=$(CFLAGS) -DUSE_SFMT -DSFMT_MEXP=19937 -c -Wall -Wextra -std=c11 -Wno-unused-parameter
CXXFLAGS :=$(CXXFLAGS) -DUSE_SFMT -DSFMT_MEXP=19937 -c -Wall -Wextra -std=c++17
INCLUDES =-Iincludes
LINKER  ?=$(CXXC)
LIBS     =-lpthread
LDFLAGS  =

ifeq (Debug, $(findstring Debug,$(Target)))
	CFLAGS   :=$(CFLAGS) -g3
	CXXFLAGS :=$(CXXFLAGS) -g3
else
	CFLAGS   :=$(CFLAGS) -O3 -march=native -mtune=native -funroll-loops
	CXXFLAGS :=$(CXXFLAGS) -O3 -march=native -mtune=native -funroll-loops
endif

ROOT_DIR   =.
SRC_DIR    =$(ROOT_DIR)/src
BIN_DIR    =$(ROOT_DIR)/build/bin

OBJS_ROOT  =$(ROOT_DIR)/build/objs
OBJS_DIR   =$(OBJS_ROOT)/$(Target)

OUTPUT_DIR  =$(BIN_DIR)/$(Target)
OUTPUT_FILE =$(OUTPUT_DIR)/$(CURRENT)

SFMT_OBJS = $(OBJS_DIR)/sfmt.o
CDF_OBJS = $(OBJS_DIR)/cdflib.o

OBJS=$(SFMT_OBJS) $(OBJS_DIR)/$(CURRENT).o

ifeq (-DUSE_CDFLIB, $(findstring -DUSE_CDFLIB,$(CXXFLAGS)))
	OBJS :=$(OBJS) $(CDF_OBJS)
endif

# default target
#$(shell $(MAKE) -qp -f $(lastword $(MAKEFILE_LIST)) : 2>/dev/null | grep $(shell echo $(OUTPUT_FILE): | sed 's|./||') | awk '{ for ( n=1; n<=NF; n++ ) if($$n ~ "[.]o$$") print $$n }') \
build: $(OUTPUT_FILE)
$(OUTPUT_FILE): $(OBJS_DIR) $(OUTPUT_DIR) $(OBJS)
	$(LINKER) $(LDFLAGS) $(OBJS) $(LIBS) -o "$(OUTPUT_FILE)"

.PHONY: clean
clean:
	rm -vf $(OBJS_ROOT)/**/*.o
	rm -vf $(OUTPUT_FILE)

.PHONY: run
run: build
	$(shell readlink -f $(OUTPUT_FILE)) $(ARGS)

$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

$(OBJS_DIR):
	mkdir -p $(OBJS_DIR)

# Pattern rules:
$(OBJS_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) "$<" -o "$(OBJS_DIR)/$(*F).o"

$(SFMT_OBJS):
	@mkdir -p $(OBJS_DIR)/sfmt
	$(CC) -c $(CFLAGS) $(INCLUDES) $(SRC_DIR)/sfmt/SFMT.c -o $(SFMT_OBJS)

# $(OUTPUT_DIR)/unit1: $(OBJS_DIR)/unit1.o
# $(OUTPUT_DIR)/unit2e31: $(OBJS_DIR)/unit2e31.o
# $(OUTPUT_DIR)/unit2e61: $(OBJS_DIR)/unit2e61.o
# $(OUTPUT_DIR)/unit3e71: $(OBJS_DIR)/unit3e71.o
# $(OUTPUT_DIR)/unit4e81: $(OBJS_DIR)/unit4e81.o
# $(OUTPUT_DIR)/unit4e111: $(OBJS_DIR)/unit4e111.o
