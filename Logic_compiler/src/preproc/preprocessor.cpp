#include "preprocessor.h"
#include <cctype>
#include <string>

SourceView Preprocessor::loadView(const char* pathrel) {
    // Carica il file tramite FileManager
    SourceView new_view = file_mgr.loadFile(pathrel);

    // Salva nella cache circolare (sovrascrive da 0 a 63)
    loadedCache[cacheCounter] = new_view;
    cacheCounter = (cacheCounter + 1) % 64;

    return new_view;
}

void Preprocessor::skip_whitespace_and_comments() {
    while (cursor < source.length()) {
        char c = source[cursor];

        // Salta spazi e tab (ma preserva i newline per il tracciamento)
        if (c == ' ' || c == '\t' || c == '\r') {
            cursor++;
            continue;
        }

        // Gestione commenti //
        if (c == '/' && cursor + 1 < source.length() && source[cursor + 1] == '/') {
            cursor += 2;
            while (cursor < source.length() && source[cursor] != '\n') {
                cursor++;
            }
            continue;
        }

        // Gestione commenti /* */
        if (c == '/' && cursor + 1 < source.length() && source[cursor + 1] == '*') {
            cursor += 2;
            while (cursor + 1 < source.length() && !(source[cursor] == '*' && source[cursor + 1] == '/')) {
                cursor++;
            }
            cursor = std::min(cursor + 2, source.length());
            continue;
        }

        break;
    }
}

std::string_view Preprocessor::next_token() {
    skip_whitespace_and_comments();
    if (cursor >= source.length()) return {};

    size_t start = cursor;
    while (cursor < source.length() && !std::isspace(static_cast<unsigned char>(source[cursor]))) {
        cursor++;
    }
    return source.get_str().substr(start, cursor - start);
}

std::string_view Preprocessor::read_line() {
    size_t start = cursor;
    while (cursor < source.length() && source[cursor] != '\n') {
        cursor++;
    }
    std::string_view line = source.get_str().substr(start, cursor - start);
    if (cursor < source.length() && source[cursor] == '\n') cursor++; // Consuma \n
    return line;
}

void Preprocessor::process_directive(MacroType type) {
    switch (type) {
    case MacroType::Define:
    case MacroType::DefineM: {
        std::string_view name = next_token();
        if (!name.empty()) {
            defined_macros.insert(name);
        }
        read_line(); // Scarta il resto della riga
        break;
    }
    case MacroType::Include: {
        skip_whitespace_and_comments();
        size_t start = cursor;
        while (cursor < source.length() && source[cursor] != '\n' && source[cursor] != '\r') {
            cursor++;
        }
        std::string_view raw_path = source.get_str().substr(start, cursor - start);

        // Pulisce le virgolette o i punti angolari ("file.h" o <file.h>)
        size_t p_start = raw_path.find_first_of("\"<");
        size_t p_end = raw_path.find_last_of("\">");

        if (p_start != std::string_view::npos && p_end != std::string_view::npos && p_end > p_start) {
            std::string path(raw_path.substr(p_start + 1, p_end - p_start - 1));

            // Preprocessa ricorsivamente il file incluso
            SourceView inc_view = loadView(path.c_str());
            if (!inc_view.empty()) {
                Preprocessor sub_prep(inc_view);
                sub_prep.defined_macros = this->defined_macros; // Eredita le macro
                sub_prep.process();

                std::string_view res = sub_prep.get_result();
                output_buffer.insert(output_buffer.end(), res.begin(), res.end());
            }
        }
        break;
    }
    case MacroType::Ifndef: {
        std::string_view name = next_token();
        bool is_defined = defined_macros.find(name) != defined_macros.end();

        // Se è definito, saltiamo il blocco fino a #enddef
        if (is_defined) {
            while (cursor < source.length()) {
                if (source[cursor] == '#') {
                    size_t saved_cursor = cursor;
                    std::string_view tok = next_token();
                    if (macro_hash_match(tok) == MacroType::Enddef) {
                        read_line();
                        break;
                    }
                }
                else {
                    cursor++;
                }
            }
        }
        break;
    }
    case MacroType::Skip: {
        // Salta fino a #enddef
        while (cursor < source.length()) {
            if (source[cursor] == '#') {
                size_t saved_cursor = cursor;
                std::string_view tok = next_token();
                if (macro_hash_match(tok) == MacroType::Enddef) {
                    read_line();
                    break;
                }
            }
            else {
                cursor++;
            }
        }
        break;
    }
    case MacroType::Enddef:
        // Gestito contestualmente dai blocchi condizionali o ignorato se isolato
        read_line();
        break;
    default:
        read_line();
        break;
    }
}

void Preprocessor::process() {
    while (cursor < source.length()) {
        char c = source[cursor];

        if (c == '#') {
            size_t token_start = cursor;
            std::string_view token = next_token();
            MacroType type = macro_hash_match(token);

            if (type != MacroType::Unknown && type != MacroType::None) {
                process_directive(type);
            }
            else {
                // Non è una direttiva valida, scrivi come testo normale
                output_buffer.insert(output_buffer.end(), source.get() + token_start, source.get() + cursor);
            }
        }
        else {
            output_buffer.push_back(c);
            cursor++;
        }
    }
}