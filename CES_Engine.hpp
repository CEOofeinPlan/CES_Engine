/* @brief The Cell System only stores the character that should be outputed; Its much faster and more flexible; Every Cell is stored in a HashMap.*/
#include <unordered_map>
#include <iostream>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <type_traits>
#include <functional>
#include <stack>
#include <string>

#if defined(_WIN32)
    #include <windows.h>
#endif

#if defined(__linux__) || defined(__APPLE__)
    #include <unistd.h>
    #include <termios.h>
    #include <fcntl.h>
#endif

namespace CES {
    // Everything is based on those three structures; Every type of systems you are choosing is based on it.
    struct CES_COLOR {
        uint8_t r = 0; // red
        uint8_t g = 0; // green
        uint8_t b = 0; // blue
        uint8_t a = 0; // alpha

        CES_COLOR(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0, uint8_t alpha = 255) {
            r = red;
            g = green;
            b = blue;
            a = alpha;
        }

        explicit CES_COLOR(uint32_t color) {
            r = (color >> 24) & 0xFF;   // Extracting red
            g = (color >> 16) & 0xFF;   // Extracting green
            b = (color >> 8) & 0xFF;    // Extracting blue
            a = color & 0xFF;           // Extracting alpha
        }

        /* @brief With Alpha */
        void Convert_HEX_32(uint32_t color) {
            r = (color >> 24) & 0xFF;   // Extracting red
            g = (color >> 16) & 0xFF;   // Extracting green
            b = (color >> 8) & 0xFF;    // Extracting blue
            a = color & 0xFF;           // Extracting alpha
        }

        /* @brief Without Alpha */
        void Convert_HEX_24(uint32_t color) {
            if (color > 0xFFFFFF) color = color & 0xFFFFFF; // Getting the Alpha out
            r = (color >> 16) & 0xFF;   // Extracting red
            g = (color >> 8) & 0xFF;    // Extracting green
            b = color & 0xFF;           // Extracting blue
        }
    };

    struct CES_XY {
        uint16_t x; // max. 65K Character
        uint16_t y; // max. 65K Character
        
        CES_COLOR ARGB; // Alpha-Red-Green-Blue

        CES_XY() = default;
        CES_XY(const CES_XY&) = default;
        CES_XY(CES_XY&&) noexcept = default;
        CES_XY& operator=(const CES_XY&) = default;
        CES_XY& operator=(CES_XY&&) noexcept = default;

        bool operator==(const CES_XY& other) const noexcept {
            return x == other.x && y == other.y;
        }
    };
}

namespace std {

    template <>
    struct hash<CES::CES_XY> {
        std::size_t operator()(const CES::CES_XY& p) const noexcept {
            // Use size_t to avoid narrowing issues
            return (static_cast<std::size_t>(p.x) << 16) ^ static_cast<std::size_t>(p.y);
        }
    };
}

namespace CES {
    using namespace std;

    struct TerminalCapabilities {
        int supportsColor = 1;
        bool supportsRGB = false;
        bool supportsCursor = false;
        bool supportsMouse = false;
    };

    #ifdef CES_CELL_SYSTEM
        class CES_Screen {
            public:
                // Finding the Terminal:
                // TTY =>
                    // Color:           16 colors
                    // Cursor-Control:  True
                    // RGB:             False
                    // Transparency:    False
                    // Mouse:           False
                // Emulator =>
                    // Color:           8-24 bits (needs to be indentified later on)
                    // Cursor-Conrol:   True
                    // RGB:             True
                    // Transparency:    Not all (Kitty, Alacritty does have it, needs to be indentified later on)
                    // Mouse:           True
                // Multiplexer =>
                    // Color:           Depends on the host (which terminal is it based on like: xterm_256color)
                    // Cursor-Control:  True
                    // RGB:             True
                    // Transparency:    False
                    // Mouse:           True
                // Old Windows =>
                    // Color:           16 colors
                    // Cursor-Control:  False
                    // RGB:             False
                    // Transparency:    False
                    // Mouse:           False
                // New Windows =>
                    // Color:           24 bits
                    // Cursor-Control:  True
                    // RGB:             True
                    // Transparency:    False
                    // Mouse:           True (Windows Terminal)

