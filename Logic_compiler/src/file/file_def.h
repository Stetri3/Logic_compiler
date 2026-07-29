#pragma once
#include <memory>
#include <string_view>
#include <cstdint>

struct SourceView {
	const char* path = nullptr;
	const char* rawptr = nullptr;
	size_t size = 0;
	// Interfaccia di lettura uniforme
	[[nodiscard]] constexpr const char* get() const noexcept { return rawptr; }
	[[nodiscard]] constexpr size_t length() const noexcept { return size; }
	[[nodiscard]] constexpr bool empty() const noexcept { return size == 0; }
	[[nodiscard]] constexpr char operator[](size_t idx) const noexcept { return rawptr[idx]; }
	[[nodiscard]] constexpr std::string_view get_str() const noexcept { return std::string_view(rawptr, size); }
};

struct SourceFile {
	const char* path = nullptr;
	std::unique_ptr<char[]> data = nullptr;
	size_t size = 0;

	[[nodiscard]] const char* get() const noexcept { return data.get(); }
	[[nodiscard]] size_t length() const noexcept { return size; }
	[[nodiscard]] bool empty() const noexcept { return size == 0; }
	[[nodiscard]] char operator[](size_t idx) const noexcept { return data[idx]; }
	[[nodiscard]] std::string_view get_str() const noexcept { return std::string_view(data.get(), size); }

	[[nodiscard]] SourceView cref() const noexcept {
		return SourceView{ path, data.get(), size };
	}
};