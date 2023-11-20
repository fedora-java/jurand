cog.shell = {"dash", "-eux", "-c"}
cog.clean = {"target/object_files"}

CXX = cog.expand(os.getenv("CXX") or "g++")
CXXFLAGS = cog.expand(os.getenv("CXXFLAGS"))
table.insert(CXXFLAGS, {"-g", "-std=c++2a", "-Isrc", "-Wall", "-Wextra", "-Wpedantic"})
LDFLAGS = cog.expand(os.getenv("LDFLAGS"))
LDLIBS = cog.expand(os.getenv("LDLIBS"))

if cog.targets["coverage"] then
	table.insert(CXXFLAGS, {"--coverage", "-fno-elide-constructors", "-fno-default-inline"})
	table.insert(LDFLAGS, {"--coverage"})
end

local function compile(targets, sources) return {
	CXX, CXXFLAGS, "-MD", "-MP", "-MF", cog.c_dependency_file_for(targets[1]), "-MT", targets[1], "-c", "-o", targets[1], sources[1]
} end

local function link(targets, sources) return {
	CXX, "-o", targets[1], LDFLAGS, sources, LDLIBS
} end

local function object_file(name)
	local result = "target/object_files/"..name..".o"
	return cog.rule(result, cog.c_dependency_file_for(result)):dep("src/"..name..".cpp", cog.c_prerequisites):recipe(compile)
end

local function executable(name, object_file)
	return cog.rule("target/bin/"..name):dep(object_file):recipe(link)
end

local function build_manpage(_, sources) return {
	"asciidoctor", "-b", "manpage", "-D", "target/manpages", sources[1],
} end

local function manpage(name)
	return cog.rule("target/manpages/"..name..".7"):dep("manpages/"..name..".adoc"):recipe(build_manpage)
end

cog.rule("build"):dep(executable("jurand", object_file("jurand")[1]))
cog.rule("test-compile"):dep(executable("jurand_test", object_file("jurand_test")[1]))
cog.rule("test", "target/test_resources"):dep("test.sh", "test-compile", "build"):recipe(function(_, sources) return {"./"..sources[1]} end)
cog.rule("manpages"):dep(manpage("java_remove_annotations"), manpage("java_remove_imports"))
cog.rule("target/coverage.info"):dep("test"):recipe {
	"lcov", "--output-file", "target/coverage.info", "--directory", "target/object_files", "--capture", "--exclude", "/usr/include/*",
}
cog.rule("target/coverage"):dep("target/coverage.info"):recipe {
	"genhtml", "-o", "target/coverage", "target/coverage.info",
}
cog.rule("coverage"):dep("target/coverage")

cog.rule("install", "target/installed_files"):dep("build", "manpages"):recipe([[
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
]])
