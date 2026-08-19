#pragma once
#include "CIntegers.h"
#include "CType_traits.h"
#include "std_types.h"
#include "CTime.h"

// --- Syscall & OS Level Bindings ---

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
extern "C" {
    long write(int fd, const void* buf, decltype(sizeof(0)) count) noexcept;
}
#endif

namespace cc::io {

    using size_type = decltype(sizeof(0));

    // --- Low-Level Console Write ---

    inline void raw_write(const char* str, size_type len) noexcept {
        if (!str || len == 0) return;

#if defined(_WIN32) || defined(_WIN64)
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE && hOut != nullptr) {
            DWORD written = 0;
            WriteFile(hOut, str, static_cast<DWORD>(len), &written, nullptr);
        }
#else
        // File descriptor 1 = stdout
        write(1, str, len);
#endif
    }

    inline size_type cstr_length(const char* str) noexcept {
        if (!str) return 0;
        size_type len = 0;
        while (str[len] != '\0') ++len;
        return len;
    }


    // --- String View Concept Detection ---
    // Rileva se un tipo ha .CStrView() o .c_str_view() che espone {const char*, unsigned_integral}

    template <typename V>
    concept StringViewLike = requires(V v) {
        { v.data() } -> same_as<const char*>;
        { v.size() } -> unsigned_integral;
    } || requires(V v) {
        { v.ptr } -> same_as<const char*>;
        { v.size } -> unsigned_integral;
    } || requires(V v) {
        { v.data } -> same_as<const char*>;
        { v.len } -> unsigned_integral;
    };

    template <typename T>
    concept CustomPrintableByMethod = requires(const T & obj) {
        { obj.CStrView() } -> StringViewLike;
    } || requires(const T & obj) {
        { obj.c_str_view() } -> StringViewLike;
    };


    // --- Fast Formatting Utilities ---

    template <integral T>
    inline size_type format_integral(char* buffer, T val) noexcept {
        char* p = buffer;
        using UnsignedT = make_unsigned_t<T>;
        UnsignedT uval = 0;

        if constexpr (is_signed_v<T>) {
            if (val < 0) {
                *p++ = '-';
                uval = static_cast<UnsignedT>(0) - static_cast<UnsignedT>(val);
            }
            else {
                uval = static_cast<UnsignedT>(val);
            }
        }
        else {
            uval = static_cast<UnsignedT>(val);
        }

        // Buffer temporaneo per inversione cifre
        char temp[32];
        size_type digits = 0;
        do {
            temp[digits++] = static_cast<char>('0' + (uval % 10));
            uval /= 10;
        } while (uval > 0);

        for (size_type i = digits; i > 0; --i) {
            *p++ = temp[i - 1];
        }

        return static_cast<size_type>(p - buffer);
    }

    // Formatta un float/double a precisione fissa (default 3 decimali) senza dipendenze
    inline size_type format_floating(char* buffer, double val, unsigned int precision = 3) noexcept {
        char* p = buffer;

        if (val < 0.0) {
            *p++ = '-';
            val = -val;
        }

        auto int_part = static_cast<uint64_t>(val);
        double remainder = val - static_cast<double>(int_part);

        p += format_integral(p, int_part);

        if (precision > 0) {
            *p++ = '.';
            for (unsigned int i = 0; i < precision; ++i) {
                remainder *= 10.0;
                auto digit = static_cast<unsigned int>(remainder);
                *p++ = static_cast<char>('0' + digit);
                remainder -= static_cast<double>(digit);
            }
        }

        return static_cast<size_type>(p - buffer);
    }


    // --- CCout Implementation ---

    class FastOutputStream {
    public:
        constexpr FastOutputStream() noexcept = default;

        // 1. Stringhe raw / puntatori a char
        FastOutputStream& operator<<(const char* str) noexcept {
            if (str) {
                raw_write(str, cstr_length(str));
            }
            return *this;
        }

        // 2. Singoli caratteri
        FastOutputStream& operator<<(char c) noexcept {
            raw_write(&c, 1);
            return *this;
        }

        // 3. Booleani
        FastOutputStream& operator<<(bool b) noexcept {
            if (b) {
                raw_write("true", 4);
            }
            else {
                raw_write("false", 5);
            }
            return *this;
        }

        // 4. CSpan<const char> o CSpan<char>
        template <typename T>
            requires (is_same_v<remove_cv_t<T>, char>)
        FastOutputStream& operator<<(const CSpan<T>& span) noexcept {
            raw_write(span.data(), span.size());
            return *this;
        }

        // 5. Array statici di char (es. string literals T[N])
        template <size_type N>
        FastOutputStream& operator<<(const char(&arr)[N]) noexcept {
            size_type len = (N > 0 && arr[N - 1] == '\0') ? N - 1 : N;
            raw_write(arr, len);
            return *this;
        }

        // 6. Interi generici
        template <integral T>
            requires (!same_as<remove_cv_t<T>, char> && !same_as<remove_cv_t<T>, bool>)
        FastOutputStream& operator<<(T val) noexcept {
            char buffer[32];
            size_type len = format_integral(buffer, val);
            raw_write(buffer, len);
            return *this;
        }

        // 7. Floating Point (float, double, long double)
        template <floating_point T>
        FastOutputStream& operator<<(T val) noexcept {
            char buffer[64];
            size_type len = format_floating(buffer, static_cast<double>(val));
            raw_write(buffer, len);
            return *this;
        }

        // 8. CDuration / Duration (unita' auto-adattiva: s, ms, us, ns)
        FastOutputStream& operator<<(const time::Duration& d) noexcept {
            int64_t ns = d.nanoseconds();
            int64_t abs_ns = (ns < 0) ? -ns : ns;

            if (abs_ns >= 1'000'000'000LL) {
                *this << d.as_seconds() << " s";
            }
            else if (abs_ns >= 1'000'000LL) {
                *this << d.as_milliseconds() << " ms";
            }
            else if (abs_ns >= 1'000LL) {
                *this << d.as_microseconds() << " us";
            }
            else {
                *this << ns << " ns";
            }
            return *this;
        }

        // 9. CTimer / Timer (stampa il tempo trascorso corrente)
        FastOutputStream& operator<<(const time::Timer& t) noexcept {
            return *this << t.elapsed();
        }

        // 10. CTimePoint / TimePoint (stampa il tick grezzo)
        FastOutputStream& operator<<(const time::TimePoint& tp) noexcept {
            *this << "@" << tp.raw_ticks() << " ticks";
            return *this;
        }

        // 11. Puntatori a oggetti/dati (esclude puntatori a funzione e a char)
        template <typename T>
            requires (pointer<T>
        && !same_as<remove_cv_t<remove_pointer_t<T>>, char>
            && !is_function_v<remove_pointer_t<T>>)
            FastOutputStream& operator<<(T ptr) noexcept {
            if (!ptr) {
                raw_write("0x0", 3);
                return *this;
            }

            char buffer[32];
            buffer[0] = '0';
            buffer[1] = 'x';

            auto val = reinterpret_cast<decltype(sizeof(0))>(ptr);
            char temp[24];
            size_type digits = 0;
            const char* hex_digits = "0123456789abcdef";

            do {
                temp[digits++] = hex_digits[val & 0xF];
                val >>= 4;
            } while (val > 0);

            size_type idx = 2;
            for (size_type i = digits; i > 0; --i) {
                buffer[idx++] = temp[i - 1];
            }

            raw_write(buffer, idx);
            return *this;
        }

        // 12. Tipi personalizzati tramite CStrView() o c_str_view()
        template <CustomPrintableByMethod T>
        FastOutputStream& operator<<(const T& obj) noexcept {
            if constexpr (requires { obj.CStrView(); }) {
                auto view = obj.CStrView();
                write_view(view);
            }
            else if constexpr (requires { obj.c_str_view(); }) {
                auto view = obj.c_str_view();
                write_view(view);
            }
            return *this;
        }

        // 13. Supporto a manipolatori (noexcept e non-noexcept)
        FastOutputStream& operator<<(FastOutputStream& (*manip)(FastOutputStream&) noexcept) noexcept {
            return manip(*this);
        }

        FastOutputStream& operator<<(FastOutputStream& (*manip)(FastOutputStream&)) noexcept {
            return manip(*this);
        }

    private:
        template <typename V>
        static void write_view(const V& v) noexcept {
            if constexpr (requires { { v.data() } -> same_as<const char*>; }) {
                raw_write(v.data(), static_cast<size_type>(v.size()));
            }
            else if constexpr (requires { { v.ptr } -> same_as<const char*>; }) {
                raw_write(v.ptr, static_cast<size_type>(v.size));
            }
            else if constexpr (requires { { v.data } -> same_as<const char*>; }) {
                raw_write(v.data, static_cast<size_type>(v.len));
            }
        }
    };

    inline FastOutputStream& endl(FastOutputStream& os) noexcept {
#if defined(_WIN32) || defined(_WIN64)
        raw_write("\r\n", 2);
#else
        raw_write("\n", 1);
#endif
        return os;
    }

} // namespace cc::io


namespace cc {

    // Concept esportato pubblicamente per verificare se un tipo è stampabile con CStrView
    template <typename T>
    concept custom_printable = io::CustomPrintableByMethod<T>;

    // Manipolatore per newline
    using io::endl;

    // Oggetto globale inline per l'output su console
    inline io::FastOutputStream CCout{};

} // namespace cc