#pragma once

/* 
    ============================================================
    nilo::console - header-only cross-platform console utilities
    ------------------------------------------------------------
    Author  : nilotoo.
    Version : v1.0.0
    Updated : 2026-10
    License : MIT
    ------------------------------------------------------------
    SPDX-License-Identifier: MIT
    SPDX-FileCopyrightText: 2026 nilotoo.

    Hi, I'm nilotoo.
    Thank you for using this header-only library.
    Licensed under the MIT License. Copyright (c) 2026 nilotoo.

    This is a console utility toolkit, mainly focused on:
    * console commands
    * output streams
    * input streams
    * buffer clearing / flushing

    !!!Requires : C++17 or later (std::optional, std::string_view, if constexpr)
    ------------------------------------------------------------
    Install :
        Place this file at:
        <your-project>/include/nilo/console.hpp
        (create the `nilo` folder under your include
        directory if it does not exist)

    Then include it as:
        #include <nilo/console.hpp>
    ------------------------------------------------------------
    Thread safety:     
        initConsole() may be called concurrently.
        Other APIs are not thread-safe.
    ============================================================
*/

#if defined(_MSVC_LANG)
    #if _MSVC_LANG < 201703L
        #error "nilo::console requires C++17 or later (MSVC: use /std:c++17 or newer)"
    #endif
#elif defined(__cplusplus)
    #if __cplusplus < 201703L
        #error "nilo::console requires C++17 or later (use -std=c++17 or newer)"
    #endif
#else
    #error "nilo::console requires a C++17-capable compiler"
#endif

// Library version macros -------------------------------------
#define NILO_CONSOLE_VERSION_MAJOR 1
#define NILO_CONSOLE_VERSION_MINOR 0
#define NILO_CONSOLE_VERSION_PATCH 0

#define NILO_CONSOLE_VERSION_STRING      "1.0.0"
#define NILO_CONSOLE_VERSION_STRING_FULL "nilo::console v1.0.0 (2026-10)"

// Numeric form for easy comparison: 0xMMmmpp
#define NILO_CONSOLE_VERSION \
    ((NILO_CONSOLE_VERSION_MAJOR << 16) | \
     (NILO_CONSOLE_VERSION_MINOR <<  8) | \
     (NILO_CONSOLE_VERSION_PATCH))

#include <array>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #include <conio.h>
#else
    #include <unistd.h>
    #include <poll.h>
    #include <termios.h>
#endif

namespace nilo::console {

// ---- Version constants (namespace-scoped) ------------------
inline constexpr int              VERSION_MAJOR       = NILO_CONSOLE_VERSION_MAJOR;
inline constexpr int              VERSION_MINOR       = NILO_CONSOLE_VERSION_MINOR;
inline constexpr int              VERSION_PATCH       = NILO_CONSOLE_VERSION_PATCH;
inline constexpr std::string_view VERSION_STRING      = NILO_CONSOLE_VERSION_STRING;
inline constexpr std::string_view VERSION_STRING_FULL = NILO_CONSOLE_VERSION_STRING_FULL;

// Forward declaration.
inline bool initConsole();

namespace detail {

// ---------------------------------------------------------
// Cache the result of initConsole() so ensureConsoleInit()
// can return a stable status. std::call_once also makes it
// safe under multithreaded first use.
// ---------------------------------------------------------
inline bool& consoleInitResult() {
    static bool r = false;
    return r;
}

inline bool ensureConsoleInit() {
#ifdef _WIN32
    static std::once_flag once;
    std::call_once(once, [] { consoleInitResult() = initConsole(); });
    return consoleInitResult();
#else
    return true;
#endif
}

#ifndef _WIN32
// RAII guard that switches the POSIX terminal into raw mode
// and restores the original settings on destruction.
struct TermGuard {
    termios oldt{};
    bool active = false;

