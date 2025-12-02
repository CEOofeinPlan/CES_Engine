#define CES_CELL_SYSTEM
#define CES_GEOMETRY_UNIT
#define MINIAUDIO_IMPLEMENTATION

#include "CES_Engine_TMP.hpp"
#include <thread>
#include <chrono>
#include <unordered_set>

using namespace std;

int main() {
    CES::CES_Screen sys;
    CES::CES_Shapes::CES_Polygon polygon;
    
    CES::CES_XY pt1;
    CES::CES_XY pt2;
    CES::CES_XY pt3;
    CES::CES_XY pt4;
    CES::CES_XY pt5;

    pt1.x = 10, pt1.y = 10;
    pt2.x = 15, pt2.y = 15;
    pt3.x = 12, pt3.y = 20;
    pt4.x = 8, pt4.y = 20;
    pt5.x = 5, pt5.y = 15;

    unordered_set<CES::CES_XY, CES::CES_XY::CES_XY_Hash> pt_hold = {pt1, pt2, pt3, pt4, pt5};

    CES::CES_COLOR color1(255, 125, 255);
    CES::CES_COLOR color2(0, 255, 255);
    sys.ClearConsole();
    polygon.calculate_polygon(pt_hold, '0', color1);
    sys.pack_load_geometry(polygon.pack_load_system_polygon());
    sys.OutputCurWindow();
    sys.OutputCurWindow();
    int i = 0;
    while (true) {
        i++;
        if (i == 2000) break;
        this_thread::sleep_for(chrono::milliseconds(1));
    }
    return 0;
}