                TerminalCapabilities supported;
                
                CES_Screen() 
                {
                    /* Defining the terminal capabilities */

                    // If ANSI is set, then this is supported: Cursor, Mouse, RGB
                    
                    // Windows:
                    // First decide if the current windows is older than windows 10
                    // ! --> If you compile your project make sure it is compiled "static"; People who wants to use your project or play your game needs the file: "windows.h"; If the "static" attribute, in your compile command is set, then no issue won't appear in this context.

                    #if defined(_WIN32)
                    #define WIN32_LEAN_AND_MEAN
                        bool initialized = true;
                        HKEY hKey;
                        DWORD major = 0, minor = 0, build = 0;
                        DWORD size = sizeof(DWORD);

                        LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                                                    L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                                                    0, KEY_READ, &hKey);

                        if (result != ERROR_SUCCESS) {
                            std::cerr << "Konnte Registry nicht öffnen!\n";
                            initialized = false;
                        } else {
                            // Major-Version (windows 10+)
                            result = RegGetValueW(hKey, nullptr, L"CurrentMajorVersionNumber", RRF_RT_REG_DWORD, nullptr, &major, &size);
                            if (result != ERROR_SUCCESS) {
                                // Older Version of windows 10
                                wchar_t versionStr[16];
                                DWORD strSize = sizeof(versionStr);
                                result = RegGetValueW(hKey, nullptr, L"CurrentVersion", RRF_RT_REG_SZ, nullptr, &versionStr, &strSize);
                                if (result != ERROR_SUCCESS) {
                                    std::cerr << "Konnte Versions-Info nicht auslesen!\n";
                                    initialized = false;
                                } else {
                                    swscanf_s(versionStr, L"%lu.%lu", &major, &minor);
                                }
                            } else {
                                // Minor-Version for windows 10+
                                size = sizeof(DWORD);
                                RegGetValueW(hKey, nullptr, L"CurrentMinorVersionNumber", RRF_RT_REG_DWORD, nullptr, &minor, &size);
                            }

                            RegCloseKey(hKey);
                        }

                        // Build-Number
                        wchar_t buildStr[16];
                        DWORD buildSize = sizeof(buildStr);
                        result = RegGetValueW(
                            HKEY_LOCAL_MACHINE,
                            L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                            L"CurrentBuildNumber",
                            RRF_RT_REG_SZ,
                            nullptr,
                            &buildStr,
                            &buildSize
                        );
                        if (result == ERROR_SUCCESS) {
                            build = _wtoi(buildStr);
                        }

                        RegCloseKey(hKey);

                        // --- Feature-Set after windows-version ---
                        if (major < 10) {
                            // Any Windows older than 10
                            supported.supportsColor = 16;
                            supported.supportsRGB = false;
                            supported.supportsMouse = false;
                            supported.supportsCursor = false;
                        } else if (major == 10) {
                            if (build >= 22000) {
                                // Windows 11 (Windows 10 Kernel Version >= 22000)
                                supported.supportsColor = 0xFFFFFF;
                                supported.supportsRGB = true;
                                supported.supportsMouse = true;
                                supported.supportsCursor = true;
                            } else {
                                // Windows 10
                                supported.supportsColor = 0xFFFFFF;
                                supported.supportsRGB = true;
                                supported.supportsMouse = false;
                                supported.supportsCursor = true;
                            }
                        } else if (major > 10) {
                            // Zukunft: Any newer windows than windows 11
                            supported.supportsColor = 0xFFFFFF;
                            supported.supportsRGB = true;
                            supported.supportsMouse = true;
                            supported.supportsCursor = true;
                        }
                        
