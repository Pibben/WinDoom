// Copyright © 1998, Bruce A. Lewis
// This code may not be re-used in a commercial product
// of any kind without express written consent from
// the author, Bruce A. Lewis.
//
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
#include <ddraw.h>
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
#include "doomstat.h"
#include "d_event.h"
#include "d_main.h"
#include "m_argv.h"
#include "g_game.h"
#include "i_system.h"
#include "m_music.h"
#include "i_cd.h"
#include "i_midi.h"
#include "dxerr.h"

#include "d_console.h"

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

#define SB_SIZE       20480
#define NUM_DSBUFFERS 256

BOOL SetupDirectSound();
void ShutdownDirectSound(void);

LPDIRECTSOUND        lpDS;
LPDIRECTSOUNDBUFFER  lpDSPrimary;
LPDIRECTSOUNDBUFFER  lpDSBuffer[NUM_DSBUFFERS];

/////////////////////////////////////////////////////////////////////////////////////
// Direct Input
/////////////////////////////////////////////////////////////////////////////////////

void HandleKeyStrokes(void);
void KeyboardHandler(void);
BOOL InitDirectInput(void);
void ShutdownDirectInput(void);

LPDIRECTINPUT        lpDirectInput = 0;
LPDIRECTINPUTDEVICE  lpMouse       = 0;
LPDIRECTINPUTDEVICE2 lpJoystick    = 0;
LPDIRECTINPUTDEVICE  lpKeyboard    = 0;

BOOL          bDILive = FALSE;
char          szMouseBuf[1024];
unsigned char diKeyState[256];

extern int    always_run;
extern int    mvert;
extern int    keylink;

extern int    joyb1;
extern int    joyb2;
extern int    joyb3;
extern int    joyb4;
extern int    mouseb1;
extern int    mouseb2;
extern int    mouseb3;

/////////////////////////////////////////////////////////////////////////////////////
// Direct Draw
/////////////////////////////////////////////////////////////////////////////////////

void RenderScene(void);
void InitGlobals(void);
BOOL SetupDirectDraw(HWND);
void ShutdownDirectDraw(void);

typedef struct tagD3DAppMode
   {
    int     w;                // width
    int     h;                // height
    int     bpp;              // bits per pixel
    int     refresh;          // Refresh rate
    BOOL    ModeX;            // Is this a ModeX video mode?
    BOOL    bThisDriverCanDo; // can current D3D driver render in this mode?
   }D3DAppMode;

int                  NumModes; // number of available display modes
int                  CurrMode; // number of current display mode (only when fullscreen)
D3DAppMode           Mode[50]; // desc avail modes
D3DAppMode           ThisMode; // description of this mode, identical to Mode[CurrMode]

BOOL                 DDHard3D = 0;
int                  DDCount = 0;
LPDIRECTDRAW         lpDD;          // DirectDraw Interface
LPDIRECTDRAW2        lpDD2;         // DirectDraw Interface
LPDIRECTDRAWSURFACE  lpDDSPrimary;  // DirectDraw Surface
LPDIRECTDRAWSURFACE  lpDDSBack;     // DirectDraw Backbuffer Surface
LPDIRECTDRAWSURFACE  lpDDSZBuf;     // DirectDraw Z-buffer Surface
LPDIRECTDRAWSURFACE  lpDDSBitMap;   // DirectDraw BitMap Surface
LPDIRECTDRAWSURFACE  lpDDSName;     // DirectDraw Menu Name Surface
LPDIRECTDRAWSURFACE  lpDDSMenu;     // DirectDraw Menu Options Surface
LPDIRECTDRAWSURFACE  lpDDSChars;    // DirectDraw Character Sets Surface
LPDIRECTDRAWCLIPPER  lpDDClipper;   // DirectDrawClipper object - windowed mode
int                  BPP;           // bit depth of the current display mode

LPDIRECTDRAWPALETTE  lpPalette = NULL; // Palette
PALETTEENTRY         rPal[768];        // Temp palette used later on

// this structure holds a RGB triple in three unsigned bytes
typedef struct RGB_color_typ
   {
    unsigned char red;      // red   component of color 0-63
    unsigned char green;    // green component of color 0-63
    unsigned char blue;     // blue  component of color 0-63
   }xRGB_color, *xRGB_color_ptr;

// this structure holds an entire color palette
typedef struct xRGB_palette_typ
   {
    int start_reg;          // index of the starting register that is save
    int end_reg;            // index of the ending registe that is saved
    xRGB_color colors[256];  // the storage area for the palette
   }xRGB_palette, *xRGB_palette_ptr;

