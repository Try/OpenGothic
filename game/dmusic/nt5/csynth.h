//      Copyright (c) 1996-1999 Microsoft Corporation
//
//      CSynth.h
//

#ifndef __CSYNTH_H__
#define __CSYNTH_H__

#include "synth.h"

#define MAX_CHANNEL_GROUPS	1000
#define MAX_VOICES			1000

// Forward declarations
struct IDirectSoundSynthSink;

class CSynth : public CListItem
{
friend class CControlLogic;
public:	
					CSynth();
					~CSynth();
    CSynth *		GetNext() {return(CSynth *)CListItem::GetNext();};
    HRESULT			SetStereoMode(DWORD dwBufferFlags);
    HRESULT			SetSampleRate(DWORD dwSampleRate);
	HRESULT			Activate(DWORD dwSampleRate, DWORD dwBufferFlags);
	HRESULT			Deactivate();
	HRESULT			Download(LPHANDLE phDownload, void *pdwData, LPBOOL bpFree);
	HRESULT			Unload(HANDLE hDownload,HRESULT ( CALLBACK *lpFreeMemory)(HANDLE,HANDLE),HANDLE hUserData);
	HRESULT			PlayBuffer(IDirectMusicSynthSink *pSynthSink, REFERENCE_TIME rt, LPBYTE lpBuffer, DWORD cbBuffer, ULONG ulCable);
	HRESULT			SetNumChannelGroups(DWORD dwCableCount);
    void            SetGainAdjust(VREL vrGainAdjust);
    HRESULT			Open(DWORD dwCableCount, DWORD dwVoices, BOOL fReverb);
	HRESULT			Close();
    void			ResetPerformanceStats();
    HRESULT			AllNotesOff();
    HRESULT			SetMaxVoices(short nMaxVoices,short nTempVoices);
    HRESULT			GetMaxVoices(short *pnMaxVoices,short *pnTempVoices);
    HRESULT			GetPerformanceStats(PerfStats *pStats);
	void			Mix(short **ppvBuffer, DWORD *pdwIDs, DWORD *pdwFuncIDs, long *plPitchBends, DWORD dwBufferCount, DWORD dwBufferFlags, DWORD dwLength, LONGLONG llPosition);
    HRESULT         SetChannelPriority(DWORD dwChannelGroup,DWORD dwChannel,DWORD dwPriority);
    HRESULT         GetChannelPriority(DWORD dwChannelGroup,DWORD dwChannel,LPDWORD pdwPriority);
	HRESULT			SetReverb(DMUS_WAVES_REVERB_PARAMS *pParams);
    void            GetReverb(DMUS_WAVES_REVERB_PARAMS *pParams);
    void            SetReverbActive(BOOL fReverb);
    BOOL            IsReverbActive();

	/* DirectX8 methods */
	HRESULT			PlayBuffer(STIME stTime, REFERENCE_TIME rt, LPBYTE lpBuffer, DWORD cbBuffer, ULONG ulCable);
	HRESULT			PlayBuffer(IDirectSoundSynthSink *pSynthSink, REFERENCE_TIME rt, LPBYTE lpBuffer, DWORD cbBuffer, ULONG ulCable);
	HRESULT			PlayVoice(IDirectSoundSynthSink *pSynthSink, REFERENCE_TIME rt, DWORD dwVoiceId, DWORD dwChannelGroup, DWORD dwChannel, DWORD dwDLId, VREL vrVolume, PREL prPitch, SAMPLE_TIME stVoiceStart, SAMPLE_TIME stLoopStart, SAMPLE_TIME stLoopEnd);
    HRESULT			StopVoice(IDirectSoundSynthSink *pSynthSink, REFERENCE_TIME rt, DWORD dwVoiceId);
    HRESULT			GetVoiceState(DWORD dwVoice[], DWORD cbVoice, DMUS_VOICE_STATE VoiceState[]);
    HRESULT			Refresh(DWORD dwDownloadID, DWORD dwFlags);
	HRESULT			AssignChannelToBuses(DWORD dwChannelGroup, DWORD dwChannel, LPDWORD pdwBusses, DWORD cBusses);
public:
    bool            BusIDToFunctionID(DWORD dwBusID, DWORD *pdwFunctionID, long *plPitchBends, DWORD *pdwIndex);   // Converts the passed bus id into the equivalent function id and position in buffer array. 
private:
    void			StealNotes(STIME stTime);
    void			StartMix(short *pBuffer,DWORD dwlength,BOOL bInterleaved);
    void			FinishMix(short *pBuffer,DWORD dwlength,BOOL bInterleaved);
	short			ChangeVoiceCount(CVoiceList *pList,short nOld,short nCount);

private:
    DWORD *         m_pdwBusIDs;        // Temp pointer to array of bus ids. This is valid only during a mix.
    DWORD *         m_pdwFuncIDs;       // Temp pointer to array of corresponding functional ids. This is also only valid during a mix.
    long *          m_plPitchBends;     // Temp pointer to array of corresponding pitch offsets.
    DWORD           m_dwBufferCount;    // Size of two preceding arrays.
    CVoice *        OldestVoice();
    void            QueueVoice(CVoice *pVoice);
    CVoice *        StealVoice(DWORD dwPriority);
    STIME           m_stLastTime;       // Sample time of last mix.
    CVoiceList      m_VoicesFree;       // List of available voices.
    CVoiceList      m_VoicesExtra;      // Extra voices for temporary overload.
    CVoiceList      m_VoicesInUse;      // List of voices currently in use.
    short           m_nMaxVoices;       // Number of allowed voices.
    short           m_nExtraVoices;      // Number of voices over the limit that can be used in a pinch.
    STIME           m_stLastStats;      // Last perfstats refresh.
    PerfStats       m_BuildStats;       // Performance info accumulator.
    PerfStats       m_CopyStats;        // Performance information for display.

    BOOL            m_fReverbActive;    // Whether reverb is currently on or off.
    long *          m_pStates;          // State storage for reverb.
    void *          m_pCoefs;           // Coeeficient storage for reverb.
    DMUS_WAVES_REVERB_PARAMS	m_ReverbParams; // Reverb settings.

public:	
    VREL            m_vrGainAdjust;     // Final output gain adjust
	// DLS-1 compatibility parameters: set these off to emulate hardware
	// which can't vary volume/pan during playing of a note.
    BOOL            m_fAllowPanWhilePlayingNote;
    BOOL            m_fAllowVolumeChangeWhilePlayingNote;

    STIME           m_stMinSpan;        // Minimum time allowed for mix time span.
    STIME           m_stMaxSpan;        // Maximum time allowed for mix time span.
	DWORD           m_dwSampleRate;		// Sample rate 
    DWORD           m_dwStereo;			// Is the output stereo 
    CInstManager    m_Instruments;      // Instrument manager.
	CControlLogic **m_ppControl;		// Array of open ControlLogics.
	DWORD			m_dwControlCount;	// # of open CLs.
    
    CRITICAL_SECTION m_CriticalSection; // Critical section to manage access.
    BOOL             m_fCSInitialized;
	BOOL			m_sfMMXEnabled;		// Is MMX enabled 
};

#endif// __CSYNTH_H__