                    #elif defined(__linux__) || defined(__APPLE__)
                        // Cursor
                        // Basically every terminal on linux has it
                        supported.supportsCursor = true;

                        // Color and RGB
                        const char* term = getenv("TERM");
                        const char* colorterm = getenv("COLORTERM");
                        if (colorterm && string(colorterm) == "truecolor") { supported.supportsColor = 0xFFFFFF; supported.supportsRGB = true; }
                        if (string(term).find("256color") != string::npos) supported.supportsColor = 256;
                        if (term) supported.supportsColor = 16;

                        // Mouse support
                        termios oldt, newt;
                        tcgetattr(STDIN_FILENO, &oldt);
                        newt = oldt;
                        newt.c_lflag &= ~(ICANON | ECHO);
                        tcsetattr(STDIN_FILENO, TCSANOW, &newt);

                        // Non-blocking stdin
                        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
                        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

                        // Xterm Mouse Mode activation (normal + button events)
                        std::cout << "\033[?1000h"; // Mouse click tracking
                        std::cout << "\033[?1002h"; // Mouse drag tracking
                        std::cout.flush();

                        char buf[32] = {0};
                        ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));

                        // Terminal reset
                        std::cout << "\033[?1000l\033[?1002l";
                        std::cout.flush();
                        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
                        fcntl(STDIN_FILENO, F_SETFL, flags);

                        if (n > 0) supported.supportsMouse = true;
                    #endif
                }


                void WriteCell(char c, unsigned short x, unsigned short y, uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0, bool output_directly = true, bool output_with_assigned_color = true) {
                    CES_XY p1;
                    p1.x = x;
                    p1.y = y;
                    p1.ARGB.r = red;
                    p1.ARGB.g = green;
                    p1.ARGB.b = blue;
                    Cell_Storages.insert_or_assign(p1, c);
                    if (output_directly) {
                        cout << "\033[" << y << ";" << x << "H";                                                // Move cursor to the new position
                        cout    << "\033[38;2;"                         /**/
                                << static_cast<int>(p1.ARGB.r) << ";"   //
                                << static_cast<int>(p1.ARGB.g) << ";"   // Place the new character
                                << static_cast<int>(p1.ARGB.b) << "m"   //
                                << c << "\033[0m";                      /**/
                        cout << "\033[?25l";                            // Hide cursor
                    } else {
                        cout << "Work in Progress!" << endl;
                    } 
                }

                void ClearConsole() {
                    cout << "\033[2J";      // Delete the screen
                    cout << "\033[?25l";    // Hide cursor
                }

                unordered_map<CES_XY, char> Cell_Storages;

            private:
                // For not having to output it directly // The bool is for knowing if it is outputed
                stack<pair<unordered_map<CES_XY, char>, bool>> last;
        };

    #endif

    #ifdef CES_GRID_SYSTEM

    #endif

    #ifdef CES_COLOR_UNIT
        // Its not necessary during an easy project
        // Its more usefull if you need to calculate a color or if u want a 16 color pallet
        // Every terminal will calculate a truecolor to their pallet, so this UNIT is NOT directly necessary 

        /* ANSI 16-color terminal defines */

        // 16 colors + if they appear in an 8-color terminal or not.
        /** @def CES_16_BLACK
         *  @brief Black (appears also in 3-bit terminals)
         */
        #define CES_16_BLACK            0x000000

        /** @def CES_16_RED
         *  @brief Red (appears also in 3-bit terminals)
         */
        #define CES_16_RED              0x800000

        /** @def CES_16_GREEN
         *  @brief Green (appears also in 3-bit terminals)
         */
        #define CES_16_GREEN            0x008000

        /** @def CES_16_YELLOW
         *  @brief Yellow (appears also in 3-bit terminals)
         */
        #define CES_16_YELLOW           0x808000

        /** @def CES_16_BLUE
         *  @brief Blue (appears also in 3-bit terminals)
         */
        #define CES_16_BLUE             0x000080

        /** @def CES_16_MAGENTA
         *  @brief Magenta (appears also in 3-bit terminals)
         */
        #define CES_16_MAGENTA          0x800080

        /** @def CES_16_CYAN
         *  @brief Cyan (appears also in 3-bit terminals)
         */
        #define CES_16_CYAN             0x008080

        /** @def CES_16_LIGHT_GRAY
         *  @brief White / Light Gray (appears also in 3-bit terminals)
         */
        #define CES_16_LIGHT_GRAY       0xC0C0C0    // It is kinda like white

        /** @def CES_16_GRAY
         *  @brief Bright Black (Gray)
         */
        #define CES_16_GRAY             0x808080

        /** @def CES_16_BRIGHT_RED
         *  @brief Bright Red
         */
        #define CES_16_BRIGHT_RED       0xFF0000

        /** @def CES_16_BRIGHT_GREEN
         *  @brief Bright Green
         */
        #define CES_16_BRIGHT_GREEN     0x00FF00

        /** @def CES_16_BRIGHT_YELLOW
         *  @brief Bright Yellow
         */
        #define CES_16_BRIGHT_YELLOW    0xFFFF00

        /** @def CES_16_BRIGHT_BLUE
         *  @brief Bright Blue
         */
        #define CES_16_BRIGHT_BLUE      0x0000FF

        /** @def CES_16_BRIGHT_MAGENTA
         *  @brief Bright Magenta
         */
        #define CES_16_BRIGHT_MAGENTA   0xFF00FF

        /** @def CES_16_BRIGHT_CYAN
         *  @brief Bright Cyan
         */
        #define CES_16_BRIGHT_CYAN      0x00FFFF

        /** @def CES_16_WHITE
         *  @brief White
         */
        #define CES_16_WHITE 0xFFFFFF

        // It's used to compare color values easier, only AND
        #define CES_CC_A(r,g,b,v1,v2,v3) ((r) == (v1) && (g) == (v2) && (b) == (v3))

        // It's used to compare color values easier, only OR
        #define CES_CC_O(r,g,b,v1,v2,v3) ((r) == (v1) || (g) == (v2) || (b) == (v3))

        // It's used to compare color values easier + alpha, only AND
        #define CES_ACC_A(r,g,b,a,v1,v2,v3,v4) ((r) == (v1) && (g) == (v2) && (b) == (v3) && (a) == (v4)) 

        // It's used to compare color values easier + alpha, only OR
        #define CES_ACC_O(r,g,b,a,v1,v2,v3,v4) ((r) == (v1) || (g) == (v2) || (b) == (v3) || (a) == (v4)) 

        // Rounding the input colors down or up to let it fit into a 16-color or 8-color terminal
        void CES_ROUND_COLORS(CES_COLOR* color, bool form = true) { // True = 16-color, False = 8 color
            // Areas: 0 < 128 < 192 < 255
            // After division with 64 it is: 0, 2, 3, 4

            short red = round(color->r / 64);
            short green = round(color->g / 64);
            short blue = round(color->b / 64);

            // 64 isn't available for those terminals

            if (red == 1) red++;
            if (green == 1) green++;
            if (blue == 1) blue++;

            // First if is RED, second is GREEN, third is BLUE
            // It's not the best solution till now, but it makes it easier to read
            // 8 color terminal
                 if (CES_CC_A(red, green, blue, 0,0,0))   color->Convert_HEX_24(CES_16_BLACK);
            else if (CES_CC_A(red, green, blue, 2,0,0))   color->Convert_HEX_24(CES_16_RED);
            else if (CES_CC_A(red, green, blue, 0,2,0))   color->Convert_HEX_24(CES_16_GREEN);
            else if (CES_CC_A(red, green, blue, 2,2,0))   color->Convert_HEX_24(CES_16_YELLOW);
            else if (CES_CC_A(red, green, blue, 0,0,2))   color->Convert_HEX_24(CES_16_BLUE);
            else if (CES_CC_A(red, green, blue, 2,0,2))   color->Convert_HEX_24(CES_16_MAGENTA);
            else if (CES_CC_A(red, green, blue, 0,2,2))   color->Convert_HEX_24(CES_16_CYAN);
            else if (CES_CC_A(red, green, blue, 3,3,3))   color->Convert_HEX_24(CES_16_LIGHT_GRAY);
            // 16 color terminal
            else if (CES_CC_A(red, green, blue, 2,2,2))   color->Convert_HEX_24(CES_16_GRAY);
            else if (CES_CC_A(red, green, blue, 4,0,0))   color->Convert_HEX_24(CES_16_BRIGHT_RED);
            else if (CES_CC_A(red, green, blue, 0,4,0))   color->Convert_HEX_24(CES_16_BRIGHT_GREEN);
            else if (CES_CC_A(red, green, blue, 4,4,0))   color->Convert_HEX_24(CES_16_BRIGHT_YELLOW);
            else if (CES_CC_A(red, green, blue, 0,0,4))   color->Convert_HEX_24(CES_16_BRIGHT_BLUE);
            else if (CES_CC_A(red, green, blue, 4,0,4))   color->Convert_HEX_24(CES_16_BRIGHT_MAGENTA);
            else if (CES_CC_A(red, green, blue, 0,4,4))   color->Convert_HEX_24(CES_16_BRIGHT_CYAN);
            else if (CES_CC_A(red, green, blue, 4,4,4))   color->Convert_HEX_24(CES_16_WHITE);

            return;
        }
    #endif
}

