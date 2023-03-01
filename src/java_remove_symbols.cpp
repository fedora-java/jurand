#include <array>
#include <mutex>
#include <thread>

#include <java_symbols.hpp>

using namespace java_symbols;

int main(int argc, const char** argv)
{
	auto args = std::span<const char*>(argv + 1, argc - 1);
	
	auto parameter_dict = parse_arguments(args, {"-a", "--dry-run"});
	
	auto default_root = std::array<std::string, 1> {"."};
	
	auto fileroots = std::span<std::string>(default_root);
	
	const auto parameters = interpret_args(parameter_dict);
	
	if (auto it = parameter_dict.find(""); it != parameter_dict.end())
	{
		fileroots = it->second;
	}
	
	auto files = std::vector<std::filesystem::path>();
	files.reserve(256);
	
	for (const auto& fileroot : fileroots)
	{
		auto to_handle = std::filesystem::directory_entry(std::filesystem::path(fileroot));
		
		if (to_handle.is_regular_file())
		{
			files.emplace_back(std::move(to_handle));
		}
		else
		{
			for (auto& dir_entry : std::filesystem::recursive_directory_iterator(fileroot))
			{
				to_handle = std::move(dir_entry);
				
				if (to_handle.is_regular_file() and to_handle.path().native().ends_with(".java"))
				{
					files.emplace_back(std::move(to_handle));
				}
			}
		}
	}
	
	auto errors_mtx = std::mutex();
	auto errors = std::vector<std::string>();
	
	auto jthreads = std::vector<std::jthread>(std::max(1u, std::thread::hardware_concurrency()));
	
	{
		auto begin = std::size_t(0);
		
		for (std::size_t i = 0; i != std::size(jthreads); ++i)
		{
			auto remainder = std::size(files) % std::size(jthreads);
			auto length = std::size(files) / std::size(jthreads) + bool(i < remainder);
			
			jthreads[i] = std::jthread([&, begin, length]() noexcept -> void
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
	
	jthreads.clear();
	
	if (not errors.empty())
	{
		auto message = std::string("Error: exceptions occured during the process:\n");
		
		for (const auto& error : errors)
		{
			message += "* ";
			message += error;
			message += "\n";
		}
		
		throw std::runtime_error(message);
	}
}
