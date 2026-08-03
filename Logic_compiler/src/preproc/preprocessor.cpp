#include "preprocessor.h"

Snippet Preprocessor::loadView(const char* pathrel)
{
	return file_mgr.loadFile(pathrel).content();
}

Preprocessor::Defined Preprocessor::findDefined(std::string_view macroName) const
{
	if (auto it = storedMacros.find(macroName); it != storedMacros.end()) {
		return it->second;
	}

	// Sentinel (flags == 0)
	return Defined{.flags = 0};
}

uint32_t Preprocessor::findNext(const char c)
{
	uint32_t cur = cursor;
	while (cur < sourceContent.size) {
		if (file_mgr.getChar(sourceContent, cur) == c) {
			return cur;
		}
		++cur; //Default check for current char too, do ++cursor and then findNext() if want to avoid current char check

	}
	return UINT32_MAX; //Char not found
}

bool Preprocessor::skipToNext(const char to)
{
	const uint32_t foundCur = findNext(to);
	if (foundCur != UINT32_MAX){
		cursor = foundCur;
		return true;
	}
	//Next not found logic
	return false;
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
	case MacroType::DefineM:
		break;
	case MacroType::Enddef:
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

	}
	return false;
}

void Preprocessor::process()
{
	while (cursor < sourceContent.size) {
		ol();

		if (currChar() == '#') {
			const uint32_t mEnd = findNext(' ');
			if (mEnd == UINT32_MAX) {
				//Error! bad preprocessor code
				//here we break everything and clear compilation
				return;
			}
			const uint8_t l = static_cast<uint8_t>(mEnd - cursor + 1u);
			MacroType thisMacro = macro_hash_match(file_mgr.read(sourceContent, cursor, l));
			cursor += l;
			process_directive(thisMacro);
		}
		//copy stuff into output
	}
	
}
