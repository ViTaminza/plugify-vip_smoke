// vip_smoke unit tests — the engine-independent part of the port: the "smoke_color" value
// parsing of the original vip_smoke (SplitStringBySpace + the three std::stoi calls and the
// "random" / "Invalid smoke color format" branches).
//
// Run by ../../test.sh (inside the Steam Runtime image, cwd = build/tests).

#include "smoke_color.hpp"

#include <cstdio>
#include <string>

namespace {

int g_checks = 0;
int g_failures = 0;

void Check(bool condition, const std::string& what) {
	++g_checks;
	if (!condition) {
		++g_failures;
		std::printf("FAIL: %s\n", what.c_str());
	}
}

using vip_smoke::Color;
using vip_smoke::ColorParse;

void TestExactNumbers() {
	Color color;
	Check(vip_smoke::ParseSmokeColor("0 255 255", color) == ColorParse::Color, "three numbers parse");
	Check(color.red == 0 && color.green == 255 && color.blue == 255, "0 255 255 keeps its values");

	Check(vip_smoke::ParseSmokeColor("128 0 128", color) == ColorParse::Color, "the LITE purple parses");
	Check(color.red == 128 && color.green == 0 && color.blue == 128, "128 0 128 keeps its values");

	// The shipped groups.ini values, all of the "R G B" shape.
	const char* values[] = {"255 215 0", "242 9 9", "26 148 27", "8 75 242", "170 80 255",
							"165 42 42", "0 255 0", "0 0 255", "168 7 153"};
	for (const char* value : values) {
		Check(vip_smoke::ParseSmokeColor(value, color) == ColorParse::Color,
			  std::string("production value parses: ") + value);
	}
}

void TestRandom() {
	Color color;
	Check(vip_smoke::ParseSmokeColor("random", color) == ColorParse::Random, "'random' is the random branch");
	// The original compares the whole string, so only the exact literal matches.
	Check(vip_smoke::ParseSmokeColor("Random", color) == ColorParse::Invalid, "'Random' is not the literal");
	Check(vip_smoke::ParseSmokeColor("random ", color) == ColorParse::Invalid, "a trailing space is not the literal");
	Check(vip_smoke::ParseSmokeColor(" random", color) == ColorParse::Invalid, "a leading space is not the literal");
}

void TestInvalid() {
	Color color;
	Check(vip_smoke::ParseSmokeColor("", color) == ColorParse::Invalid, "an empty value is invalid");
	Check(vip_smoke::ParseSmokeColor("   ", color) == ColorParse::Invalid, "whitespace only is invalid");
	Check(vip_smoke::ParseSmokeColor("1 2", color) == ColorParse::Invalid, "two numbers are invalid");
	Check(vip_smoke::ParseSmokeColor("1", color) == ColorParse::Invalid, "one number is invalid");
	Check(vip_smoke::ParseSmokeColor("1 2 abc", color) == ColorParse::Invalid,
		  "a non-numeric token is invalid (std::stoi would have thrown)");
	Check(vip_smoke::ParseSmokeColor("99999999999999999999 0 0", color) == ColorParse::Invalid,
		  "an out-of-range token is invalid (std::stoi would have thrown)");
}

void TestSplitter() {
	// The original's SplitStringBySpace: quotes toggle "inside quotes" and are dropped, so quoted
	// whitespace does not separate. A fully quoted value is therefore ONE token (CCommand::
	// Tokenize would do the same), which is not "random" and not three numbers - Invalid.
	Color color;
	Check(vip_smoke::ParseSmokeColor("\"0 255 255\"", color) == ColorParse::Invalid,
		  "a fully quoted value is one token, not three numbers");

	// Padding and repeated separators around unquoted tokens are fine.
	const std::string padded[] = {" 0 255 255", "0  255   255 ", "0 255 255\t"};
	for (const std::string& value : padded) {
		Check(vip_smoke::ParseSmokeColor(value, color) == ColorParse::Color, "padded value parses: " + value);
		Check(color.red == 0 && color.green == 255 && color.blue == 255,
			  "padded value keeps its numbers: " + value);
	}

	// Quoting a single token still separates the others (the quotes are dropped, the token is kept).
	Check(vip_smoke::ParseSmokeColor("0 \"255\" 255", color) == ColorParse::Color, "a quoted token parses");
	Check(color.red == 0 && color.green == 255 && color.blue == 255, "a quoted token keeps its value");

	// Extra tokens are ignored (only the first three are read).
	Check(vip_smoke::ParseSmokeColor("1 2 3 4", color) == ColorParse::Color, "four tokens parse");
	Check(color.red == 1 && color.green == 2 && color.blue == 3, "only the first three tokens are read");
}

void TestStoiPrefix() {
	// std::stoi("12abc") is 12: a trailing garbage suffix is not an error.
	Color color;
	Check(vip_smoke::ParseSmokeColor("12abc 34 56", color) == ColorParse::Color, "a numeric prefix parses");
	Check(color.red == 12 && color.green == 34 && color.blue == 56, "the numeric prefix keeps its value");
}

} // namespace

int main() {
	TestExactNumbers();
	TestRandom();
	TestInvalid();
	TestSplitter();
	TestStoiPrefix();
	std::printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