    TermGuard() {
        if (tcgetattr(STDIN_FILENO, &oldt) != 0) return;
        termios newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        newt.c_cc[VMIN]  = 1;
        newt.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &newt) != 0) return;
        active = true;
    }
    ~TermGuard() {
        if (active) tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    }
    TermGuard(const TermGuard&) = delete;
    TermGuard& operator=(const TermGuard&) = delete;
};

// Read a single byte from stdin, retrying on EINTR.
// Returns false on EOF or unrecoverable error.
inline bool readOneCharRaw(char& ch) {
    char c = 0;
    for (;;) {
        ssize_t n = ::read(STDIN_FILENO, &c, 1);
        if (n == 1) { ch = c; return true; }
        if (n < 0 && errno == EINTR) continue;
        return false;
    }
}
#endif

// ---------------------------------------------------------
// String helpers
// ---------------------------------------------------------
inline std::string_view trimView(std::string_view s) {
    constexpr std::string_view WS = " \t\r\n\f\v";
    auto first = s.find_first_not_of(WS);
    if (first == std::string_view::npos) return {};
    auto last = s.find_last_not_of(WS);
    return s.substr(first, last - first + 1);
}

inline std::string trimCopy(std::string_view s) {
    return std::string(trimView(s));
}

inline std::string toLowerAscii(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        out.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

// ---------------------------------------------------------
// parseValue: parses a single value from a trimmed string.
//   - Rejects trailing garbage (requires ws + eof).
//   - Rejects leading '-' for unsigned integer types.
//   - Rejects non-finite floating-point values (NaN, Inf).
// ---------------------------------------------------------
template <typename T>
bool parseValue(std::string_view s, T& out) {
    if constexpr (std::is_same_v<T, std::string>) {
        out = std::string(s);
        return true;
    }
    else if constexpr (std::is_same_v<T, bool>) {
        auto v = toLowerAscii(s);
        if (v == "1" || v == "y" || v == "yes" ||
            v == "true"  || v == "t" || v == "on")  { out = true;  return true; }
        if (v == "0" || v == "n" || v == "no"  ||
            v == "false" || v == "f" || v == "off") { out = false; return true; }
        return false;
    }
    else if constexpr (std::is_arithmetic_v<T>) {
        static_assert(!std::is_same_v<T, char> &&
                      !std::is_same_v<T, signed char> &&
                      !std::is_same_v<T, unsigned char>,
                      "char types not supported; use std::string instead");

        // Unsigned integer types must not receive a leading '-'.
        // (istringstream would otherwise wrap e.g. "-1" to UINT_MAX.)
        if constexpr (std::is_unsigned_v<T>) {
            auto t = trimView(s);
            if (!t.empty() && t.front() == '-') return false;
        }

        std::istringstream iss{std::string(s)};
        T v{};
        iss >> v;
        if (iss.fail()) return false;

        // Reject NaN/Inf for floating-point types.
        if constexpr (std::is_floating_point_v<T>) {
            if (!std::isfinite(static_cast<double>(v))) return false;
        }

        iss >> std::ws;
        if (!iss.eof()) return false;  // trailing junk -> reject
        out = v;
        return true;
    }
    else {
        // User-defined type: fall back to operator>>.
        std::istringstream iss{std::string(s)};
        T v{};
        iss >> v;
        if (iss.fail()) return false;
        iss >> std::ws;
        if (!iss.eof()) return false;
        out = v;
        return true;
    }
}

// Returns true when stdin/stdout look like an interactive console.
inline bool isInteractiveConsole() {
#ifdef _WIN32
    HANDLE hIn  = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD  inMode = 0, outMode = 0;
    if (hIn  == INVALID_HANDLE_VALUE || hIn  == nullptr) return false;
    if (hOut == INVALID_HANDLE_VALUE || hOut == nullptr) return false;
    if (!GetConsoleMode(hIn,  &inMode))  return false;
    if (!GetConsoleMode(hOut, &outMode)) return false;
    return true;
#else
    return ::isatty(STDIN_FILENO) != 0 && ::isatty(STDOUT_FILENO) != 0;
#endif
}

} // namespace detail

