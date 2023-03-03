# Jurand -- Java removal of annotations

A tool for manipulating symbols present in `.java` source files.

## Usage

	jurand [-a] [list of file paths]... [-n <list of class names>...] [-p <list of patterns>...]
		-a		also remove annotations used in code
		-n		list of simple (not fully-qualified) class names
		-p		list of patterns to match names used in code`
		
		-i, --in-place
				replace the contents of files

The tool removes import statements of specified matching class names and if `-a`
is specified then also removes annotations.

Class name matching can be done two ways: `-n` to match simple class names or
`-p` to match the fully qualified name (if at all present in that form in the
sources) against the provided regex pattern.

The tool writes the results to standard output unless `-i` option is specified
in which case it will replace the original files' content.

If any of the file paths is a directory then the tool does recursive file search
itself.
