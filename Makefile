include rules.mk

.PHONY: force all clean test-compile test manpages

all: $(call Executable_file,jurand)

clean:
	@rm -rfv target

test-compile: $(call Executable_file,jurand) $(call Executable_file,jurand_test)

test: test.sh test-compile
	@./$<

CXXFLAGS += -g -std=c++2a -Isrc -Wall -Wextra -Wpedantic

$(eval $(call Variable_rule,target/compile_flags,$(CXX) $(CXXFLAGS)))
$(eval $(call Variable_rule,target/link_flags,$(CXX) $(LDFLAGS) $(LDLIBS)))

$(eval $(call Executable_file_rule,jurand,jurand.cpp))
$(eval $(call Executable_file_rule,jurand_test,jurand_test.cpp))

$(eval $(call Manpage_rule,java_remove_annotations))
$(eval $(call Manpage_rule,java_remove_imports))

manpages: $(manpages)

-include target/dependencies/*.mk
