//      Copyright (c) 1996-1999 Microsoft Corporation
//      CControlLogic.cpp
//

#ifdef DMSYNTH_MINIPORT
#include "common.h"
#else
#include "simple.h"
#include <mmsystem.h>
#include <dmusicc.h>
#include <dmusics.h>
#include "synth.h"
#include "misc.h"
#include "csynth.h"
#include "debug.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//
// Manage the global critical section. #pragma's disable the warning about
// not compiling with -GX when using exception handling, which we don't
// care about.
//
//
// The critical section must be global because it protects global
// data in the CMIDIRecorder class. These functions are called from 
// DllMain().
//

CRITICAL_SECTION CControlLogic::s_CriticalSection;
BOOL CControlLogic::s_fCSInitialized = FALSE;

#pragma warning(push)
#pragma warning(disable:4530)

/* static */ BOOL CControlLogic::InitCriticalSection()
{
    s_fCSInitialized = FALSE;
    try
    {
        ::InitializeCriticalSection(&s_CriticalSection);
    } catch(...)
    {
        return FALSE;
    }

    s_fCSInitialized = TRUE;
    return TRUE;
}

#pragma warning(pop)

/* static */ void CControlLogic::KillCriticalSection()
{
    if (s_fCSInitialized)
    {
        ::DeleteCriticalSection(&s_CriticalSection);
        s_fCSInitialized = FALSE;
    }
}

CControlLogic::CControlLogic()

{
    m_pSynth = NULL;
    m_pInstruments = NULL;
    DWORD nIndex;
    GMReset();
    m_fGSActive = FALSE;
    m_fXGActive = FALSE;
    for (nIndex = 0;nIndex < 16;nIndex++)
    {
        m_fSustain[nIndex] = FALSE;
        m_dwProgram[nIndex] = 0;
    }
    m_fEmpty = TRUE;
}

CControlLogic::~CControlLogic()

{
}

void CControlLogic::GMReset()

{
    static int nPartToChannel[16] = { 
        9,0,1,2,3,4,5,6,7,8,10,11,12,13,14,15 
    };
    int nX;
    for (nX = 0; nX < 16; nX++)
    {
        int nY;
        m_nData[nX] = 0;
        m_prFineTune[nX] = 0;
        m_bDrums[nX] = 0;
        for (nY = 0; nY < 12; nY++)
        {
            m_prScaleTune[nX][nY] = 0;
        }
        m_nCurrentRPN[nX] = (short) 0x3FFF;
        m_prCoarseTune[nX] = 0; 
        m_bPartToChannel[nX] = (BYTE)nPartToChannel[nX];
        m_fMono[nX] = FALSE;
        m_dwPriority[nX] = DAUD_STANDARD_VOICE_PRIORITY + 
            ((16 - nX) * DAUD_CHAN15_VOICE_PRIORITY_OFFSET);
        m_bBankH[nX] = 0;
        m_bBankL[nX] = 0;
    }
    m_bDrums[0] = 1;
    m_vrMasterVolume = 0;
    m_fGSActive = FALSE;
    m_fXGActive = FALSE;
}

HRESULT CControlLogic::Init(CInstManager *pInstruments, CSynth *pSynth)

{
    m_pSynth = pSynth;
    m_pInstruments = pInstruments;
    m_vrGainAdjust = 0;
    CVoice::Init();
    return S_OK;
}

void CControlLogic::ClearAll()

{
    int nIndex;
    ::EnterCriticalSection(&s_CriticalSection);
    m_Notes.ClearMIDI(0x7FFFFFFF);
    for (nIndex = 0; nIndex < 16; nIndex++) {
        m_ModWheel[nIndex].ClearMIDI(0x7FFFFFFF);
        m_Volume[nIndex].ClearMIDI(0x7FFFFFFF);
        m_Pan[nIndex].ClearMIDI(0x7FFFFFFF);
        m_Expression[nIndex].ClearMIDI(0x7FFFFFFF);
        m_PitchBend[nIndex].ClearMIDI(0x7FFFFFFF);
        m_Pressure[nIndex].ClearMIDI(0x7FFFFFFF);
        m_ReverbSends[nIndex].ClearMIDI(0x7FFFFFFF);
        m_ChorusSends[nIndex].ClearMIDI(0x7FFFFFFF);
        m_CutOffFreqCC[nIndex].ClearMIDI(0x7FFFFFFF);
    }
    for (nIndex = 0; nIndex < 16; nIndex++) {
        m_ModWheel[nIndex].RecordMIDI(0, 0);
        m_Volume[nIndex].RecordMIDI(0, 100);
        m_Pan[nIndex].RecordMIDI(0, 64);
        m_Expression[nIndex].RecordMIDI(0, 127);
        m_PitchBend[nIndex].RecordMIDI(0, 0x2000);
        m_Pressure[nIndex].RecordMIDI(0, 0);
        m_ReverbSends[nIndex].RecordMIDI(0, 40);
        m_ChorusSends[nIndex].RecordMIDI(0, 0);
        m_CutOffFreqCC[nIndex].RecordMIDI(0, 64);
        m_Notes.RecordEvent(0, nIndex, NOTE_SUSTAIN, 0);
    }
    m_fEmpty = TRUE;
    ::LeaveCriticalSection(&s_CriticalSection);
}

