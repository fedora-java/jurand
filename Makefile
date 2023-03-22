-include target/deps/*.mk

.PHONY: all test-compile test clean

all: target/bin/jurand

test-compile: target/bin/jurand target/bin/jurand_test

test: test.sh test-compile
	@./test.sh

clean:
	@rm -rfv target

CXXFLAGS += -g -std=c++2a -Isrc -Wall -Wextra -Wpedantic

define Executable
target/bin/$(1): src/$(1).cpp
	@mkdir -p target/bin target/deps
	$$(CXX) $$(CXXFLAGS) $$(LDFLAGS) -MD -MF target/deps/$(1).mk -MT $$@ -o $$@ $$< $$(LDLIBS)
endef

$(eval $(call Executable,jurand))
$(eval $(call Executable,jurand_test))
