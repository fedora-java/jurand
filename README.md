# Jurand -- Java removal of annotations

A tool for manipulating symbols present in `.java` source files.

The tool can be used for patching `.java` sources in cases where using `sed` is
insufficient due to Java language syntax. The tool follows Java language rules
rather than applying simple regular expressions on the source code.

Currently the tool is able to remove `import` statements and annotations.

## Usage

```
jurand [optional flags] <matcher>... [file path]...
    Matcher:
        -n <name>
                simple (not fully-qualified) class name
        -p <pattern>
                regex pattern to match names used in code
    
    Optional flags:
        -a      also remove annotations used in code
        -i, --in-place
                replace the contents of files
        --strict
                (wih -i only) fail if any of the specified options was redundant
                and no changes associated with the option were made
```

The tool removes import statements of specified matching class names and if
**`-a`** is specified then also removes annotations. Arguments can be specified
in arbitrary order.

File path arguments are handled the following way:

* Symlinks are ignored
* Regular files are handled regardless of the file name
* Directories are traversed recursively and all `.java` files are handled

If no file path is provided then the tool reads from the standard input.

Class name matching can be done two ways: **`-n`** to match simple class names
or **`-p`** to match the fully qualified name (if at all present in that form in
the sources) against the provided (extended POSIX) regex pattern. These options
can be specified multiple times.

The tool writes the results to standard output unless **`-i`** option is
specified in which case it will replace the original files' content.

Additionally, when doing in-place modifications, it is possible to also specify
**`--strict`** which will cause the tool invocation to fail in the following
cases:

* No changes were made to any of the user-provided file or directory subtree
*  One of the matchers did not match anything
*  **`-a`** was specified but no annotation was removed

Unicode literals (`\uXXXX`) are currently not recognized.
