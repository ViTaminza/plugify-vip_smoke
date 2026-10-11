#pragma once
// The value parsing of the original vip_smoke's OnEntitySpawned, extracted from plugin.cpp so it
// can be unit-tested without an engine (plugins/vip_smoke/tests/vip_smoke_tests.cpp).
//
// The original's rule: fewer than three tokens means the value is a random colour when it is
// exactly the literal "random", and otherwise an invalid format (logged in red); three or more
// tokens are the red, green and blue numbers.
//
// SplitStringBySpace keeps the original's byte-for-byte behaviour: a '"' toggles "inside quotes"
// and is dropped, whitespace outside quotes separates tokens, empty tokens are dropped. The three
// numbers are parsed with std::stoi semantics (a leading number wins, "12abc" is 12); a token that
// does not start with a number - which made std::stoi throw std::invalid_argument, terminating the
// server from this engine callback - or one that does not fit an int answers Invalid instead, and
// the caller logs the original's "Invalid smoke color format" line.

#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>

namespace vip_smoke {

struct Color {
	int red = 0;
	int green = 0;
	int blue = 0;
};

enum class ColorParse {
	Color,   // three numbers (the first three tokens)
	Random,  // the literal "random"
	Invalid, // fewer than three tokens and not "random", or a token that is not a number
};

inline std::vector<std::string> SplitStringBySpace(const std::string& input) {
	std::vector<std::string> tokens;
	std::string current;
	bool inQuotes = false;

	for (const char ch : input) {
		if (ch == '"') {
			inQuotes = !inQuotes;
			continue;
		}
		if (std::isspace(static_cast<unsigned char>(ch)) && !inQuotes) {
			if (!current.empty()) {
				tokens.push_back(current);
				current.clear();
			}
		} else {
			current += ch;
		}
	}
	if (!current.empty()) {
		tokens.push_back(current);
	}
	return tokens;
}

// std::stoi without the throw: fails when no number starts the token or when the value does not fit
// an int. Note that std::stoi("12abc") is 12, so a trailing garbage suffix is not an error.
inline bool ParseInteger(const std::string& text, int& value) {
	char* end = nullptr;
	errno = 0;
	const long parsed = std::strtol(text.c_str(), &end, 10);
	if (end == text.c_str() || errno == ERANGE) {
		return false;
	}
	value = static_cast<int>(parsed);
	return true;
}

inline ColorParse ParseSmokeColor(const std::string& value, Color& color) {
	const std::vector<std::string> args = SplitStringBySpace(value);
	if (args.size() < 3 && value == "random") {
		return ColorParse::Random;
	}
	if (args.size() < 3) {
		return ColorParse::Invalid;
	}
	Color parsed;
	if (!ParseInteger(args[0], parsed.red) || !ParseInteger(args[1], parsed.green) ||
		!ParseInteger(args[2], parsed.blue)) {
		return ColorParse::Invalid;
	}
	color = parsed;
	return ColorParse::Color;
}

} // namespace vip_smoke
