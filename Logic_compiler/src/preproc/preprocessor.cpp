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

PCursor Preprocessor::findPrev(const char c) const
{ //returns a cursor (so a sourceContent relative value)
	PCursor cur = cursor();
	while (cur > 0) {
		--cur; // Decrement first to move backwards from current position
		if (file_mgr.getChar(sourceContent(), cur) == c) {
			return cur;
		}
	}
	return PCursorMax; //Char not found
}

Snippet Preprocessor::readToNext(std::initializer_list<char> cs)
{
	//EXCLUDING CHARACTER c (same in other similar helpers)
	//TODO: optimize for constexpr and static
	const PCursor nextC = findNext(cs);
	if (nextC == PCursorMax)
		return SNIPPET_SENT;
	const PCursor oldCursor = cursor();
	cursor() = nextC;
	return from(oldCursor);
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

void Preprocessor::ol_back()
{}

int Preprocessor::process_directive(MacroType type)
{
	//called with cursor on the char after "#macro [space]"
	//ex. "#macro something" cursor is on s
	switch (type)
	{
	case MacroType::None:
		return handle_none();
	case MacroType::Unknown:
		return handle_unknown();
	case MacroType::Define:
		return handle_define();
	case MacroType::Define_M:
		return handle_define_m();
	case MacroType::Include:
		return handle_include();
	case MacroType::If:
		return handle_if();
	case MacroType::Ifndef:
		return handle_ifndef();
	case MacroType::Else:
		return handle_else();
	case MacroType::Elif:
		return handle_elif();
	case MacroType::Endif:
		return handle_endif();
	case MacroType::Skip:
		return handle_skip();
	case MacroType::Import:
		return handle_import();
	case MacroType::Param:
		return handle_param();
	default:
		break;
	}
	return -1;
}

int Preprocessor::evalCondition(Snippet cond)
{
	if (currActive == MacroType::Ifndef) {
		//logic to get the name and check;
	}
	return false;
}

int Preprocessor::skip_if_branch()
{
	//process() by default always works on active branches only.
	//This gets called when a branch is false
	//skips to the nearest #else or #elif
	//treats #else as #if true (restart execution, nothing happened)
	//#elif [condition] as #if condition

	bool stringed2 = false; //"#" or '#' is NOT macro stuff
	bool stringed1 = false;
	auto stringed = [&]() { return stringed1 || stringed2; };
	uint16_t nestCount = 1;

	//called when getting out of the branch (cursor on the first space or \n after the macro name)
	auto end_branching = [&]() {
		const PCursor endLine = findNext('\n');
		if (endLine == PCursorMax)
		{
			//Lets process() handle end of file
			cursor() = sourceContent().size;
			return 422;
		}
		cursor() = endLine;
		return 0;
	};

	while (nestCount > 0) {
		//by default the cursor is always at the end of the macro line here (for checking optimizations)
		//Or wherever in non macro lines
		if  (sourceContent().size < cursor() + 5) [[unlikely]]  {
			//A few chars of padding just to keep it clean (an #endif is at least 6 extra chars)
			// 
			//error, file done but branching incomplete
			return -422;
		}
		++cursor();

		const char currInitial = currChar();

		if (currInitial == '\"') {
			stringed2 = !stringed2;
			continue;
		}
		if (currInitial == '\'') {
			stringed1 = !stringed1;
			continue;
		}

		if (stringed()) continue;

		if (currInitial == '#') {
			const Snippet nameSnippet = readToNext({ ' ', '\n' });
			if (nameSnippet == SNIPPET_SENT) {
				//Error! bad preprocessor code
				//here we break everything and clear compilation
				return -400;
			}
			const std::string_view macroName = file_mgr[nameSnippet];
			switch (macro_hash_match(macroName))
			{
			case MacroType::If:
			case MacroType::Ifndef:
				++nestCount;
				break;
			case MacroType::Define: {
				const PCursor endLine = findNext('\n');
				if (endLine == PCursorMax) [[unlikely]] {
					//error, last line
					return -422;
				}
				cursor() = endLine;
				break;
			}
			case MacroType::Define_M:
				//todo: implementation
				//It's hard to implement and basically always useless so let's estabilish
				//no #macros allowed in definitions (inert)
				break;
			case MacroType::Skip: {
				//command order is usually top down, but skip is the dumbest most commanding macro ever
				//ALWAYS removes the line immediately below it, no exceptions no nesting
				const PCursor endLine1 = findNext('\n');
				if (endLine1 == PCursorMax || endLine1 + 1 >= sourceContent().size) [[unlikely]] {
					//error, last line/skip on last line
					return -422;
				}
				cursor() = endLine1 + 1;
				const PCursor endLine2 = findNext('\n');
				if (endLine2 == PCursorMax) [[unlikely]] {
					//error, last line
					return -422;
				}
				cursor() = endLine2;
				break;
			}
			case MacroType::Elif:
				if (nestCount == 1) {
					return 105102; //branching ended with new branching
				}

				break;
			case MacroType::Else: {
				if (nestCount == 1) {
					const int retVal = end_branching();
					return retVal ? retVal : 1; //branching ended with inversion (1 or 422 (eof))
				}
				break;
			}
			case MacroType::Endif:
				if (nestCount == 1) {
					return end_branching();
					 //branching ended (0 or 422 (eof))
				}
				--nestCount;
				break;
			default:
				break;
			}
		}
	}
	return -1;
}

int Preprocessor::handle_define()
{
	//strict parsing rules, #define [name] [content] \n anything else yields error
	//only exception is __/__, (next line) though definition calls parse to single line only
	// ... __/__ \n [tabbing ...] == ... [space] ...
	//for now minimal version, TODO(Much later): Implement syntax checking (better here than at calls)
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
		const Snippet defContent = from(beginning);
		const Defined def{
			.content = defContent,
			.t = MacroType::Define,
			.flags = 3 //second bit up if it has newline tabbing
		};
		if (isDefined(nameStr)) {
			//Add warning: macro redefinition
			//default behavior is override from that point forwards (like undef + define)
			storedMacros[nameStr] = def;
			return 1; //Fall through, but warning
		}
		else {
			storedMacros.insert({ nameStr, def });
		}
	}
	
	return 0;
}

