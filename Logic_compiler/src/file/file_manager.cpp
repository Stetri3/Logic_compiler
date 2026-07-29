#include "file_manager.h"
#include <cstring>
#include <fstream>
#include <memory>


// ==========================================
// HELPER PRIVATI (I/O e Path Resolution)
// ==========================================

// Costruisce il path completo (src_path + relative_path) gestendo le slash
static std::unique_ptr<char[]> build_full_path(const char* base_path, const char* rel_path) {
    if (!base_path || !*base_path) {
        size_t len = std::strlen(rel_path) + 1;
        auto full = std::make_unique<char[]>(len);
        std::memcpy(full.get(), rel_path, len);
        return full;
    }

    size_t base_len = std::strlen(base_path);
    size_t rel_len = std::strlen(rel_path);
    bool needs_slash = (base_path[base_len - 1] != '/' && base_path[base_len - 1] != '\\');

    size_t total_len = base_len + (needs_slash ? 1 : 0) + rel_len + 1;
    auto full = std::make_unique<char[]>(total_len);

    std::memcpy(full.get(), base_path, base_len);
    size_t offset = base_len;
    if (needs_slash) {
        full[offset++] = '/';
    }
    std::memcpy(full.get() + offset, rel_path, rel_len + 1);

    return full;
}

// Cerca un file per path dentro 'loaded'.
// data != nullptr distingue qualsiasi file valido (anche vuoto) da uno slot libero.
static int find_loaded_index(const std::vector<SourceFile>& loaded, const char* path) noexcept {
    for (size_t i = 0; i < loaded.size(); ++i) {
        if (loaded[i].data != nullptr && loaded[i].path != nullptr) {
            if (std::strcmp(loaded[i].path, path) == 0) {
                return static_cast<int>(i);
            }
        }
    }
    return -1;
}

// ==========================================
// METODI PUBBLICI
// ==========================================

SourceFile FileManager::pullFile(const char* path) const {
    auto full_path = build_full_path(src_path, path);

    std::ifstream file(full_path.get(), std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return SourceFile{ nullptr, nullptr, 0 }; // File non trovato o errore I/O
    }

    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (file_size < 0) {
        return SourceFile{ nullptr, nullptr, 0 };
    }

    const size_t bytes_to_read = static_cast<size_t>(file_size);

    // Allochiamo sempre (bytes_to_read + 1) per il '\0' finale.
    // In questo modo anche per un file vuoto (bytes_to_read == 0) data != nullptr!
    auto buffer = std::make_unique<char[]>(bytes_to_read + 1);

    if (bytes_to_read > 0) {
        if (!file.read(buffer.get(), bytes_to_read)) {
            return SourceFile{ nullptr, nullptr, 0 };
        }
    }

    // Null-terminator di sicurezza per Lexer/Preprocessor
    buffer[bytes_to_read] = '\0';

    return SourceFile{ path, std::move(buffer), bytes_to_read };
}

SourceView FileManager::loadFile(const char* path) {
    // 1. Controlla se è già in memoria
    int idx = find_loaded_index(loaded, path);
    if (idx != -1) {
        return loaded[idx].cref();
    }

    // 2. Leggi da disco
    SourceFile sf = pullFile(path);
    if (sf.data == nullptr) {
        return SourceView{ nullptr, nullptr, 0 };
    }

    // 3. Inserisci usando la Free List intrinseca O(1)
    if (next_free < loaded.size()) {
        size_t target_slot = next_free;

        // Estrae l'indice del prossimo slot libero memorizzato nel campo 'path'
        next_free = reinterpret_cast<uintptr_t>(loaded[target_slot].path);

        loaded[target_slot] = std::move(sf);
        return loaded[target_slot].cref();
    }
    else {
        loaded.push_back(std::move(sf));
        next_free = loaded.size();
        return loaded.back().cref();
    }
}

SourceFile FileManager::PopFile(size_t ldIndex) {
    if (ldIndex >= loaded.size() || loaded[ldIndex].data == nullptr) {
        return SourceFile{ nullptr, nullptr, 0 };
    }

    // Sposta l'ownership fuori dal vector
    SourceFile moved = std::move(loaded[ldIndex]);

    // Trasforma lo slot in libero: data torna nullptr e path traccia il vecchio next_free
    loaded[ldIndex].data = nullptr;
    loaded[ldIndex].size = 0;
    loaded[ldIndex].path = reinterpret_cast<const char*>(static_cast<uintptr_t>(next_free));

    next_free = ldIndex;

    return moved;
}

SourceFile FileManager::PopFile(const char* path) {
    int idx = find_loaded_index(loaded, path);
    if (idx != -1) {
        return PopFile(static_cast<size_t>(idx));
    }

    // Se non era stato caricato, lo legge da disco e lo ritorna direttamente
    return pullFile(path);
}

void FileManager::RmFile(const char* path) {
    int idx = find_loaded_index(loaded, path);
    if (idx != -1) {
        PopFile(static_cast<size_t>(idx));
    }
}