#pragma once

#include <cctype>
#include <cstddef>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <regex>
#include <vector>
#include <set>
#include <map>
#include <string_view>
#include <tuple>
#include <optional>
#include <mutex>
#include <span>
#include <syncstream>

#include <iostream>

using String_view_set = std::set<std::string_view, std::less<>>;
using String_map = std::map<std::string, std::string, std::less<>>;

using Parameter_dict = std::map<std::string_view, std::vector<std::string_view>, std::less<>>;

struct Named_regex : std::regex
{
	Named_regex() = default;
	
	Named_regex(const char* pattern, auto&&... args)
		:
		Named_regex(std::string_view(pattern), std::forward<decltype(args)>(args)...)
	{
	}
	
	Named_regex(std::string_view pattern, auto&&... args)
		:
		std::regex(pattern.data(), pattern.length(), std::forward<decltype(args)>(args)...),
		name_(pattern)
	{
	}
	
	Named_regex(std::string&& pattern, auto&&... args)
		:
		std::regex(pattern, std::forward<decltype(args)>(args)...),
		name_(std::move(pattern))
	{
	}
	
	operator std::string_view() const noexcept
	{
		return name_;
	}
	
private:
	std::string name_;
};

struct Path_origin_entry : std::filesystem::path
{
	Path_origin_entry() = default;
	
	Path_origin_entry(auto&& path, std::string_view origin)
		:
		std::filesystem::path(std::forward<decltype(path)>(path)),
		origin_(origin)
	{
	}
	
	[[nodiscard]] std::string_view origin() const noexcept
	{
		return origin_;
	}
	
private:
	std::string_view origin_;
};

template<typename Type, typename Mutex_type>
struct Locked : std::reference_wrapper<Type>
{
	Locked(Type& value, Mutex_type& mutex)
		:
		std::reference_wrapper<Type>::reference_wrapper(value),
		lock_(mutex)
	{
	}
	
private:
	std::lock_guard<Mutex_type> lock_;
};

template<typename Type>
struct Mutex
{
	
	[[nodiscard]] Locked<Type, std::mutex> lock()
	{
		return Locked(value_, mutex_);
	}
	
	[[nodiscard]] Locked<const Type, std::mutex> lock() const
	{
		return Locked(value_, mutex_);
	}
	
private:
	std::mutex mutex_;
	Type value_;
};

struct Parameters
{
	std::vector<Named_regex> patterns_;
	String_view_set names_;
	bool also_remove_annotations_ = false;
	bool in_place_ = false;
	bool strict_mode_ = false;
};

struct Strict_mode
{
	std::atomic<bool> any_annotation_removed_ = false;
	Mutex<std::map<std::string_view, bool, std::less<>>> patterns_matched_;
	Mutex<std::map<std::string_view, bool, std::less<>>> names_matched_;
	Mutex<std::map<std::string_view, bool>> files_truncated_;
};

inline static auto strict_mode = std::optional<Strict_mode>();

/*!
 * Helper functions for manipulating java symbols
 */
