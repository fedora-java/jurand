#!/bin/bash

set -eux

mkdir -p target/manpages

for manpage in manpages/*; do
	manpage="${manpage##*/}"
	manpage="${manpage%.txt}"
	asciidoc -b docbook -d manpage -o "target/manpages/${manpage}.xml" "manpages/${manpage}.txt"
	xmlto man --skip-validation -o "target/manpages" "target/manpages/${manpage}.xml"
done
