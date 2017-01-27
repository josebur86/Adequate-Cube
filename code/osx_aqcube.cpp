#include "SDL2/SDL.h"

#include <assert.h>
#include <stdio.h>

#include "aqcube_platform.h"

static void OSX_RenderGradient(void *Pixels, int Width, int Height)
{
    assert(Pixels);

    uint *Pixel = (uint *)Pixels;
    for (int HeightIndex = 0; HeightIndex < Height; ++HeightIndex)
    {
        for (int WidthIndex = 0; WidthIndex < Width; ++WidthIndex)
        {
            uint8 R = (uint8)WidthIndex;
            uint8 G = (uint8)HeightIndex;
            uint8 B = (uint8)WidthIndex + 256 + (uint)HeightIndex + 128;
            uint Color = (R << 16) | (G << 8) | (B << 0);

            *Pixel++ = Color;
        }
    }
}

int main(int argc, char** argv)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        printf("Unable to initialize SDL!\n");
        return 1;
    }

    SDL_Window *Window = SDL_CreateWindow("Adequate Cube", 
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          800, 600,
                                          0);
    if (Window)
    {
        SDL_Surface *Surface = SDL_GetWindowSurface(Window);
        if (Surface)
        {
            // TODO(joe): Allocate game memory.
            // TODO(joe): Lock that update loop to 30 fps.
            // TODO(joe): Compile the game code into it's on .so file.

            bool GlobalRunning = true;
            while (GlobalRunning)
            {
                SDL_Event Event;
                while (SDL_PollEvent(&Event))
                {
                    if (Event.type == SDL_QUIT)
                    {
                        GlobalRunning = false;
                    }
                }

                OSX_RenderGradient(Surface->pixels, Surface->w, Surface->h);

                SDL_UpdateWindowSurface(Window);
            }
        }
        else
        {
            printf("Unable to get the window surface.");
        }

        SDL_DestroyWindow(Window);
    }
    else
    {
        printf("Unable to create window.");
    }

    return 0;
}
