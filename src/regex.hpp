#pragma once

#include <memory>
#include <string_view>

namespace java_symbols
{
struct Regex
{
	virtual ~Regex() = default;
	
	virtual bool search(std::string_view text) = 0;
};

std::unique_ptr<Regex> compile_extended_regex(std::string_view pattern);
} // namespace java_symbols
