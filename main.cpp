#include <iostream>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

int main(int argc, char* argv[])
{
    std::cout << "Hello, World!" << std::endl;
    const int a = 500;
    mouse_event(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK,
                a,
                a,
                0,
                GetMessageExtraInfo());


    std::cin.get();

    return 0;
}
