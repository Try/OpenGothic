//
// Copyright (c) 1996-2001 Microsoft Corporation
// CSynth.cpp
//
// READ THIS!!!!!!!!!!!!!!!!!!!!!!!!!!!
//
// 4530: C++ exception handler used, but unwind semantics are not enabled. Specify -GX
//
// We disable this because we use exceptions and do *not* specify -GX (USE_NATIVE_EH in
// sources).
//
// The one place we use exceptions is around construction of objects that call
// InitializeCriticalSection. We guarantee that it is safe to use in this case with
// the restriction given by not using -GX (automatic objects in the call chain between
// throw and handler are not destructed). Turning on -GX buys us nothing but +10% to code
// size because of the unwind code.
//
// Any other use of exceptions must follow these restrictions or -GX must be turned on.
//
// READ THIS!!!!!!!!!!!!!!!!!!!!!!!!!!!
//
#pragma warning(disable:4530)

#ifdef DMSYNTH_MINIPORT
#include "common.h"
#else

#include "simple.h"
#include <mmsystem.h>
#include "dsoundp.h"
#include <dmusicc.h>
#include <dmusics.h>
#include "synth.h"
#include "CSynth.h"
#ifdef REVERB_ENABLED
#include "sverb.h"
#endif
#include "debug.h"
#endif

#include "dsoundp.h"    // For IDirectSoundSynthSink

#ifdef _X86_
#define MMX_ENABLED 1
#endif

#ifdef MMX_ENABLED
BOOL MultiMediaInstructionsSupported();
#endif


CSynth::CSynth()

{
    DWORD nIndex;
    CVoice *pVoice;

    m_fCSInitialized = FALSE;
    ::InitializeCriticalSection(&m_CriticalSection);
    // Note: on pre-Blackcomb OS's, this call can raise an exception; if it
    // ever pops in stress, we can add an exception handler and retry loop.
    m_fCSInitialized = TRUE;

    for (nIndex = 0;nIndex < MAX_NUM_VOICES;nIndex++)
    {
        pVoice = new CVoice;
        if (pVoice != NULL)
        {
            m_VoicesFree.AddHead(pVoice);
        }
    }
    for (nIndex = 0;nIndex < NUM_EXTRA_VOICES;nIndex++)
    {
        pVoice = new CVoice;
        if (pVoice != NULL)
        {
            m_VoicesExtra.AddHead(pVoice);
        }
    }
    m_fReverbActive = FALSE;
    m_pCoefs = NULL;
    m_pStates = NULL;
    m_ReverbParams.fInGain = 0.0;
    m_ReverbParams.fReverbMix = -10.0;
    m_ReverbParams.fReverbTime = 1000.0;
    m_ReverbParams.fHighFreqRTRatio = (float) 0.001;
    m_ppControl = NULL;
    m_dwControlCount = 0;
    m_nMaxVoices = MAX_NUM_VOICES;
    m_nExtraVoices = NUM_EXTRA_VOICES;
    m_stLastStats = 0;
    m_fAllowPanWhilePlayingNote = TRUE;
    m_fAllowVolumeChangeWhilePlayingNote = TRUE;
    ResetPerformanceStats();
    m_stLastTime = 0;
    m_dwSampleRate = SAMPLE_RATE_22;
    SetSampleRate(SAMPLE_RATE_22);
    SetStereoMode(BUFFERFLAG_INTERLEAVED);
    SetGainAdjust(600);

    m_sfMMXEnabled = FALSE;
#ifdef MMX_ENABLED
    m_sfMMXEnabled = MultiMediaInstructionsSupported();
#endif // MMX_ENABLED
}

CSynth::~CSynth()

{
    CVoice *pVoice;

    if (m_fCSInitialized)
    {
        // If CS never initialized, nothing else will have been set up
        //
        Close();
        while (pVoice = m_VoicesInUse.RemoveHead())
        {
            delete pVoice;
        }
        while (pVoice = m_VoicesFree.RemoveHead())
        {
            delete pVoice;
        }
        while (pVoice = m_VoicesExtra.RemoveHead())
        {
            delete pVoice;
        }

        DeleteCriticalSection(&m_CriticalSection);
    }
}

short CSynth::ChangeVoiceCount(CVoiceList *pList,short nOld,short nCount)

{
    if (nCount > nOld)
    {
        short nNew = nCount - nOld;
        for (;nNew != 0; nNew--)
        {
            CVoice *pVoice = new CVoice;
            if (pVoice != NULL)
            {
                pList->AddHead(pVoice);
            }
        }
    }
    else
    {
        short nNew = nOld - nCount;
        for (;nNew > 0; nNew--)
        {
            CVoice *pVoice = pList->RemoveHead();
            if (pVoice != NULL)
            {
                delete pVoice;
            }
            else
            {
                nCount += nNew;
                break;
            }
        }
    }
    return nCount;
}

HRESULT CSynth::SetMaxVoices(short nVoices,short nTempVoices)

{
    if (nVoices < 1)
    {
        nVoices = 1;
    }
    if (nTempVoices < 1)
    {
        nTempVoices = 1;
    }
    ::EnterCriticalSection(&m_CriticalSection);

    m_nMaxVoices = ChangeVoiceCount(&m_VoicesFree,m_nMaxVoices,nVoices);
    m_nExtraVoices = ChangeVoiceCount(&m_VoicesExtra,m_nExtraVoices,nTempVoices);

    ::LeaveCriticalSection(&m_CriticalSection);
    return S_OK;
}

HRESULT CSynth::SetNumChannelGroups(DWORD dwCableCount)

