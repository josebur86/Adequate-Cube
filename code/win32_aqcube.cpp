#define DIRECTINPUT_VERSION 0x0800
#define INITGUID 1
#include <dinput.h>
#include <dsound.h>
#include <windows.h>

#include <assert.h>
#include <stdio.h>

#pragma warning(push)
#pragma warning(disable : 4244)
#pragma warning(disable : 4456)
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#pragma warning(pop)

#include "aqcube.h"
#include "aqcube_platform.h"
#include "aqcube_math.h"

//
// DirectInput
//
typedef HRESULT direct_input_8_create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID *ppvOut,
                                      LPUNKNOWN punkOuter);
direct_input_8_create *GlobalDirectInput8Create;

static void Win32LoadDirectInput()
{
    char DirectInputDLL[] = "dinput8.dll"; // TODO(joe): dinput.dll?
    HMODULE DirectInputLib = LoadLibraryA(DirectInputDLL);
    if (DirectInputLib)
    {
        GlobalDirectInput8Create = (direct_input_8_create *)GetProcAddress(DirectInputLib, "DirectInput8Create");
    }
}

#pragma pack(push, 1)
struct ControllerDataFormat
{
    LONG XAxis;
    LONG YAxis;
    LONG ZAxis;
    LONG XAxisRotation;
    LONG YAxisRotation;
    LONG ZAxisRotation;
    // LONG UVSlider[2];
    // DWORD POVHats[4];
    BYTE Buttons[14];
    BYTE Reserved0;
    BYTE Reserved1;
};
#pragma pack(pop)