void CControlLogic::ClearMIDI(STIME stEndTime)
{
    DWORD dwIndex;
    ::EnterCriticalSection(&s_CriticalSection);
    if (!m_fEmpty)
    {        
        m_fEmpty = TRUE;
        for (dwIndex = 0;dwIndex < 16; dwIndex++)
        {
            m_fEmpty = m_ModWheel[dwIndex].ClearMIDI(stEndTime) && m_fEmpty;
            m_fEmpty = m_PitchBend[dwIndex].ClearMIDI(stEndTime) && m_fEmpty;
            m_fEmpty = m_Volume[dwIndex].ClearMIDI(stEndTime) && m_fEmpty;
            m_fEmpty = m_Expression[dwIndex].ClearMIDI(stEndTime) && m_fEmpty;
            m_fEmpty = m_Pan[dwIndex].ClearMIDI(stEndTime) && m_fEmpty;
            m_fEmpty = m_Pressure[dwIndex].ClearMIDI(stEndTime) && m_fEmpty;
            m_fEmpty = m_ReverbSends[dwIndex].ClearMIDI(stEndTime) && m_fEmpty;
            m_fEmpty = m_ChorusSends[dwIndex].ClearMIDI(stEndTime) && m_fEmpty;
            m_fEmpty = m_CutOffFreqCC[dwIndex].ClearMIDI(stEndTime) && m_fEmpty;
        }
    }
    ::LeaveCriticalSection(&s_CriticalSection);
}

void CControlLogic::SetGainAdjust(VREL vrGainAdjust)
{
    m_vrGainAdjust = vrGainAdjust;
}

