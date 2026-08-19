#pragma once
#include "CIntegers.h"
#include "CType_traits.h"

// --- OS Timer Bindings ---

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif

namespace cc::time {

    // --- High-Resolution Monotonic Clock Backend ---

    namespace detail {
#if defined(_WIN32) || defined(_WIN64)
        inline int64_t get_frequency() noexcept {
            LARGE_INTEGER freq;
            QueryPerformanceFrequency(&freq);
            return freq.QuadPart;
        }

        inline int64_t get_ticks() noexcept {
            LARGE_INTEGER counter;
            QueryPerformanceCounter(&counter);
            return counter.QuadPart;
        }

        inline int64_t ticks_to_nanoseconds(int64_t ticks) noexcept {
            static const int64_t freq = get_frequency();
            // Evita overflow moltiplicando prima di dividere quando possibile
            return (ticks * 1'000'000'000LL) / freq;
        }
#else
        inline int64_t get_ticks() noexcept {
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            return static_cast<int64_t>(ts.tv_sec) * 1'000'000'000LL + static_cast<int64_t>(ts.tv_nsec);
        }

        inline int64_t ticks_to_nanoseconds(int64_t ticks) noexcept {
            return ticks; // Su POSIX i ticks sono già in nanosecondi
        }
#endif
    } // namespace detail


    // --- Duration Representation ---

    class Duration {
    private:
        int64_t _ns = 0;

    public:
        constexpr Duration() noexcept = default;
        constexpr explicit Duration(int64_t nanoseconds) noexcept : _ns(nanoseconds) {}

        // Factory Constructors
        [[nodiscard]] static constexpr Duration from_nanoseconds(int64_t ns) noexcept { return Duration(ns); }
        [[nodiscard]] static constexpr Duration from_microseconds(int64_t us) noexcept { return Duration(us * 1'000LL); }
        [[nodiscard]] static constexpr Duration from_milliseconds(int64_t ms) noexcept { return Duration(ms * 1'000'000LL); }
        [[nodiscard]] static constexpr Duration from_seconds(int64_t s) noexcept { return Duration(s * 1'000'000'000LL); }
        [[nodiscard]] static constexpr Duration from_seconds(double s) noexcept {
            return Duration(static_cast<int64_t>(s * 1'000'000'000.0));
        }

        // Integer Accessors
        [[nodiscard]] constexpr int64_t nanoseconds() const noexcept { return _ns; }
        [[nodiscard]] constexpr int64_t microseconds() const noexcept { return _ns / 1'000LL; }
        [[nodiscard]] constexpr int64_t milliseconds() const noexcept { return _ns / 1'000'000LL; }
        [[nodiscard]] constexpr int64_t seconds() const noexcept { return _ns / 1'000'000'000LL; }

        // Floating Point Accessors
        [[nodiscard]] constexpr double as_nanoseconds() const noexcept { return static_cast<double>(_ns); }
        [[nodiscard]] constexpr double as_microseconds() const noexcept { return static_cast<double>(_ns) / 1'000.0; }
        [[nodiscard]] constexpr double as_milliseconds() const noexcept { return static_cast<double>(_ns) / 1'000'000.0; }
        [[nodiscard]] constexpr double as_seconds() const noexcept { return static_cast<double>(_ns) / 1'000'000'000.0; }

        // Arithmetic Operators
        constexpr Duration operator+(const Duration& other) const noexcept { return Duration(_ns + other._ns); }
        constexpr Duration operator-(const Duration& other) const noexcept { return Duration(_ns - other._ns); }
        constexpr Duration& operator+=(const Duration& other) noexcept { _ns += other._ns; return *this; }
        constexpr Duration& operator-=(const Duration& other) noexcept { _ns -= other._ns; return *this; }

        template <arithmetic Num>
        constexpr Duration operator*(Num scalar) const noexcept {
            return Duration(static_cast<int64_t>(_ns * scalar));
        }

        template <arithmetic Num>
        constexpr Duration operator/(Num scalar) const noexcept {
            return Duration(static_cast<int64_t>(_ns / scalar));
        }

        // Comparisons
        constexpr bool operator==(const Duration& other) const noexcept = default;
        constexpr auto operator<=>(const Duration& other) const noexcept = default;
    };


    // --- TimePoint Representation ---

    class TimePoint {
    private:
        int64_t _ticks = 0;

    public:
        constexpr TimePoint() noexcept = default;
        constexpr explicit TimePoint(int64_t ticks) noexcept : _ticks(ticks) {}

        [[nodiscard]] static TimePoint now() noexcept {
            return TimePoint(detail::get_ticks());
        }

        [[nodiscard]] constexpr int64_t raw_ticks() const noexcept { return _ticks; }

        [[nodiscard]] Duration operator-(const TimePoint& other) const noexcept {
            int64_t delta_ticks = _ticks - other._ticks;
            return Duration(detail::ticks_to_nanoseconds(delta_ticks));
        }

        constexpr bool operator==(const TimePoint& other) const noexcept = default;
        constexpr auto operator<=>(const TimePoint& other) const noexcept = default;
    };


    // --- High-Performance Stopwatch / Timer ---

    class Timer {
    private:
        TimePoint _start_time;

    public:
        Timer() noexcept {
            reset();
        }

        // Riavvia il timer
        void reset() noexcept {
            _start_time = TimePoint::now();
        }

        // Calcola il tempo trascorso senza resettare
        [[nodiscard]] Duration elapsed() const noexcept {
            return TimePoint::now() - _start_time;
        }

        [[nodiscard]] double elapsed_s() const noexcept {
            return elapsed().as_seconds();
        }

        [[nodiscard]] double elapsed_ms() const noexcept {
            return elapsed().as_milliseconds();
        }

        [[nodiscard]] double elapsed_us() const noexcept {
            return elapsed().as_microseconds();
        }

        [[nodiscard]] int64_t elapsed_ns() const noexcept {
            return elapsed().nanoseconds();
        }

        // Ritorna il tempo trascorso e resetta l'origine al momento corrente (Lap)
        Duration lap() noexcept {
            TimePoint current = TimePoint::now();
            Duration dt = current - _start_time;
            _start_time = current;
            return dt;
        }
    };

} // namespace cc::time


namespace cc {

    // Deploy Type Aliases (C[Name] Standard)
    using CDuration = time::Duration;
    using CTimePoint = time::TimePoint;
    using CTimer = time::Timer;

} // namespace cc