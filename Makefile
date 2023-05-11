include rules.mk

.PHONY: force all clean test-compile test coverage manpages
.DEFAULT_GOAL = all

CXXFLAGS += -g -std=c++2a -Isrc -Wall -Wextra -Wpedantic

all: $(call Executable_file,jurand)

test-compile: $(call Executable_file,jurand) $(call Executable_file,jurand_test)

test: test.sh test-compile
	@./$<

$(call Executable_file,jurand): $(call Object_file,jurand.cpp)
$(call Executable_file,jurand_test): $(call Object_file,jurand_test.cpp)

manpages: \
	$(call Manpage_7,java_remove_annotations)\
	$(call Manpage_7,java_remove_imports)\

-include target/dependencies/*.mk