xRGB_palette xpal;

void CreateDDBitmap(HBITMAP hBitMap, BITMAP *BitMap, LPDIRECTDRAWSURFACE *lpDDSurf);

BOOL FAR PASCAL DDEnumCallback(GUID FAR* lpGUID, LPSTR lpDriverDesc, LPSTR lpDriverName, LPVOID lpContext);
static HRESULT CALLBACK EnumDisplayModesCallback(LPDDSURFACEDESC pddsd, LPVOID lpContext);

BOOL CheckSurfaces(void);

/////////////////////////////////////////////////////////////////////////////////////
// Application Defines and Data
/////////////////////////////////////////////////////////////////////////////////////

#define MUSIC_NONE      0
#define MUSIC_CDAUDIO   1
#define MUSIC_MIDI      2

#define RENDER_GL       0
#define RENDER_D3D      1

#define KS_KEYUP        0
#define KS_KEYDOWN    128

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

extern int usemouse;
extern int usejoystick;

short     si_Kbd[256];

char      szMsgText[256];

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

char *YesNo[] = { "No", "Yes" };

void  ParseCommand(PSTR szCmdLine);

void  GameLoop(void);

void  D_DoomMain(void);
void *W_CacheLumpName(char *, int);
void  I_SetPalette(byte *);
void  MY_DoomSetup(void);
void  MY_DoomLoop(void);
void  WinDoomExit(void);
void  HandleKeyboard(void);

/////////////////////////////////////////////////////////////////////////////////////
// Windows Defines and Data
/////////////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
char      szAppName[] = "WinDoom";
char      szDbgName[] = "WinDoom.dbg";
char      szCfgName[] = "WinDoom.cfg";
HINSTANCE hInst;
HWND      hMainWnd;
HACCEL    ghAccel;

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
    int         p;

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

    ParseCommand(szCmdLine);

    p = M_CheckParm("-width");
    if (p && p < myargc-1)
        SCREENWIDTH = atoi(myargv[p+1]);
    else
        SCREENWIDTH = 320;

    p = M_CheckParm("-height");
    if (p && p < myargc-1)
        SCREENHEIGHT = atoi(myargv[p+1]);
    else
        SCREENHEIGHT = 200;

    sprintf(szMsgText, "Resolution: %d x %d\n",SCREENWIDTH,SCREENHEIGHT);
    WriteDebug(szMsgText);

    // This sets the video mode to video mode described in the default video mode
    InitGlobals();
    if (!SetupDirectDraw(hwnd))
       {
        ShutdownDirectSound();
        exit(0);
       }

    MoveWindow(hwnd, 0,0, Mode[CurrMode].w, Mode[CurrMode].h, TRUE);

    hAccel = LoadAccelerators(hInstance,"AppAccel");
    ghAccel = hAccel;

    bDILive = InitDirectInput();

    for (k = 0; k < 256; k++)
        si_Kbd[k] = KS_KEYUP;

    PlayMidiFile(szMidiFile);

    bQuit = FALSE;

    sprintf(szMsgText, "WinDoom Version %2d.%02d\n", CONSVERS/100, CONSVERS%100);
    WriteDebug(szMsgText);

    D_DoomMain();
    MY_DoomSetup();
    GameMode = GAME_PLAY;

    ShowCursor(FALSE);

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
    ShowCursor(TRUE);
    for (i = 4; i >= 0; i--)
        free(screens[i]);
    for (i = (WinDoomAC-1); i > 0; i--)
        free(WinDoomAV[i]);
    return msg.wParam;
   }