{
    HRESULT hr = S_OK;
    CControlLogic **ppControl;
    if ((dwCableCount < 1) || (dwCableCount > MAX_CHANNEL_GROUPS))
    {
        Trace(1,"Error: Request to set synth to %ld channel groups is invalid.\n",dwCableCount);
        return E_INVALIDARG;
    }
    ::EnterCriticalSection(&m_CriticalSection);
    if (m_dwControlCount != dwCableCount)
    {
        try
        {
            ppControl = new CControlLogic *[dwCableCount];
        }
        catch( ... )
        {
            ppControl = NULL;
        }

        if (ppControl)
        {
            DWORD dwX;
            for (dwX = 0; dwX < dwCableCount; dwX++)
            {
                ppControl[dwX] = NULL;
            }
            if (m_dwControlCount < dwCableCount)
            {
                for (dwX = 0; dwX < m_dwControlCount; dwX++)
                {
                    ppControl[dwX] = m_ppControl[dwX];
                }
                for (;dwX < dwCableCount; dwX++)
                {
                    try
                    {
                        ppControl[dwX] = new CControlLogic;
                    }
                    catch( ... )
                    {
                        ppControl[dwX] = NULL;
                    }

                    if (ppControl[dwX])
                    {
                        hr = ppControl[dwX]->Init(&m_Instruments, this);
                        if (FAILED(hr))
                        {
                            delete ppControl[dwX];
                            ppControl[dwX] = NULL;
                            dwCableCount = dwX;
                            break;
                        }

                        ppControl[dwX]->SetGainAdjust(m_vrGainAdjust);
                    }
                    else
                    {
                        dwCableCount = dwX;
                        break;
                    }
                }
            }
            else
            {
                AllNotesOff();
                for (dwX = 0; dwX < dwCableCount; dwX++)
                {
                    ppControl[dwX] = m_ppControl[dwX];
                }
                for (; dwX < m_dwControlCount; dwX++)
                {
                    if (m_ppControl[dwX])
                    {
                        delete m_ppControl[dwX];
                    }
                }
            }
            if (m_ppControl)
            {
                delete[] m_ppControl;
            }
            m_ppControl = ppControl;
            m_dwControlCount = dwCableCount;
        }
        else
        {
            hr = E_OUTOFMEMORY;
        }
    }
    ::LeaveCriticalSection(&m_CriticalSection);
    return hr;
}

void CSynth::SetGainAdjust(VREL vrGainAdjust)
{
    DWORD idx;

    m_vrGainAdjust = vrGainAdjust;
    ::EnterCriticalSection(&m_CriticalSection);

    for (idx = 0; idx < m_dwControlCount; idx++)
    {
        m_ppControl[idx]->SetGainAdjust(m_vrGainAdjust);
    }

    ::LeaveCriticalSection(&m_CriticalSection);
}

HRESULT CSynth::SetReverb(DMUS_WAVES_REVERB_PARAMS *pParams)

{
    m_ReverbParams = *pParams;
    if (m_pCoefs)
    {
#ifdef REVERB_ENABLED
        ::SetSVerb(m_ReverbParams.fInGain,m_ReverbParams.fReverbMix,
            m_ReverbParams.fReverbTime,m_ReverbParams.fHighFreqRTRatio,m_pCoefs );
#endif
    }
    return S_OK;;
}

void CSynth::SetReverbActive(BOOL fReverb)

{
    ::EnterCriticalSection(&m_CriticalSection);
#ifdef REVERB_ENABLED
    if (m_fReverbActive != fReverb)
    {
        if (m_fReverbActive = fReverb)
        {
            if (!m_pCoefs)
            {
                long lSize = GetCoefsSize();
                m_pCoefs = (void *) malloc(lSize);
                lSize = GetStatesSize();
                m_pStates = (long *) malloc(lSize);
                if (m_pCoefs && m_pStates)
                {
                    memset((void *) m_pStates,0,lSize);
                    InitSVerb( (float) m_dwSampleRate, m_pCoefs);
                    InitSVerbStates( m_pStates );
                    SetReverb(&m_ReverbParams);
                }
            }
            else if (m_pStates)
            {
                InitSVerbStates( m_pStates );
            }
        }
    }
#else
        m_fReverbActive = FALSE;
#endif
    ::LeaveCriticalSection(&m_CriticalSection);
}

void CSynth::GetReverb(DMUS_WAVES_REVERB_PARAMS *pParams)
{
    *pParams = m_ReverbParams;
}

BOOL CSynth::IsReverbActive()

{
    return m_fReverbActive;
}


HRESULT CSynth::Open(DWORD dwCableCount, DWORD dwVoices, BOOL fReverb)

{
    HRESULT hr = S_OK;
    if ((dwCableCount < 1) || (dwCableCount > MAX_CHANNEL_GROUPS))
    {
        Trace(1,"Error: Request to open synth with %ld channel groups is invalid.\n",dwCableCount);
        return E_INVALIDARG;
    }
    if (m_ppControl)
    {
        Trace(1,"Error: Request to open synth failed because synth was already opened.\n");
        return E_FAIL;  // Already opened.
    }
    ::EnterCriticalSection(&m_CriticalSection);
    hr = SetNumChannelGroups(dwCableCount);
    if (SUCCEEDED(hr))
    {
        short nTemp = (short) dwVoices / 4;
        if (nTemp < 4) nTemp = 4;
        SetMaxVoices((short) dwVoices, nTemp);
    }
    SetReverbActive(fReverb);
    m_vrGainAdjust = 0;
    ::LeaveCriticalSection(&m_CriticalSection);
    return hr;
}

HRESULT CSynth::Close()

