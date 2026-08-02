#pragma once
#include <string_view>
#include <cstdint>

struct SourceView {
	const char* rawptr = nullptr;
	size_t size = 0;
	// Interfaccia di lettura uniforme
	[[nodiscard]] constexpr const char* get() const noexcept { return rawptr; }
	[[nodiscard]] constexpr size_t length() const noexcept { return size; }
	[[nodiscard]] constexpr bool empty() const noexcept { return size == 0; }
	[[nodiscard]] constexpr char operator[](size_t idx) const noexcept { return rawptr[idx]; }
	[[nodiscard]] constexpr std::string_view get_str() const noexcept { return std::string_view(rawptr, size); }
};
struct FileInfo {
	uint32_t offset_begin;
	uint32_t name_size; //Careful: name_size EXCLUDES the null termination \0 (always required)
	uint32_t file_size;
};