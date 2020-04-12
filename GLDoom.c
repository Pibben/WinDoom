/////////////////////////////////////////////////////////////////////////////////////
// Windows Includes...
/////////////////////////////////////////////////////////////////////////////////////
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <fcntl.h>

/////////////////////////////////////////////////////////////////////////////////////
// DirectX Includes...
/////////////////////////////////////////////////////////////////////////////////////
#include <direct.h>
#include <dinput.h>
#include <dsound.h>
#include <dplay.h>

/////////////////////////////////////////////////////////////////////////////////////
// Application Includes...
/////////////////////////////////////////////////////////////////////////////////////
#include "resource.h"  // Required for Win32 Resources

/////////////////////////////////////////////////////////////////////////////////////
// "WinDoom" Includes...
/////////////////////////////////////////////////////////////////////////////////////
#include "doomdef.h"
#include "d_event.h"
#include "d_main.h"
#include "m_argv.h"
#include "i_system.h"
#include "m_music.h"
#include "i_cd.h"
#include "i_midi.h"
#include "dxerr.h"

/////////////////////////////////////////////////////////////////////////////////////
// DirectX Defines and Data
/////////////////////////////////////////////////////////////////////////////////////

#undef RELEASE
#ifdef __cplusplus
#define RELEASE(x) if (x != NULL) {x->Release(); x = NULL;}
#else
#define RELEASE(x) if (x != NULL) {x->lpVtbl->Release(x); x = NULL;}
#endif

/////////////////////////////////////////////////////////////////////////////////////
// Direct Sound
/////////////////////////////////////////////////////////////////////////////////////

#define NUM_SOUND_FX 128
#define SB_SIZE      20480

BOOL SetupDirectSound();
void ShutdownDirectSound(void);

LPDIRECTSOUND        lpDS;
LPDIRECTSOUNDBUFFER  lpDSPrimary;
LPDIRECTSOUNDBUFFER  lpDSBuffer[NUM_SOUND_FX];

/////////////////////////////////////////////////////////////////////////////////////
// Direct Input
/////////////////////////////////////////////////////////////////////////////////////

void HandleKeyStrokes(void);
void KeyboardHandler(void);
BOOL InitDirectInput(void);
void ShutdownDirectInput(void);

LPDIRECTINPUT        lpDirectInput = 0;
LPDIRECTINPUTDEVICE  lpMouse       = 0;
LPDIRECTINPUTDEVICE  lpJoystick    = 0;
LPDIRECTINPUTDEVICE  lpKeyboard    = 0;

BOOL          bDILive = FALSE;
char          szMouseBuf[1024];
unsigned char diKeyState[256];

/////////////////////////////////////////////////////////////////////////////////////
// Direct Play
/////////////////////////////////////////////////////////////////////////////////////

LPDIRECTPLAY3A    lpDirectPlay3A;
int               iDPConnections = 0;

BOOL SetupDirectPlay(void);
BOOL FAR PASCAL DPEnumConnCallback(LPCGUID lpguidSP, LPVOID lpConnection, DWORD dwConnectionSize,
                                   LPCDPNAME lpName, DWORD dwFlags, LPVOID lpContext);
void ShutDownDirectPlay(void);

/////////////////////////////////////////////////////////////////////////////////////
// Application Defines and Data
/////////////////////////////////////////////////////////////////////////////////////

#define MUSIC_NONE      0
#define MUSIC_CDAUDIO   1
#define MUSIC_MIDI      2

#define RENDER_GL       0
#define RENDER_D3D      1

#define KS_KEYUP        0
#define KS_KEYDOWN    255

/////////////////////////////////////////////////////////////////////////////////////
// Game states -- these are the modes in which the outer game loop can be
/////////////////////////////////////////////////////////////////////////////////////
#define GAME_START  0
#define GAME_SPLASH 1
#define GAME_MENU   2
#define GAME_PLAY   3
#define GAME_EXIT   4
#define GAME_QUIT   5
#define GAME_LIMBO  6
#define GAME_PAUSE  7

int     GameMode = GAME_START;

extern byte *screens[5];

short     si_Kbd[256];

char      szMsgText[2048];

DWORD     dwCurrWidth, dwCurrHeight, dwCurrBPP;

extern    CD_Data_t   CDData;
extern    MIDI_Data_t MidiData;

char        szMidiFile[] = "doomsong.mid";

int         MusicType = MUSIC_MIDI;
int         RenderType = RENDER_D3D;
BOOL        bQuit = FALSE;

void  GetWindowsVersion(void);
void  WriteDebug(char *);

int   WinDoomAC;
char *WinDoomAV[256];

void  ParseCommand(PSTR szCmdLine);

void  GameLoop(void);

void  D_DoomMain(void);
void *W_CacheLumpName(char *, int);
void  I_SetPalette(byte *);
void  MY_DoomSetup(void);
void  MY_DoomLoop(void);
void  WinDoomExit(void);

/////////////////////////////////////////////////////////////////////////////////////
// Windows Defines and Data
/////////////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
char      szAppName[] = "WinDoom";
char      szDbgName[] = "WinDoom.dbg";
char      szCfgName[] = "WinDoom.cfg";
HINSTANCE hInst;
HWND      hMainWnd;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int iCmdShow)
   {
    MSG         msg;
    WNDCLASSEX  wndclass;
    FILE       *fn;
    HWND        hwnd;
    HDC         hDCDesk;
    HWND        hDesktop;
    HACCEL      hAccel;
    int         i, k;

    hInst = hInstance;

    fn = fopen(szDbgName, "w");
    fclose(fn);

    // We get the current video setup here. Really don't need to do this. The data isn't used.
    dwCurrWidth = GetSystemMetrics(SM_CXSCREEN);
    dwCurrHeight = GetSystemMetrics(SM_CYSCREEN);
    hDesktop = GetDesktopWindow();
	hDCDesk  = GetDC(hDesktop);
	dwCurrBPP = GetDeviceCaps(hDCDesk,BITSPIXEL);
	ReleaseDC(hDesktop,hDCDesk);

    wndclass.cbSize        = sizeof(wndclass);
    wndclass.style         = CS_HREDRAW | CS_VREDRAW;
    wndclass.lpfnWndProc   = WndProc;
    wndclass.cbClsExtra    = 0;
    wndclass.cbWndExtra    = 0;
    wndclass.hInstance     = hInstance;
    wndclass.hIcon         = LoadIcon(NULL, IDI_WINLOGO);
    wndclass.hCursor       = LoadCursor(NULL, IDC_WAIT);
    wndclass.hbrBackground =(HBRUSH) GetStockObject(BLACK_BRUSH);
    wndclass.lpszMenuName  = NULL;
    wndclass.lpszClassName = szAppName;
    wndclass.hIconSm       = LoadIcon(NULL, IDI_WINLOGO);

    RegisterClassEx(&wndclass);

    CDData.CDStatus = cd_empty;
    CDData.CDMedia = FALSE;
    CDData.CDPosition = 0;
    CDData.CDCode[0] = '\0';

    hwnd = CreateWindow(szAppName,            // window class name
	                    "WinDoom",              // window caption
                        WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,  // window style
                        0,0,                  // initial x & y position
                        dwCurrWidth, dwCurrHeight,
                        NULL,                  // parent window handle
                        NULL,                  // window menu handle
                        hInstance,             // program instance handle
                        NULL);		          // creation parameters
/*
SystemParametersInfo()
*/

    hMainWnd = hwnd;
    SetupDirectSound();
    // Make the window visible & update its client area
    ShowWindow(hwnd, iCmdShow);// Show the window
    UpdateWindow(hwnd);        // Sends WM_PAINT message

    // This is used to determine what OS we're on (Windows9X or Windows NT)
    GetWindowsVersion();
    // This sets the video mode to video mode described in the default video mode
    InitGlobals();
    SetupDirectDraw(hwnd);

    SetupDirectPlay();

    MoveWindow(hwnd, 0,0, Mode[CurrMode].w, Mode[CurrMode].h, TRUE);

    hAccel = LoadAccelerators(hInstance,"AppAccel");

    bDILive = InitDirectInput();

    for (k = 0; k < 256; k++)
        si_Kbd[k] = KS_KEYUP;

    PlayMidiFile(szMidiFile);

    bQuit = FALSE;

    ParseCommand(szCmdLine);

    D_DoomMain();
    MY_DoomSetup();
    GameMode = GAME_PLAY;

    while (!bQuit)
	   {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		   {
            if (msg.message == WM_QUIT)
			   {
				bQuit = TRUE;
                break;
               }
            if (!TranslateAccelerator(msg.hwnd, hAccel, &msg))
			   {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
               }
           }
        if (GameMode == GAME_PLAY)
           GameLoop();
        if (GameMode == GAME_QUIT)
           I_Quit();
       }
// This blows up sometimes -- don't know why...
    for (i = 4; i >= 0; i--)
        free(screens[i]);
/* This doesn't work at all for some reason...
    for (i = (WinDoomAC-1); i > 0; i--)
        free(WinDoomAV[i]);
*/
    return msg.wParam;
   }