static bool Win32SetControllerDataFormat(IDirectInputDevice8 *Device)
{
    DIOBJECTDATAFORMAT PS4Data[]
        = { { &GUID_XAxis, offsetof(ControllerDataFormat, XAxis), DIDFT_ABSAXIS | DIDFT_ANYINSTANCE, 0 },
            { &GUID_YAxis, offsetof(ControllerDataFormat, YAxis), DIDFT_ABSAXIS | DIDFT_ANYINSTANCE, 0 },
            { &GUID_ZAxis, offsetof(ControllerDataFormat, ZAxis), DIDFT_ABSAXIS | DIDFT_ANYINSTANCE, 0 },

            { &GUID_RxAxis, offsetof(ControllerDataFormat, XAxisRotation), DIDFT_ABSAXIS | DIDFT_ANYINSTANCE, 0 },
            { &GUID_RyAxis, offsetof(ControllerDataFormat, YAxisRotation), DIDFT_ABSAXIS | DIDFT_ANYINSTANCE, 0 },
            { &GUID_RzAxis, offsetof(ControllerDataFormat, ZAxisRotation), DIDFT_ABSAXIS | DIDFT_ANYINSTANCE, 0 },

#if 0
        // TODO(joe): DPAD
        {&GUID_POV, offsetof(ControllerDataFormat, POVHats)+sizeof(DWORD)*0, DIDFT_POV | DIDFT_MAKEINSTANCE(0), 0},
        {&GUID_POV, offsetof(ControllerDataFormat, POVHats)+sizeof(DWORD)*1, DIDFT_POV | DIDFT_MAKEINSTANCE(1), 0},
        {&GUID_POV, offsetof(ControllerDataFormat, POVHats)+sizeof(DWORD)*2, DIDFT_POV | DIDFT_MAKEINSTANCE(2), 0},
        {&GUID_POV, offsetof(ControllerDataFormat, POVHats)+sizeof(DWORD)*3, DIDFT_POV | DIDFT_MAKEINSTANCE(3), 0},
#endif

            { &GUID_Button, offsetof(ControllerDataFormat, Buttons) + sizeof(BYTE) * 0,
              DIDFT_PSHBUTTON | DIDFT_MAKEINSTANCE(0), 0 },
            { &GUID_Button, offsetof(ControllerDataFormat, Buttons) + sizeof(BYTE) * 1,
              DIDFT_PSHBUTTON | DIDFT_MAKEINSTANCE(1), 0 },
            { &GUID_Button, offsetof(ControllerDataFormat, Buttons) + sizeof(BYTE) * 2,
              DIDFT_PSHBUTTON | DIDFT_MAKEINSTANCE(2), 0 },
            { &GUID_Button, offsetof(ControllerDataFormat, Buttons) + sizeof(BYTE) * 3,
              DIDFT_PSHBUTTON | DIDFT_MAKEINSTANCE(3), 0 },
            { &GUID_Button, offsetof(ControllerDataFormat, Buttons) + sizeof(BYTE) * 4,
              DIDFT_PSHBUTTON | DIDFT_MAKEINSTANCE(4), 0 },
            { &GUID_Button, offsetof(ControllerDataFormat, Buttons) + sizeof(BYTE) * 5,
              DIDFT_PSHBUTTON | DIDFT_MAKEINSTANCE(5), 0 },
            { &GUID_Button, offsetof(ControllerDataFormat, Buttons) + sizeof(BYTE) * 6,
              DIDFT_PSHBUTTON | DIDFT_MAKEINSTANCE(6), 0 },
            { &GUID_Button, offsetof(ControllerDataFormat, Buttons) + sizeof(BYTE) * 7,
              DIDFT_PSHBUTTON | DIDFT_MAKEINSTANCE(7), 0 },
            { &GUID_Button, offsetof(ControllerDataFormat, Buttons) + sizeof(BYTE) * 8,
              DIDFT_PSHBUTTON | DIDFT_MAKEINSTANCE(8), 0 },
            { &GUID_Button, offsetof(ControllerDataFormat, Buttons) + sizeof(BYTE) * 9,
              DIDFT_PSHBUTTON | DIDFT_MAKEINSTANCE(9), 0 },
            { &GUID_Button, offsetof(ControllerDataFormat, Buttons) + sizeof(BYTE) * 10,
              DIDFT_PSHBUTTON | DIDFT_MAKEINSTANCE(10), 0 },
            { &GUID_Button, offsetof(ControllerDataFormat, Buttons) + sizeof(BYTE) * 11,
              DIDFT_PSHBUTTON | DIDFT_MAKEINSTANCE(11), 0 },
            { &GUID_Button, offsetof(ControllerDataFormat, Buttons) + sizeof(BYTE) * 12,
              DIDFT_PSHBUTTON | DIDFT_MAKEINSTANCE(12), 0 },
            { &GUID_Button, offsetof(ControllerDataFormat, Buttons) + sizeof(BYTE) * 13,
              DIDFT_PSHBUTTON | DIDFT_MAKEINSTANCE(13), 0 },
          };

    assert(PS4Data[0].dwOfs % 4 == 0);

    DWORD ObjectCount = ArrayCount(PS4Data);

    DIDATAFORMAT Format;
    Format.dwSize = sizeof(Format);
    Format.dwObjSize = sizeof(DIOBJECTDATAFORMAT);
    Format.dwFlags = 0; // DIDF_ABSAXIS;
    Format.dwDataSize = sizeof(ControllerDataFormat);
    Format.dwNumObjs = ObjectCount;
    Format.rgodf = PS4Data;

    assert(Format.dwDataSize % 4 == 0);
    assert(Format.dwDataSize > PS4Data[ObjectCount - 1].dwOfs);

    HRESULT Result = Device->SetDataFormat(&Format);
    return Result == DI_OK;
}

struct EnumDeviceResult
{
    bool DeviceFound;
    GUID DeviceGuid;
};

// NOTE(joe): This is what my computer is reporting the PlayStation 4 DualShock controller as.
// What about other controllers?
#define PS4_DEVICE ((DIDEVTYPE_HID) | (DI8DEVTYPE_1STPERSON) | (DI8DEVTYPE1STPERSON_SIXDOF << 8))

