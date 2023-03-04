#include <array>
#include <mutex>
#include <thread>

#include <java_symbols.hpp>

using namespace java_symbols;

int main(int argc, const char** argv)
{
	auto args = std_span<const char*>(argv + 1, argc - 1);
	
	auto parameter_dict = parse_arguments(args, {"-a", "-i", "--in-place"});
	
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
	
	auto fileroots = std_span<std::string>(parameter_dict.find("")->second);
	
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
	
	auto files = std::vector<std::filesystem::path>();
	files.reserve(32);
	
	for (std::string& fileroot : fileroots)
	{
		auto to_handle = std::filesystem::path(std::move(fileroot));
		
		if (not std::filesystem::exists(to_handle))
		{
			std::cout << to_handle.native() << ": File does not exist" << "\n";
			return 2;
		}
		
		if (std::filesystem::is_regular_file(to_handle) and not std::filesystem::is_symlink(to_handle))
		{
			files.emplace_back(std::move(to_handle));
		}
		else if (std::filesystem::is_directory(to_handle))
		{
			for (const auto& dir_entry : std::filesystem::recursive_directory_iterator(to_handle))
			{
				to_handle = dir_entry;
				
				if (std::filesystem::is_regular_file(to_handle)
					and not std::filesystem::is_symlink(to_handle)
					and std_ends_with(to_handle.native(), ".java"))
				{
					files.emplace_back(std::move(to_handle));
				}
			}
		}
	}
	
	if (files.empty())
	{
		std::cout << "jurand: no valid input files" << "\n";
		return 1;
	}
	
	auto errors_mtx = std::mutex();
	auto errors = std::vector<std::string>();
	
	// auto threads = std::vector<std::jthread>(std::max(1u, std::thread::hardware_concurrency()));
	auto threads = std::vector<std::thread>(std::min(std::size(files), std::max<std::size_t>(1, std::thread::hardware_concurrency())));
	
	{
		auto begin = std::size_t(0);
		
		for (std::size_t i = 0; i != std::size(threads); ++i)
		{
			auto remainder = std::size(files) % std::size(threads);
			auto length = std::size(files) / std::size(threads) + bool(i < remainder);
			
			threads[i] = std::thread([&, begin, length]() noexcept -> void
			{
				for (auto j = begin; j != begin + length; ++j)
				{
					try
					{
						handle_file(std::move(files[j]), parameters);
					}
					catch (std::exception& ex)
					{
						auto lg = std::lock_guard(errors_mtx);
						errors.emplace_back(ex.what());
					}
				}
			});
			
			begin += length;
		}
	}
	
	for (auto& thread : threads)
	{
		thread.join();
	}
	
	threads.clear();
	
	if (not errors.empty())
	{
		std::cout << "jurand: exceptions occured during the process:" << "\n";
		
		for (const auto& error : errors)
		{
			std::cout << "- " << error << "\n";
		}
		
		return 2;
	}
}
