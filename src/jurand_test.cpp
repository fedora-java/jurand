#include <iostream>
#include <sstream>

#include "java_symbols.hpp"

using namespace java_symbols;

static void assert_eq(const auto& expected, const auto& actual)
{
	if (expected != actual)
	{
		auto message = std::stringstream();
		message << "Test failed: different values: expected: " << expected << ", actual: " << actual;
		throw std::runtime_error(std::move(message).str());
	}
}

static void assert_that(bool value)
{
	if (not value)
	{
		throw std::runtime_error("Test failed");
	}
}

int main()
{
	std::cout << "Running tests..." << "\n";
	
	using next_annotation_t = std::tuple<std::string_view, std::string>;
	
	assert_eq(0, ignore_whitespace_comments("a", 0));
	assert_eq(1, ignore_whitespace_comments("ab", 1));
	assert_eq(0, ignore_whitespace_comments("/", 0));
	assert_eq(0, ignore_whitespace_comments("*", 0));
	assert_eq(2, ignore_whitespace_comments("//", 0));
	assert_eq(4, ignore_whitespace_comments("/**/", 0));
	assert_eq(7, ignore_whitespace_comments("/* a */", 0));
	assert_eq(5, ignore_whitespace_comments("/**/ a", 0));
	assert_eq(4, ignore_whitespace_comments("//a\n", 0));
	
	assert_eq("", std::get<0>(next_symbol("")));
	assert_eq("", std::get<0>(next_symbol(" ")));
	assert_eq("(", std::get<0>(next_symbol("(foo")));
	assert_eq("foo", std::get<0>(next_symbol("foo")));
	assert_eq("foo", std::get<0>(next_symbol(" foo ")));
	assert_eq("foo", std::get<0>(next_symbol("//\n\nfoo")));
	assert_eq("foo", std::get<0>(next_symbol("/* */ foo ")));
	
	assert_eq(0, find_token("@", "@"));
	assert_eq(1, find_token(" @", "@"));
	assert_eq(1, find_token("(@)", "@"));
	assert_eq(3, find_token("//\n@", "@"));
	assert_eq(6, find_token("/*\n*/\n@", "@"));
	
	assert_eq(3, find_token("' '@", "@"));
	assert_eq(4, find_token("'\''@", "@"));
	assert_eq(8, find_token("'\\uFFFE'@", "@"));
	assert_eq(4, find_token("\"//\"@", "@"));
	assert_eq(4, find_token("\"/*\"@", "@"));
	
	assert_eq(2, find_token("())", ")"));
	assert_eq(1, find_token("()", ")", 1));
	assert_eq(4, find_token("(()))", ")"));
	assert_eq(3, find_token("'\"'@", "@"));
	
	assert_eq(4, find_token("// @", "@"));
	assert_eq(5, find_token("// @\n", "@"));
	
	assert_eq(5, find_token("/*@*/", "@"));
	assert_eq(7, find_token("/* @ */", "@"));
	assert_eq(7, find_token("/*\n@ */", "@"));
	assert_eq(6, find_token("// /*@", "@"));
	assert_eq(10, find_token("/**//*@ */", "@"));
	assert_eq(7, find_token("/**///@", "@"));
	
	assert_eq(3, find_token("'@'", "@"));
	assert_eq(8, find_token(R"('\uFFFE')", R"(\u)"));
	assert_eq(4, find_token(R"('\'')", R"(\')"));
	assert_eq(3, find_token(R"("@")", "@"));
	assert_eq(5, find_token(R"("""@")", "@"));
	assert_eq(6, find_token(R"("" "@")", "@"));
	assert_eq(8, find_token(R"("\\" "@")", "@"));
	assert_eq(10, find_token(R"("\\\"" "@")", "@"));
	
	assert_eq(2, find_token("()", ")"));
	assert_eq(4, find_token("(())", ")"));
	
	assert_eq(8, find_token("noimport", "import", 0, true));
	assert_eq(7, find_token("_import", "import", 0, true));
	assert_eq(1, find_token("/import", "import", 0, true));
	assert_eq(1, find_token("+import", "import", 0, true));
	
	assert_eq(9, find_token("importnot", "import", 0, true));
	assert_eq(7, find_token("import_", "import", 0, true));
	assert_eq(0, find_token("import/", "import", 0, true));
	assert_eq(0, find_token("import+", "import", 0, true));
	
	assert_that(next_annotation_t("@A", "A") == next_annotation("@A"));
	assert_that(next_annotation_t("@A", "A") == next_annotation("@A\n"));
	assert_that(next_annotation_t("@A()", "A") == next_annotation("@A()"));
	
	assert_that(next_annotation_t("@A", "A") == next_annotation("@A class B {}"));
	
	assert_that(next_annotation_t("@A(a = ')')", "A") == next_annotation("@A(a = ')')"));
	assert_that(next_annotation_t("@A(a = ')')", "A") == next_annotation("@A(a = ')') class B {}"));
	
	assert_that(next_annotation_t("@A(a = \")\")", "A") == next_annotation("@A(a = \")\")"));
	assert_that(next_annotation_t("@A(a = \")))\" /*)))*/)", "A") == next_annotation("@A(a = \")))\" /*)))*/) class B {}"));
	assert_that(next_annotation_t("@A(/* ) */)", "A") == next_annotation("@A(/* ) */)"));
	assert_that(next_annotation_t("@A", "A") == next_annotation("method(@A Object o)"));
	
	assert_that(next_annotation_t("@A(\nvalue = \")\" /* ) */\n// )\n)", "A") == next_annotation("@A(\nvalue = \")\" /* ) */\n// )\n)\n"));
	
	assert_that(next_annotation_t("@D", "D") == next_annotation(" // @A\n/* @B */\nvalue = \"@C\";\n@D"));
	
	assert_that(next_annotation_t("@a.b.C", "a.b.C") == next_annotation("@a.b.C"));
	assert_that(next_annotation_t("@a/**/.B", "a.B") == next_annotation("@a/**/.B"));
	
	assert_that(next_annotation_t("@A(value = /* ) */ \")\")", "A") == next_annotation("@A(value = /* ) */ \")\")//)"));
	
	{
		constexpr std::string_view original_content = R"(
import java.lang.Runnable;
import java.util.List;
import static java.util.*;
import static java.lang.String.valueOf;
import com.google.common.util.concurrent.Service;)";
		
		auto args = std::vector<Named_regex>();
		
		args.emplace_back("Runnable");

		assert_eq(R"(
import java.util.List;
import static java.util.*;
import static java.lang.String.valueOf;
import com.google.common.util.concurrent.Service;)", std::get<0>(remove_imports(original_content, args, {})));
		args.clear();
		
		args.emplace_back("[*]");
		
		assert_eq(R"(
import java.lang.Runnable;
import java.util.List;
import static java.lang.String.valueOf;
import com.google.common.util.concurrent.Service;)", std::get<0>(remove_imports(original_content, args, {})));
		args.clear();
		
		args.emplace_back("java[.]util");
		assert_eq(R"(
import java.lang.Runnable;
import static java.lang.String.valueOf;
import com.google.common.util.concurrent.Service;)", std::get<0>(remove_imports(original_content, args, {})));
		args.clear();
		
		args.emplace_back("util");
		assert_eq(R"(
import java.lang.Runnable;
import static java.lang.String.valueOf;
)", std::get<0>(remove_imports(original_content, args, {})));
		args.clear();
		
		args.emplace_back("java");
		assert_eq(R"(
import com.google.common.util.concurrent.Service;)", std::get<0>(remove_imports(original_content, args, {})));
		args.clear();
		
		args.emplace_back("static");
		assert_eq(original_content, std::get<0>(remove_imports(original_content, args, {})));
		args.clear();
		
		args.emplace_back("A");
		assert_eq("", std::get<0>(remove_imports("import A ;", args, {})));
		args.clear();
		
		args.emplace_back("A");
		assert_eq(" ", std::get<0>(remove_imports("import A ; ", args, {})));
		args.clear();
		
		args.emplace_back("A");
		assert_eq("", std::get<0>(remove_imports("import/**/A;", args, {})));
		args.clear();
		
		args.emplace_back("A");
		assert_eq("/**/", std::get<0>(remove_imports("import/**/A/**/;/**/", args, {})));
		args.clear();
		
		args.emplace_back("A");
		assert_eq("", std::get<0>(remove_imports("import//\nA;", args, {})));
		args.clear();
		
		args.emplace_back("A[.]C");
		assert_eq("", std::get<0>(remove_imports("import A./*B;*/C;", args, {})));
		args.clear();
		
		args.emplace_back("A");
		assert_eq("", std::get<0>(remove_imports("import static A;", args, {})));
		args.clear();
		
		args.emplace_back("A");
		assert_eq("", std::get<0>(remove_imports("import static a . b /**/ . A;", args, {})));
		args.clear();
		
		args.emplace_back("static");
		assert_eq("", std::get<0>(remove_imports("import xstatic .A;", args, {})));
		args.clear();
		
		args.emplace_back("static");
		assert_eq("", std::get<0>(remove_imports("import staticx.A;", args, {})));
		args.clear();
		
		args.emplace_back("A");
		assert_eq("", std::get<0>(remove_imports("import static/**/A;", args, {})));
		args.clear();
		
		args.emplace_back("A");
		assert_eq("", std::get<0>(remove_imports("import/**/static/**/A;", args, {})));
		args.clear();
		
		args.emplace_back("A");
		assert_eq("import/* A */B;", std::get<0>(remove_imports("import/* A */B;", args, {})));
		args.clear();
	}
	
	{
		auto patterns = std::vector<Named_regex>();
		
		patterns.emplace_back("Nullable");
		assert_eq("new Object[initialCapacity];", remove_annotations("new @Nullable Object[initialCapacity];", patterns, {}, {}));
		patterns.clear();
		
		patterns.emplace_back("A");
		assert_eq("//)", remove_annotations("@A(value = /* ) */ \")\")//)", patterns, {}, {}));
		patterns.clear();
		
		patterns.emplace_back("A");
		assert_eq("\nclass C {}", remove_annotations(R"(
@A
class C {})", patterns, {}, {}));
		patterns.clear();
	
		patterns.emplace_back("A");
		assert_eq("\n	class C {}", remove_annotations(R"(
	@A
	class C {})", patterns, {}, {}));
		patterns.clear();
		
		constexpr std::string_view original_content = R"(
@SuppressWarnings
@SuppressFBWarnings(value = {"EI_EXPOSE_REP", "EI_EXPOSE_REP2"})
@org.junit.Test
@org.junit.jupiter.api.Test)";
		
		patterns.emplace_back("SuppressWarnings");
		assert_eq(R"(
@SuppressFBWarnings(value = {"EI_EXPOSE_REP", "EI_EXPOSE_REP2"})
@org.junit.Test
@org.junit.jupiter.api.Test)", remove_annotations(original_content, patterns, {}, {}));
		patterns.clear();
		
		patterns.emplace_back("Suppress");
		assert_eq(R"(
@org.junit.Test
@org.junit.jupiter.api.Test)", remove_annotations(original_content, patterns, {}, {}));
		patterns.clear();
		
		patterns.emplace_back("org[.]junit[.]Test");
		assert_eq(R"(
@SuppressWarnings
@SuppressFBWarnings(value = {"EI_EXPOSE_REP", "EI_EXPOSE_REP2"})
@org.junit.jupiter.api.Test)", remove_annotations(original_content, patterns, {}, {}));
		patterns.clear();
		
		patterns.emplace_back("Test");
		assert_eq(R"(
@SuppressWarnings
@SuppressFBWarnings(value = {"EI_EXPOSE_REP", "EI_EXPOSE_REP2"})
)", remove_annotations(original_content, patterns, {}, {}));
		patterns.clear();
		
		patterns.emplace_back("@SuppressWarnings");
		assert_eq(original_content, remove_annotations(original_content, patterns, {}, {}));
		patterns.clear();
		
		patterns.emplace_back("EI_EXPOSE_REP");
		assert_eq(original_content, remove_annotations(original_content, patterns, {}, {}));
		patterns.clear();
		
		patterns.emplace_back("A");
		assert_eq("@a/*A*/.B", remove_annotations("@a/*A*/.B", patterns, {}, {}));
		patterns.clear();
		
		patterns.emplace_back("B");
		assert_eq("", remove_annotations("@a/*A*/.B", patterns, {}, {}));
		patterns.clear();
		
		patterns.emplace_back("A");
		assert_eq("", remove_annotations("@ A", patterns, {}, {}));
		patterns.clear();
		
		patterns.emplace_back("A");
		assert_eq("", remove_annotations("@//\nA", patterns, {}, {}));
		patterns.clear();
		
		patterns.emplace_back("B");
		assert_eq("@A/*(B)*/", remove_annotations("@A/*(B)*/", patterns, {}, {}));
		patterns.clear();
	}
	
	std::cout << "[PASS] Unit tests" << "\n";
}
