#pragma once
#include <vector>
#include <cstdint>
#include "file_def.h"



class FileManager {
	const char* src_path = nullptr;
	std::vector<SourceFile> loaded;
	size_t next_free = 0; //points to the earliest free space in loaded (simple O(N) since loaded is small)
	//check done on data == nullptr ^ size == 0, path can hold next_free
public:
	FileManager(const char* base_path) : src_path(base_path) { loaded.reserve(100); }

	SourceView loadFile(const char* path); //loads in loaded (if not already loaded), returns View struct
	SourceFile pullFile(const char* path) const; //moves directly in return, no loading
	SourceFile PopFile(const char* path); //Moves ownership to return from loaded, if not loaded reads from disk
	SourceFile PopFile(size_t ldIndex); //Moves (thus leaving an empty space) from loaded[ldIndex]
	void RmFile(const char* path); //Frees the file from loaded
};