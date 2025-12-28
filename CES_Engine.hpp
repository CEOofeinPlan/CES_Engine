/* @brief The Cell System only stores the character that should be outputed; Its much faster and more flexible; Every Cell is stored in a HashMap.*/
    #include <unordered_map>
    #include <unistd.h>
    #include <cmath>
    #include <cstdint>
    #include <cstdlib>
    #include <filesystem>
    #include <type_traits>
    #include <functional>
    #include <stack>
    #include <string>
    #include <unordered_set>
    #include <algorithm>
    #include <set>
    #include <thread>
    #include <mutex>
    #include <atomic>
    #include <condition_variable>
    #include <queue>
    #include <variant>
    #include <fstream>
    #include <iostream>


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
            uint16_t r = 0; // red
            uint16_t g = 0; // green
            uint16_t b = 0; // blue
            uint16_t a = 0; // alpha

            CES_COLOR(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0, uint8_t alpha = 255) {
                r = red;
                g = green;
                b = blue;
                a = alpha;
            }

            uint32_t to_uint() const noexcept {
                return (uint32_t(r) << 24) |
                    (uint32_t(g) << 16) |
                    (uint32_t(b) << 8 ) |
                    uint32_t(a);
            }

            inline void Convert_Without_Alpha(int color) noexcept {
                r = color << 24;
                g = color << 16;
                b = color << 8;
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
            int x; // max. 65K Character
            int y; // max. 65K Character
            int z;      // Enough layers 
            
            CES_COLOR ARGB; // Alpha-Red-Green-Blue

            char32_t c; // UniCode Character

            CES_XY() = default;
            CES_XY(int X, int Y) : x(X), y(Y) {}
            CES_XY(int X, int Y, int Z) : x(X), y(Y), z(Z) {};
            CES_XY(const CES_XY& o) {
                x = o.x;
                y = o.y;
                z = o.z;
                ARGB = o.ARGB;
                c = o.c;
            };
            CES_XY(int X, int Y, int Z, CES_COLOR color, char32_t C) : x(X), y(Y), z(Z), ARGB(color), c(C) {};
            CES_XY(CES_XY&&) noexcept = default;
            CES_XY& operator=(const CES_XY&) = default;
            CES_XY& operator=(CES_XY&&) noexcept = default;

            bool operator==(const CES_XY& other) const noexcept {
                return x == other.x
                    && y == other.y;
            }

            bool fullEquals(const CES_XY& other) const noexcept {
                return x == other.x && y == other.y
                    && ARGB == other.ARGB && c == other.c && z == other.z;
            }

            struct CES_XY_Hash {
                size_t operator()(const CES_XY& v) const noexcept {
                    size_t h = 0;

                    auto mix = [&h](size_t x) {
                        h ^= x + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
                    };

                    mix(static_cast<size_t>(v.x));
                    mix(static_cast<size_t>(v.y));
                    mix(static_cast<size_t>(v.z));
                    mix(static_cast<size_t>(v.ARGB.to_uint()));
                    mix(static_cast<size_t>(v.c));

                    return h;
                }
            };

            struct PairHash {
                size_t operator()(const pair<int, int>& p) const noexcept {
                    return (static_cast<size_t>(p.first) << 16) | p.second;
                }
            };

            struct VecHash {
                size_t operator()(const vector<CES_XY>& v) const noexcept {
                    size_t h = 0;
                    CES_XY_Hash hasher;

                    for (const auto& p : v) {
                        h ^= hasher(p)
                        + 0x9e3779b97f4a7c15ULL
                        + (h << 6)
                        + (h >> 2);
                    }
                    return h;
                }
            };

            struct VecPtrHash
            {
                size_t operator()(const vector<CES::CES_XY>* v) const noexcept
                {
                    return hash<const void*>{}(v);
                }
            };



            string convertCHAR32toCHAR(char32_t c) const
            {
                string out;

                if (c <= 0x7F) {
                    out.push_back(static_cast<char>(c));
                }
                else if (c <= 0x7FF) {
                    out.push_back(static_cast<char>(0xC0 | ((c >> 6) & 0x1F)));
                    out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
                }
                else if (c <= 0xFFFF) {
                    out.push_back(static_cast<char>(0xE0 | ((c >> 12) & 0x0F)));
                    out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
                }
                else if (c <= 0x10FFFF) {
                    out.push_back(static_cast<char>(0xF0 | ((c >> 18) & 0x07)));
                    out.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
                }
                else {
                    out = "\xEF\xBF\xBD";
                }

                return out;
            }

        };

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
                        // ! --> If you compile your project make sure it is compiled "static"; People who wants to use your project or play your game needs the file: "windows.h"; If the "static" attribute, in your compile command is set, then no issue will appear in this context.

                        ClearConsole();
                        // I/O without comparing to C
                        ios::sync_with_stdio(false);
                        cin.tie(nullptr);
                        #if defined(_WIN32)
                        #define WIN32_LEAN_AND_MEAN
                            SetConsoleCP(CP_UTF8);
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
                        ClearConsole();
                        cout << "\033[1;1H";
                    }

                    void OutputCurWindow() {
                        sort(change.begin(), change.end(), [](const CES::CES_XY& a, const CES::CES_XY& b) {
                            return a.z < b.z;
                        });

                        auto main = &change;
                        auto erase = &remove;
                        auto cur = &frame;
                        auto mtx = &mtx_write;
                        pair<int, int> size = WidthHeight();

                        ThreadPool pool(4);
                        unordered_map<pair<int,int>, CES::CES_XY, PairHash> thread_0;
                        unordered_map<pair<int,int>, CES::CES_XY, PairHash> thread_1;
                        unordered_map<pair<int,int>, CES::CES_XY, PairHash> thread_2;
                        unordered_map<pair<int,int>, CES::CES_XY, PairHash> thread_3;

                        // ! Tiles are numbered clockwise. (Thread 0: left-top, thread 1: right-top, thread 2: right-bottom, thread 3: left-bottom)
                        atomic<int> remaining = 4;
                        // Thread 0
                        pool.enqueue([&main, &size, &thread_0, &remaining, &cur, &erase, &mtx]() {
                            int x = size.first;
                            int y = size.second;
                            int x_start = 0;
                            int y_start = 0;
                            int x_end = x / 2;
                            int y_end = y / 2;

                            for (auto& [p, c] : *erase) {
                                x = c.x;
                                y = c.y;
                                if (x < x_start || x >= x_end || y < y_start || y >= y_end) continue;
                                unique_lock<mutex> lock(*mtx);
                                cur->erase({x,y});
                                lock.unlock();
                                // INT_MIN so the next for loop can overwrite it.
                                CES::CES_XY xy(c.x,c.y,INT_MIN,CES_COLOR(0,0,0),' ');
                                thread_0[{x,y}] = xy;
                            }

                            for (CES::CES_XY& c : *main) {
                                x = c.x;
                                y = c.y;
                                // If the current cell is in the thread area, if not CONTINUE.
                                if (x < x_start || x >= x_end || y < y_start || y >= y_end) continue;
                                if (cur->count({x,y})) {
                                    auto it = cur->find({x,y});
                                    if (it->second.z >= c.z) continue;
                                }
                                // Due to this operation: "==" only x and y will be compared, nothing more.
                                // If the cell does not exist; It will be inserted.
                                thread_0[{x,y}] = c;
                            }
                            remaining.fetch_sub(1, memory_order_release);
                        });

                        // Thread 1
                        pool.enqueue([&main, &size, &thread_1, &remaining, &cur, &erase, &mtx]() {
                            int x = size.first;
                            int y = size.second;
                            int x_start = x/2;
                            int y_start = 0;
                            int x_end = x;
                            int y_end = y/2;

                            for (auto& [p, c] : *erase) {
                                x = c.x;
                                y = c.y;
                                if (x < x_start || x >= x_end || y < y_start || y >= y_end) continue;
                                unique_lock<mutex> lock(*mtx);
                                cur->erase({x,y});
                                lock.unlock();
                                // INT_MIN so the next for loop can overwrite it.
                                CES::CES_XY xy(c.x,c.y,INT_MIN,CES_COLOR(0,0,0),' ');
                                thread_1[{x,y}] = xy;
                            }

                            for (CES::CES_XY& c : *main) {
                                x = c.x;
                                y = c.y;
                                // If the current cell is in the thread area, if not CONTINUE.
                                if (x < x_start || x >= x_end || y < y_start || y >= y_end) continue;
                                if (cur->count({x,y})) {
                                    auto it = cur->find({x,y});
                                    if (it->second.z >= c.z) continue;
                                }
                                // Due to this operation: "==" only x and y will be compared, nothing more.
                                // If the cell does not exist; It will be inserted.
                                thread_1[{x,y}] = c;
                            }
                            remaining.fetch_sub(1, memory_order_release);
                        });

                        // Thread 2
                        pool.enqueue([&main, &size, &thread_2, &remaining, &cur, &erase, &mtx]() {
                            int x = size.first;
                            int y = size.second;
                            int x_start = x/2;
                            int y_start = y/2;
                            int x_end = x;
                            int y_end = y;

                            for (auto& [p, c] : *erase) {
                                x = c.x;
                                y = c.y;
                                if (x < x_start || x >= x_end || y < y_start || y >= y_end) continue;
                                unique_lock<mutex> lock(*mtx);
                                cur->erase({x,y});
                                lock.unlock();
                                // INT_MIN so the next for loop can overwrite it.
                                CES::CES_XY xy(c.x,c.y,INT_MIN,CES_COLOR(0,0,0),' ');
                                thread_2[{x,y}] = c;
                            }

                            for (CES::CES_XY& c : *main) {
                                x = c.x;
                                y = c.y;
                                // If the current cell is in the thread area, if not CONTINUE.
                                if (x < x_start || x >= x_end || y < y_start || y >= y_end) continue;
                                if (cur->count({x,y})) {
                                    auto it = cur->find({x,y});
                                    if (it->second.z >= c.z) continue;
                                }
                                // Due to this operation: "==" only x and y will be compared, nothing more.
                                // If the cell does not exist; It will be inserted.
                                thread_2[{x,y}] = c;
                            }
                            remaining.fetch_sub(1, memory_order_release);
                        });

                        // Thread 3
                        pool.enqueue([&main, &size, &thread_3, &remaining, &cur, &erase, &mtx]() {
                            int x = size.first;
                            int y = size.second;
                            int x_start = 0;
                            int y_start = y/2;
                            int x_end = x/2;
                            int y_end = y;

                            for (auto& [p, c] : *erase) {
                                x = c.x;
                                y = c.y;
                                if (x < x_start || x >= x_end || y < y_start || y >= y_end) continue;
                                unique_lock<mutex> lock(*mtx);
                                cur->erase({x,y});
                                lock.unlock();
                                // INT_MIN so the next for loop can overwrite it.
                                CES::CES_XY xy(c.x,c.y,INT_MIN,CES_COLOR(0,0,0),' ');
                                thread_3[{x,y}] = c;
                            }

                            for (CES::CES_XY& c : *main) {
                                x = c.x;
                                y = c.y;
                                // If the current cell is in the thread area, if not CONTINUE.
                                if (x < x_start || x >= x_end || y < y_start || y >= y_end) continue;
                                if (cur->count({x,y})) {
                                    auto it = cur->find({x,y});
                                    if (it->second.z >= c.z) continue;
                                }
                                // Due to this operation: "==" only x and y will be compared, nothing more.
                                // If the cell does not exist; It will be inserted.
                                thread_3[{x,y}] = c;
                            }
                            remaining.fetch_sub(1, memory_order_release);
                        });

                        while (remaining.load(memory_order_acquire) > 0) {
                            this_thread::yield();
                        }
                        string everything = "";
                        for (auto& c : thread_3) {
                            everything += c.second.convertCHAR32toCHAR(c.second.c);
                            everything += " ";
                        }

                        ofstream("Debug.txt") << everything << endl;


                        string framebuffer;

                        // Thread 0
                        for (auto& [p, c] : thread_0) {
                            framebuffer += "\033[" + to_string(c.y+1) + ";" + to_string(c.x+1) + "H";
                            framebuffer += "\033[38;2;" +
                                        to_string((int)c.ARGB.r) + ";" +
                                        to_string((int)c.ARGB.g) + ";" +
                                        to_string((int)c.ARGB.b) + "m";
                            framebuffer += c.convertCHAR32toCHAR(c.c);
                            framebuffer += "\033[0m";
                            //if (c.x == 15 && c.y == 11 && c.c == ' ') ofstream("Debug.txt") << "true" << endl;
                        }

                        // Thread 1
                        for (auto& [p, c] : thread_1) {
                            framebuffer += "\033[" + to_string(c.y+1) + ";" + to_string(c.x+1) + "H";
                            framebuffer += "\033[38;2;" +
                                        to_string((int)c.ARGB.r) + ";" +
                                        to_string((int)c.ARGB.g) + ";" +
                                        to_string((int)c.ARGB.b) + "m";
                            framebuffer += c.convertCHAR32toCHAR(c.c);
                            framebuffer += "\033[0m";
                            //if (c.x == 15 && c.y == 11) ofstream("Debug.txt") << "true" << endl;
                        }

                        // Thread 2
                        for (auto& [p, c] : thread_2) {
                            framebuffer += "\033[" + to_string(c.y+1) + ";" + to_string(c.x+1) + "H";
                            framebuffer += "\033[38;2;" +
                                        to_string((int)c.ARGB.r) + ";" +
                                        to_string((int)c.ARGB.g) + ";" +
                                        to_string((int)c.ARGB.b) + "m";
                            framebuffer += c.convertCHAR32toCHAR(c.c);
                            framebuffer += "\033[0m";
                            //if (c.x == 15 && c.y == 11) ofstream("Debug.txt") << "true" << endl;
                        }

                        // Thread 3
                        for (auto& [p, c] : thread_3) {
                            framebuffer += "\033[" + to_string(c.y+1) + ";" + to_string(c.x+1) + "H";
                            framebuffer += "\033[38;2;" +
                                        to_string((int)c.ARGB.r) + ";" +
                                        to_string((int)c.ARGB.g) + ";" +
                                        to_string((int)c.ARGB.b) + "m";
                            framebuffer += c.convertCHAR32toCHAR(c.c);
                            framebuffer += "\033[0m";
                            //if (c.x == 15 && c.y == 11) ofstream("Debug.txt") << "true" << endl;
                        }

                        // Hide Cursor
                        framebuffer += "\033[?25l";
                        #if defined(_WIN32)
                            DWORD written;
                            WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),
                                    framebuffer.data(),
                                    (DWORD)framebuffer.size(),
                                    &written,
                                    nullptr);
                        #else
                            write(STDOUT_FILENO, framebuffer.data(), framebuffer.size());
                        #endif

                        // ! Needs to be cleared.
                        change.clear();
                        frame.insert(thread_0.begin(), thread_0.end());
                        frame.insert(thread_1.begin(), thread_1.end());
                        frame.insert(thread_2.begin(), thread_2.end());
                        frame.insert(thread_3.begin(), thread_3.end());
                    }

                    void OutputCurWindowAlias() {

                    }

                    inline void writeCell(CES::CES_XY& xy) {
                        change.push_back(xy);
                    }

                    void writeCell(int x, int y, int z, CES::CES_COLOR color, char32_t c) {
                        CES::CES_XY xy;
                        xy.x = x;
                        xy.y = y;
                        xy.z = z;
                        xy.c = c;
                        xy.ARGB = color;
                        change.push_back(xy);
                    }

                    inline void writeCell(vector<CES::CES_XY>* xy) {
                        for (auto& c : *xy) {
                            change.push_back(c);
                        }
                    }

                    void removeCell(int x, int y) {
                        CES::CES_XY xy;
                        xy.x = x;
                        xy.y = y;
                        xy.z = INT_MIN;
                        xy.c = ' ';
                        xy.ARGB = CES::CES_COLOR(0,0,0);
                        remove[{xy.x,xy.y}] = xy;
                    }

                    void removeCell(CES::CES_XY& xy) {
                        xy.ARGB = CES_COLOR(0,0,0);
                        xy.c = ' ';
                        xy.z = INT_MIN;
                        remove[{xy.x,xy.y}] = xy;
                    }

                    void removeCell(vector<CES::CES_XY>* xy) {
                        for (auto& c : *xy) {
                            c.ARGB = CES_COLOR(0,0,0);
                            c.z = INT_MIN;
                            c.c = ' ';
                            remove[{c.x, c.y}] = c;
                        }
                    }

                    pair<int, int> WidthHeight() {
                        int cols = 0, rows = 0;
                        #if defined(__WIN32)
                            CONSOLE_SCREEN_BUFFER_INFO csbi;
                            if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
                                cols = csbi.srWindow.Right - csbi.srWindow.Left+1;
                                rows = csbi.srWindow.Bottom - csbi.srWindow.Top+1;
                            }
                        #else
                            struct winsize w;
                            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
                                cols = w.ws_col;
                                rows = w.ws_row;
                            }
                        #endif

                        return {cols, rows};
                    }

                    void ClearConsole() {
                        cout << "\033[2J";      // Delete the screen
                        cout << "\033[1;1H";
                        cout << "\033[?25l";    // Hide cursor
                    }
                    
                    struct PairHash {
                        template <class T1, class T2>
                        size_t operator()(const pair<T1, T2>& p) const noexcept {
                            return hash<T1>()(p.first) ^ (hash<T2>()(p.second) << 1);
                        }
                    };

                    // Idea -- Rendering fast in a terminal engine:
                    //  1. The main Threads (developer threads) are writing their cells into the 'change'
                    //  2. When the main code calls the 'CES::CES_Screen::OutputCurWindow()'-method this will happen:
                    //      1. The current vector is sorted after the z-index.
                    //      2. 4 worker threads are made which are getting a pointer to the current sorted vector.
                    //          -> A worker thread works as followed:
                    //              1. If the current cell is NOT in his area, he will continue with the iteration
                    //              2. If the current cell is in his area, he will do the following steps:
                    //                  1. He checks if the position of this cell already exists in his own pair set (sorted<pair<...,...>>)
                    //                  2. If it does he will continue with the iteration
                    //                  3. If not he will write this cell in the pair set as well as in his area 'unordered_set'.
                    //      3. After all threads are done calculating every cell, they will return their hash set of cells.
                    //      4. The Render thread is then outputing those cells in the terminal.
                    //      5. 'change' is cleared afterwards
                    //
                    // ! --> IMPORTANT: THE 'change' is ONLY for changes.
                    //  Means if you do no changes, then the frame will be CONSTANT the same.
                    // ! -> Also: If a cell needs to be deleted use the following steps:
                    //  1. Write the z index of the cells at MINIMUM.
                    //  2. Change their character to: ' '
                    //  3. Make their color black.
                    //  4. Insert this cell into the 'change'
                    vector<CES::CES_XY> change;
                    unordered_map<pair<int,int>, CES::CES_XY, PairHash> remove;
                    unordered_map<pair<int,int>, CES::CES_XY, PairHash> frame;
                    mutex mtx_write;
            };

        #endif

        #ifdef CES_GRID_SYSTEM

        #endif

        #ifdef CES_THREAD_POOL
            class ThreadPool {
                private:
                    using Task = function<void()>;
                    using PersistentTask = function<void(atomic_bool&)>;

                    /* One-time worker pool */
                    vector<thread> workers;
                    queue<Task> task_queue;
                    mutex queue_mtx;
                    condition_variable cv;
                    atomic_bool stop{false};

                    /* Persistent workers */
                    vector<thread> persistent_workers;
                    vector<unique_ptr<atomic_bool>> persistent_flags;
                    mutex persistent_mtx;

                public:
                    explicit ThreadPool(size_t thread_count) {
                        for (size_t i = 0; i < thread_count; ++i) {
                            workers.emplace_back([this] {
                                worker_loop();
                            });
                        }
                    }

                    /* ThreadPool cannot be copied or moved */
                    ThreadPool(const ThreadPool&) = delete;
                    ThreadPool& operator=(const ThreadPool&) = delete;

                    ~ThreadPool() {
                        shutdown();
                    }

                    /* Works once not consistent */
                    void enqueue(Task task) {
                        {
                            lock_guard<mutex> lock(queue_mtx);
                            if (stop.load(memory_order_relaxed)) {
                                return;
                            }
                            task_queue.push(move(task));
                        }
                        cv.notify_one();
                    }

                    /* Persistent task  */
                    void start_persistent(PersistentTask task) {
                        auto flag = make_unique<atomic_bool>(true);
                        atomic_bool& flag_ref = *flag;

                        lock_guard<mutex> lock(persistent_mtx);
                        persistent_flags.push_back(move(flag));

                        persistent_workers.emplace_back([task, &flag_ref] {
                            try {
                                while (flag_ref.load(memory_order_acquire)) {
                                    task(flag_ref);
                                    this_thread::yield(); // stops busy-wait
                                }
                            } catch (...) {
                                /* thread must not destroy the pool */
                            }
                        });
                    }

                private:
                    void worker_loop() {
                        while (true) {
                            Task task;

                            {
                                unique_lock<mutex> lock(queue_mtx);
                                cv.wait(lock, [this] {
                                    return stop.load(memory_order_acquire) || !task_queue.empty();
                                });

                                if (stop.load(memory_order_relaxed) && task_queue.empty()) {
                                    return;
                                }

                                task = move(task_queue.front());
                                task_queue.pop();
                            }

                            try {
                                task();
                            } catch (...) {
                                /* thread must not destroy the pool */
                            }
                        }
                    }

                    void shutdown() {
                        /* One-time worker stopping */
                        {
                            lock_guard<mutex> lock(queue_mtx);
                            stop.store(true, memory_order_release);
                        }
                        cv.notify_all();

                        for (auto& t : workers) {
                            if (t.joinable()) {
                                t.join();
                            }
                        }

                        /* Persistent worker stopping */
                        {
                            lock_guard<mutex> lock(persistent_mtx);
                            for (auto& flag : persistent_flags) {
                                flag->store(false, memory_order_release);
                            }
                        }

                        for (auto& t : persistent_workers) {
                            if (t.joinable()) {
                                t.join();
                            }
                        }
                    }
                };
        #endif

        /*
        *
        *   Doesn't work yet. But not as a priority listed either.
        * 
        */

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
                    if (CES_CC_A(red, green, blue, 0,0,0))    color->Convert_Without_Alpha(CES_16_BLACK);
                else if (CES_CC_A(red, green, blue, 2,0,0))   color->Convert_Without_Alpha(CES_16_RED);
                else if (CES_CC_A(red, green, blue, 0,2,0))   color->Convert_Without_Alpha(CES_16_GREEN);
                else if (CES_CC_A(red, green, blue, 2,2,0))   color->Convert_Without_Alpha(CES_16_YELLOW);
                else if (CES_CC_A(red, green, blue, 0,0,2))   color->Convert_Without_Alpha(CES_16_BLUE);
                else if (CES_CC_A(red, green, blue, 2,0,2))   color->Convert_Without_Alpha(CES_16_MAGENTA);
                else if (CES_CC_A(red, green, blue, 0,2,2))   color->Convert_Without_Alpha(CES_16_CYAN);
                else if (CES_CC_A(red, green, blue, 3,3,3))   color->Convert_Without_Alpha(CES_16_LIGHT_GRAY);
                // 16 color terminal
                else if (CES_CC_A(red, green, blue, 2,2,2))   color->Convert_Without_Alpha(CES_16_GRAY);
                else if (CES_CC_A(red, green, blue, 4,0,0))   color->Convert_Without_Alpha(CES_16_BRIGHT_RED);
                else if (CES_CC_A(red, green, blue, 0,4,0))   color->Convert_Without_Alpha(CES_16_BRIGHT_GREEN);
                else if (CES_CC_A(red, green, blue, 4,4,0))   color->Convert_Without_Alpha(CES_16_BRIGHT_YELLOW);
                else if (CES_CC_A(red, green, blue, 0,0,4))   color->Convert_Without_Alpha(CES_16_BRIGHT_BLUE);
                else if (CES_CC_A(red, green, blue, 4,0,4))   color->Convert_Without_Alpha(CES_16_BRIGHT_MAGENTA);
                else if (CES_CC_A(red, green, blue, 0,4,4))   color->Convert_Without_Alpha(CES_16_BRIGHT_CYAN);
                else if (CES_CC_A(red, green, blue, 4,4,4))   color->Convert_Without_Alpha(CES_16_WHITE);

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
                            // Asynchron
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

                    // removes sound from the system
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

                    // Resumes sound
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

                    // returns a pointer to the sound currently playing to allow direct access to functions from 'miniaudio.h'
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
            #include <algorithm>
            #define PI 3.1415926535897932384626433

            class CES_Shapes {
                public:
                    CES_Shapes() {}

                    static CES::CES_Screen* sys;

                    struct CES_Line {
                        CES::CES_XY a;
                        CES::CES_XY b;
                        vector<CES::CES_XY> l;
                        int z;

                        CES_Line(int Z) : z(Z) {}

                        void calculate_line(CES::CES_XY& d, CES::CES_XY& e, char32_t c, CES::CES_COLOR color, bool change = false) {
                            if (((a == d && b == e) || (a == e && b == d)) && !change) return;
                            l.clear();

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
                                xy.z = z;

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
                    };

                    struct CES_Circle {
                        CES::CES_XY a;
                        CES::CES_XY b;
                        vector<CES::CES_XY> l;
                        vector<CES::CES_XY> clean;
                        int z;

                        CES_Circle(int Z) : z(Z) {}

                        void calculate_circle(CES::CES_XY& d, CES::CES_XY& e, char32_t c, CES::CES_COLOR color, bool change = false) {
                            if (((a == d && b == e) || (a == e && b == d)) && !change) return;

                            clean = move(l);
                            l.clear();

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
                                xy.z = z;
                                xy.c = c;
                                xy.ARGB = color;

                                l.push_back(xy);
                            }
                        }

                        vector<CES::CES_XY>* pack_load_system_circle() { return &l; }
                    };

                    struct CES_Polygon {
                        unordered_set<CES::CES_XY, CES::CES_XY::CES_XY_Hash> exist; // Original Points
                        vector<CES::CES_XY> l;     // Edge Points
                        vector<CES::CES_XY> clean;
                        int z;

                        CES_Polygon(int Z) : z(Z) {}

                        void calculate_polygon(const unordered_set<CES::CES_XY, CES::CES_XY::CES_XY_Hash>& xy, char32_t c, CES::CES_COLOR color, bool change = false)
                        {
                            // Exit if polygon is unchanged
                            if (exist == xy)
                                return;

                            exist = xy;

                            clean = move(l);
                            l.clear();

                            if (xy.size() < 2)
                                return;

                            vector<CES::CES_XY> tmp;
                            tmp.reserve(xy.size());
                            tmp.insert(tmp.end(), xy.begin(), xy.end());

                            // Centroid
                            double cx = 0.0, cy = 0.0;
                            for (const auto& p : tmp) {
                                cx += p.x;
                                cy += p.y;
                            }
                            cx /= tmp.size();
                            cy /= tmp.size();

                            // Sort by polar angle, tie-break by distance
                            sort(tmp.begin(), tmp.end(),
                                [cx, cy](const CES::CES_XY& a, const CES::CES_XY& b) {
                                    double aa = atan2(a.y - cy, a.x - cx);
                                    double ab = atan2(b.y - cy, b.x - cx);
                                    if (aa != ab)
                                        return aa < ab;

                                    double da = (a.x - cx) * (a.x - cx) + (a.y - cy) * (a.y - cy);
                                    double db = (b.x - cx) * (b.x - cx) + (b.y - cy) * (b.y - cy);
                                    return da < db;
                                });

                            // Bresenham
                            auto draw_line =
                                [&](const CES::CES_XY& d,
                                    const CES::CES_XY& e,
                                    char32_t ch,
                                    CES::CES_COLOR ARGB,
                                    int z)
                            {
                                int x0 = d.x, y0 = d.y;
                                int x1 = e.x, y1 = e.y;

                                int dx = abs(x1 - x0);
                                int dy = abs(y1 - y0);

                                int sx = (x0 < x1) ? 1 : -1;
                                int sy = (y0 < y1) ? 1 : -1;

                                int err = dx - dy;

                                while (true) {
                                    CES::CES_XY p;
                                    p.x = x0;
                                    p.y = y0;
                                    p.z = z;
                                    p.c = ch;
                                    p.ARGB = ARGB;
                                    l.push_back(p);

                                    if (x0 == x1 && y0 == y1)
                                        break;

                                    int e2 = err << 1;
                                    if (e2 > -dy) { err -= dy; x0 += sx; }
                                    if (e2 <  dx) { err += dx; y0 += sy; }
                                }
                            };

                            for (size_t i = 0; i + 1 < tmp.size(); ++i) {
                                draw_line(tmp[i], tmp[i + 1], c, color, z);
                            }

                            // Closing edge
                            draw_line(tmp.back(), tmp.front(), c, color, z);
                        }

                        vector<CES::CES_XY>* pack_load_system_polygon() { return &l; }
                    };

                    struct CES_Ellipse {
                        vector<CES::CES_XY> exist; // Original Points
                        vector<CES::CES_XY> l;     // Edge Points
                        vector<CES::CES_XY> clean;
                        int z;

                        CES_Ellipse(int Z) : z(Z) {}

                        void calculate_ellipse(CES::CES_XY p1, float r1, float r2, char32_t c, CES::CES_COLOR color) {
                            clean = move(l);
                            l.clear();
                            r1 *= 2; // To balance the terminal size of: 2:1.
                            int x0 = static_cast<int>(p1.x);
                            int y0 = static_cast<int>(p1.y);

                            int x = 0;
                            int y = static_cast<int>(r2);
                            int rx2 = static_cast<int>(r1 * r1);
                            int ry2 = static_cast<int>(r2 * r2);
                            int tworx2 = 2 * rx2;
                            int twory2 = 2 * ry2;
                            int px = 0;
                            int py = tworx2 * y;

                            int p = static_cast<int>(ry2 - (rx2 * r2) + (0.25f * rx2));
                            while (px < py) {
                                auto plot = [&](int dx, int dy){
                                    CES::CES_XY pt1, pt2, pt3, pt4;
                                    pt1.x = x0 + dx, pt1.y = y0 + dy, pt1.c = c, pt1.ARGB = color;
                                    pt2.x = x0 - dx, pt2.y = y0 + dy, pt2.c = c, pt2.ARGB = color;
                                    pt3.x = x0 + dx, pt3.y = y0 - dy, pt3.c = c, pt3.ARGB = color;
                                    pt4.x = x0 - dx, pt4.y = y0 - dy, pt4.c = c, pt4.ARGB = color;
                                    l.push_back(pt1);
                                    l.push_back(pt2);
                                    l.push_back(pt3);
                                    l.push_back(pt4);
                                };
                                plot(x, y);
                                x++;
                                px += twory2;
                                if (p < 0)
                                    p += ry2 + px;
                                else {
                                    y--;
                                    py -= tworx2;
                                    p += ry2 + px - py;
                                }
                            }

                            p = static_cast<int>(ry2 * (x + 0.5f) * (x + 0.5f) + rx2 * (y - 1) * (y - 1) - rx2 * ry2);
                            while (y >= 0) {
                                auto plot = [&](int dx, int dy){
                                    CES::CES_XY pt1, pt2, pt3, pt4;
                                    pt1.x = x0 + dx, pt1.y = y0 + dy, pt1.c = c, pt1.ARGB = color, pt1.z = z;
                                    pt2.x = x0 - dx, pt2.y = y0 + dy, pt2.c = c, pt2.ARGB = color, pt2.z = z;
                                    pt3.x = x0 + dx, pt3.y = y0 - dy, pt3.c = c, pt3.ARGB = color, pt3.z = z;
                                    pt4.x = x0 - dx, pt4.y = y0 - dy, pt4.c = c, pt4.ARGB = color, pt4.z = z;
                                    l.push_back(pt1);
                                    l.push_back(pt2);
                                    l.push_back(pt3);
                                    l.push_back(pt4);
                                };
                                plot(x, y);
                                y--;
                                py -= tworx2;
                                if (p > 0)
                                    p += rx2 - py;
                                else {
                                    x++;
                                    px += twory2;
                                    p += rx2 - py + px;
                                }
                            }

                        }

                        vector<CES::CES_XY>* pack_load_system_ellipse() { return &l; }
                    };
                    
                    class CES_Transformation {
                        public:
                            // The clean setup doesn't need to send the clean points to the system.
                            /*
                            *
                            *   Still one error; If there is more than one area to fill, it will only fill one.
                            *   --> Should fill all available areas of a shape. 
                            * 
                            */
                            int fill_shape(vector<CES::CES_XY>* list, CES::CES_COLOR color, uint32_t c) {
                                if (!list || list->empty()) return 0;

                                long sumX = 0, sumY = 0;
                                for (auto& p : *list) {
                                    sumX += p.x;
                                    sumY += p.y;
                                }
                                int cx = int(sumX / list->size());
                                int cy = int(sumY / list->size());
                                int z = (*list)[0].z;

                                auto isBorder = [&](int x, int y) {
                                    for (auto& p : *list)
                                        if (p.x == x && p.y == y)
                                            return true;
                                    return false;
                                };

                                if (isBorder(cx, cy)) {
                                    static const int off[4][2] = {
                                        {1,0},{-1,0},{0,1},{0,-1}
                                    };
                                    bool found = false;
                                    for (auto& o : off) {
                                        if (!isBorder(cx + o[0], cy + o[1])) {
                                            cx += o[0];
                                            cy += o[1];
                                            found = true;
                                            break;
                                        }
                                    }
                                    if (!found)
                                        return 0;
                                }

                                stack<pair<int,int>> st;
                                st.push({cx, cy});

                                set<pair<int,int>> visited;

                                int filled = 0;

                                while (!st.empty()) {

                                    auto [x, y] = st.top(); st.pop();
                                    if (visited.count({x,y})) continue;
                                    visited.insert({x,y});

                                    if (isBorder(x, y)) continue;

                                    CES::CES_XY p;
                                    p.x = x;
                                    p.y = y;
                                    p.c = c;
                                    p.z = z;
                                    p.ARGB = color;
                                    list->push_back(p);
                                    filled++;

                                    st.push({x+1, y});
                                    st.push({x-1, y});
                                    st.push({x, y+1});
                                    st.push({x, y-1});
                                }

                                return filled;
                            }

                            int rotation(std::vector<CES::CES_XY>* xy, double deg)
                            {
                                if (xy->empty())
                                    return -1;

                                // Calculate heavypoint
                                double cx = 0.0;
                                double cy = 0.0;

                                for (const auto& p : *xy) {
                                    cx += p.x;
                                    cy += p.y;
                                }

                                cx /= xy->size();
                                cy /= xy->size();

                                // Calculate angle
                                const double rad = deg * PI / 180.0;
                                const double cosA = std::cos(rad);
                                const double sinA = std::sin(rad);

                                // rotate
                                for (auto& p : *xy) {
                                    const double x = p.x - cx;
                                    const double y = p.y - cy;

                                    const double xNew = x * cosA - y * sinA;
                                    const double yNew = x * sinA + y * cosA;

                                    // korrekt runden statt abschneiden
                                    p.x = static_cast<int>(std::lround(xNew + cx));
                                    p.y = static_cast<int>(std::lround(yNew + cy));
                                }

                                return 0;
                            }

                            int scale(std::vector<CES::CES_XY>* xy, float scale)
                            {
                                if (xy == nullptr || xy->size() < 3)
                                    return -1;

                                // Calculate heavypoint
                                double cx = 0.0, cy = 0.0;
                                for (const auto& p : *xy) {
                                    cx += p.x;
                                    cy += p.y;
                                }
                                cx /= xy->size();
                                cy /= xy->size();

                                // Scale points
                                struct P { CES::CES_XY p; double ang; };

                                std::vector<P> pts;
                                pts.reserve(xy->size());

                                for (const auto& p : *xy) {
                                    CES::CES_XY s = p;
                                    s.x = static_cast<int>(std::lround((p.x - cx) * scale + cx));
                                    s.y = static_cast<int>(std::lround((p.y - cy) * scale + cy));

                                    double ang = std::atan2(
                                        static_cast<double>(s.y) - cy,
                                        static_cast<double>(s.x) - cx
                                    );

                                    pts.push_back({ s, ang });
                                }

                                // Sort after angle (curve parameter)
                                std::sort(pts.begin(), pts.end(),
                                    [](const P& a, const P& b) { return a.ang < b.ang; });

                                std::vector<CES::CES_XY> result;
                                constexpr double MAX_DIST = 1.0;

                                auto densify = [&](const CES::CES_XY& a, const CES::CES_XY& b)
                                {
                                    result.push_back(a);

                                    double dx = b.x - a.x;
                                    double dy = b.y - a.y;
                                    double d = std::hypot(dx, dy);

                                    int steps = static_cast<int>(std::ceil(d / MAX_DIST));
                                    for (int i = 1; i < steps; ++i) {
                                        double t = static_cast<double>(i) / steps;
                                        CES::CES_XY p = a;
                                        p.x = static_cast<int>(std::lround(a.x + t * dx));
                                        p.y = static_cast<int>(std::lround(a.y + t * dy));
                                        result.push_back(p);
                                    }
                                };

                                // curve resample
                                for (size_t i = 0; i < pts.size(); ++i) {
                                    const auto& a = pts[i].p;
                                    const auto& b = pts[(i + 1) % pts.size()].p; // closed.
                                    densify(a, b);
                                }

                                xy->swap(result);
                                return 0;
                            }

                            /*
                            *
                            * Not tested yet.
                            * 
                            */


                            bool collision(vector<CES::CES_XY>* x, vector<CES::CES_XY>* y) {
                                unordered_set<CES::CES_XY, CES::CES_XY::CES_XY_Hash> xyz(y->begin(), y->end());
                                for (auto& c : *x) {
                                    if (xyz.count(c)) return true;
                                }
                                return false;
                            }
                    };
            };
    #endif

};