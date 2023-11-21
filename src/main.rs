use jurand::*;

fn main() -> std::process::ExitCode
{
	let args: std::vec::Vec<std::ffi::OsString> = std::env::args_os().skip(1).collect();
	let args_view: std::vec::Vec<&std::ffi::OsStr> = args.iter().map(|arg| arg.as_os_str()).collect();
	let parameter_dict = parse_arguments(args_view.as_slice(), &std::collections::BTreeSet::from([
		"-a", "-i", "--in-place", "-s", "--strict",
	]));
	
	if parameter_dict.is_empty()
	{
		print!("\
Usage: jurand [optional flags] <matcher>... [file path]...
    Matcher:
        -n <name>
                simple (not fully-qualified) class name
        -p <pattern>
                regex pattern to match names used in code
        
    Optional flags:
        -a      also remove annotations used in code
        -i, --in-place
                replace the contents of files
        -s, --strict
                (wih -i only) fail if any of the specified options was redundant
                and no changes associated with the option were made

        -h, --help
                print help message
");
		return std::process::ExitCode::SUCCESS;
	}
	
	let parameters = interpret_args(&parameter_dict);
	
	if parameters.names.is_empty() && parameters.patterns.is_empty()
	{
		println!("{}", "jurand: no matcher specified");
		return std::process::ExitCode::from(1);
	}
	
	let fileroots = parameter_dict.get("").unwrap();
	
	if fileroots.is_empty()
	{
		if parameters.in_place
		{
			println!("{}", "jurand: no input files");
			return std::process::ExitCode::from(1);
		}
		
		handle_file(&std::ffi::OsStr::new(""), &std::ffi::OsStr::new(""), &parameters).unwrap();
		
		return std::process::ExitCode::SUCCESS;
	}
	
	let mut files = std::vec::Vec::<(std::path::PathBuf, std::path::PathBuf)>::new();
	
	for &fileroot in fileroots
	{
		let path = std::path::Path::new(fileroot);
		
		if ! path.exists()
		{
			println!("{}", "jurand: file does not exist: ");
			return std::process::ExitCode::from(2);
		}
		
		if path.is_file() && ! path.is_symlink()
		{
			files.push((path.to_path_buf(), std::path::Path::new(fileroot).to_path_buf()));
		}
		else if path.is_dir()
		{
			for entry in walkdir::WalkDir::new(path)
			{
				if let Ok(entry) = entry
				{
					let entry = entry.path();
					
					if entry.is_file() && ! entry.is_symlink()
						&& entry.extension().unwrap_or_default() == "java"
					{
						files.push((entry.to_path_buf(), std::path::Path::new(fileroot).to_path_buf()));
					}
				}
			}
		}
	}
	
	if files.is_empty()
	{
		println!("{}", "jurand: no valid input files");
		return std::process::ExitCode::from(1);
	}
	
	if parameters.strict_mode
	{
		STRICT_MODE.set(StrictMode
		{
			any_annotation_removed: std::sync::atomic::AtomicBool::new(false),
			patterns_matched: std::sync::Mutex::new(parameters.patterns.iter().map(|p| (p.to_string(), false)).collect()),
			names_matched: std::sync::Mutex::new(parameters.names.iter().map(|n| (n.to_owned(), false)).collect()),
			files_truncated: std::sync::Mutex::new(files.iter().map(|(_, origin)| (origin.clone(), false)).collect()),
		}).unwrap();
	}
	
	let files_count = std::sync::atomic::AtomicUsize::new(0);
	
	std::thread::scope(|scope|
	{
		let mut threads = Vec::new();
		threads.reserve(std::thread::available_parallelism().unwrap_or(1.try_into().unwrap()).get());
		let capacity = threads.capacity();
		
		let files = &files;
		let parameters = &parameters;
		let files_count = &files_count;
		
		for _ in 0 .. capacity
		{
			threads.push(scope.spawn(move ||
			{
				loop
				{
					let index = files_count.fetch_add(1, std::sync::atomic::Ordering::AcqRel);
					
					if index >= files.len()
					{
						break;
					}
					
					let (file, origin) = &files[index];
					
					handle_file(file.as_os_str(), origin.as_os_str(), &parameters).unwrap();
				}
			}));
		}
	});
	
	let mut exit_code = std::process::ExitCode::SUCCESS;
	
	if let Some(strict) = STRICT_MODE.get()
	{
		for (path, _) in strict.files_truncated.lock().unwrap().iter().filter(|(_, &b)| ! b)
		{
			println!("jurand: strict mode: no changes were made in {}", path.display());
			exit_code = std::process::ExitCode::from(3);
		}
		
		for (name, _) in strict.names_matched.lock().unwrap().iter().filter(|(_, &b)| ! b)
		{
			println!("jurand: strict mode: simple name {} did not match anything", name);
			exit_code = std::process::ExitCode::from(3);
		}
		
		for (pattern, _) in strict.patterns_matched.lock().unwrap().iter().filter(|(_, &b)| ! b)
		{
			println!("jurand: strict mode: pattern {} did not match anything", pattern);
			exit_code = std::process::ExitCode::from(3);
		}
		
		if parameters.also_remove_annotations && ! strict.any_annotation_removed.load(std::sync::atomic::Ordering::Acquire)
		{
			println!("jurand: strict mode: -a was specified but no annotation was removed");
			exit_code = std::process::ExitCode::from(3);
		}
	}
	
	return exit_code;
}
