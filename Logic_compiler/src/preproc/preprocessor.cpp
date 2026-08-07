#include "preprocessor.h"
#include <string_view>

Snippet Preprocessor::loadView(const char* pathrel)
{
	return file_mgr.loadFile(pathrel).content();
}

PCursor Preprocessor::findInLine(std::string_view sequence) const
{
	const PCursor endLine = findNext('\n'); //remember findNext returns a cursor
	if (endLine == PCursorMax) return PCursorMax;
	const Snippet forwards = to(endLine);
	const std::string_view forwards_str = file_mgr[forwards];
	const size_t nextC = forwards_str.find(sequence); //64 bit length
	if (nextC == std::string_view::npos || nextC + cursor() > PCursorMax)
		return PCursorMax;
	return static_cast<PCursor>(cursor() + nextC);
}

Preprocessor::Defined Preprocessor::findDefined(std::string_view macroName) const
{
	if (auto it = storedMacros.find(macroName); it != storedMacros.end()) {
		return it->second;
	}

	// Sentinel (flags == 0)
	return Defined{.flags = 0};
}

int Preprocessor::addMacro(std::string_view macroName, Defined macroC)
{
	return 0;
}

void Preprocessor::forgetMacro(std::string_view macroName)
{}

PCursor Preprocessor::findNext(const char c) const
{ //returns a cursor (so a sourceContent relative value)
	PCursor cur = cursor();
	while (cur < sourceContent().size) {
		if (file_mgr.getChar(sourceContent(), cur) == c) {
			return cur;
		}
		++cur; //Default check for current char too, do ++cursor and then findNext() if want to avoid current char check

	}
	return PCursorMax; //Char not found
}

Snippet Preprocessor::peekToNext(const char c) const
{
	const PCursor nextC = findNext(c);
	if (nextC == PCursorMax)
		return SNIPPET_SENT;
	return to(nextC);
}

bool Preprocessor::skipToNext(const char c)
{
	const PCursor foundCur = findNext(c);
	if (foundCur != PCursorMax){
		cursor() = foundCur;
		return true;
	}
	//Next not found logic
	return false;
}

Snippet Preprocessor::readToNext(const char c)
{
	//EXCLUDING CHARACTER c (same in other similar helpers)
	const PCursor nextC = findNext(c);
	if (nextC == PCursorMax)
		return SNIPPET_SENT;
	const PCursor oldCursor = cursor();
	cursor() = nextC;
	return from(oldCursor);
}

PCursor Preprocessor::findNext(std::string_view sequence) const
{
	const Snippet forwards = to(sourceContent().size); //size can be thought of as a cursor
	const std::string_view forwards_str = file_mgr[forwards];
	const size_t nextC = forwards_str.find(sequence);
	if (nextC == std::string_view::npos || static_cast<uint64_t>(nextC) + cursor() > PCursorMax)
		return PCursorMax;
	return static_cast<PCursor>(cursor() + nextC);
}

PCursor Preprocessor::findNext(std::initializer_list<char> cs) const
{
	if (cs.size() == 0) return PCursorMax;
	if (cs.size() == 1) return findNext(*cs.begin());

	// look up table
	bool lut[256] = { false };
	for (char c : cs) {
		lut[static_cast<unsigned char>(c)] = true;
	}

	PCursor cur = cursor();
	const PCursor endCur = sourceContent().size;

	while (cur < endCur) {
		unsigned char ch = static_cast<unsigned char>(file_mgr.getChar(sourceContent(), cur));
		if (lut[ch]) {
			return cur;
		}
		++cur;
	}

	return PCursorMax;
}

PCursor Preprocessor::findNext(std::initializer_list<std::string_view> sequences) const
{
	const Snippet forwards = to(static_cast<PCursor>(0) + sourceContent().size);
	const std::string_view forwards_str = file_mgr[forwards];

	size_t min_pos = std::string_view::npos; //64 bit PLength

	for (const std::string_view seq : sequences)
	{
		const size_t pos = forwards_str.find(seq);
		if (pos < min_pos)
		{
			min_pos = pos;
			// Early exit optimization if a match occurs at the very start of the stream
			if (min_pos == 0)
				break;
		}
	}

	if (min_pos == std::string_view::npos || static_cast<uint64_t>(min_pos) + cursor() >= PCursorMax)
		return PCursorMax;

	return cursor() + static_cast<PLength>(min_pos);
}

