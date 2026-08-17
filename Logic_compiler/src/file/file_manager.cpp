#include <fstream>
#include <cstring>
#include <string>
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

    // Build the full path using src_path if provided
    std::string full_path;
    if (src_path && *src_path) {
        full_path = src_path;
        char last = full_path.back();
        if (last != '/' && last != '\\') {
            full_path += '/';
        }
        full_path += path;
    }
    else {
        full_path = path;
    }

    std::ifstream file(full_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return _NOT_FOUND;

    const auto file_sz = file.tellg();
    if (file_sz < 0) return _NOT_FOUND;

    file.seekg(0, std::ios::beg);

    const uint32_t name_len = static_cast<uint32_t>(name_sv.size());
    const uint32_t raw_content_len = static_cast<uint32_t>(file_sz);
    const uint32_t current_offset = static_cast<uint32_t>(data.size());

    // Allocate upper-bound space in data: [nome_file][\0][raw_content]
    const size_t max_entry_size = name_len + 1 + raw_content_len;
    data.resize(current_offset + max_entry_size);

    char* write_ptr = data.data() + current_offset;
    std::memcpy(write_ptr, path, name_len);
    write_ptr[name_len] = '\0';

    // Read directly into destination buffer
    char* content_dest = write_ptr + name_len + 1;
    file.read(content_dest, raw_content_len);

    // In-place CRLF (\r\n) -> LF (\n) normalization pass
    uint32_t sanitized_len = 0;
    for (uint32_t r = 0; r < raw_content_len; ++r) {
        char ch = content_dest[r];
        if (ch == '\r') {
            // Convert \r\n or standalone \r into a single \n
            content_dest[sanitized_len++] = '\n';
            if (r + 1 < raw_content_len && content_dest[r + 1] == '\n') {
                ++r; // Skip the paired '\n'
            }
        }
        else {
            content_dest[sanitized_len++] = ch;
        }
    }

    // Shrink data vector to actual sanitized size
    const size_t actual_entry_size = name_len + 1 + sanitized_len;
    data.resize(current_offset + actual_entry_size);

    FileInfo info{ current_offset, name_len, sanitized_len };
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