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
    #include <unordered_set>

    #if defined(_WIN32)
        #include <windows.h>
    #endif

    #if defined(__linux__) || defined(__APPLE__)
        #include <unistd.h>
        #include <termios.h>
        #include <fcntl.h>
    #endif

using namespace std;

class CES {
    public:
        CES() = default;

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

            bool operator==(const CES_COLOR& c) const noexcept {
                if (c.a == a && c.r == r && c.b == b && c.g == g) return true;
                return false;
            };
        };

        struct ColorHash {
            size_t operator()(const CES_COLOR& c) const noexcept {
                uint32_t packed =
                    (c.r << 24) |
                    (c.g << 16) |
                    (c.b << 8 ) |
                    (c.a);
                return hash<uint32_t>()(packed);
            }
        };

        struct ColorEq {
            bool operator()(const CES_COLOR& a, const CES_COLOR& b) const noexcept {
                return a.r==b.r && a.g==b.g && a.b==b.b && a.a==b.a;
            }
        };

        struct CES_XY {
            uint16_t x; // max. 65K Character
            uint16_t y; // max. 65K Character
            
            CES_COLOR ARGB; // Alpha-Red-Green-Blue

            char32_t c; // UniCode Character

            CES_XY() = default;
            CES_XY(const CES_XY&) = default;
            CES_XY(CES_XY&&) noexcept = default;
            CES_XY& operator=(const CES_XY&) = default;
            CES_XY& operator=(CES_XY&&) noexcept = default;

            bool operator==(const CES_XY& other) const noexcept {
                return x == other.x
                    && y == other.y
                    && ARGB == other.ARGB
                    && c == other.c;
            }

            struct CES_XY_Hash {
                size_t operator()(const CES_XY& xy) const noexcept {
                    size_t h1 = hash<uint16_t>()(xy.x);
                    size_t h2 = hash<uint16_t>()(xy.y);

                    uint32_t packed =
                        (xy.ARGB.r << 24) |
                        (xy.ARGB.g << 16) |
                        (xy.ARGB.b <<  8) |
                        (xy.ARGB.a);

                    size_t h3 = hash<uint32_t>()(packed);
                    size_t h4 = hash<char32_t>()(xy.c);

                    return ((h1 ^ (h2 << 1)) ^ (h3 << 1)) ^ (h4 << 1);
                }
            };


            string convertCHAR32toCHAR(char32_t c) const {
                string utf8;
                if (c <= 0x7F) {
                    utf8 += static_cast<char>(c);
                } else if (c <= 0x7FF) {
                    utf8 += static_cast<char>(0xC0 | ((c >> 6) & 0x1F));
                    utf8 += static_cast<char>(0x80 | (c & 0x3F));
                } else if (c <= 0xFFFF) {
                    utf8 += static_cast<char>(0xE0 | ((c >> 12) & 0x0F));
                    utf8 += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
                    utf8 += static_cast<char>(0x80 | (c & 0x3F));
                } else {
                    utf8 += static_cast<char>(0xF0 | ((c >> 18) & 0x07));
                    utf8 += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
                    utf8 += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
                    utf8 += static_cast<char>(0x80 | (c & 0x3F));
                }

                return utf8;
            }
        };

        template<typename HashA, typename EqA, typename HashB, typename EqB>
        static bool sets_equal(const unordered_set<CES_XY, HashA, EqA>& a, const unordered_set<CES_XY, HashB, EqB>& b)
        {
            if (a.size() != b.size()) return false;

            for (const auto& e : a) {
                if (b.find(e) == b.end()) return false;
            }

            return true;
        }

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
                                cerr << "Konnte Registry nicht öffnen!\n";
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
                                        cerr << "Konnte Versions-Info nicht auslesen!\n";
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
                            cout << "\033[?1000h"; // Mouse click tracking
                            cout << "\033[?1002h"; // Mouse drag tracking
                            cout.flush();

                            char buf[32] = {0};
                            ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));

                            // Terminal reset
                            cout << "\033[?1000l\033[?1002l";
                            cout.flush();
                            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
                            fcntl(STDIN_FILENO, F_SETFL, flags);

                            if (n > 0) supported.supportsMouse = true;
                        #endif

                        // Bringing the terminal in a constant state of no scrolling
                        cout << "\033[?1049h";
                    }

                    ~CES_Screen() {
                        // Moving out from this specific terminal setting
                        cout << "\033[?1049l";
                    }

                    // Only assigning
                    void WriteCell(char32_t c, uint16_t x, uint16_t y, CES_COLOR color) {
                        CES_XY p1;
                        p1.x = x;
                        p1.y = y;
                        p1.ARGB = color;
                        p1.c = c;
                        Cell_Storages[{p1.x, p1.y}] = p1;
                        Cell_Changed[{p1.x, p1.y}] = true;
                    }

                    // Only assigning
                    void pack_load_geometry(vector<CES_XY>* xy) {
                        for (auto& i : *xy) {
                            if (Cell_Storages[{i.x, i.y}] == i) continue;
                            Cell_Storages[{i.x, i.y}] = i;
                            Cell_Changed[{i.x, i.y}] = true;
                        }
                    }

                    // Alpha is left.
                    void OutputCurWindow() {
                        for (auto& i : Cell_Changed) { // Big O(N)
                            // Dirty-Region-Rendering
                            if (i.second) {
                                // Big O(logN)
                                // first.second = y; first.first = x;
                                CES_XY a = Cell_Storages[{i.first.first, i.first.second}];
                                cout << "\033[" << a.y << ";" << a.x << "H";   // Move cursor to the new position
                                cout    << "\033[38;2;"                        
                                        << static_cast<int>(a.ARGB.r) << ";"   //
                                        << static_cast<int>(a.ARGB.g) << ";"   // Place the new character
                                        << static_cast<int>(a.ARGB.b) << "m"   //
                                        << a.convertCHAR32toCHAR(a.c) << "\033[0m";
                                cout << "\033[?25l";                           // Hide cursor
                            }
                        }
                        //
                        // => O(N) (overlapped) O(logN) ==> O(N (overlapped) logN) ===> O(N) <-- Better than asymptotic is not possible currently.
                        //
                    }

                    void ClearConsole() {
                        cout << "\033[2J";      // Delete the screen
                        cout << "\033[?25l";    // Hide cursor
                    }
                    
                    struct PairHash {
                        template <class T1, class T2>
                        size_t operator()(const pair<T1, T2>& p) const noexcept {
                            return hash<T1>()(p.first) ^ (hash<T2>()(p.second) << 1);
                        }
                    };

                    // Sparke Rendering
                    unordered_map<pair<uint16_t, uint16_t>, CES_XY, PairHash> Cell_Storages;
                    // Dirty-Region-Rendiner
                    unordered_map<pair<uint16_t, uint16_t>, bool, PairHash> Cell_Changed;
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

        #ifdef CES_AUDIO_UNIT
            #include <string>
            #include <unordered_map>
            #include <thread>
            #include <chrono>
            #include <mutex>

            #define MINIAUDIO_IMPLEMENTATION
            #include "miniaudio.h"

            #pragma comment(lib, "winmm.lib")

            class CES_AUDIO {
                public:
                    CES_AUDIO() = default;

                    ~CES_AUDIO() {
                        if (initialized) {
                            for (auto& kv : holding_sound) {
                                if (kv.second) {
                                    ma_sound_uninit(kv.second);
                                    delete kv.second;
                                }
                            }

                            holding_sound.clear();
                            ma_engine_uninit(&engine);
                            initialized = false;
                        }
                    }

                    int initSound(const string& path, bool async = true, bool loop = false) {
                        if (!initialized) {
                            ma_engine_config cfg = ma_engine_config_init();
                            if (ma_engine_init(&cfg, &engine) != MA_SUCCESS) return -1;
                            initialized = true;
                        }

                        auto sound = new ma_sound;
                        if (ma_sound_init_from_file(&engine, path.c_str(), 0, nullptr, nullptr, sound) != MA_SUCCESS) {
                            delete sound;
                            return -2;
                        }

                        if (holding_sound[path] != NULL) return -3;

                        if (loop) ma_sound_set_looping(sound, MA_TRUE);

                        {
                            lock_guard<mutex> lock(mtx);
                            holding_sound[path] = sound;
                        }

                        ma_sound_start(sound);

                        if (!loop && !async) {
                            while (ma_sound_is_playing(sound) == MA_TRUE) {
                                this_thread::sleep_for(chrono::milliseconds(50));
                            }

                            lock_guard<mutex> lock(mtx);
                            ma_sound_uninit(sound);
                            delete sound;
                            holding_sound.erase(path);
                        } else {
                            // Asynchron abspielen
                            thread([sound, this, path]() {
                                while (ma_sound_is_playing(sound)) {
                                    this_thread::sleep_for(chrono::milliseconds(50));
                                }
                                ma_sound_uninit(sound);
                                delete sound;
                                holding_sound.erase(path);
                            }).detach();
                        }

                        return 0;
                    }

                    // Entfernt Sound aus dem System
                    int uninitSound(const string& path) {
                        lock_guard<mutex> lock(mtx);
                        auto it = holding_sound.find(path);
                        if (it != holding_sound.end()) {
                            if (it->second) {
                                ma_sound_uninit(it->second);
                                delete it->second;
                            }
                            holding_sound.erase(it);
                        }
                        return 0;
                    }

                    // Pausiert oder setzt Sound fort
                    void PauseResumeSound(const string& path) {
                        ma_sound* sound = nullptr;
                        {
                            lock_guard<mutex> lock(mtx);
                            auto it = holding_sound.find(path);
                            if (it == holding_sound.end()) return;
                            sound = it->second;
                        }

                        if (!sound) return;

                        if (ma_sound_is_playing(sound) == MA_TRUE) ma_sound_stop(sound);
                        else ma_sound_start(sound);
                    }

                    // Gibt Zeiger auf Sound zurück
                    inline ma_sound* getSoundPTR(const string& path) {
                        lock_guard<mutex> lock(mtx);
                        auto it = holding_sound.find(path);
                        return (it != holding_sound.end()) ? it->second : nullptr;
                    }

                private:
                    ma_engine engine{};
                    bool initialized = false;
                    unordered_map<string, ma_sound*> holding_sound;
                    mutex mtx;
            };
        #endif

        #ifdef CES_GEOMETRY_UNIT
            #include <vector>
            #include <cmath>
            #include <unordered_set>
            #define PI 3.1415926535897932384626433

            class CES_Shapes {
                public:
                    CES_Shapes() = default;

                    // Nested types müssen den CES::Scope kennen
                    struct CES_Line {
                        CES::CES_XY a;
                        CES::CES_XY b;
                        vector<CES::CES_XY> l;

                        CES_Line() = default;

                        void calculate_line(CES::CES_XY& d, CES::CES_XY& e, char32_t c, CES::CES_COLOR color, bool change = false) {
                            if (((a == d && b == e) || (a == e && b == d)) && !change) return;

                            int x0 = d.x, y0 = d.y;
                            int x1 = e.x, y1 = e.y;

                            a = d;
                            b = e;

                            int dx = abs(x0 - x1);
                            int dy = abs(y0 - y1);

                            int sx = (x0 < x1) ? 1 : -1;
                            int sy = (y0 < y1) ? 1 : -1;

                            int err = dx - dy;

                            while (true) {
                                CES::CES_XY xy;
                                xy.x = x0;
                                xy.y = y0;
                                xy.ARGB = color;
                                xy.c = c;

                                l.push_back(xy);

                                if (x0 == x1 && y0 == y1) break;

                                int e2 = 2 * err;
                                if (e2 > -dy) {
                                    err -= dy;
                                    x0 += sx;
                                }
                                if (e2 < dx) {
                                    err += dx;
                                    y0 += sy;
                                }
                            }
                        }

                        vector<CES::CES_XY>* pack_load_system_line() { return &l; }

                        vector<CES::CES_XY> PartPolygonReturn() {
                            vector<CES::CES_XY> tmp = l;
                            l.clear();
                            return tmp;
                        }
                    };

                    struct CES_Circle {
                        CES::CES_XY a;
                        CES::CES_XY b;
                        vector<CES::CES_XY> l;

                        CES_Circle() = default;

                        void calculate_circle(CES::CES_XY& d, CES::CES_XY& e, char32_t c, CES::CES_COLOR color, bool change = false) {
                            if (((a == d && b == e) || (a == e && b == d)) && !change) return;

                            double xc = d.x;
                            double yc = d.y;

                            double dx = e.x - d.x;
                            double dy = e.y - d.y;
                            double r = sqrt(dx*dx + dy*dy);

                            double xScale = 2.0;

                            for (double angle = 0.0; angle < 2.0 * PI; angle += 0.01) {
                                double x = xc + cos(angle) * r * xScale;
                                double y = yc + sin(angle) * r;

                                CES::CES_XY xy;
                                xy.x = static_cast<int>(round(x));
                                xy.y = static_cast<int>(round(y));
                                xy.c = c;
                                xy.ARGB = color;

                                l.push_back(xy);
                            }
                        }

                        vector<CES::CES_XY>* pack_load_system_circle() { return &l; }
                    };

                    struct CES_Polygon {
                        unordered_set<CES::CES_XY, CES::CES_XY::CES_XY_Hash> exist;
                        vector<CES::CES_XY> polygon;

                        CES_Polygon() = default;

                        void calculate_polygon(const unordered_set<CES::CES_XY, CES::CES_XY::CES_XY_Hash>& xy, char32_t c, CES::CES_COLOR color, bool change = false) {
                            if (CES::sets_equal(xy, exist) && !change) return;

                            vector<CES::CES_XY> tmp(xy.begin(), xy.end());

                            for (size_t i = 0; i < tmp.size() - 1; ++i) {
                                CES_Line line;
                                line.calculate_line(tmp[i], tmp[i+1], c, color, true);
                                vector<CES::CES_XY> a = line.PartPolygonReturn();
                                polygon.insert(polygon.end(), a.begin(), a.end());
                            }

                            CES_Line line;
                            line.calculate_line(tmp[0], tmp[tmp.size() - 1], c, color, true);
                            vector<CES::CES_XY> a = line.PartPolygonReturn();
                            polygon.insert(polygon.end(), a.begin(), a.end());
                        }

                        vector<CES::CES_XY>* pack_load_system_polygon() { return &polygon; }
                    };
            };
    #endif

};