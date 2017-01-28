#include "SDL2/SDL.h"

#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>

#include "aqcube_platform.h"

bool GlobalRunning = true;

int GlobalX = 0;
int GlobalY = 0;

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
            uint8 B = (uint8)WidthIndex + GlobalX + (uint)HeightIndex + GlobalY;
            uint Color = (R << 16) | (G << 8) | (B << 0);

            *Pixel++ = Color;
        }
    }
}

static void OSX_ProcessButtonState(button_state *Button, bool IsDown)
{
    assert(Button->IsDown != IsDown);
    Button->IsDown = IsDown;
}

static void OSX_ProcessInput(game_controller_input *Input)
{
    SDL_Event Event;
    while (SDL_PollEvent(&Event))
    {
        switch(Event.type)
        {
            case SDL_QUIT:
            {
                GlobalRunning = false;
            } break;
            case SDL_KEYDOWN:
            {
                if (!Event.key.repeat)
                {
                    SDL_Keycode KeyCode = Event.key.keysym.sym;
                    if (KeyCode == SDLK_w)
                    {
                        OSX_ProcessButtonState(&Input->Up, true);
                    }
                    else if (KeyCode == SDLK_s)
                    {
                        OSX_ProcessButtonState(&Input->Down, true);
                    }
                    else if (KeyCode == SDLK_a)
                    {
                        OSX_ProcessButtonState(&Input->Left, true);
                    }
                    else if (KeyCode == SDLK_d)
                    {
                        OSX_ProcessButtonState(&Input->Right, true);
                    }
                }
            } break;
            case SDL_KEYUP:
            {
                if (!Event.key.repeat)
                {
                    SDL_Keycode KeyCode = Event.key.keysym.sym;
                    if (KeyCode == SDLK_w)
                    {
                        OSX_ProcessButtonState(&Input->Up, false);
                    }
                    else if (KeyCode == SDLK_s)
                    {
                        OSX_ProcessButtonState(&Input->Down, false);
                    }
                    else if (KeyCode == SDLK_a)
                    {
                        OSX_ProcessButtonState(&Input->Left, false);
                    }
                    else if (KeyCode == SDLK_d)
                    {
                        OSX_ProcessButtonState(&Input->Right, false);
                    }
                }
            } break;
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
            // TODO(joe): Lock that update loop to 30 fps.
            
            // TODO(joe): Compile the game code into it's on .so file.
            
            game_memory Memory = {};
            Memory.PermanentStorageSize = Megabytes(64);
            Memory.PermanentStorage = mmap(0, Memory.PermanentStorageSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
            Memory.TransientStorageSize = Gigabytes((uint64)4);
            Memory.TransientStorage = mmap(0, Memory.TransientStorageSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

            if (Memory.PermanentStorage && Memory.TransientStorage)
            {
                game_controller_input Input = {}; 

                while (GlobalRunning)
                {
                    OSX_ProcessInput(&Input);
                    if (Input.Up.IsDown)
                    {
                        GlobalY -= 10;
                    }
                    if (Input.Down.IsDown)
                    {
                        GlobalY += 10;
                    }
                    if (Input.Left.IsDown)
                    {
                        GlobalX -= 10;
                    }
                    if (Input.Right.IsDown)
                    {
                        GlobalX += 10;
                    }

                    OSX_RenderGradient(Surface->pixels, Surface->w, Surface->h);

                    SDL_UpdateWindowSurface(Window);
                }
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