{
    ::EnterCriticalSection(&m_CriticalSection);
    AllNotesOff();
    DWORD dwX;
    for (dwX = 0; dwX < m_dwControlCount; dwX++)
    {
        if (m_ppControl[dwX])
        {
            delete m_ppControl[dwX];
        }
    }
    m_dwControlCount = 0;
    if (m_ppControl)
    {
        delete[] m_ppControl;
        m_ppControl = NULL;
    }
    m_stLastStats = 0;
    m_stLastTime = 0;
    m_fReverbActive = FALSE;
#ifdef REVERB_ENABLED
    if (m_pCoefs)
    {
        free(m_pCoefs);
        m_pCoefs = NULL;
    }
    if (m_pStates)
    {
        free(m_pStates);
        m_pStates = NULL;
    }
#endif
    ::LeaveCriticalSection(&m_CriticalSection);
    return S_OK;
}

HRESULT CSynth::GetMaxVoices(
    short * pnMaxVoices,    // Returns maximum number of allowed voices for continuous play.
    short * pnTempVoices )  // Returns number of extra voices for voice overflow.
{
    if (pnMaxVoices != NULL)
    {
        *pnMaxVoices = m_nMaxVoices;
    }
    if (pnTempVoices != NULL)
    {
        *pnTempVoices = m_nExtraVoices;
    }
    return S_OK;
}

HRESULT CSynth::SetSampleRate(
    DWORD dwSampleRate)
{
    HRESULT hr = S_OK;

    // Can't set the sample rate to 0
    if (dwSampleRate == 0)
    {
        Trace(1,"Error: Request to set sample rate to 0 failed.\n");
        return E_INVALIDARG;
    }

    ::EnterCriticalSection(&m_CriticalSection);
    AllNotesOff();
    if ( m_dwSampleRate || dwSampleRate )
    {
        m_stLastTime *= dwSampleRate;
        m_stLastTime /= m_dwSampleRate;
    }

//>>>>>>>>>>> why is this commented out????
    // m_stLastTime = MulDiv(m_stLastTime,dwSampleRate,m_dwSampleRate);
    m_stLastStats = 0;
    m_dwSampleRate = dwSampleRate;
    m_stMinSpan = dwSampleRate / 100;   // 10 ms.
    m_stMaxSpan = (dwSampleRate + 19) / 20;    // 50 ms.
    ::LeaveCriticalSection(&m_CriticalSection);
    m_Instruments.SetSampleRate(dwSampleRate);
    return hr;
}

HRESULT CSynth::Activate(DWORD dwSampleRate, DWORD dwBufferFlags)

{
#ifdef REVERB_ENABLED
    if (m_fReverbActive && m_pStates && m_pCoefs)
    {
        InitSVerb( (float) m_dwSampleRate, m_pCoefs);
        InitSVerbStates( m_pStates );
        SetReverb(&m_ReverbParams);
    }
#endif
    m_stLastTime = 0;
    SetSampleRate(dwSampleRate);
    SetStereoMode(dwBufferFlags);
    ResetPerformanceStats();
    return S_OK;
}

HRESULT CSynth::Deactivate()

{
    AllNotesOff();
    return S_OK;
}

HRESULT CSynth::GetPerformanceStats(PerfStats *pStats)

{
    if (pStats == NULL)
    {
        Trace(1,"Error: Null pointer passed for performance stats.\n");
        return E_POINTER;
    }
    *pStats = m_CopyStats;
    return (S_OK);
}

bool CSynth::BusIDToFunctionID(DWORD dwBusID, DWORD *pdwFunctionID, long *plPitchBends, DWORD *pdwIndex)

{
    // This should only be called if the internal bus pointers exist and there is at least one buffer.
    assert(m_pdwBusIDs && m_pdwFuncIDs && m_plPitchBends && m_dwBufferCount);
    // Scan through the list of bus ids, looking for the match for dwBusID.
    for ( DWORD nIndexSinkIds = 0; nIndexSinkIds < m_dwBufferCount; nIndexSinkIds++ )
    {
        // Is this one it?
        if (m_pdwBusIDs[nIndexSinkIds] == dwBusID)
        {
            *pdwFunctionID = m_pdwFuncIDs[nIndexSinkIds];
            if (plPitchBends)
                *plPitchBends = m_plPitchBends[nIndexSinkIds];
            if (pdwIndex)
                *pdwIndex = nIndexSinkIds;
            return true;
        }
    }
    return false;
}

