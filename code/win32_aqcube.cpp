#include <windows.h>
#include <dsound.h>

#include <cassert>
#include <cstdio>

#include "aqcube_platform.h"

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

            Result.UpdateGameAndRender = (update_game_and_render *)GetProcAddress(Result.GameDLL, "UpdateGameAndRender");
            Result.GetSoundSamples = (get_sound_samples *)GetProcAddress(Result.GameDLL, "GetSoundSamples");

            Result.IsValid = Result.UpdateGameAndRender && Result.GetSoundSamples;
        }
    }

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

struct sound_marker
{
    DWORD WriteCursor;
    DWORD WriteEndCursor;
    DWORD PlayCursor;
    DWORD TargetCursor;
};

void DebugDrawLine(win32_back_buffer ScreenBuffer, int SoundBufferBytes, int SoundByte, int Color, int Row)
{
    int Padding = 10;
    int MarkerHeight = 100;

    int Canvas = ScreenBuffer.Width - (2*Padding);

    int PixelOffset = (int)(Padding + (SoundByte * ((float)Canvas / (float)SoundBufferBytes)));

    int YOffset = (ScreenBuffer.Height / 2) - (MarkerHeight / 2)+(Row*MarkerHeight);
    for (int YIndex = YOffset; YIndex < YOffset + MarkerHeight; ++YIndex)
    {
        int32 *Pixel = ((int32 *)ScreenBuffer.Memory + (YIndex * ScreenBuffer.Width)) + PixelOffset;
        *Pixel = Color;
    }
}

void DebugDrawSoundMarker(win32_back_buffer ScreenBuffer, int SoundBufferBytes, sound_marker Marker)
{
    int PlayColor     = 0x000000FF;
    int WriteColor    = 0x0000FF00;
    int TargetColor   = 0x00FFFFFF;

    DebugDrawLine(ScreenBuffer, SoundBufferBytes, Marker.PlayCursor, PlayColor, -1);
    DebugDrawLine(ScreenBuffer, SoundBufferBytes, Marker.WriteCursor, WriteColor, 0);
    DebugDrawLine(ScreenBuffer, SoundBufferBytes, Marker.TargetCursor, TargetColor, 1);
}

void DebugDrawSoundMarkers(win32_back_buffer ScreenBuffer, int SoundBufferBytes, sound_marker *Markers, int MarkerCount)
{
    for(int MarkerIndex = 0; MarkerIndex < MarkerCount; ++MarkerIndex)
    {
        sound_marker *CurrentMarker = Markers + MarkerIndex;
        DebugDrawSoundMarker(GlobalBackBuffer, SoundBufferBytes, *CurrentMarker);
    }
}

