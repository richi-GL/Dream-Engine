#include "window.hpp"

using namespace Dream;

int main()
{
    Window window;
    window.CreateWindow();
    window.CreateRenderer();
    window.GameLoop();
    return 0;
}