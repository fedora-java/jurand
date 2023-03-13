#!/bin/bash

set -e

# Unit tests
./target/bin/jurand_test

rm -rf target/test_resources
mkdir -p target/test_resources

run_tool()
{
	local filename="${1}"; shift
	cp -r "test_resources/${filename}" "target/test_resources/${filename}"
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

test_strict()
{
	local filename="${1}"; shift
	local expected="${1}"; shift
	cp "test_resources/${expected}" "target/test_resources/${expected}"
	run_tool "${filename}" --strict "${@}" | grep "strict mode" 1>/dev/null
}

################################################################################
# Tests for simple invocations and return codes

if ! ./target/bin/jurand | grep 'Usage:'; then
	echo "fail: Usage string not printed"
	exit 1
fi

if ! ./target/bin/jurand -h | grep 'Usage:'; then
	echo "fail: Usage string not printed"
	exit 1
fi

if ./target/bin/jurand -i; then
	echo "fail: should have failed"
	exit 1
fi

if ./target/bin/jurand -n; then
	echo "fail: should have failed"
	exit 1
fi

if ./target/bin/jurand -p; then
	echo "fail: should have failed"
	exit 1
fi

if ./target/bin/jurand nonexisting_file -n "A"; then
	echo "fail: should have failed"
	exit 1
fi

if [ -n "$(echo "import A;" | ./target/bin/jurand -n "A")" ]; then
	echo "fail: output should be empty"
	exit 1
fi

if [ "$(echo "import A;" | ./target/bin/jurand -n "B")" != "import A;" ]; then
	echo "fail: output should be identical to input"
	exit 1
fi

################################################################################
# Tests for actual matching and removal

test_file "Simple.java" "Simple.1.java" -a -n "D"
test_file "Simple.java" "Simple.1.java" -a -p "a[.]b[.]c[.]D"

test_file "Underscore.java" "Underscore.1.java" -n "b_c"
test_file "Underscore.java" "Underscore.1.java" -p "a"
test_file "Underscore.java" "Underscore.2.java" -a -n "b_c"
test_file "Underscore.java" "Underscore.2.java" -a -p "a"

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

test_file "Utf_8.java" "Utf_8.1.java" -n "č"
test_file "Utf_8.java" "Utf_8.2.java" -a -n "č"
test_file "Utf_8.java" "Utf_8.3.java" -a -n "ď"

test_file "Regression5.java" "Regression5.1.java" -a -n Serial

################################################################################
# Tests for tool termination on invalid sources, result is irrelevant

run_tool "Termination.1.java" -n "C" || :
run_tool "Termination.2.java" -a -n "C" || :
run_tool "Termination.3.java" -a -n "C" || :
run_tool "Termination.4.java" -a -n "C" || :
run_tool "Termination.5.java" -a -n "C" || :
run_tool "Termination.6.java" -a -n "C" || :

################################################################################
# Tests of directory traversal

run_tool "directory" -a -n "Annotation"
for filename in A a/B a/b/C; do
	diff -u "target/test_resources/directory/${filename}.java" "target/test_resources/directory/${filename}.1.java"
done

################################################################################
# Tests of strict mode

# Nothing was matched/removed
test_strict "Strict.1.java" "Strict.1.java" -p "z"
test_strict "Strict.1.java" "Strict.1.java" -n "z"
test_strict "Strict.2.java" "Strict.2.java" -p "z"
test_strict "Strict.2.java" "Strict.2.java" -n "z"
# -a but no annotation was removed
test_strict "Strict.2.java" "Strict.2.java" -a -p "a"
test_strict "Strict.2.java" "Strict.2.java" -a -n "C"
# Should print the directory name
run_tool "directory" -a --strict -n "XXX" | grep "strict mode:.*/directory" 1>/dev/null

################################################################################

echo Tests PASSED