// =========================================================
// initConsole
//   Enables UTF-8 code page + ANSI VT processing on Windows.
//   No-op on POSIX (terminals are expected to speak ANSI).
// =========================================================
inline bool initConsole() {
#ifdef _WIN32
    bool outOk = SetConsoleOutputCP(CP_UTF8) != 0;
    SetConsoleCP(CP_UTF8);         // input CP failure does not affect output

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE || hOut == nullptr) return false;

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return false;

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    bool modeOk = SetConsoleMode(hOut, dwMode) != 0;
    return outOk && modeOk;
#else
    return true;
#endif
}

// =========================================================
// clearInputBuffer
//   POSIX: flush the kernel tty buffer (including partial lines)
//          and drain the iostream streambuf without blocking.
//   Windows: drain any pending keystrokes via _kbhit/_getch.
// =========================================================
inline void clearInputBuffer() {
    std::cin.clear();
#ifdef _WIN32
    while (_kbhit()) _getch();
#else
    ::tcflush(STDIN_FILENO, TCIFLUSH);

    // Drain whatever is already buffered inside iostream without
    // blocking. Using ignore(max, '\n') here could stall when the
    // buffered data contains no newline.
    auto* buf = std::cin.rdbuf();
    if (buf) {
        std::streamsize n = buf->in_avail();
        while (n > 0) {
            char tmp[256];
            std::streamsize chunk = (n < static_cast<std::streamsize>(sizeof(tmp)))
                                    ? n
                                    : static_cast<std::streamsize>(sizeof(tmp));
            buf->sgetn(tmp, chunk);
            n -= chunk;
        }
    }
#endif
}

// =========================================================
// clearScreen
//   Falls back to printing blank lines when ANSI is unavailable
//   (e.g. failed console init, redirected output) instead of
//   spraying escape sequences into the stream.
// =========================================================
inline void clearScreen() {
    if (!detail::ensureConsoleInit() || !detail::isInteractiveConsole()) {
        for (int i = 0; i < 40; ++i) std::cout << '\n';
        std::cout << std::flush;
        return;
    }
    std::cout << "\033[2J\033[3J\033[H" << std::flush;
}

// =========================================================
// waitForEnter
// =========================================================
inline bool waitForEnter(const std::string& prompt = "Press ENTER to continue...") {
    detail::ensureConsoleInit();
    std::cout << prompt << std::flush;
    std::string dummy;
    return static_cast<bool>(std::getline(std::cin, dummy));
}

