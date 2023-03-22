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
	@mkdir -p target
	@echo '$(2)' | cmp -s - $$@ || echo '$(2)' > $$@
endef

define Dependency_file_rule # source_file
$(call Dependency_file,$(1)): src/$(1) target/compile_flags
	@mkdir -p target/dependencies
	$$(CXX) $$(CXXFLAGS) -MM -MG -MMD -MP -MF $$@ -MT $(call Object_file,$(1)) -o /dev/null $$<
endef

define Object_file_rule # source_file
$(call Dependency_file_rule,$(1))
$(call Object_file,$(1)): $(call Dependency_file,$(1))
	@mkdir -p target/object_files
	$$(CXX) $$(CXXFLAGS) -MMD -MP -MF $(call Dependency_file,$(1)) -MT $(call Object_file,$(1)) -c -o $$@ $(addprefix src/,$(1))
endef

define Executable_file_rule # source_files...
$(foreach source_file,$(2),
$(call Object_file_rule,$(source_file)))
$(call Executable_file,$(1)): $(call Object_file,$(2)) target/link_flags
	@mkdir -p target/bin
	$$(CXX) $$(LDFLAGS) -o $$@ $(call Object_file,$(2)) $$(LDLIBS)
endef