void ParseCommand(PSTR szCmdLine)
   {
    int   j;
    char *s;

    WinDoomAC = 1;
    s = strtok(szCmdLine, " ");
    while (s != NULL)
       {
        j = strlen(s);
        WinDoomAV[WinDoomAC] = (char *)malloc(j);
        strcpy(WinDoomAV[WinDoomAC], s);
        WinDoomAC++;
        s = strtok(NULL, " ");
       }
    
    myargc = WinDoomAC;
    myargv = WinDoomAV;
   }

void GameLoop()
   {
    DIMOUSESTATE         diMouseState;          /* DirectInput mouse state structure */
    static DIMOUSESTATE  diHoldMouse;           /* DirectInput mouse state structure */
    DIJOYSTATE           diJoyState;            /* DirectInput joystick state structure */
    static DIJOYSTATE    diHoldJoy;             /* DirectInput joystick state structure */
    HRESULT          hresult;
    static  event_t  event;

    RetryMouse:;
        hresult = lpMouse->lpVtbl->GetDeviceState(lpMouse, sizeof(DIMOUSESTATE), &diMouseState);
        if (hresult == DIERR_INPUTLOST)
           {
            hresult = lpMouse->lpVtbl->Acquire(lpMouse);
            if (SUCCEEDED(hresult))
                goto RetryMouse;
           }
        else
        if (hresult != DI_OK)
           {
            DI_Error(hresult, "GetDeviceState (mouse)");
           }

        if (SUCCEEDED(hresult))
           {
            if ((diMouseState.lX != diHoldMouse.lX) ||
                (diMouseState.lY != diHoldMouse.lY) ||
                (diMouseState.lZ != diHoldMouse.lZ) ||
                (diMouseState.rgbButtons[0] != diHoldMouse.rgbButtons[0]) ||
                (diMouseState.rgbButtons[1] != diHoldMouse.rgbButtons[1]) ||
                (diMouseState.rgbButtons[2] != diHoldMouse.rgbButtons[2]) ||
                (diMouseState.rgbButtons[3] != diHoldMouse.rgbButtons[3]))
               {
                diHoldMouse.lX = diMouseState.lX;
                diHoldMouse.lY = diMouseState.lY;
                diHoldMouse.lZ = diMouseState.lZ;
                diHoldMouse.rgbButtons[0] = diMouseState.rgbButtons[0];
                diHoldMouse.rgbButtons[1] = diMouseState.rgbButtons[1];
                diHoldMouse.rgbButtons[2] = diMouseState.rgbButtons[2];
                diHoldMouse.rgbButtons[3] = diMouseState.rgbButtons[3];

                event.type = ev_mouse;
                event.data1 = ((diMouseState.rgbButtons[0] & 0x80) >> 7) |
                              ((diMouseState.rgbButtons[1] & 0x80) >> 6) |
                              ((diMouseState.rgbButtons[2] & 0x80) >> 5);
                event.data2 = diMouseState.lX;
                event.data3 = (diMouseState.lY * -1);
                D_PostEvent(&event);
               }
           }

    RetryJoy:;
    if (lpJoystick != 0)
       {
        hresult = lpJoystick->lpVtbl->GetDeviceState(lpJoystick, sizeof(DIJOYSTATE), &diJoyState);
        if (hresult == DIERR_INPUTLOST)
           {
            hresult = lpJoystick->lpVtbl->Acquire(lpJoystick);
            if (SUCCEEDED(hresult))
                goto RetryJoy;
           }
        else
        if (hresult != DI_OK)
           {
            DI_Error(hresult, "GetDeviceState (joystick)");
           }

        if (SUCCEEDED(hresult))
           {
            if ((diJoyState.lX != diHoldJoy.lX) ||
                (diJoyState.lY != diHoldJoy.lY) ||
                (diJoyState.rgbButtons[0] != diHoldJoy.rgbButtons[0]) ||
                (diJoyState.rgbButtons[1] != diHoldJoy.rgbButtons[1]) ||
                (diJoyState.rgbButtons[2] != diHoldJoy.rgbButtons[2]) ||
                (diJoyState.rgbButtons[3] != diHoldJoy.rgbButtons[3]))
               {
                diHoldJoy.lX = diJoyState.lX;
                diHoldJoy.lY = diJoyState.lY;
                diHoldJoy.rgbButtons[0] = diJoyState.rgbButtons[0];
                diHoldJoy.rgbButtons[1] = diJoyState.rgbButtons[1];
                diHoldJoy.rgbButtons[2] = diJoyState.rgbButtons[2];
                diHoldJoy.rgbButtons[3] = diJoyState.rgbButtons[3];

                event.type = ev_joystick;
                event.data1 = ((diJoyState.rgbButtons[0] & 0x80) >> 7) |
                              ((diJoyState.rgbButtons[1] & 0x80) >> 6) |
                              ((diJoyState.rgbButtons[2] & 0x80) >> 5) |
                              ((diJoyState.rgbButtons[3] & 0x80) >> 4);
                event.data2 = diJoyState.lX;
                event.data3 = diJoyState.lY;
                D_PostEvent(&event);
               }
           }
       }

    RetryKeyboard:;
        hresult = lpKeyboard->lpVtbl->GetDeviceState(lpKeyboard, sizeof(diKeyState), &diKeyState);
        if (hresult == DIERR_INPUTLOST)
           {
            hresult = lpKeyboard->lpVtbl->Acquire(lpKeyboard);
            if (SUCCEEDED(hresult))
                goto RetryKeyboard;
           }
        else
        if (hresult != DI_OK)
           {
            DI_Error(hresult, "GetDeviceState (keyboard)");
           }
        else
           {
            HandleKeyStrokes();
           }

    MY_DoomLoop();
   }

