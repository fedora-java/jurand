use std::io::Write;
use std::io::Read;

/// NOTE
/// \u000a -- '\n'
/// \u002a -- '*'
/// \u002e -- '.'
/// \u002f -- '/'
/// \u0040 -- '@'
/// \u005c -- '\'

pub fn ignore_whitespace_comments(content: &[u8], mut position: usize) -> usize
{
	while position != content.len()
	{
		if content[position].is_ascii_whitespace()
		{
			position += 1;
			continue;
		}
		
		let result = position;
		
		if content[position ..].starts_with(b"//")
		{
			position += 2;
			
			loop
			{
				if position == content.len()
				{
					return content.len();
				}
				else if content[position] == b'\n'
				{
					position += 1;
					break;
				}
				
				position += 1;
			}
		}
		else if content[position ..].starts_with(b"/*")
		{
			position += 2;
			
			loop
			{
				if position + 2 > content.len()
				{
					return content.len();
				}
				else if &content[position ..][.. 2] == b"*/"
				{
					position += 2;
					break;
				}
				
				position += 1;
			}
		}
		
		if result == position
		{
			return result;
		}
	}
	
	return position;
}

fn is_identifier_char(c: u8) -> bool
{
	return c == b'_' || (! c.is_ascii_punctuation() && ! c.is_ascii_whitespace());
}

pub fn next_symbol(content: &[u8], mut position: usize) -> (&[u8], usize)
{
	let mut symbol_length = 0;
	
	if position < content.len()
	{
		position = ignore_whitespace_comments(content, position);
		
		if position < content.len()
		{
			symbol_length = 1;
			
			if is_identifier_char(content[position])
			{
				while position + symbol_length != content.len() && is_identifier_char(content[position + symbol_length])
				{
					symbol_length += 1;
				}
			}
		}
	}
	
	return (&content[position ..][.. symbol_length], position + symbol_length);
}

pub fn find_token(content: &[u8], token: &str, mut position: usize, alphanumeric: bool, mut stack: usize) -> usize
{
	while position + token.len() <= content.len()
	{
		position = ignore_whitespace_comments(content, position);
		
		if position == content.len()
		{
			break;
		}
		
		if (token != ")" || stack == 0) && content[position ..].starts_with(token.as_bytes())
			&& ! (alphanumeric && ((position > 0 && is_identifier_char(content[position - 1]))
				|| (position + token.len() < content.len() && (is_identifier_char(content[position + token.len()])))))
		{
			return position;
		}
		else if content[position] == b'\''
		{
			if content[position ..].starts_with(b"'\\''")
			{
				position += 3;
			}
			else
			{
				loop
				{
					position += 1;
					
					if position >= content.len() || content[position] == b'\''
					{
						break;
					}
				}
			}
		}
		else if content[position] == b'"'
		{
			position += 1;
			
			while position < content.len() && content[position] != b'"'
			{
				if content[position ..].starts_with(b"\\\\")
				{
					position += 2;
				}
				else if content[position ..].starts_with(b"\\\"")
				{
					position += 2;
				}
				else
				{
					position += 1;
				}
			}
		}
		else if stack != 0 && content[position] == b')'
		{
			stack -= 1;
		}
		else if content[position] == b'('
		{
			stack += 1;
		}
		
		position += 1;
	}
	
	return content.len();
}

pub fn next_annotation(content: &[u8], mut position: usize) -> (&[u8], String)
{
	let mut result = String::new();
	let mut end_pos = content.len();
	position = find_token(content, "@", position, false, 0);
	let mut expecting_dot = false;
	
	if position < content.len()
	{
		let mut symbol;
		(symbol, end_pos) = next_symbol(content, position + 1);
		let mut new_end_pos = end_pos;
		
		while ! symbol.is_empty()
		{
			// catch `@A ...`
			if symbol == b"." && content[new_end_pos ..].starts_with(b"..")
			{
				break;
			}
			
			if expecting_dot && symbol != b"."
			{
				if symbol == b"("
				{
					end_pos = find_token(content, ")", new_end_pos, false, 0);
					
					if end_pos == content.len()
					{
						result.clear();
						position = end_pos;
						break;
					}
					
					end_pos += 1;
				}
				
				break;
			}
			
			result.push_str(std::str::from_utf8(symbol).unwrap());
			expecting_dot = ! expecting_dot;
			end_pos = new_end_pos;
			(symbol, new_end_pos) = next_symbol(content, new_end_pos);
		}
	}
	
	return (&content[position .. end_pos], result);
}

pub struct Parameters
{
	pub patterns: std::vec::Vec<regex::Regex>,
	pub names: std::collections::BTreeSet<String>,
	pub also_remove_annotations: bool,
	pub in_place: bool,
	pub strict_mode: bool,
}

