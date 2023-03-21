mod java_symbols;
#[cfg(test)]
mod java_symbols_test;
use crate::java_symbols::*;

fn main() -> std::process::ExitCode
{
	let args : std::vec::Vec<std::ffi::OsString> = std::env::args_os().skip(1).collect();
	let args_view : std::vec::Vec<&std::ffi::OsStr> = args.iter().map(|arg| arg.as_os_str()).collect();
	let parameter_dict = parse_arguments(args_view.as_slice(), &std::collections::BTreeSet::from([
		"-a", "-i", "--in-place", "-s", "--strict",
	].map(std::ffi::OsStr::new)));
	
	if parameter_dict.is_empty()
	{
		println!("{}",
"Usage: jurand [optional flags] <matcher>... [file path]...
    Matcher:
        -n <name>
                simple (not fully-qualified) class name
        -p <pattern>
                regex pattern to match names used in code
        
    Optional flags:
        -a      also remove annotations used in code
        -i, --in-place
                replace the contents of files
        --strict
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
	
	let fileroots = parameter_dict.get(std::ffi::OsStr::new("")).unwrap();
	
	if fileroots.is_empty()
	{
		if parameters.in_place
		{
			println!("{}", "jurand: no input files");
			return std::process::ExitCode::from(1);
		}
		
		handle_file("".into(), &parameters).unwrap();
		
		return std::process::ExitCode::SUCCESS;
	}
	
	let mut files = std::vec::Vec::<std::ffi::OsString>::new();
	
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
			files.push(path.as_os_str().to_owned());
		}
		else if path.is_dir()
		{
			for entry in walkdir::WalkDir::new(path)
			{
				if let Ok(entry) = entry
				{
					let entry = entry.path();
					
					if entry.is_file() && ! entry.is_symlink()
						&& os_str_bytes::RawOsStr::new(entry.as_os_str()).ends_with(".java")
					{
						files.push(entry.as_os_str().to_owned());
					}
				}
			}
		}
	}
	
	let files = std::sync::Arc::new(std::sync::Mutex::new(files));
	let mut threads = std::vec::Vec::<std::thread::JoinHandle<()>>::with_capacity(
		std::thread::available_parallelism().unwrap_or(std::num::NonZeroUsize::new(1).unwrap()).into()
	);
	
	let parameters = std::sync::Arc::new(parameters);
	
	for _ in 0 .. threads.capacity()
	{
		let files = std::sync::Arc::clone(&files);
		let parameters = parameters.clone();
		
		threads.push(std::thread::spawn(move ||
		{
			loop
			{
				let mut files = files.lock().unwrap();
				
				if files.is_empty()
				{
					break;
				}
				
				handle_file(files.pop().unwrap(), &parameters).unwrap();
			}
		}));
	}
	
	for thread in threads
	{
		thread.join().unwrap();
	}
	
	if parameters.strict_mode
	{
		// collect
	}
	
	return std::process::ExitCode::SUCCESS;
}
