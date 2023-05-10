include rules.mk

.PHONY: force all clean test-compile test coverage manpages

all: $(call Executable_file,jurand)

clean:
	@rm -rfv target

test-compile: $(call Executable_file,jurand) $(call Executable_file,jurand_test)

test: test.sh test-compile
	@./$<

CXXFLAGS += -g -std=c++2a -Isrc -Wall -Wextra -Wpedantic

$(eval $(call Variable_rule,target/compile_flags,$(CXX) $(CXXFLAGS)))
$(eval $(call Variable_rule,target/link_flags,$(CXX) $(LDFLAGS) $(LDLIBS)))

$(call Executable_file,jurand): $(call Object_file,jurand.cpp)
$(call Executable_file,jurand_test): $(call Object_file,jurand_test.cpp)

manpages: \
	$(call Manpage_7,java_remove_annotations)\
	$(call Manpage_7,java_remove_imports)\

-include target/dependencies/*.mk