void HandleKeyStrokes()
   {
    static  event_t  event;

    if (((diKeyState[DIK_BACK] & 0x80) == 0) && (si_Kbd[DIK_BACK] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = KEY_BACKSPACE;
        D_PostEvent(&event);
        si_Kbd[DIK_BACK] = KS_KEYUP;
       }

    if ((diKeyState[DIK_BACK] & 0x80) && (si_Kbd[DIK_BACK] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = KEY_BACKSPACE;
        D_PostEvent(&event);
        si_Kbd[DIK_BACK] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_ESCAPE] & 0x80) == 0) && (si_Kbd[DIK_ESCAPE] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = KEY_ESCAPE;
        D_PostEvent(&event);
        si_Kbd[DIK_ESCAPE] = KS_KEYUP;
       }

    if ((diKeyState[DIK_ESCAPE] & 0x80) && (si_Kbd[DIK_ESCAPE] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = KEY_ESCAPE;
        D_PostEvent(&event);
        si_Kbd[DIK_ESCAPE] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_GRAVE] & 0x80) == 0) && (si_Kbd[DIK_GRAVE] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = KEY_CONSOLE;
        D_PostEvent(&event);
        si_Kbd[DIK_GRAVE] = KS_KEYUP;
       }

    if ((diKeyState[DIK_GRAVE] & 0x80) && (si_Kbd[DIK_GRAVE] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = KEY_CONSOLE;
        D_PostEvent(&event);
        si_Kbd[DIK_GRAVE] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_MINUS] & 0x80) == 0) && (si_Kbd[DIK_MINUS] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = KEY_MINUS;
        D_PostEvent(&event);
        si_Kbd[DIK_MINUS] = KS_KEYUP;
       }

    if ((diKeyState[DIK_MINUS] & 0x80) && (si_Kbd[DIK_MINUS] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = KEY_MINUS;
        D_PostEvent(&event);
        si_Kbd[DIK_MINUS] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_ADD] & 0x80) == 0) && (si_Kbd[DIK_ADD] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = '+';
        D_PostEvent(&event);
        si_Kbd[DIK_ADD] = KS_KEYUP;
       }

    if ((diKeyState[DIK_ADD] & 0x80) && (si_Kbd[DIK_ADD] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = '+';
        D_PostEvent(&event);
        si_Kbd[DIK_ADD] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_EQUALS] & 0x80) == 0) && (si_Kbd[DIK_EQUALS] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = KEY_EQUALS;
        D_PostEvent(&event);
        si_Kbd[DIK_EQUALS] = KS_KEYUP;
       }

    if ((diKeyState[DIK_EQUALS] & 0x80) && (si_Kbd[DIK_EQUALS] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = KEY_EQUALS;
        D_PostEvent(&event);
        si_Kbd[DIK_EQUALS] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_UP] & 0x80) == 0) && (si_Kbd[DIK_UP] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = KEY_UPARROW;
        D_PostEvent(&event);
        si_Kbd[DIK_UP] = KS_KEYUP;
       }

    if ((diKeyState[DIK_UP] & 0x80) && (si_Kbd[DIK_UP] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = KEY_UPARROW;
        D_PostEvent(&event);
        si_Kbd[DIK_UP] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_DOWN] & 0x80) == 0) && (si_Kbd[DIK_DOWN] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = KEY_DOWNARROW;
        D_PostEvent(&event);
        si_Kbd[DIK_DOWN] = KS_KEYUP;
       }

    if ((diKeyState[DIK_DOWN] & 0x80) && (si_Kbd[DIK_DOWN] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = KEY_DOWNARROW;
        D_PostEvent(&event);
        si_Kbd[DIK_DOWN] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_LEFT] & 0x80) == 0) && (si_Kbd[DIK_LEFT] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = KEY_LEFTARROW;
        D_PostEvent(&event);
        si_Kbd[DIK_LEFT] = KS_KEYUP;
       }

    if ((diKeyState[DIK_LEFT] & 0x80) && (si_Kbd[DIK_LEFT] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = KEY_LEFTARROW;
        D_PostEvent(&event);
        si_Kbd[DIK_LEFT] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_RIGHT] & 0x80) == 0) && (si_Kbd[DIK_RIGHT] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = KEY_RIGHTARROW;
        D_PostEvent(&event);
        si_Kbd[DIK_RIGHT] = KS_KEYUP;
       }

    if ((diKeyState[DIK_RIGHT] & 0x80) && (si_Kbd[DIK_RIGHT] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = KEY_RIGHTARROW;
        D_PostEvent(&event);
        si_Kbd[DIK_RIGHT] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_RETURN] & 0x80) == 0) && (si_Kbd[DIK_RETURN] == KS_KEYDOWN))
        si_Kbd[DIK_RETURN] = KS_KEYUP;

    if ((diKeyState[DIK_RETURN] & 0x80) && (si_Kbd[DIK_RETURN] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = KEY_ENTER;
        D_PostEvent(&event);
        si_Kbd[DIK_RETURN] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_F1] & 0x80) == 0) && (si_Kbd[DIK_F1] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = KEY_F1;
        D_PostEvent(&event);
        si_Kbd[DIK_F1] = KS_KEYUP;
       }

    if ((diKeyState[DIK_F1] & 0x80) && (si_Kbd[DIK_F1] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = KEY_F1;
        D_PostEvent(&event);
        si_Kbd[DIK_F1] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_F2] & 0x80) == 0) && (si_Kbd[DIK_F2] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = KEY_F2;
        D_PostEvent(&event);
        si_Kbd[DIK_F2] = KS_KEYUP;
       }

    if ((diKeyState[DIK_F2] & 0x80) && (si_Kbd[DIK_F2] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = KEY_F2;
        D_PostEvent(&event);
        si_Kbd[DIK_F2] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_F3] & 0x80) == 0) && (si_Kbd[DIK_F3] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = KEY_F3;
        D_PostEvent(&event);
        si_Kbd[DIK_F3] = KS_KEYUP;
       }

    if ((diKeyState[DIK_F3] & 0x80) && (si_Kbd[DIK_F3] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = KEY_F3;
        D_PostEvent(&event);
        si_Kbd[DIK_F3] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_F4] & 0x80) == 0) && (si_Kbd[DIK_F4] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = KEY_F4;
        D_PostEvent(&event);
        si_Kbd[DIK_F4] = KS_KEYUP;
       }

    if ((diKeyState[DIK_F4] & 0x80) && (si_Kbd[DIK_F4] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = KEY_F4;
        D_PostEvent(&event);
        si_Kbd[DIK_F4] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_F5] & 0x80) == 0) && (si_Kbd[DIK_F5] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = KEY_F5;
        D_PostEvent(&event);
        si_Kbd[DIK_F5] = KS_KEYUP;
       }

    if ((diKeyState[DIK_F5] & 0x80) && (si_Kbd[DIK_F5] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = KEY_F5;
        D_PostEvent(&event);
        si_Kbd[DIK_F5] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_F6] & 0x80) == 0) && (si_Kbd[DIK_F6] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = KEY_F6;
        D_PostEvent(&event);
        si_Kbd[DIK_F6] = KS_KEYUP;
       }

    if ((diKeyState[DIK_F6] & 0x80) && (si_Kbd[DIK_F6] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = KEY_F6;
        D_PostEvent(&event);
        si_Kbd[DIK_F6] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_F7] & 0x80) == 0) && (si_Kbd[DIK_F7] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = KEY_F7;
        D_PostEvent(&event);
        si_Kbd[DIK_F7] = KS_KEYUP;
       }

    if ((diKeyState[DIK_F7] & 0x80) && (si_Kbd[DIK_F7] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = KEY_F7;
        D_PostEvent(&event);
        si_Kbd[DIK_F7] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_F8] & 0x80) == 0) && (si_Kbd[DIK_F8] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = KEY_F8;
        D_PostEvent(&event);
        si_Kbd[DIK_F8] = KS_KEYUP;
       }

    if ((diKeyState[DIK_F8] & 0x80) && (si_Kbd[DIK_F8] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = KEY_F8;
        D_PostEvent(&event);
        si_Kbd[DIK_F8] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_F9] & 0x80) == 0) && (si_Kbd[DIK_F9] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = KEY_F9;
        D_PostEvent(&event);
        si_Kbd[DIK_F9] = KS_KEYUP;
       }

    if ((diKeyState[DIK_F9] & 0x80) && (si_Kbd[DIK_F9] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = KEY_F9;
        D_PostEvent(&event);
        si_Kbd[DIK_F9] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_F10] & 0x80) == 0) && (si_Kbd[DIK_F10] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = KEY_F10;
        D_PostEvent(&event);
        si_Kbd[DIK_F10] = KS_KEYUP;
       }

    if ((diKeyState[DIK_F10] & 0x80) && (si_Kbd[DIK_F10] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = KEY_F10;
        D_PostEvent(&event);
        si_Kbd[DIK_F10] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_F11] & 0x80) == 0) && (si_Kbd[DIK_F11] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = KEY_F11;
        D_PostEvent(&event);
        si_Kbd[DIK_F11] = KS_KEYUP;
       }

    if ((diKeyState[DIK_F11] & 0x80) && (si_Kbd[DIK_F11] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = KEY_F11;
        D_PostEvent(&event);
        si_Kbd[DIK_F11] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_F12] & 0x80) == 0) && (si_Kbd[DIK_F12] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = KEY_F12;
        D_PostEvent(&event);
        si_Kbd[DIK_F12] = KS_KEYUP;
       }

    if ((diKeyState[DIK_F12] & 0x80) && (si_Kbd[DIK_F12] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = KEY_F12;
        D_PostEvent(&event);
        si_Kbd[DIK_F12] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_0] & 0x80) == 0) && (si_Kbd[DIK_0] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = '0';
        D_PostEvent(&event);
        si_Kbd[DIK_0] = KS_KEYUP;
       }

    if ((diKeyState[DIK_0] & 0x80) && (si_Kbd[DIK_0] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = '0';
        D_PostEvent(&event);
        si_Kbd[DIK_0] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_1] & 0x80) == 0) && (si_Kbd[DIK_1] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = '1';
        D_PostEvent(&event);
        si_Kbd[DIK_1] = KS_KEYUP;
       }

    if ((diKeyState[DIK_1] & 0x80) && (si_Kbd[DIK_1] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = '1';
        D_PostEvent(&event);
        si_Kbd[DIK_1] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_2] & 0x80) == 0) && (si_Kbd[DIK_2] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = '2';
        D_PostEvent(&event);
        si_Kbd[DIK_2] = KS_KEYUP;
       }

    if ((diKeyState[DIK_2] & 0x80) && (si_Kbd[DIK_2] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = '2';
        D_PostEvent(&event);
        si_Kbd[DIK_2] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_3] & 0x80) == 0) && (si_Kbd[DIK_3] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = '3';
        D_PostEvent(&event);
        si_Kbd[DIK_3] = KS_KEYUP;
       }

    if ((diKeyState[DIK_3] & 0x80) && (si_Kbd[DIK_3] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = '3';
        D_PostEvent(&event);
        si_Kbd[DIK_3] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_4] & 0x80) == 0) && (si_Kbd[DIK_4] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = '4';
        D_PostEvent(&event);
        si_Kbd[DIK_4] = KS_KEYUP;
       }

    if ((diKeyState[DIK_4] & 0x80) && (si_Kbd[DIK_4] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = '4';
        D_PostEvent(&event);
        si_Kbd[DIK_4] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_5] & 0x80) == 0) && (si_Kbd[DIK_5] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = '5';
        D_PostEvent(&event);
        si_Kbd[DIK_5] = KS_KEYUP;
       }

    if ((diKeyState[DIK_5] & 0x80) && (si_Kbd[DIK_5] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = '5';
        D_PostEvent(&event);
        si_Kbd[DIK_5] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_6] & 0x80) == 0) && (si_Kbd[DIK_6] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = '6';
        D_PostEvent(&event);
        si_Kbd[DIK_6] = KS_KEYUP;
       }

    if ((diKeyState[DIK_6] & 0x80) && (si_Kbd[DIK_6] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = '6';
        D_PostEvent(&event);
        si_Kbd[DIK_6] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_7] & 0x80) == 0) && (si_Kbd[DIK_7] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = '7';
        D_PostEvent(&event);
        si_Kbd[DIK_7] = KS_KEYUP;
       }

    if ((diKeyState[DIK_7] & 0x80) && (si_Kbd[DIK_7] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = '7';
        D_PostEvent(&event);
        si_Kbd[DIK_7] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_8] & 0x80) == 0) && (si_Kbd[DIK_8] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = '8';
        D_PostEvent(&event);
        si_Kbd[DIK_8] = KS_KEYUP;
       }

    if ((diKeyState[DIK_8] & 0x80) && (si_Kbd[DIK_8] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = '8';
        D_PostEvent(&event);
        si_Kbd[DIK_8] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_9] & 0x80) == 0) && (si_Kbd[DIK_9] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = '9';
        D_PostEvent(&event);
        si_Kbd[DIK_9] = KS_KEYUP;
       }

    if ((diKeyState[DIK_9] & 0x80) && (si_Kbd[DIK_9] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = '9';
        D_PostEvent(&event);
        si_Kbd[DIK_9] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_A] & 0x80) == 0) && (si_Kbd[DIK_A] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = 'a';
        D_PostEvent(&event);
        si_Kbd[DIK_A] = KS_KEYUP;
       }

    if ((diKeyState[DIK_A] & 0x80) && (si_Kbd[DIK_A] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = 'a';
        D_PostEvent(&event);
        si_Kbd[DIK_A] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_B] & 0x80) == 0) && (si_Kbd[DIK_B] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = 'b';
        D_PostEvent(&event);
        si_Kbd[DIK_B] = KS_KEYUP;
       }

    if ((diKeyState[DIK_B] & 0x80) && (si_Kbd[DIK_B] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = 'b';
        D_PostEvent(&event);
        si_Kbd[DIK_B] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_C] & 0x80) == 0) && (si_Kbd[DIK_C] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = 'c';
        D_PostEvent(&event);
        si_Kbd[DIK_C] = KS_KEYUP;
       }

    if ((diKeyState[DIK_C] & 0x80) && (si_Kbd[DIK_C] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = 'c';
        D_PostEvent(&event);
        //PlayCDMusic();
        si_Kbd[DIK_C] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_D] & 0x80) == 0) && (si_Kbd[DIK_D] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = 'd';
        D_PostEvent(&event);
        si_Kbd[DIK_D] = KS_KEYUP;
       }

    if ((diKeyState[DIK_D] & 0x80) && (si_Kbd[DIK_D] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = 'd';
        D_PostEvent(&event);
        si_Kbd[DIK_D] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_E] & 0x80) == 0) && (si_Kbd[DIK_E] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = 'e';
        D_PostEvent(&event);
        si_Kbd[DIK_E] = KS_KEYUP;
       }

    if ((diKeyState[DIK_E] & 0x80) && (si_Kbd[DIK_E] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = 'e';
        D_PostEvent(&event);
        si_Kbd[DIK_E] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_F] & 0x80) == 0) && (si_Kbd[DIK_F] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = 'f';
        D_PostEvent(&event);
        si_Kbd[DIK_F] = KS_KEYUP;
       }

    if ((diKeyState[DIK_F] & 0x80) && (si_Kbd[DIK_F] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = 'f';
        D_PostEvent(&event);
        si_Kbd[DIK_F] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_G] & 0x80) == 0) && (si_Kbd[DIK_G] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = 'g';
        D_PostEvent(&event);
        si_Kbd[DIK_G] = KS_KEYUP;
       }

    if ((diKeyState[DIK_G] & 0x80) && (si_Kbd[DIK_G] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = 'g';
        D_PostEvent(&event);
        si_Kbd[DIK_G] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_H] & 0x80) == 0) && (si_Kbd[DIK_H] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = 'h';
        D_PostEvent(&event);
        si_Kbd[DIK_H] = KS_KEYUP;
       }

    if ((diKeyState[DIK_H] & 0x80) && (si_Kbd[DIK_H] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = 'h';
        D_PostEvent(&event);
        si_Kbd[DIK_H] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_I] & 0x80) == 0) && (si_Kbd[DIK_I] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = 'i';
        D_PostEvent(&event);
        si_Kbd[DIK_I] = KS_KEYUP;
       }

    if ((diKeyState[DIK_I] & 0x80) && (si_Kbd[DIK_I] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = 'i';
        D_PostEvent(&event);
        si_Kbd[DIK_I] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_J] & 0x80) == 0) && (si_Kbd[DIK_J] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = 'j';
        D_PostEvent(&event);
        si_Kbd[DIK_J] = KS_KEYUP;
       }

    if ((diKeyState[DIK_J] & 0x80) && (si_Kbd[DIK_J] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = 'j';
        D_PostEvent(&event);
        si_Kbd[DIK_J] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_K] & 0x80) == 0) && (si_Kbd[DIK_K] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = 'k';
        D_PostEvent(&event);
        si_Kbd[DIK_K] = KS_KEYUP;
       }

    if ((diKeyState[DIK_K] & 0x80) && (si_Kbd[DIK_K] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = 'k';
        D_PostEvent(&event);
        si_Kbd[DIK_K] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_L] & 0x80) == 0) && (si_Kbd[DIK_L] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = 'l';
        D_PostEvent(&event);
        si_Kbd[DIK_L] = KS_KEYUP;
       }

    if ((diKeyState[DIK_L] & 0x80) && (si_Kbd[DIK_L] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = 'l';
        D_PostEvent(&event);
        si_Kbd[DIK_L] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_M] & 0x80) == 0) && (si_Kbd[DIK_M] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = 'm';
        D_PostEvent(&event);
        si_Kbd[DIK_M] = KS_KEYUP;
       }

    if ((diKeyState[DIK_M] & 0x80) && (si_Kbd[DIK_M] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = 'm';
        D_PostEvent(&event);
        //PlayMidiMusic();
        si_Kbd[DIK_M] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_N] & 0x80) == 0) && (si_Kbd[DIK_N] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = 'n';
        D_PostEvent(&event);
        si_Kbd[DIK_N] = KS_KEYUP;
       }

    if ((diKeyState[DIK_N] & 0x80) && (si_Kbd[DIK_N] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = 'n';
        D_PostEvent(&event);
        //PlayNextSong();
        si_Kbd[DIK_N] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_O] & 0x80) == 0) && (si_Kbd[DIK_O] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = 'o';
        D_PostEvent(&event);
        si_Kbd[DIK_O] = KS_KEYUP;
       }

    if ((diKeyState[DIK_O] & 0x80) && (si_Kbd[DIK_O] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = 'o';
        D_PostEvent(&event);
        si_Kbd[DIK_O] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_P] & 0x80) == 0) && (si_Kbd[DIK_P] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = 'p';
        D_PostEvent(&event);
        si_Kbd[DIK_P] = KS_KEYUP;
       }

    if ((diKeyState[DIK_P] & 0x80) && (si_Kbd[DIK_P] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = 'p';
        D_PostEvent(&event);
        //PauseResumeMusic();
        si_Kbd[DIK_P] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_Q] & 0x80) == 0) && (si_Kbd[DIK_Q] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = 'q';
        D_PostEvent(&event);
        si_Kbd[DIK_Q] = KS_KEYUP;
       }

    if ((diKeyState[DIK_Q] & 0x80) && (si_Kbd[DIK_Q] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = 'q';
        D_PostEvent(&event);
        si_Kbd[DIK_Q] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_R] & 0x80) == 0) && (si_Kbd[DIK_R] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = 'r';
        D_PostEvent(&event);
        si_Kbd[DIK_R] = KS_KEYUP;
       }

    if ((diKeyState[DIK_R] & 0x80) && (si_Kbd[DIK_R] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = 'r';
        D_PostEvent(&event);
        si_Kbd[DIK_R] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_S] & 0x80) == 0) && (si_Kbd[DIK_S] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = 's';
        D_PostEvent(&event);
        si_Kbd[DIK_S] = KS_KEYUP;
       }

    if ((diKeyState[DIK_S] & 0x80) && (si_Kbd[DIK_S] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = 's';
        D_PostEvent(&event);
        si_Kbd[DIK_S] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_T] & 0x80) == 0) && (si_Kbd[DIK_T] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = 't';
        D_PostEvent(&event);
        si_Kbd[DIK_T] = KS_KEYUP;
       }

    if ((diKeyState[DIK_T] & 0x80) && (si_Kbd[DIK_T] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = 't';
        D_PostEvent(&event);
        si_Kbd[DIK_T] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_U] & 0x80) == 0) && (si_Kbd[DIK_U] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = 'u';
        D_PostEvent(&event);
        si_Kbd[DIK_U] = KS_KEYUP;
       }

    if ((diKeyState[DIK_U] & 0x80) && (si_Kbd[DIK_U] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = 'u';
        D_PostEvent(&event);
        si_Kbd[DIK_U] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_V] & 0x80) == 0) && (si_Kbd[DIK_V] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = 'v';
        D_PostEvent(&event);
        si_Kbd[DIK_V] = KS_KEYUP;
       }

    if ((diKeyState[DIK_V] & 0x80) && (si_Kbd[DIK_V] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = 'v';
        D_PostEvent(&event);
        si_Kbd[DIK_V] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_W] & 0x80) == 0) && (si_Kbd[DIK_W] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = 'w';
        D_PostEvent(&event);
        si_Kbd[DIK_W] = KS_KEYUP;
       }

    if ((diKeyState[DIK_W] & 0x80) && (si_Kbd[DIK_W] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = 'w';
        D_PostEvent(&event);
        si_Kbd[DIK_W] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_X] & 0x80) == 0) && (si_Kbd[DIK_X] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = 'x';
        D_PostEvent(&event);
        si_Kbd[DIK_X] = KS_KEYUP;
       }

    if ((diKeyState[DIK_X] & 0x80) && (si_Kbd[DIK_X] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = 'x';
        D_PostEvent(&event);
        si_Kbd[DIK_X] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_Y] & 0x80) == 0) && (si_Kbd[DIK_Y] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = 'y';
        D_PostEvent(&event);
        si_Kbd[DIK_Y] = KS_KEYUP;
       }

    if ((diKeyState[DIK_Y] & 0x80) && (si_Kbd[DIK_Y] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = 'y';
        D_PostEvent(&event);
        si_Kbd[DIK_Y] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_Z] & 0x80) == 0) && (si_Kbd[DIK_Z] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = 'z';
        D_PostEvent(&event);
        si_Kbd[DIK_Z] = KS_KEYUP;
       }

    if ((diKeyState[DIK_Z] & 0x80) && (si_Kbd[DIK_Z] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = 'z';
        D_PostEvent(&event);
        si_Kbd[DIK_Z] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_SPACE] & 0x80) == 0) && (si_Kbd[DIK_SPACE] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = ' ';
        D_PostEvent(&event);
        si_Kbd[DIK_SPACE] = KS_KEYUP;
       }

    if ((diKeyState[DIK_SPACE] & 0x80) && (si_Kbd[DIK_SPACE] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = ' ';
        D_PostEvent(&event);
        si_Kbd[DIK_SPACE] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_COMMA] & 0x80) == 0) && (si_Kbd[DIK_COMMA] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = ',';
        D_PostEvent(&event);
        si_Kbd[DIK_COMMA] = KS_KEYUP;
       }

    if ((diKeyState[DIK_COMMA] & 0x80) && (si_Kbd[DIK_COMMA] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = ',';
        D_PostEvent(&event);
        si_Kbd[DIK_COMMA] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_PERIOD] & 0x80) == 0) && (si_Kbd[DIK_PERIOD] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = '.';
        D_PostEvent(&event);
        si_Kbd[DIK_PERIOD] = KS_KEYUP;
       }

    if ((diKeyState[DIK_PERIOD] & 0x80) && (si_Kbd[DIK_PERIOD] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = '.';
        D_PostEvent(&event);
        si_Kbd[DIK_PERIOD] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_DECIMAL] & 0x80) == 0) && (si_Kbd[DIK_DECIMAL] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = '.';
        D_PostEvent(&event);
        si_Kbd[DIK_DECIMAL] = KS_KEYUP;
       }

    if ((diKeyState[DIK_DECIMAL] & 0x80) && (si_Kbd[DIK_DECIMAL] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = '.';
        D_PostEvent(&event);
        si_Kbd[DIK_DECIMAL] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_RSHIFT] & 0x80) == 0) && (si_Kbd[DIK_RSHIFT] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = KEY_RSHIFT;
        D_PostEvent(&event);
        si_Kbd[DIK_RSHIFT] = KS_KEYUP;
       }

    if ((diKeyState[DIK_RSHIFT] & 0x80) && (si_Kbd[DIK_RSHIFT] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = KEY_RSHIFT;
        D_PostEvent(&event);
        si_Kbd[DIK_RSHIFT] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_LSHIFT] & 0x80) == 0) && (si_Kbd[DIK_LSHIFT] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = KEY_RSHIFT;
        D_PostEvent(&event);
        si_Kbd[DIK_LSHIFT] = KS_KEYUP;
       }

    if ((diKeyState[DIK_LSHIFT] & 0x80) && (si_Kbd[DIK_LSHIFT] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = KEY_RSHIFT;
        D_PostEvent(&event);
        si_Kbd[DIK_LSHIFT] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_RCONTROL] & 0x80) == 0) && (si_Kbd[DIK_RCONTROL] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = KEY_RCTRL;
        D_PostEvent(&event);
        si_Kbd[DIK_RCONTROL] = KS_KEYUP;
       }

    if ((diKeyState[DIK_RCONTROL] & 0x80) && (si_Kbd[DIK_RCONTROL] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = KEY_RCTRL;
        D_PostEvent(&event);
        si_Kbd[DIK_RCONTROL] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_LCONTROL] & 0x80) == 0) && (si_Kbd[DIK_LCONTROL] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = KEY_RCTRL;
        D_PostEvent(&event);
        si_Kbd[DIK_LCONTROL] = KS_KEYUP;
       }

    if ((diKeyState[DIK_LCONTROL] & 0x80) && (si_Kbd[DIK_LCONTROL] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = KEY_RCTRL;
        D_PostEvent(&event);
        si_Kbd[DIK_LCONTROL] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_SYSRQ] & 0x80) == 0) && (si_Kbd[DIK_SYSRQ] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = KEY_SCRNSHOT;
        D_PostEvent(&event);
        si_Kbd[DIK_SYSRQ] = KS_KEYUP;
       }

    if ((diKeyState[DIK_SYSRQ] & 0x80) && (si_Kbd[DIK_SYSRQ] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = KEY_SCRNSHOT;
        D_PostEvent(&event);
        si_Kbd[DIK_SYSRQ] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_RMENU] & 0x80) == 0) && (si_Kbd[DIK_RMENU] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = KEY_RALT;
        D_PostEvent(&event);
        si_Kbd[DIK_RMENU] = KS_KEYUP;
       }

    if ((diKeyState[DIK_RMENU] & 0x80) && (si_Kbd[DIK_RMENU] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = KEY_RALT;
        D_PostEvent(&event);
        si_Kbd[DIK_RMENU] = KS_KEYDOWN;
       }

    if (((diKeyState[DIK_LMENU] & 0x80) == 0) && (si_Kbd[DIK_LMENU] == KS_KEYDOWN))
       {
        event.type = ev_keyup;
        event.data1 = KEY_RALT;
        D_PostEvent(&event);
        si_Kbd[DIK_LMENU] = KS_KEYUP;
       }

    if ((diKeyState[DIK_LMENU] & 0x80) && (si_Kbd[DIK_LMENU] == KS_KEYUP))
       {
        event.type = ev_keydown;
        event.data1 = KEY_RALT;
        D_PostEvent(&event);
        si_Kbd[DIK_LMENU] = KS_KEYDOWN;
       }

/*

These keys have not been mapped to DOOM keystrokes (yet)...

#define DIK_LBRACKET        0x1A
#define DIK_RBRACKET        0x1B
#define DIK_SEMICOLON       0x27
#define DIK_APOSTROPHE      0x28
#define DIK_BACKSLASH       0x2B
#define DIK_SLASH           0x35    //   on main keyboard
#define DIK_CAPITAL         0x3A

#define DIK_SCROLL          0x46    // Scroll Lock

#define DIK_NUMLOCK         0x45
#define DIK_NUMPAD7         0x47
#define DIK_NUMPAD8         0x48
#define DIK_NUMPAD9         0x49
#define DIK_NUMPAD4         0x4B
#define DIK_NUMPAD5         0x4C
#define DIK_NUMPAD6         0x4D
#define DIK_NUMPAD1         0x4F
#define DIK_NUMPAD2         0x50
#define DIK_NUMPAD3         0x51
#define DIK_NUMPAD0         0x52
#define DIK_SUBTRACT        0x4A    // - on numeric keypad
#define DIK_DIVIDE          0xB5    // / on numeric keypad
#define DIK_MULTIPLY        0x37    //   on numeric keypad
#define DIK_NUMPADENTER     0x9C    // Enter on numeric keypad

#define DIK_HOME            0xC7    // Home on arrow keypad
#define DIK_PRIOR           0xC9    // PgUp on arrow keypad
#define DIK_END             0xCF    // End on arrow keypad
#define DIK_NEXT            0xD1    // PgDn on arrow keypad
#define DIK_INSERT          0xD2    // Insert on arrow keypad
#define DIK_DELETE          0xD3    // Delete on arrow keypad

#define DIK_LWIN            0xDB    // Left Windows key
#define DIK_RWIN            0xDC    // Right Windows key

#define DIK_APPS            0xDD    // AppMenu key

*/

   }

