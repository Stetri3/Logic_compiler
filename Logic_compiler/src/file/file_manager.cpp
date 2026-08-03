#include <fstream>
#include <cstring>
#include "file_manager.h"

FileInfo FileManager::loadFile(const char* path) {
    if (!path) return _NOT_FOUND;

    std::string_view name_sv(path);
    FileInfo existing = _findFile(name_sv);
    if (existing.offset_begin != _NOT_FOUND.offset_begin ||
        existing.name_size != _NOT_FOUND.name_size ||
        existing.file_size != _NOT_FOUND.file_size) {
        return existing;
    }

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return _NOT_FOUND;

    const auto file_sz = file.tellg();
    if (file_sz < 0) return _NOT_FOUND;

    file.seekg(0, std::ios::beg);

    const uint32_t name_len = static_cast<uint32_t>(name_sv.size());
    const uint32_t content_len = static_cast<uint32_t>(file_sz);
    const uint32_t current_offset = static_cast<uint32_t>(data.size());

    // Layout in data: [nome_file][\0][contenuto_file]
    const size_t total_entry_size = name_len + 1 + content_len;
    data.resize(current_offset + total_entry_size);

    char* write_ptr = data.data() + current_offset;
    std::memcpy(write_ptr, path, name_len);
    write_ptr[name_len] = '\0';

    file.read(write_ptr + name_len + 1, content_len);

    FileInfo info{ current_offset, name_len, content_len };
    loaded.push_back(info);
    return info;
}

FileInfo FileManager::loadFromView(std::string_view view, const char* filename) {
    if (!filename) return _NOT_FOUND;

    std::string_view name_sv(filename);
    FileInfo existing = _findFile(name_sv);
    if (existing.offset_begin != _NOT_FOUND.offset_begin ||
        existing.name_size != _NOT_FOUND.name_size ||
        existing.file_size != _NOT_FOUND.file_size) {
        return existing;
    }

    const uint32_t name_len = static_cast<uint32_t>(name_sv.size());
    const uint32_t content_len = static_cast<uint32_t>(view.length());
    const uint32_t current_offset = static_cast<uint32_t>(data.size());

    const size_t total_entry_size = name_len + 1 + content_len;
    data.resize(current_offset + total_entry_size);

    char* write_ptr = data.data() + current_offset;
    std::memcpy(write_ptr, filename, name_len);
    write_ptr[name_len] = '\0';

    if (content_len > 0 && view.data()) {
        std::memcpy(write_ptr + name_len + 1, view.data(), content_len);
    }

    FileInfo info{ current_offset, name_len, content_len };
    loaded.push_back(info);
    return info;
}

void FileManager::pop() {
    if (loaded.empty()) return;

    const FileInfo last = loaded.back();
    loaded.pop_back();

    // Ridimensiona il buffer contiguo per rilasciare l'ultimo elemento
    data.resize(last.offset_begin);
}