void CSynth::Mix(short **ppvBuffer, DWORD *pdwIDs, DWORD *pdwFuncIDs, long *plPitchBends, DWORD dwBufferCount, DWORD dwBufferFlags,  DWORD dwLength, LONGLONG llPosition)
{
    static BOOL fDidLast = FALSE;

    STIME stEndTime;
    CVoice *pVoice;
    CVoice *pNextVoice;
    long lNumVoices = 0;
    DWORD i;

    ::EnterCriticalSection(&m_CriticalSection);

    // Store pointers to the id arrays so we can access them from BusIDToFunctionID.
    m_pdwBusIDs = pdwIDs;
    m_pdwFuncIDs = pdwFuncIDs;
    m_plPitchBends = plPitchBends;
    m_dwBufferCount = dwBufferCount;

/*  // Useful for debugging the incoming buses...
    static DWORD sdwCountDown = 0;
    if (!sdwCountDown)
    {
        for (DWORD dwIX = 0; dwIX < dwBufferCount; dwIX++)
        {
            Trace(0,"%ld:%ld->%ld\t",dwIX,pdwIDs[dwIX],pdwFuncIDs[dwIX]);
        }
        Trace(0,"\n");
        sdwCountDown = 100;
    }
    sdwCountDown--;*/

    LONG    lTime = - (LONG)::GetTheCurrentTime();

    stEndTime = llPosition + dwLength;
    StealNotes(stEndTime);
    DWORD dwX;

    for ( i = 0; i < dwBufferCount; i++ )
    {
        // For interleaved buffers only the first buss is valid
        if ( dwBufferFlags & BUFFERFLAG_INTERLEAVED && i > 0 )
        {
            break;
        }
        StartMix(ppvBuffer[i],dwLength,(dwBufferFlags&BUFFERFLAG_INTERLEAVED));
    }

    for (dwX = 0; dwX < m_dwControlCount; dwX++)
    {
        m_ppControl[dwX]->QueueNotes(llPosition, stEndTime);
        m_ppControl[dwX]->QueueWaves(stEndTime);
    }
    pVoice = m_VoicesInUse.GetHead();

    for (;pVoice != NULL;pVoice = pNextVoice)
    {
        if (pVoice->m_stWaveStopTime && (pVoice->m_stWaveStopTime < stEndTime))
        {
            pVoice->StopVoice(pVoice->m_stWaveStopTime);
        }
        pNextVoice = pVoice->GetNext();
        pVoice->Mix(ppvBuffer, dwBufferFlags, dwLength, llPosition, stEndTime);

        lNumVoices++;

        if (pVoice->m_fInUse == FALSE)
        {
            m_VoicesInUse.Remove(pVoice);
            m_VoicesFree.AddHead(pVoice);

            if (pVoice->m_stStartTime < m_stLastStats)
            {
                m_BuildStats.dwTotalSamples += (long) (pVoice->m_stStopTime - m_stLastStats);
            }
            else
            {
                m_BuildStats.dwTotalSamples += (long) (pVoice->m_stStopTime - pVoice->m_stStartTime);
            }
        }
    }

    for (dwX = 0; dwX < m_dwControlCount; dwX++)
    {
        m_ppControl[dwX]->ClearMIDI(stEndTime);
    }

#ifdef REVERB_ENABLED
    if (m_fReverbActive && m_pCoefs && m_pStates && !(dwBufferFlags & BUFFERFLAG_MULTIBUFFER) )
    {
        if (dwBufferFlags & BUFFERFLAG_INTERLEAVED)
        {
            SVerbStereoToStereoShort(dwLength,ppvBuffer[0],ppvBuffer[0],m_pCoefs,m_pStates);
        }
        else
        {
            SVerbMonoToMonoShort(dwLength,ppvBuffer[0],ppvBuffer[0],m_pCoefs,m_pStates);
        }
    }
#endif

    for ( i = 0; i < dwBufferCount; i++ )
    {
        // For interleaved buffers only the first buss is valid
        if ( dwBufferFlags & BUFFERFLAG_INTERLEAVED && i > 0 )
        {
            break;
        }
        FinishMix(ppvBuffer[i],dwLength,(dwBufferFlags&BUFFERFLAG_INTERLEAVED));
    }

    if (stEndTime > m_stLastTime)
    {
        m_stLastTime = stEndTime;
    }

    lTime += ::GetTheCurrentTime();

    m_BuildStats.dwTotalTime += lTime;

    if ((m_stLastStats + m_dwSampleRate) <= m_stLastTime)
    {
        DWORD dwElapsed = (DWORD) (m_stLastTime - m_stLastStats);
        pVoice = m_VoicesInUse.GetHead();

        for (;pVoice != NULL;pVoice = pVoice->GetNext())
        {
            if (pVoice->m_stStartTime < m_stLastStats)
            {
                m_BuildStats.dwTotalSamples += dwElapsed;
            }
            else
            {
                m_BuildStats.dwTotalSamples += (long) (m_stLastTime - pVoice->m_stStartTime);
            }
        }

        if (dwElapsed == 0)
            dwElapsed = 1;

        if (m_BuildStats.dwTotalSamples == 0)
            m_BuildStats.dwTotalSamples = 1;

        m_BuildStats.dwVoices =
            (m_BuildStats.dwTotalSamples + (dwElapsed >> 1)) / dwElapsed;
        {
            m_BuildStats.dwCPU = MulDiv(m_BuildStats.dwTotalTime,
                m_dwSampleRate, dwElapsed);
        }

        m_CopyStats = m_BuildStats;
        memset(&m_BuildStats, 0, sizeof(m_BuildStats));
        m_stLastStats = m_stLastTime;
    }

    m_pdwBusIDs = NULL;
    m_pdwFuncIDs = NULL;
    m_plPitchBends = NULL;
    m_dwBufferCount = 0;

    ::LeaveCriticalSection(&m_CriticalSection);
}

CVoice *CSynth::OldestVoice()

{
    CVoice *pVoice;
    CVoice *pBest = NULL;
    pVoice = m_VoicesInUse.GetHead();
    pBest = pVoice;
    if (pBest)
    {
        pVoice = pVoice->GetNext();
        for (;pVoice;pVoice = pVoice->GetNext())
        {
            if (!pVoice->m_fTag)
            {
                if (pBest->m_fTag)
                {
                    pBest = pVoice;
                }
                else
                {
                    if (pVoice->m_dwPriority <= pBest->m_dwPriority)
                    {
                        if (pVoice->m_fNoteOn)
                        {
                            if (pBest->m_fNoteOn)
                            {
                                if (pBest->m_stStartTime > pVoice->m_stStartTime)
                                {
                                    pBest = pVoice;
                                }
                            }
                        }
                        else
                        {
                            if (pBest->m_fNoteOn ||
                                (pBest->m_vrVolume > pVoice->m_vrVolume))
                            {
                                pBest = pVoice;
                            }
                        }
                    }
                }
            }
        }
        if (pBest->m_fTag)
        {
            pBest = NULL;
        }
    }
    return pBest;
}

CVoice *CSynth::StealVoice(DWORD dwPriority)

