include rules.mk

.PHONY: force all clean test-compile test coverage manpages test-install clean-install
.DEFAULT_GOAL = all

CXXFLAGS += -g -std=c++2a -Wall -Wextra -Wpedantic

all: $(call Executable_file,jurand)

test-compile: $(call Executable_file,jurand) $(call Executable_file,jurand_test)

test: test.sh test-compile
	@./$<

$(call Executable_file,jurand): $(call Object_file,jurand.cpp)
$(call Executable_file,jurand_test): $(call Object_file,jurand_test.cpp)

manpages: \
	$(call Manpage,jurand.1)\

test-install: export buildroot = target/buildroot
test-install: export bindir = /usr/bin
test-install: export mandir = /usr/share/man
test-install: all manpages
	./install.sh

clean-install:
	@rm -rfv target/buildroot

-include target/dependencies/*.mk
