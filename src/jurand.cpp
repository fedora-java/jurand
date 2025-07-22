#include <array>
#include <thread>
#include <utility>
#include <atomic>

#include "java_symbols.hpp"

using namespace java_symbols;

int main(int argc, const char** argv)
{
	auto args = std::span<const char*>(argv + 1, argc - 1);
	
	auto parameter_dict = parse_arguments(args, {"-a", "-i", "--in-place", "-s", "--strict"});
	
	if (parameter_dict.empty())
	{
		std::cout << 1 + (R"""(
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
)""");
		return 0;
	}
	
	const auto parameters = interpret_args(parameter_dict);
	
	if (parameters.names_.empty() and parameters.patterns_.empty())
	{
		std::cout << "jurand: no matcher specified" << "\n";
		return 1;
	}
	
	const auto fileroots = std::span<std::string_view>(parameter_dict.find("")->second);
	
	if (fileroots.empty())
	{
		if (parameters.in_place_)
		{
			std::cout << "jurand: no input files" << "\n";
			return 1;
		}
		
		handle_file({}, parameters);
		
		return 0;
	}
	
	auto files_count = std::atomic<std::ptrdiff_t>(0);
	auto files = std::vector<Path_origin_entry>();
	files.reserve(32);
	
	for (auto fileroot : fileroots)
	{
		auto to_handle = std::filesystem::path(fileroot);
		
		if (not std::filesystem::exists(to_handle))
		{
			std::cout << "jurand: file does not exist: " << to_handle.native() << "\n";
			return 2;
		}
		
		if (std::filesystem::is_regular_file(to_handle) and not std::filesystem::is_symlink(to_handle))
		{
			files.emplace_back(Path_origin_entry(std::move(to_handle), fileroot));
		}
		else if (std::filesystem::is_directory(to_handle))
		{
			for (const auto& dir_entry : std::filesystem::recursive_directory_iterator(to_handle))
			{
				to_handle = dir_entry;
				
				if (std::filesystem::is_regular_file(to_handle)
					and not std::filesystem::is_symlink(to_handle)
					and to_handle.native().ends_with(".java"))
				{
					files.emplace_back(Path_origin_entry(std::move(to_handle), fileroot));
				}
			}
		}
	}
	
	if (files.empty())
	{
		std::cout << "jurand: no valid input files" << "\n";
		return 1;
	}
	
	if (parameters.strict_mode_)
	{
		strict_mode.emplace();
		
		for (auto fileroot : fileroots)
		{
			strict_mode->files_truncated_.lock().get().try_emplace(fileroot);
		}
		
		for (const auto& pattern : parameters.patterns_)
		{
			strict_mode->patterns_matched_.lock().get().try_emplace(pattern);
		}
		
		for (const auto& name : parameters.names_)
		{
			strict_mode->names_matched_.lock().get().try_emplace(name);
		}
	}
	
	auto threads = std::vector<std::thread>(std::min(std::size(files), std::max<std::size_t>(1, std::thread::hardware_concurrency())));
	
	auto errors = Mutex<std::vector<std::string>>();
	
	for (auto& thread : threads)
	{
		thread = std::thread([&]() noexcept -> void
		{
			while (true)
			{
				auto index = files_count.fetch_add(1, std::memory_order_acq_rel);
				
				if (index >= std::ssize(files))
				{
					break;
				}
				
				try
				{
					handle_file(std::move(files[index]), parameters);
				}
				catch (std::exception& ex)
				{
					errors.lock().get().emplace_back(ex.what());
				}
			}
		});
	}
	
	for (auto& thread : threads)
	{
		thread.join();
	}
	
	threads.clear();
	
	int exit_code = 0;
	
	if (auto& errors_unlocked = errors.lock().get(); not errors_unlocked.empty())
	{
		exit_code = 2;
		std::cout << "jurand: exceptions occured during the process:\n";
		
		for (const auto& error : errors_unlocked)
		{
			std::cout << "* " << error << "\n";
		}
	}
	else if (strict_mode)
	{
		for (const auto& file_entry : strict_mode->files_truncated_.lock().get())
		{
			if (not file_entry.second)
			{
				std::cout << "jurand: strict mode: no changes were made in " << file_entry.first << "\n";
				exit_code = 3;
			}
		}
		
		for (const auto& name_entry : strict_mode->names_matched_.lock().get())
		{
			if (not name_entry.second)
			{
				std::cout << "jurand: strict mode: simple name " << name_entry.first << " did not match anything" << "\n";
				exit_code = 3;
			}
		}
		
		for (const auto& pattern_entry : strict_mode->patterns_matched_.lock().get())
		{
			if (not pattern_entry.second)
			{
				std::cout << "jurand: strict mode: pattern " << pattern_entry.first << " did not match anything" << "\n";
				exit_code = 3;
			}
		}
		
		if (parameters.also_remove_annotations_ and not strict_mode->any_annotation_removed_.load(std::memory_order_acquire))
		{
			std::cout << "jurand: strict mode: -a was specified but no annotation was removed" << "\n";
			exit_code = 3;
		}
	}
	
	return exit_code;
}