// =========================================================
// countdownWait
//   Interactive console: animated countdown, ENTER skips.
//   Non-interactive (redirected / CI / ANSI unavailable):
//   silently sleeps for the requested duration.
//
//   POSIX uses TermGuard (raw mode) and poll() so a single
//   keypress is seen immediately.
// =========================================================
inline bool countdownWait(int seconds) {
    using namespace std::chrono;

    if (seconds <= 0) return false;

    const bool vtOk = detail::ensureConsoleInit();
    const bool interactive = vtOk && detail::isInteractiveConsole();

    if (!interactive) {
        // Silent non-interactive wait; no ANSI, no key polling.
        std::this_thread::sleep_for(std::chrono::seconds(seconds));
        return false;
    }

#ifndef _WIN32
    detail::TermGuard term;
#endif

    struct CursorGuard {
        ~CursorGuard() {
            // Show cursor, go to column 0, clear the countdown line, newline.
            std::cout << "\033[?25h\r\033[2K\n" << std::flush;
        }
    } guard;

    constexpr int TICK_MS = 100;
    constexpr int ANIM_MS = 400;
    constexpr std::array<const char*, 3> DOT_STRS = { ".  ", ".. ", "..." };

    auto start = steady_clock::now();
    auto end   = start + std::chrono::seconds(seconds);

    std::cout << "\033[?25l" << std::flush;   // hide cursor

#ifndef _WIN32
    bool stdinClosed = false;
#endif

    while (true) {
        auto now = steady_clock::now();
        if (now >= end) break;

        auto remainMs  = duration_cast<milliseconds>(end - now).count();
        auto elapsedMs = duration_cast<milliseconds>(now - start).count();
        size_t dotIdx  = static_cast<size_t>((elapsedMs / ANIM_MS) % DOT_STRS.size());

        std::cout << "\r" << DOT_STRS[dotIdx]
                  << " (" << (remainMs / 1000) << "."
                  << (remainMs % 1000) / 100 << "s)"
                  << " press ENTER to skip.        "
                  << std::flush;

#ifdef _WIN32
        if (_kbhit()) {
            int ch = _getch();
            if (ch == 0 || ch == 0xE0) { _getch(); continue; }  // swallow extended key
            if (ch == '\r' || ch == '\n') return true;
        }
        std::this_thread::sleep_for(milliseconds(TICK_MS));
#else
        if (!stdinClosed) {
            struct pollfd pfd{};
            pfd.fd     = STDIN_FILENO;
            pfd.events = POLLIN;
            int r = ::poll(&pfd, 1, TICK_MS);

            if (r > 0 && (pfd.revents & POLLIN)) {
                char c;
                if (!detail::readOneCharRaw(c)) {
                    // EOF on stdin: stop polling from now on and just sleep.
                    stdinClosed = true;
                }
                else if (c == '\n' || c == '\r') {
                    return true;
                }
                else {
                    // Drain any additional buffered bytes; ENTER wins.
                    struct pollfd probe{};
                    probe.fd     = STDIN_FILENO;
                    probe.events = POLLIN;
                    while (::poll(&probe, 1, 0) > 0 && (probe.revents & POLLIN)) {
                        char d;
                        if (!detail::readOneCharRaw(d)) { stdinClosed = true; break; }
                        if (d == '\n' || d == '\r') return true;
                    }
                }
            }
        }
        else {
            std::this_thread::sleep_for(milliseconds(TICK_MS));
        }
#endif
    }
    return false;
}

// =========================================================
// waitForAnyKeys
//   Windows: swallows the second byte of extended key codes.
//   POSIX:   raw mode; drains ESC sequences (arrow keys) so the
//            next input read is not polluted. Falls back to
//            "Press ENTER" when raw mode is unavailable.
// =========================================================
inline void waitForAnyKeys(const std::string& prompt = "Press any key to continue...") {
    detail::ensureConsoleInit();
    std::cout << prompt << std::flush;

#ifdef _WIN32
    int ch = _getch();
    if (ch == 0 || ch == 0xE0) _getch();   // extended key: swallow 2nd byte
#else
    detail::TermGuard guard;
    if (!guard.active) {
        std::cout << "(Press ENTER) " << std::flush;
        std::string dummy;
        std::getline(std::cin, dummy);
        std::cout << '\n' << std::flush;
        return;
    }
    char c;
    if (detail::readOneCharRaw(c)) {
        // If the key was ESC, swallow any trailing bytes of an escape
        // sequence (arrow keys arrive as ESC [ A/B/C/D). A short timeout
        // keeps a bare ESC from blocking.
        if (c == '\033') {
            struct pollfd pfd{};
            pfd.fd     = STDIN_FILENO;
            pfd.events = POLLIN;
            while (::poll(&pfd, 1, 20) > 0 && (pfd.revents & POLLIN)) {
                char d;
                if (!detail::readOneCharRaw(d)) break;
            }
        }
    }
#endif
    std::cout << '\n' << std::flush;
}

// =========================================================
// Input APIs
// =========================================================

