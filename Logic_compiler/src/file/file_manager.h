#pragma once
#include <cstdint>
#include <vector>
#include "file_def.h"
#include "Alloc_optimized.h"
#include <string_view>



class FileManager {

	friend class Preprocessor;

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
	
	FileInfo _findFile(std::string_view name) const {
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
	FileInfo loadFromView(std::string_view view, const char* filename);
	void pop(); //flushes last

	bool isLoaded(std::string_view name) const {
		const FileInfo found = _findFile(name);
		return !(found.offset_begin == _NOT_FOUND.offset_begin &&
			found.file_size == _NOT_FOUND.file_size &&
			found.name_size == _NOT_FOUND.name_size);
	}
	
	std::string_view getName(FileInfo info) { return _getName(info); }



	std::string_view read(uint32_t global_offset, uint32_t size) const {//global
		return std::string_view(data.data() + global_offset, size);
	}

	std::string_view read(FileInfo file, uint32_t local_offset, uint32_t size) const {//local (file relative)
		return read(file.offset_begin + local_offset, size);
	}
	
	std::string_view read(Snippet content, uint32_t local_offset, uint32_t size) const {//local (file relative)
		return read(content.offset + local_offset, size);
	}

	char getChar(uint32_t global_offset) const { return data[global_offset]; }
	//Notare che il file restituisce l'offset relativo a inizio file (includendo il nome)
	char getChar(FileInfo file, uint32_t local_offset) const { return data[file.offset_begin + local_offset]; }
	//Mentre lo snippet (content) parte dall'inizio snippet, generalmente usato per escludere il nome
	char getChar(Snippet content, uint32_t local_offset) const { return data[content.offset + local_offset]; }

	std::string_view operator[](Snippet s) const {
		return read(s.offset, s.size);
	}

	std::string_view operator[](FileInfo info) const {
		return (*this)[info.content()];
	}

	std::string_view operator[](uint32_t index) const { return (*this)[loaded[index]]; }
};