void WinDoomExit()
   {
    GameMode = GAME_LIMBO;
    ShutDownDirectPlay();
    ShutdownDirectSound();
    ShutdownDirectInput();
    ShutdownDirectDraw();
    StopMusic();
    SendMessage(hMainWnd, WM_CLOSE, 0, 0);
   };

LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
   {
    static HDC         hDC;
    static PAINTSTRUCT ps;
    static event_t     event;
    static unsigned char KeyPress;
    static int         scancode;

    switch(iMsg)
       {
        case WM_CREATE:
             GetCDInfo(hwnd);
             break;

        case MM_MCINOTIFY:
             if (wParam == MCI_NOTIFY_SUCCESSFUL)
                {
                 if (MidiData.MidiStatus == midi_play)
                    MidiReplay(hwnd, &MidiData);
                 if (CDData.CDStatus == cd_play)
                    CDTrackPlay(hwnd, &CDData);
                }
             if (wParam == MCI_NOTIFY_FAILURE)
                {
                 if (CDData.CDStatus == cd_play)
                    {
                     MidiPlay(hwnd, &MidiData);
                     CDData.CDStatus == cd_stop;
                    }
                }
             break;

       case WM_KEYDOWN:
            if ((lParam & 0x40000000) != 0)  // This "debounces" the keys so that we only process
               break;                        // the message when the key is first pressed and not after.

            switch(wParam)
               {
                case VK_PAUSE:
                     event.type = ev_keydown;
                     event.data1 = KEY_PAUSE;
                     D_PostEvent(&event);
                     break;
                case VK_TAB:
                     event.type = ev_keydown;
                     event.data1 = KEY_TAB;
                     D_PostEvent(&event);
                     break;
               }
            break;

       case WM_ACTIVATE:
            if (hwnd == (HWND)lParam)
               {
                if (LOWORD(wParam) == WA_INACTIVE)
                   GameMode = GAME_PLAY;
                else
                if (LOWORD(wParam) == WA_ACTIVE)
                   GameMode = GAME_PAUSE;
               }
            break;

       case WM_KEYUP:
            switch(wParam)
               {
                case VK_PAUSE:
                     event.type = ev_keyup;
                     event.data1 = KEY_PAUSE;
                     D_PostEvent(&event);
                     break;
                case VK_TAB:
                     event.type = ev_keyup;
                     event.data1 = KEY_TAB;
                     D_PostEvent(&event);
                     break;
               }
            break;

        case WM_DESTROY:
             PostQuitMessage(0);
             return 0;
       }
    return DefWindowProc(hwnd, iMsg, wParam, lParam);
   }