// Low-level: read one line.
[[nodiscard]]
inline bool readLine(std::string& out, const std::string& prompt = "") {
    detail::ensureConsoleInit();
    if (!prompt.empty()) std::cout << prompt << std::flush;
    return static_cast<bool>(std::getline(std::cin, out));
}

// ---------------------------------------------------------
// promptStringOpt: distinguishes EOF (nullopt) from a legal
// empty string.
// ---------------------------------------------------------
[[nodiscard]]
inline std::optional<std::string> promptStringOpt(const std::string& prompt,
                                                  bool required = false,
                                                  bool trim     = true) {
    detail::ensureConsoleInit();
    while (true) {
        std::cout << prompt << std::flush;
        std::string line;
        if (!std::getline(std::cin, line)) return std::nullopt;   // EOF
        std::string v = trim ? detail::trimCopy(line) : line;
        if (required && v.empty()) {
            std::cout << "Input cannot be empty. Please try again.\n";
            continue;
        }
        return v;
    }
}

// ---------------------------------------------------------
// promptString: EOF maps to "" (legacy semantics).
// ---------------------------------------------------------
[[nodiscard]]
inline std::string promptString(const std::string& prompt,
                                bool required = false,
                                bool trim     = true) {
    return promptStringOpt(prompt, required, trim).value_or(std::string{});
}

// Legacy alias.
[[nodiscard]]
inline std::string promptRequired(const std::string& prompt) {
    return promptString(prompt, /*required=*/true, /*trim=*/true);
}

// ---------------------------------------------------------
// promptNumber<T>: any arithmetic type except char/bool.
//   - unsigned types reject leading '-'
//   - floating-point types reject NaN / Inf
// ---------------------------------------------------------
template <typename T>
[[nodiscard]]
std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>, T>
promptNumber(const std::string& prompt,
             T minVal = std::numeric_limits<T>::lowest(),
             T maxVal = std::numeric_limits<T>::max()) {
    static_assert(!std::is_same_v<T, char> &&
                  !std::is_same_v<T, signed char> &&
                  !std::is_same_v<T, unsigned char>,
                  "promptNumber<char> not supported; use prompt<std::string>");
    detail::ensureConsoleInit();
    while (true) {
        std::cout << prompt << std::flush;
        std::string line;
        if (!std::getline(std::cin, line)) return T{};
        auto trimmed = detail::trimView(line);
        if (trimmed.empty()) {
            std::cout << "Input cannot be empty. Please try again.\n";
            continue;
        }
        T v{};
        if (!detail::parseValue(trimmed, v)) {
            std::cout << "Invalid input. Please enter a valid number.\n";
            continue;
        }
        // NaN has already been rejected by parseValue, so < / > behave.
        if (v < minVal || v > maxVal) {
            std::cout << "Value out of range [" << minVal << ", "
                      << maxVal << "]. Please try again.\n";
            continue;
        }
        return v;
    }
}

// Legacy alias.
[[nodiscard]]
inline int promptInt(const std::string& prompt, int min, int max) {
    return promptNumber<int>(prompt, min, max);
}

// ---------------------------------------------------------
// promptBool
// ---------------------------------------------------------
[[nodiscard]]
inline bool promptBool(const std::string& prompt, bool defaultValue = false) {
    detail::ensureConsoleInit();
    while (true) {
        std::cout << prompt << std::flush;
        std::string line;
        if (!std::getline(std::cin, line)) return defaultValue;
        auto trimmed = detail::trimView(line);
        if (trimmed.empty()) return defaultValue;
        bool v = false;
        if (!detail::parseValue(trimmed, v)) {
            std::cout << "Invalid input. Please answer yes/no (y/n).\n";
            continue;
        }
        return v;
    }
}

// =========================================================
// prompt<T> sugar
// =========================================================
template <typename T>
[[nodiscard]]
std::enable_if_t<std::is_same_v<T, std::string>, T>
prompt(const std::string& msg, bool required = false) {
    return promptString(msg, required, /*trim=*/true);
}

