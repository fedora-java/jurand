#!/bin/bash

set -e

./target/bin/jurand_test

mkdir -p target/test_resources

run_tool()
{
	local filename="${1}"; shift
	cp "test_resources/${filename}" "target/test_resources/${filename}"
	./target/bin/jurand -i "target/test_resources/${filename}" "${@}"
}

test_file()
{
	local filename="${1}"; shift
	local expected="${1}"; shift
	cp "test_resources/${expected}" "target/test_resources/${expected}"
	run_tool "${filename}" "${@}"
	diff -u "target/test_resources/${filename}" "target/test_resources/${expected}"
}

test_file "Simple.java" "Simple.1.java" -a -n "D"
test_file "Simple.java" "Simple.1.java" -a -p "a[.]b[.]c[.]D"

test_file "Attributes.java" "Attributes.1.java" -a -n "D"
test_file "Attributes.java" "Attributes.1.java" -a -p "a"

test_file "Pattern.java" "Pattern.1.java" -a -p "annotations"

test_file "Fqn.java" "Fqn.1.java" -a -n "D"

test_file "Collision.java" "Collision.1.java" -a -p "something"
test_file "Collision.java" "Collision.2.java" -a -p "annotations"

test_file "Fqn_collision.java" "Fqn_collision.1.java" -a -n "Test"
test_file "Fqn_collision.java" "Fqn_collision.1.java" -a -p "junit"
test_file "Fqn_collision.java" "Fqn_collision.2.java" -a -p "jupiter[.]api[.]Test"

test_file "Static_import.java" "Static_import.1.java" -a -n "C"
test_file "Static_import.java" "Static_import.2.java" -a -p "b"
test_file "Static_import.java" "Static_import.2.java" -a -n "b"

test_file "Static_import_collision.java" "Static_import_collision.1.java" -a -n "C"

test_file "Garbage.java" "Garbage.1.java" -a -n "Nullable"

test_file "Imports.java" "Imports.0.java" -a -p "static"
test_file "Imports.java" "Imports.1.java" -a -p "util"
test_file "Imports.java" "Imports.2.java" -a -p "java[.]lang[.]"

# The purpose of these tests is to check that the tool does not loop, the
# result is irrelevant
run_tool "Termination.1.java" -n "C" || :
run_tool "Termination.2.java" -a -n "C" || :
run_tool "Termination.3.java" -a -n "C" || :
run_tool "Termination.4.java" -a -n "C" || :
run_tool "Termination.5.java" -a -n "C" || :
run_tool "Termination.6.java" -a -n "C" || :

echo PASSED
