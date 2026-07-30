//Header di definizioni passive per il lexer

#pragma once
#include <cstdint>
#include <string_view>
#include <memory>

enum class TokenType: uint16_t {
	//Keywords:
	//bool
	kFalse, kTrue,
	//Control
	kIf, kElse, kFor, kWhile, kEval,
	//Env-relative
	kConstexpr, kConst, kStatic, kInline, kTemplate, //No virtual for now
	//Env-sectional
	kPublic, kPrivate, kRequires, kNamespace,
	//comp op
	kNew, kDelete, kAuto, kSizeof, kTypeof,
	//builtin
	kType, kByte, kVoid, 
	//Identifiers
	UDO, LInt, LString, //UDO = user defined object (variable)
	//Inline operators (and some access)
	Plus, Minus, Star, Slash, Amp, Bar, Up, Not, Perc, Question, Equals, Colon,
	//Access
	Dot, Arrow, 
	//Contexting
	Left, Right, SqLeft, SqRight, BrLeft, BrRight, TrLeft, TrRight, Semi, Comma,
	//Meta
	Eof, Unknown, External
};
struct Token {
	uint64_t line = 0; //Line of the file in the loaded cache or "line position" in the arena
	TokenType type = TokenType::Unknown;
	uint16_t filename = 0xFFFF; //index of the file in the loaded cache, or 0 for the arena
	uint16_t begin = 0; //offset at the line or offset at the cache line
	uint16_t end = 0;
};