void CControlLogic::QueueNotes(STIME stStartTime, STIME stEndTime)
{
    CNote note;

    ::EnterCriticalSection(&s_CriticalSection);
    
    while (m_Notes.GetNote(stEndTime,&note))
    {
        if (note.m_stTime < stStartTime)
        {
            TraceI(2, "Note started %d samples late!\n", (long)(stStartTime - note.m_stTime));

            note.m_stTime = stStartTime;
        }

        if (note.m_bKey > 0x7F) // Special command events.
        {
            long lTemp;
            DWORD dwPart = note.m_bPart;
            DWORD dwCommand = note.m_bKey;
            BYTE bData = note.m_bVelocity;
            switch (dwCommand)
            {
            case NOTE_PROGRAMCHANGE:
                m_dwProgram[dwPart] = bData |
                    (m_bBankH[dwPart] << 16) | 
                    (m_bBankL[dwPart] << 8);
                break;
            case NOTE_CC_BANKSELECTH:
                m_bBankH[dwPart] = bData;
                break;
            case NOTE_CC_BANKSELECTL:
                m_bBankL[dwPart] = bData;
                break;
            case NOTE_CC_POLYMODE:
                m_fMono[dwPart] = FALSE;
                break;
            case NOTE_CC_MONOMODE:
                m_fMono[dwPart] = TRUE;
                break;
            case NOTE_CC_RPN_MSB:
                m_nCurrentRPN[dwPart] = (m_nCurrentRPN[dwPart] & 0x7f) + (bData << 7);
                break;
            case NOTE_CC_RPN_LSB:
                m_nCurrentRPN[dwPart] = (m_nCurrentRPN[dwPart] & 0x3f80) + bData;
                break;
            case NOTE_CC_NRPN:
                m_nCurrentRPN[dwPart] = 0x3FFF;
                break;
            case NOTE_CC_DATAENTRYLSB:
                m_nData[dwPart] &= ~0x7F;
                m_nData[dwPart] |= bData;
                switch (m_nCurrentRPN[dwPart])
                {
                case RPN_PITCHBEND: // Don't do anything, Roland ignores lsb
                    break;
                case RPN_FINETUNE:
                    lTemp = m_nData[dwPart];
                    lTemp -= 8192;
                    lTemp *= 100;
                    lTemp /= 8192;
                    m_prFineTune[dwPart] = lTemp;
                    break;
                case RPN_COARSETUNE: // Ignore lsb
                    break;            
                }
                break;
            case NOTE_CC_DATAENTRYMSB:
                m_nData[dwPart] &= ~(0x7F << 7);
                m_nData[dwPart] |= bData << 7;
                switch (m_nCurrentRPN[dwPart])
                {
                case RPN_PITCHBEND:
                    m_PitchBend[dwPart].m_prRange = bData * 100;
                    break;
                case RPN_FINETUNE:
                    lTemp = m_nData[dwPart];
                    lTemp -= 8192;
                    lTemp *= 100;
                    lTemp /= 8192;
                    m_prFineTune[dwPart] = lTemp;
                    break;
                case RPN_COARSETUNE:
                    m_prCoarseTune[dwPart] = 100 * (bData - 64);
                    break;        
                }
                break;
            case NOTE_SUSTAIN: // special sustain marker
                m_fSustain[dwPart] = (BOOL) bData;
                if (bData == FALSE)
                {
                    CVoice * pVoice = m_pSynth->m_VoicesInUse.GetHead();
                    for (;pVoice != NULL;pVoice = pVoice->GetNext())
                    {
                        if (pVoice->m_fSustainOn &&
                            (pVoice->m_nPart == dwPart) &&
                            (pVoice->m_pControl == this))
                        {
                            pVoice->StopVoice(note.m_stTime);
                        }
                    }
                } 
                break;
            case NOTE_ALLOFF:
                {
                    CVoice *pVoice = m_pSynth->m_VoicesInUse.GetHead();
                    for (;pVoice != NULL; pVoice = pVoice->GetNext())
                    {
                        if (pVoice->m_fNoteOn && !pVoice->m_fSustainOn &&
                            (pVoice->m_nPart == dwPart) &&
                            (pVoice->m_pControl == this))
                        {
                            if (m_fSustain[dwPart])
                            {
                                pVoice->m_fSustainOn = TRUE;
                            }
                            else
                            {
                                pVoice->StopVoice(note.m_stTime);
                            }
                        }
                    }
                }
                break;
            case NOTE_SOUNDSOFF:
                {
                    CVoice *pVoice = m_pSynth->m_VoicesInUse.GetHead();
                    for (;pVoice != NULL; pVoice = pVoice->GetNext())
                    {
                        if ((pVoice->m_nPart == dwPart) &&
                            (pVoice->m_pControl == this))
                        {
                            pVoice->QuickStopVoice(note.m_stTime);
                        }
                    }
                }
                break;
            case NOTE_ASSIGNRECEIVE:
                m_bPartToChannel[dwPart] = (BYTE) bData;
                break;
            case NOTE_MASTERVOLUME:
                m_vrMasterVolume = CMIDIRecorder::VelocityToVolume(bData);
                break;
            }
        }
        else if (note.m_bVelocity == 0)  // Note Off.
        {
            CVoice * pVoice = m_pSynth->m_VoicesInUse.GetHead();
            WORD nPart = note.m_bPart;
            DWORD dwNoteID = 0; // Use to track multiple voices on one note.
            for (;pVoice != NULL;pVoice = pVoice->GetNext())
            {
                if (pVoice->m_fNoteOn && !pVoice->m_fSustainOn &&
                    (pVoice->m_nKey == (WORD) note.m_bKey) &&
                    (pVoice->m_nPart == nPart) &&
                    (pVoice->m_pControl == this)) 
                {
                    if (!dwNoteID || (dwNoteID == pVoice->m_dwNoteID))
                    {
                        dwNoteID = pVoice->m_dwNoteID;
                        if (m_fSustain[nPart])
                        {
                            pVoice->m_fSustainOn = TRUE;
                        }
                        else
                        {
                            pVoice->StopVoice(note.m_stTime);
                        }
                    }
                }
            }
        }
        else   // Note On.
        {
            DWORD dwProgram = m_dwProgram[note.m_bPart];
            if (m_bDrums[note.m_bPart])
            {
                dwProgram |= F_INSTRUMENT_DRUMS;
            }
            if (m_fMono[note.m_bPart])
            {
                CVoice * pVoice = m_pSynth->m_VoicesInUse.GetHead();
                WORD nPart = note.m_bPart;
                for (;pVoice != NULL;pVoice = pVoice->GetNext())
                {
                    if (pVoice->m_fNoteOn && (pVoice->m_nPart == nPart) &&
                        (pVoice->m_pControl == this)) 
                    {
                        pVoice->StopVoice(note.m_stTime);
                    }
                }
            }
            // While we are working with the instrument, including copying
            // the data over from the region, we have to make sure it
            // can not be removed from the instrument list.
            EnterCriticalSection(&m_pInstruments->m_CriticalSection);
            CInstrument * pInstrument = 
                m_pInstruments->GetInstrument(dwProgram,note.m_bKey,note.m_bVelocity);
            if (!pInstrument) 
            {
                if (dwProgram & F_INSTRUMENT_DRUMS)
                {
                    dwProgram = F_INSTRUMENT_DRUMS;
                    pInstrument = 
                        m_pInstruments->GetInstrument(dwProgram,note.m_bKey,note.m_bVelocity);
                }
                else if (m_fXGActive)
                {
                    if ((dwProgram & 0x7F0000) == 0x7F0000) // Drum?
                    {
                        dwProgram &= 0x7F007F;              // Enforce 0 LSB
                        pInstrument = 
                            m_pInstruments->GetInstrument(dwProgram,note.m_bKey,note.m_bVelocity);
                        if (!pInstrument)
                        {
                            dwProgram = 0x7F0000;
                            pInstrument = 
                                m_pInstruments->GetInstrument(dwProgram,note.m_bKey,note.m_bVelocity);
                        }
                    }
                    else
                    {
                        dwProgram &= 0x7F;  // Fall back to GM set.
                        pInstrument = 
                            m_pInstruments->GetInstrument(dwProgram,note.m_bKey,note.m_bVelocity);
                    }
                }
            }
            if (pInstrument != NULL)
            {
                DWORD dwNotesLost = 1;  // Assume note will be lost, will be decremented if played 
                CSourceRegion * pRegion = NULL;
                static DWORD sdwNoteID = 0; // Generate a unique id that will be placed in all voices that play this note.
                sdwNoteID++;                // This will be used to keep the voices associated so we can stop them all at once later.
                while ( pRegion = pInstrument->ScanForRegion(note.m_bKey, note.m_bVelocity, pRegion) ) 
                {
                    WORD nPart = note.m_bPart;
                    CVoice * pVoice = m_pSynth->m_VoicesInUse.GetHead();
                    if (!pRegion->m_bAllowOverlap)
                    {
                        for (;pVoice != NULL; pVoice = pVoice->GetNext())
                        {
                            if ((pVoice->m_nPart == nPart) &&
                                (pVoice->m_nKey == note.m_bKey) &&
                                (pVoice->m_pControl == this) && 
                                (pVoice->m_pRegion  == pRegion))
                            {
                                pVoice->QuickStopVoice(note.m_stTime);
                            }
                        }    
                    }

                    if (pRegion->m_bGroup != 0)
                    {
                        pVoice = m_pSynth->m_VoicesInUse.GetHead();
                        for (;pVoice != NULL;pVoice = pVoice->GetNext())
                        {
                            if ((pVoice->m_dwGroup == pRegion->m_bGroup) &&
                                (pVoice->m_nPart == nPart) &&
                                (pVoice->m_dwProgram == dwProgram) &&
                                (pVoice->m_pControl == this))
                            {
                                pVoice->QuickStopVoice(note.m_stTime);
                            }
                        }
                    }
                
                    pVoice = m_pSynth->m_VoicesFree.RemoveHead();
                    if (pVoice == NULL)
                    {
                        pVoice = m_pSynth->m_VoicesExtra.RemoveHead();
                    }

                    if (pVoice == NULL)
                    {
                        pVoice = m_pSynth->StealVoice(m_dwPriority[nPart]);
                        
                        // The voice IDs are used by the VoiceServiceThread in DMusic
                        // to refill the streaming wave buffers....
                        // Since the voice is stolen this voice could really belong to
                        // a streaming wave in which case preserving the voice ID will 
                        // break the refill code. (NOTE!! This is different from stealing
                        // voices for waves. Waves will ultimately preserve the voice ID as 
                        // they pass it to StartWave where it gets assigned to the voice's 
                        // m_dwVoiceId member).

                        if(pVoice)
                        {
                            // Set the voice ID to something unacceptable
                            pVoice->m_dwVoiceId = 0xffffffff;
                        }
                    }

                    if (pVoice != NULL)
                    {
                        PREL prPitch = m_prFineTune[nPart] + m_prScaleTune[nPart][note.m_bKey % 12];
                        if (!m_bDrums[nPart]) 
                        {
                            if (m_fXGActive)
                            {
                                if ((dwProgram & 0x7F0000) != 0x7F0000)
                                {
                                    prPitch += m_prCoarseTune[nPart];
                                }
                            }
                            else
                            {
                                prPitch += m_prCoarseTune[nPart];
                            }
                        }

                        pVoice->m_nKey = note.m_bKey;
                        pVoice->m_nPart = nPart;
                        pVoice->m_dwProgram = dwProgram;
                        pVoice->m_dwPriority = m_dwPriority[nPart];
                        pVoice->m_pControl = this;
                        pVoice->m_pRegion = pRegion;
                        pVoice->m_dwNoteID = sdwNoteID;

                        if (pVoice->StartVoice(m_pSynth,
                            pRegion, note.m_stTime, 
                            &m_ModWheel[nPart],
                            &m_PitchBend[nPart],
                            &m_Expression[nPart],
                            &m_Volume[nPart],
                            &m_Pan[nPart],
                            &m_Pressure[nPart],
                            &m_ReverbSends[nPart],
                            &m_ChorusSends[nPart],
                            &m_CutOffFreqCC[nPart],
                            &m_BusIds[nPart],
                            (WORD)note.m_bKey,
                            (WORD)note.m_bVelocity,
                            m_vrMasterVolume, 
                            prPitch))
                        {
                            pVoice->m_fInUse = TRUE;
                            m_pSynth->QueueVoice(pVoice);
                            dwNotesLost = 0;    // Note played remove notelost assumpstion 
                        }
                        else
                        {
                            m_pSynth->m_VoicesFree.AddHead(pVoice);
                        }
                    } 
                } 
                m_pSynth->m_BuildStats.dwNotesLost += dwNotesLost;
            } 
            else 
            {
                Trace(1,"Error: No instrument/region was found for patch # %lx, note %ld\n",
                    dwProgram, (long) note.m_bKey);
            }
            LeaveCriticalSection(&m_pInstruments->m_CriticalSection);
        }
    }

    ::LeaveCriticalSection(&s_CriticalSection);
}

