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
	kIf, kElse, kFor, kWhile, kEval, kBreak, kContinue, kReturn,
	//Env-relative
	kConstexpr, kConst, kStatic, kInline, kTemplate, //No virtual for now
	//Env-sectional
	kPublic, kPrivate, kRequires, kNamespace,
	//comp op
	kNew, kDelete, kAuto, kSizeof, kTypeof,
	//builtin
	kType, kByte, kVoid, kStruct, kEnum,
	//Identifiers
	Ident, LInt, LFloat, LString, LChar,//Ident = user defined object (variable)
	//Inline operators (and some access)
	Plus, Minus, Star, Slash, Amp, Bar, Up, Tilde, Excl, Perc, Question, Equals, Colon, Greater, Lesser,
	//Assignment operators
	PPlus, MMinus, PlusEquals, MinusEquals, StarEquals, SlashEquals, PercEquals, AmpEquals, BarEquals, TildeEquals, UpEquals,
	//Double/boolean operators
	EEquals, NEquals, AAmp, BBar, GrEquals, LeEquals,
	GGreater, LLesser, GGrEquals, LLeEquals,
	CColon, SSlash,
	//Access
	Dot, Dots, Arrow, 
	//Contexting
	Left, Right, SqLeft, SqRight, BrLeft, BrRight, Semi, Comma,
	WideComment,
	//Meta
	Eof, Unknown, External
};
struct Token {
	uint32_t globalOffset = 0; //offset in the manager data
	uint16_t size = 0;
	TokenType type = TokenType::Unknown;
};