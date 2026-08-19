#pragma once
#include <cstdint>

struct Snippet {
	uint32_t offset;
	uint32_t size;
	bool operator==(const Snippet&) const = default;
};
struct FileInfo {
	uint32_t offset_begin;
	uint32_t name_size; //Careful: name_size EXCLUDES the null termination \0 (always required)
	uint32_t file_size; //Size EXCLUDING (name + \0)

	inline constexpr uint32_t cOffset() const { return offset_begin + name_size + 1; }

	inline constexpr Snippet content() const {
		return Snippet(offset_begin + name_size + 1, file_size);
	}
	inline constexpr Snippet content_full() const {
		return Snippet(offset_begin, file_size + name_size + 1);
	}
	inline constexpr Snippet name() const {
		return Snippet(offset_begin, name_size);
	}
};