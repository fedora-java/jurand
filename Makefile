include rules.mk

.PHONY: force all test-compile test clean

all: $(call Executable_file,jurand)

test-compile: $(call Executable_file,jurand) $(call Executable_file,jurand_test)

test: test.sh test-compile
	@./$<

clean:
	@rm -rfv target

CXXFLAGS += -g -std=c++2a -Isrc -Wall -Wextra -Wpedantic

$(eval $(call Variable_rule,target/compile_flags,$(CXX) $(CXXFLAGS)))
$(eval $(call Variable_rule,target/link_flags,$(CXX) $(LDFLAGS) $(LDLIBS)))

$(eval $(call Executable_file_rule,jurand,jurand.cpp))
$(eval $(call Executable_file_rule,jurand_test,jurand_test.cpp))

-include target/dependencies/*.mk
