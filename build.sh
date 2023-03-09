#!/bin/bash

set -eux

mkdir -p target/bin

CXXFLAGS+=' -g -std=c++2a -Isrc -Wall -Wextra -Wpedantic'

for source in jurand jurand_test; do
	${CXX:-g++} ${CXXFLAGS} ${LDFLAGS:-} -o "target/bin/${source}" "src/${source}.cpp" ${LDLIBS:-}
done