#[derive(Debug)]
pub struct StrictMode
{
	pub any_annotation_removed: std::sync::atomic::AtomicBool,
	pub patterns_matched: std::sync::Mutex<std::collections::BTreeMap<String, bool>>,
	pub names_matched: std::sync::Mutex<std::collections::BTreeMap<String, bool>>,
	pub files_truncated: std::sync::Mutex<std::collections::BTreeMap<std::path::PathBuf, bool>>,
}

pub static STRICT_MODE: std::sync::OnceLock<StrictMode> = std::sync::OnceLock::new();

fn name_matches(name: &str, patterns: &[regex::Regex],
	names: &std::collections::BTreeSet<String>,
	imported_names: &std::collections::BTreeMap<String, String>) -> bool
{
	let mut simple_name = name;
	
	if let Some(pos) = name.rfind('.')
	{
		simple_name = &name[pos + 1 ..];
	}
	
	if names.contains(simple_name)
	{
		STRICT_MODE.get().map(|s| *s.names_matched.lock().unwrap().get_mut(simple_name).unwrap() = true);
		
		return true;
	}
	
	if let Some(imported_name) = imported_names.get(simple_name)
	{
		if name == simple_name || *imported_name == name
		{
			return true;
		}
	}
	
	for pattern in patterns
	{
		if pattern.is_match(name)
		{
			STRICT_MODE.get().map(|s| *s.patterns_matched.lock().unwrap().get_mut(pattern.as_str()).unwrap() = true);
			
			return true;
		}
	}
	
	return false;
}

pub fn remove_imports(content: &[u8], patterns: &[regex::Regex],
	names: &std::collections::BTreeSet<String>)
-> (std::vec::Vec<u8>, std::collections::BTreeMap<String, String>)
{
	let mut result = (std::vec::Vec::<u8>::new(), std::collections::BTreeMap::<String, String>::new());
	let (ref mut new_content, ref mut removed_classes) = result;
	new_content.reserve(content.len());
	let mut position: usize = 0;
	
	while position < content.len()
	{
		let mut next_position = find_token(content, "import", position, true, 0);
		let mut copy_end = content.len();
		
		if next_position < content.len()
		{
			let mut import_name = String::new();
			let (mut symbol, mut end_pos) = next_symbol(content, next_position + 6);
			
			let empty_set = std::collections::BTreeSet::new();
			let mut names_passed = names;
			
			let mut is_static = false;
			
			if symbol == b"static"
			{
				is_static = true;
				names_passed = &empty_set;
				(symbol, end_pos) = next_symbol(content, end_pos);
			}
			
			while symbol != b";"
			{
				if symbol.is_empty()
				{
					new_content.clear();
					new_content.extend_from_slice(content);
					removed_classes.clear();
					return result;
				}
				
				import_name.push_str(std::str::from_utf8(symbol).unwrap());
				(symbol, end_pos) = next_symbol(content, end_pos);
			}
			
			// Skip whitespace until one newline but only if newline is found
			{
				let mut skip_space = end_pos;
				
				while skip_space != content.len() && content[skip_space].is_ascii_whitespace()
				{
					skip_space += 1;
					
					if content[skip_space - 1] == b'\n'
					{
						end_pos = skip_space;
						break;
					}
				}
			}
			
			copy_end = end_pos;
			
			let empty_map = std::collections::BTreeMap::new();
			let mut matches = name_matches(&import_name, patterns, names_passed, &empty_map);
			
			if is_static
			{
				if let Some(pos) = import_name.rfind('.')
				{
					let import_nonstatic_name = &import_name[.. pos];
					matches = matches || name_matches(import_nonstatic_name, patterns, names, &empty_map);
				}
			}
			
			if matches
			{
				copy_end = next_position;
				
				
				if let Some(pos) = import_name.rfind('.')
				{
					let simple_import_name = &import_name[pos + 1 ..];
					
					// Add only non-star and non-static imports
					if ! import_name.ends_with('*') && ! is_static
					{
						removed_classes.insert(simple_import_name.to_string(), import_name.to_owned());
					}
				}
			}
			
			next_position = end_pos;
		}
		
		new_content.extend_from_slice(&content[position .. copy_end]);
		position = next_position;
	}
	
	return result;
}

pub fn remove_annotations(content: &[u8], patterns: &[regex::Regex],
	names: &std::collections::BTreeSet<String>,
	imported_names: &std::collections::BTreeMap<String, String>)
-> std::vec::Vec<u8>
{
	let mut position: usize = 0;
	let mut result = std::vec::Vec::<u8>::new();
	result.reserve(content.len());
	
	while position < content.len()
	{
		let (annotation, annotation_name) = next_annotation(content, position);
		let mut next_position = content.len();
		let mut copy_end = content.len();
		
		if annotation.as_ptr() as usize != content.as_ptr() as usize + content.len()
		{
			copy_end = annotation.as_ptr() as usize + annotation.len() - content.as_ptr() as usize;
			next_position = copy_end;
			
			if annotation_name != "interface" && name_matches(&annotation_name, patterns, names, imported_names)
			{
				copy_end = annotation.as_ptr() as usize - content.as_ptr() as usize;
				
				let mut skip_space = next_position;
				
				while skip_space != content.len() && content[skip_space].is_ascii_whitespace()
				{
					skip_space += 1;
				}
				
				if skip_space != content.len()
				{
					next_position = skip_space;
				}
			}
		}
		
		result.extend_from_slice(&content[position .. copy_end]);
		position = next_position;
	}
	
	return result;
}

