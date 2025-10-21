// The following Code is the complete Engine
//
// Credits:
// - Cedric Schmermund (CEOofeinPlan); cedric.schmermund@gmail.com
// 
// If you find any bug or problem in this code, either write me an email, or put it into the issue list on the GitHub Repository.
//
// If you have any Ideas for this Engine, which could make it better, please also write me an email.
//
// If you a contribution, give ur own code credits and write me an email and make a pull request.
//
// ID / Last worked at it / name
// 1 / 04.10.2025 / Cedric Schmermund (CEOofeinPlan)
// 2 / 05.10.2025 / Cedric Schmermund (CEOofeinPlan)
// 3 / 06.10.2025 / Cedric Schmermund (CEOofeinPlan)
// 4 / 07.10.2025 / Cedric Schmermund (CEOofeinPlan)
// 5 / 14.10.2025 / Cedric Schmermund (CEOofeinPlan)
// 6 / 15.10.2025 / Cedric Schmermund (CEOofeinPlan)
// 7 / 16.10.2025 / Cedric Schmermund (CEOofeinPlan)

// General planning:
// First the engine, than the modules

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

    struct TerminalCapabilities {
        int supportsColor = 1;
        bool supportsRGB = false;
        bool supportsCursor = false;
        bool supportsMouse = false;
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
    #include <atomic>
    #include <set>

    using namespace std;

    class CES_AUDIO {
        public:
            enum CES_CUR_AUDIO_STATE {
                PLAY,
                PAUSE,
                LOOP,
                DELETE_AUDIO
            };

            void initializeSoundCard() {
                string command;
                #if defined(_WIN32)
                    command = "wmic sounddev get name";
                #elif defined(__linux__)
                    command = "cat /proc/asound/cards";
                #elif defined(__APPLE__)
                    command = "system_profiler SPAudioDataType";
                #else 
                    // Even if there is an soundcard, you couldn't play anything
                    hasSoundCard = false;
                    return;
                #endif

                FILE* pipe = popen(command.c_str(), "r");
                if (!pipe) {
                    hasSoundCard = false;
                    return;
                }

                char buffer[256];
                string out;
                while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                    out += buffer;
                }
                pclose(pipe);

                #if defined(_WIN32) 
                    hasSoundCard = out.find("Name") != string::npos;
                #elif defined(__linux__)
                    hasSoundCard = out.find("0 [") != string::npos;
                #elif defined(__APPLE__)
                    hasSoundCard = out.find("Output") != string::npos;
                #endif
            }

            /*@brief WAV audio files are working on every system, if your file isn't wav it's gonna be automatically converted into one; PUT false INTO THE FIRST PARAMETER TO STOP THIS PROCESS.*/
            int playAudio(const string& path, bool running, bool convert = true) {
                if (!convert) return -1;
                namespace fs = std::filesystem;
                string wavPath = path;
                if (!(path.size() > 4 && path.substr(path.size() - 4) == ".wav")) {
                    return -2;
                }
                
                if (wavPath.empty()) {
                    return -4;
                }

                #ifdef _WIN32
                    if (running) {
                        string cmd = "powershell -c \"(New-Object Media.SoundPlayer '" + fs::absolute(wavPath).string() + "').PlaySync()\"";
                        system(cmd.c_str());
                    }
                #elif defined(__linux__)
                    // Checking for paplay / aplay
                    if (system("command -v paplay >/dev/null 2>&1") == 0) {
                        system(("paplay '" + wavPath + "'").c_str());
                    } else {
                        system(("aplay '" + wavPath + "'").c_str());
                    }
                #elif defined(__APPLE__)
                    string cmd = "afplay '" + wavPath + "'";
                    system(cmd.c_str());
                #else
                    // Debug Command
                #endif

                return 0;
            }
            
            int inputSound(string& PATH, bool loop = false) {
                if (!(PATH.size() > 4 && PATH.substr(PATH.size() - 4) == ".wav")) {
                    return -1;
                }

                auto it = find(path.begin(), path.end(), PATH);

                if (it == path.end()) return -2;

                

                thread sound;

                pair<string, thread::id> tmp;

                tmp.first = PATH;
                tmp.second = sound.get_id();

                path.insert(tmp);

                pair<thread::id, atomic<CES_CUR_AUDIO_STATE>> tmp1;
                tmp1.first = sound.get_id();
                tmp1.second = CES_CUR_AUDIO_STATE::PAUSE;
                running.insert(tmp1);

                thrs.insert(sound.get_id());

                sound = thread([=]() mutable {
                    auto it = running.begin();
                    while (it != running.end()) { if (it->first == this_thread::get_id()) break; else it++;}
                    if (it != running.end()) {
                        do {
                            int elapsed = 0;
                            while (elapsed < getWavDuration(PATH) && it->second != CES_CUR_AUDIO_STATE::DELETE_AUDIO) {
                                if (it->second == CES_CUR_AUDIO_STATE::PAUSE) {
                                    this_thread::sleep_for(chrono::milliseconds(10));
                                    continue;
                                }

                                this
                            }
                        }
                    }
                });
            }

            bool hasSoundCard;
            
            private:
                void runningSound();

                set<pair<string, thread::id>> path;
                set<thread::id> thrs;
                set<pair<thread::id, atomic<CES_CUR_AUDIO_STATE>>> running;

                struct WAVHeader {
                    char riff[4];             // "RIFF"
                    uint32_t chunkSize;
                    char wave[4];             // "WAVE"
                    char fmt[4];              // "fmt "
                    uint32_t subchunk1Size;   // 16 for PCM
                    uint16_t audioFormat;     // PCM = 1
                    uint16_t numChannels;
                    uint32_t sampleRate;
                    uint32_t byteRate;
                    uint16_t blockAlign;
                    uint16_t bitsPerSample;
                    char data[4];             // "data"
                    uint32_t subchunk2Size;   // size of the data section
                };

                double getWavDuration(const string& filename) {
                    ifstream file(filename, ios::binary);
                    if (!file) return 0.0;
                    WAVHeader header{};
                    file.read(reinterpret_cast<char*>(&header), sizeof(WAVHeader));

                    if (string(header.riff, 4) != "RIFF" || string(header.wave, 4) != "WAVE") return 0.0;

                    double duration = static_cast<double>(header.subchunk2Size) / (header.sampleRate * header.numChannels * (header.bitsPerSample / 8.0));

                    return duration;
                }
    };

#endif

