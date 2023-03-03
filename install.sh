#!/bin/bash

set -eux

readonly metafile="target/installed_files"

install_file()
{
	local attr="${1}"; shift
	local target_dir="${1}"; shift
	
	for source_file in "${@}"; do
		install -m "${attr}" -p -D -t "${buildroot}/${target_dir}" "${source_file}"
		if [ "${target_dir}" = "${mandir}" ]; then
			local suffix=".gz"
		fi
		echo "${target_dir}/${source_file##*/}${suffix:-}" >> "${metafile}"
	done
}

install_file 755 "${bindir}" target/bin/jurand
install_file 644 "${rpmmacrodir}" macros/macros.jurand
install_file 644 "${mandir}" target/manpages/*.7