{
    CVoice *pVoice;
    CVoice *pBest = NULL;
    pVoice = m_VoicesInUse.GetHead();
    for (;pVoice != NULL;pVoice = pVoice->GetNext())
    {
        if (pVoice->m_dwPriority <= dwPriority)
        {
            if (!pBest)
            {
                pBest = pVoice;
            }
            else
            {
                if (pVoice->m_fNoteOn == FALSE)
                {
                    if ((pBest->m_fNoteOn == TRUE) ||
                        (pBest->m_vrVolume > pVoice->m_vrVolume))
                    {
                        pBest = pVoice;
                    }
                }
                else
                {
                    if (pBest->m_stStartTime > pVoice->m_stStartTime)
                    {
                        pBest = pVoice;
                    }
                }
            }
        }
    }
    if (pBest != NULL)
    {
        pBest->ClearVoice();
        pBest->m_fInUse = FALSE;
        m_VoicesInUse.Remove(pBest);

        pBest->SetNext(NULL);
    }
    return pBest;
}

void CSynth::QueueVoice(CVoice *pVoice)

/*  This function queues a voice in the list of currently
    synthesizing voices. It places them in the queue so that
    the higher priority voices are later in the queue. This
    allows the note stealing algorithm to take off the top of
    the queue.
    And, we want older playing notes to be later in the queue
    so the note ons and offs overlap properly. So, the queue is
    sorted in priority order with older notes later within one
    priority level.
*/

{
    CVoice *pScan = m_VoicesInUse.GetHead();
    CVoice *pNext = NULL;
    if (!pScan) // Empty list?
    {
        m_VoicesInUse.AddHead(pVoice);
        return;
    }
    if (pScan->m_dwPriority > pVoice->m_dwPriority)
    {   // Are we lower priority than the head of the list?
        m_VoicesInUse.AddHead(pVoice);
        return;
    }

    pNext = pScan->GetNext();
    for (;pNext;)
    {
        if (pNext->m_dwPriority > pVoice->m_dwPriority)
        {
            // Lower priority than next in the list.
            pScan->SetNext(pVoice);
            pVoice->SetNext(pNext);
            return;
        }
        pScan = pNext;
        pNext = pNext->GetNext();
    }
    // Reached the end of the list.
    pScan->SetNext(pVoice);
    pVoice->SetNext(NULL);
}

void CSynth::StealNotes(STIME stTime)

{
    CVoice *pVoice;
    long lToMove = m_nExtraVoices - m_VoicesExtra.GetCount();
    if (lToMove > 0)
    {
        for (;lToMove > 0;)
        {
            pVoice = m_VoicesFree.RemoveHead();
            if (pVoice != NULL)
            {
                m_VoicesExtra.AddHead(pVoice);
                lToMove--;
            }
            else break;
        }
        if (lToMove > 0)
        {
            pVoice = m_VoicesInUse.GetHead();
            for (;pVoice;pVoice = pVoice->GetNext())
            {
                if (pVoice->m_fTag) // Voice is already slated to be returned.
                {
                    lToMove--;
                }
            }
            for (;lToMove > 0;lToMove--)
            {
                pVoice = OldestVoice();
                if (pVoice != NULL)
                {
                    pVoice->QuickStopVoice(stTime);
                    m_BuildStats.dwNotesLost++;
                }
                else break;
            }
        }
    }
}

#ifndef i386
void CSynth::StartMix(short *pBuffer,DWORD dwLength, BOOL bInterleaved)
{
    DWORD dwIndex = 0;
    DWORD dwLen = dwLength << bInterleaved;

    for (; dwIndex < dwLen; dwIndex++)
    {
        pBuffer[dwIndex] <<= 1;
    }
}
#else
void CSynth::StartMix(short *pBuffer,DWORD dwLength, BOOL bInterleaved)
{
    DWORD dwIndex;
    DWORD dwLen = dwLength << bInterleaved;

    dwIndex = 0;

    if (m_sfMMXEnabled && dwLen >= 16)
    {
        dwIndex = (dwLen & ~0x0000000F);

        _asm {
            mov         eax, dwIndex
            mov         ebx, pBuffer
            lea         ebx, [ebx+eax*2]        // move to the end to start...
            neg         eax

TopOfLoop:
            movq        mm0, [ebx+eax*2]
            movq        mm1, [ebx+eax*2+8]

            psraw       mm0, 1

            movq        mm2, [ebx+eax*2+16]

            psraw       mm1, 1

            movq        mm3, [ebx+eax*2+24]

            psraw       mm2, 1

            movq        [ebx+eax*2], mm0

            psraw       mm3, 1

            movq        [ebx+eax*2+8], mm1
            movq        [ebx+eax*2+16], mm2
            movq        [ebx+eax*2+24], mm3

            add         eax, 16
            jl          TopOfLoop

            emms
        }
    }

    for (; dwIndex < dwLen; dwIndex++)
    {
        pBuffer[dwIndex] <<= 1;
    }
}
#endif

//////////////////////////////////////////////////////////////////
// FinishMix - C Base for Optimized code
#ifndef i386
void CSynth::FinishMix(short *pBuffer,DWORD dwLength, BOOL bInterleaved)

