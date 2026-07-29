#pragma once
#include <string_view>
#include <cstdint>
struct ParserContext {
	uint64_t line;
	uint32_t state;
};

class Parser {
	uint64_t cursor = 0;
};