void CControlLogic::Flush(STIME stTime)

{
    DWORD dwIndex;
    ::EnterCriticalSection(&s_CriticalSection);
    if (!m_fEmpty)
    {        
        m_fEmpty = TRUE;
        for (dwIndex = 0;dwIndex < 16; dwIndex++)
        {
            m_fEmpty = m_ModWheel[dwIndex].FlushMIDI(stTime) && m_fEmpty;
            m_fEmpty = m_PitchBend[dwIndex].FlushMIDI(stTime) && m_fEmpty;
            m_fEmpty = m_Volume[dwIndex].FlushMIDI(stTime) && m_fEmpty;
            m_fEmpty = m_Expression[dwIndex].FlushMIDI(stTime) && m_fEmpty;
            m_fEmpty = m_Pan[dwIndex].FlushMIDI(stTime) && m_fEmpty;
            m_fEmpty = m_Pressure[dwIndex].FlushMIDI(stTime) && m_fEmpty;
            m_fEmpty = m_ReverbSends[dwIndex].FlushMIDI(stTime) && m_fEmpty;
            m_fEmpty = m_ChorusSends[dwIndex].FlushMIDI(stTime) && m_fEmpty;
            m_fEmpty = m_CutOffFreqCC[dwIndex].FlushMIDI(stTime) && m_fEmpty;
        }
        m_Notes.FlushMIDI(stTime);
    }
    ::LeaveCriticalSection(&s_CriticalSection);
}

BOOL CControlLogic::RecordMIDI(STIME stTime,BYTE bStatus, BYTE bData1, BYTE bData2)

