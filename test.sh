#!/bin/bash

set -e

jurand_bin=${jurand_bin:-./target/debug/jurand}

rm -rf target/test_resources
mkdir -p target/test_resources

run_tool()
{
	local filename="${1}"; shift
	cp -r "test_resources/${filename}" "target/test_resources/${filename}"
	${jurand_bin} -i "target/test_resources/${filename}" "${@}"
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
	run_tool "${filename}" -s "${@}" | grep "strict mode" 1>/dev/null
}

################################################################################
# Tests for simple invocations and return codes

if ! ${jurand_bin} | grep 'Usage:'; then
	echo "[FAIL] Usage string not printed"
	exit 1
fi

if ! ${jurand_bin} -h | grep 'Usage:'; then
	echo "[FAIL] Usage string not printed"
	exit 1
fi

if ${jurand_bin} -i; then
	echo "[FAIL] Should have failed"
	exit 1
fi

if ${jurand_bin} -i -n 'D'; then
	echo "[FAIL] Should have failed"
	exit 1
fi

if ${jurand_bin} -n; then
	echo "[FAIL] Should have failed"
	exit 1
fi

if ${jurand_bin} -p; then
	echo "[FAIL] Should have failed"
	exit 1
fi

if ${jurand_bin} nonexisting_file -n "A"; then
	echo "[FAIL] Should have failed"
	exit 1
fi

if [ -n "$(echo "import A;" | ${jurand_bin} -n "A")" ]; then
	echo "[FAIL] Output should be empty"
	exit 1
fi

if [ "$(echo "import A;" | ${jurand_bin} -n "B")" != "import A;" ]; then
	echo "[FAIL] Output should be identical to input"
	exit 1
fi

{
	cp "test_resources/Simple.java" "target/test_resources/Simple.java"
	
	if ! ${jurand_bin} -a -n 'D' "target/test_resources/Simple.java" | grep "target/test_resources/Simple.java"; then
		echo "[FAIL] Should have printed the file name"
		exit 1
	fi
	
	rm -f "target/test_resources/Simple.java"
}
{
	cp -r "test_resources/directory/resources" -t "target/test_resources"
	
	if ${jurand_bin} -i -n 'D' "target/test_resources/resources"; then
		echo "[FAIL] Should have failed"
		exit 1
	fi
	
	rm -rf "target/test_resources/resources"
}

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

test_file "Array.java" "Array.1.java" -a -n C
test_file "Array.java" "Array.2.java" -a -n D
test_file "Array.java" "Array.3.java" -a -n E
test_file "Array.java" "Array.4.java" -a -n F
test_file "Array.java" "Array.5.java" -a -n C -n D -n E -n F

################################################################################
# Tests for tool termination on invalid sources, result is irrelevant

run_tool "Termination.1.java" -n "C" || :
run_tool "Termination.2.java" -a -n "C" || :
run_tool "Termination.3.java" -a -n "C" || :
run_tool "Termination.4.java" -a -n "C" || :
run_tool "Termination.5.java" -a -n "C" || :
run_tool "Termination.6.java" -a -n "C" || :
run_tool "Termination.7.java" -a -n "C" || :
run_tool "Termination.8.java" -a -n "C" || :

################################################################################
# Tests of directory traversal

run_tool "directory" -a -n "Annotation"
for filename in A a/B a/b/C; do
	diff -u "target/test_resources/directory/${filename}.java" "target/test_resources/directory/${filename}.1.java"
done

################################################################################
# Tests of strict mode

# Succesful for coverage
test_file "Simple.java" "Simple.1.java" -a -s -n "D"

# Nothing was matched/removed
test_strict "Strict.1.java" "Strict.1.java" -p "z"
test_strict "Strict.1.java" "Strict.1.java" -n "z"
test_strict "Strict.2.java" "Strict.2.java" -p "z"
test_strict "Strict.2.java" "Strict.2.java" -n "z"
# -a but no annotation was removed
test_strict "Strict.2.java" "Strict.2.java" -a -p "a"
test_strict "Strict.2.java" "Strict.2.java" -a -n "C"
# Should print the directory name
run_tool "directory" -a -s -n "XXX" | grep "strict mode:.*/directory" 1>/dev/null

################################################################################

echo "[PASS] Integration tests"