#if 0
void DebugDrawSoundBuffer(win32_back_buffer ScreenBuffer, game_sound_buffer *SoundBuffer, int StartByte, int SoundBufferBytes)
{
    int BytesPerSample = 2*sizeof(int16);

    int Padding = 10;
    int MarkerHeight = 100;
    int WaveHeight = MarkerHeight / 2;
    int Canvas = ScreenBuffer.Width - (2*Padding);

    int CenterPointY = (ScreenBuffer.Height / 2);

    int16 *Sample = SoundBuffer->Samples;
    for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
    {
        int SoundByte = StartByte + (SampleIndex * BytesPerSample);
        int PixelX = (int)(Padding + (SoundByte * ((float)Canvas / (float)SoundBufferBytes)));
        int PixelY = (int)((-(*Sample) * ((float)WaveHeight/(float)SoundBuffer->ToneVolume)) + CenterPointY);

        assert(PixelY <= CenterPointY+WaveHeight);
        assert(PixelY >= CenterPointY-WaveHeight);

        int32 *Pixel = ((int32 *)ScreenBuffer.Memory + (PixelY * ScreenBuffer.Width)) + PixelX;
        *Pixel = 0x00FFFFFF;

        Sample += 2; // Left/Right
    }
}
#endif

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

            char *GameDLLFileName = "aqcube.dll";
            char *GameTempDLLFileName = "aqcube_temp.dll";
            char *GameLockFile = "lock.tmp";

            if(Memory.PermanentStorage && Memory.TransientStorage)
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

                    Win32ResizeBackBuffer(&GlobalBackBuffer, 960, 540);

                    win32_sound_output SoundOutput = {};
                    SoundOutput.SamplesPerSec = 44100;
                    SoundOutput.BytesPerSample = 2*sizeof(int16);
                    SoundOutput.BufferSize = SoundOutput.SamplesPerSec * SoundOutput.BytesPerSample;
                    SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSec / 15;
                    SoundOutput.SafetyBytes = (SoundOutput.SamplesPerSec * SoundOutput.BytesPerSample) / GameUpdateHz / 3;

                    SoundOutput.ToneVolume = 1600;
                    InitSound(Window, SoundOutput.SamplesPerSec, SoundOutput.BufferSize);

                    game_controller_input Input = {};

                    bool SoundIsValid = false;
                    const int MarkerCount = 10;
                    int MarkerIndex = 0;
                    sound_marker SoundMarkers[MarkerCount] = {};

                    LARGE_INTEGER LastFrameCount = Win32GetClock();
                    GlobalRunning = true;
                    while(GlobalRunning)
                    {
                        FILETIME GameDLLWriteTime = Win32GetLastWriteTime(GameDLLFileName);
                        if (CompareFileTime(&Game.LastWriteTime, &GameDLLWriteTime) != 0)
                        {
                            FreeGameCode(&Game);
                            Game = Win32LoadGameCode(GameDLLFileName, GameTempDLLFileName, GameLockFile);
                        }

                        Win32ProcessPendingMessages(&Input);

                        game_back_buffer BackBuffer = {};
                        BackBuffer.Memory = GlobalBackBuffer.Memory;
                        BackBuffer.Width = GlobalBackBuffer.Width;
                        BackBuffer.Height = GlobalBackBuffer.Height;
                        BackBuffer.Pitch = GlobalBackBuffer.Pitch;
                        BackBuffer.BytesPerPixel = GlobalBackBuffer.BytesPerPixel;

                        if (Game.UpdateGameAndRender)
                        {
                            Game.UpdateGameAndRender(&Memory, &BackBuffer, &Input);
                        }

                        DWORD PlayCursor = 0;
                        DWORD WriteCursor = 0;
                        if (GlobalSecondaryBuffer &&
                            GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
                        {
                            if (!SoundIsValid)
                            {
                                SoundOutput.RunningSamples = WriteCursor / SoundOutput.BytesPerSample;
                                SoundIsValid = true;
                            }

                            DWORD ByteToLock = (SoundOutput.RunningSamples * SoundOutput.BytesPerSample) % SoundOutput.BufferSize;

                            DWORD ExpectedSoundBytesPerFrame = (SoundOutput.SamplesPerSec*SoundOutput.BytesPerSample) / GameUpdateHz;
                            DWORD ExpectedFrameBoundaryByte = PlayCursor + ExpectedSoundBytesPerFrame;
                            DWORD SafeWriteCursor = WriteCursor;
                            if (SafeWriteCursor < PlayCursor)
                            {
                                SafeWriteCursor += SoundOutput.BufferSize;
                            }
                            assert(SafeWriteCursor >= PlayCursor);
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

                            if (MarkerIndex >= MarkerCount)
                            {
                                MarkerIndex = 0;
                            }
                            sound_marker *Marker = SoundMarkers + MarkerIndex;
                            Marker->WriteCursor = ByteToLock;
                            Marker->WriteEndCursor = ByteToLock+BytesToWrite;
                            Marker->TargetCursor = TargetCursor;
                            Marker->PlayCursor = PlayCursor;
                            DebugDrawSoundMarkers(GlobalBackBuffer, SoundOutput.BufferSize, SoundMarkers, MarkerCount);

                            game_sound_buffer SoundBuffer = {};
                            // TODO(joe): We should only need to allocate this block of memory once instead
                            // of on every frame.
                            SoundBuffer.Samples = (int16 *)VirtualAlloc(0, SoundOutput.BufferSize,
                                    MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
                            SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
                            SoundBuffer.ToneVolume = SoundOutput.ToneVolume;
                            SoundBuffer.SamplesPerSec = SoundOutput.SamplesPerSec;

                            if (Game.GetSoundSamples)
                            {
                                Game.GetSoundSamples(&SoundBuffer, &Memory);
                            }
                            if (SoundIsValid)
                            {
                                Win32WriteToSoundBuffer(&SoundBuffer, &SoundOutput, ByteToLock, BytesToWrite);
                            }
                            VirtualFree(SoundBuffer.Samples, 0, MEM_RELEASE);

                            /*
                             *  Soundcard update granularity: 1764 bytes - 441 samples - 10 milliseconds
                             *  Samples Per Sec: 44100 samples
                             *  Bytes Per Sample: 4 bytes
                             *  It seems like I have a card that provides updates an interval that is less than a frame's
                             *  time.
                             *
                             *  Difference Between PlayCursor and Write Cursor: 5292 bytes - 1323 samples - 30 milliseconds
                             *  However, if the latency between the play cursor and the write cursor is our minimum expected
                             *  latency, then this card will take roughly a frame to catch up.
                             */
#if 1
                            int LastMarkerIndex = MarkerIndex-1;
                            if (LastMarkerIndex < 0)
                            {
                                LastMarkerIndex = MarkerCount-1;
                            }
                            sound_marker *LastMarker = SoundMarkers + LastMarkerIndex;
                            char SoundCursorString[255];
                            snprintf(SoundCursorString, 255, "Last Play Cursor: %i Play Cursor: %i Diff: %i\n",
                                    LastMarker->PlayCursor, Marker->PlayCursor, Marker->PlayCursor - LastMarker->PlayCursor);
                            OutputDebugStringA(SoundCursorString);

#endif
                            ++MarkerIndex;
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

#if 0
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
                else
                {
                    // TODO(joe): Log that we couldn't load the game handle.
                }
            }
        }
    }

    return 0;
}