{
    WORD nPreChannel = bStatus & 0xF; 
    CNote note;
    bStatus = bStatus & 0xF0;
    BOOL bReturn = TRUE;
    WORD nPart;

    ::EnterCriticalSection(&s_CriticalSection);

    for (nPart = 0;nPart < 16; nPart++)
    {
        if (nPreChannel == m_bPartToChannel[nPart])
        {
            switch (bStatus)
            {
            case MIDI_NOTEOFF :
                bData2 = 0;
            case MIDI_NOTEON :
                note.m_bPart = (BYTE) nPart;
                note.m_bKey = bData1;
                note.m_bVelocity = bData2;
                bReturn = m_Notes.RecordNote(stTime,&note);
                break;
            case MIDI_CCHANGE :
                switch (bData1)
                {
                case CC_BANKSELECTH :
                    bReturn = m_Notes.RecordEvent(stTime, nPart, NOTE_CC_BANKSELECTH, bData2);
                    break;
                case CC_MODWHEEL :
                    m_fEmpty = FALSE;
                    bReturn = m_ModWheel[nPart].RecordMIDI(stTime,(long) bData2);
                    break;
                case CC_VOLUME :
                    m_fEmpty = FALSE;
                    bReturn = m_Volume[nPart].RecordMIDI(stTime,(long) bData2);
                    break;
                case CC_PAN :
                    m_fEmpty = FALSE;
                    bReturn = m_Pan[nPart].RecordMIDI(stTime,(long) bData2);
                    break;
                case CC_EXPRESSION :
                    m_fEmpty = FALSE;
                    bReturn = m_Expression[nPart].RecordMIDI(stTime,(long)bData2);
                    break;
                case CC_BANKSELECTL :
                    bReturn = m_Notes.RecordEvent(stTime, nPart, NOTE_CC_BANKSELECTL, bData2);
                    break;
                case CC_RESETALL:
                    m_fEmpty = FALSE;
                    if (bData2)
                    {
                        bReturn = bReturn && m_Volume[nPart].RecordMIDI(stTime, 100);
                        bReturn = bReturn && m_Pan[nPart].RecordMIDI(stTime, 64);
                    }
                    bReturn = bReturn && m_Expression[nPart].RecordMIDI(stTime, 127);
                    bReturn = bReturn && m_PitchBend[nPart].RecordMIDI(stTime, 0x2000);
                    bReturn = bReturn && m_ModWheel[nPart].RecordMIDI(stTime, 0);   
                    bReturn = bReturn && m_Pressure[nPart].RecordMIDI(stTime, 0);
                    bReturn = bReturn && m_ReverbSends[nPart].RecordMIDI(stTime, 40);
                    bReturn = bReturn && m_ChorusSends[nPart].RecordMIDI(stTime, 0);
                    bReturn = bReturn && m_CutOffFreqCC[nPart].RecordMIDI(stTime, 64);
                    bData2 = 0;
                    // fall through into Sustain Off case....
        
                case CC_SUSTAIN :
                    bReturn = m_Notes.RecordEvent(stTime, nPart, NOTE_SUSTAIN, bData2);
                    break;
                case CC_ALLSOUNDSOFF:
                    bReturn = m_Notes.RecordEvent(stTime, nPart, NOTE_SOUNDSOFF, 0);
                    break;
                case CC_ALLNOTESOFF:
                    bReturn = m_Notes.RecordEvent(stTime, nPart, NOTE_ALLOFF, 0);
                    break;
                case CC_DATAENTRYMSB:
                    bReturn = m_Notes.RecordEvent(stTime, nPart, NOTE_CC_DATAENTRYMSB, bData2);
                    break;
                case CC_DATAENTRYLSB:
                    bReturn = m_Notes.RecordEvent(stTime, nPart, NOTE_CC_DATAENTRYLSB, bData2);
                    break;
                case CC_NRPN_LSB :
                case CC_NRPN_MSB :
                    bReturn = m_Notes.RecordEvent(stTime, nPart, NOTE_CC_NRPN, bData2);
                    break;
                case CC_RPN_LSB:
                    bReturn = m_Notes.RecordEvent(stTime, nPart, NOTE_CC_RPN_LSB, bData2);
                    break; 
                case CC_RPN_MSB:
                    bReturn = m_Notes.RecordEvent(stTime, nPart, NOTE_CC_RPN_MSB, bData2);
                    break;
                case CC_MONOMODE :
                    bReturn = m_Notes.RecordEvent(stTime, nPart, NOTE_CC_MONOMODE, bData2);
                    bReturn = m_Notes.RecordEvent(stTime, nPart, NOTE_SOUNDSOFF, 0);
                    break;
                case CC_POLYMODE :
                    bReturn = m_Notes.RecordEvent(stTime, nPart, NOTE_CC_POLYMODE, bData2);
                    bReturn = m_Notes.RecordEvent(stTime, nPart, NOTE_SOUNDSOFF, 0);
                    break;
                case CC_REVERB :
                    m_fEmpty = FALSE;
                    bReturn = m_ReverbSends[nPart].RecordMIDI(stTime,(long)bData2);
                    break;
                case CC_CHORUS :
                    m_fEmpty = FALSE;
                    bReturn = m_ChorusSends[nPart].RecordMIDI(stTime,(long)bData2);
                    break;
                case CC_CUTOFFFREQ:
                    m_fEmpty = FALSE;
                    bReturn = m_CutOffFreqCC[nPart].RecordMIDI(stTime,(long)bData2);
                    break;
                default:
                    break;
                }
                break;

            case MIDI_PCHANGE :
                bReturn = m_Notes.RecordEvent(stTime, nPart, NOTE_PROGRAMCHANGE, bData1);
                break;
        
            case MIDI_PBEND :
                m_fEmpty = FALSE;
                {
                    WORD nBend;
                    nBend = bData2 << 7;
                    nBend |= bData1;
                    bReturn = m_PitchBend[nPart].RecordMIDI(stTime,(long)nBend);
                }
                break;

            case MIDI_MTOUCH:
                m_fEmpty = FALSE;
                bReturn = m_Pressure[nPart].RecordMIDI(stTime,(long)bData1);
                break;
            }
        }
    }
    ::LeaveCriticalSection(&s_CriticalSection);

    return bReturn;
}

HRESULT CControlLogic::RecordSysEx(DWORD dwSysExLength,BYTE *pSysExData, STIME stTime)

