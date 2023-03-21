use crate::java_symbols::*;

#[test]
fn test_ignore_whitespace_comments()
{
	assert_eq!(0, ignore_whitespace_comments(b"a", 0));
	assert_eq!(1, ignore_whitespace_comments(b"ab", 1));
	assert_eq!(0, ignore_whitespace_comments(b"/", 0));
	assert_eq!(0, ignore_whitespace_comments(b"*", 0));
	assert_eq!(2, ignore_whitespace_comments(b"//", 0));
	assert_eq!(4, ignore_whitespace_comments(b"/**/", 0));
	assert_eq!(7, ignore_whitespace_comments(b"/* a */", 0));
	assert_eq!(5, ignore_whitespace_comments(b"/**/ a", 0));
	assert_eq!(4, ignore_whitespace_comments(b"//a\n", 0));
}

#[test]
fn test_next_symbol()
{
	assert_eq!(b"", next_symbol(b"", 0).0);
	assert_eq!(b"", next_symbol(b" ", 0).0);
	assert_eq!(b"(", next_symbol(b"(foo", 0).0);
	assert_eq!(b"foo", next_symbol(b"foo", 0).0);
	assert_eq!(b"foo", next_symbol(b" foo ", 0).0);
	assert_eq!(b"foo", next_symbol(b"//\n\nfoo", 0).0);
	assert_eq!(b"foo", next_symbol(b"/* */ foo ", 0).0);
}

#[test]
fn test_find_token()
{
	assert_eq!(0, find_token(b"@", "@", 0, false, 0));
	assert_eq!(1, find_token(b" @", "@", 0, false, 0));
	assert_eq!(1, find_token(b"(@)", "@", 0, false, 0));
	assert_eq!(3, find_token(b"//\n@", "@", 0, false, 0));
	assert_eq!(6, find_token(b"/*\n*/\n@", "@", 0, false, 0));
	
	assert_eq!(3, find_token(b"' '@", "@", 0, false, 0));
	assert_eq!(4, find_token(b"'\''@", "@", 0, false, 0));
	assert_eq!(8, find_token(b"'\\uFFFE'@", "@", 0, false, 0));
	assert_eq!(4, find_token(b"\"//\"@", "@", 0, false, 0));
	assert_eq!(4, find_token(b"\"/*\"@", "@", 0, false, 0));
	
	assert_eq!(2, find_token(b"())", ")", 0, false, 0));
	assert_eq!(1, find_token(b"()", ")", 1, false, 0));
	assert_eq!(4, find_token(b"(()))", ")", 0, false, 0));
	assert_eq!(3, find_token(b"'\"'@", "@", 0, false, 0));
	
	assert_eq!(4, find_token(b"// @", "@", 0, false, 0));
	assert_eq!(5, find_token(b"// @\n", "@", 0, false, 0));
	
	assert_eq!(5, find_token(b"/*@*/", "@", 0, false, 0));
	assert_eq!(7, find_token(b"/* @ */", "@", 0, false, 0));
	assert_eq!(7, find_token(b"/*\n@ */", "@", 0, false, 0));
	assert_eq!(6, find_token(b"// /*@", "@", 0, false, 0));
	assert_eq!(10, find_token(b"/**//*@ */", "@", 0, false, 0));
	assert_eq!(7, find_token(b"/**///@", "@", 0, false, 0));
	assert_eq!(3, find_token(b"'@'", "@", 0, false, 0));
	
	assert_eq!(8, find_token(b"'\\uFFFE'", "\\u", 0, false, 0));
	assert_eq!(4, find_token(b"'\\''", "\\'", 0, false, 0));
	assert_eq!(3, find_token(b"\"@\"", "@", 0, false, 0));
	assert_eq!(5, find_token(b"\"\"\"@\"", "@", 0, false, 0));
	assert_eq!(6, find_token(b"\"\" \"@\"", "@", 0, false, 0));
	assert_eq!(8, find_token(b"\"\\\\\" \"@\"", "@", 0, false, 0));
	assert_eq!(10, find_token(b"\"\\\\\\\"\" \"@\"", "@", 0, false, 0));
	
	assert_eq!(2, find_token(b"()", ")", 0, false, 0));
	assert_eq!(4, find_token(b"(())", ")", 0, false, 0));
	
	assert_eq!(8, find_token(b"noimport", "import", 0, true, 0));
	assert_eq!(7, find_token(b"_import", "import", 0, true, 0));
	assert_eq!(1, find_token(b"/import", "import", 0, true, 0));
	assert_eq!(1, find_token(b"+import", "import", 0, true, 0));
	
	assert_eq!(9, find_token(b"importnot", "import", 0, true, 0));
	assert_eq!(7, find_token(b"import_", "import", 0, true, 0));
	assert_eq!(0, find_token(b"import/", "import", 0, true, 0));
	assert_eq!(0, find_token(b"import+", "import", 0, true, 0));
}