PCursor Preprocessor::findNext(const CharBitLUT& charLUT) const
{
	PCursor cur = cursor();
	const PCursor end_cur = sourceContent().size;

	while (cur < end_cur) {
		char ch = file_mgr.getChar(sourceContent(), cur);
		if (is_in(charLUT, ch)) {
			return cur;
		}
		++cur;
	}

	return PCursorMax;
}

PCursor Preprocessor::findNextInv(const CharBitLUT& falseLUT) const
{
	PCursor cur = cursor();
	const PCursor end_cur = sourceContent().size;

	while (cur < end_cur) {
		char ch = file_mgr.getChar(sourceContent(), cur);
		if (!is_in(falseLUT, ch)) {
			return cur;
		}
		++cur;
	}

	return PCursorMax;
}

void Preprocessor::ol()
{

}

void Preprocessor::process_directive(MacroType type)
{
	//called with cursor on the #
	switch (type)
	{
	case MacroType::None:
		break;
	case MacroType::Unknown:
		break;
	case MacroType::Define:
		break;
	case MacroType::Define_M:
		break;
	case MacroType::Include:
		break;
	case MacroType::If:
		break;
	case MacroType::Ifndef:
		break;
	case MacroType::Else:
		break;
	case MacroType::Skip:
		break;
	case MacroType::Import:
		break;
	case MacroType::Param:
		break;
	default:
		break;
	}
}

bool Preprocessor::evalCondition()
{
	if (currActive == MacroType::Ifndef) {
		//logic to get the name and check;
	}
	return false;
}

int Preprocessor::handle_define()
{
	//strict parsing rules, #define [name] [content] \n anything else yields error
	//only exception is __/__, (next line) though definition calls parse to single line only
	// ... __/__ \n [tabbing ...] == ... [space] ...
	ol();
	const Snippet defName = readToNext(' ');
	const std::string_view nameStr = file_mgr[defName];
	ol();
	PCursor lineEnd = findInLine("__/__");
	//TODO: Implement class wide sentinels and push/pop error data to know what PCursorMax represents
	if (lineEnd == PCursorMax) { //for now PCursorMax is default (no "__/__" present in line)
		const Snippet defContent = readToNext('\n');
		const Defined def{
			.content = defContent,
			.t = MacroType::Define,
			.flags = 1
		};
		if (isDefined(nameStr)) {
			//Add warning: macro redefinition
			//default behavior is override from that point forwards
			storedMacros[nameStr] = def;
			return 1; //Fall through, but warning
		}
		else {
			storedMacros.insert({ nameStr, def });
		}
	}
	else {
		const PCursor beginning = cursor();
		while (lineEnd != PCursorMax) {
			cursor() = findNext('\n') + 1;
			lineEnd = findInLine("__/__");
		}
		cursor() = findNext('\n');
	}
	
	return 0;
}

int Preprocessor::handle_define_m()
{
	ol();
	const Snippet defName = readToNext(' ');
	const std::string_view nameStr = file_mgr[defName];
	return 0;
}

void Preprocessor::process()
{
	while (cursor() < sourceContent().size) {
		ol();

		if (currChar() == '#') {
			const PCursor mEnd = findNext(' ');
			if (mEnd == PCursorMax) {
				//Error! bad preprocessor code
				//here we break everything and clear compilation
				return;
			}
			const uint8_t l = static_cast<uint8_t>(mEnd - cursor() + 1u); //8 bit length
			MacroType thisMacro = macro_hash_match(file_mgr.read(sourceContent(), cursor(), l));
			cursor() += l;
			process_directive(thisMacro);
		}
		//copy stuff into output
	}
	
}

void Preprocessor::processLayer()
{
	while (cursor() < sourceContent().size)
	{
		ol();

	}
}