{
    HRESULT hSuccess = S_OK;
    int nPart;
    int nTune;
    DWORD dwAddress;
    BOOL fClearAll = FALSE;
    BOOL fResetPatches = FALSE;
    if (dwSysExLength < 6) 
    {
        Trace(4,"Warning: Unknown sysex message sent to synth.\n");
        return E_FAIL;
    }

    EnterCriticalSection(&s_CriticalSection);

    switch (pSysExData[1])  // ID number
    {
    case 0x7E : // General purpose ID
        if (pSysExData[3] == 0x09) 
        {
            GMReset();
            fClearAll = TRUE;
            fResetPatches = TRUE;
        }
        break;
    case 0x7F : // Real time ID
        if (pSysExData[3] == 0x04)
        {
            if (pSysExData[4] == 1) // Master Volume
            {
                m_Notes.RecordEvent(stTime, 0, NOTE_MASTERVOLUME, pSysExData[6]);
            }
        }
        break;
    case 0x41 : // Roland
        if (dwSysExLength < 11) 
        {
            Trace(4,"Warning: Unknown sysex message sent to synth.\n");
            LeaveCriticalSection(&s_CriticalSection);
            return E_FAIL;
        }
        if (pSysExData[3] != 0x42) break;
        if (pSysExData[4] != 0x12) break;
        nPart = pSysExData[6] & 0xF;
        dwAddress = (pSysExData[5] << 16) |
            ((pSysExData[6] & 0xF0) << 8) | pSysExData[7];
        switch (dwAddress)
        {
        case 0x40007F :     // GS Reset.
            GMReset();
            m_fXGActive = FALSE;
            fClearAll = TRUE;
            m_fGSActive = TRUE;
            fResetPatches = TRUE;
            break;
        case 0x401002 :     // Set Receive Channel.
            if (m_fGSActive)
            {
                if (pSysExData[8])
                {
                    m_Notes.RecordEvent(stTime, nPart, NOTE_ASSIGNRECEIVE, pSysExData[8] - 1);
                }
            }
            break;
        case 0x401015 :     // Use for Rhythm.
            if (m_fGSActive)
            {
                m_bDrums[nPart] = pSysExData[8];
                fClearAll = TRUE;
            }
            break;
        case 0x401040 :     // Scale Tuning.
            if (m_fGSActive)
            {
                for (nTune = 0;nTune < 12; nTune++)
                {
                    if (pSysExData[9 + nTune] & 0x80) break;
                    m_prScaleTune[nPart][nTune] = 
                        (PREL) pSysExData[8 + nTune] - (PREL) 64;
                }
            }
            break;
        }
        break;
    case 0x43 : // Yamaha
        if ((pSysExData[3] == 0x4C) &&
            (pSysExData[4] == 0) &&
            (pSysExData[5] == 0) &&
            (pSysExData[6] == 0x7E) &&
            (pSysExData[7] == 0))
        {   // XG System On
            m_fXGActive = TRUE;
            m_fGSActive = FALSE;
            GMReset();
            m_fXGActive = TRUE;
            m_bDrums[0] = 0;
            m_bBankH[0] = 127;
            fClearAll = TRUE;
            fResetPatches = TRUE;
        }
        break;
    }
    if (fClearAll)
    {
        Flush(0);
        for (nPart = 0;nPart < 16;nPart++)
        {        
            m_Notes.RecordEvent(stTime, nPart, NOTE_SOUNDSOFF, 0);
            m_Notes.RecordEvent(stTime, nPart, NOTE_SUSTAIN, 0);
            m_Volume[nPart].RecordMIDI(stTime, 100);
            m_Pan[nPart].RecordMIDI(stTime, 64);
            m_Expression[nPart].RecordMIDI(stTime, 127);
            m_PitchBend[nPart].RecordMIDI(stTime, 0x2000);
            m_ModWheel[nPart].RecordMIDI(stTime, 0);
            m_Pressure[nPart].RecordMIDI(stTime, 0);
            m_ReverbSends[nPart].RecordMIDI(stTime, 40);
            m_ChorusSends[nPart].RecordMIDI(stTime, 0);
            m_CutOffFreqCC[nPart].RecordMIDI(stTime, 64);
        }
    }
    if (fResetPatches)
    {
        for (nPart = 0;nPart < 16;nPart++)
        {     
            if ((nPart == 0) && (m_fXGActive))
            {
                m_Notes.RecordEvent(stTime-1, nPart, NOTE_CC_BANKSELECTH, 127);
            }
            else
            {
                m_Notes.RecordEvent(stTime-1, nPart, NOTE_CC_BANKSELECTH, 0);
            }
            m_Notes.RecordEvent(stTime-1, nPart, NOTE_CC_BANKSELECTL, 0);
            m_Notes.RecordEvent(stTime, nPart, NOTE_PROGRAMCHANGE, 0);
        }
    }

    LeaveCriticalSection(&s_CriticalSection);
    return hSuccess;
}

HRESULT CControlLogic::SetChannelPriority(DWORD dwChannel,DWORD dwPriority)

{
    DWORD dwPart;
    for (dwPart = 0;dwPart < 16; dwPart++)
    {
        if (m_bPartToChannel[dwPart] == dwChannel)
        {
            m_dwPriority[dwPart] = dwPriority;
        }
    }
    return S_OK;
}

HRESULT CControlLogic::GetChannelPriority(DWORD dwChannel,LPDWORD pdwPriority)

{
    DWORD dwPart;
    for (dwPart = 0;dwPart < 16; dwPart++)
    {
        if (m_bPartToChannel[dwPart] == dwChannel)
        {
            *pdwPriority = m_dwPriority[dwPart];
            break;
        }
    }
    return S_OK;
}

//////////////////////////////////////////////////////////
// Directx8 Methods 

BOOL CControlLogic::RecordWaveEvent(
    STIME stTime, BYTE bChannel, DWORD dwVoiceId, VREL vrVolume, PREL prPitch, 
    SAMPLE_TIME stVoiceStart, SAMPLE_TIME stLoopStart, SAMPLE_TIME stLoopEnd,
    CWaveArt* pWaveArt)

