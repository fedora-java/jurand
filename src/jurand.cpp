#include <array>
#include <mutex>
#include <thread>
#include <future>
#include <utility>
#include <functional>
#include <atomic>

#include <java_symbols.hpp>

using namespace java_symbols;

struct Strict_mode_enabled final : Strict_mode
{
	std::atomic<bool> any_annotation_removed_ = false;
	std::mutex mutex_;
	std::map<std::string_view, bool, Transparent_string_cmp> patterns_matched_;
	std::map<std::string_view, bool, Transparent_string_cmp> names_matched_;
	std::map<std::string_view, bool> files_truncated_;
	
	void any_annotation_removed() override
	{
		any_annotation_removed_.store(true, std::memory_order_release);
	}
	
	void pattern_matched(std::string_view pattern) override
	{
		auto lg = std::lock_guard(mutex_);
		patterns_matched_.at(pattern) = true;
	}
	
	void name_matched(std::string_view simple_name) override
	{
		auto lg = std::lock_guard(mutex_);
		names_matched_.at(simple_name) = true;
	}
	
	void file_truncated(std::string_view path) override
	{
		auto lg = std::lock_guard(mutex_);
		files_truncated_.at(path) = true;
	}
}
static strict_mode_enabled;

int main(int argc, const char** argv)
{
	auto args = std_span<const char*>(argv + 1, argc - 1);
	
	auto parameter_dict = parse_arguments(args, {"-a", "-i", "--in-place", "--strict"});
	
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
        --strict
                (wih -i only) fail if any of the specified options was redundant
                and no changes associated with the option were made

        -h, --help
                print help message
)""");
		return 0;
	}
	
	const auto parameters = interpret_args(parameter_dict);
	
	if (parameters.strict_mode_)
	{
		strict_mode = &strict_mode_enabled;
	}
	
	if (parameters.names_.empty() and parameters.patterns_.empty())
	{
		std::cout << "jurand: no matcher specified" << "\n";
		return 1;
	}
	
	const auto fileroots = std_span<std::string_view>(parameter_dict.find("")->second);
	
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
			tasks.emplace_back(std::bind(&handle_file, Path_origin_entry(std::move(to_handle), fileroot), std::placeholders::_1));
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
					tasks.emplace_back(std::bind(&handle_file, Path_origin_entry(std::move(to_handle), fileroot), std::placeholders::_1));
				}
			}
		}
	}
	
	if (tasks.empty())
	{
		std::cout << "jurand: no valid input files" << "\n";
		return 1;
	}
	
	if (parameters.strict_mode_)
	{
		for (const auto& fileroot : fileroots)
		{
			strict_mode_enabled.files_truncated_.try_emplace(fileroot);
		}
		
		for (const auto& pattern : parameters.patterns_)
		{
			strict_mode_enabled.patterns_matched_.try_emplace(pattern);
		}
		
		for (const auto& name : parameters.names_)
		{
			strict_mode_enabled.names_matched_.try_emplace(name);
		}
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
	
	if (parameters.strict_mode_)
	{
		if (parameters.also_remove_annotations_ and not strict_mode_enabled.any_annotation_removed_.load(std::memory_order_acquire))
		{
			std::cout << "jurand: strict mode: -a was specified but no annotation was removed" << "\n";
			exit_code = 3;
		}
		
		for (const auto& name_entry : strict_mode_enabled.names_matched_)
		{
			if (not name_entry.second)
			{
				std::cout << "jurand: strict mode: simple name " << name_entry.first << " did not match anything" << "\n";
				exit_code = 3;
			}
		}
		
		for (const auto& pattern_entry : strict_mode_enabled.patterns_matched_)
		{
			if (not pattern_entry.second)
			{
				std::cout << "jurand: strict mode: pattern " << pattern_entry.first << " did not match anything" << "\n";
				exit_code = 3;
			}
		}
		
		for (const auto& file_entry : strict_mode_enabled.files_truncated_)
		{
			if (not file_entry.second)
			{
				std::cout << "jurand: strict mode: no changes were made in " << file_entry.first << "\n";
				exit_code = 3;
			}
		}
	}
	
	return exit_code;
}