int Preprocessor::handle_define_m()
{
	ol();
	const Snippet defName = readToNext(' ');
	const std::string_view nameStr = file_mgr[defName];
	const PCursor endDef = findNext("#enddef");
	if (endDef == PCursorMax)
		return -3; //critical error, -3 for end not found
	const Snippet defContent = to(endDef); //remember that to() excludes the character (no #)

	const Defined def{
			.content = defContent,
			.t = MacroType::Define_M,
			.flags = 1 //remember 0 is sentinel
	};
	if (isDefined(nameStr)) {
		//Add warning: macro redefinition
		//default behavior is override from that point forwards (like undef + define)
		storedMacros[nameStr] = def;
		return 1; //Fall through, but warning
	}
	else {
		storedMacros.insert({ nameStr, def });
	}
	return 0;
}

int Preprocessor::handle_include()
{
	ol(); // Skip whitespace following `#include`

	if (end()) {
		return -1; // Error: Unexpected end of file after #include
	}

	const char openChar = currChar();
	char closeChar = '\0';

	if (openChar == '"') {
		closeChar = '"';
	}
	else if (openChar == '<') {
		closeChar = '>';
	}
	else {
		return -1; // Error: Expected '"' or '<' after #include
	}

	// Advance cursor past the opening quote/bracket
	++cursor();

	// Read the path snippet up to the matching closing character
	const Snippet pathSnippet = readToNext(closeChar);
	if (pathSnippet.offset == SNIPPET_SENT.offset) {
		return -1; // Error: Unterminated include path
	}

	//for now only local file inclusion (easy to change though)

	// Consume the closing quote/bracket
	++cursor();

	const std::string_view includePath = file_mgr[pathSnippet];

	// Load the target file's content
	Snippet includedContent = loadView(includePath.data());
	if (includedContent.offset == SNIPPET_SENT.offset || includedContent.size == PLengthMax) {
		return -2; // Error: Include file not found or failed to load
	}

	// Push new frame onto the stack to process the included file recursively
	inputStack.push_back(SourceFrame{
		.content = includedContent,
		.cursor = 0
		});

	return 0; // Successfully queued included file on the input stack
}

int Preprocessor::handle_if()
{
	const PCursor beginning = cursor();
	cursor() = findNext('\n');
	ol_back();
	const Snippet rawCond = from(beginning);
	int res = evalCondition(rawCond);
	if (res < 0) {
		//Fatal error (syntax and such), abort
		return res; 
	}
	uint8_t result = static_cast<uint8_t>(res & 3u);
	branchFlags.push_back(result);
	return 0;
}

int Preprocessor::handle_ifndef()
{
	return 0;
}

int Preprocessor::handle_else()
{
	return 0;
}

int Preprocessor::handle_elif()
{
	return 0;
}

int Preprocessor::handle_endif()
{
	return 0;
}

int Preprocessor::handle_skip()
{
	return 0;
}

int Preprocessor::handle_import()
{
	return 0;
}

int Preprocessor::handle_param()
{
	return 0;
}

void Preprocessor::process()
{
	while (cursor() < sourceContent().size) {
		ol();

		
	}
	
}

void Preprocessor::processLayer()
{
	int ret = 0;
	while (cursor() < sourceContent().size)
	{
		
		ol();
		if (currChar() == '#') {
			const PCursor mEnd = findNext(' ');
			if (mEnd == PCursorMax) {
				//Error! bad preprocessor code
				//here we break everything and clear compilation
				return;
			}
			const uint8_t l = static_cast<uint8_t>(mEnd - cursor()); //8 bit length
			MacroType thisMacro = macro_hash_match(file_mgr.read(sourceContent(), cursor(), l));
			cursor() += l + 1;
			ret = process_directive(thisMacro);
		}
		//copy stuff into output
	}
}
