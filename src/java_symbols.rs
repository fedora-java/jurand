use std::io::Write;
use std::io::Read;

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
				else if &content[position .. position + 2] == b"*/"
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
	
	return (&content[position .. position + symbol_length], position + symbol_length);
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

pub fn next_annotation(content: &[u8], mut position: usize) -> (&[u8], std::vec::Vec<u8>)
{
	let mut result = std::vec::Vec::<u8>::new();
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
			
			result.extend_from_slice(symbol);
			expecting_dot = ! expecting_dot;
			end_pos = new_end_pos;
			(symbol, new_end_pos) = next_symbol(content, new_end_pos);
		}
	}
	
	return (&content[position .. end_pos], result);
}

pub struct Parameters
{
	pub patterns : std::vec::Vec<regex::bytes::Regex>,
	pub names : std::collections::BTreeSet<std::vec::Vec<u8>>,
	pub also_remove_annotations : bool,
	pub in_place : bool,
	pub strict_mode : bool,
}

fn name_matches(name: &[u8], patterns: &[regex::bytes::Regex],
	names: &std::collections::BTreeSet<std::vec::Vec<u8>>,
	imported_names: &std::collections::BTreeMap<std::vec::Vec<u8>, std::vec::Vec<u8>>) -> bool
{
	let mut simple_name = name;
	
	if let Some(pos) = name.iter().rposition(|&b| b == b'.')
	{
		simple_name = &name[pos + 1 ..];
	}
	
	
	if names.contains(simple_name)
	{
		// strict_mode.map(|s| s.name_matched(name));
		
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
			// strict_mode.map(|s| s.pattern_matched(pattern));
			
			return true;
		}
	}
	
	return false;
}

pub fn remove_imports(content: &[u8], patterns: &[regex::bytes::Regex],
	names: &std::collections::BTreeSet<std::vec::Vec<u8>>)
-> (std::vec::Vec<u8>, std::collections::BTreeMap<std::vec::Vec<u8>, std::vec::Vec<u8>>)
{
	let mut result = (std::vec::Vec::<u8>::new(), std::collections::BTreeMap::<std::vec::Vec<u8>, std::vec::Vec<u8>>::new());
	let (ref mut new_content, ref mut removed_classes) = result;
	new_content.reserve(content.len());
	let mut position : usize = 0;
	
	while position < content.len()
	{
		let mut next_position = find_token(content, "import", position, true, 0);
		let mut copy_end = content.len();
		
		if next_position < content.len()
		{
			let mut import_name = std::vec::Vec::<u8>::new();
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
				
				import_name.extend_from_slice(symbol);
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
			let mut matches = name_matches(import_name.as_slice(), patterns, names_passed, &empty_map);
			
			if is_static
			{
				if let Some(pos) = import_name.iter().rposition(|&b| b == b'.')
				{
					let import_nonstatic_name = &import_name.as_slice()[.. pos];
					matches = matches || name_matches(import_nonstatic_name, patterns, names, &empty_map);
				}
			}
			
			if matches
			{
				copy_end = next_position;
				
				let mut simple_import_name = std::vec::Vec::new();
				
				if let Some(pos) = import_name.iter().rposition(|&b| b == b'.')
				{
					simple_import_name.extend_from_slice(&import_name.as_slice()[pos + 1 ..]);
				}
				
				// Add only non-star and non-static imports
				if ! import_name.ends_with(b"*") && ! is_static
				{
					removed_classes.insert(simple_import_name, import_name.to_owned());
				}
			}
			
			next_position = end_pos;
		}
		
		new_content.extend_from_slice(&content[position .. copy_end]);
		position = next_position;
	}
	
	return result;
}

pub fn remove_annotations(content: &[u8], patterns: &[regex::bytes::Regex],
	names: &std::collections::BTreeSet<std::vec::Vec<u8>>,
	imported_names: &std::collections::BTreeMap<std::vec::Vec<u8>, std::vec::Vec<u8>>)
-> std::vec::Vec<u8>
{
	let mut position : usize = 0;
	let mut result = std::vec::Vec::<u8>::new();
	result.reserve(content.len());
	
	while position < content.len()
	{
		let (ref annotation, annotation_name) = next_annotation(content, position);
		let mut next_position = content.len();
		let mut copy_end = content.len();
		
		if annotation.as_ptr() as usize != content.as_ptr() as usize + content.len()
		{
			copy_end = annotation.as_ptr() as usize + annotation.len() - content.as_ptr() as usize;
			next_position = copy_end;
			
			if annotation_name.as_slice() != b"interface" && name_matches(&annotation_name, patterns, names, imported_names)
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
			// strict_mode.map(|s| s.any_annotation_removed());
		}
	}
	
	return new_content;
}

