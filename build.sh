#!/bin/bash

set -eux

mkdir -p target/bin

CXXFLAGS+=' -std=c++2a -Isrc -Wall -Wextra -Wpedantic'

${CXX} ${CXXFLAGS} -o target/bin/java_remove_symbols src/java_remove_symbols.cpp ${LDFLAGS}
${CXX} ${CXXFLAGS} -o target/bin/java_remove_symbols_test src/java_remove_symbols_test.cpp ${LDFLAGS}
