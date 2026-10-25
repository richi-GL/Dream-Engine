#include <SDL2/SDL.h>
#include <iostream>

#include "window.hpp"
#include "defmap.hpp"

namespace Dream
{
    int Window::CreateWindow()
    {
        if (SDL_Init(SDL_INIT_VIDEO) < 0)
        {
            std::cerr << "Error al inicializar SDL: "
                    << SDL_GetError() << '\n';
            return 1;
        }

        m_Window = SDL_CreateWindow(
            "Dream",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            Dream::ScreenWidth,
            Dream::ScreenHeight,
            SDL_WINDOW_SHOWN
        );

        if (!m_Window)
        {
            std::cerr << "Error al crear ventana: "
                    << SDL_GetError() << '\n';
            SDL_Quit();
            return 1;
        }
        
        return 0;
        
    }

    int Window::CreateRenderer()
    {
        if (!m_Window)
        {
            SDL_Log("No se puede crear el renderer sin una ventana");
            return -1;
        }

        m_Renderer = SDL_CreateRenderer(
            m_Window,
            -1,
            SDL_RENDERER_ACCELERATED
        );

        if (!m_Renderer)
        {
            SDL_Log("Error creando renderer: %s", SDL_GetError());
            return -1;
        }

        return 0;
    }

    
    void Window::Destroy()
    {
        if (m_Renderer)
        {
            SDL_DestroyRenderer(m_Renderer);
            m_Renderer = nullptr;
        }

        if (m_Window)
        {
            SDL_DestroyWindow(m_Window);
            m_Window = nullptr;
        }
    }
}