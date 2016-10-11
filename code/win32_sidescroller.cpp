#include <windows.h>

#include <cstdint>
#include <cstdio>

struct win32_back_buffer
{
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
};

struct game_state
{
    int OffsetX;
    int OffsetY;
};

static bool GlobalRunning = true;
static win32_back_buffer GlobalBackBuffer;
game_state GlobalGameState;

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

static void Update(win32_back_buffer *BackBuffer, game_state GameState)
{
    int32_t *Pixel = (int32_t *)BackBuffer->Memory;        
    for (int YIndex = 0; YIndex < BackBuffer->Height; ++YIndex)
    {
        for (int XIndex = 0; XIndex < BackBuffer->Width; ++XIndex)
        {
            uint8_t b = GameState.OffsetX + XIndex;
            uint8_t g = GameState.OffsetY + YIndex;
            uint8_t r = GameState.OffsetX + XIndex + GameState.OffsetY + YIndex;
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
            Win32ResizeBackBuffer(&GlobalBackBuffer, 960, 540);

            GlobalGameState.OffsetX = 0;
            GlobalGameState.OffsetY = 0;

            GlobalRunning = true;
            while(GlobalRunning)
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
                        case WM_KEYDOWN:
                        {
                            uint32_t KeyCode = (uint32_t)Message.wParam;

                            // TODO(joe): The processing of key presses should
                            // not happen here. This should only translate
                            // keypresses into user input for the game to
                            // handle.
                            if (KeyCode == 'W')
                            {
                                GlobalGameState.OffsetY -= 10;
                            }
                            else if (KeyCode == 'S')
                            {
                                GlobalGameState.OffsetY += 10;
                            }
                            else if (KeyCode == 'A')
                            {
                                GlobalGameState.OffsetX -= 10;
                            }
                            else if (KeyCode == 'D')
                            {
                                GlobalGameState.OffsetX += 10;
                            }
                        } break;
                        default:
                        {
                            TranslateMessage(&Message);
                            DispatchMessageA(&Message);
                        } break;
                    }
                }

                Update(&GlobalBackBuffer, GlobalGameState);
                HDC DeviceContext = GetDC(Window);
                Win32PaintBackBuffer(DeviceContext, &GlobalBackBuffer);
            }
        }
    }

    return 0;
}