void ParseCommand(PSTR szCmdLine)
   {
    char *s;

    WinDoomAC = 1;
    s = strtok(szCmdLine, " ");
    while (s != NULL)
       {
        WinDoomAV[WinDoomAC] = (char *)malloc(strlen(s)+1);
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
    static  event_t  mbevent;
    static  event_t  jbevent;

    if (usemouse == TRUE)
       {
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
                event.type = ev_mouse;
                event.data1 = 0;
                if (mouseb1 == 0)
                    event.data1 |= ((diMouseState.rgbButtons[0] & 0x80) >> 7);
                else
                   {
                    if ((diHoldMouse.rgbButtons[0] & 0x80) != (diMouseState.rgbButtons[0] & 0x80))
                       {
                        if ((diMouseState.rgbButtons[0] & 0x80) == 0x80)
                           {
                            mbevent.type = ev_keydown;
                           }
                        else
                           {
                            mbevent.type = ev_keyup;
                           }
                        mbevent.data1 = mouseb1;
                        D_PostEvent(&mbevent);
                       }
                   }
                if (mouseb2 == 0)
                    event.data1 |= ((diMouseState.rgbButtons[1] & 0x80) >> 6);
                else
                   {
                    if ((diHoldMouse.rgbButtons[1] & 0x80) != (diMouseState.rgbButtons[1] & 0x80))
                       {
                        if ((diMouseState.rgbButtons[1] & 0x80) == 0x80)
                           {
                            mbevent.type = ev_keydown;
                           }
                        else
                           {
                            mbevent.type = ev_keyup;
                           }
                        mbevent.data1 = mouseb2;
                        D_PostEvent(&mbevent);
                       }
                   }
                if (mouseb3 == 0)
                    event.data1 |= ((diMouseState.rgbButtons[2] & 0x80) >> 5);
                else
                   {
                    if ((diHoldMouse.rgbButtons[2] & 0x80) != (diMouseState.rgbButtons[2] & 0x80))
                       {
                        if ((diMouseState.rgbButtons[2] & 0x80) == 0x80)
                           {
                            mbevent.type = ev_keydown;
                           }
                        else
                           {
                            mbevent.type = ev_keyup;
                           }
                        mbevent.data1 = mouseb3;
                        D_PostEvent(&mbevent);
                       }
                   }
                event.data2 = diMouseState.lX*4;
                if (mvert == TRUE)
                   {
                    event.data3 = (diMouseState.lY * -4);
                   }
                else
                   {
                    event.data3 = 0;
                   }
                D_PostEvent(&event);
                diHoldMouse.lX = diMouseState.lX;
                diHoldMouse.lY = diMouseState.lY;
                diHoldMouse.lZ = diMouseState.lZ;
                diHoldMouse.rgbButtons[0] = diMouseState.rgbButtons[0];
                diHoldMouse.rgbButtons[1] = diMouseState.rgbButtons[1];
                diHoldMouse.rgbButtons[2] = diMouseState.rgbButtons[2];
                diHoldMouse.rgbButtons[3] = diMouseState.rgbButtons[3];
               }
           }
       }

    if (usejoystick == TRUE)
       {
    RetryJoy:;
    if (lpJoystick != 0)
       {
        hresult = lpJoystick->lpVtbl->Poll(lpJoystick);

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
                event.type = ev_joystick;
                event.data1 = 0;
                if (joyb1 == 0)
                    event.data1 |= ((diJoyState.rgbButtons[0] & 0x80) >> 7);
                else
                   {
                    if ((diHoldJoy.rgbButtons[0] & 0x80) != (diJoyState.rgbButtons[0] & 0x80))
                       {
                        if ((diJoyState.rgbButtons[0] & 0x80) == 0x80)
                           {
                            jbevent.type = ev_keydown;
                           }
                        else
                           {
                            jbevent.type = ev_keyup;
                           }
                        jbevent.data1 = joyb1;
                        D_PostEvent(&jbevent);
                       }
                   }
                if (joyb2 == 0)
                    event.data1 |= ((diJoyState.rgbButtons[1] & 0x80) >> 6);
                else
                   {
                    if ((diHoldJoy.rgbButtons[1] & 0x80) != (diJoyState.rgbButtons[1] & 0x80))
                       {
                        if ((diJoyState.rgbButtons[1] & 0x80) == 0x80)
                           {
                            jbevent.type = ev_keydown;
                           }
                        else
                           {
                            jbevent.type = ev_keyup;
                           }
                        jbevent.data1 = joyb2;
                        D_PostEvent(&jbevent);
                       }
                   }
                if (joyb3 == 0)
                    event.data1 |= ((diJoyState.rgbButtons[2] & 0x80) >> 5);
                else
                   {
                    if ((diHoldJoy.rgbButtons[2] & 0x80) != (diJoyState.rgbButtons[2] & 0x80))
                       {
                        if ((diJoyState.rgbButtons[2] & 0x80) == 0x80)
                           {
                            jbevent.type = ev_keydown;
                           }
                        else
                           {
                            jbevent.type = ev_keyup;
                           }
                        jbevent.data1 = joyb3;
                        D_PostEvent(&jbevent);
                       }
                   }
                if (joyb4 == 0)
                    event.data1 |= ((diJoyState.rgbButtons[3] & 0x80) >> 4);
                else
                   {
                    if ((diHoldJoy.rgbButtons[3] & 0x80) != (diJoyState.rgbButtons[3] & 0x80))
                       {
                        if ((diJoyState.rgbButtons[2] & 0x80) == 0x80)
                           {
                            jbevent.type = ev_keydown;
                           }
                        else
                           {
                            jbevent.type = ev_keyup;
                           }
                        jbevent.data1 = joyb4;
                        D_PostEvent(&jbevent);
                       }
                   }
                event.data2 = diJoyState.lX;
                event.data3 = diJoyState.lY;
                D_PostEvent(&event);
                diHoldJoy.lX = diJoyState.lX;
                diHoldJoy.lY = diJoyState.lY;
                diHoldJoy.rgbButtons[0] = diJoyState.rgbButtons[0];
                diHoldJoy.rgbButtons[1] = diJoyState.rgbButtons[1];
                diHoldJoy.rgbButtons[2] = diJoyState.rgbButtons[2];
                diHoldJoy.rgbButtons[3] = diJoyState.rgbButtons[3];
               }
           }
       }
       }

    HandleKeyboard();

    MY_DoomLoop();
   }

