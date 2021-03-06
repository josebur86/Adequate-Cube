#define DIRECTINPUT_VERSION 0x0800
#define INITGUID 1
#include <dinput.h>
#include <dsound.h>
#include <windows.h>

#include <stdio.h>

#include <glad/glad.h>
#include "third_party/GLAD/src/glad.c"

#include "aqcube.h"
#include "aqcube_math.h"
#include "aqcube_renderer.h"
#include "aqcube_platform.h"
#include "aqcube_platform.cpp"
#include "aqcube_software_renderer.cpp"
#include "aqcube_controller.h"

read_file_result DEBUGWin32ReadFile(char *FileName);
static font_data Win32InitFont(char *FontFileName)
{
    font_data Result = {};

    read_file_result FontFile = DEBUGWin32ReadFile(FontFileName);
    if (FontFile.Contents)
    {
        Result.IsLoaded = true;
        stbtt_InitFont(&Result.FontInfo, (u8 *)FontFile.Contents, stbtt_GetFontOffsetForIndex((u8 *)FontFile.Contents, 0));

        r32 FontScale = 20.0f;
        Result.Scale = stbtt_ScaleForPixelHeight(&Result.FontInfo, FontScale);
        stbtt_GetFontVMetrics(&Result.FontInfo, &Result.Ascent, &Result.Descent, &Result.LineGap);
    }

    return Result;
}

struct win32_back_buffer
{
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
};

struct win32_sound_output
{
    DWORD BytesPerSample;
    WORD SamplesPerSec;
    DWORD BufferSize;
    DWORD SafetyBytes;

    u32 RunningSamples;
    int LatencySampleCount;
    s16 ToneVolume;
};

read_file_result DEBUGWin32ReadFile(char *Filename)
{
    read_file_result Result = {};

    HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (FileHandle)
    {
        LARGE_INTEGER FileSize;
        if (GetFileSizeEx(FileHandle, &FileSize))
        {
            DWORD FileSize32 = (DWORD)FileSize.QuadPart; // TODO(joe): Safe truncation?
            Result.Contents = VirtualAlloc(0, FileSize32, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if (Result.Contents)
            {
                DWORD BytesRead = 0;
                ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0);
                if (BytesRead == FileSize32)
                {
                    Result.SizeInBytes = FileSize32;
                }
                else
                {
                    Win32FreeMemory(Result.Contents);
                }
            }
        }

        CloseHandle(FileHandle);
    }

    return Result;
}

bool DEBUGWin32WriteFile(char *Filename, void *Memory, u32 FileSize)
{
    bool Result = false;
    HANDLE FileHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (FileHandle)
    {
        DWORD BytesWritten = 0;
        if (WriteFile(FileHandle, Memory, FileSize, &BytesWritten, 0) && BytesWritten == FileSize)
        {
            Result = true;
        }

        CloseHandle(FileHandle);
    }

    return Result;
}

void Win32FreeMemory(void *Memory)
{
    if (Memory)
    {
        VirtualFree(Memory, 0, MEM_RELEASE);
        Memory = 0;
    }
}

static bool GlobalRunning = true;

static win32_back_buffer GlobalBackBuffer;
static LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
static LARGE_INTEGER GlobalPerfFrequencyCount;

static vector2 Win32GetWindowSize(HWND Window)
{
    RECT Rect;
    GetClientRect(Window, &Rect);

    vector2 Result = V2(Rect.right - Rect.left, Rect.bottom - Rect.top);
    return Result;
}

static void Win32ResizeBackBuffer(win32_back_buffer *Buffer, vector2 ClientSize)
{
    // TODO(joe): There is some concern that the VirtualAlloc could fail which
    // would leave us without a buffer. See if there's a better way to handle
    // this memory reallocation.
    if (Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }

    Buffer->Width = (s32)ClientSize.X;
    Buffer->Height = (s32)ClientSize.Y;

    Buffer->BytesPerPixel = 4;

    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = Buffer->Height;
    // TODO(joe): Treat the buffer as top-down for now. It might be better to
    // treat the back buffer as bottom up in the future.
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;

    // Uncompressed: The value for blue is in the least significant 8
    // bits, followed by 8 bits each for green and red.
    // BB GG RR XX
    Buffer->Info.bmiHeader.biCompression = BI_RGB;
    Buffer->Pitch = Buffer->Width * Buffer->BytesPerPixel;
    int BufferMemorySize = Buffer->Height * Buffer->Width * Buffer->BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BufferMemorySize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}