{
    DWORD dwIndex = 0;
    long lMax = (long) m_BuildStats.dwMaxAmplitude;
    long lTemp;
    DWORD dwLen = dwLength << bInterleaved;

    for (; dwIndex < dwLen; dwIndex++)
    {
        lTemp = pBuffer[dwIndex];
        lTemp <<= 1;

        if (lTemp < -32767) lTemp = -32767;
        if (lTemp > 32767) lTemp = 32767;

        pBuffer[dwIndex] = (short) lTemp;
        if (lTemp > lMax)
        {
            lMax = lTemp;
        }
        else if (lTemp < 0 && -lTemp > lMax)
        {
            lMax = -lTemp;
        }
    }
    m_BuildStats.dwMaxAmplitude = lMax;
}
#else
void CSynth::FinishMix(short *pBuffer,DWORD dwLength, BOOL bInterleaved)
{
    DWORD dwIndex;
    long lMax = (long) m_BuildStats.dwMaxAmplitude;
    long lTemp;
    DWORD dwLen = dwLength << bInterleaved;

    short PosMax, NegMax;

    PosMax =   (short) lMax;
    NegMax = - (short) lMax;

    dwIndex = 0;

    if (m_sfMMXEnabled && dwLen >= 4)
    {
        dwIndex = (dwLen & ~0x3);

        _asm {
            mov         eax, dwIndex
            mov         ebx, pBuffer
            lea         ebx, [ebx+eax*2]        // move to the end to start...
            neg         eax
            pxor        mm0, mm0
            movsx       ecx,  WORD PTR PosMax
            movsx       edx,  WORD PTR NegMax
Start:
            movq        mm1, QWORD PTR [ebx+eax*2]
            movq        mm2, mm1
            punpcklwd   mm1, mm0
            punpckhwd   mm2, mm0
            pslld       mm1, 16
            pslld       mm2, 16
            psrad       mm1, 15
            psrad       mm2, 15
            packssdw    mm1, mm2
            movq        QWORD PTR [ebx+eax*2], mm1
            movsx       esi, WORD PTR [ebx+eax*2]
            movsx       edi, WORD PTR [ebx+eax*2+2]

            cmp         esi, ecx
            jg          Max1
Max10:      cmp         edi, edx
            jl          Min1

Min10:      movsx       esi, WORD PTR [ebx+eax*2+4]
            cmp         edi, ecx
            jg          Max2
Max20:      cmp         edi, edx
            jl          Min2

Min20:      movsx       edi, WORD PTR [ebx+eax*2+6]
            cmp         esi, ecx
            jg          Max3
Max30:      cmp         esi, edx
            jl          Min3

Min30:      cmp         edi, ecx
            jg          Max4
Max40:      cmp         edi, edx
            jl          Min4

Min40:      add         eax, 4
            jl          Start
            jmp         Finished
Max1:
            mov         ecx, esi
            jmp         Max10
Max2:
            mov         ecx, edi
            jmp         Max20
Max3:
            mov         ecx, esi
            jmp         Max30
Max4:
            mov         ecx, edi
            jmp         Max40
Min1:
            mov         edx, esi
            jmp         Min10
Min2:
            mov         edx, edi
            jmp         Min20
Min3:
            mov         edx, esi
            jmp         Min30
Min4:
            mov         edx, edi
            jmp         Min40
Finished:
            emms
            mov         WORD PTR PosMax, cx
            mov         WORD PTR NegMax, dx
        }
        if (lMax < PosMax)
            lMax = PosMax;

        if (lMax < -NegMax)
            lMax = -NegMax;
    }

    for (; dwIndex < dwLen; dwIndex++)
    {
        lTemp = pBuffer[dwIndex];
        lTemp <<= 1;

        if (lTemp < -32767) lTemp = -32767;
        if (lTemp > 32767) lTemp = 32767;

        pBuffer[dwIndex] = (short) lTemp;
        if (lTemp > lMax)
        {
            lMax = lTemp;
        }
        else if (lTemp < 0 && -lTemp > lMax)
        {
            lMax = -lTemp;
        }
    }

    m_BuildStats.dwMaxAmplitude = lMax;
}
#endif

HRESULT CSynth::Unload(HANDLE hDownload,
                       HRESULT ( CALLBACK *lpFreeMemory)(HANDLE,HANDLE),
                       HANDLE hUserData)
{
    return m_Instruments.Unload( hDownload, lpFreeMemory, hUserData);
}

HRESULT CSynth::Download(LPHANDLE phDownload, void * pdwData, LPBOOL bpFree)

{
    return m_Instruments.Download( phDownload, (DWORD *) pdwData,  bpFree);
}

HRESULT CSynth::PlayBuffer(IDirectMusicSynthSink *pSynthSink, REFERENCE_TIME rt, LPBYTE lpBuffer, DWORD cbBuffer, ULONG ulCable)

{
    STIME stTime;

    ::EnterCriticalSection(&m_CriticalSection);

    if ( rt == 0 ) // Special case of time == 0.
    {
        stTime = m_stLastTime;
    }
    else
    {
        pSynthSink->RefTimeToSample(rt, &stTime);
    }

    PlayBuffer(stTime, rt, lpBuffer, cbBuffer, ulCable);

    ::LeaveCriticalSection(&m_CriticalSection);
    return S_OK;
}

HRESULT CSynth::PlayBuffer(IDirectSoundSynthSink *pSynthSink, REFERENCE_TIME rt, LPBYTE lpBuffer, DWORD cbBuffer, ULONG ulCable)

{
    STIME stTime;

    ::EnterCriticalSection(&m_CriticalSection);

    if ( rt == 0 ) // Special case of time == 0.
    {
        stTime = m_stLastTime;
    }
    else
    {
        pSynthSink->RefToSampleTime(rt, &stTime);
    }

    //Trace(0,"Reftime[%lx%08lx] LastTime[%lx%08lx] SampleTime[%lx%08lx]\n\r",(DWORD)(rt>>32),(DWORD)(rt&0x00000000ffffffff),(DWORD)(m_stLastTime>>32),(DWORD)(m_stLastTime&0x00000000ffffffff),(DWORD)(stTime>>32),(DWORD)(stTime&0x00000000ffffffff));

    PlayBuffer(stTime, rt, lpBuffer, cbBuffer, ulCable);

    ::LeaveCriticalSection(&m_CriticalSection);
    return S_OK;
}

HRESULT CSynth::PlayBuffer(STIME stTime, REFERENCE_TIME rt, LPBYTE lpBuffer, DWORD cbBuffer, ULONG ulCable)