#[test]
fn test_next_annotation()
{
	fn assert_eq(expected_annotation: &str, expected_annotation_name: &str, expression: &str)
	{
		let (annotation, annotation_name) = next_annotation(expression.as_bytes(), 0);
		assert_eq!(expected_annotation.as_bytes(), annotation);
		assert_eq!(expected_annotation_name.as_bytes(), annotation_name);
	}
	
	assert_eq("@A", "A", "@A");
	assert_eq("@A", "A", "@A\n");
	assert_eq("@A()", "A", "@A()");
	
	assert_eq("@A", "A", "@A class B {}");
	
	assert_eq("@A(a = ')')", "A", "@A(a = ')')");
	assert_eq("@A(a = ')')", "A", "@A(a = ')') class B {}");
	
	assert_eq("@A(a = \")\")", "A", "@A(a = \")\")");
	assert_eq("@A(a = \")))\" /*)))*/)", "A", "@A(a = \")))\" /*)))*/) class B {}");
	assert_eq("@A(/* ) */)", "A", "@A(/* ) */)");
	assert_eq("@A", "A", "method(@A Object o)");
	
	assert_eq("@A(\nvalue = \")\" /* ) */\n// )\n)", "A", "@A(\nvalue = \")\" /* ) */\n// )\n)\n");
	
	assert_eq("@D", "D", " // @A\n/* @B */\nvalue = \"@C\";\n@D");
	
	assert_eq("@a.b.C", "a.b.C", "@a.b.C");
	assert_eq("@a/**/.B", "a.B", "@a/**/.B");
	
	assert_eq("@A(value = /* ) */ \")\")", "A", "@A(value = /* ) */ \")\")//)");
}

