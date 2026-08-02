#pragma once
#include <cstdint>
#include <vector>
#include "file_def.h"
#include "Alloc_optimized.h"
#include <string_view>



class FileManager {

	static constexpr FileInfo _NOT_FOUND = FileInfo{0, 0, 0};
	static constexpr const char* _EMPTY = "\xFF\xFF\xFF\xFF\xFF\xFF\xFF"; //7 0xFF chars + \0
	static constexpr std::string_view _EMPTY_SV = std::string_view(_EMPTY);

	const char* src_path = nullptr;
	OsPagedVector<char> data{};
	std::vector<FileInfo> loaded;
	//file name (path) getter helper
	std::string_view _getName(FileInfo f) const {
		const char* fbegin = data.data() + f.offset_begin;
		return std::string_view(fbegin, f.name_size);
	}
	std::string_view _getName(uint32_t index) const {
		const FileInfo ld = loaded[index];
		return _getName(ld);
	}
	
	FileInfo _findFile(std::string_view name) {
		if (!name.data() || name == _EMPTY_SV) return _NOT_FOUND;
		for (auto i : loaded) {
			if (name == _getName(i))
				return i;
		}
		return _NOT_FOUND;
	}

public:
	FileManager(const char* base_path) : src_path(base_path) { data.reserve(1024 * 1024 * 1024); }
	FileInfo loadFile(const char* path); //loads in data (if not already loaded), returns indexing
	FileInfo loadFromView(SourceView view, const char* filename);
	void pop(); //flushes last


	SourceView operator[](FileInfo info) const {
		const char* fBegin = data.data() + info.offset_begin;
		return SourceView{ fBegin + info.name_size + 1, info.file_size };//+1 for the null termination
	}

	SourceView operator[](uint32_t index) const { return (*this)[loaded[index]]; }
};