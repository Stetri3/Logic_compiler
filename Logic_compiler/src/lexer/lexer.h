#pragma once
#include <cstdint>
#include "Alloc_optimized.h"
#include "token.h"

class Lexer {
	uint32_t cursor;
	OsPagedVector<Token> outBuffer{1024*1024}; //Initial 1kib tokens

};