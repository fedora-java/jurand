#include <array>
#include <mutex>
#include <thread>
#include <future>
#include <utility>
#include <functional>
#include <atomic>

#include <java_symbols.hpp>

using namespace java_symbols;

// TODO replace with C++20 `std::bind_front`
auto bind_handle_file(std::filesystem::path&& path)
{
	return [path = std::move(path)](const Parameters& parameters) -> void
	{
		handle_file(std::move(path), parameters);
	};
}

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
	
	auto tasks_end = std::atomic<std::size_t>(0);
	auto tasks = std::vector<std::packaged_task<void(const Parameters&)>>();
	tasks.reserve(32);
	
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
			tasks.emplace_back(bind_handle_file(std::move(to_handle)));
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
					tasks.emplace_back(bind_handle_file(std::move(to_handle)));
				}
			}
		}
	}
	
	if (tasks.empty())
	{
		std::cout << "jurand: no valid input files" << "\n";
		return 1;
	}
	
	auto threads = std::vector<std::thread>(std::min(std::size(tasks), std::max<std::size_t>(1, std::thread::hardware_concurrency())));
	
	for (auto& thread : threads)
	{
		thread = std::thread([&]() noexcept -> void
		{
			while (true)
			{
				if (auto index = tasks_end.fetch_add(1, std::memory_order_acq_rel); index < tasks.size())
				{
					tasks[index](parameters);
				}
				else
				{
					break;
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
	
	for (auto& task : tasks)
	{
		try
		{
			task.get_future().get();
		}
		catch (std::exception& ex)
		{
			std::cout << "jurand: an exception occured during the process: " << ex.what() << "\n";
			exit_code = 2;
		}
	}
	
	return exit_code;
}
