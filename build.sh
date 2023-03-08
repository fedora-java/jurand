#!/bin/bash

set -eux

mkdir -p target/bin

CXXFLAGS+=' -std=c++2a -Isrc -Wall -Wextra -Wpedantic'

${CXX:-g++} ${CXXFLAGS} ${LDFLAGS:-} -o "target/bin/regex.o" -c "src/regex.cpp" ${LDLIBS:-}

LDLIBS+="target/bin/regex.o"

for source in jurand jurand_test; do
	${CXX:-g++} ${CXXFLAGS} ${LDFLAGS:-} -o "target/bin/${source}" "src/${source}.cpp" ${LDLIBS:-}
done
