#include <iostream>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <thread>

int main(int argc, char* argv[])
{
    //system(R"(start C:\/"Program Files (x86)/"\Microsoft\Edge\Application\msedge.exe)");
    ShowWindow(GetConsoleWindow(), 0);
    //std::cout << "Hello, World!" << std::endl;
    int x, y  = 0;
    int rate = 150;
    int acc = 5;
    bool isPaused = false;
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        isPaused = (GetKeyState(VK_HOME) & 0x8000) != 0;
        rate = (GetKeyState(VK_F12) & 0x8000) != 0 ? rate += acc : rate;
        rate = (GetKeyState(VK_F11) & 0x8000) != 0 ? rate -= acc : rate;
        if(GetKeyState(VK_END)  & 0x8000 ){
            break;
        }
        if(GetKeyState(VK_LBUTTON) & 0x8000) {
            if (!isPaused) {
                mouse_event(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK,
                            x,
                            y,
                            0,
                            GetMessageExtraInfo());

                y += rate;
            }
        }
    }

    return 0;
}