void HandleKeyboard()
   {
    HRESULT          hresult;

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
   }

void HandleKeyStrokes()
   {
    static  event_t  event;
    int     i;

    if ((keylink == TRUE) && (diKeyState[DIK_LMENU] == KS_KEYDOWN))
        diKeyState[DIK_RMENU] = KS_KEYDOWN;
    if ((keylink == TRUE) && (diKeyState[DIK_RMENU] == KS_KEYDOWN) && (diKeyState[DIK_LMENU] == KS_KEYUP))
        diKeyState[DIK_LMENU] = KS_KEYDOWN;

    if ((keylink == TRUE) && (diKeyState[DIK_LSHIFT] == KS_KEYDOWN))
        diKeyState[DIK_RSHIFT] = KS_KEYDOWN;
    if ((keylink == TRUE) && (diKeyState[DIK_RSHIFT] == KS_KEYDOWN) && (diKeyState[DIK_LSHIFT] == KS_KEYUP))
        diKeyState[DIK_LSHIFT] = KS_KEYDOWN;

    if ((keylink == TRUE) && (diKeyState[DIK_LCONTROL] == KS_KEYDOWN))
        diKeyState[DIK_RCONTROL] = KS_KEYDOWN;
    if ((keylink == TRUE) && (diKeyState[DIK_RCONTROL] == KS_KEYDOWN) && (diKeyState[DIK_LCONTROL] == KS_KEYUP))
        diKeyState[DIK_LCONTROL] = KS_KEYDOWN;

    // Can this be limited to a smaller range? What about international keyboards?
    for (i = 1; i < 256; i++)
       {
        if (((diKeyState[i] & 0x80) == 0) && (si_Kbd[i] == KS_KEYDOWN))
           {
            event.type = ev_keyup;
            event.data1 = i;
            D_PostEvent(&event);
            si_Kbd[i] = KS_KEYUP;
           }

        if ((diKeyState[i] & 0x80) && (si_Kbd[i] == KS_KEYUP))
           {
            if ((i != DIK_TAB) || ((diKeyState[DIK_LMENU] == KS_KEYUP) && (diKeyState[DIK_RMENU] == KS_KEYUP)))
               {
                event.type = ev_keydown;
                event.data1 = i;
                D_PostEvent(&event);
                si_Kbd[i] = KS_KEYDOWN;
              }
           }
       }
   }

