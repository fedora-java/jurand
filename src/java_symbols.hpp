#pragma once

#include <cctype>
#include <cstddef>

#include <filesystem>
#include <fstream>
#include <regex>
#include <vector>
#include <set>
#include <map>
#include <string_view>
#include <tuple>
#include <mutex>
#include <optional>
#include <type_traits>

#include <iostream>

inline std::ptrdiff_t std_ssize(const auto& range) noexcept
{
	return std::size(range);
}

inline bool std_contains(const auto& range, const auto& value) noexcept
{
	return range.find(value) != range.end();
}

inline bool std_ends_with(std::string_view string, std::string_view suffix)
{
	return string.length() >= suffix.length() and suffix == std::string_view(string.data() + string.length() - suffix.length(), suffix.length());
}

struct std_osyncstream : protected std::lock_guard<std::mutex>
{
	static inline auto cout_mtx = std::mutex();
	static inline auto clog_mtx = std::mutex();
	
	~std_osyncstream()
	{
		stream_.flush();
	}
	
	std_osyncstream(std::ostream& stream, std::mutex& mtx) noexcept
		:
		lock_guard(mtx),
		stream_(stream)
	{
	}
	
	std_osyncstream& operator<<(const auto& value)
	{
		stream_ << value;
		return *this;
	}
	
	std_osyncstream& write(const char* content, std::streamsize length)
	{
		stream_.write(content, length);
		return *this;
	}
	
	std::ostream& stream_;
};

template<typename Type>
struct std_span
{
	std_span() = default;
	
	std_span(const std_span&) noexcept = default;
	std_span& operator=(const std_span&) noexcept = default;
	
	std_span(Type* begin, Type* end) noexcept
		:
		begin_(begin),
		end_(end)
	{
	}
	
	std_span(Type* begin, std::size_t size) noexcept
		:
		begin_(begin),
		end_(begin + size)
	{
	}
	
	std_span(auto&& range) noexcept
	requires(not std::is_same_v<std_span, std::decay_t<decltype(range)>>)
		:
		std_span(std::data(range), std::size(range))
	{
	}
	
	Type* begin() noexcept {return begin_;}
	Type* end() noexcept {return end_;}
	
	const Type* begin() const noexcept {return begin_;}
	const Type* end() const noexcept {return end_;}
	
	bool empty() const noexcept {return begin_ == end_;}
	
	Type* begin_ = nullptr;
	Type* end_ = nullptr;
};

/*!
 * Helper functions for manipulating java symbols
 */
