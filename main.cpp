#define CES_CELL_SYSTEM
#define CES_GEOMETRY_UNIT
#define CES_INPUT_UNIT
#include "CES_Engine.hpp"

int main() {
    try {
        CES::CES_Screen sys;
        CES::CES_XY pt1(20,20,2,CES::CES_COLOR(255,155,255), '1');
        CES::CES_XY pt2(40,20,2,CES::CES_COLOR(255,155,255), '1');
        CES::CES_XY pt3(40,40,2,CES::CES_COLOR(255,155,255), '1');
        CES::CES_XY pt4(20,40,2,CES::CES_COLOR(255,155,255), '1');
        CES::ThreadPool thr(2);
        CES::CES_Input input;
        CES::CES_Shapes::CES_Polygon qdt1(2);
        CES::CES_Shapes::CES_Transformation trans;

        qdt1.calculate_polygon({pt1,pt2,pt3,pt4}, '0', CES::CES_COLOR(255,255,255));
        trans.fill_shape(qdt1.pack_load_system_polygon(), CES::CES_COLOR(255,155,255), '1');
        sys.writeCell(qdt1.pack_load_system_polygon());

        bool run = true;

        thr.start_persistent([&run, &input](atomic_bool& running) {
            if (input.isDown(CES::ALT) && input.isDown(CES::F5)) run = false;
            this_thread::sleep_for(chrono::milliseconds(16));
        });

        thr.start_persistent([&run, &input](atomic_bool& running) {
            input.update();
            this_thread::sleep_for(chrono::milliseconds(16));
        });

        thr.start_persistent([&input, &sys, &pt1, &pt2, &pt3, &pt4, &qdt1, &trans](atomic_bool& running) { 
            
            CES::CES_Shapes::CES_Polygon qdt1_cpy = qdt1;
            int x = 0, y = 0;
            if (input.isDown(CES::D)) x++;
            if (input.isDown(CES::A)) x--;
            if (input.isDown(CES::W)) y--;
            if (input.isDown(CES::S)) y++;
            if (x != 0 || y != 0) {
                pt1.x += x, pt2.x += x, pt3.x += x, pt4.x += x;
                pt1.y += y, pt2.y += y, pt3.y += y, pt4.y += y;
                qdt1.calculate_polygon({pt1,pt2,pt3,pt4}, '0', CES::CES_COLOR(255,255,255));
                trans.fill_shape(qdt1.pack_load_system_polygon(), CES::CES_COLOR(255,155,255), '1');
                sys.writeCell(qdt1.pack_load_system_polygon());
                sys.removeCell(qdt1_cpy.pack_load_system_polygon());
                this_thread::sleep_for(chrono::milliseconds(16));
            }
        });
        while (run) {
            sys.OutputCurWindow();
            this_thread::sleep_for(chrono::milliseconds(16));
        }
    } catch (std::exception& e) {
        ofstream("Debug.txt") << e.what() << endl;
    }

    return 0;
}

/*
*
*   Für Linux den Teil des Renders so lassen.
*   Windows muss allerdings seinen eigenen nativen Console API bekommen, ansonsten zu INSTABIL.
*   
*
*/