void WriteDebug(char *Message)
   {
    FILE *fn;

    fn = fopen(szDbgName, "a+");
    fprintf(fn, "%s", Message);
    fclose(fn);
   }

void GetWindowsVersion()
   {
    OSVERSIONINFO OSVersionInfo;
    FILE *fn;

    fn = fopen(szDbgName, "a+");

    OSVersionInfo.dwOSVersionInfoSize = sizeof(OSVersionInfo);
    GetVersionEx(&OSVersionInfo);

    switch(OSVersionInfo.dwPlatformId)
        {
         case VER_PLATFORM_WIN32s:
              fprintf(fn, "Platform is Windows %d.%02d\n", OSVersionInfo.dwMajorVersion,
                                                           OSVersionInfo.dwMinorVersion);
              break;
         case VER_PLATFORM_WIN32_WINDOWS:
              fprintf(fn, "Platform is Windows 9X ");
              fprintf(fn, "Version %d.%02d.%d\n", OSVersionInfo.dwMajorVersion,
                                                 OSVersionInfo.dwMinorVersion,
                                                 OSVersionInfo.dwBuildNumber & 0xFFFF);
              break;
         case VER_PLATFORM_WIN32_NT:
              fprintf(fn, "Platform is Windows NT ");
              fprintf(fn, "Version %d.%02d ", OSVersionInfo.dwMajorVersion, OSVersionInfo.dwMinorVersion);
              fprintf(fn, "Build %d\n", OSVersionInfo.dwBuildNumber & 0xFFFF);
              break;
        }
    if (strlen(OSVersionInfo.szCSDVersion) > 0)
       fprintf(fn, "Windows Info: %s\n", OSVersionInfo.szCSDVersion);
    fclose(fn);
   }