#[test]
fn test_remove_imports()
{
	let original_content = b"
import java.lang.Runnable;
import java.util.List;
import static java.util.*;
import static java.lang.String.valueOf;
import com.google.common.util.concurrent.Service;
";
	
	assert_eq!(b"
import java.util.List;
import static java.util.*;
import static java.lang.String.valueOf;
import com.google.common.util.concurrent.Service;
",
	remove_imports(original_content, &[regex::bytes::Regex::new("Runnable").unwrap()],
		&std::collections::BTreeSet::new()).0.as_slice()
	);
	
	assert_eq!(b"
import java.lang.Runnable;
import java.util.List;
import static java.lang.String.valueOf;
import com.google.common.util.concurrent.Service;
",
	remove_imports(original_content, &[regex::bytes::Regex::new("[*]").unwrap()],
		&std::collections::BTreeSet::new()).0.as_slice()
	);
	
	assert_eq!(b"
import java.lang.Runnable;
import static java.lang.String.valueOf;
import com.google.common.util.concurrent.Service;
",
	remove_imports(original_content, &[regex::bytes::Regex::new("java[.]util").unwrap()],
		&std::collections::BTreeSet::new()).0.as_slice()
	);
	
	assert_eq!(b"
import java.lang.Runnable;
import static java.lang.String.valueOf;
",
	remove_imports(original_content, &[regex::bytes::Regex::new("util").unwrap()],
		&std::collections::BTreeSet::new()).0.as_slice()
	);
	
	assert_eq!(b"
import com.google.common.util.concurrent.Service;
",
	remove_imports(original_content, &[regex::bytes::Regex::new("java").unwrap()],
		&std::collections::BTreeSet::new()).0.as_slice()
	);
	
	assert_eq!(original_content, remove_imports(original_content, &[regex::bytes::Regex::new("static").unwrap()],
		&std::collections::BTreeSet::new()).0.as_slice()
	);
	
	assert_eq!(b"", remove_imports(b"import A ;", &[regex::bytes::Regex::new("A").unwrap()],
		&std::collections::BTreeSet::new()).0.as_slice()
	);
	
	assert_eq!(b" ", remove_imports(b"import A ; ", &[regex::bytes::Regex::new("A").unwrap()],
		&std::collections::BTreeSet::new()).0.as_slice()
	);
	
	assert_eq!(b"", remove_imports(b"import/**/A;", &[regex::bytes::Regex::new("A").unwrap()],
		&std::collections::BTreeSet::new()).0.as_slice()
	);
	
	assert_eq!(b"/**/", remove_imports(b"import/**/A/**/;/**/", &[regex::bytes::Regex::new("A").unwrap()],
		&std::collections::BTreeSet::new()).0.as_slice()
	);
	
	assert_eq!(b"", remove_imports(b"import//\nA;", &[regex::bytes::Regex::new("A").unwrap()],
		&std::collections::BTreeSet::new()).0.as_slice()
	);
	
	assert_eq!(b"", remove_imports(b"import A./*B;*/C;", &[regex::bytes::Regex::new("A[.]C").unwrap()],
		&std::collections::BTreeSet::new()).0.as_slice()
	);
	
	assert_eq!(b"", remove_imports(b"import static A;", &[regex::bytes::Regex::new("A").unwrap()],
		&std::collections::BTreeSet::new()).0.as_slice()
	);
	
	assert_eq!(b"", remove_imports(b"import static a . b /**/ . A;", &[regex::bytes::Regex::new("A").unwrap()],
		&std::collections::BTreeSet::new()).0.as_slice()
	);
	
	assert_eq!(b"", remove_imports(b"import xstatic .A;", &[regex::bytes::Regex::new("static").unwrap()],
		&std::collections::BTreeSet::new()).0.as_slice()
	);
	
	assert_eq!(b"", remove_imports(b"import staticx.A;", &[regex::bytes::Regex::new("static").unwrap()],
		&std::collections::BTreeSet::new()).0.as_slice()
	);
	
	assert_eq!(b"", remove_imports(b"import static/**/A;", &[regex::bytes::Regex::new("A").unwrap()],
		&std::collections::BTreeSet::new()).0.as_slice()
	);
	
	assert_eq!(b"", remove_imports(b"import/**/static/**/A;", &[regex::bytes::Regex::new("A").unwrap()],
		&std::collections::BTreeSet::new()).0.as_slice()
	);
	
	assert_eq!(b"import/* A */B;", remove_imports(b"import/* A */B;", &[regex::bytes::Regex::new("A").unwrap()],
		&std::collections::BTreeSet::new()).0.as_slice()
	);
}