void WinDoomExit()
   {
    if (demorecording)
       {
        if (demotype == DEMO_I)
           G_EndDemo();
        else
           G_EndDemo_II();
       }
    GameMode = GAME_LIMBO;
    StopMusic();
    ShutdownDirectInput();
    ShutdownDirectSound();
    ShutdownDirectDraw();
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
                case VK_SHIFT:
                     if (keylink == TRUE)
                        {
                         event.type = ev_keydown;
                         event.data1 = DIK_RSHIFT;
                         D_PostEvent(&event);
                        }
                     break;
                case VK_CONTROL:
                     if (keylink == TRUE)
                        {
                         event.type = ev_keydown;
                         event.data1 = DIK_RCONTROL;
                         D_PostEvent(&event);
                        }
                     break;
               }
            break;

       case WM_ACTIVATE:
            if (LOWORD(wParam) != WA_INACTIVE)
               {
                GameMode = GAME_PLAY;
               }
            else
               {
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
                case VK_SHIFT:
                     if (keylink == TRUE)
                        {
                         event.type = ev_keyup;
                         event.data1 = DIK_RSHIFT;
                         D_PostEvent(&event);
                        }
                     break;
                case VK_CONTROL:
                     if (keylink == TRUE)
                        {
                         event.type = ev_keyup;
                         event.data1 = DIK_RCONTROL;
                         D_PostEvent(&event);
                        }
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
        for (buff = 0; buff < NUM_DSBUFFERS; buff++)
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

    if (Channel > NUM_DSBUFFERS)
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

    for (buff = 0; buff < NUM_DSBUFFERS; buff++)
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
// DirectDraw
/////////////////////////////////////////////////////////////////////////////////////

DWORD FrameCount, FrameCount0, FrameTime, FrameTime0, FrameRate;

void InitGlobals(void)
   {
    lpDD = NULL;
    lpDD2 = NULL;
    lpDDSPrimary = NULL;
    lpDDSBack = NULL;
    lpDDClipper = NULL;
   }

BOOL SetupDirectDraw( HWND hwnd )
   {
    static RECT    rect;
    static HDC     hdc;
    static HRESULT hresult;
    static int     width, height;
    static DDSURFACEDESC ddsd;
    static DDSCAPS ddscaps;
    static DDCAPS  DriverCaps, HELCaps;
    static char    msg[512];
    int            i, x;
    FILE *fn;

    // Record the current display BPP
    hdc = GetDC(hwnd);
    BPP = GetDeviceCaps(hdc, BITSPIXEL);
    ReleaseDC(hwnd, hdc);

    // create the main DirectDraw object
    hresult = DirectDrawEnumerate(DDEnumCallback, &lpDD);
    if (hresult != DD_OK)
        DD_Error(hresult, "DirectDrawEnumerate");
    if (lpDD == 0)
       {
        hresult = DirectDrawCreate( NULL, &lpDD, NULL );
        if (hresult != DD_OK)
            DD_Error(hresult, "DirectDrawCreate");
       }

    // Get the DirectDraw capabilities
    memset(&DriverCaps, 0, sizeof(DDCAPS));
    DriverCaps.dwSize = sizeof(DDCAPS);
        
    memset(&HELCaps, 0, sizeof(DDCAPS));
    HELCaps.dwSize = sizeof(DDCAPS);
        
    hresult = lpDD->lpVtbl->GetCaps(lpDD, &DriverCaps, &HELCaps);
    if (hresult != DD_OK)
       DD_Error(hresult, "GetCaps");

    // Does this driver have 3D hardware capabilites?
    if (DriverCaps.dwCaps & DDCAPS_3D)
        DDHard3D = 1;

    hresult = lpDD->lpVtbl->SetCooperativeLevel(lpDD, hwnd, DDSCL_NORMAL);
    if (hresult != DD_OK)
        DD_Error(hresult, "SetCooperativeLevel");
    hresult = lpDD->lpVtbl->QueryInterface(lpDD, &IID_IDirectDraw2, (LPVOID *)&lpDD2);
    if (hresult != DD_OK)
        DD_Error(hresult, "QueryInterface");

    // Get exclusive mode
    hresult = lpDD2->lpVtbl->SetCooperativeLevel(lpDD2, hwnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWMODEX);
    if (hresult != DD_OK)
        DD_Error(hresult, "SetCooperativeLevel");

    // See what display modes are available...
    lpDD2->lpVtbl->EnumDisplayModes(lpDD2, DDEDM_REFRESHRATES | DDEDM_STANDARDVGAMODES, NULL, 0, EnumDisplayModesCallback );

    CurrMode = -1;  // should be -1 for proper checking...
    fn = fopen(szDbgName, "a+");
    fprintf(fn, "Mode count : %d\n", NumModes);
    fprintf(fn, "DirectDraw Video Mode List\n");
    fprintf(fn, "Mode Width x Height x Color Bits ModeX\n");
    for (x = 0; x < NumModes; x++)
       {
        if (Mode[x].bpp == 8)
           fprintf(fn, "%2d    %4d    %4d        %2d      %s\n", x,Mode[x].w,Mode[x].h,Mode[x].bpp,YesNo[Mode[x].ModeX]);
        if ((Mode[x].w == SCREENWIDTH) && (Mode[x].h == SCREENHEIGHT) && (Mode[x].bpp == 8))
           CurrMode = x;
       }
    fclose(fn);

    // should check video mode here and abort if not available...
    if (CurrMode == -1)
       {
        hresult = lpDD->lpVtbl->SetCooperativeLevel(lpDD, hwnd, DDSCL_NORMAL);
        RELEASE(lpDD);
        return(FALSE);
       }
    // Set the video mode
    if (Mode[CurrMode].ModeX == FALSE)
        hresult = lpDD2->lpVtbl->SetDisplayMode(lpDD2, Mode[CurrMode].w, Mode[CurrMode].h, Mode[CurrMode].bpp, 0, DDSDM_STANDARDVGAMODE );
    else
        hresult = lpDD2->lpVtbl->SetDisplayMode(lpDD2, Mode[CurrMode].w, Mode[CurrMode].h, Mode[CurrMode].bpp, 0, 0 );
    if (hresult != DD_OK)
        DD_Error(hresult, "SetDisplayMode");

    memset(&ddsd, sizeof(DDSURFACEDESC), 0);
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
    //ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX | DDSCAPS_VIDEOMEMORY;
    ddsd.dwBackBufferCount = 1;
    hresult = lpDD2->lpVtbl->CreateSurface(lpDD2, &ddsd, &lpDDSPrimary, NULL);
    if (hresult != DD_OK)
        DD_Error(hresult, "CreateSurface");
    ddscaps.dwCaps = DDSCAPS_BACKBUFFER;
    hresult = lpDDSPrimary->lpVtbl->GetAttachedSurface(lpDDSPrimary, &ddscaps, &lpDDSBack);
    if (hresult != DD_OK)
        DD_Error(hresult, "GetAttachedSurface");

    // Retrieve the caps of the primary surface
    lpDDSPrimary->lpVtbl->GetCaps(lpDDSPrimary, &ddscaps);
    WriteDebug("DirectDraw Back Buffer Surface Description:\n");
    ddsd.dwSize = sizeof(ddsd);
    lpDDSBack->lpVtbl->GetSurfaceDesc(lpDDSBack, &ddsd);
    sprintf(szMsgText, "Screen Width %d, Height %d, Surface Pitch %d\n",ddsd.dwWidth, ddsd.dwHeight, ddsd.lPitch );
    WriteDebug(szMsgText);

    // Create and attach palette (only if we in 8-bit palettized color modes)
    if ((Mode[CurrMode].bpp == 8) && (ddscaps.dwCaps & DDCAPS_PALETTE))
       {
        // Set up palette 
        // These 3 are read only, I can't change them.
        //rPal[0].peFlags   = D3DPAL_READONLY;
        //rPal[255].peFlags = D3DPAL_READONLY;
 
        // The rest are free for my use, it can set then to whatever I want.
        for (i = 0; i < 256; i++)
           {
            rPal[i].peRed   = xpal.colors[i].red*4;
            rPal[i].peBlue  = xpal.colors[i].blue*4;
            rPal[i].peGreen = xpal.colors[i].green*4;
            rPal[i].peFlags = D3DPAL_FREE;
           }
   
        // Create the palette
        hresult = lpDD2->lpVtbl->CreatePalette(lpDD2, DDPCAPS_8BIT | DDPCAPS_ALLOW256 | DDPCAPS_INITIALIZE, rPal, &lpPalette, NULL);
        if (hresult != DD_OK)
            DD_Error(hresult, "CreatePalette");
        // Set the back buffer's palette
        hresult = lpDDSBack->lpVtbl->SetPalette(lpDDSBack, lpPalette);
        if (hresult != DD_OK)
            DD_Error(hresult, "SetPalette - Back Buffer");
        // Set the primary surface's palette
        hresult = lpDDSPrimary->lpVtbl->SetPalette(lpDDSPrimary, lpPalette);
        if (hresult != DD_OK)
            DD_Error(hresult, "SetPalette - Primary Surface");
       }
    return TRUE;
   }


// Checks all the surfaces created, if they are lost it restores them.  Returns FALSE
// if a surface was lost, so the viewport can be recreated if necessary
BOOL CheckSurfaces()
   {
	// Check the primary surface
    if (lpDDSPrimary)
       {
        if (lpDDSPrimary->lpVtbl->IsLost(lpDDSPrimary))
           {
            lpDDSPrimary->lpVtbl->Restore(lpDDSPrimary);
            return FALSE;
           }
       }

    return TRUE;
   }

void CreateDDBitmap(HBITMAP hBitMap, BITMAP *BitMap, LPDIRECTDRAWSURFACE *lpDDSurf)
   {
    HDC                 hdcImage, hdc;
    DDSURFACEDESC       ddsd;
    HRESULT             hresult;
    LPDIRECTDRAWSURFACE lpDDTemp;

    ZeroMemory(&ddsd, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
    ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
    ddsd.dwWidth = BitMap->bmWidth;
    ddsd.dwHeight = BitMap->bmHeight;

    hresult = lpDD2->lpVtbl->CreateSurface(lpDD2, &ddsd, &lpDDTemp, NULL);
    if (hresult != DD_OK)
       {
        DD_Error( hresult, "CreateDDBitmap CreateSurface");
        return;
       }

    hdcImage = CreateCompatibleDC(NULL);
    if (!hdcImage)
	   WriteDebug("createcompatible dc failed\n");

    SelectObject(hdcImage, hBitMap);

    //
    // get size of surface.
    //
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_HEIGHT | DDSD_WIDTH;
    lpDDTemp->lpVtbl->GetSurfaceDesc(lpDDTemp, &ddsd);

    hresult = lpDDTemp->lpVtbl->GetDC(lpDDTemp, &hdc);
    if (hresult == DD_OK)
       {
        StretchBlt(hdc, 0, 0, ddsd.dwWidth, ddsd.dwHeight, hdcImage, 0, 0, BitMap->bmWidth, BitMap->bmHeight, SRCCOPY);
        lpDDTemp->lpVtbl->ReleaseDC(lpDDTemp, hdc);
       }
    else
       {
        DD_Error(hresult, "CreateDDBitmap GetDC");
       }
    *lpDDSurf = lpDDTemp;
    DeleteDC(hdcImage);
   }

#define PU_CACHE 101

void RenderScene()
   {
    DDSURFACEDESC ddsd;
    static int    i, j, x, y, dwsw, pad;
    HRESULT       hresult;
    DWORD        *Buff, *Scrn;


    if (GameMode != GAME_PLAY)
       return;

    if (CheckSurfaces() == FALSE)
       return;

    Scrn = (DWORD *)screens[0];
    ddsd.dwSize = sizeof(ddsd);
    lpDDSBack->lpVtbl->GetSurfaceDesc(lpDDSBack, &ddsd);
    hresult = lpDDSBack->lpVtbl->Lock(lpDDSBack, NULL, &ddsd, DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT, NULL);
    if (hresult == DD_OK)
       {
        Buff = (DWORD *)ddsd.lpSurface;
        if (ddsd.lPitch == SCREENWIDTH)
            memcpy(Buff, Scrn, SCREENWIDTH*SCREENHEIGHT);
        else
           {
            dwsw = SCREENWIDTH / 4;
            pad = ddsd.lPitch / 4;
            for (j = 0; j < SCREENHEIGHT; j++)
               {
                x = j * dwsw; // tried adding -- it's slower...don't know why ?!?
                y = j * pad;
                memcpy(&Buff[y], &Scrn[x], SCREENWIDTH);
               }
           }
        lpDDSBack->lpVtbl->Unlock(lpDDSBack, NULL);
       }
    else
       DD_Error(hresult, "Lock DD Surface");
    // we have rendered the backbuffer, call flip so we can see it
    while (lpDDSPrimary->lpVtbl->Flip(lpDDSPrimary, NULL, DDFLIP_WAIT) == DDERR_WASSTILLDRAWING);
   }


void ShutdownDirectDraw()
   {
    WriteDebug("Running ShutdownDirectDraw...\n");
    if(lpDD2 != NULL)
       lpDD2->lpVtbl->RestoreDisplayMode(lpDD2);
    MoveWindow(hMainWnd, 0,0, dwCurrWidth, dwCurrHeight, TRUE);
    WriteDebug("Restored display mode...\n");
    if(lpDD2 != NULL)
        lpDD2->lpVtbl->SetCooperativeLevel(lpDD2, hMainWnd, DDSCL_NORMAL);
    WriteDebug("Reset cooperative level...\n");
    RELEASE(lpDDSBack);
    lpDDSBack = 0;
    RELEASE(lpDDSPrimary);
    lpDDSPrimary = 0;
    RELEASE(lpPalette);
    lpPalette = 0;
    RELEASE(lpDD2);
    lpDD2 = 0;
    RELEASE(lpDD);
    lpDD = 0;
    WriteDebug("Shutdown DirectDraw Complete...\n");
   }

// Finds a DirectDraw driver that doesn't have 3D(if any)
BOOL FAR PASCAL DDEnumCallback(GUID FAR* lpGUID, LPSTR lpDriverDesc, LPSTR lpDriverName, LPVOID lpContext)
   {
    LPDIRECTDRAW  l_lpDD;
    DDCAPS DriverCaps, HELCaps;
    HRESULT hresult;

    // Make sure the guid is valid
    if (lpGUID)
       {
        // Try to create a DirectDraw object
        hresult = DirectDrawCreate(lpGUID, &l_lpDD, NULL);
        if (hresult != DD_OK)
           DD_Error(hresult, "DirectDrawCreate");
        
        // Get the DirectDraw capabilities
        memset(&DriverCaps, 0, sizeof(DDCAPS));
        DriverCaps.dwSize = sizeof(DDCAPS);
        
        memset(&HELCaps, 0, sizeof(DDCAPS));
        HELCaps.dwSize = sizeof(DDCAPS);
        
        hresult = l_lpDD->lpVtbl->GetCaps(l_lpDD, &DriverCaps, &HELCaps);
        if (hresult != DD_OK)
           DD_Error(hresult, "GetCaps");

        // Does this driver have 3D hardware capabilites?
        if (!(DriverCaps.dwCaps & DDCAPS_NOHARDWARE) && (DriverCaps.dwCaps & DDCAPS_GDI))
           {
            *(LPDIRECTDRAW*)lpContext = l_lpDD;
            return DDENUMRET_CANCEL;
           }
        else
           {
            *(LPDIRECTDRAW*)lpContext = NULL;
            l_lpDD->lpVtbl->Release(l_lpDD);
            DDCount++;
           }
       }
    return DDENUMRET_OK;
   }


static HRESULT CALLBACK EnumDisplayModesCallback(LPDDSURFACEDESC pddsd, LPVOID lpContext)
   {
    /*
     * Very large resolutions cause problems on some hardware.  They are also
     * not very useful for real-time rendering.  We have chosen to disable
     * them by not reporting them as available.
     */
    if (pddsd->dwWidth > 1024 || pddsd->dwHeight > 768)
        return DDENUMRET_OK;
    /*
     * Save this mode at the end of the mode array and increment mode count
     */
    Mode[NumModes].w = pddsd->dwWidth;
    Mode[NumModes].h = pddsd->dwHeight;
    Mode[NumModes].bpp = pddsd->ddpfPixelFormat.dwRGBBitCount;
    Mode[NumModes].bThisDriverCanDo = FALSE;
    if ((pddsd->ddsCaps.dwCaps & DDSCAPS_MODEX) == DDSCAPS_MODEX)
       Mode[NumModes].ModeX = TRUE;
    else
       Mode[NumModes].ModeX = FALSE;
    if ((pddsd->dwFlags & DDSD_REFRESHRATE) == DDSD_REFRESHRATE)
       Mode[NumModes].refresh = pddsd->dwRefreshRate;
    else
       Mode[NumModes].refresh = 0;
    NumModes++;
    if (NumModes == 50)
        return DDENUMRET_CANCEL;
    else
        return DDENUMRET_OK;
   }


/////////////////////////////////////////////////////////////////////////////////////
// DirectInput
/////////////////////////////////////////////////////////////////////////////////////

BOOL InitDirectInput()
   {
    HRESULT hresult;
    LPDIRECTINPUTDEVICE lpJoy;
    DIPROPRANGE         diprg; 
    DIPROPDWORD         dipdw;

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

    hresult = lpDirectInput->lpVtbl->CreateDevice(lpDirectInput, &GUID_Joystick, &lpJoy, NULL );
    if (hresult == DI_OK)
       {
        hresult = lpJoy->lpVtbl->QueryInterface(lpJoy,&IID_IDirectInputDevice2,(LPVOID *)&lpJoystick); 
        if (hresult != DI_OK)
           {
            DI_Error( hresult, "CreateDevice (real joystick)");
           }
        RELEASE(lpJoy);
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
        else
           {
            diprg.diph.dwSize       = sizeof(diprg); 
            diprg.diph.dwHeaderSize = sizeof(diprg.diph); 
            diprg.diph.dwObj        = DIJOFS_X; 
            diprg.diph.dwHow        = DIPH_BYOFFSET; 
            diprg.lMin              = -1000; 
            diprg.lMax              = +1000; 
            lpJoystick->lpVtbl->SetProperty(lpJoystick, DIPROP_RANGE, &diprg.diph);
            diprg.diph.dwObj        = DIJOFS_Y; 
            lpJoystick->lpVtbl->SetProperty(lpJoystick, DIPROP_RANGE, &diprg.diph);

            dipdw.diph.dwSize       = sizeof(DIPROPDWORD);
            dipdw.diph.dwHeaderSize = sizeof(dipdw.diph);
            dipdw.diph.dwHow        = DIPH_BYOFFSET;
            dipdw.dwData            = 1500;
            dipdw.diph.dwObj        = DIJOFS_X;
            lpJoystick->lpVtbl->SetProperty(lpJoystick, DIPROP_DEADZONE, &dipdw.diph);
            dipdw.diph.dwObj         = DIJOFS_Y;
            lpJoystick->lpVtbl->SetProperty(lpJoystick, DIPROP_DEADZONE, &dipdw.diph);
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

