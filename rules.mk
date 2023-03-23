define Dependency_file
$(addprefix target/dependencies/,$(addsuffix .mk,$(subst /,.,$(basename $(1)))))
endef

define Object_file
$(addprefix target/object_files/,$(addsuffix .o,$(subst /,.,$(basename $(1)))))
endef

define Executable_file
$(addprefix target/bin/,$(addsuffix ,$(subst /,.,$(basename $(1)))))
endef

define Variable_rule # target_file, string_value
$(1): force
	@mkdir -p $$(dir $$@)
	@echo '$(2)' | cmp -s - $$@ || echo '$(2)' > $$@
endef

define Manpage_rule # source_file
target/manpages/$(1).xml: manpages/$(1).txt
	@mkdir -p $$(dir $$@)
	asciidoc -b docbook -d manpage -o $$@ $$<

target/manpages/$(1).7: target/manpages/$(1).xml
	xmlto man --skip-validation -o target/manpages target/manpages/$(1).xml
manpages += target/manpages/$(1).7
endef

define Dependency_file_rule # source_file
$(call Dependency_file,$(1)): src/$(1)
	@mkdir -p $$(dir $$@)
	$$(CXX) $$(CXXFLAGS) -MM -MG -MMD -MP -MF $$@ -MT $(call Object_file,$(1)) -o /dev/null $$<
endef

define Object_file_rule # source_file
$(call Dependency_file_rule,$(1))
$(call Object_file,$(1)): $(call Dependency_file,$(1)) target/compile_flags
	@mkdir -p $$(dir $$@)
	$$(CXX) $$(CXXFLAGS) -MMD -MP -MF $$< -MT $$@ -c -o $$@ $(addprefix src/,$(1))
endef

define Executable_file_rule # executable_name, source_file, object_files...
$(call Object_file_rule,$(2))
$(call Executable_file,$(1)): target/link_flags $(call Object_file,$(2)) $(call Object_file,$(3))
	@mkdir -p $$(dir $$@)
	$$(CXX) -o $$@ $$(LDFLAGS) $$(wordlist 2,$$(words $$^),$$^) $$(LDLIBS)
endef
