#include <windows.h>
#include <dsound.h>

#include <cassert>
#include <cstdio>

#include "aqcube_platform.h"
#include "aqcube.cpp"

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

    uint32 RunningSamples;
    int LatencySampleCount;
    int16 ToneVolume;
};

read_file_result DEBUGWin32ReadFile(char *Filename)
{
    read_file_result Result = {};

    HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (FileHandle)
    {
        LARGE_INTEGER FileSize;
        if(GetFileSizeEx(FileHandle, &FileSize))
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

bool DEBUGWin32WriteFile(char *Filename, void *Memory, uint32 FileSize)
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

static void Win32ResizeBackBuffer(win32_back_buffer *Buffer, int Width, int Height)
{
    // TODO(joe): There is some concern that the VirtualAlloc could fail which
    // would leave us without a buffer. See if there's a better way to handle
    // this memory reallocation.
    if (Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }

    Buffer->Width = Width;
    Buffer->Height = Height;

    Buffer->BytesPerPixel = 4;

    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = Buffer->Height;
    // TODO(joe): Treat the buffer as top-down for now. It might be better to
    // treat the back buffer as bottom up in the future.
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB; // Uncompressed: The value for blue is in the least significant 8 bits, followed by 8 bits each for green and red.
                                                   // BB GG RR XX
                                                   //
    Buffer->Pitch = Buffer->Width * Buffer->BytesPerPixel;
    int BufferMemorySize = Buffer->Height * Buffer->Width * Buffer->BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BufferMemorySize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}
static void Win32PaintBackBuffer(HDC DeviceContext, win32_back_buffer *BackBuffer)
{
    int OffsetX = 10;
    int OffsetY = 10;

    // Note(joe): For right now, I'm just going to blit the buffer as-is without any stretching.
    StretchDIBits(DeviceContext,
                  OffsetX, OffsetY, BackBuffer->Width, BackBuffer->Height, // Destination
                  0, 0, BackBuffer->Width, BackBuffer->Height, // Source
                  BackBuffer->Memory,
                  &BackBuffer->Info,
                  DIB_RGB_COLORS,
                  SRCCOPY);
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
                //SecondaryBufferDescription.dwFlags = 0;
                SecondaryBufferDescription.dwBufferBytes = BufferSize;
                SecondaryBufferDescription.lpwfxFormat = &Format;

                HRESULT SecondaryBufferResult = DirectSound->CreateSoundBuffer(&SecondaryBufferDescription, &GlobalSecondaryBuffer, 0);
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

static void Win32WriteToSoundBuffer(game_sound_buffer *SoundBuffer, win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite)
{
    void *AudioPointer1 = 0;
    DWORD AudioBytes1 = 0;
    void *AudioPointer2 = 0;
    DWORD AudioBytes2 = 0;
    assert((AudioBytes1 / SoundOutput->BytesPerSample) == 0);
    assert((AudioBytes2 / SoundOutput->BytesPerSample) == 0);
    if (GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite,
                &AudioPointer1, &AudioBytes1,
                &AudioPointer2, &AudioBytes2,
                0) == DS_OK)
    {
        int16 *Samples = SoundBuffer->Samples;

        int16 *Sample = (int16 *)AudioPointer1;
        int SamplesToWrite1 = AudioBytes1 / SoundOutput->BytesPerSample;
        for (int SampleIndex = 0; SampleIndex < SamplesToWrite1; ++SampleIndex)
        {
            *Sample++ = *Samples++; // Left
            *Sample++ = *Samples++; // Right

            ++SoundOutput->RunningSamples;
        }
        Sample = (int16 *)AudioPointer2;
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
    float Result = ((float)(End.QuadPart - Start.QuadPart)) /
                    (float)GlobalPerfFrequencyCount.QuadPart;
    return Result;
}

static LRESULT CALLBACK Win32MainCallWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;

    switch(Message)
    {
        case WM_CLOSE:
        case WM_DESTROY:
        {
            GlobalRunning = false;
        } break;
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
        } break;
        default:
        {
            // OutputDebugStringA("default\n");
            Result = DefWindowProcA(Window, Message, WParam, LParam);
        } break;
    }


    return Result;
}

static void Win32ProcessButtonState(button_state *Button, bool IsDown)
{
    assert(Button->IsDown != IsDown);
    Button->IsDown = IsDown;
}

static void Win32ProcessPendingMessages(game_controller_input *Input)
{
    // Process the message pump.
    MSG Message;
    while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
    {
        switch(Message.message)
        {
            case WM_QUIT:
                {
                    GlobalRunning = false;
                } break;
            case WM_KEYUP:
            case WM_KEYDOWN:
                {
                    uint32 KeyCode = (uint32)Message.wParam;
                    bool IsDown = ((Message.lParam & (1 << 31)) == 0);
                    bool WasDown = ((Message.lParam & (1 << 30)) != 0);

                    if (IsDown != WasDown)
                    {
                        if (KeyCode == 'W')
                        {
                            Win32ProcessButtonState(&Input->Up, IsDown);
                        }
                        else if (KeyCode == 'S')
                        {
                            Win32ProcessButtonState(&Input->Down, IsDown);
                        }
                        else if (KeyCode == 'A')
                        {
                            Win32ProcessButtonState(&Input->Left, IsDown);
                        }
                        else if (KeyCode == 'D')
                        {
                            Win32ProcessButtonState(&Input->Right, IsDown);
                        }
                    }
                } break;
            default:
                {
                    TranslateMessage(&Message);
                    DispatchMessageA(&Message);
                } break;
        }
    }
}


int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
    WNDCLASSA WindowClass = {};
    WindowClass.style = CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainCallWindowCallback;
    WindowClass.hInstance = Instance;
    WindowClass.hCursor = LoadCursor(0, IDC_ARROW);
    WindowClass.lpszClassName = "AdequateCubeWindowClass";

    if(RegisterClassA(&WindowClass))
    {
        HWND Window = CreateWindowEx( 0,
                                      WindowClass.lpszClassName,
                                      "Adequate Cube",
                                      WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                      CW_USEDEFAULT,
                                      CW_USEDEFAULT,
                                      CW_USEDEFAULT,
                                      CW_USEDEFAULT,
                                      0,
                                      0,
                                      Instance,
                                      0);
        if (Window)
        {
            game_memory Memory = {};
            Memory.PermanentStorageSize = Megabytes(64);
            Memory.PermanentStorage = VirtualAlloc(0, Memory.PermanentStorageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            Memory.TransientStorageSize = Gigabytes((uint64)4);
            Memory.TransientStorage = VirtualAlloc(0, Memory.TransientStorageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

            if(Memory.PermanentStorage && Memory.TransientStorage)
            {
                QueryPerformanceFrequency(&GlobalPerfFrequencyCount);

                Win32ResizeBackBuffer(&GlobalBackBuffer, 960, 540);

                win32_sound_output SoundOutput = {};
                SoundOutput.SamplesPerSec = 44100;
                SoundOutput.BytesPerSample = 2*sizeof(int16);
                SoundOutput.BufferSize = SoundOutput.SamplesPerSec * SoundOutput.BytesPerSample;
                SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSec / 15;

                SoundOutput.ToneVolume = 1600;
                InitSound(Window, SoundOutput.SamplesPerSec, SoundOutput.BufferSize);

#if 0
                UINT TimerResolutionMS = 1;

                int MonitorHz = 60;
                int GameUpdateHz = 30;
                float TargetFrameSeconds = 1.0f / (float)GameUpdateHz;

                bool TimeIsGranular = timeBeginPeriod(TimerResolutionMS) == TIMERR_NOERROR;
#endif
                game_controller_input Input = {};

                LARGE_INTEGER LastFrameCount = Win32GetClock();
                GlobalRunning = true;
                while(GlobalRunning)
                {
                    Win32ProcessPendingMessages(&Input);

                    DWORD PlayCursor = 0;
                    DWORD WriteCursor = 0;
                    DWORD ByteToLock = 0;
                    DWORD BytesToWrite = 0;
                    bool OutputSound = false;
                    if (GlobalSecondaryBuffer && GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
                    {
                        OutputSound = true;

                        ByteToLock = (SoundOutput.RunningSamples * SoundOutput.BytesPerSample) % SoundOutput.BufferSize;
                        DWORD TargetCursor = (PlayCursor + (SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample)) % SoundOutput.BufferSize;
                        if (ByteToLock > TargetCursor)
                        {
                            BytesToWrite = (SoundOutput.BufferSize - ByteToLock) + TargetCursor;
                        }
                        else
                        {
                            BytesToWrite = TargetCursor - ByteToLock;
                        }
#if 0
                        char SoundCursorString[255];
                        snprintf(SoundCursorString, 255, "Play Cursor: %i RunningSamples: %i ByteToLock: %i BytesToWrite: %i\n", PlayCursor, SoundOutput.RunningSamples, ByteToLock, BytesToWrite);
                        OutputDebugStringA(SoundCursorString);
#endif
                    }

                    game_back_buffer BackBuffer = {};
                    BackBuffer.Memory = GlobalBackBuffer.Memory;
                    BackBuffer.Width = GlobalBackBuffer.Width;
                    BackBuffer.Height = GlobalBackBuffer.Height;
                    BackBuffer.Pitch = GlobalBackBuffer.Pitch;
                    BackBuffer.BytesPerPixel = GlobalBackBuffer.BytesPerPixel;

                    game_sound_buffer SoundBuffer = {};
                    // TODO(joe): We should only need to allocate this block of memory once instead
                    // of on every frame.
                    SoundBuffer.Samples = (int16 *)VirtualAlloc(0, SoundOutput.BufferSize,
                                                                MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
                    SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
                    SoundBuffer.ToneVolume = SoundOutput.ToneVolume;
                    SoundBuffer.SamplesPerSec = SoundOutput.SamplesPerSec;
                    UpdateGameAndRender(&Memory, &BackBuffer, &SoundBuffer, &Input);

                    if (OutputSound)
                    {
                        Win32WriteToSoundBuffer(&SoundBuffer, &SoundOutput, ByteToLock, BytesToWrite);
                    }
                    VirtualFree(SoundBuffer.Samples, 0, MEM_RELEASE);
#if 0
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

#if 1
                    char FrameTimeString[255];
                    float MSPerFrame = 1000.0f * Win32GetElapsedSeconds(LastFrameCount, EndCount);
                    float FPS = 1000.0f / MSPerFrame;
                    snprintf(FrameTimeString, 255, "ms/f: %.2f f/s: %.2f \n", MSPerFrame, FPS);
                    OutputDebugStringA(FrameTimeString);
#endif
                    LastFrameCount = EndCount;

                    // TODO(joe): Weird issue: After the app is out of focus for a while the DeviceContext
                    // becomes NULL and we can no longer paint to the screen. WM_APPACTIVATE?
                    HDC DeviceContext = GetDC(Window);
                    Win32PaintBackBuffer(DeviceContext, &GlobalBackBuffer);
                    ReleaseDC(Window, DeviceContext);
                }
            }
        }
    }

    return 0;
}