#[test]
fn test_remove_annotations()
{
	assert_eq!(b"new Object[initialCapacity];", remove_annotations(
		b"new @Nullable Object[initialCapacity];",
		&[regex::bytes::Regex::new("Nullable").unwrap()],
		&std::collections::BTreeSet::new(), &std::collections::BTreeMap::new()).as_slice()
	);
	
	assert_eq!(b"//)", remove_annotations(
		b"@A(value = /* ) */ \")\")//)",
		&[regex::bytes::Regex::new("A").unwrap()],
		&std::collections::BTreeSet::new(), &std::collections::BTreeMap::new()).as_slice()
	);
	
	assert_eq!(b"\nclass C {}", remove_annotations(
		b"
@A
class C {}",
		&[regex::bytes::Regex::new("A").unwrap()],
		&std::collections::BTreeSet::new(), &std::collections::BTreeMap::new()).as_slice()
	);
	
	assert_eq!(b"\n	class C {}", remove_annotations(
		b"
	@A
	class C {}",
		&[regex::bytes::Regex::new("A").unwrap()],
		&std::collections::BTreeSet::new(), &std::collections::BTreeMap::new()).as_slice()
	);
	
	assert_eq!(b"@a/*A*/.B", remove_annotations(b"@a/*A*/.B",
		&[regex::bytes::Regex::new("A").unwrap()],
		&std::collections::BTreeSet::new(), &std::collections::BTreeMap::new()).as_slice()
	);
	
	assert_eq!(b"", remove_annotations(b"@a/*A*/.B",
		&[regex::bytes::Regex::new("B").unwrap()],
		&std::collections::BTreeSet::new(), &std::collections::BTreeMap::new()).as_slice()
	);
	
	assert_eq!(b"", remove_annotations(b"@ A",
		&[regex::bytes::Regex::new("A").unwrap()],
		&std::collections::BTreeSet::new(), &std::collections::BTreeMap::new()).as_slice()
	);
	
	assert_eq!(b"", remove_annotations(b"@//\nA",
		&[regex::bytes::Regex::new("A").unwrap()],
		&std::collections::BTreeSet::new(), &std::collections::BTreeMap::new()).as_slice()
	);
	
	assert_eq!(b"@A/*(B)*/", remove_annotations(b"@A/*(B)*/",
		&[regex::bytes::Regex::new("B").unwrap()],
		&std::collections::BTreeSet::new(), &std::collections::BTreeMap::new()).as_slice()
	);
	
	let original_content = b"
@SuppressWarnings
@SuppressFBWarnings(value = {\"EI_EXPOSE_REP\", \"EI_EXPOSE_REP2\"})
@org.junit.Test
@org.junit.jupiter.api.Test
";
	
	assert_eq!(b"
@SuppressFBWarnings(value = {\"EI_EXPOSE_REP\", \"EI_EXPOSE_REP2\"})
@org.junit.Test
@org.junit.jupiter.api.Test
", remove_annotations(original_content, &[regex::bytes::Regex::new("SuppressWarnings").unwrap()],
		&std::collections::BTreeSet::new(), &std::collections::BTreeMap::new()).as_slice()
	);
	
	assert_eq!(b"
@org.junit.Test
@org.junit.jupiter.api.Test
", remove_annotations(original_content, &[regex::bytes::Regex::new("Suppress").unwrap()],
		&std::collections::BTreeSet::new(), &std::collections::BTreeMap::new()).as_slice()
	);
	
	assert_eq!(b"
@SuppressWarnings
@SuppressFBWarnings(value = {\"EI_EXPOSE_REP\", \"EI_EXPOSE_REP2\"})
@org.junit.jupiter.api.Test
", remove_annotations(original_content, &[regex::bytes::Regex::new("org[.]junit[.]Test").unwrap()],
		&std::collections::BTreeSet::new(), &std::collections::BTreeMap::new()).as_slice()
	);
	
	assert_eq!(b"
@SuppressWarnings
@SuppressFBWarnings(value = {\"EI_EXPOSE_REP\", \"EI_EXPOSE_REP2\"})

", remove_annotations(original_content, &[regex::bytes::Regex::new("Test").unwrap()],
		&std::collections::BTreeSet::new(), &std::collections::BTreeMap::new()).as_slice()
	);
}
