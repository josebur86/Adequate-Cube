#include <windows.h>
#include <dsound.h>

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>

#define PI32 3.14159265359f

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

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
    int WavePeriod;
    float tSine;
    int16 ToneVolume;
};

struct game_state
{
    int OffsetX;
    int OffsetY;

    int ToneHz;
};

static bool GlobalRunning = true;
static win32_back_buffer GlobalBackBuffer;
static LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
game_state GlobalGameState;
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
    DWORD Channels = 2;
    WORD BitsPerSample = 16;
    DWORD BlockAlign = Channels * (BitsPerSample / 8);
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
                    HRESULT PlayResult = GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
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

static void Win32WriteToSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite)
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
        int16 *Sample = (int16 *)AudioPointer1;
        int SamplesToWrite1 = AudioBytes1 / SoundOutput->BytesPerSample;
        for (int SampleIndex = 0; SampleIndex < SamplesToWrite1; ++SampleIndex)
        {
            float SineValue = sinf(SoundOutput->tSine);
            uint16 ToneValue = (uint16)(SoundOutput->ToneVolume*SineValue);

            *Sample++ = ToneValue; // Left
            *Sample++ = ToneValue; // Right

            ++SoundOutput->RunningSamples;
            SoundOutput->tSine += 2*PI32 * 1.0f/(float)SoundOutput->WavePeriod;
        }
        Sample = (int16 *)AudioPointer2;
        int SamplesToWrite2 = AudioBytes2 / SoundOutput->BytesPerSample;
        for (int SampleIndex = 0; SampleIndex < SamplesToWrite2; ++SampleIndex)
        {
            float SineValue = sinf(SoundOutput->tSine);
            uint16 ToneValue = (uint16)(SoundOutput->ToneVolume*SineValue);

            *Sample++ = ToneValue; // Left
            *Sample++ = ToneValue; // Right

            ++SoundOutput->RunningSamples;
            SoundOutput->tSine += 2*PI32 * 1.0f/(float)SoundOutput->WavePeriod;
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

static void Update(win32_back_buffer *BackBuffer, game_state GameState)
{
    int32 *Pixel = (int32 *)BackBuffer->Memory;        
    for (int YIndex = 0; YIndex < BackBuffer->Height; ++YIndex)
    {
        for (int XIndex = 0; XIndex < BackBuffer->Width; ++XIndex)
        {
            uint8 b = GameState.OffsetX + XIndex;
            uint8 g = GameState.OffsetY + YIndex;
            uint8 r = GameState.OffsetX + XIndex + GameState.OffsetY + YIndex;
            *Pixel++ = (b << 0 | g << 8 | r << 16);
        }
    }
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

struct button_state
{
    bool IsDown;
};

struct game_controller_input
{
    button_state Up;
    button_state Down;
    button_state Left;
    button_state Right;
};

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
    WindowClass.lpszClassName = "SideScrollerWindowClass";

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
            GlobalGameState.OffsetX = 0;
            GlobalGameState.OffsetY = 0;
            GlobalGameState.ToneHz = 256;

            QueryPerformanceFrequency(&GlobalPerfFrequencyCount);

            Win32ResizeBackBuffer(&GlobalBackBuffer, 960, 540);

            win32_sound_output SoundOutput = {};
            SoundOutput.SamplesPerSec = 44100;
            SoundOutput.BytesPerSample = 2*sizeof(int16);
            SoundOutput.BufferSize = SoundOutput.SamplesPerSec * SoundOutput.BytesPerSample;
            SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSec / 15;

            SoundOutput.ToneVolume = 1600;
            SoundOutput.WavePeriod = SoundOutput.SamplesPerSec / GlobalGameState.ToneHz; // samples
            InitSound(Window, SoundOutput.SamplesPerSec, SoundOutput.BufferSize);

            UINT TimerResolutionMS = 1;
            bool TimeIsGranular = timeBeginPeriod(TimerResolutionMS) == TIMERR_NOERROR;

            int MonitorHz = 60;
            int GameUpdateHz = 30;
            float TargetFrameSeconds = 1.0f / (float)GameUpdateHz;
            
            game_controller_input Input = {};

            LARGE_INTEGER LastFrameCount = Win32GetClock();
            GlobalRunning = true;
            while(GlobalRunning)
            {
                Win32ProcessPendingMessages(&Input);

                // TODO(joe): Input handling is something that should happen
                // outside of the platform layer. Move this to the game
                // layer when it is available.
                if (Input.Up.IsDown)
                {
                    GlobalGameState.OffsetY -= 10;
                    GlobalGameState.ToneHz += 10;
                    SoundOutput.WavePeriod = SoundOutput.SamplesPerSec / GlobalGameState.ToneHz;
                }
                if (Input.Down.IsDown)
                {
                    GlobalGameState.OffsetY += 10;
                    GlobalGameState.ToneHz -= 10;
                    SoundOutput.WavePeriod = SoundOutput.SamplesPerSec / GlobalGameState.ToneHz;
                }
                if (Input.Left.IsDown)
                {
                    GlobalGameState.OffsetX -= 10;
                }
                if (Input.Right.IsDown)
                {
                    GlobalGameState.OffsetX += 10;
                }

                DWORD PlayCursor = 0;
                DWORD WriteCursor = 0;
                if (GlobalSecondaryBuffer && GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
                {
                    DWORD ByteToLock = (SoundOutput.RunningSamples * SoundOutput.BytesPerSample) % SoundOutput.BufferSize;
                    DWORD TargetCursor = (PlayCursor + (SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample)) % SoundOutput.BufferSize;
                    DWORD BytesToWrite = 0;
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
                    Win32WriteToSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite);
                }

                Update(&GlobalBackBuffer, GlobalGameState);
                
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

                LARGE_INTEGER EndCount = Win32GetClock();

#if 1
                char FrameTimeString[255];
                float MSPerFrame = 1000.0f * Win32GetElapsedSeconds(LastFrameCount, EndCount);
                float FPS = 1000.0f / MSPerFrame;
                snprintf(FrameTimeString, 255, "ms/f: %.2f f/s: %.2f \n", MSPerFrame, FPS);
                OutputDebugStringA(FrameTimeString);
#endif
                LastFrameCount = EndCount;

                HDC DeviceContext = GetDC(Window);
                Win32PaintBackBuffer(DeviceContext, &GlobalBackBuffer);
            }
        }
    }

    return 0;
}