static void Win32PaintBackBuffer(HDC DeviceContext, win32_back_buffer *BackBuffer)
{
    int OffsetX = 10;
    int OffsetY = 10;

    // Note(joe): For right now, I'm just going to blit the buffer as-is without any stretching.
    StretchDIBits(DeviceContext, OffsetX, OffsetY, BackBuffer->Width, BackBuffer->Height, // Destination
                  0, 0, BackBuffer->Width, BackBuffer->Height, // Source
                  BackBuffer->Memory, &BackBuffer->Info, DIB_RGB_COLORS, SRCCOPY);
}

static WINDOWPLACEMENT GlobalLastWindowPlacement;

static void Win32ToggleFullScreen(HWND Window)
{
    DWORD dwStyle = GetWindowLong(Window, GWL_STYLE);
    if (dwStyle & WS_OVERLAPPEDWINDOW)
    {
        MONITORINFO MonitorInfo = { sizeof(MonitorInfo) };
        if (GetWindowPlacement(Window, &GlobalLastWindowPlacement)
            && GetMonitorInfo(MonitorFromWindow(Window, MONITOR_DEFAULTTOPRIMARY), &MonitorInfo))
        {
            SetWindowLong(Window, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(Window, HWND_TOP, MonitorInfo.rcMonitor.left, MonitorInfo.rcMonitor.top,
                         MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left,
                         MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    }
    else
    {
        SetWindowLong(Window, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(Window, &GlobalLastWindowPlacement);
        SetWindowPos(Window, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

static void InitSound(HWND Window, DWORD SamplesPerSec, DWORD BufferSize)
{
    WORD Channels = 2;
    WORD BitsPerSample = 16;
    WORD BlockAlign = Channels * (BitsPerSample / 8);
    DWORD AvgBytesPerSec = SamplesPerSec * BlockAlign;

    // Create the primary buffer
    // Note(joe): This call can fail if there isn't an audio device at startup.
    LPDIRECTSOUND DirectSound;
    if (DirectSoundCreate(NULL, &DirectSound, NULL) == DS_OK)
    {
        if (DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY) == DS_OK)
        {
            DSBUFFERDESC PrimaryBufferDescription = {};
            PrimaryBufferDescription.dwSize = sizeof(PrimaryBufferDescription);
            PrimaryBufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

            LPDIRECTSOUNDBUFFER PrimaryBuffer;
            if (DirectSound->CreateSoundBuffer(&PrimaryBufferDescription, &PrimaryBuffer, 0) == DS_OK)
            {
                OutputDebugStringA("Created the primary buffer.\n");

                WAVEFORMATEX Format = {};
                Format.wFormatTag = WAVE_FORMAT_PCM;
                Format.nChannels = Channels;
                Format.wBitsPerSample = BitsPerSample;
                Format.nSamplesPerSec = SamplesPerSec;
                Format.nBlockAlign = BlockAlign;
                Format.nAvgBytesPerSec = AvgBytesPerSec;

                PrimaryBuffer->SetFormat(&Format);

                DSBUFFERDESC SecondaryBufferDescription = {};
                SecondaryBufferDescription.dwSize = sizeof(SecondaryBufferDescription);
                // SecondaryBufferDescription.dwFlags = 0;
                SecondaryBufferDescription.dwBufferBytes = BufferSize;
                SecondaryBufferDescription.lpwfxFormat = &Format;

                HRESULT SecondaryBufferResult
                    = DirectSound->CreateSoundBuffer(&SecondaryBufferDescription, &GlobalSecondaryBuffer, 0);
                if (SecondaryBufferResult == DS_OK)
                {
                    GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
                    OutputDebugStringA("Created the secondary buffer.\n");
                }
                else
                {
                    // TODO(joe): Diagnostics for when we can't create the primary buffer.
                }
            }
            else
            {
                // TODO(joe): Diagnostics for when we can't create the primary buffer.
            }
        }
        else
        {
            // TODO(joe): Handle failing to set the cooperative level.
        }
    }
    else
    {
        // TODO(joe): Handle when the sound cannot be initialized.
    }
}

static void Win32WriteToSoundBuffer(game_sound_buffer *SoundBuffer, win32_sound_output *SoundOutput, DWORD ByteToLock,
                                    DWORD BytesToWrite)
{
    void *AudioPointer1 = 0;
    DWORD AudioBytes1 = 0;
    void *AudioPointer2 = 0;
    DWORD AudioBytes2 = 0;
    Assert((AudioBytes1 / SoundOutput->BytesPerSample) == 0);
    Assert((AudioBytes2 / SoundOutput->BytesPerSample) == 0);
    if (GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite, &AudioPointer1, &AudioBytes1, &AudioPointer2,
                                    &AudioBytes2, 0)
        == DS_OK)
    {
        s16 *Samples = SoundBuffer->Samples;

        s16 *Sample = (s16 *)AudioPointer1;
        int SamplesToWrite1 = AudioBytes1 / SoundOutput->BytesPerSample;
        for (int SampleIndex = 0; SampleIndex < SamplesToWrite1; ++SampleIndex)
        {
            *Sample++ = *Samples++; // Left
            *Sample++ = *Samples++; // Right

            ++SoundOutput->RunningSamples;
        }
        Sample = (s16 *)AudioPointer2;
        int SamplesToWrite2 = AudioBytes2 / SoundOutput->BytesPerSample;
        for (int SampleIndex = 0; SampleIndex < SamplesToWrite2; ++SampleIndex)
        {
            *Sample++ = *Samples++; // Left
            *Sample++ = *Samples++; // Right

            ++SoundOutput->RunningSamples;
        }

        GlobalSecondaryBuffer->Unlock(AudioPointer1, AudioBytes1, AudioPointer2, AudioBytes2);
    }
}

inline static LARGE_INTEGER Win32GetClock()
{
    LARGE_INTEGER Result;
    QueryPerformanceCounter(&Result);
    return Result;
}

inline static float Win32GetElapsedSeconds(LARGE_INTEGER Start, LARGE_INTEGER End)
{
    float Result = ((float)(End.QuadPart - Start.QuadPart)) / (float)GlobalPerfFrequencyCount.QuadPart;
    return Result;
}

struct win32_game_code
{
    HMODULE GameDLL;
    FILETIME LastWriteTime;
    char *GameDLLFileName;

    update_game_and_render *UpdateGameAndRender;
    get_sound_samples *GetSoundSamples;

    bool IsValid;
};

static FILETIME Win32GetLastWriteTime(char *FileName)
{
    FILETIME Result = {};

    WIN32_FILE_ATTRIBUTE_DATA FileAttributes;
    if (GetFileAttributesExA(FileName, GetFileExInfoStandard, &FileAttributes))
    {
        Result = FileAttributes.ftLastWriteTime;
    }

    return Result;
}

static void FreeGameCode(win32_game_code *GameCode)
{
    GameCode->IsValid = false;

    FreeLibrary(GameCode->GameDLL);

    GameCode->GameDLL = 0;
    GameCode->UpdateGameAndRender = 0;
    GameCode->GetSoundSamples = 0;
}

static win32_game_code Win32LoadGameCode(char *GameFile, char *TempGameFile, char *LockFile)
{
    win32_game_code Result = {};

    // NOTE(joe): Make sure that the lock file is gone so we are sure that the
    // DLL has finished building.
    WIN32_FILE_ATTRIBUTE_DATA FileAttributes;
    if (!GetFileAttributesExA(LockFile, GetFileExInfoStandard, &FileAttributes))
    {
        CopyFileA(GameFile, TempGameFile, FALSE);
        HMODULE Handle = LoadLibraryA(TempGameFile);
        if (Handle)
        {
            Result.GameDLLFileName = GameFile;
            Result.GameDLL = Handle;
            Result.LastWriteTime = Win32GetLastWriteTime(GameFile);

            Result.UpdateGameAndRender
                = (update_game_and_render *)GetProcAddress(Result.GameDLL, "UpdateGameAndRender");
            Result.GetSoundSamples = (get_sound_samples *)GetProcAddress(Result.GameDLL, "GetSoundSamples");

            Result.IsValid = Result.UpdateGameAndRender && Result.GetSoundSamples;
        }
    }

    return Result;
}

static LRESULT CALLBACK Win32MainCallWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;

    switch (Message)
    {
    case WM_CLOSE:
    case WM_DESTROY:
    {
        GlobalRunning = false;
    }
    break;
    case WM_PAINT:
    {
        OutputDebugStringA("Paint\n");

        PAINTSTRUCT Paint;
        HDC DeviceContext = BeginPaint(Window, &Paint);

        // Clear the entire client window to black.
        RECT ClientRect;
        GetClientRect(Window, &ClientRect);
        int ClientWidth = ClientRect.right - ClientRect.left;
        int ClientHeight = ClientRect.bottom - ClientRect.top;
        PatBlt(DeviceContext, 0, 0, ClientWidth, ClientHeight, BLACKNESS);

        Win32PaintBackBuffer(DeviceContext, &GlobalBackBuffer);

        EndPaint(Window, &Paint);
    }
    break;
    default:
    {
        // OutputDebugStringA("default\n");
        Result = DefWindowProcA(Window, Message, WParam, LParam);
    }
    break;
    }

    return Result;
}

static void Win32ProcessButtonState(controller_button_state *Button, bool IsDown)
{
    Assert(Button->IsDown != IsDown);
    Button->IsDown = IsDown;
}

static void Win32ProcessPendingMessages(HWND Window, game_controller *KeyboardController)
{
    // Process the message pump.
    MSG Message;
    while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
    {
        switch (Message.message)
        {
        case WM_QUIT:
        {
            GlobalRunning = false;
        }
        break;
        case WM_KEYUP:
        case WM_KEYDOWN:
        {
            u32 KeyCode = (u32)Message.wParam;
            bool IsDown = ((Message.lParam & (1 << 31)) == 0);
            bool WasDown = ((Message.lParam & (1 << 30)) != 0);

            if (IsDown != WasDown)
            {
                if (KeyCode == 'W')
                {
                    Win32ProcessButtonState(&KeyboardController->Up, IsDown);
                }
                else if (KeyCode == 'S')
                {
                    Win32ProcessButtonState(&KeyboardController->Down, IsDown);
                }
                else if (KeyCode == 'A')
                {
                    Win32ProcessButtonState(&KeyboardController->Left, IsDown);
                }
                else if (KeyCode == 'D')
                {
                    Win32ProcessButtonState(&KeyboardController->Right, IsDown);
                }
                else if (KeyCode == VK_RETURN)
                {
                    if (!IsDown && WasDown)
                    {
                        Win32ToggleFullScreen(Window);
                    }
                }
            }
        }
        break;
        default:
        {
            TranslateMessage(&Message);
            DispatchMessageA(&Message);
        }
        break;
        }
    }
}

static r32 NormalizeControllerAxis(s32 Value, s32 Center, s32 DeadZone)
{
    r32 Result = 0.0f;
    if (Abs(Value - Center) > DeadZone)
    {
        Result = (Value - Center) / (r32)Center;
    }

    return Result;
}

static HGLRC InitOpenGL(HDC DeviceContext)
{
    PIXELFORMATDESCRIPTOR PixelFormatDesc = {};
    PixelFormatDesc.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    PixelFormatDesc.nVersion = 1;
    PixelFormatDesc.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    PixelFormatDesc.iPixelType = PFD_TYPE_RGBA;
    PixelFormatDesc.cColorBits = 32;
    //PixelFormatDesc.cDepthBits = ?;
    //PixelFormatDesc.cStencilBits = ?;
    PixelFormatDesc.iLayerType = PFD_MAIN_PLANE;

    int PixelFormat = ChoosePixelFormat(DeviceContext, &PixelFormatDesc);
    Assert(PixelFormat);

    BOOL Result = SetPixelFormat(DeviceContext, PixelFormat, &PixelFormatDesc);
    Assert(Result == TRUE);

    HGLRC GLContext = wglCreateContext(DeviceContext);
    Assert(GLContext);

    Result = wglMakeCurrent(DeviceContext, GLContext);
    Assert(Result == TRUE);

    int LoadGLResult = gladLoadGL();
    Assert(LoadGLResult);

    return GLContext;
}

static void KillOpenGL(HGLRC GLContext)
{
    wglMakeCurrent(0, 0);
    wglDeleteContext(GLContext);
}

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
    Win32LoadDirectInput();
    //GlobalDebugFont = Win32InitFont("C:/Windows/Fonts/SourceCodePro-Regular.ttf");
    GlobalDebugFont = Win32InitFont("C:/Windows/Fonts/arial.ttf");

    WNDCLASSA WindowClass = {};
    WindowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainCallWindowCallback;
    WindowClass.hInstance = Instance;
    WindowClass.hCursor = LoadCursor(0, IDC_ARROW);
    WindowClass.lpszClassName = "AdequateCubeWindowClass";

    if (RegisterClassA(&WindowClass))
    {
        HWND Window = CreateWindowEx(0, WindowClass.lpszClassName, "Adequate Cube", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                     CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, Instance, 0);
        if (Window)
        {
            IDirectInputDevice8 *Controller = Win32InitDirectInputController(Instance, Window);
            HDC DeviceContext = GetDC(Window);
            HGLRC GLContext = InitOpenGL(DeviceContext);

            game_memory Memory = {};
            Memory.PermanentStorageSize = Megabytes(64);
            Memory.PermanentStorage
                = VirtualAlloc(0, Memory.PermanentStorageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            Memory.TransientStorageSize = Gigabytes((u64)4);
            Memory.TransientStorage
                = VirtualAlloc(0, Memory.TransientStorageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

            Memory.DEBUGLoadBitmap = DEBUGLoadBitmap;
            Memory.DEBUGLoadFontGlyph = DEBUGLoadFontGlyph;
            Memory.DEBUGGetFontKernAdvanceFor = DEBUGGetFontKernAdvanceFor;

            Memory.RendererClear = SoftwareRendererClear;
            Memory.RendererDrawBitmap = SoftwareRendererDrawBitmap;

            char GameDLLFileName[] = "../build/aqcube.dll";
            char GameTempDLLFileName[] = "../build/aqcube_temp.dll";
            char GameLockFile[] = "../build/lock.tmp";

            if (Memory.PermanentStorage && Memory.TransientStorage)
            {
                win32_game_code Game = Win32LoadGameCode(GameDLLFileName, GameTempDLLFileName, GameLockFile);
                if (Game.IsValid)
                {
                    QueryPerformanceFrequency(&GlobalPerfFrequencyCount);
#if 1
                    UINT TimerResolutionMS = 1;

                    int MonitorHz = 60;
                    int GameUpdateHz = MonitorHz / 2;
                    float TargetFrameSeconds = 1.0f / (float)GameUpdateHz;

                    bool TimeIsGranular = timeBeginPeriod(TimerResolutionMS) == TIMERR_NOERROR;
#endif

                    vector2 ClientSize = V2(960, 540);
                    Win32ResizeBackBuffer(&GlobalBackBuffer, ClientSize);
                    game_back_buffer BackBuffer = {};
                    BackBuffer.Memory = GlobalBackBuffer.Memory;
                    BackBuffer.Width = GlobalBackBuffer.Width;
                    BackBuffer.Height = GlobalBackBuffer.Height;
                    BackBuffer.Pitch = GlobalBackBuffer.Pitch;
                    BackBuffer.BytesPerPixel = GlobalBackBuffer.BytesPerPixel;
                    software_render_target RenderTarget = {};
                    RenderTarget.Type = RenderTarget_Software;
                    RenderTarget.BackBuffer = &BackBuffer;
                    Memory.RenderTarget = (render_target *)&RenderTarget;

                    bool SoundIsValid = false;
                    win32_sound_output SoundOutput = {};
                    SoundOutput.SamplesPerSec = 44100;
                    SoundOutput.BytesPerSample = 2 * sizeof(s16);
                    SoundOutput.BufferSize = SoundOutput.SamplesPerSec * SoundOutput.BytesPerSample;
                    SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSec / 15;
                    SoundOutput.SafetyBytes
                        = (SoundOutput.SamplesPerSec * SoundOutput.BytesPerSample) / GameUpdateHz / 3;
                    SoundOutput.ToneVolume = 1600;
                    InitSound(Window, SoundOutput.SamplesPerSec, SoundOutput.BufferSize);

                    game_input Input = {};
                    game_input *NewInput = &Input;
                    game_controller *KeyboardController = NewInput->Controllers;
                    KeyboardController->IsConnected = true;

                    r32 LastFrameTime = 0.0f;
                    LARGE_INTEGER LastFrameCount = Win32GetClock();
                    GlobalRunning = true;
                    while (GlobalRunning)
                    {
                        FILETIME GameDLLWriteTime = Win32GetLastWriteTime(GameDLLFileName);
                        if (CompareFileTime(&Game.LastWriteTime, &GameDLLWriteTime) != 0)
                        {
                            FreeGameCode(&Game);
                            Game = Win32LoadGameCode(GameDLLFileName, GameTempDLLFileName, GameLockFile);
                        }

                        Win32ProcessPendingMessages(Window, KeyboardController);
                        NewInput->dt = TargetFrameSeconds; // TODO(joe): Should we measure this instead
                        //            of assuming a constant frame time?

                        if (Controller)
                        {
                            game_controller *AnalogController = NewInput->Controllers + 1;
                            AnalogController->IsConnected = true;
                            AnalogController->IsAnalog = true;

#if 0
                            // TODO(joe): Is it possible to get this value from a PlayStation 4
                            // controller using DirectInput?
                            DIPROPDWORD DeadZone = {};
                            DeadZone.diph.dwSize = sizeof(DIPROPDWORD);
                            DeadZone.diph.dwHeaderSize = sizeof(DIPROPHEADER);
                            DeadZone.diph.dwObj = 0;
                            DeadZone.diph.dwHow = DIPH_DEVICE;
                            HRESULT Result = Controller->GetProperty(DIPROP_DEADZONE, &DeadZone.diph);
#else
                            DWORD DeadZone = 1000;
#endif

                            ControllerDataFormat ControllerState = GetControllerState(Controller);
#if 0
                            char Buffer[256];
                            snprintf(Buffer, sizeof(Buffer), "XAxis: %li\tYAxis: %li\t Square: %u\n",
                                    ControllerState.XAxis, ControllerState.YAxis, ControllerState.Buttons[0]);
                            OutputDebugStringA(Buffer);
#endif

                            s32 Center = 32768;
                            AnalogController->XAxis = NormalizeControllerAxis(ControllerState.XAxis, Center, DeadZone);
                            AnalogController->YAxis = NormalizeControllerAxis(ControllerState.YAxis, Center, DeadZone);
                        }

                        //Win32ResizeBackBuffer(&GlobalBackBuffer, WindowSize);

                        
                        if (Game.UpdateGameAndRender)
                        {
                            Game.UpdateGameAndRender(&Memory, &BackBuffer, NewInput, LastFrameTime);
                        }

                        DWORD PlayCursor = 0;
                        DWORD WriteCursor = 0;
                        if (GlobalSecondaryBuffer
                            && GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
                        {
                            if (!SoundIsValid)
                            {
                                SoundOutput.RunningSamples = WriteCursor / SoundOutput.BytesPerSample;
                                SoundIsValid = true;
                            }

                            DWORD ByteToLock
                                = (SoundOutput.RunningSamples * SoundOutput.BytesPerSample) % SoundOutput.BufferSize;

                            DWORD ExpectedSoundBytesPerFrame
                                = (SoundOutput.SamplesPerSec * SoundOutput.BytesPerSample) / GameUpdateHz;
                            DWORD ExpectedFrameBoundaryByte = PlayCursor + ExpectedSoundBytesPerFrame;
                            DWORD SafeWriteCursor = WriteCursor;
                            if (SafeWriteCursor < PlayCursor)
                            {
                                SafeWriteCursor += SoundOutput.BufferSize;
                            }
                            Assert(SafeWriteCursor >= PlayCursor);
                            SafeWriteCursor += SoundOutput.SafetyBytes;
                            bool AudioCardIsLowLatency = (SafeWriteCursor < ExpectedFrameBoundaryByte);

                            DWORD TargetCursor = 0;
                            if (AudioCardIsLowLatency)
                            {
                                TargetCursor = (ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame);
                            }
                            else
                            {
                                TargetCursor = (WriteCursor + ExpectedSoundBytesPerFrame + SoundOutput.SafetyBytes);
                            }
                            TargetCursor = TargetCursor % SoundOutput.BufferSize;

                            DWORD BytesToWrite = 0;
                            if (ByteToLock > TargetCursor)
                            {
                                BytesToWrite = (SoundOutput.BufferSize - ByteToLock) + TargetCursor;
                            }
                            else
                            {
                                BytesToWrite = TargetCursor - ByteToLock;
                            }

                            game_sound_buffer SoundBuffer = {};
                            // TODO(joe): We should only need to allocate this block of memory once instead
                            // of on every frame.
                            SoundBuffer.Samples = (s16 *)VirtualAlloc(0, SoundOutput.BufferSize,
                                                                        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
                            SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
                            SoundBuffer.ToneVolume = SoundOutput.ToneVolume;
                            SoundBuffer.SamplesPerSec = SoundOutput.SamplesPerSec;

#if 0
                            if (Game.GetSoundSamples)
                            {
                                Game.GetSoundSamples(&SoundBuffer, &Memory);
                            }
                            if (SoundIsValid)
                            {
                                Win32WriteToSoundBuffer(&SoundBuffer, &SoundOutput, ByteToLock, BytesToWrite);
                            }
#endif
                            VirtualFree(SoundBuffer.Samples, 0, MEM_RELEASE);
                        }
                        else
                        {
                            SoundIsValid = false;
                        }
#if 1
                        LARGE_INTEGER FrameCount = Win32GetClock();
                        float ElapsedTime = Win32GetElapsedSeconds(LastFrameCount, FrameCount);
                        if (ElapsedTime < TargetFrameSeconds)
                        {
                            DWORD TimeToSleep = (DWORD)(1000.0f * (TargetFrameSeconds - ElapsedTime));
                            if (TimeIsGranular)
                            {
                                if (TimeToSleep > 0)
                                {
                                    Sleep(TimeToSleep);
                                }
                            }

                            ElapsedTime = Win32GetElapsedSeconds(LastFrameCount, Win32GetClock());
                            while (ElapsedTime < TargetFrameSeconds)
                            {
                                ElapsedTime = Win32GetElapsedSeconds(LastFrameCount, Win32GetClock());
                            }
                        }
                        else
                        {
                            // TODO(joe): Log that we missed a frame.
                        }
#endif

                        LARGE_INTEGER EndCount = Win32GetClock();

                        LastFrameTime = 1000.0f * Win32GetElapsedSeconds(LastFrameCount, EndCount);
                        LastFrameCount = EndCount;

                        Win32PaintBackBuffer(DeviceContext, &GlobalBackBuffer);
                    }
                }
                else
                {
                    // TODO(joe): Log that we couldn't load the game handle.
                }
            }

            KillOpenGL(GLContext);
            ReleaseDC(Window, DeviceContext);
        }
    }

    return 0;
}