{
    ::EnterCriticalSection(&m_CriticalSection);

    if (cbBuffer <= sizeof(DWORD))
    {
#ifdef DBG
        if (stTime < m_stLastTime)
        {
            static DWORD dwFailed = 0;
            if ((lpBuffer[0] & 0xF0) == MIDI_NOTEON)
            {
                if (!dwFailed)
                {
                    Trace(2,"Warning: Note On arrived too late to the synth, synth has mixed ahead by %ld samples. This could be caused by latency calculated too low.\n",
                         (long) (m_stLastTime - stTime));
                        dwFailed = 100;
                }
                dwFailed--;
            }
        }
        else if ((stTime - (10000 * 1000)) > m_stLastTime)
        {
            static DWORD dwFailed = 0;
            if ((lpBuffer[0] & 0xF0) == MIDI_NOTEON)
            {
                if (!dwFailed)
                {
                    Trace(2,"Warning: Note On at sample %ld, was stamped too late for synth, which is at mix time %ld, error is %ld samples\n",
                         (long) stTime, (long) m_stLastTime, (long) (m_stLastTime - stTime));
                        dwFailed = 100;
                }
                dwFailed--;
            }
        }
#endif
        if (ulCable <= m_dwControlCount)
        {
            if (ulCable == 0) // Play all groups if 0.
            {
                for (; ulCable < m_dwControlCount; ulCable++)
                {
                    m_ppControl[ulCable]->RecordMIDI(stTime,lpBuffer[0],
                        lpBuffer[1], lpBuffer[2]);
                }
            }
            else
            {
                m_ppControl[ulCable - 1]->RecordMIDI(stTime,lpBuffer[0],
                lpBuffer[1], lpBuffer[2]);

            }
        }
        else
        {
            Trace(1,"Error: MIDI event on channel group %ld is beyond range of %ld opened channel groups\n",
                ulCable, m_dwControlCount);
        }
    }
    else
    {
        if (ulCable <= m_dwControlCount)
        {
            if (ulCable == 0)
            {
                for (; ulCable < m_dwControlCount; ulCable++)
                {
                    m_ppControl[ulCable]->RecordSysEx(cbBuffer,
                        &lpBuffer[0], stTime);
                }
            }
            else
            {
                m_ppControl[ulCable-1]->RecordSysEx(cbBuffer,
                    &lpBuffer[0], stTime);
            }
        }
    }

    ::LeaveCriticalSection(&m_CriticalSection);
    return S_OK;
}

// Sets the stereo mode for the synth
// this value is currently only being used
// to deterim whether one can pan or not.
HRESULT CSynth::SetStereoMode(
    DWORD dwBufferFlags )
{
    HRESULT hr = S_OK;

    if (dwBufferFlags & BUFFERFLAG_INTERLEAVED )
    {
        m_dwStereo = TRUE;
    }
    else if (dwBufferFlags & BUFFERFLAG_MULTIBUFFER )
    {
        m_dwStereo = TRUE;
    }
    else
        m_dwStereo = FALSE;

    return hr;
}

void CSynth::ResetPerformanceStats()

{
    m_BuildStats.dwNotesLost = 0;
    m_BuildStats.dwTotalTime = 0;
    m_BuildStats.dwVoices = 0;
    m_BuildStats.dwTotalSamples = 0;
    m_BuildStats.dwCPU = 0;
    m_BuildStats.dwMaxAmplitude = 0;
    m_CopyStats = m_BuildStats;
}

HRESULT CSynth::AllNotesOff()

{
    CVoice *pVoice;
    ::EnterCriticalSection(&m_CriticalSection);
    while (pVoice = m_VoicesInUse.RemoveHead())
    {
        pVoice->ClearVoice();
        pVoice->m_fInUse = FALSE;
        m_VoicesFree.AddHead(pVoice);

        if (pVoice->m_stStartTime < m_stLastStats)
        {
            m_BuildStats.dwTotalSamples += (long) (pVoice->m_stStopTime - m_stLastStats);
        }
        else
        {
            m_BuildStats.dwTotalSamples += (long) (pVoice->m_stStopTime - pVoice->m_stStartTime);
        }
    }
    ::LeaveCriticalSection(&m_CriticalSection);
    return (S_OK);
}

HRESULT CSynth::SetChannelPriority(
    DWORD dwChannelGroup,
    DWORD dwChannel,
    DWORD dwPriority)
{
    HRESULT hr = S_OK;

    ::EnterCriticalSection(&m_CriticalSection);

    dwChannelGroup--;
    if ((dwChannelGroup >= m_dwControlCount) || (dwChannel > 15))
    {
        Trace(1,"Error: Request to set channel priority on channel group %ld, channel %ld, is out of range.\n",
            dwChannelGroup,dwChannel);
        hr = E_INVALIDARG;
    }
    else
    {
        if (m_ppControl)
        {
            hr = m_ppControl[dwChannelGroup]->SetChannelPriority(dwChannel,dwPriority);
        }
    }
    ::LeaveCriticalSection(&m_CriticalSection);

    return hr;
}

HRESULT CSynth::GetChannelPriority(
    DWORD dwChannelGroup,
    DWORD dwChannel,
    LPDWORD pdwPriority)
{
    HRESULT hr = S_OK;

    ::EnterCriticalSection(&m_CriticalSection);

    dwChannelGroup--;
    if ((dwChannelGroup >= m_dwControlCount) || (dwChannel > 15))
    {
        Trace(1,"Error: Request to get channel priority on channel group %ld, channel %ld, is out of range.\n",
            dwChannelGroup,dwChannel);
        hr = E_INVALIDARG;
    }
    else
    {
        if (m_ppControl)
        {
            hr = m_ppControl[dwChannelGroup]->GetChannelPriority(dwChannel,pdwPriority);
        }
    }
    ::LeaveCriticalSection(&m_CriticalSection);

    return hr;
}


//////////////////////////////////////////////////////////
// Directx8 Methods