namespace java_symbols
{
/*!
 * Decodes a Unicode escape sequence \uXXXX to its character representation.
 * Simplified version assuming valid Java source files.
 * 
 * @param content The string content containing potential Unicode escape
 * @param position The starting position to check for Unicode escape
 * @param decoded_char Output parameter for the decoded character
 * @param sequence_length Output parameter for the length of the Unicode sequence
 * 
 * @return True if a valid Unicode escape sequence was found and decoded
 */
inline bool decode_unicode_escape(std::string_view content, std::ptrdiff_t position, char32_t& decoded_char, std::ptrdiff_t& sequence_length) noexcept
{
	sequence_length = 0;
	
	if (position + 5 >= std::ssize(content) || content[position] != '\\' || content[position + 1] != 'u')
	{
		return false;
	}
	
	// Check for valid hex digits - simplified validation
	auto hex_start = position + 2;
	for (std::ptrdiff_t i = 0; i < 4; ++i)
	{
		char c = content[hex_start + i];
		if (!std::isxdigit(static_cast<unsigned char>(c)))
		{
			return false;
		}
	}
	
	// Parse hex digits - simplified logic
	char32_t result = 0;
	for (std::ptrdiff_t i = 0; i < 4; ++i)
	{
		char c = content[hex_start + i];
		result = result * 16;
		if (c >= '0' && c <= '9')
		{
			result += c - '0';
		}
		else if (c >= 'A' && c <= 'F')
		{
			result += c - 'A' + 10;
		}
		else if (c >= 'a' && c <= 'f')
		{
			result += c - 'a' + 10;
		}
	}
	
	decoded_char = result;
	sequence_length = 6; // \uXXXX
	return true;
}

/*!
 * Checks if a character at the given position (potentially Unicode-escaped) matches the target character.
 * Simplified version assuming valid Java source files.
 * 
 * @param content The string content to check
 * @param position The position to check
 * @param target_char The character to match against
 * @param actual_length Output parameter for the actual length of the character representation
 * 
 * @return True if the character at position matches target_char
 */
inline bool char_matches_at_position(std::string_view content, std::ptrdiff_t position, char target_char, std::ptrdiff_t& actual_length) noexcept
{
	actual_length = 1; // Default for non-Unicode chars
	
	if (position >= std::ssize(content))
	{
		return false;
	}
	
	// First check for Unicode escape
	char32_t decoded_char;
	std::ptrdiff_t unicode_length;
	if (decode_unicode_escape(content, position, decoded_char, unicode_length))
	{
		actual_length = unicode_length;
		return static_cast<char>(decoded_char) == target_char;
	}
	
	// Regular character check
	return content[position] == target_char;
}

/*!
 * Helper function to check for Unicode-escaped comment delimiters.
 * Checks for patterns like \u002F\u002F (//) or \u002F\u002A (slash-star)
 */
inline std::ptrdiff_t check_unicode_comment_start(std::string_view content, std::ptrdiff_t position) noexcept
{
	// Check for \u002F\u002F (Unicode-escaped //)
	std::ptrdiff_t char1_length, char2_length;
	if (char_matches_at_position(content, position, '/', char1_length) &&
	    position + char1_length < std::ssize(content) &&
	    char_matches_at_position(content, position + char1_length, '/', char2_length))
	{
		// Single-line comment - find newline
		auto comment_start = position + char1_length + char2_length;
		position = content.find('\n', comment_start);
		
		if (position == std::ptrdiff_t(content.npos))
		{
			return std::ssize(content);
		}
		
		return position + 1;
	}
	// Check for \u002F\u002A (Unicode-escaped /*)
	else if (char_matches_at_position(content, position, '/', char1_length) &&
	         position + char1_length < std::ssize(content) &&
	         char_matches_at_position(content, position + char1_length, '*', char2_length))
	{
		// Multi-line comment - find */
		auto comment_start = position + char1_length + char2_length;
		
		// Look for end of comment (could also be Unicode-escaped)
		while (comment_start < std::ssize(content))
		{
			std::ptrdiff_t end_char1_length, end_char2_length;
			if (char_matches_at_position(content, comment_start, '*', end_char1_length) &&
			    comment_start + end_char1_length < std::ssize(content) &&
			    char_matches_at_position(content, comment_start + end_char1_length, '/', end_char2_length))
			{
				return comment_start + end_char1_length + end_char2_length;
			}
			++comment_start;
		}
		
		return std::ssize(content); // Unterminated comment
	}
	
	return position; // No comment found
}

/*!
 * Iterates over @p content starting at @p position to find the first character
 * which is not part of a Java comment nor a whitespace character.
 * Supports Unicode-escaped comment delimiters.
 * 
 * @return The position of the first non-whitespace non-comment character or the
 * length of the string if none is found.
 */
inline std::ptrdiff_t ignore_whitespace_comments(std::string_view content, std::ptrdiff_t position) noexcept
{
	while (position != std::ssize(content))
	{
		if (std::isspace(static_cast<unsigned char>(content[position])))
		{
			++position;
			continue;
		}
		
		auto result = position;
		
		// Check for regular comment delimiters first
		if (auto subst = content.substr(position, 2); subst == "//")
		{
			position = content.find('\n', position + 2);
			
			if (position == std::ptrdiff_t(content.npos))
			{
				return std::ssize(content);
			}
			
			++position;
		}
		else if (subst == "/*")
		{
			position = content.find("*/", position + 2);
			
			if (position == std::ptrdiff_t(content.npos))
			{
				return std::ssize(content);
			}
			
			position += 2;
		}
		else
		{
			// Check for Unicode-escaped comment delimiters
			auto unicode_comment_pos = check_unicode_comment_start(content, position);
			if (unicode_comment_pos != position)
			{
				position = unicode_comment_pos;
			}
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
 * Checks if a character at the given position (potentially Unicode-escaped) is a valid identifier character.
 * 
 * @param content The string content to check
 * @param position The position to check
 * @param char_length Output parameter for the actual length of the character representation
 * 
 * @return True if the character at position is a valid identifier character
 */
inline bool is_identifier_char_at_position(std::string_view content, std::ptrdiff_t position, std::ptrdiff_t& char_length) noexcept
{
	char_length = 1; // Default for non-Unicode chars
	
	if (position >= std::ssize(content))
	{
		return false;
	}
	
	// Check for Unicode escape first
	char32_t decoded_char;
	std::ptrdiff_t unicode_length;
	if (decode_unicode_escape(content, position, decoded_char, unicode_length))
	{
		char_length = unicode_length;
		// For Unicode escapes, we check if the decoded character would be a valid identifier char
		if (decoded_char <= 127) // ASCII range
		{
			return is_identifier_char(static_cast<char>(decoded_char));
		}
		// For non-ASCII Unicode characters, they are generally valid identifier chars
		// unless they are punctuation or whitespace
		return decoded_char != 0x0040 && decoded_char != 0x002F && decoded_char != 0x002A; // '@', '/', '*'
	}
	
	// Regular character check
	return is_identifier_char(content[position]);
}

/*!
 * Iterates over @p content starting at @p position to find the next uncommented
 * symbol and returns it and an index pointing past it. The symbol is either a
 * sequence of alphanumeric characters or a single non-alphanumeric character or
 * an empty string if the end has been reached. Supports Unicode escape sequences.
 */
inline std::tuple<std::string_view, std::ptrdiff_t> next_symbol(std::string_view content, std::ptrdiff_t position = 0) noexcept
{
	auto symbol_length = std::ptrdiff_t(0);
	
	if (position < std::ssize(content))
	{
		position = ignore_whitespace_comments(content, position);
		
		if (position < std::ssize(content))
		{
			symbol_length = 1;
			
			if (is_identifier_char(content[position]))
			{
				while (position + symbol_length != std::ssize(content) and is_identifier_char(content[position + symbol_length]))
				{
					++symbol_length;
				}
			}
		}
	}
	
	return std::tuple(content.substr(position, symbol_length), position + symbol_length);
}

/*!
 * Iterates over @p content starting at @p position to find a string @p token
 * which is present in the source code neither inside a comment nor inside a
 * string nor inside a character literal. Supports Unicode escape sequences.
 * 
 * Special case when @p token == ')', this function counts opening and closing
 * parentheses and returns the first parenthesis outside.
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
	while (position < std::ssize(content))
	{
		position = ignore_whitespace_comments(content, position);
		
		if (position == std::ssize(content))
		{
			break;
		}
		
		// Check for single character tokens (like '@', '/', '*') that might be Unicode-escaped
		if (token.length() == 1)
		{
			std::ptrdiff_t char_length;
			if (char_matches_at_position(content, position, token[0], char_length))
			{
				bool is_valid_match = true;
				
				// Apply alphanumeric constraints if needed
				if (alphanumeric)
				{
					if (position > 0 && is_identifier_char(content[position - 1]))
					{
						is_valid_match = false;
					}
					else if (position + char_length < std::ssize(content) && is_identifier_char(content[position + char_length]))
					{
						is_valid_match = false;
					}
				}
				
				// Special handling for parentheses stack counting
				if (token == ")" && stack == 0 && is_valid_match)
				{
					return position;
				}
				else if (token != ")" && is_valid_match)
				{
					return position;
				}
			}
		}
		// Multi-character tokens - check regular substring match
		else if (position + std::ssize(token) <= std::ssize(content))
		{
			auto substr = content.substr(position, token.length());
			
			if ((token != ")" or stack == 0) and substr == token
				and not (alphanumeric and ((position > 0 and is_identifier_char(content[position - 1]))
					or (position + std::ssize(token) < std::ssize(content) and (is_identifier_char(content[position + token.length()]))))))
			{
				return position;
			}
		}
		
		// Handle string and character literals, and parentheses counting
		if (content[position] == '\'')
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
				while (position < std::ssize(content) and content[position] != '\'');
			}
		}
		else if (content[position] == '"')
		{
			++position;
			
			while (position < std::ssize(content) and content[position] != '"')
			{
				if (content.substr(position, 2) == "\\\\"
					or content.substr(position, 2) == "\\\"")
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
		else
		{
			// Check if we're at a Unicode escape and skip it appropriately
			char32_t decoded_char;
			std::ptrdiff_t unicode_length;
			if (decode_unicode_escape(content, position, decoded_char, unicode_length))
			{
				position += unicode_length - 1; // -1 because we'll increment at the end
			}
		}
		
		++position;
	}
	
	return std::ssize(content);
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
	auto end_pos = std::ssize(content);
	position = find_token(content, "@", position);
	bool expecting_dot = false;
	
	if (position < std::ssize(content))
	{
		// Calculate the actual length of the @ symbol (could be Unicode-escaped)
		std::ptrdiff_t at_symbol_length;
		char_matches_at_position(content, position, '@', at_symbol_length);
		
		auto symbol = std::string_view();
		std::tie(symbol, end_pos) = next_symbol(content, position + at_symbol_length);
		auto new_end_pos = end_pos;
		
		while (not symbol.empty())
		{
			// catch `@A ...`
			if (symbol == "." and content.substr(new_end_pos).starts_with(".."))
			{
				break;
			}
			
			if (expecting_dot and symbol != ".")
			{
				if (symbol == "(")
				{
					end_pos = find_token(content, ")", new_end_pos);
					
					if (end_pos == std::ssize(content))
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
			std::tie(symbol, new_end_pos) = next_symbol(content, new_end_pos);
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
 * this case, we match the full name, and this will only remove names that are
 * used in their simple form in code (therefore, they refer to the removed
 * import declaration. If a name is used in its fully qualified form in the
 * code, it will be caught by the other matchers.
 * 
 * @return The simple class name.
 */
inline bool name_matches(std::string_view name, std::span<const Named_regex> patterns,
	const String_view_set& names, const String_map& imported_names) noexcept
{
	auto simple_name = name;
	
	if (auto pos = name.rfind('.'); pos != name.npos)
	{
		simple_name = name.substr(pos + 1);
	}
	
	if (names.contains(simple_name))
	{
		if (strict_mode)
		{
			strict_mode->names_matched_.lock().get().at(simple_name) = true;
		}
		
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
			if (strict_mode)
			{
				strict_mode->patterns_matched_.lock().get().at(pattern) = true;
			}
			
			return true;
		}
	}
	
	return false;
}

/*!
 * Iterates over @p content to remove all import statements provided
 * as @p patterns and @p names. Patterns match the string representation
 * following the `import [static]` string. @p names match only the simple
 * class names.
 * 
 * @return The resulting string with import statements removed and a map of
 * removed simple class names to the fully qualified name as present in the
 * import statement.
 */
inline std::tuple<std::string, String_map> remove_imports(
	std::string_view content, std::span<const Named_regex> patterns, const String_view_set& names)
{
	auto result = std::tuple<std::string, String_map>();
	auto& [new_content, removed_classes] = result;
	new_content.reserve(content.size());
	auto position = std::ptrdiff_t(0);
	
	while (position < std::ssize(content))
	{
		auto next_position = find_token(content, "import", position, true);
		auto copy_end = std::ssize(content);
		
		if (next_position < std::ssize(content))
		{
			auto import_name = std::string();
			auto [symbol, end_pos] = next_symbol(content, next_position + 6);
			
			const auto empty_set = String_view_set();
			const auto* names_passed = &names;
			
			bool is_static = false;
			
			if (symbol == "static")
			{
				is_static = true;
				names_passed = &empty_set;
				std::tie(symbol, end_pos) = next_symbol(content, end_pos);
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
				std::tie(symbol, end_pos) = next_symbol(content, end_pos);
			}
			
			// Skip whitespace until one newline but only if a newline is found
			{
				auto skip_space = end_pos;
				
				while (skip_space != std::ssize(content) and std::isspace(static_cast<unsigned char>(content[skip_space])))
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
				if (not import_name.ends_with("*") and not is_static)
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
 * present in the source code. @p names match only the simple class names.
 * 
 * @return The resulting string with annotations removed.
 */
inline std::string remove_annotations(std::string_view content, std::span<const Named_regex> patterns,
	const String_view_set& names, const String_map& imported_names)
{
	auto position = std::ptrdiff_t(0);
	auto result = std::string();
	result.reserve(content.size());
	
	while (position < std::ssize(content))
	{
		auto [annotation, annotation_name] = next_annotation(content, position);
		auto next_position = std::ssize(content);
		auto copy_end = std::ssize(content);
		
		if (annotation.begin() != content.end())
		{
			copy_end = annotation.end() - content.begin();
			next_position = copy_end;
			
			if (annotation_name != "interface" and name_matches(annotation_name, patterns, names, imported_names))
			{
				copy_end = annotation.begin() - content.begin();
				
				auto skip_space = next_position;
				
				while (skip_space != std::ssize(content) and std::isspace(static_cast<unsigned char>(content[skip_space])))
				{
					++skip_space;
				}
				
				if (skip_space != std::ssize(content))
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
		auto content_size = new_content.size();
		new_content = remove_annotations(new_content, parameters.patterns_, parameters.names_, removed_classes);
		
		if (strict_mode and new_content.size() < content_size)
		{
			strict_mode->any_annotation_removed_.store(true, std::memory_order_release);
		}
	}
	
	return new_content;
}

inline std::string handle_file(const Path_origin_entry& path, const Parameters& parameters)
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
		auto osyncstream = std::osyncstream(std::cout);
		
		if (not path.empty())
		{
			osyncstream << path.native() << ":\n";
		}
		
		osyncstream << content;
	}
	else if (content.size() < original_content.size())
	{
		auto ofs = std::ofstream(path);
		
		if (ofs.fail())
		{
			throw std::ios_base::failure("Could not open file for writing");
		}
		
		ofs.exceptions(std::ios_base::badbit | std::ios_base::failbit);
		ofs << content;
		std::osyncstream(std::clog) << "Removing symbols from file " << path.native() << "\n";
		
		if (strict_mode)
		{
			strict_mode->files_truncated_.lock().get().at(path.origin()) = true;
		}
	}
	
	return content;
}
catch (std::exception& ex)
{
	auto message = (path.native().empty() ? "" : path.native() + ": ") + ex.what();
	throw std::runtime_error(message);
}

////////////////////////////////////////////////////////////////////////////////

inline Parameter_dict parse_arguments(std::span<const char*> args, const String_view_set& no_argument_flags)
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
			last_flag = result.try_emplace(arg).first;
			
			if (no_argument_flags.contains(arg))
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
			result.names_.insert(name);
		}
	}
	
	if (parameters.contains("-a"))
	{
		result.also_remove_annotations_ = true;
	}
	
	if (parameters.contains("-i") or parameters.contains("--in-place"))
	{
		result.in_place_ = true;
		
		if (parameters.contains("-s") or parameters.contains("--strict"))
		{
			result.strict_mode_ = true;
		}
	}
	
	return result;
}
} // namespace java_symbols
