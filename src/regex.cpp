#include <regex.h>

#include <regex.hpp>

namespace java_symbols
{
struct Extended_regex : Regex, regex_t
{
	~Extended_regex()
	{
		regfree(this);
	}
	
	bool search(std::string_view text) override
	{
		auto result = re_search(this, text.data(), text.length(), 0, text.length(), nullptr);
		
		if (result == -2)
		{
			throw std::runtime_error("re_search internal error");
		}
		else if (result == -1)
		{
			return false;
		}
		else
		{
			return true;
		}
	}
};

std::unique_ptr<Regex> compile_extended_regex(std::string_view pattern)
{
	auto result = std::make_unique<Extended_regex>();
	
	auto compile_result = re_compile_pattern(pattern.begin(), pattern.length(), result.get());
	
	if (compile_result != nullptr)
	{
		throw std::runtime_error(std::string("re_compile_pattern failed: ") + compile_result);
	}
	
	return result;
}
} // namespace java_symbols
