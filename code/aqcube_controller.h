#pragma once

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

    Assert(PS4Data[0].dwOfs % 4 == 0);

    DWORD ObjectCount = ArrayCount(PS4Data);

    DIDATAFORMAT Format;
    Format.dwSize = sizeof(Format);
    Format.dwObjSize = sizeof(DIOBJECTDATAFORMAT);
    Format.dwFlags = 0; // DIDF_ABSAXIS;
    Format.dwDataSize = sizeof(ControllerDataFormat);
    Format.dwNumObjs = ObjectCount;
    Format.rgodf = PS4Data;

    Assert(Format.dwDataSize % 4 == 0);
    Assert(Format.dwDataSize > PS4Data[ObjectCount - 1].dwOfs);

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
        Assert(Result == DI_OK);
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

static ControllerDataFormat GetControllerState(IDirectInputDevice8 *Controller)
{
    ControllerDataFormat ControllerState = {};
    HRESULT Status = Controller->GetDeviceState(sizeof(ControllerDataFormat), &ControllerState);
    Assert(Status == DI_OK);

    return ControllerState;
}