{
    WORD nPreChannel = bChannel;
    CWaveEvent wave;
    BOOL bReturn = TRUE;
    WORD nPart;

    ::EnterCriticalSection(&s_CriticalSection);

    for (nPart = 0;nPart < 16; nPart++)
    {
        if (nPreChannel == m_bPartToChannel[nPart])
        {
            wave.m_bPart        = (BYTE)nPart;
            wave.m_dwVoiceId    = dwVoiceId;
            wave.m_vrVolume     = vrVolume;
            wave.m_prPitch      = prPitch;
            wave.m_pWaveArt     = pWaveArt;
            wave.m_stVoiceStart = stVoiceStart;
            wave.m_stLoopStart  = stLoopStart;
            wave.m_stLoopEnd    = stLoopEnd;
            wave.m_pWaveArt->AddRef();

            bReturn = m_Waves.RecordWave(stTime,&wave);
        }
    }

    ::LeaveCriticalSection(&s_CriticalSection);

    return bReturn;
}

void CControlLogic::QueueWaves(STIME stEndTime)
{
    CWaveEvent wave;
    BOOL fGotAWave = TRUE;

    while (fGotAWave)
    {
        EnterCriticalSection(&m_pInstruments->m_CriticalSection);
        fGotAWave = m_Waves.GetWave(stEndTime,&wave);
        if (!fGotAWave)
        {
            LeaveCriticalSection(&m_pInstruments->m_CriticalSection);
            break;
        }

        WORD nPart = wave.m_bPart;

        CVoice * pVoice = m_pSynth->m_VoicesFree.RemoveHead();

        if (pVoice == NULL)
        {
            pVoice = m_pSynth->m_VoicesExtra.RemoveHead();
        }

        if (pVoice == NULL)
        {
            pVoice = m_pSynth->StealVoice(m_dwPriority[nPart]);
        }

        if (pVoice != NULL)
        {
            PREL prPitch = wave.m_prPitch;
            prPitch += m_prFineTune[nPart]; 
            prPitch += m_prCoarseTune[nPart];

            pVoice->m_nKey = 0xffff;                // set to unused values 
            pVoice->m_dwProgram = 0xffffffff;       // set to unused values 
            pVoice->m_nPart = nPart;
            pVoice->m_dwPriority = m_dwPriority[nPart];
            pVoice->m_pControl = this;
            pVoice->m_pRegion  = NULL;

            if (pVoice->StartWave(m_pSynth,
                wave.m_pWaveArt,
                wave.m_dwVoiceId, 
                wave.m_stTime, 
                &m_PitchBend[nPart],
                &m_Expression[nPart],
                &m_Volume[nPart],
                &m_Pan[nPart],
                &m_ReverbSends[nPart],
                &m_ChorusSends[nPart],
                &m_CutOffFreqCC[nPart],
                &m_BusIds[nPart],
                wave.m_vrVolume,    
                prPitch,
                wave.m_stVoiceStart,
                wave.m_stLoopStart,
                wave.m_stLoopEnd
                ))
            {
                pVoice->m_fInUse = TRUE;
                m_pSynth->QueueVoice(pVoice);
            }
            else
            {
                m_pSynth->m_VoicesFree.AddHead(pVoice);
            }
        } 
        else 
        {
            Trace(1,"Error: No voice avaible for voice id #%lx\n", wave.m_dwVoiceId);
        }
        if (wave.m_pWaveArt) wave.m_pWaveArt->Release(); // no longer need to hold this ref count
        LeaveCriticalSection(&m_pInstruments->m_CriticalSection);
    }
}

void CControlLogic::FlushWaveByStopTime(DWORD dwID, STIME stStopTime)
{
    EnterCriticalSection(&m_pInstruments->m_CriticalSection);
    m_Waves.RemoveWaveByStopTime(dwID, stStopTime);
    LeaveCriticalSection(&m_pInstruments->m_CriticalSection);
}

HRESULT CControlLogic::AssignChannelToBuses(DWORD dwChannel, LPDWORD pdwBusIds, DWORD dwBusCount)
{
    HRESULT hr = S_OK;

    DWORD dwPart;
    for (dwPart = 0;dwPart < 16; dwPart++)
    {
        if (m_bPartToChannel[dwPart] == dwChannel)
        {
            //
            // Assign new bus ids to this channel 
            //
            if ( pdwBusIds && dwBusCount > 0 )
            {
                hr = m_BusIds[dwPart].AssignBuses(pdwBusIds, dwBusCount);
            }
        }
    }

    return hr;
}

/////////////////////////////////////////////////////////////////
CWaveDataList    CWaveIn::m_sFreeList;
DWORD            CWaveIn::m_sUsageCount = 0;


CWaveData::CWaveData() 
{
    m_stTime = 0;
}

CWaveIn::CWaveIn()
{
    m_sUsageCount++;
    m_stCurrentTime = 0;
}

CWaveIn::~CWaveIn()

{
    ClearWave(0x7FFFFFFF);
    m_sUsageCount--;
    // If there are no instances of CMIDIRecorder left, get rid of the free pool.
    if (!m_sUsageCount)
    {
        CWaveData *pWD;
        while (pWD = m_sFreeList.RemoveHead())
        {
            delete pWD;
        }
    }
}

/*void CWaveIn::Init()
{
    int nIndex;
    static BOOL fAlreadyDone = FALSE;
    if (!fAlreadyDone)
    {
        m_sFreeList.RemoveAll();
        for (nIndex = 0; nIndex < MAX_MIDI_EVENTS; nIndex++)
        {
            m_sFreeList.AddHead(&m_sEventBuffer[nIndex]);
        }
        fAlreadyDone = TRUE;
    }
}*/

