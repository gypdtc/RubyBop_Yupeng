ifneq ($(TRAVIS_CI), 1)
	CC = gcc
endif

INCFLAGS = -I./ -I./ipa
DEFS = -DCOLLECT_STATS -DIPA_SYSTEM_ALLOC -DNO_HOOK -DSUPPORT_THREADS -USUPPORT_THREADS -D_GNU_SOURCE
OPT_FLAGS = -O3 -march=native
DEBUG_FLAGS = -ggdb3 -g3
CFLAGS = $(OPT_FLAGS) $(DEBUG_FLAGS) -fPIC -Wall -Wno-missing-braces  $(INCFLAGS) $(DEFS)
TEST_SOURCE = $(wildcard tests/*.c) $(wildcard tests/parallel/*.c)
SOURCE = $(wildcard *.c)
ALL_SOURCE = $(TEST_SOURCE) $(SOURCE)
HEADERS = $(wildcard *.h)
TEST_BINARIES = $(basename $(TEST_SOURCE))
TEST_OBJECTS = $(patsubst %.c,%.o, $(TEST_SOURCE))
DEP = $(SOURCE:.c=.d) $(TEST_SOURCE:.c=.d)

OBJECTS = ipa.o memmap.o ipa_utils.o ipa_sync.o ipa_overrides.o file_io.o
LDFLAGS = -Wl,--no-as-needed -ldl -rdynamic -lm
LIBRARY = libipa.a

default: $(LIBRARY)

libipa.a: $(OBJECTS)
	@echo Building library $@
	@ar rcs $@ $?
	@ranlib $@

%.d: %.c
	@echo "Creating dependency $@"
	@$(CC) $(CFLAGS) -MM -o $@ $?
-include $(DEP)


tests/parallel/stack_%: tests/parallel/stack_%.c
	@echo "Linking $@"
	@$(CC) $(CFLAGS) -pthread $? -o $@ $(LDFLAGS)

# stack tests doesn't depend on the whole system -- special case
tests/stack_%: tests/stack_%.c
	@echo "Linking $@"
	@$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

tests/%: tests/%.o $(LIBRARY)
	@echo "Linking $@"
	@$(CC) -rdynamic $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.c
	@echo "Compiling $@"
	@$(CC) $(CFLAGS) -c -o $@ $<

tests: $(TEST_OBJECTS) $(TEST_BINARIES)
# NOTE: Tests objects is listed as a dependency so make will not auto-remove them


test: $(TEST_BINARIES)
	@echo Test binaries: $(sort $(notdir $(TEST_BINARIES)))
	@ruby tests/test_runner.rb $(sort $(TEST_BINARIES))
	@echo "Running conflict free tests"
	@echo "Testing normal sized allocations"
	@./tests/speculating -i 10000 -t 4  | ruby tests/no_conflict.rb
	@echo "Testing large sized allocations"
	@./tests/speculating -i 500 -t 4 -s -1 | ruby tests/no_conflict.rb

clean:
	rm -f $(TEST_OBJECTS) $(TEST_BINARIES) $(OBJECTS) gmon.out $(LIBRARY)
clobber: clean
	rm -f $(DEP)
