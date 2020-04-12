// Copyright © 1998, Bruce A. Lewis
// This code may not be re-used in a commercial product
// of any kind without express written consent from
// the author, Bruce A. Lewis.
//
// Global define needed for calling these functions
#include <windows.h>
#include <mmsystem.h>
#include "i_midi.h"
#include "io.h"

// Local define needed only for error message capture and display
#define     MCIMSGLEN  128
char        MidiMsg[MCIMSGLEN+1];

BOOL MidiPlay(HWND hWnd, MIDI_Data_t *mdd)
   {
    MCI_OPEN_PARMS     open;
    MCI_PLAY_PARMS     play;
    MCIERROR           MCI_Error;
    UINT               uDeviceId;

    if (access(mdd->szMidiFile, 4) != 0)
       {
        mdd->MidiStatus = midi_nofile;
        return(FALSE);
       }

    open.dwCallback       = (DWORD)hWnd;
    open.lpstrDeviceType  = "sequencer";
    open.lpstrElementName = mdd->szMidiFile;

    MCI_Error = mciSendCommand(0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_ELEMENT, (DWORD)&open);

    if (MCI_Error)
       {
        mciGetErrorString(MCI_Error, MidiMsg, MCIMSGLEN);
        MessageBox(hWnd, MidiMsg, NULL, MB_ICONSTOP | MB_OK);
        mdd->MidiError = MCI_Error;
        return(FALSE);
       }

    // play back midi file
    //......................

    uDeviceId = open.wDeviceID;
    play.dwCallback = (DWORD)hWnd;
                           
    MCI_Error = mciSendCommand(uDeviceId, MCI_PLAY, MCI_NOTIFY, (DWORD)&play);
    if (MCI_Error)
       {
        mciGetErrorString(MCI_Error, MidiMsg, MCIMSGLEN);
        MessageBox(hWnd, MidiMsg, NULL, MB_ICONSTOP | MB_OK);
        mciSendCommand(uDeviceId, MCI_CLOSE, 0, (DWORD)NULL);
        mdd->MidiError = MCI_Error;
        return(FALSE);
       }
    mdd->MidiDevice = uDeviceId;
    mdd->MidiStatus = midi_play;
    return(TRUE);
   }

BOOL MidiReplay(HWND hWnd, MIDI_Data_t *mdd)
   {
    MCI_PLAY_PARMS     play;
    MCIERROR           MCI_Error;

    play.dwCallback = (DWORD)hWnd;
    play.dwFrom = 0;
                           
    MCI_Error = mciSendCommand(mdd->MidiDevice, MCI_PLAY, MCI_NOTIFY | MCI_FROM, (DWORD)&play);
    if (MCI_Error)
       {
        mciGetErrorString(MCI_Error, MidiMsg, MCIMSGLEN);
        MessageBox(hWnd, MidiMsg, NULL, MB_ICONSTOP | MB_OK);
        return(FALSE);
       }
    return(TRUE);
   }

BOOL MidiPause(HWND hWnd, MIDI_Data_t *mdd)
   {
    MCI_SET_PARMS      set;
    MCI_STATUS_PARMS   status;
    MCIERROR           MCI_Error;

    mdd->MidiError = 0;
    set.dwTimeFormat   = MCI_FORMAT_MILLISECONDS;
    mciSendCommand(mdd->MidiDevice, MCI_SET, MCI_SET_TIME_FORMAT, (DWORD)(LPVOID)&set);

    status.dwCallback  = (DWORD)hWnd;
    status.dwItem = MCI_STATUS_POSITION;

    MCI_Error = mciSendCommand(mdd->MidiDevice, MCI_STATUS, MCI_STATUS_ITEM, (DWORD)&status);
    if (MCI_Error)
       {
        mciGetErrorString(MCI_Error, MidiMsg, MCIMSGLEN);
        strcat(MidiMsg, "\nMidiPause");
        MessageBox(hWnd, MidiMsg, NULL, MB_ICONSTOP | MB_OK);
        mdd->MidiError = MCI_Error;
        return(FALSE);
       }
    mdd->MidiPosition = status.dwReturn;
    mciSendCommand(mdd->MidiDevice, MCI_STOP, 0, (DWORD)NULL);
    mdd->MidiStatus = midi_pause;
    return(TRUE);
   }

BOOL MidiResume(HWND hWnd, MIDI_Data_t *mdd)
   {
    MCI_SET_PARMS      set;
    MCI_PLAY_PARMS     play;
    MCIERROR           MCI_Error;

    mdd->MidiError = 0;
    set.dwTimeFormat   = MCI_FORMAT_MILLISECONDS;
    mciSendCommand(mdd->MidiDevice, MCI_SET, MCI_SET_TIME_FORMAT, (DWORD)(LPVOID) &set);

    play.dwCallback = (DWORD)hWnd;
    play.dwTo = 0;
    play.dwFrom = mdd->MidiPosition;
    MCI_Error = mciSendCommand(mdd->MidiDevice, MCI_PLAY, MCI_NOTIFY | MCI_FROM, (DWORD)(LPVOID)&play);
    if (MCI_Error)
       {
        mciGetErrorString(MCI_Error, MidiMsg, MCIMSGLEN);
        strcat(MidiMsg, "\nMidiResume");
        MessageBox(hWnd, MidiMsg, NULL, MB_ICONSTOP | MB_OK);
        mdd->MidiError = MCI_Error;
        return(FALSE);
       }
    mdd->MidiStatus = midi_play;
    return(TRUE);
   }

void MidiStop(MIDI_Data_t *mdd)
   {
    mciSendCommand(mdd->MidiDevice, MCI_STOP, 0, (DWORD)NULL);
    mciSendCommand(mdd->MidiDevice, MCI_CLOSE, 0, (DWORD)NULL);
    mdd->MidiStatus = midi_stop;
   }

