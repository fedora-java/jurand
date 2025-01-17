#!/bin/bash

set -eux

readonly metafile="target/installed_files"

install_file()
{
	local attr="${1}"; shift
	local target_dir="${1}"; shift
	local suffix
	
	for source_file in "${@}"; do
		local final_dir
		final_dir="${target_dir}"
		if [ "${target_dir}" = "${mandir}" ]; then
			suffix=".gz"
			final_dir+="/man${source_file##*.}"
		else
			suffix=""
		fi
		install -m "${attr}" -p -D -t "${buildroot}/${final_dir}" "${source_file}"
		echo "${final_dir}/${source_file##*/}${suffix:-}" >> "${metafile}"
	done
}

install_file 755 "${bindir}" target/bin/jurand
install_file 644 "${rpmmacrodir}" macros/macros.jurand
install_file 644 "${mandir}" target/manpages/*