fn handle_content(content: &[u8], parameters: &Parameters) -> std::vec::Vec<u8>
{
	let (mut new_content, removed_classes) = remove_imports(content, parameters.patterns.as_slice(), &parameters.names);
	
	if parameters.also_remove_annotations
	{
		let content = new_content;
		new_content = remove_annotations(&content, &parameters.patterns, &parameters.names, &removed_classes);
		
		if new_content.len() < content.len()
		{
			STRICT_MODE.get().map(|s| s.any_annotation_removed.store(true, std::sync::atomic::Ordering::Release));
		}
	}
	
	return new_content;
}

pub fn handle_file(path: &std::ffi::OsStr, origin: &std::ffi::OsStr, parameters: &Parameters)
-> std::io::Result<std::vec::Vec<u8>>
{
	let mut original_content = std::vec::Vec::<u8>::new();
	
	if path.is_empty()
	{
		std::io::stdin().read_to_end(&mut original_content)?;
	}
	else
	{
		std::fs::File::open(&path)?.read_to_end(&mut original_content)?;
	}
	
	let content = handle_content(&original_content, parameters);
	
	if ! parameters.in_place
	{
		let mut ostream = std::io::stdout().lock();
		
		if ! path.is_empty()
		{
			writeln!(ostream, "{}:", std::path::Path::new(path).display())?;
		}
		
		ostream.write(content.as_slice())?;
		ostream.write(b"\n")?;
	}
	else if content.len() < original_content.len()
	{
		std::fs::OpenOptions::new().write(true).truncate(true).open(&path)?.write(content.as_slice())?;
		let mut ostream = std::io::stdout().lock();
		writeln!(ostream, "Removing symbols from file {}", std::path::Path::new(path).display())?;
		
		STRICT_MODE.get().map(|s| *s.files_truncated.lock().unwrap().get_mut(std::path::Path::new(origin)).unwrap() = true);
	}
	
	return Ok(content);
}

type ParameterDict<'t> = std::collections::BTreeMap<&'t str, std::vec::Vec<&'t std::ffi::OsStr>>;

pub fn parse_arguments<'t>(args: &[&'t std::ffi::OsStr], no_argument_flags: &std::collections::BTreeSet<&str>)
-> ParameterDict<'t>
{
	let mut result = ParameterDict::<'t>::new();
	
	if args.is_empty()
	{
		return result;
	}
	
	let mut last_flag = "";
	result.insert(last_flag, std::vec::Vec::new());
	
	for &arg in args
	{
		if arg == "-h" || arg == "--help"
		{
			result.clear();
			return result;
		}
		else if let Some(arg) = arg.to_str()
		{
			let mut chars = arg.chars();
			
			if chars.next() == Some('-')
			{
				if let Some(next) = chars.next()
				{
					if next == '-' || next.is_ascii_alphabetic()
					{
						let insert_result = result.get(arg);
						
						if insert_result.is_none()
						{
							result.insert(arg, std::vec::Vec::new());
						}
						
						last_flag = arg;
						
						if no_argument_flags.contains(arg)
						{
							last_flag = "";
						}
						
						continue;
					}
				}
			}
		}
		
		result.get_mut(last_flag).unwrap().push(arg);
		last_flag = "";
	}
	
	return result;
}

pub fn interpret_args(parameters: &ParameterDict) -> Parameters
{
	let mut result = Parameters
	{
		patterns: std::vec::Vec::<regex::Regex>::new(),
		names: std::collections::BTreeSet::new(),
		also_remove_annotations: false,
		in_place: false,
		strict_mode: false,
	};
	
	if let Some(patterns) = parameters.get("-p")
	{
		result.patterns.reserve(patterns.capacity());
		
		for pattern in patterns
		{
			result.patterns.push(regex::Regex::new(pattern.to_str().unwrap()).unwrap());
		}
	}
	
	if let Some(names) = parameters.get("-n")
	{
		for &name in names
		{
			result.names.insert(name.to_str().unwrap().to_string());
		}
	}
	
	if parameters.get("-a").is_some()
	{
		result.also_remove_annotations = true;
	}
	
	if parameters.get("-i").is_some() || parameters.get("--in-place").is_some()
	{
		result.in_place = true;
		
		if parameters.get("-s").is_some() || parameters.get("--strict").is_some()
		{
			result.strict_mode = true;
		}
	}
	
	return result;
}
