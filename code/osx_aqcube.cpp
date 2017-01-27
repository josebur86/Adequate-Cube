#include "SDL2/SDL.h"

#include <assert.h>
#include <stdio.h>

#include "aqcube_platform.h"

static void OSX_RenderGradient(void *Pixels, int Width, int Height)
{
    assert(Pixels);

    int *Pixel = (int *)Pixels;
    for (int HeightIndex = 0; HeightIndex < Height; ++HeightIndex)
    {
        for (int WidthIndex = 0; WidthIndex < Width; ++WidthIndex)
        {
            int8 R = (int8)WidthIndex;
            int8 G = (int8)HeightIndex;
            int8 B = (int8)WidthIndex+HeightIndex;
            int Color = (R << 16) | (G << 8) | (B << 0);

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

    int NumberOfDisplays = SDL_GetNumVideoDisplays();
    printf("Number of Displays: %i\n", NumberOfDisplays);

    SDL_Window *Window = SDL_CreateWindow("Adequate Cube", 
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          800, 600,
                                          0);
    if (Window)
    {
        printf("Window created!\n");

        // TODO(joe): Allocate video memory.
        SDL_Surface *Surface = SDL_GetWindowSurface(Window);
        if (Surface)
        {
            SDL_PixelFormat *Format = Surface->format;
            printf("W: %i H: %i BPP: %i\n", Surface->w, Surface->h, Format->BitsPerPixel);

            // TODO(joe): Allocate game memory.
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

        SDL_DestroyWindow(Window);
    }

    return 0;
}
