CFLAGS=-O3 -g -Wall -Wextra -Werror -std=c++0x -Wno-sign-compare 
LIBS=-lnuma -lpthread -lrt
CXX=g++

INCLUDE=include
SRC=src
TEST:=test
SOURCES:=$(wildcard $(SRC)/*.cc $(SRC)/*.c)
OBJECTS:=$(patsubst $(SRC)/%.cc,build/%.o,$(SOURCES))
TEST_SOURCES:=$(wildcard $(TEST)/*.cc)
TEST_OBJECTS:=$(patsubst test/%.cc,test/%.o,$(TESTSOURCES))
NON_MAIN_OBJECTS:=$(filter-out build/main.o,$(OBJECTS))
DEPSDIR:=.deps
DEPCFLAGS=-MD -MF $(DEPSDIR)/$*.d -MP

all:CFLAGS+=-DTESTING=0 -fno-omit-frame-pointer
all:env build/sync_bench

test:CFLAGS+=-DTESTING=1
test:env build/tests

-include $(wildcard $(DEPSDIR)/*.d)

build/%.o: src/%.cc $(DEPSDIR)/stamp GNUmakefile
	@mkdir -p build
	@echo + cc $<
	@$(CXX) $(CFLAGS) $(DEPCFLAGS) -I$(INCLUDE) -c -o $@ $<

$(TEST_OBJECTS):$(OBJECTS)

test/%.o: test/%.cc $(DEPSDIR)/stamp GNUmakefile
	@echo + cc $<
	@$(CXX) $(CFLAGS) -Wno-missing-field-initializers -Wno-conversion-null $(DEPCFLAGS) -Istart -I$(SRC) -I$(INCLUDE) -c -o $@ $<

build/sync_bench:$(OBJECTS)
	@$(CXX) $(CFLAGS) -o $@ $^ $(LIBS)

build/tests:$(NON_MAIN_OBJECTS) $(TEST_OBJECTS)
	@$(CXX) $(CFLAGS) -o $@ $^ $(LIBS)

$(DEPSDIR)/stamp:
	@mkdir -p $(DEPSDIR)
	@touch $@

.PHONY: clean env

clean:
	@rm -rf build $(DEPSDIR) $(TEST_OBJECTS) *~ include/*~ src/*~