/////////////////////////////////////////////////////////////////////////////////////
// DIRECTX
/////////////////////////////////////////////////////////////////////////////////////
// The next section of code deals with Microsoft's DirectX.
/////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////
// DIRECTSOUND - Sound effects
/////////////////////////////////////////////////////////////////////////////////////

BOOL SetupDirectSound()
   {
    HRESULT        hresult;
    int            buff;
    DSBUFFERDESC   dsbdesc;
    WAVEFORMATEX   wfx;

    // Create an instance of DirectSound
    hresult = DirectSoundCreate(NULL, &lpDS, NULL);
    if (hresult != DS_OK)
       {
        DS_Error(hresult, "DirectSoundCreate");
        for (buff = 0; buff < NUM_SOUND_FX; buff++)
           lpDSBuffer[buff] = 0;
        return FALSE;
       }

    // Set the cooperative level so it doesn't get confused
    hresult = lpDS->lpVtbl->SetCooperativeLevel(lpDS, hMainWnd, DSSCL_EXCLUSIVE);
    if (hresult != DS_OK)
       DS_Error(hresult, "DirectSound.SetCooperativeLevel");

    // Set up DSBUFFERDESC structure.
    memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));  // Zero it out.
    dsbdesc.dwSize              = sizeof(DSBUFFERDESC);
    dsbdesc.dwFlags             = DSBCAPS_PRIMARYBUFFER;
    dsbdesc.dwBufferBytes       = 0;
    dsbdesc.lpwfxFormat         = NULL;

    hresult = lpDS->lpVtbl->CreateSoundBuffer(lpDS, &dsbdesc, &lpDSPrimary, NULL);
    if (hresult != DS_OK)
       {
        DS_Error(hresult, "DirectSound.CreateSoundBuffer - Primary");
       }

    // Set up wave format structure.
    memset( &wfx, 0, sizeof(WAVEFORMATEX) );
    wfx.wFormatTag         = WAVE_FORMAT_PCM;      
    wfx.nChannels          = 2;
    wfx.nSamplesPerSec     = 11025;
    wfx.nAvgBytesPerSec    = 11025*2*1;
    wfx.nBlockAlign        = 2; // ?
    wfx.wBitsPerSample     = (WORD)8;
    wfx.cbSize             = 0;

    hresult = lpDSPrimary->lpVtbl->SetFormat(lpDSPrimary, &wfx);
    if (hresult != DS_OK)
       {
        DS_Error(hresult, "DirectSound.SetFormat - Primary");
       }

    // Set the cooperative level so it doesn't get confused
    hresult = lpDS->lpVtbl->SetCooperativeLevel(lpDS, hMainWnd, DSSCL_NORMAL);
    if (hresult != DS_OK)
       DS_Error(hresult, "DirectSound.SetCooperativeLevel");

    return(TRUE);
   }

