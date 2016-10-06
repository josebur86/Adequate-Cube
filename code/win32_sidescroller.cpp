#include <windows.h>
#include <cstdint>

struct win32_back_buffer
{
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
};

static bool GlobalRunning = true;
static win32_back_buffer GlobalBackBuffer;

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

static LRESULT CALLBACK Win32MainCallWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;
    // TODO(joe): This method is mainly responsible for responding to any keys that are pressed.
    // Need to handle keyboard user input.

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

            int32_t *Pixel = (int32_t *)GlobalBackBuffer.Memory;        
            for (int YIndex = 0; YIndex < GlobalBackBuffer.Height; ++YIndex)
            {
                for (int XIndex = 0; XIndex < GlobalBackBuffer.Width; ++XIndex)
                {
                    uint8_t b = 255;
                    uint8_t g = 30;
                    uint8_t r = 127;
                    *Pixel++ = (b << 0 | g << 8 | r << 16);
                }
            }

            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);
    
            int OffsetX = 10;
            int OffsetY = 10;

            RECT ClientRect;
            GetClientRect(Window, &ClientRect);
            int ClientWidth = ClientRect.right - ClientRect.left;
            int ClientHeight = ClientRect.bottom - ClientRect.top;
            PatBlt(DeviceContext, 0, 0, ClientWidth, ClientHeight, BLACKNESS);

            // Note(joe): For right now, I'm just going to blit the buffer as-is without any stretching.
            StretchDIBits(DeviceContext,
                          OffsetX, OffsetY, GlobalBackBuffer.Width, GlobalBackBuffer.Height, // Destination
                          0, 0, GlobalBackBuffer.Width, GlobalBackBuffer.Height, // Source
                          GlobalBackBuffer.Memory, 
                          &GlobalBackBuffer.Info,
                          DIB_RGB_COLORS,
                          SRCCOPY);

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
                                      "Generic Side-Scroller",
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

            

            GlobalRunning = true;
            while(GlobalRunning)
            {
                // Process the message pump.
                MSG Message;
                while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
                {
                    if (Message.message == WM_QUIT)
                    {
                        GlobalRunning = false;
                    }
                    else
                    {
                        TranslateMessage(&Message);
                        DispatchMessageA(&Message);
                    }
                }
            }
        }
    }

    return 0;
}
