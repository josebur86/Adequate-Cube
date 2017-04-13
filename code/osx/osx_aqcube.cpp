#include "include/SDL2/SDL.h"

#include <assert.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "../aqcube_platform.h" // TODO(joe): I'm not sure if I like that this is needed.
#include "../aqcube_platform.cpp"
#include "../aqcube.h"

u64 GlobalTimerFreq;
bool GlobalRunning = true;

//
// File Read
//
static read_file_result DEBUGOSXReadFile(char *FileName)
{
    read_file_result Result = {};

    s32 File = open(FileName, O_RDONLY);
    if (File != -1)
    {
        struct stat FileStat = {};
        s32 StatResult = fstat(File, &FileStat);
        Assert(StatResult == 0);

        Result.SizeInBytes = FileStat.st_size;
        Result.Contents = mmap(0, 
                               Result.SizeInBytes, 
                               PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
        ssize_t BytesRead = read(File, Result.Contents, Result.SizeInBytes);
        Assert(BytesRead == Result.SizeInBytes);

        close(File);
    }

    return Result;
}

//
// Font Init
//

static font_data OSXInitFont(char *FontFileName)
{
    font_data Result = {};

    read_file_result FontFile = DEBUGOSXReadFile(FontFileName);
    if (FontFile.Contents)
    {
        Result.IsLoaded = true;
        stbtt_InitFont(&Result.FontInfo, 
                       (u8 *)FontFile.Contents, 
                       stbtt_GetFontOffsetForIndex((u8 *)FontFile.Contents, 0));

        Result.Scale = stbtt_ScaleForPixelHeight(&Result.FontInfo, 80);
        stbtt_GetFontVMetrics(&Result.FontInfo, &Result.Ascent, &Result.Descent, &Result.LineGap);
    }
    
    // TODO(joe): Close the read file result.

    return Result;
}

///
/// Controller Input
///

static SDL_GameController *OSX_InitController()
{
    SDL_GameController *Result;
    printf("Controllers...\n");
    for (int ControllerIndex = 0; ControllerIndex < SDL_NumJoysticks(); ++ControllerIndex)
    {
        if (SDL_IsGameController(ControllerIndex))
        {
            printf("%s\n", SDL_GameControllerNameForIndex(ControllerIndex));
            Result = SDL_GameControllerOpen(ControllerIndex);
            assert(Result);
            break;
        }
    }

    return Result;
}

static void OSX_ProcessButtonState(controller_button_state *Button, bool IsDown)
{
    assert(Button->IsDown != IsDown);
    Button->IsDown = IsDown;
}

static void OSX_ProcessGameController(SDL_GameController *Controller, game_input *Input)
{
    // TODO(joe): Get analog stick data.
    game_controller *AnalogController = Input->Controllers + 1;
    AnalogController->IsAnalog = true;

    if (Controller)
    {
        AnalogController->IsConnected = true;

        bool IsDown = (SDL_GameControllerGetButton(Controller, SDL_CONTROLLER_BUTTON_DPAD_UP) == 1);
        if (AnalogController->Up.IsDown != IsDown)
        {
            OSX_ProcessButtonState(&AnalogController->Up, IsDown);
        }

        IsDown = (SDL_GameControllerGetButton(Controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN) == 1);
        if (AnalogController->Down.IsDown != IsDown)
        {
            OSX_ProcessButtonState(&AnalogController->Down, IsDown);
        }

        IsDown = (SDL_GameControllerGetButton(Controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT) == 1);
        if (AnalogController->Left.IsDown != IsDown)
        {
            OSX_ProcessButtonState(&AnalogController->Left, IsDown);
        }

        IsDown = (SDL_GameControllerGetButton(Controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) == 1);
        if (AnalogController->Right.IsDown != IsDown)
        {
            OSX_ProcessButtonState(&AnalogController->Right, IsDown);
        }
    }
}

static void OSX_ProcessInput(game_input *Input)
{
    game_controller *KeyboardController = Input->Controllers;
    KeyboardController->IsConnected = true;

    SDL_Event Event;
    while (SDL_PollEvent(&Event))
    {
        switch (Event.type)
        {
        case SDL_QUIT:
        {
            GlobalRunning = false;
        }
        break;

#if 1
        case SDL_KEYDOWN:
        {
            if (!Event.key.repeat)
            {
                SDL_Keycode KeyCode = Event.key.keysym.sym;
                if (KeyCode == SDLK_w)
                {
                    OSX_ProcessButtonState(&KeyboardController->Up, true);
                }
                else if (KeyCode == SDLK_s)
                {
                    OSX_ProcessButtonState(&KeyboardController->Down, true);
                }
                else if (KeyCode == SDLK_a)
                {
                    OSX_ProcessButtonState(&KeyboardController->Left, true);
                }
                else if (KeyCode == SDLK_d)
                {
                    OSX_ProcessButtonState(&KeyboardController->Right, true);
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
                    OSX_ProcessButtonState(&KeyboardController->Up, false);
                }
                else if (KeyCode == SDLK_s)
                {
                    OSX_ProcessButtonState(&KeyboardController->Down, false);
                }
                else if (KeyCode == SDLK_a)
                {
                    OSX_ProcessButtonState(&KeyboardController->Left, false);
                }
                else if (KeyCode == SDLK_d)
                {
                    OSX_ProcessButtonState(&KeyboardController->Right, false);
                }
            }
        }
        break;
#endif
        }
    }
}

static float OSX_GetElapsedSeconds(u64 Start, u64 End)
{
    float Result = ((float)(End - Start)) / (float)GlobalTimerFreq;

    return Result;
}

struct game_code
{
    void *Handle;
    int WriteTime;

    update_game_and_render *UpdateGameAndRender;

    bool IsValid;
};

static int OSX_GetLastWriteTime(const char *FileName)
{
    int Result = 0;

    struct stat FileData = {};
    stat(FileName, &FileData);
    Result = FileData.st_mtimespec.tv_sec;

    return Result;
}

static game_code OSX_LoadGameCode(const char *GameFileName)
{
    game_code Result = {};

    Result.Handle = dlopen(GameFileName, RTLD_NOW | RTLD_LOCAL);
    if (Result.Handle)
    {
        Result.WriteTime           = OSX_GetLastWriteTime(GameFileName);
        Result.UpdateGameAndRender = (update_game_and_render *)dlsym(Result.Handle, "UpdateGameAndRender");

        Result.IsValid = (Result.UpdateGameAndRender != 0);
    }

    return Result;
}

static void OSX_FreeGameCode(game_code *Game)
{
    if (Game->Handle)
    {
        dlclose(Game->Handle);
    }

    Game->Handle              = 0;
    Game->UpdateGameAndRender = 0;
    Game->IsValid             = false;
}

int main(int argc, char **argv)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0)
    {
        printf("Unable to initialize SDL!\n");
        return 1;
    }

    SDL_GameController *Controller = OSX_InitController();
    GlobalDebugFont = OSXInitFont("/Library/Fonts/Arial.ttf");

    GlobalTimerFreq = SDL_GetPerformanceFrequency();

    SDL_Window *Window
        = SDL_CreateWindow("Adequate Cube", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600, 0);
    if (Window)
    {
        SDL_Surface *Surface = SDL_GetWindowSurface(Window);
        if (Surface)
        {
            char GameFileName[] = "../build/libaqcube.so";
            game_code Game      = OSX_LoadGameCode(GameFileName);

            game_memory Memory          = {};
            Memory.PermanentStorageSize = Megabytes(64);
            Memory.PermanentStorage
                = mmap(0, Memory.PermanentStorageSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
            Memory.TransientStorageSize = Gigabytes((u64)4);
            Memory.TransientStorage
                = mmap(0, Memory.TransientStorageSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

            Memory.DEBUGLoadBitmap = DEBUGLoadBitmap;
            Memory.DEBUGLoadFontGlyph = DEBUGLoadFontGlyph;
            Memory.DEBUGGetFontKernAdvanceFor = DEBUGGetFontKernAdvanceFor;

            if (Memory.PermanentStorage && Memory.TransientStorage)
            {
                game_back_buffer BackBuffer = {};
                BackBuffer.Memory           = Surface->pixels;
                BackBuffer.Width            = Surface->w;
                BackBuffer.Height           = Surface->h;
                BackBuffer.Pitch            = Surface->pitch;
                BackBuffer.BytesPerPixel    = Surface->format->BytesPerPixel;

                int MonitorHz            = 60;
                int GameUpdateHz         = MonitorHz / 2;
                float TargetFrameSeconds = 1.0f / (float)GameUpdateHz;

                game_input Input = {};

                u64 LastFrameCount = SDL_GetPerformanceCounter();
                while (GlobalRunning)
                {
                    if (Game.WriteTime != OSX_GetLastWriteTime(GameFileName))
                    {
                        OSX_FreeGameCode(&Game);
                        Game = OSX_LoadGameCode(GameFileName);
                    }
                    assert(Game.IsValid);

                    OSX_ProcessInput(&Input);
                    OSX_ProcessGameController(Controller, &Input);

                    Input.dt = TargetFrameSeconds;

                    if (Game.UpdateGameAndRender)
                    {
                        Game.UpdateGameAndRender(&Memory, &BackBuffer, &Input);
                    }

                    u64 FrameCount = SDL_GetPerformanceCounter();
                    float ElapsedTime = OSX_GetElapsedSeconds(LastFrameCount, FrameCount);
                    if (ElapsedTime < TargetFrameSeconds)
                    {
                        u32 TimeToSleep = (u32)(1000.0f * (TargetFrameSeconds - ElapsedTime));
                        if (TimeToSleep > 0)
                        {
                            SDL_Delay(TimeToSleep);
                        }

                        ElapsedTime = OSX_GetElapsedSeconds(LastFrameCount, SDL_GetPerformanceCounter());
                        while (ElapsedTime < TargetFrameSeconds)
                        {
                            ElapsedTime = OSX_GetElapsedSeconds(LastFrameCount, SDL_GetPerformanceCounter());
                        }
                    }
                    else
                    {
                        // TODO(joe): Log that we missed a frame.
                    }

                    u64 EndCount = SDL_GetPerformanceCounter();
#if 0
                    float MSPerFrame = 1000.0f * OSX_GetElapsedSeconds(LastFrameCount, EndCount);
                    float FPS = 1000.0f / MSPerFrame;
                    printf("ms/f: %.2f f/s: %.2f \n", MSPerFrame, FPS);
#endif
                    LastFrameCount = EndCount;

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