void CreateSoundBuffer(int Channel, int length, unsigned char *data)
   {
    HRESULT        hresult;
    //int            buff;
    DSBUFFERDESC   dsbdesc;
    PCMWAVEFORMAT  pcmwf;
    void          *buffer, *buff2;
    DWORD          size1, size2;

    if (Channel > NUM_SOUND_FX)
       {
        WriteDebug("Invalid sound effect...\n");
        return;
       }

    // Set up wave format structure.
    memset( &pcmwf, 0, sizeof(PCMWAVEFORMAT) );
    pcmwf.wf.wFormatTag         = WAVE_FORMAT_PCM;      
    pcmwf.wf.nChannels          = 1;
    pcmwf.wf.nSamplesPerSec     = 11025;
    pcmwf.wf.nBlockAlign        = 1; // ?
    pcmwf.wf.nAvgBytesPerSec    = 11025*1*1;
    pcmwf.wBitsPerSample        = (WORD)8;

    // Set up DSBUFFERDESC structure.
    memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));  // Zero it out.
    dsbdesc.dwSize              = sizeof(DSBUFFERDESC);
    dsbdesc.dwFlags             = DSBCAPS_CTRLDEFAULT|DSBCAPS_CTRLPOSITIONNOTIFY;
    dsbdesc.dwBufferBytes       = length;
    dsbdesc.lpwfxFormat         = (LPWAVEFORMATEX)&pcmwf;

    if ((hresult = lpDS->lpVtbl->CreateSoundBuffer(lpDS, &dsbdesc, &lpDSBuffer[Channel], NULL)) != DS_OK)
       {
        DS_Error(hresult, "DirectSound.CreateSoundBuffer");
        return;
       }

    hresult = lpDSBuffer[Channel]->lpVtbl->Lock(lpDSBuffer[Channel],0,length,&buffer,&size1,&buff2,&size2,DSBLOCK_ENTIREBUFFER );
    if (hresult == DS_OK)
       {
        memcpy(buffer, data, length);
        hresult = lpDSBuffer[Channel]->lpVtbl->Unlock(lpDSBuffer[Channel],buffer,length,buff2,size2);
        if (hresult != DS_OK)
           DS_Error(hresult, "lpDSBuffer.Unlock");
       }
    else
       {
        DS_Error(hresult, "lpDSBuffer.Lock");
       }
   }