template <typename T>
[[nodiscard]]
std::enable_if_t<std::is_same_v<T, bool>, T>
prompt(const std::string& msg, bool defaultValue = false) {
    return promptBool(msg, defaultValue);
}

template <typename T>
[[nodiscard]]
std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>, T>
prompt(const std::string& msg,
       T minVal = std::numeric_limits<T>::lowest(),
       T maxVal = std::numeric_limits<T>::max()) {
    return promptNumber<T>(msg, minVal, maxVal);
}

template <typename T>
[[nodiscard]]
std::enable_if_t<!std::is_same_v<T, std::string> && !std::is_arithmetic_v<T>, T>
prompt(const std::string& msg) {
    static_assert(std::is_default_constructible_v<T>,
                  "prompt<T> requires T to be default-constructible");
    detail::ensureConsoleInit();
    while (true) {
        std::cout << msg << std::flush;
        std::string line;
        if (!std::getline(std::cin, line)) return T{};
        auto trimmed = detail::trimView(line);
        T v{};
        if (!detail::parseValue(trimmed, v)) {
            std::cout << "Invalid input. Please try again.\n";
            continue;
        }
        return v;
    }
}

// =========================================================
// read(T&): type deduced from the variable; no <T> needed.
// =========================================================
template <typename T>
void read(T& out, const std::string& prompt = "") {
    out = nilo::console::prompt<T>(prompt);
}

// =========================================================
// promptAny: classify the user's input at runtime.
//   Disambiguation order: int -> double -> bool -> string
//   Examples: "1" -> int, "1.5" -> double, "yes" -> bool,
//             anything else -> string.
// =========================================================
struct AnyValue {
    enum class Kind { String, Int, Double, Bool };
    Kind kind = Kind::String;
    std::string s;
    long long   i = 0;
    double      d = 0.0;
    bool        b = false;

    bool isString() const { return kind == Kind::String; }
    bool isInt()    const { return kind == Kind::Int;    }
    bool isDouble() const { return kind == Kind::Double; }
    bool isBool()   const { return kind == Kind::Bool;   }

    std::string asString() const { return s; }
    long long   asInt()    const { return i; }
    double      asDouble() const { return d; }
    bool        asBool()   const { return b; }
};

[[nodiscard]]
inline AnyValue promptAny(const std::string& prompt) {
    detail::ensureConsoleInit();
    while (true) {
        std::cout << prompt << std::flush;
        std::string line;
        if (!std::getline(std::cin, line)) return {};
        auto trimmed = detail::trimView(line);
        if (trimmed.empty()) {
            std::cout << "Input cannot be empty. Please try again.\n";
            continue;
        }

        AnyValue v;
        v.s = std::string(trimmed);

        // int?
        {
            long long iv;
            std::istringstream iss{std::string(trimmed)};
            iss >> iv;
            if (!iss.fail()) {
                iss >> std::ws;
                if (iss.eof()) {
                    v.kind = AnyValue::Kind::Int;
                    v.i = iv;
                    return v;
                }
            }
        }
        // double? (reject NaN / Inf so they fall through to string)
        {
            double dv;
            std::istringstream iss{std::string(trimmed)};
            iss >> dv;
            if (!iss.fail() && std::isfinite(dv)) {
                iss >> std::ws;
                if (iss.eof()) {
                    v.kind = AnyValue::Kind::Double;
                    v.d = dv;
                    return v;
                }
            }
        }
        // bool?
        {
            bool bv;
            if (detail::parseValue(trimmed, bv)) {
                v.kind = AnyValue::Kind::Bool;
                v.b = bv;
                return v;
            }
        }
        return v;   // string
    }
}

} // namespace nilo::console

/* ============================================================
 * nilo::console v1.0.0
 *
 * MIT License
 *
 * Copyright (c) 2026 nilotoo.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall
 * be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
 * KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
 * OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * ============================================================ */