#include <iostream>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <thread>


int main(int argc, char* argv[])
{
    ShowWindow(GetConsoleWindow(), 0);
    //std::cout << "Hello, World!" << std::endl;
    int x, y  = 0;
    bool isPaused = false;
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        isPaused = (GetAsyncKeyState(VK_HOME)) != 0;
        if(GetAsyncKeyState(VK_END)){
            break;
        }
        if(GetAsyncKeyState(VK_LBUTTON)) {
            if (!isPaused) {
                mouse_event(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK,
                            x,
                            y,
                            0,
                            GetMessageExtraInfo());

                y += 150;
            }
        }
    }

    return 0;
}