BOOL Win32DirectInputEnumDeviceCallback(LPCDIDEVICEINSTANCE DeviceInstance, LPVOID AppValue)
{
    if (DeviceInstance->dwDevType == PS4_DEVICE)
    {
        OutputDebugStringA("PlayStation 4 Controller Found!\n");

        EnumDeviceResult *Result = (EnumDeviceResult *)AppValue;
        Result->DeviceFound = true;
        Result->DeviceGuid = DeviceInstance->guidInstance;

        // TODO(joe): Should we really stop? What about multiplayer?
        return DIENUM_STOP;
    }

    return DIENUM_CONTINUE;
}

BOOL Win32DirectInputEnumObjectCallback(LPCDIDEVICEOBJECTINSTANCE ObjectInstance, LPVOID AppValue)
{
    char Buffer[256];
    int InstanceNumber = DIDFT_GETINSTANCE(ObjectInstance->dwType);
    int ObjectType = DIDFT_GETTYPE(ObjectInstance->dwType);
    snprintf(Buffer, 256, "%s Offset: %lu Instance Number: %i ", ObjectInstance->tszName, ObjectInstance->dwOfs,
             InstanceNumber);
    OutputDebugStringA(Buffer);

    switch (ObjectType)
    {
    case DIDFT_AXIS:
        OutputDebugStringA("DIDFT_AXIS\n");
        break;
    case DIDFT_ABSAXIS:
        OutputDebugStringA("DIDFT_ABSAXIS\n");
        break;
    case DIDFT_BUTTON:
        OutputDebugStringA("DIDFT_BUTTON\n");
        break;
    case DIDFT_PSHBUTTON:
        OutputDebugStringA("DIDFT_PSHBUTTON\n");
        break;
    case DIDFT_POV:
        OutputDebugStringA("DIDFT_POV\n");
        break;
    default:
        OutputDebugStringA("Unknown\n");
        break;
    };

    return DIENUM_CONTINUE;
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

static loaded_bitmap DEBUGLoadBitmap(char *FileName)
{
    loaded_bitmap Result = {};

    s32 X, Y, N;
    u8 *Pixels = stbi_load(FileName, &X, &Y, &N, 0);
    if (Pixels)
    {
        Result.IsValid = true;
        Result.Width = X;
        Result.Height = Y;
        Result.Pitch = X*N;
        s32 BufferSize = X*Y*N;

        // TODO(joe): Allocate from an arena.
        Result.Pixels = (u8 *)VirtualAlloc(0, BufferSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

        u8 *SourceRow = Pixels;
        u8 *DestRow = Result.Pixels;
        for (s32 RowIndex = 0; RowIndex < (s32)Y; ++RowIndex)
        {
            u32 *SourcePixel = (u32 *)SourceRow;
            u32 *DestPixel = (u32 *)DestRow;
            for (s32 ColIndex = 0; ColIndex < (s32)X; ++ColIndex)
            {
                u8 R = (u8)(*SourcePixel >> 0  & 0xFF);
                u8 G = (u8)(*SourcePixel >> 8  & 0xFF);
                u8 B = (u8)(*SourcePixel >> 16 & 0xFF);
                u8 A = (u8)(*SourcePixel >> 24 & 0xFF);

                u32 Pixel = (A << 24) | (R << 16) | (G << 8) | (B << 0);
                *DestPixel = Pixel;

                ++SourcePixel;
                ++DestPixel;
            }

            SourceRow += Result.Pitch;
            DestRow += Result.Pitch;
        }

        stbi_image_free(Pixels);
    }

    return Result;
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
    assert((AudioBytes1 / SoundOutput->BytesPerSample) == 0);
    assert((AudioBytes2 / SoundOutput->BytesPerSample) == 0);
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
    assert(Button->IsDown != IsDown);
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

static IDirectInputDevice8 *Win32InitDirectInputController(HINSTANCE Instance, HWND Window)
{
    IDirectInputDevice8 *Device = 0;

    IDirectInput8 *DirectInput;
    HRESULT Result
        = GlobalDirectInput8Create(Instance, DIRECTINPUT_VERSION, IID_IDirectInput8A, (LPVOID *)&DirectInput, 0);
    if (Result == DI_OK)
    {
        EnumDeviceResult DeviceResult = {};
        Result = DirectInput->EnumDevices(DI8DEVCLASS_GAMECTRL, Win32DirectInputEnumDeviceCallback, &DeviceResult,
                                          DIEDFL_ALLDEVICES | DIEDFL_ATTACHEDONLY);
        assert(Result == DI_OK);
        if (DeviceResult.DeviceFound)
        {
            Result = DirectInput->CreateDevice(DeviceResult.DeviceGuid, &Device, 0);
            if (Device)
            {
                DIDEVCAPS DeviceCapibilities = {};
                DeviceCapibilities.dwSize = sizeof(DeviceCapibilities);

                Result = Device->GetCapabilities(&DeviceCapibilities);

                bool Emulated = ((DeviceCapibilities.dwFlags & DIDC_EMULATED) != 0);
                bool SupportsFF = ((DeviceCapibilities.dwFlags & DIDC_FORCEFEEDBACK) != 0);
                s32 AxesCount = DeviceCapibilities.dwAxes;
                s32 ButtonCount = DeviceCapibilities.dwButtons;
                bool Polled = ((DeviceCapibilities.dwFlags & DIDC_POLLEDDEVICE) != 0);

                char Buffer[256];
                snprintf(Buffer, 256, "Controller Capabilties:\n\tEmulated: %i\n\tFF %i\n\tAxes Count: %i\n\tButton "
                                      "Count: %i\n\tPolled: %i\n",
                         Emulated, SupportsFF, AxesCount, ButtonCount, Polled);
                OutputDebugStringA(Buffer);

                // TODO(joe): DISCL_FOREGROUND?
                Device->SetCooperativeLevel(Window, DISCL_BACKGROUND | DISCL_EXCLUSIVE);
#if 0
                Device->EnumObjects(Win32DirectInputEnumObjectCallback, 0, DIDFT_ALL);
#endif
                Win32SetControllerDataFormat(Device);
                Result = Device->Acquire();
                if (Result != DI_OK)
                {
                    // TODO(joe): What needs to be released?
                    Device = 0;
                }
            }
        }
    }

    return Device;
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

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
    Win32LoadDirectInput();

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

            game_memory Memory = {};
            Memory.PermanentStorageSize = Megabytes(64);
            Memory.PermanentStorage
                = VirtualAlloc(0, Memory.PermanentStorageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            Memory.TransientStorageSize = Gigabytes((u64)4);
            Memory.TransientStorage
                = VirtualAlloc(0, Memory.TransientStorageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

            Memory.DEBUGLoadBitmap = DEBUGLoadBitmap;

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

                            ControllerDataFormat ControllerState = {};
                            if (Controller->GetDeviceState(sizeof(ControllerDataFormat), &ControllerState) == DI_OK)
                            {
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
                        }

                        //Win32ResizeBackBuffer(&GlobalBackBuffer, WindowSize);

                        game_back_buffer BackBuffer = {};
                        BackBuffer.Memory = GlobalBackBuffer.Memory;
                        BackBuffer.Width = GlobalBackBuffer.Width;
                        BackBuffer.Height = GlobalBackBuffer.Height;
                        BackBuffer.Pitch = GlobalBackBuffer.Pitch;
                        BackBuffer.BytesPerPixel = GlobalBackBuffer.BytesPerPixel;

                        if (Game.UpdateGameAndRender)
                        {
                            Game.UpdateGameAndRender(&Memory, &BackBuffer, NewInput);
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

#if 0
                        char FrameTimeString[255];
                        float MSPerFrame = 1000.0f * Win32GetElapsedSeconds(LastFrameCount, EndCount);
                        float FPS        = 1000.0f / MSPerFrame;
                        snprintf(FrameTimeString, 255, "ms/f: %.2f f/s: %.2f \n", MSPerFrame, FPS);
                        OutputDebugStringA(FrameTimeString);
#endif
                        LastFrameCount = EndCount;

                        Win32PaintBackBuffer(DeviceContext, &GlobalBackBuffer);
                    }
                }
                else
                {
                    // TODO(joe): Log that we couldn't load the game handle.
                }
            }

            ReleaseDC(Window, DeviceContext);
        }
    }

    return 0;
}