namespace java_symbols
{
//! Allows comparison between string and string_view
struct Transparent_string_cmp : std::less<std::string_view>
{
	using is_transparent = void;
};

using Transparent_string_set = std::set<std::string, Transparent_string_cmp>;
using Transparent_string_map = std::map<std::string, std::string, Transparent_string_cmp>;

using Parameter_dict = std::map<std::string, std::vector<std::string>, Transparent_string_cmp>;

struct Parameters
{
	std::vector<std::regex> patterns_;
	Transparent_string_set names_;
	bool also_remove_annotations_ = false;
	bool in_place_ = false;
};

/*!
 * Iterates over @p content starting at @p position to find the first character
 * which is not part of a Java comment nor a whitespace character.
 * 
 * @return The position of the first non-whitespace non-comment character or the
 * length of the string if none is found.
 */
inline std::ptrdiff_t ignore_whitespace_comments(std::string_view content, std::ptrdiff_t position) noexcept
{
	while (position != std_ssize(content))
	{
		if (std::isspace(static_cast<unsigned char>(content[position])))
		{
			++position;
			continue;
		}
		
		auto result = position;
		
		if (auto subst = content.substr(position, 2); subst == "//")
		{
			position = content.find('\n', position + 2);
			
			if (position == std::ptrdiff_t(content.npos))
			{
				return std_ssize(content);
			}
			
			++position;
		}
		else if (subst == "/*")
		{
			position = content.find("*/", position + 2);
			
			if (position == std::ptrdiff_t(content.npos))
			{
				return std_ssize(content);
			}
			
			position += 2;
		}
		
		if (result == position)
		{
			return result;
		}
	}
	
	return position;
}

inline bool is_identifier_char(char c) noexcept
{
	return c == '_' or (not std::ispunct(static_cast<unsigned char>(c)) and not std::isspace(static_cast<unsigned char>(c)));
}

/*!
 * Iterates over @p content starting at @p position to find the next uncommented
 * symbol. and returns it and an index pointing past it. The symbol is either a
 * sequence of alphanumeric characters or a single non-aphanumeric character or
 * an empty string if the end has been reached.
 */
inline std::string_view next_symbol(std::string_view content, std::ptrdiff_t position = 0) noexcept
{
	auto symbol_length = std::ptrdiff_t(0);
	
	if (position < std_ssize(content))
	{
		position = ignore_whitespace_comments(content, position);
		
		if (position < std_ssize(content))
		{
			symbol_length = 1;
			
			if (is_identifier_char(content[position]))
			{
				while (position + symbol_length != std_ssize(content) and is_identifier_char(content[position + symbol_length]))
				{
					++symbol_length;
				}
			}
		}
	}
	
	return content.substr(position, symbol_length);
}

/*!
 * Iterates over @p content starting at @p position to find a string @p token
 * which is present in the source code neither inside a comment nor inside a
 * string nor inside a character literal.
 * 
 * Special case when @p token == ')', this function counts opening and closing
 * parentheses and returns the first parethesis outside.
 * 
 * @param alphanumeric If true, considers only tokens that are surrounded by
 * whitespace, comments or are at the boundaries of @p content.
 * 
 * @return The starting index of the token or the length of @p content if not
 * found.
 */
inline std::ptrdiff_t find_token(std::string_view content, std::string_view token,
	std::ptrdiff_t position = 0, bool alphanumeric = false, std::ptrdiff_t stack = 0) noexcept
{
	while (position + std_ssize(token) <= std_ssize(content))
	{
		position = ignore_whitespace_comments(content, position);
		
		if (position == std_ssize(content))
		{
			break;
		}
		
		auto substr = content.substr(position, token.length());
		
		if ((token != ")" or stack == 0) and substr == token
			and not (alphanumeric and ((position > 0 and is_identifier_char(content[position - 1]))
				or (position + std_ssize(token) < std_ssize(content) and (is_identifier_char(content[position + token.length()]))))))
		{
			return position;
		}
		else if (content[position] == '\'')
		{
			if (content.substr(position, 4) == "'\\''")
			{
				position += 3;
			}
			else
			{
				do
				{
					++position;
				}
				while (position < std_ssize(content) and content[position] != '\'');
			}
		}
		else if (content[position] == '"')
		{
			++position;
			
			while (position < std_ssize(content) and content[position] != '"')
			{
				if (content.substr(position, 2) == "\\\\")
				{
					position += 2;
				}
				else if (content.substr(position, 2) == "\\\"")
				{
					position += 2;
				}
				else
				{
					++position;
				}
			}
		}
		else if (stack != 0 and content[position] == ')')
		{
			--stack;
		}
		else if (content[position] == '(')
		{
			++stack;
		}
		
		++position;
	}
	
	return std_ssize(content);
}

/*!
 * Iterates over @p content starting at @p position to find the next Java
 * annotation.
 * 
 * @return A pair consisting of the whole extent of the annotation as present in
 * the @p content and the name of the annotation with all whitespace and comments
 * stripped. If no annotation is found, returns a view pointing past the
 * @p content with length 0 and an empty string.
 */
inline std::tuple<std::string_view, std::string> next_annotation(std::string_view content, std::ptrdiff_t position = 0)
{
	auto result = std::string();
	auto end_pos = std_ssize(content);
	position = find_token(content, "@", position);
	bool expecting_dot = false;
	
	if (position < std_ssize(content))
	{
		auto symbol = std::string_view();
		symbol = next_symbol(content, position + 1);
		end_pos = symbol.end() - content.begin();
		auto new_end_pos = end_pos;
		
		while (not symbol.empty())
		{
			if (expecting_dot and symbol != ".")
			{
				if (symbol == "(")
				{
					end_pos = find_token(content, ")", new_end_pos);
					
					if (end_pos == std_ssize(content))
					{
						result = std::string();
						position = end_pos;
						break;
					}
					
					++end_pos;
				}
				
				break;
			}
			
			result += symbol;
			expecting_dot = not expecting_dot;
			end_pos = new_end_pos;
			symbol = next_symbol(content, new_end_pos);
			new_end_pos = symbol.end() - content.begin();
		}
	}
	
	return std::tuple(content.substr(position, end_pos - position), std::move(result));
}

/*!
 * @param name Class name found in code, may be fully-qualified or simple.
 * @param patterns A range of patterns.
 * @param names A table of names, these are matched exactly by the simple class
 * names.
 * @param imported_names Names that have their import declarations removed. In
 * this case, we match the full name and this will only remove names that are
 * used in their simple form in code (therefore they refer to the removed
 * import declaration. If a name is used in its fully-qualified form in the
 * code, it will be catched by the other matchers.
 * 
 * @return The simple class name.
 */
inline bool name_matches(std::string_view name, std_span<const std::regex> patterns,
	const Transparent_string_set& names, const Transparent_string_map& imported_names) noexcept
{
	auto simple_name = name;
	
	if (auto pos = name.rfind('.'); pos != name.npos)
	{
		simple_name = name.substr(pos + 1);
	}
	
	if (std_contains(names, simple_name))
	{
		return true;
	}
	
	if (auto it = imported_names.find(simple_name); it != imported_names.end())
	{
		if (name == simple_name or it->second == name)
		{
			return true;
		}
	}
	
	for (const auto& pattern : patterns)
	{
		if (std::regex_search(name.begin(), name.end(), pattern))
		{
			return true;
		}
	}
	
	return false;
}

/*!
 * Iterates over @p content to remove all import statements provided
 * as @p patterns and @p names. Patterns match the string representation
 * following the `import [static]` string. @p names matches only the simple
 * class names.
 * 
 * @return The resulting string with import statements removed and a map of
 * removed simple class names to the fully-qualified name as present in the
 * import statement.
 */
inline std::tuple<std::string, Transparent_string_map> remove_imports(
	std::string_view content, std_span<const std::regex> patterns, const Transparent_string_set& names)
{
	auto result = std::tuple<std::string, Transparent_string_map>();
	auto& [new_content, removed_classes] = result;
	new_content.reserve(content.size());
	auto position = std::ptrdiff_t(0);
	
	while (position < std_ssize(content))
	{
		auto next_position = find_token(content, "import", position, true);
		auto copy_end = std_ssize(content);
		
		if (next_position < std_ssize(content))
		{
			auto import_name = std::string();
			auto symbol = next_symbol(content, next_position + 6);
			auto end_pos = symbol.end() - content.begin();
			
			const auto empty_set = Transparent_string_set();
			const auto* names_passed = &names;
			
			bool is_static = false;
			
			if (symbol == "static")
			{
				is_static = true;
				names_passed = &empty_set;
				symbol = next_symbol(content, end_pos);
				end_pos = symbol.end() - content.begin();
			}
			
			while (symbol != ";")
			{
				if (symbol.empty())
				{
					new_content.clear();
					new_content.append(content);
					removed_classes.clear();
					return result;
				}
				
				import_name += symbol;
				symbol = next_symbol(content, end_pos);
				end_pos = symbol.end() - content.begin();
			}
			
			// Skip whitespace until one newline but only if newline is found
			{
				auto skip_space = end_pos;
				
				while (skip_space != std_ssize(content) and std::isspace(static_cast<unsigned char>(content[skip_space])))
				{
					++skip_space;
					
					if (content[skip_space - 1] == '\n')
					{
						end_pos = skip_space;
						break;
					}
				}
			}
			
			copy_end = end_pos;
			
			bool matches = name_matches(import_name, patterns, *names_passed, {});
			
			if (is_static)
			{
				if (auto pos = import_name.rfind('.'); pos != import_name.npos)
				{
					auto import_nonstatic_name = std::string_view(import_name.c_str(), pos);
					matches = matches or name_matches(import_nonstatic_name, patterns, names, {});
				}
			}
			
			if (matches)
			{
				copy_end = next_position;
				
				auto simple_import_name = std::string();
				
				if (auto pos = import_name.rfind('.'); pos != import_name.npos)
				{
					simple_import_name = import_name.substr(pos + 1);
				}
				
				// Add only non-star and non-static imports
				if (not std_ends_with(import_name, "*") and not is_static)
				{
					removed_classes.try_emplace(std::move(simple_import_name), std::move(import_name));
				}
			}
			
			next_position = end_pos;
		}
		
		new_content += content.substr(position, copy_end - position);
		position = next_position;
	}
	
	return result;
}

/*!
 * Iterates over @p content to remove all annotations provided as @p patterns
 * and @p names. Patterns match the string representation of the annotations as
 * present in the source code. @p names matches only the simple class names.
 * 
 * @return The resulting string with annotations removed.
 */
inline std::string remove_annotations(std::string_view content, std_span<const std::regex> patterns,
	const Transparent_string_set& names, const Transparent_string_map& imported_names)
{
	auto position = std::ptrdiff_t(0);
	auto result = std::string();
	result.reserve(content.size());
	
	while (position < std_ssize(content))
	{
		auto [annotation, annotation_name] = next_annotation(content, position);
		auto next_position = std_ssize(content);
		auto copy_end = std_ssize(content);
		
		if (annotation.begin() != content.end())
		{
			copy_end = annotation.end() - content.begin();
			next_position = copy_end;
			
			if (annotation_name != "interface" and name_matches(annotation_name, patterns, names, imported_names))
			{
				copy_end = annotation.begin() - content.begin();
				
				auto skip_space = next_position;
				
				while (skip_space != std_ssize(content) and std::isspace(static_cast<unsigned char>(content[skip_space])))
				{
					++skip_space;
				}
				
				if (skip_space != std_ssize(content))
				{
					next_position = skip_space;
				}
			}
		}
		
		result += content.substr(position, copy_end - position);
		position = next_position;
	}
	
	return result;
}

////////////////////////////////////////////////////////////////////////////////

inline std::string handle_content(std::string_view content, const Parameters& parameters)
{
	auto [new_content, removed_classes] = remove_imports(content, parameters.patterns_, parameters.names_);
	
	if (parameters.also_remove_annotations_)
	{
		new_content = remove_annotations(new_content, parameters.patterns_, parameters.names_, removed_classes);
	}
	
	return new_content;
}

inline std::string handle_file(std::filesystem::path path, const Parameters& parameters)
try
{
	auto original_content = std::string();
	
	if (path.empty())
	{
		original_content = std::string(std::istreambuf_iterator<char>(std::cin), {});
	}
	else
	{
		auto ifs = std::ifstream(path);
		
		if (ifs.fail())
		{
			throw std::ios_base::failure("Could not open file for reading");
		}
		
		ifs.exceptions(std::ios_base::badbit | std::ios_base::failbit);
		original_content = std::string(std::istreambuf_iterator<char>(ifs), {});
	}
	
	auto content = handle_content(original_content, parameters);
	
	if (not parameters.in_place_)
	{
		auto osyncstream = std_osyncstream(std::cout, std_osyncstream::cout_mtx);
		
		if (not path.empty())
		{
			osyncstream << path.native() << ":\n";
		}
		
		osyncstream.write(content.c_str(), content.size());
	}
	else if (content.size() < original_content.size())
	{
		auto ofs = std::ofstream(path);
		
		if (ofs.fail())
		{
			throw std::ios_base::failure("Could not open file for writing");
		}
		
		ofs.exceptions(std::ios_base::badbit | std::ios_base::failbit);
		ofs.write(content.c_str(), content.size());
		std_osyncstream(std::clog, std_osyncstream::clog_mtx) << "Removing symbols from file " << path.native() << "\n";
	}
	
	return content;
}
catch (std::exception& ex)
{
	auto message = (path.native().empty() ? "" : path.native() + ": ") + ex.what();
	throw std::runtime_error(message);
}

////////////////////////////////////////////////////////////////////////////////

inline Parameter_dict parse_arguments(std_span<const char*> args, const Transparent_string_set& no_argument_flags)
{
	auto result = Parameter_dict();
	
	if (args.empty())
	{
		return result;
	}
	
	auto unflagged_parameters = result.try_emplace("").first;
	auto last_flag = unflagged_parameters;
	
	for (std::string_view arg : args)
	{
		if (arg == "-h" or arg == "--help")
		{
			return Parameter_dict();
		}
		else if (arg.size() >= 2 and arg[0] == '-' and (std::isalnum(static_cast<unsigned char>(arg[1])) or (arg[1] == '-')))
		{
			last_flag = result.try_emplace(std::string(arg)).first;
			
			if (std_contains(no_argument_flags, arg))
			{
				last_flag = unflagged_parameters;
			}
		}
		else
		{
			last_flag->second.emplace_back(arg);
			last_flag = unflagged_parameters;
		}
	}
	
	return result;
}

inline Parameters interpret_args(const Parameter_dict& parameters)
{
	auto result = Parameters();
	
	if (auto it = parameters.find("-p"); it != parameters.end())
	{
		result.patterns_.reserve(it->second.size());
		
		for (const auto& pattern : it->second)
		{
			result.patterns_.emplace_back(pattern, std::regex_constants::extended);
		}
	}
	
	if (auto it = parameters.find("-n"); it != parameters.end())
	{
		for (const auto& name : it->second)
		{
			result.names_.insert(std::move(name));
		}
	}
	
	if (std_contains(parameters, "-a"))
	{
		result.also_remove_annotations_ = true;
	}
	
	if (std_contains(parameters, "-i") or std_contains(parameters, "--in-place"))
	{
		result.in_place_ = true;
	}
	
	return result;
}
} // namespace java_symbols