/*
BOOL CWaveIn::FlushWave(STIME stTime)

{
    CWaveData *pWD;
    CWaveData *pLast = NULL;

    for (pWD = m_EventList.GetHead();pWD != NULL;pWD = pWD->GetNext())
    {
        if (pWD->m_stTime >= stTime)
        {
            if (pLast == NULL)
            {
                m_EventList.RemoveAll();
            }
            else
            {
                pLast->SetNext(NULL);
            }
            m_sFreeList.Cat(pWD);
            break;
        }
        pLast = pWD;
    }
    return m_EventList.IsEmpty();
}
*/

BOOL CWaveIn::ClearWave(STIME stTime)

{
    CWaveData *pWD;

    for (;pWD = m_EventList.GetHead();)
    {
        if (pWD->m_stTime < stTime)
        {
            m_EventList.RemoveHead();
            m_stCurrentTime = pWD->m_stTime;
            m_lCurrentData  = pWD->m_WaveEventData;
            if (pWD->m_WaveEventData.m_pWaveArt)
            {
                pWD->m_WaveEventData.m_pWaveArt->Release();
                pWD->m_WaveEventData.m_pWaveArt = NULL;
            }
            m_sFreeList.AddHead(pWD);
            
        }
        else break;
    }
    return m_EventList.IsEmpty();
}

BOOL CWaveIn::RemoveWave(DWORD dwID)

{
    CWaveData *pWD = m_EventList.GetHead();
    CWaveData *pWDNext = NULL;

    for (; pWD; pWD = pWDNext)
    {
        pWDNext = pWD->GetNext();
        if (pWD->m_WaveEventData.m_dwVoiceId == dwID)
        {
            m_EventList.Remove(pWD);
            if (pWD->m_WaveEventData.m_pWaveArt)
            {
                pWD->m_WaveEventData.m_pWaveArt->Release();
                pWD->m_WaveEventData.m_pWaveArt = NULL;
            }
            m_sFreeList.AddHead(pWD);
        }
    }
    return m_EventList.IsEmpty();
}

BOOL CWaveIn::RemoveWaveByStopTime(DWORD dwID, STIME stStopTime)

{
    CWaveData *pWD = m_EventList.GetHead();
    CWaveData *pWDNext = NULL;

    for (; pWD; pWD = pWDNext)
    {
        pWDNext = pWD->GetNext();
        if (pWD->m_WaveEventData.m_dwVoiceId == dwID && pWD->m_stTime >= stStopTime)
        {
            m_EventList.Remove(pWD);
            if (pWD->m_WaveEventData.m_pWaveArt)
            {
                pWD->m_WaveEventData.m_pWaveArt->Release();
                pWD->m_WaveEventData.m_pWaveArt = NULL;
            }
            m_sFreeList.AddHead(pWD);
        }
    }
    return m_EventList.IsEmpty();
}

BOOL CWaveIn::RecordWave(STIME stTime, CWaveEvent* pWave)

{
    CWaveData *pWD = m_sFreeList.RemoveHead();
    if (!pWD)
    {
        pWD = new CWaveData;
    }    

    CWaveData *pScan = m_EventList.GetHead();
    CWaveData *pNext;

    if (pWD)
    {
        pWD->m_stTime        = stTime;
        pWD->m_WaveEventData = *pWave;
        pWD->m_WaveEventData.m_stTime = stTime;
        if (pScan == NULL)
        {
            m_EventList.AddHead(pWD);
        }
        else
        {
            if (pScan->m_stTime > stTime)
            {
                m_EventList.AddHead(pWD);
            }
            else
            {
                for (;pScan != NULL; pScan = pNext)
                {
                    pNext = pScan->GetNext();
                    if (pNext == NULL)
                    {
                        pScan->SetNext(pWD);
                    }
                    else
                    {
                        if (pNext->m_stTime > stTime)
                        {
                            pWD->SetNext(pNext);
                            pScan->SetNext(pWD);
                            break;
                        }
                    }
                }
            }
        }
        return (TRUE);
    }
    Trace(1,"Error: Wave Event pool empty.\n");

    return (FALSE);
}

BOOL CWaveIn::GetWave(STIME stTime, CWaveEvent *pWave)

{
    CWaveData *pWD = m_EventList.GetHead();

    if (pWD != NULL)
    {
        if (pWD->m_stTime <= stTime)
        {
            *pWave =  pWD->m_WaveEventData;
            m_EventList.RemoveHead();
            m_sFreeList.AddHead(pWD);
            return (TRUE);
        }
    }
    return (FALSE);
}

/////////////////////////////////////////////////////////////////
CBusIds::CBusIds()
{
    m_dwBusIds[0] = DSBUSID_LEFT;
    m_dwBusIds[1] = DSBUSID_RIGHT;
    m_dwBusIds[2] = DSBUSID_REVERB_SEND;
    m_dwBusIds[3] = DSBUSID_CHORUS_SEND;
    m_dwBusCount = NUM_DEFAULT_BUSES;
}

CBusIds::~CBusIds()
{
}

HRESULT CBusIds::Initialize()
{
    HRESULT hr = S_OK;

    m_dwBusIds[0] = DSBUSID_LEFT;
    m_dwBusIds[1] = DSBUSID_RIGHT;
    m_dwBusIds[2] = DSBUSID_REVERB_SEND;
    m_dwBusIds[3] = DSBUSID_CHORUS_SEND;
    m_dwBusCount = NUM_DEFAULT_BUSES;

    return hr;
}

HRESULT CBusIds::AssignBuses(LPDWORD pdwBusIds, DWORD dwBusCount)
{
    HRESULT hr = S_OK;

    if ( pdwBusIds && dwBusCount > 0 )
    {
        if ( dwBusCount > MAX_DAUD_CHAN )
            dwBusCount = MAX_DAUD_CHAN;

        memcpy(m_dwBusIds,pdwBusIds,sizeof(DWORD)*dwBusCount);
        m_dwBusCount = dwBusCount;
    }

    return hr;
}

