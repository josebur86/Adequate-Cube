#include "SDL2/SDL.h"

#include <stdio.h>

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
    }

    while (1)
    {
    }

    SDL_DestroyWindow(Window);
    return 0;
}