#ifdef CES_AUDIO_UNIT
    #include <cstdio>
    #include <fstream>
    #include <vector>
    #include <cstdint>
    #include <algorithm>
    #include <thread>
    #include <map>
    #include <set>
    #include <tuple>

    #define MINIAUDIO_IMPLENTATION
    // miniaudio.h is NOT my file
    // miniaudio.h is owned by David Reid 
    // miniaudio.h Repository -> https://github.com/mackron/miniaudio
    #include "miniaudio.h"

    #pragma comment(lib, "winmm.lib")

    using namespace std;
        // MIDI Numbers
        enum CES_NOTE {
            C_1 = 0,  C_1R = 1,  DB_1 = 1,  D_1 = 2,  D_1R = 3,  EB_1 = 3,  E_1 = 4,
            F_1 = 5,  F_1R = 6,  GB_1 = 6,  G_1 = 7,  G_1R = 8,  AB_1 = 8,  A_1 = 9,
            A_1R = 10, BB_1 = 10, B_1 = 11,

            C0 = 12, C0R = 13, DB0 = 13, D0 = 14, D0R = 15, EB0 = 15, E0 = 16,
            F0 = 17, F0R = 18, GB0 = 18, G0 = 19, G0R = 20, AB0 = 20, A0 = 21,
            A0R = 22, BB0 = 22, B0 = 23,

            C1 = 24, C1R = 25, DB1 = 25, D1 = 26, D1R = 27, EB1 = 27, E1 = 28,
            F1 = 29, F1R = 30, GB1 = 30, G1 = 31, G1R = 32, AB1 = 32, A1 = 33,
            A1R = 34, BB1 = 34, B1 = 35,

            C2 = 36, C2R = 37, DB2 = 37, D2 = 38, D2R = 39, EB2 = 39, E2 = 40,
            F2 = 41, F2R = 42, GB2 = 42, G2 = 43, G2R = 44, AB2 = 44, A2 = 45,
            A2R = 46, BB2 = 46, B2 = 47,

            C3 = 48, C3R = 49, DB3 = 49, D3 = 50, D3R = 51, EB3 = 51, E3 = 52,
            F3 = 53, F3R = 54, GB3 = 54, G3 = 55, G3R = 56, AB3 = 56, A3 = 57,
            A3R = 58, BB3 = 58, B3 = 59,

            C4 = 60, C4R = 61, DB4 = 61, D4 = 62, D4R = 63, EB4 = 63, E4 = 64,
            F4 = 65, F4R = 66, GB4 = 66, G4 = 67, G4R = 68, AB4 = 68, A4 = 69,
            A4R = 70, BB4 = 70, B4 = 71,

            C5 = 72, C5R = 73, DB5 = 73, D5 = 74, D5R = 75, EB5 = 75, E5 = 76,
            F5 = 77, F5R = 78, GB5 = 78, G5 = 79, G5R = 80, AB5 = 80, A5 = 81,
            A5R = 82, BB5 = 82, B5 = 83,

            C6 = 84, C6R = 85, DB6 = 85, D6 = 86, D6R = 87, EB6 = 87, E6 = 88,
            F6 = 89, F6R = 90, GB6 = 90, G6 = 91, G6R = 92, AB6 = 92, A6 = 93,
            A6R = 94, BB6 = 94, B6 = 95,

            C7 = 96, C7R = 97, DB7 = 97, D7 = 98, D7R = 99, EB7 = 99, E7 = 100,
            F7 = 101, F7R = 102, GB7 = 102, G7 = 103, G7R = 104, AB7 = 104, A7 = 105,
            A7R = 106, BB7 = 106, B7 = 107,

            C8 = 108, C8R = 109, DB8 = 109, D8 = 110, D8R = 111, EB8 = 111, E8 = 112,
            F8 = 113, F8R = 114, GB8 = 114, G8 = 115, G8R = 116, AB8 = 116, A8 = 117,
            A8R = 118, BB8 = 118, B8 = 119,

            C9 = 120, C9R = 121, DB9 = 121, D9 = 122, D9R = 123, EB9 = 123, E9 = 124,
            F9 = 125, F9R = 126, GB9 = 126, G9 = 127
        };

    class CES_AUDIO {
        public:

            // Normal sound input through a .wav file

            int initSound(string path, bool async = true, bool loop = false) {
                if (!initialized) {
                    if (ma_engine_init(NULL, &engine) != MA_SUCCESS) return -1;
                    initialized = true;
                }

                auto sound = new ma_sound;
                if (ma_sound_init_from_file(&engine, path.c_str(), 0, NULL, NULL, sound) != MA_SUCCESS) {
                    delete sound;
                    return -2;
                }

                holding_sound[path] = sound;

                ma_sound_start(sound);

                if (loop) ma_sound_set_looping(sound, MA_TRUE);

                // Async oder sync
                if (!loop && !async) {
                    while (ma_sound_is_playing(sound)) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    }
                    ma_sound_uninit(sound);
                    delete sound;
                    holding_sound.erase(path);
                } else if (!loop) {
                    // async non-loop: Thread cleanup
                    std::thread([sound, this, path](){
                        while (ma_sound_is_playing(sound)) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(50));
                        }
                        ma_sound_uninit(sound);
                        delete sound;
                        holding_sound.erase(path);
                    }).detach();
                }

                return 0;
            }

            int uninitSound(string path) {
                ma_sound_uninit(holding_sound[path]);
                free(holding_sound[path]);
                return 0;
            }

            void PauseResumeSound(string path) {
                if (ma_sound_is_playing(holding_sound[path])) ma_sound_stop(holding_sound[path]);
                else ma_sound_start(holding_sound[path]);
            }

            inline ma_sound* getSoundPTR(string path) { return holding_sound[path]; }

            // Sound input through your own sheet of music

            class CES_Componist {
                public:
                    struct Note {
                        double frequency; // (Hz)
                        double duration;  // (s)
                        double amplitude; // 0.0 -> 1.0
                    };

                    double convertFrequency(int n) { return 440.0 * pow(2.0, (n-69.0) / 12.0); }

                    // f = 100% Klang
                    // p = 075% Klang
                    // . = 050% Klang
                    // - = 025% Klang

                    enum CES_INSTRUMENTAL_TYPE {
                        piano
                    };

                    int convertSound(vector<pair<CES_NOTE, string>>& music, CES_INSTRUMENTAL_TYPE options, string title, bool replace = false) {
                        title += ".wav";
                        if (!filesystem::exists(title) || replace) {
                            switch (options) {
                                case CES_INSTRUMENTAL_TYPE::piano:
                                    // unordered_map<idx, vector<pair<MIDI, Sound>>>
                                    unordered_map<int, vector<pair<int, char>>> componistList;
                                    for (auto& i : music) {
                                        // Valid
                                        cout << i.first << endl;
                                        if (i.first > -1 && i.first < 127) {
                                            int MIDI = i.first;
                                            for (int j = 0; j < i.second.length(); ++j) {
                                                pair<int, char> tmp = {MIDI, i.second[j]};
                                                componistList[j].push_back(tmp);
                                            }
                                        } else continue;
                                    }
                                    cout << componistList.empty() << endl;
                                    auto samples = buildSound(componistList, CES_INSTRUMENTAL_TYPE::piano);
                                    samples = applyFade(samples);
                                    saveWAV(title, samples);
                                break;

                            };
                        }

                        return 0;
                    }
                private:
                    unordered_map<int, vector<uint16_t>> tnMap;
                    const int SAMPLE_RATE = 44100;
                    const double PI = 3.14159265358979323846;

                    void applyPianoADSR(vector<int16_t>& s) {
                        int n = s.size();
                        int attack = 0.005 * SAMPLE_RATE;
                        int decay  = 0.15  * SAMPLE_RATE;
                        for (int i = 0; i < n; ++i) {
                            double amp = 1.0;
                            if (i < attack) amp = i / (double)attack;
                            else if (i < decay) {
                                amp = 1.0 - 0.85 * ((i-attack) / (double)(decay-attack));
                            }

                            s[i] = static_cast<int16_t>(s[i]*amp);
                        }
                    }

                    void applyPianoEnvelope(vector<int16_t>& s)
                    {
                        int N = s.size();

                        int attack = SAMPLE_RATE * 0.004; // 4ms
                        int decay  = SAMPLE_RATE * 0.35;  // 350ms

                        for (int i = 0; i < N; i++)
                        {
                            double env;

                            if (i < attack)
                                env = (double)i / attack;  // Linear Attack
                            else if (i < decay)
                            {
                                double x = (i - attack) / (double)(decay - attack);
                                env = 1.0 * pow(1.0 - x, 1.8); 
                            }
                            else
                                env = 0.0; 

                            s[i] = (int16_t)(s[i] * env); 
                        }
                    }

                    vector<int16_t> generateWarmPiano(double freq, double duration = 0.25)
                    {
                        int N = SAMPLE_RATE * duration;
                        vector<int16_t> out(N);

                        double f = freq;
                        double amp = 0.6;

                        for (int i = 0; i < N; i++)
                        {
                            double t = i / (double)SAMPLE_RATE;

                            double tri = 2.0 * fabs(2.0 * (t*f - floor(t*f + 0.5))) - 1.0;

                            double sq = (fmod(t*f, 1.0) < 0.5 ? 1.0 : -1.0);

                            double value = 
                                tri * amp +        
                                sq * 0.15;         

                            value = max(-1.0, min(1.0, value));

                            out[i] = (int16_t)(value * 32767.0);
                        }

                        return out;
                    }

                    vector<int16_t> applyFade(const vector<int16_t>& samples, double fadeDuration = 0.01) {
                        int fadeSample = static_cast<int>(samples.size() * fadeDuration);
                        vector<int16_t> out = samples;
                        for (int i = 0; i < fadeSample; ++i) {
                            double factor = i / static_cast<double>(fadeSample);
                            out[i] = static_cast<int16_t>(out[i] * factor);
                            out[samples.size()-i-1] = static_cast<int16_t>(out[samples.size()-i-1] * factor);
                        }
                        return out;
                    }

                    void saveWAV(string& title, const vector<int16_t>& samples) {
                        ofstream file(title, ios::binary);
                        file.write("RIFF", 4);
                        uint32_t fileSize = 36 + samples.size() * sizeof(int16_t);
                        file.write(reinterpret_cast<char*>(&fileSize), 4);
                        file.write("WAVE", 4);

                        file.write("fmt ", 4);
                        uint32_t subChunk1Size = 16;
                        file.write(reinterpret_cast<char*>(&subChunk1Size), 4);
                        uint16_t audioFormat = 1;
                        file.write(reinterpret_cast<char*>(&audioFormat), 2);
                        uint16_t numChannels = 1;
                        file.write(reinterpret_cast<char*>(&numChannels), 2);
                        uint32_t sampleRate = SAMPLE_RATE;
                        file.write(reinterpret_cast<char*>(&sampleRate), 4);
                        uint32_t byteRate = SAMPLE_RATE * numChannels * sizeof(int16_t);
                        file.write(reinterpret_cast<char*>(&byteRate), 4);
                        uint16_t blockAlign = numChannels * sizeof(int16_t);
                        file.write(reinterpret_cast<char*>(&blockAlign), 2);
                        uint16_t bitsPerSample = 16;
                        file.write(reinterpret_cast<char*>(&bitsPerSample), 2);

                        file.write("data", 4);
                        uint32_t subChunk2Size = samples.size() * sizeof(int16_t);
                        file.write(reinterpret_cast<char*>(&subChunk2Size), 4);
                        file.write(reinterpret_cast<const char*>(samples.data()), subChunk2Size);
                    }

                    vector<int16_t> buildSound(const unordered_map<int, vector<pair<int, char>>>& song,
                           CES_INSTRUMENTAL_TYPE type, double base = 0.25)
                    {
                        vector<int16_t> final;

                        for (const auto& [index, notes] : song) {
                            vector<vector<int16_t>> layer;

                            for (const auto& [midi, sym] : notes) {
                                double freq = convertFrequency(midi);
                                double dur = base;

                                switch (sym) {
                                    case '.': dur *= 0.5; break;
                                    case '-': dur *= 2.0; break;
                                    case 'p': dur = 0.5; break;
                                    case 'f': dur = 1.2; break;
                                    default: break;
                                }

                                vector<int16_t> samples;

                                switch (type) {
                                    case CES_INSTRUMENTAL_TYPE::piano:
                                        samples = generateWarmPiano(freq, dur);
                                        applyPianoEnvelope(samples);

                                        for (auto& s : samples)
                                            s = static_cast<int16_t>(s * 0.6f);

                                        break;

                                    default:
                                        break;
                                }

                                layer.push_back(move(samples));
                            }

                            size_t len = 0;
                            for (auto& v : layer)
                                len = max(len, v.size());

                            vector<float> mixFloat(len, 0.0f);

                            for (auto& v : layer) {
                                for (size_t i = 0; i < v.size(); ++i) {
                                    mixFloat[i] += v[i];
                                }
                            }

                            float peak = 0.0f;
                            for (size_t i = 0; i < len; ++i)
                                peak = max(peak, fabs(mixFloat[i]));

                            if (peak < 1.0f) peak = 1.0f;

                            float scale = 32767.0f / peak;

                            vector<int16_t> mix(len);
                            for (size_t i = 0; i < len; ++i) {
                                mix[i] = static_cast<int16_t>(mixFloat[i] * scale);
                            }

                            vector<int16_t> silence(SAMPLE_RATE / 100, 0);
                            final.insert(final.end(), silence.begin(), silence.end());
                            final.insert(final.end(), mix.begin(), mix.end());
                        }

                        return final;
                    }
            };
            
            private:
                ma_engine engine;
                bool initialized = false;
                unordered_map<string, ma_sound*> holding_sound;
    };

#endif
