MAKEFLAGS += -r

Dependency_file = $(addprefix target/dependencies/,$(addsuffix .mk,$(subst /,.,$(basename $(1)))))
Object_file = $(addprefix target/object_files/,$(addsuffix .o,$(subst /,.,$(basename $(1)))))
Executable_file = $(addprefix target/bin/,$(addsuffix ,$(subst /,.,$(basename $(1)))))
Manpage_7 = $(addprefix target/manpages/,$(addsuffix .7,$(1)))

target target/object_files target/dependencies target/bin target/coverage target/manpages:
	@mkdir -p $@

define Variable_rule # target_file, string_value
$(1): force | target
	@echo '$(2)' | cmp -s - $$@ || echo '$(2)' > $$@
endef

target/manpages/%.xml: manpages/%.adoc | target/manpages
	asciidoc -b docbook -d manpage -o $@ $<

target/manpages/%.7: target/manpages/%.xml | target/manpages
	xmlto man --skip-validation -o target/manpages target/manpages/$(*F).xml

# $(call Object_file,%) $(call Dependency_file,%)&: src/%.cpp target/compile_flags | target/object_files target/dependencies
$(call Object_file,%): src/%.cpp target/compile_flags | target/object_files target/dependencies
	$(CXX) $(CXXFLAGS) -MMD -MP -MF $(call Dependency_file,$(<F)) -MT $(call Object_file,$(<F)) -c -o $(call Object_file,$(<F)) $(addprefix src/,$(<F))

$(call Executable_file,%): target/link_flags | target/bin
	$(CXX) -o $@ $(LDFLAGS) $(wordlist 2,$(words $^),$^) $(LDLIBS)

coverage: CXXFLAGS += --coverage -fno-elide-constructors -fno-default-inline
coverage: LDFLAGS += --coverage
coverage: test | target/coverage
	@lcov --output-file target/coverage.info --directory target/object_files --capture --exclude '/usr/include/*'
	@genhtml -o target/coverage target/coverage.info