pub fn handle_file(path: std::ffi::OsString, parameters: &Parameters) -> std::io::Result<std::vec::Vec<u8>>
{
	let mut original_content = std::vec::Vec::<u8>::new();
	
	if path.as_os_str().is_empty()
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
		
		if ! path.as_os_str().is_empty()
		{
			ostream.write(os_str_bytes::RawOsStr::new(path.as_os_str()).as_raw_bytes())?;
			ostream.write(b":\n")?;
		}
		
		ostream.write(content.as_slice())?;
		ostream.write(b"\n")?;
	}
	else if content.len() < original_content.len()
	{
		// strict_mode.map(|b| b.file_truncated(path));
		
		std::fs::OpenOptions::new().write(true).truncate(true).open(&path)?.write(content.as_slice())?;
		let mut ostream = std::io::stdout().lock();
		ostream.write(b"Removing symbols from file ")?;
		ostream.write(os_str_bytes::RawOsStr::new(path.as_os_str()).as_raw_bytes())?;
		ostream.write(b"\n")?;
	}
	
	return Ok(content);
}

type ParameterDict<'args> = std::collections::BTreeMap<&'args std::ffi::OsStr, std::vec::Vec<&'args std::ffi::OsStr>>;

pub fn parse_arguments<'args>(args: &[&'args std::ffi::OsStr], no_argument_flags: &std::collections::BTreeSet<&std::ffi::OsStr>) -> ParameterDict<'args>
{
	let mut result = std::collections::BTreeMap::<&std::ffi::OsStr, std::vec::Vec<&std::ffi::OsStr>>::new();
	
	if args.is_empty()
	{
		return result;
	}
	
	result.insert(std::ffi::OsStr::new(""), std::vec::Vec::new());
	let mut last_flag = std::ffi::OsStr::new("");
	
	for &arg in args
	{
		if arg == "-h" || arg == "--help"
		{
			result.clear();
			return result;
		}
		else if arg.len() >= 2 && os_str_bytes::RawOsStr::new(arg).starts_with('-')
			&& (os_str_bytes::RawOsStr::new(arg).as_raw_bytes()[1].is_ascii_alphanumeric() || os_str_bytes::RawOsStr::new(arg).as_raw_bytes()[1] == b'-')
		{
			let insert_result = result.get(arg);
			
			if insert_result.is_none()
			{
				result.insert(arg, std::vec::Vec::new());
			}
			
			last_flag = arg;
			
			if no_argument_flags.contains(arg)
			{
				last_flag = std::ffi::OsStr::new("");
			}
		}
		else
		{
			result.get_mut(last_flag).unwrap().push(arg);
			last_flag = std::ffi::OsStr::new("");
		}
	}
	
	return result;
}

pub fn interpret_args(parameters: &ParameterDict) -> Parameters
{
	let mut result = Parameters
	{
		patterns : std::vec::Vec::<regex::bytes::Regex>::new(),
		names : std::collections::BTreeSet::new(),
		also_remove_annotations : false,
		in_place : false,
		strict_mode : false,
	};
	
	if let Some(patterns) = parameters.get(std::ffi::OsStr::new("-p"))
	{
		result.patterns.reserve(patterns.capacity());
		
		for pattern in patterns
		{
			result.patterns.push(regex::bytes::Regex::new(pattern.to_str().unwrap()).unwrap());
		}
	}
	
	if let Some(names) = parameters.get(std::ffi::OsStr::new("-n"))
	{
		for &name in names
		{
			result.names.insert(os_str_bytes::RawOsStr::new(name).as_raw_bytes().to_vec());
		}
	}
	
	if parameters.get(std::ffi::OsStr::new("-a")).is_some()
	{
		result.also_remove_annotations = true;
	}
	
	if parameters.get(std::ffi::OsStr::new("-i")).is_some() || parameters.get(std::ffi::OsStr::new("--in-place")).is_some()
	{
		result.in_place = true;
		
		if parameters.get(std::ffi::OsStr::new("-s")).is_some() || parameters.get(std::ffi::OsStr::new("--strict")).is_some()
		{
			result.strict_mode = true;
		}
	}
	
	return result;
}
