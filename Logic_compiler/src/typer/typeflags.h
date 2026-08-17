#pragma once
//FILE FOR TYPE FLAG PARSING

#include <predef.h>//precomp header

#include <cstdint>
#include <memory>
#include <array>
#include <string_view>
#include "std_types.h"


namespace typer {
	//C style flag array (since dimension is dynamic)
	using FlagArr = cc::CSpan<uint32_t>;



	enum class TypeFlagCode : uint16_t {//type flag keys
		//reserved
		stop, //0 code is reserved for array stoppage
		//time eval options
		sent, //sentinel
		exec_time, //comptime, runtime, both, etc.

		//memory flags
		mem_alloc, //if needs to allocate memory (1)
		mem_space, //which space it can allocate to (2)
		mem_alloc_cap, //  (48) (maybe to update to 56 in the future)
		mem_alloc_size, // (48) //
		mem_space_cap, // (2) + (48) (or just 48 if mem_space is set earlier to just one option)
		//because of reasons like this it might be good to set a flag precedence
		mem_space_size, // (2) + (48)
		mem_page_align, // (2) (for different sized pages)
		mem_page_fill, // (2) not really good to use this if you're not sure
		mem_access_in, // (4) write/overwrite, edit, etc.
		mem_access_out, //(4) same thing but for reading
		mem_copiable, //(1)
		mem_movable, //(1)
		mem_revivable, //(1) if on, it must be deleted manually (doesn't die from exiting scope)

		//member options
		has_methods_c, //(1), if it has constexpr methods
		has_methods_r, //(1)
		has_methods_st, //(1), if it has non constexpr static methods
		has_methods_stc, //(1), if it has (self reflecting) static constexpr methods
		//obviously they don't need to be self reflecting, but if they aren't this deoptimization makes no sense
		
		has_members_c, //in here members refers to non-function members
		has_members_r, 
		has_members_st,
		has_members_stc,


		count,
	};
	
	namespace detail {

		struct TypeFlagInfo {
			std::string_view name{};
			TypeFlagCode code = TypeFlagCode::sent;
			uint8_t bitLen = 0;
		};

		consteval cc::CArray<TypeFlagInfo, static_cast<size_t>(TypeFlagCode::count)> makeTypeFlagInfo() {
			cc::CArray<TypeFlagInfo, static_cast<size_t>(TypeFlagCode::count)> ret{};
			using TF = TypeFlagCode;

#define el(code_val, name_str, bit_len) \
        ret[static_cast<uint16_t>(TF::code_val)] = TypeFlagInfo{ .name = name_str, .code = TF::code_val, .bitLen = bit_len }

			// Reserved / Time eval
			el(stop, "stop", 0);
			el(sent, "sent", 0);
			el(exec_time, "exec_time", 2);

			// Memory flags
			el(mem_alloc, "mem_alloc", 1);
			el(mem_space, "mem_space", 2);
			el(mem_alloc_cap, "mem_alloc_cap", 48);
			el(mem_alloc_size, "mem_alloc_size", 48);
			el(mem_space_cap, "mem_space_cap", 48);
			el(mem_space_size, "mem_space_size", 48);
			el(mem_page_align, "mem_page_align", 2);
			el(mem_page_fill, "mem_page_fill", 2);
			el(mem_access_in, "mem_access_in", 4);
			el(mem_access_out, "mem_access_out", 4);
			el(mem_copiable, "mem_copiable", 1);
			el(mem_movable, "mem_movable", 1);
			el(mem_revivable, "mem_revivable", 1);

			// Member options
			el(has_methods_c, "has_methods_c", 1);
			el(has_methods_r, "has_methods_r", 1);
			el(has_methods_st, "has_methods_st", 1);
			el(has_methods_stc, "has_methods_stc", 1);
			el(has_members_c, "has_members_c", 1);
			el(has_members_r, "has_members_r", 1);
			el(has_members_st, "has_members_st", 1);
			el(has_members_stc, "has_members_stc", 1);

#undef el
			return ret;
		}

		inline constexpr auto TYPE_FLAG_INFO = makeTypeFlagInfo();

		static_assert([] {
			for (size_t i = 0; i < TYPE_FLAG_INFO.size(); ++i) {
				if (TYPE_FLAG_INFO[i].name.empty()) return false;
			}
			return true;
			}(), "A flag definition is missing in makeTypeFlagInfo()!");

	} // namespace detail



	template <TypeFlagCode _code>
	struct TypeFlagDeclImpl {
		static constexpr detail::TypeFlagInfo info = detail::TYPE_FLAG_INFO[(uint16_t)_code];
		static constexpr std::string_view name = info.name;
		static constexpr auto code = _code;
		static constexpr uint16_t bitLen = info.bitLen;
		static constexpr uint16_t wordLen = (bitLen + 16 + 31) / 32; //length in uint32, includes the code

		static constexpr auto getData(cc::CSpan<uint32_t> buffer, uint32_t index) {
			if constexpr (bitLen <= 16) {
				return static_cast<uint16_t>(buffer[index] & UINT16_MAX);
			} else if constexpr (bitLen <= 48) {
				//on good code it should be impossible to have an unfinished array
				
				DBAssert(buffer.size() > 1 && "Error! unexpected end of array");

				return static_cast<uint64_t>((static_cast<uint64_t>(buffer[index] & UINT16_MAX) << 32)
					| buffer[index + 1]);
			}
			else {
				//If bigger than 48 (aka uint64) returns an owning copy null-term snippet INCLUDING the code

				// Ceiling division: ceil((bitLen + 16) / 32)
				DBAssert(wordLen + index <= buffer.size() && "Error! unexpected end of array");
				const cc::CSpan dataSpan = buffer.subspan(index, wordLen);
				return cc::CArray<uint32_t, wordLen>(dataSpan); //makes owning copy of the subspan
			}
		}
	};
	
	template <auto code>
	using TypeFlagDecl = TypeFlagDeclImpl<static_cast<TypeFlagCode>(code)>;

	
}