void ShutdownDirectSound()
   {
    int buff;

    DWORD BufferStatus;

    for (buff = 0; buff < NUM_SOUND_FX; buff++)
       {
        if (lpDSBuffer[buff] != 0)
           {
            BufferStatus = DSBSTATUS_PLAYING;
            while (BufferStatus == DSBSTATUS_PLAYING)
                lpDSBuffer[buff]->lpVtbl->GetStatus(lpDSBuffer[buff], &BufferStatus);
            RELEASE(lpDSBuffer[buff]);
           }
       }
    RELEASE(lpDSPrimary);
    RELEASE(lpDS);
   }


/////////////////////////////////////////////////////////////////////////////////////
// DirectInput
/////////////////////////////////////////////////////////////////////////////////////

BOOL InitDirectInput()
   {
    HRESULT hresult;

    hresult = DirectInputCreate(hInst, DIRECTINPUT_VERSION, &lpDirectInput, NULL );
    if (hresult != DI_OK)
       {
        DI_Error( hresult, "DirectInputCreate");
        return FALSE;
       }

    hresult = lpDirectInput->lpVtbl->CreateDevice(lpDirectInput, &GUID_SysMouse, &lpMouse, NULL );
    if (hresult != DI_OK)
       {
        DI_Error( hresult, "CreateDevice (mouse)");
        ShutdownDirectInput();
        return FALSE;
       }

    hresult = lpDirectInput->lpVtbl->CreateDevice(lpDirectInput, &GUID_Joystick, &lpJoystick, NULL );
    if (hresult != DI_OK)
       {
        DI_Error( hresult, "CreateDevice (joystick)");
       }

    hresult = lpDirectInput->lpVtbl->CreateDevice(lpDirectInput, &GUID_SysKeyboard, &lpKeyboard, NULL );
    if (hresult != DI_OK)
       {
        DI_Error( hresult, "CreateDevice (keyboard)");
        ShutdownDirectInput();
        return FALSE;
       }

    hresult = lpMouse->lpVtbl->SetDataFormat(lpMouse, &c_dfDIMouse);
    if (hresult != DI_OK)
       {
        DI_Error( hresult, "SetDataFormat (mouse)");
        ShutdownDirectInput();
        return FALSE;
       }

    if (lpJoystick != 0)
       {
        hresult = lpJoystick->lpVtbl->SetDataFormat(lpJoystick, &c_dfDIJoystick);
        if (hresult != DI_OK)
           {
            DI_Error( hresult, "SetDataFormat (joystick)");
            ShutdownDirectInput();
            return FALSE;
           }
       }

    hresult = lpKeyboard->lpVtbl->SetDataFormat(lpKeyboard, &c_dfDIKeyboard);
    if (hresult != DI_OK)
       {
        DI_Error( hresult, "SetDataFormat (keyboard)");
        ShutdownDirectInput();
        return FALSE;
       }

    if (lpJoystick != 0)
       {
        hresult = lpJoystick->lpVtbl->SetCooperativeLevel(lpJoystick, hMainWnd, DISCL_EXCLUSIVE | DISCL_FOREGROUND);
        if (hresult != DI_OK)
           {
            DI_Error( hresult, "SetCooperativeLevel (joystick)");
            ShutdownDirectInput();
            return FALSE;
           }
       }

    hresult = lpMouse->lpVtbl->SetCooperativeLevel(lpMouse, hMainWnd, DISCL_EXCLUSIVE | DISCL_FOREGROUND);
    if (hresult != DI_OK)
       {
        DI_Error( hresult, "SetCooperativeLevel (mouse)");
        ShutdownDirectInput();
        return FALSE;
       }

    hresult = lpKeyboard->lpVtbl->SetCooperativeLevel(lpKeyboard, hMainWnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);
    if (hresult != DI_OK)
       {
        DI_Error( hresult, "SetCooperativeLevel (keyboard)");
        ShutdownDirectInput();
        return FALSE;
       }

    hresult = lpMouse->lpVtbl->Acquire(lpMouse);
    if (lpJoystick != 0)
       hresult = lpJoystick->lpVtbl->Acquire(lpJoystick);
    hresult = lpKeyboard->lpVtbl->Acquire(lpKeyboard);

    return TRUE;
   }


void ShutdownDirectInput()
   {
    if (lpKeyboard != 0)
       {
        lpKeyboard->lpVtbl->Unacquire(lpKeyboard);
        lpKeyboard->lpVtbl->Release(lpKeyboard);
        lpKeyboard = 0;
       }
    if (lpJoystick != 0)
       {
        lpJoystick->lpVtbl->Unacquire(lpJoystick);
        lpJoystick->lpVtbl->Release(lpJoystick);
        lpJoystick = 0;
       }
    if (lpMouse != 0)
       {
        lpMouse->lpVtbl->Unacquire(lpMouse);
        lpMouse->lpVtbl->Release(lpMouse);
        lpMouse = 0;
       }
    RELEASE(lpDirectInput);
   }

/////////////////////////////////////////////////////////////////////////////////////
// Direct Play
/////////////////////////////////////////////////////////////////////////////////////

BOOL SetupDirectPlay()
   {

    HRESULT hresult;

    hresult = CoInitialize(NULL);
    hresult = CoCreateInstance( &CLSID_DirectPlay, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectPlay3A, (LPVOID*)&lpDirectPlay3A);

    if (hresult != DP_OK)
       {
        WriteDebug("CoCreateInstance of DirectPlay FAILED.\n");
        return FALSE;
       }

    hresult = lpDirectPlay3A->lpVtbl->EnumConnections(lpDirectPlay3A,NULL,DPEnumConnCallback,NULL,DPCONNECTION_DIRECTPLAY);

    sprintf(szMsgText, "Direct Play Connection Count : %d\n", iDPConnections);
    WriteDebug(szMsgText);

    return TRUE;
   }

BOOL FAR PASCAL DPEnumConnCallback(LPCGUID lpguidSP, LPVOID lpConnection, DWORD dwConnectionSize,
     LPCDPNAME lpName, DWORD dwFlags, LPVOID lpContext)
   {
/*
    HWND     hWnd = (HWND) lpContext;
    LRESULT  iIndex;
    LPVOID   lpConnectionBuffer;

    // Store service provider name in combo box
    iIndex = SendDlgItemMessage(hWnd, IDC_SPCOMBO, CB_ADDSTRING, 0, (LPARAM) lpName->lpszShortNameA);
    if (iIndex == CB_ERR)
        goto failure;

    // make space for connection 
    lpConnectionBuffer = GlobalAllocPtr(GHND, dwConnectionSize);
    if (lpConnectionBuffer == NULL)
        goto failure;

    // Store pointer to connection  in combo box
    memcpy(lpConnectionBuffer, lpConnection, dwConnectionSize);
    SendDlgItemMessage(hWnd, IDC_SPCOMBO, CB_SETITEMDATA, (WPARAM) iIndex, (LPARAM) lpConnectionBuffer);
*/
    iDPConnections++;
    return TRUE;

/*
failure:
    return TRUE;
*/
   }

void ShutDownDirectPlay()
   {
    if (lpDirectPlay3A != NULL)
       {
        RELEASE(lpDirectPlay3A);
       }
    CoUninitialize();
   }

