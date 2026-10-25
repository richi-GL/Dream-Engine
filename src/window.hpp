#pragma once

#include <SDL2/SDL.h>
#include <iostream>

#include "defmap.hpp"

namespace Dream
{
    class Window
    {
        public:
            int CreateWindow();
            int CreateRenderer();
            int GameLoop();

           
        
        private:
            void Destroy();
            SDL_Window* m_Window = nullptr;
            SDL_Renderer* m_Renderer = nullptr;
            bool running = true;
            SDL_Event event;

    };

}