HRESULT CSynth::PlayVoice(
    IDirectSoundSynthSink *pSynthSink,
    REFERENCE_TIME rt,
    DWORD dwVoiceId,
    DWORD dwChannelGroup,
    DWORD dwChannel,
    DWORD dwDLId,
    VREL vrVolume,
    PREL prPitch,
    SAMPLE_TIME stVoiceStart,
    SAMPLE_TIME stLoopStart,
    SAMPLE_TIME stLoopEnd
    )
{
    HRESULT hr = S_OK;
    STIME stTime;

    ::EnterCriticalSection(&m_CriticalSection);

    dwChannelGroup--;
    if ((dwChannelGroup >= m_dwControlCount) || (dwChannel > 15))
    {
        Trace(1,"Error: Request to set play voice on channel group %ld, channel %ld, is out of range.\n",
            dwChannelGroup,dwChannel);
        hr = E_INVALIDARG;
    }

    if ( rt == 0 ) // Special case of time == 0.
    {
        stTime = m_stLastTime;
    }
    else
    {
        pSynthSink->RefToSampleTime(rt, &stTime);
    }

    CWaveArt *pWaveArt = m_Instruments.GetWaveArt(dwDLId);
    if ( pWaveArt )
        m_ppControl[dwChannelGroup]->RecordWaveEvent(
            stTime,
            (BYTE)(dwChannel & 0xF),
            dwVoiceId,
            vrVolume,
            prPitch,
            stVoiceStart,
            stLoopStart,
            stLoopEnd,
            pWaveArt);

    ::LeaveCriticalSection(&m_CriticalSection);

    return hr;
}

HRESULT CSynth::StopVoice(
    IDirectSoundSynthSink *pSynthSink,
    REFERENCE_TIME rt,
    DWORD dwVoiceId )
{
    HRESULT hr = S_OK;
    STIME stTime;

    ::EnterCriticalSection(&m_CriticalSection);

    if ( rt == 0 )  // Special case of time == 0.
    {
        stTime = m_stLastTime;
    }
    else
    {
        pSynthSink->RefToSampleTime(rt, &stTime);
    }

    CVoice * pVoice = m_VoicesInUse.GetHead();
    bool fFoundVoice = false;
    for (;pVoice != NULL;pVoice = pVoice->GetNext())
    {
        if ( pVoice->m_dwVoiceId == dwVoiceId )
        {
            pVoice->m_stWaveStopTime = stTime;
            fFoundVoice = true;
        }
    }
    if (!fFoundVoice)
    {
        TraceI(2, "Warning: Synth couldn't find voice %d to stop.\n", dwVoiceId);
        for (DWORD dwX = 0; dwX < m_dwControlCount; dwX++)
        {
            m_ppControl[dwX]->FlushWaveByStopTime(dwVoiceId, stTime);
        }
    }

    ::LeaveCriticalSection(&m_CriticalSection);

    return hr;
}

HRESULT CSynth::GetVoiceState(
    DWORD dwVoice[],
    DWORD cbVoice,
    DMUS_VOICE_STATE VoiceState[] )
{
    HRESULT hr = E_FAIL;

    ::EnterCriticalSection(&m_CriticalSection);

    for ( DWORD i = 0; i < cbVoice; i++ )
    {
        VoiceState[i].bExists = FALSE;
        VoiceState[i].spPosition = 0;

        CVoice * pVoice = m_VoicesInUse.GetHead();
        for (;pVoice != NULL;pVoice = pVoice->GetNext())
        {
            if ( pVoice->m_dwVoiceId == dwVoice[i] )
            {
                VoiceState[i].bExists = TRUE;
                VoiceState[i].spPosition = pVoice->GetCurrentPos();
                break;
            }
        }
    }

    ::LeaveCriticalSection(&m_CriticalSection);

    return S_OK;
}

HRESULT CSynth::Refresh(
    DWORD dwDownloadID,
    DWORD dwFlags)
{
    HRESULT hr = S_OK;

    ::EnterCriticalSection(&m_CriticalSection);

    CWave *pWave = m_Instruments.GetWave(dwDownloadID);
    if ( pWave )
    {
        if ( pWave->m_bValid )
        {
            Trace(1,"Error: Attempting to validate already validated streaming buffer\n\r");
            hr = E_FAIL;
        }

        if ( pWave->m_bActive )
        {
            Trace(1,"Error: Attempting to validate active playing streaming buffer\n\r");
            hr = E_FAIL;
        }

        if (SUCCEEDED(hr))
        {
            pWave->m_bValid = TRUE;

            if (pWave->m_bSampleType == SFORMAT_8)
            {
                DWORD dwX;
                char *pData = (char *) pWave->m_pnWave;
                for (dwX = 0; dwX < pWave->m_dwSampleLength; dwX++)
                {
                    pData[dwX] -= (char) 128;
                }
            }

            // Indicate that we did find a vaild download id
            hr = S_OK;
        }
    }

    ::LeaveCriticalSection(&m_CriticalSection);

    return hr;
}

HRESULT CSynth::AssignChannelToBuses(
    DWORD dwChannelGroup,
    DWORD dwChannel,
    LPDWORD pdwBuses,
    DWORD cBuses)
{
    HRESULT hr = S_OK;

    ::EnterCriticalSection(&m_CriticalSection);

//>>>>>>> Probably need a better check here since panning is only valid for left and right?

    dwChannelGroup--;
    if ((dwChannelGroup >= m_dwControlCount) || (dwChannel > 15))
    {
        hr = E_INVALIDARG;
    }

    if (SUCCEEDED(hr))
    {
        hr = m_ppControl[dwChannelGroup]->AssignChannelToBuses(dwChannel, pdwBuses, cBuses);
    }

    ::LeaveCriticalSection(&m_CriticalSection);

    return hr;
}
