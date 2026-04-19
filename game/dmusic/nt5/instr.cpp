//
// Copyright (c) 1996-2001 Microsoft Corporation
// Instrument.cpp
//

#ifdef DMSYNTH_MINIPORT
#include "common.h"
#else
#include "simple.h"
#include <mmsystem.h>
#include <dmerror.h>
#include "synth.h"
#include "math.h"
#include "debug.h"
// @@BEGIN_DDKSPLIT -- This section will be removed in the DDK sample.  See ddkreadme.txt for more info.
#include "..\shared\validate.h"
#if 0 // The following section will only take affect in the DDK sample.
// @@END_DDKSPLIT
#include "validate.h"
// @@BEGIN_DDKSPLIT -- This section will be removed in the DDK sample.
#endif
// @@END_DDKSPLIT
#endif

void MemDump(char * prompt);

//#include <windowsx.h>

CSourceLFO::CSourceLFO()

{
    m_pfFrequency = 3804; // f = (256*4096*16*5hz)/(samplerate)
    m_stDelay = 0;
    m_prMWPitchScale = 0;
    m_vrMWVolumeScale = 0;
    m_vrVolumeScale = 0;
    m_prPitchScale = 0;
    m_prCPPitchScale = 0;
    m_vrCPVolumeScale = 0;
    m_prCutoffScale = 0;
    m_prMWCutoffScale = 0;
    m_prCPCutoffScale = 0;
}

void CSourceLFO::Init(DWORD dwSampleRate)

{
    m_pfFrequency = (256 * 4096 * 16 * 5) / dwSampleRate;
    m_stDelay = 0;
    m_prMWPitchScale = 0;
    m_vrMWVolumeScale = 0;
    m_vrVolumeScale = 0;
    m_prPitchScale = 0;
    m_prCPPitchScale = 0;
    m_vrCPVolumeScale = 0;
    m_prCutoffScale = 0;
    m_prMWCutoffScale = 0;
    m_prCPCutoffScale = 0;
}

void CSourceLFO::SetSampleRate(long lChange)

{
    if (lChange > 0)
    {
        m_stDelay <<= lChange;
        m_pfFrequency <<= lChange;
    }
    else
    {
        m_stDelay >>= -lChange;
        m_pfFrequency >>= -lChange;
    }
}

void CSourceLFO::Verify()

{
    FORCEBOUNDS(m_pfFrequency,64,7600);
    FORCEBOUNDS(m_stDelay,0,441000);
    FORCEBOUNDS(m_vrVolumeScale,-1200,1200);
    FORCEBOUNDS(m_vrMWVolumeScale,-1200,1200);
    FORCEBOUNDS(m_prPitchScale,-1200,1200);
    FORCEBOUNDS(m_prMWPitchScale,-1200,1200);
    FORCEBOUNDS(m_prCPPitchScale,-1200,1200);
    FORCEBOUNDS(m_vrCPVolumeScale,-1200,1200);
    FORCEBOUNDS(m_prCutoffScale, -12800, 12800);
    FORCEBOUNDS(m_prMWCutoffScale, -12800, 12800);
    FORCEBOUNDS(m_prCPCutoffScale, -12800, 12800);
}

CSourceEG::CSourceEG()

{
    Init();
}

void CSourceEG::Init()

{
    m_stAttack = 0;
    m_stDecay = 0;
    m_pcSustain = 1000;
    m_stRelease = 0;
    m_trVelAttackScale = 0;
    m_trKeyDecayScale = 0;
    m_sScale = 0;
    m_stDelay = 0;
    m_stHold = 0;
    m_prCutoffScale = 0;
}

void CSourceEG::SetSampleRate(long lChange)

{
    if (lChange > 0)
    {
        m_stAttack <<= lChange;
        m_stDecay <<= lChange;
        m_stRelease <<= lChange;
    }
    else
    {
        m_stAttack >>= -lChange;
        m_stDecay >>= -lChange;
        m_stRelease >>= -lChange;
    }
}

void CSourceEG::Verify()

{
    FORCEBOUNDS(m_stAttack,0,1764000);
    FORCEBOUNDS(m_stDecay,0,1764000);
    FORCEBOUNDS(m_pcSustain,0,1000);
    FORCEBOUNDS(m_stRelease,0,1764000);
    FORCEBOUNDS(m_sScale,-1200,1200);
    FORCEBOUNDS(m_trKeyDecayScale,-12000,12000);
    FORCEBOUNDS(m_trVelAttackScale,-12000,12000);
    FORCEBOUNDS(m_trKeyHoldScale,-12000,12000);
    FORCEBOUNDS(m_prCutoffScale,-12800,12800);
}

CSourceFilter::CSourceFilter()
{
    Init(22050);
}

void CSourceFilter::Init(DWORD dwSampleRate)
{
    // First, calculate the playback samplerate in pitch rels.
    // The reference frequency is a440, which is midi note 69.
    // So, calculate the ratio of the sample rate to 440 and
    // convert into prels (1200 per octave), then add the
    // offset of 6900.
    double fSampleRate = (double)dwSampleRate;

    fSampleRate /= 440.0;
    fSampleRate = log(fSampleRate) / log(2.0);
    fSampleRate *= 1200.0;
    fSampleRate += 6900.0;
    m_prSampleRate = (PRELS)fSampleRate;

    m_prCutoff = (PRELS)0x7FFF;
    m_vrQ = (VRELS)0;
    m_prVelScale = (PRELS)0;
    m_prCutoffSRAdjust = 0;
    m_iQIndex = 0;
}

void CSourceFilter::SetSampleRate(LONG lChange)
{
    // lChange ==  1 -> doubles -> add 1200 cents
    // lChange ==  2 -> quad    -> add 2400 cents
    // lChange == -1 -> halves  -> sub 1200 cents
    // lChange == -2 -> 1/4ths  -> sub 2400 cents
    //
    if (lChange > 0)
    {
        m_prSampleRate += (1200 << (lChange - 1));
    }
    else
    {
        m_prSampleRate -= (1200 << ((-lChange) - 1));
    }

    m_prCutoffSRAdjust = FILTER_FREQ_RANGE - m_prSampleRate + m_prCutoff;
}

void CSourceFilter::Verify()
{
    if ( m_prCutoff == 0x7FFF )
    {
        m_vrQ = 0;
        m_prVelScale = 0;
    }
    else
    {
        FORCEBOUNDS(m_prCutoff, 5535, 11921);
        FORCEBOUNDS(m_vrQ, 0, 225);
        FORCEBOUNDS(m_prVelScale, -12800, 12800);
    }
}

CSourceArticulation::CSourceArticulation()

{
//    m_sVelToVolScale = -9600;
    m_wUsageCount = 0;
    m_sDefaultPan = 0;
    m_dwSampleRate = 22050;
    m_PitchEG.m_sScale = 0; // pitch envelope defaults to off
}

void CSourceArticulation::Init(DWORD dwSampleRate)

{
    m_dwSampleRate = dwSampleRate;
    m_LFO.Init(dwSampleRate);       // Set to default values.
    m_PitchEG.Init();
    m_VolumeEG.Init();
    m_LFO2.Init(dwSampleRate);
    m_Filter.Init(dwSampleRate);
}

void CSourceArticulation::SetSampleRate(DWORD dwSampleRate)

{
    if (dwSampleRate != m_dwSampleRate)
    {
        long lChange;
        if (dwSampleRate > (m_dwSampleRate * 2))
        {
            lChange = 2;        // going from 11 to 44.
        }
        else if (dwSampleRate > m_dwSampleRate)
        {
            lChange = 1;        // must be doubling
        }
        else if ((dwSampleRate * 2) < m_dwSampleRate)
        {
            lChange = -2;       // going from 44 to 11
        }
        else
        {
            lChange = -1;       // that leaves halving.
        }
        m_dwSampleRate = dwSampleRate;
        m_LFO.SetSampleRate(lChange);
        m_PitchEG.SetSampleRate(lChange);
        m_VolumeEG.SetSampleRate(lChange);
        m_LFO2.SetSampleRate(lChange);
        m_Filter.SetSampleRate(lChange);
    }
}

void CSourceArticulation::Verify()

{
    m_LFO.Verify();
    m_PitchEG.Verify();
    m_VolumeEG.Verify();
    m_LFO2.Verify();
    m_Filter.Verify();
}

void CSourceArticulation::AddRef()

{
    m_wUsageCount++;
}

void CSourceArticulation::Release()

{
    m_wUsageCount--;
    if (m_wUsageCount == 0)
    {
        delete this;
    }
}

CSourceSample::CSourceSample()

{
    m_pWave = NULL;
    m_dwLoopStart = 0;
    m_dwLoopEnd = 1;
    m_dwLoopType = WLOOP_TYPE_FORWARD;
    m_dwSampleLength = 0;
    m_prFineTune = 0;
    m_dwSampleRate = 22050;
    m_bMIDIRootKey = 60;
    m_bOneShot = TRUE;
    m_bSampleType = 0;
}

CSourceSample::~CSourceSample()

{
    if (m_pWave != NULL)
    {
        m_pWave->Release();
    }
}

void CSourceSample::Verify()

{
    if (m_pWave != NULL)
    {
        FORCEBOUNDS(m_dwSampleLength,0,m_pWave->m_dwSampleLength);
        FORCEBOUNDS(m_dwLoopEnd,1,m_dwSampleLength);
        FORCEBOUNDS(m_dwLoopStart,0,m_dwLoopEnd);
        if ((m_dwLoopEnd - m_dwLoopStart) < 6)
        {
            m_bOneShot = TRUE;
        }
    }
    FORCEBOUNDS(m_dwSampleRate,3000,200000);
    FORCEBOUNDS(m_bMIDIRootKey,0,127);
    FORCEBOUNDS(m_prFineTune,-1200,1200);
}

BOOL CSourceSample::CopyFromWave()

{
    if (m_pWave == NULL)
    {
        return FALSE;
    }
    m_dwSampleLength = m_pWave->m_dwSampleLength;
    m_dwSampleRate = m_pWave->m_dwSampleRate;
    m_bSampleType = m_pWave->m_bSampleType;
    if (m_bOneShot)
    {
        m_dwSampleLength--;
        if (m_pWave->m_bSampleType & SFORMAT_16)
        {
            m_pWave->m_pnWave[m_dwSampleLength] = 0;
        }
        else
        {
            char *pBuffer = (char *) m_pWave->m_pnWave;
            pBuffer[m_dwSampleLength] = 0;
        }
    }
    else
    {
        if (m_dwLoopStart >= m_dwSampleLength)
        {
            m_dwLoopStart = 0;
        }
        if (m_pWave->m_bSampleType & SFORMAT_16)
        {
            m_pWave->m_pnWave[m_dwSampleLength-1] =
                m_pWave->m_pnWave[m_dwLoopStart];
        }
        else
        {
            char *pBuffer = (char *) m_pWave->m_pnWave;
            pBuffer[m_dwSampleLength-1] =
                pBuffer[m_dwLoopStart];
        }
    }
    Verify();
    return (TRUE);
}


CWave::CWave()
{
    m_hUserData = NULL;
    m_lpFreeHandle = NULL;
    m_pnWave = NULL;
    m_dwSampleRate = 22050;
    m_bSampleType = SFORMAT_16;
    m_dwSampleLength = 0;
    m_wUsageCount = 0;
    m_dwID = 0;
    m_wPlayCount = 0;
    m_bStream = FALSE;
    m_bActive = FALSE;
    m_bLastSampleInit = FALSE;
    m_bValid = FALSE;
}

CWave::~CWave()

{
    if (m_pnWave && m_lpFreeHandle)
    {
        m_lpFreeHandle((HANDLE) this,m_hUserData);
    }
}

void CWave::Verify()

{
    FORCEBOUNDS(m_dwSampleRate,3000,200000);
}

void CWave::PlayOn()

{
    m_wPlayCount++;
    AddRef();
}

void CWave::PlayOff()

{
    m_wPlayCount--;
    Release();
}

BOOL CWave::IsPlaying()

{
    return (m_wPlayCount);
}

void CWave::AddRef()

{
    m_wUsageCount++;
}

void CWave::Release()

{
    m_wUsageCount--;
    if (m_wUsageCount == 0)
    {
        delete this;
    }
}

CWaveArt::CWaveArt()
{
    m_wUsageCount = 1;
    m_dwID = 0;
    m_bSampleType = 0;
    m_bStream = FALSE;
    memset(&m_WaveArtDl,0,sizeof(DMUS_WAVEARTDL));
    memset(&m_WaveformatEx,0,sizeof(WAVEFORMATEX));
}

CWaveArt::~CWaveArt()
{
//>>>>>>>>>> clear list
    while(!m_pWaves.IsEmpty())
    {
        CWaveBuffer* pWaveBuffer = m_pWaves.RemoveHead();
        if(pWaveBuffer)
        {
            pWaveBuffer->m_pWave->Release();
            pWaveBuffer->m_pWave = NULL;
            delete pWaveBuffer;
        }
    }
}

void CWaveArt::AddRef()
{
    m_wUsageCount++;
}

void CWaveArt::Release()
{
    m_wUsageCount--;
    if (m_wUsageCount == 0)
    {
        delete this;
    }
}

void CWaveArt::Verify()
{
}

CSourceRegion::CSourceRegion()
{
    m_pArticulation = NULL;
    m_vrAttenuation = 0;
    m_prTuning = 0;
    m_bKeyHigh = 127;
    m_bKeyLow = 0;
    m_bGroup = 0;
    m_bAllowOverlap = FALSE;
    m_bVelocityHigh = 127;
    m_bVelocityLow  = 0;
    m_dwChannel = 0;
    m_sWaveLinkOptions = 0;
}

CSourceRegion::~CSourceRegion()
{
    if (m_pArticulation)
    {
        m_pArticulation->Release();
    }
}

void CSourceRegion::SetSampleRate(DWORD dwSampleRate)

{
    if (m_pArticulation != NULL)
    {
        m_pArticulation->SetSampleRate(dwSampleRate);
    }
}

void CSourceRegion::Verify()

{
    FORCEBOUNDS(m_bKeyHigh,0,127);
    FORCEBOUNDS(m_bKeyLow,0,127);
    FORCEBOUNDS(m_prTuning,-12000,12000);
    FORCEBOUNDS(m_vrAttenuation,-9600,0);
    m_Sample.Verify();
    if (m_pArticulation != NULL)
    {
        m_pArticulation->Verify();
    }
}

CInstrument::CInstrument()
{
    m_dwProgram = 0;
}

CInstrument::~CInstrument()
{
    while (!m_RegionList.IsEmpty())
    {
        CSourceRegion *pRegion = m_RegionList.RemoveHead();
        delete pRegion;
    }
}

void CInstrument::Verify()

{
    CSourceRegion *pRegion = m_RegionList.GetHead();
    CSourceArticulation *pArticulation = NULL;
    for (;pRegion != NULL;pRegion = pRegion->GetNext())
    {
        if (pRegion->m_pArticulation != NULL)
        {
            pArticulation = pRegion->m_pArticulation;
        }
        pRegion->Verify();
    }
    pRegion = m_RegionList.GetHead();
    for (;pRegion != NULL;pRegion = pRegion->GetNext())
    {
        if (pRegion->m_pArticulation == NULL  && pArticulation)
        {
            pRegion->m_pArticulation = pArticulation;
            pArticulation->AddRef();
        }
    }
}

void CInstrument::SetSampleRate(DWORD dwSampleRate)

{
    CSourceRegion *pRegion = m_RegionList.GetHead();
    for (;pRegion;pRegion = pRegion->GetNext())
    {
        pRegion->SetSampleRate(dwSampleRate);
    }
}

CSourceRegion * CInstrument::ScanForRegion(DWORD dwNoteValue, DWORD dwVelocity, CSourceRegion *pRegion)

{
    if ( pRegion == NULL )
        pRegion = m_RegionList.GetHead(); // Starting search
    else
        pRegion = pRegion->GetNext();     // Continuing search through the rest of the regions

    for (;pRegion;pRegion = pRegion->GetNext())
    {
        if (dwNoteValue >= pRegion->m_bKeyLow  &&
            dwNoteValue <= pRegion->m_bKeyHigh &&
            dwVelocity  >= pRegion->m_bVelocityLow &&
            dwVelocity  <= pRegion->m_bVelocityHigh )
        {
            break ;
        }
    }
    return pRegion;
}

void CInstManager::SetSampleRate(DWORD dwSampleRate)

{
    DWORD dwIndex;
    m_dwSampleRate = dwSampleRate;
    EnterCriticalSection(&m_CriticalSection);
    for (dwIndex = 0; dwIndex < INSTRUMENT_HASH_SIZE; dwIndex++)
    {
        CInstrument *pInstrument = m_InstrumentList[dwIndex].GetHead();
        for (;pInstrument != NULL; pInstrument = pInstrument->GetNext())
        {
            pInstrument->SetSampleRate(dwSampleRate);
        }
    }
    LeaveCriticalSection(&m_CriticalSection);
}

CInstManager::CInstManager()

{
    m_dwSampleRate = 22050;
    m_fCSInitialized = FALSE;
    InitializeCriticalSection(&m_CriticalSection);
    // Note: on pre-Blackcomb OS's, this call can raise an exception; if it
    // ever pops in stress, we can add an exception handler and retry loop.
    m_fCSInitialized = TRUE;
    m_dwSynthMemUse = 0;
}

CInstManager::~CInstManager()

{
    if (m_fCSInitialized)
    {
        DWORD dwIndex;
        for (dwIndex = 0; dwIndex < INSTRUMENT_HASH_SIZE; dwIndex++)
        {
            while (!m_InstrumentList[dwIndex].IsEmpty())
            {
                CInstrument *pInstrument = m_InstrumentList[dwIndex].RemoveHead();
                delete pInstrument;
            }
        }
        for (dwIndex = 0; dwIndex < WAVE_HASH_SIZE; dwIndex++)
        {
            while (!m_WavePool[dwIndex].IsEmpty())
            {
                CWave *pWave = m_WavePool[dwIndex].RemoveHead();
                pWave->Release();
            }
        }
        while (!m_FreeWavePool.IsEmpty())
        {
            CWave *pWave = m_FreeWavePool.RemoveHead();
            pWave->Release();
        }

        for(int nCount = 0; nCount < WAVEART_HASH_SIZE; nCount++)
        {
            while(!m_WaveArtList[nCount].IsEmpty())
            {
                CWaveArt* pWaveArt = m_WaveArtList[nCount].RemoveHead();
                if(pWaveArt)
                {
                    pWaveArt->Release();
                }
            }
        }

        DeleteCriticalSection(&m_CriticalSection);
    }
}

void CInstManager::Verify()

{
    DWORD dwIndex;
    EnterCriticalSection(&m_CriticalSection);
    for (dwIndex = 0;dwIndex < INSTRUMENT_HASH_SIZE; dwIndex++)
    {
        CInstrument *pInstrument = m_InstrumentList[dwIndex].GetHead();
        for (;pInstrument != NULL;pInstrument = pInstrument->GetNext())
        {
            pInstrument->Verify();
        }
    }
    LeaveCriticalSection(&m_CriticalSection);
}

CInstrument * CInstManager::GetInstrument(DWORD dwProgram, DWORD dwKey, DWORD dwVelocity)

{
    EnterCriticalSection(&m_CriticalSection);
    CInstrument *pInstrument = m_InstrumentList[dwProgram % INSTRUMENT_HASH_SIZE].GetHead();
    for (;pInstrument != NULL; pInstrument = pInstrument->GetNext())
    {
        if (pInstrument->m_dwProgram == dwProgram)
        {
            if (pInstrument->ScanForRegion(dwKey, dwVelocity, NULL) != NULL)
            {
                break;
            }
            else
            {
                Trace(2,"Warning: No region was found in instrument # %lx that matched note %ld\n",
                    dwProgram,dwKey);
            }
        }
    }
    LeaveCriticalSection(&m_CriticalSection);
    return (pInstrument);
}


DWORD TimeCents2Samples(long tcTime, DWORD dwSampleRate)
{
    if (tcTime ==  0x80000000) return (0);
    double flTemp = tcTime;
    flTemp /= (65536 * 1200);
    flTemp = pow(2.0,flTemp);
    flTemp *= dwSampleRate;
    return (DWORD) flTemp;
}

DWORD PitchCents2PitchFract(long pcRate,DWORD dwSampleRate)

{
    double fTemp = pcRate;
    fTemp /= 65536;
    fTemp -= 6900;
    fTemp /= 1200;
    fTemp = pow(2.0,fTemp);
    fTemp *= 7381975040.0; // (440*256*16*4096);
    fTemp /= dwSampleRate;
    return (DWORD) (fTemp);
}

HRESULT CSourceArticulation::Download(DMUS_DOWNLOADINFO * pInfo,
                                void * pvOffsetTable[],
                                DWORD dwIndex,
                                DWORD dwSampleRate,
                                BOOL fNewFormat)
{
    if (fNewFormat)
    {
        DMUS_ARTICULATION2 * pdmArtic =
            (DMUS_ARTICULATION2 *) pvOffsetTable[dwIndex];
        while (pdmArtic)
        {
            if (pdmArtic->ulArtIdx)
            {
                if (pdmArtic->ulArtIdx >= pInfo->dwNumOffsetTableEntries)
                {
                    Trace(1,"Error: Download failed because articulation chunk has an error.\n");
                    return DMUS_E_BADARTICULATION;
                }
                DWORD dwPosition;
                void *pData = pvOffsetTable[pdmArtic->ulArtIdx];
                CONNECTIONLIST * pConnectionList =
                    (CONNECTIONLIST *) pData;
                CONNECTION *pConnection;
                dwPosition = sizeof(CONNECTIONLIST);
                for (dwIndex = 0; dwIndex < pConnectionList->cConnections; dwIndex++)
                {
                    pConnection = (CONNECTION *) ((BYTE *)pData + dwPosition);
                    dwPosition += sizeof(CONNECTION);
                    switch (pConnection->usSource)
                    {
                    case CONN_SRC_NONE :
                        switch (pConnection->usDestination)
                        {
                        case CONN_DST_LFO_FREQUENCY :
                            m_LFO.m_pfFrequency = PitchCents2PitchFract(
                                pConnection->lScale,dwSampleRate);
                            break;
                        case CONN_DST_LFO_STARTDELAY :
                            m_LFO.m_stDelay = TimeCents2Samples(
                                (TCENT) pConnection->lScale,dwSampleRate);
                            break;
                        case CONN_DST_EG1_ATTACKTIME :
                            m_VolumeEG.m_stAttack = TimeCents2Samples(
                                (TCENT) pConnection->lScale,dwSampleRate);
                            break;
                        case CONN_DST_EG1_DECAYTIME :
                            m_VolumeEG.m_stDecay = TimeCents2Samples(
                            (TCENT) pConnection->lScale,dwSampleRate);
                            break;
                        case CONN_DST_EG1_SUSTAINLEVEL :
                            m_VolumeEG.m_pcSustain =
                                (SPERCENT) ((long) (pConnection->lScale >> 16));
                            break;
                        case CONN_DST_EG1_RELEASETIME :
                            m_VolumeEG.m_stRelease = TimeCents2Samples(
                                (TCENT) pConnection->lScale,dwSampleRate);
                            break;
                        case CONN_DST_EG2_ATTACKTIME :
                            m_PitchEG.m_stAttack = TimeCents2Samples(
                                (TCENT) pConnection->lScale,dwSampleRate);
                            break;
                        case CONN_DST_EG2_DECAYTIME :
                            m_PitchEG.m_stDecay = TimeCents2Samples(
                                (TCENT) pConnection->lScale,dwSampleRate);
                            break;
                        case CONN_DST_EG2_SUSTAINLEVEL :
                            m_PitchEG.m_pcSustain =
                                (SPERCENT) ((long) (pConnection->lScale >> 16));
                            break;
                        case CONN_DST_EG2_RELEASETIME :
                            m_PitchEG.m_stRelease = TimeCents2Samples(
                                (TCENT) pConnection->lScale,dwSampleRate);
                            break;
                        case CONN_DST_PAN :
                            m_sDefaultPan = (short)
                                ((long) ((long) pConnection->lScale >> 12) / 125);
                            break;

                        /* DLS2 */
                        case CONN_DST_EG1_DELAYTIME:
                            m_VolumeEG.m_stDelay = TimeCents2Samples(
                                (TCENT) pConnection->lScale,dwSampleRate);
                            break;
                        case CONN_DST_EG1_HOLDTIME:
                            m_VolumeEG.m_stHold  = TimeCents2Samples(
                                (TCENT) pConnection->lScale,dwSampleRate);
                            break;
                        case CONN_DST_EG2_DELAYTIME:
                            m_PitchEG.m_stDelay  = TimeCents2Samples(
                                (TCENT) pConnection->lScale,dwSampleRate);
                            break;
                        case CONN_DST_EG2_HOLDTIME:
                            m_PitchEG.m_stHold   = TimeCents2Samples(
                                (TCENT) pConnection->lScale,dwSampleRate);
                            break;
                        case CONN_DST_VIB_FREQUENCY :
                            m_LFO2.m_pfFrequency = PitchCents2PitchFract(
                                pConnection->lScale,dwSampleRate);
                            break;
                        case CONN_DST_VIB_STARTDELAY :
                            m_LFO2.m_stDelay = TimeCents2Samples(
                                (TCENT) pConnection->lScale,dwSampleRate);
                            break;
                        case CONN_DST_FILTER_CUTOFF:
                            // First, get the filter cutoff frequency, which is relative to a440.
                            m_Filter.m_prCutoff = (PRELS)
                                (pConnection->lScale >> 16);
                            // Then, calculate the resulting prel, taking into consideration
                            // the sample rate and the base of the filter coefficient lookup
                            // table, relative to the sample rate (FILTER_FREQ_RANGE).
                            // This number can then be used directly look up the coefficients in the
                            // filter table.
                            m_Filter.m_prCutoffSRAdjust = (PRELS)
                                FILTER_FREQ_RANGE - m_Filter.m_prSampleRate + m_Filter.m_prCutoff;
                            break;
                        case CONN_DST_FILTER_Q:
                            m_Filter.m_vrQ = (VRELS)
                                (pConnection->lScale >> 16); //>>>>>>>> not really VRELS, but 1/10th's
                            m_Filter.m_iQIndex = (DWORD)
                                ((m_Filter.m_vrQ / 15.0f) + 0.5f);
                            break;
                        }
                        break;
                    case CONN_SRC_LFO :
                        switch (pConnection->usControl)
                        {
                        case CONN_SRC_NONE :
                            switch (pConnection->usDestination)
                            {
                            case CONN_DST_ATTENUATION :
                                m_LFO.m_vrVolumeScale = (VRELS)
                                    ((long) ((pConnection->lScale * 10) >> 16));
                                break;
                            case CONN_DST_PITCH :
                                m_LFO.m_prPitchScale = (PRELS)
                                    ((long) (pConnection->lScale >> 16));
                                break;

                            /* DLS2 */
                            case CONN_DST_FILTER_CUTOFF:
                                m_LFO.m_prCutoffScale = (PRELS)
                                    (pConnection->lScale >> 16);
                                break;
                            }
                            break;
                        case CONN_SRC_CC1 :
                            switch (pConnection->usDestination)
                            {
                            case CONN_DST_ATTENUATION :
                                m_LFO.m_vrMWVolumeScale = (VRELS)
                                    ((long) ((pConnection->lScale * 10) >> 16));
                                break;
                            case CONN_DST_PITCH :
                                m_LFO.m_prMWPitchScale = (PRELS)
                                    ((long) (pConnection->lScale >> 16));
                                break;

                            /* DLS2 */
                            case CONN_DST_FILTER_CUTOFF:
                                m_LFO.m_prMWCutoffScale = (PRELS)
                                    ((long) (pConnection->lScale >> 16));
                                break;
                            }
                            break;

                        /* DLS2 */
                        case CONN_SRC_CHANNELPRESSURE :
                            switch (pConnection->usDestination)
                            {
                            case CONN_DST_ATTENUATION :
                                m_LFO.m_vrCPVolumeScale = (VRELS)
                                    ((long) (pConnection->lScale >> 16));
                                break;
                            case CONN_DST_PITCH :
                                m_LFO.m_prCPPitchScale  = (PRELS)
                                    ((long) (pConnection->lScale >> 16));
                                break;

                            /* DLS2 */
                            case CONN_DST_FILTER_CUTOFF:
                                m_LFO.m_prCPCutoffScale = (PRELS)
                                    ((long) (pConnection->lScale >> 16));
                                break;
                            }
                            break;
                        }
                        break;
                    case CONN_SRC_KEYONVELOCITY :
                        switch (pConnection->usDestination)
                        {
                        case CONN_DST_EG1_ATTACKTIME :
                            m_VolumeEG.m_trVelAttackScale = (TRELS)
                                ((long) (pConnection->lScale >> 16));
                            break;
                        case CONN_DST_EG2_ATTACKTIME :
                            m_PitchEG.m_trVelAttackScale = (TRELS)
                                ((long) (pConnection->lScale >> 16));
                            break;

                        /* DLS2 */
                        case CONN_DST_FILTER_CUTOFF:
                            m_Filter.m_prVelScale = (PRELS)
                                ((long) (pConnection->lScale >> 16));
                            break;
                        }
                        break;
                    case CONN_SRC_KEYNUMBER :
                        switch (pConnection->usDestination)
                        {
                        case CONN_DST_EG1_DECAYTIME :
                            m_VolumeEG.m_trKeyDecayScale = (TRELS)
                                ((long) (pConnection->lScale >> 16));
                            break;
                        case CONN_DST_EG2_DECAYTIME :
                            m_PitchEG.m_trKeyDecayScale = (TRELS)
                                ((long) (pConnection->lScale >> 16));
                            break;

                        /* DLS2 */
                        case CONN_DST_EG1_HOLDTIME :
                            m_PitchEG.m_trKeyDecayScale = (TRELS)
                                ((long) (pConnection->lScale >> 16));
                            break;
                        case CONN_DST_EG2_HOLDTIME :
                            m_PitchEG.m_trKeyDecayScale = (TRELS)
                                ((long) (pConnection->lScale >> 16));
                        case CONN_DST_FILTER_CUTOFF :
                            m_Filter.m_prKeyScale = (PRELS)
                                ((long) (pConnection->lScale >> 16));
                            break;
                        }
                        break;
                    case CONN_SRC_EG2 :
                        switch (pConnection->usDestination)
                        {
                        case CONN_DST_PITCH :
                            m_PitchEG.m_sScale = (short)
                                ((long) (pConnection->lScale >> 16));
                            break;

                        /* DLS2 */
                        case CONN_DST_FILTER_CUTOFF:
                            m_PitchEG.m_prCutoffScale = (PRELS)
                                ((long) (pConnection->lScale >> 16));
                            break;
                        }
                        break;

                    /* DLS2 */
                    case CONN_SRC_VIBRATO :
                        switch (pConnection->usControl)
                        {
                        case CONN_SRC_NONE :
                            switch (pConnection->usDestination)
                            {
                            case CONN_DST_PITCH :
                                m_LFO2.m_prPitchScale = (PRELS)
                                    ((long) (pConnection->lScale >> 16));
                                break;
                            }
                            break;
                        case CONN_SRC_CC1 :
                            switch (pConnection->usDestination)
                            {
                            case CONN_DST_PITCH :
                                m_LFO2.m_prMWPitchScale = (PRELS)
                                    ((long) (pConnection->lScale >> 16));
                                break;
                            }
                            break;
                        case CONN_SRC_CHANNELPRESSURE :
                            switch (pConnection->usDestination)
                            {
                            case CONN_DST_PITCH :
                                m_LFO2.m_prCPPitchScale  = (PRELS)
                                    ((long) (pConnection->lScale >> 16));
                                break;
                            }
                            break;
                        }
                        break;
                    }
                }
            }
            if (pdmArtic->ulNextArtIdx)
            {
                if (pdmArtic->ulNextArtIdx >= pInfo->dwNumOffsetTableEntries)
                {
                    Trace(1,"Error: Download failed because articulation chunk has an error.\n");
                    return DMUS_E_BADARTICULATION;
                }
                pdmArtic = (DMUS_ARTICULATION2 *) pvOffsetTable[pdmArtic->ulNextArtIdx];
            }
            else
            {
                pdmArtic = NULL;
            }
        }
    }
    else
    {
        DMUS_ARTICULATION * pdmArtic =
            (DMUS_ARTICULATION *) pvOffsetTable[dwIndex];

        if (pdmArtic->ulArt1Idx)
        {
            if (pdmArtic->ulArt1Idx >= pInfo->dwNumOffsetTableEntries)
            {
                Trace(1,"Error: Download failed because articulation chunk has an error.\n");
                return DMUS_E_BADARTICULATION;
            }
            DMUS_ARTICPARAMS * pdmArticParams =
                (DMUS_ARTICPARAMS *) pvOffsetTable[pdmArtic->ulArt1Idx];

            m_LFO.m_pfFrequency = PitchCents2PitchFract(
                pdmArticParams->LFO.pcFrequency,dwSampleRate);
            m_LFO.m_stDelay = TimeCents2Samples(
                (TCENT) pdmArticParams->LFO.tcDelay,dwSampleRate);
            m_LFO.m_vrVolumeScale = (VRELS)
                ((long) ((pdmArticParams->LFO.gcVolumeScale * 10) >> 16));
            m_LFO.m_prPitchScale = (PRELS)
                ((long) (pdmArticParams->LFO.pcPitchScale >> 16));
            m_LFO.m_vrMWVolumeScale = (VRELS)
                ((long) ((pdmArticParams->LFO.gcMWToVolume * 10) >> 16));
            m_LFO.m_prMWPitchScale = (PRELS)
                ((long) (pdmArticParams->LFO.pcMWToPitch >> 16));

            m_VolumeEG.m_stAttack = TimeCents2Samples(
                (TCENT) pdmArticParams->VolEG.tcAttack,dwSampleRate);
            m_VolumeEG.m_stDecay = TimeCents2Samples(
                (TCENT) pdmArticParams->VolEG.tcDecay,dwSampleRate);
            m_VolumeEG.m_pcSustain =
                (SPERCENT) ((long) (pdmArticParams->VolEG.ptSustain >> 16));
            m_VolumeEG.m_stRelease = TimeCents2Samples(
                (TCENT) pdmArticParams->VolEG.tcRelease,dwSampleRate);
            m_VolumeEG.m_trVelAttackScale = (TRELS)
                ((long) (pdmArticParams->VolEG.tcVel2Attack >> 16));
            m_VolumeEG.m_trKeyDecayScale = (TRELS)
                ((long) (pdmArticParams->VolEG.tcKey2Decay >> 16));

            m_PitchEG.m_trKeyDecayScale = (TRELS)
                ((long) (pdmArticParams->PitchEG.tcKey2Decay >> 16));
            m_PitchEG.m_sScale = (short)
                ((long) (pdmArticParams->PitchEG.pcRange >> 16));
            m_PitchEG.m_trVelAttackScale = (TRELS)
                ((long) (pdmArticParams->PitchEG.tcVel2Attack >> 16));
            m_PitchEG.m_stAttack = TimeCents2Samples(
                (TCENT) pdmArticParams->PitchEG.tcAttack,dwSampleRate);
            m_PitchEG.m_stDecay = TimeCents2Samples(
                (TCENT) pdmArticParams->PitchEG.tcDecay,dwSampleRate);
            m_PitchEG.m_pcSustain =
                (SPERCENT) ((long) (pdmArticParams->PitchEG.ptSustain >> 16));
            m_PitchEG.m_stRelease = TimeCents2Samples(
                (TCENT) pdmArticParams->PitchEG.tcRelease,dwSampleRate);

            m_sDefaultPan = (short)
                ((long) ((long) pdmArticParams->Misc.ptDefaultPan >> 12) / 125);
        }
    }
    Verify();   // Make sure all parameters are legal.

    return S_OK;
}

HRESULT CSourceRegion::Download(DMUS_DOWNLOADINFO * pInfo,
                                void * pvOffsetTable[],
                                DWORD *pdwRegionIX,
                                DWORD dwSampleRate,
                                BOOL fNewFormat)
{
    DMUS_REGION * pdmRegion = (DMUS_REGION *) pvOffsetTable[*pdwRegionIX];
    *pdwRegionIX = pdmRegion->ulNextRegionIdx;  // Clear to avoid loops.
    pdmRegion->ulNextRegionIdx = 0;
    // Read the Region chunk...
    m_bKeyHigh = (BYTE) pdmRegion->RangeKey.usHigh;
    m_bKeyLow  = (BYTE) pdmRegion->RangeKey.usLow;
    m_bVelocityHigh = (BYTE) pdmRegion->RangeVelocity.usHigh;
    m_bVelocityLow  = (BYTE) pdmRegion->RangeVelocity.usLow;

    //
    // Fix DLS Designer bug
    // Designer was putting velocity ranges that fail
    // on DLS2 synths
    //
    if ( m_bVelocityHigh == 0 && m_bVelocityLow == 0 )
        m_bVelocityHigh = 127;

    if (pdmRegion->fusOptions & F_RGN_OPTION_SELFNONEXCLUSIVE)
    {
        m_bAllowOverlap = TRUE;
    }
    else
    {
        m_bAllowOverlap = FALSE;
    }
    m_bGroup = (BYTE) pdmRegion->usKeyGroup;
    // Now, the WSMP and WLOOP chunks...
    m_vrAttenuation = (short) ((long) ((pdmRegion->WSMP.lAttenuation) * 10) >> 16);
    m_Sample.m_prFineTune = pdmRegion->WSMP.sFineTune;
    m_Sample.m_bMIDIRootKey = (BYTE) pdmRegion->WSMP.usUnityNote;
    if (pdmRegion->WSMP.cSampleLoops == 0)
    {
        m_Sample.m_bOneShot = TRUE;
    }
    else
    {
        m_Sample.m_dwLoopStart = pdmRegion->WLOOP[0].ulStart;
        m_Sample.m_dwLoopEnd = m_Sample.m_dwLoopStart + pdmRegion->WLOOP[0].ulLength;
        m_Sample.m_bOneShot = FALSE;
        m_Sample.m_dwLoopType = pdmRegion->WLOOP[0].ulType;
    }
    m_Sample.m_dwSampleRate = dwSampleRate;

    m_sWaveLinkOptions = pdmRegion->WaveLink.fusOptions;
    m_dwChannel = pdmRegion->WaveLink.ulChannel;

    if ( (m_dwChannel != WAVELINK_CHANNEL_LEFT) && !IsMultiChannel() )
    {
        Trace(1, "Download failed: Attempt to use a non-mono channel without setting the multichannel flag.\n");
        return DMUS_E_NOTMONO;
    }

    m_Sample.m_dwID = (DWORD) pdmRegion->WaveLink.ulTableIndex;

    // Does it have its own articulation?
    //
    if (pdmRegion->ulRegionArtIdx )
    {
        if (pdmRegion->ulRegionArtIdx >= pInfo->dwNumOffsetTableEntries)
        {
            Trace(1,"Error: Download failed because articulation chunk has an error.\n");
            return DMUS_E_BADARTICULATION;
        }

        CSourceArticulation *pArticulation = new CSourceArticulation;
        if (pArticulation)
        {
            pArticulation->Init(dwSampleRate);
            HRESULT hr = pArticulation->Download(pInfo, pvOffsetTable,
                    pdmRegion->ulRegionArtIdx, dwSampleRate, fNewFormat);

            if (FAILED(hr))
            {
                delete pArticulation;
                return hr;
            }
            m_pArticulation = pArticulation;
            m_pArticulation->AddRef();
        }
        else
        {
            return E_OUTOFMEMORY;
        }
    }
    return S_OK;
}



HRESULT CInstManager::DownloadInstrument(LPHANDLE phDownload,
                                         DMUS_DOWNLOADINFO *pInfo,
                                         void *pvOffsetTable[],
                                         void *pvData,
                                         BOOL fNewFormat)

{
    DMUS_INSTRUMENT *pdmInstrument = (DMUS_INSTRUMENT *) pvData;
    CInstrument *pInstrument = new CInstrument;
    if (pInstrument)
    {
        Trace(3,"Downloading instrument %lx\n",pdmInstrument->ulPatch);
        pInstrument->m_dwProgram = pdmInstrument->ulPatch;

        DWORD dwRegionIX = pdmInstrument->ulFirstRegionIdx;
        pdmInstrument->ulFirstRegionIdx = 0; // Clear to avoid loops.
        while (dwRegionIX)
        {
            if (dwRegionIX >= pInfo->dwNumOffsetTableEntries)
            {
                Trace(1,"Error: Download failed because instrument has error in region list.\n");
                delete pInstrument;
                return DMUS_E_BADINSTRUMENT;
            }
            CSourceRegion *pRegion = new CSourceRegion;
            if (!pRegion)
            {
                delete pInstrument;
                return E_OUTOFMEMORY;
            }
            pInstrument->m_RegionList.AddHead(pRegion);
            HRESULT hr = pRegion->Download(pInfo, pvOffsetTable, &dwRegionIX, m_dwSampleRate, fNewFormat);
            if (FAILED(hr))
            {
                delete pInstrument;
                return hr;
            }
            EnterCriticalSection(&m_CriticalSection);
            CWave *pWave = m_WavePool[pRegion->m_Sample.m_dwID % WAVE_HASH_SIZE].GetHead();
            for (;pWave;pWave = pWave->GetNext())
            {
                if (pRegion->m_Sample.m_dwID == pWave->m_dwID)
                {
                    pRegion->m_Sample.m_pWave = pWave;
                    pWave->AddRef();
                    pRegion->m_Sample.CopyFromWave();
                    break;
                }
            }
            LeaveCriticalSection(&m_CriticalSection);
        }
        if (pdmInstrument->ulGlobalArtIdx)
        {
            if (pdmInstrument->ulGlobalArtIdx >= pInfo->dwNumOffsetTableEntries)
            {
                Trace(1,"Error: Download failed because of out of range articulation chunk.\n");
                delete pInstrument;
                return DMUS_E_BADARTICULATION;
            }

            CSourceArticulation *pArticulation = new CSourceArticulation;
            if (pArticulation)
            {
                pArticulation->Init(m_dwSampleRate);
                HRESULT hr = pArticulation->Download(pInfo, pvOffsetTable,
                        pdmInstrument->ulGlobalArtIdx, m_dwSampleRate, fNewFormat);
                if (FAILED(hr))
                {
                    delete pInstrument;
                    delete pArticulation;
                    return hr;
                }
                for (CSourceRegion *pr = pInstrument->m_RegionList.GetHead();
                     pr != NULL;
                     pr = pr->GetNext())
                {
                    if (pr->m_pArticulation == NULL)
                    {
                        pr->m_pArticulation = pArticulation;
                        pArticulation->AddRef();
                    }
                }
                if (!pArticulation->m_wUsageCount)
                {
                    delete pArticulation;
                }
            }
            else
            {
                delete pInstrument;
                return E_OUTOFMEMORY;
            }
        }
        else
        {
            for (CSourceRegion *pr = pInstrument->m_RegionList.GetHead();
                 pr != NULL;
                 pr = pr->GetNext())
            {
                if (pr->m_pArticulation == NULL)
                {
                    Trace(1,"Error: Download failed because region has no articulation.\n");
                    delete pInstrument;
                    return DMUS_E_NOARTICULATION;
                }
            }
        }
        EnterCriticalSection(&m_CriticalSection);
        if (pdmInstrument->ulFlags & DMUS_INSTRUMENT_GM_INSTRUMENT)
        {
            pInstrument->SetNext(NULL);
            m_InstrumentList[pInstrument->m_dwProgram % INSTRUMENT_HASH_SIZE].AddTail(pInstrument);
        }
        else
        {
            m_InstrumentList[pInstrument->m_dwProgram % INSTRUMENT_HASH_SIZE].AddHead(pInstrument);
        }
        LeaveCriticalSection(&m_CriticalSection);
        *phDownload = (HANDLE) pInstrument;
        return S_OK;
    }
    return E_OUTOFMEMORY;
}

HRESULT CInstManager::DownloadWave(LPHANDLE phDownload,
                                   DMUS_DOWNLOADINFO *pInfo,
                                   void *pvOffsetTable[],
                                   void *pvData)
{
    DMUS_WAVE *pdmWave = (DMUS_WAVE *) pvData;
    if (pdmWave->WaveformatEx.wFormatTag != WAVE_FORMAT_PCM)
    {
        Trace(1,"Error: Download failed because wave data is not PCM format.\n");
        return DMUS_E_NOTPCM;
    }

    if (pdmWave->WaveformatEx.nChannels != 1)
    {
        Trace(1,"Error: Download failed because wave data is not mono.\n");
        return DMUS_E_NOTMONO;
    }

    if (pdmWave->ulWaveDataIdx >= pInfo->dwNumOffsetTableEntries)
    {
        Trace(1,"Error: Download failed because wave data is at invalid location.\n");
        return DMUS_E_BADWAVE;
    }

    CWave *pWave = new CWave;
    if (pWave)
    {
        DMUS_WAVEDATA *pdmWaveData= (DMUS_WAVEDATA *)
            pvOffsetTable[pdmWave->ulWaveDataIdx];
        pWave->m_dwID = pInfo->dwDLId;
        pWave->m_hUserData = NULL;
        pWave->m_lpFreeHandle = NULL;
        pWave->m_dwSampleLength = pdmWaveData->cbSize;
        pWave->m_pnWave = (short *) &pdmWaveData->byData[0];
        pWave->m_dwSampleRate = pdmWave->WaveformatEx.nSamplesPerSec;

        if (pdmWave->WaveformatEx.wBitsPerSample == 8)
        {
            pWave->m_bSampleType = SFORMAT_8;
            DWORD dwX;
            char *pData = (char *) &pdmWaveData->byData[0];
            for (dwX = 0; dwX < pWave->m_dwSampleLength; dwX++)
            {
                pData[dwX] -= (char) 128;
            }
        }
        else if (pdmWave->WaveformatEx.wBitsPerSample == 16)
        {
            pWave->m_dwSampleLength >>= 1;
            pWave->m_bSampleType = SFORMAT_16;
        }
        else
        {
            Trace(1,"Error: Downloading wave %ld, bad wave format.\n",pInfo->dwDLId);
            delete pWave;
            return DMUS_E_BADWAVE;
        }
        pWave->m_dwSampleLength++;  // We always add one sample to the end for interpolation.
        EnterCriticalSection(&m_CriticalSection);
        m_WavePool[pWave->m_dwID % WAVE_HASH_SIZE].AddHead(pWave);
        LeaveCriticalSection(&m_CriticalSection);
        *phDownload = (HANDLE) pWave;
        pWave->AddRef();

        // Track memory usage
        m_dwSynthMemUse += (pWave->m_bSampleType == SFORMAT_16)?pWave->m_dwSampleLength << 1: pWave->m_dwSampleLength;
        Trace(3,"Downloading wave %ld memory usage %ld\n",pInfo->dwDLId,m_dwSynthMemUse);

        return S_OK;
    }
    return E_OUTOFMEMORY;
}

HRESULT CInstManager::Download(LPHANDLE phDownload,
                               void * pvData,
                               LPBOOL pbFree)


{
    V_INAME(IDirectMusicSynth::Download);
    V_BUFPTR_READ(pvData,sizeof(DMUS_DOWNLOADINFO));

    HRESULT hr = DMUS_E_UNKNOWNDOWNLOAD;
    void ** ppvOffsetTable;     // Array of pointers to chunks in data.
    DMUS_DOWNLOADINFO * pInfo = (DMUS_DOWNLOADINFO *) pvData;
    DMUS_OFFSETTABLE* pOffsetTable = (DMUS_OFFSETTABLE *)(((BYTE*)pvData) + sizeof(DMUS_DOWNLOADINFO));
    char *pcData = (char *) pvData;

    V_BUFPTR_READ(pvData,pInfo->cbSize);

    // Code fails if pInfo->dwNumOffsetTableEntries == 0
    // Sanity check here for debug
    assert(pInfo->dwNumOffsetTableEntries);

    ppvOffsetTable = new void *[pInfo->dwNumOffsetTableEntries];
    if (ppvOffsetTable) // Create the pointer array and validate.
    {
        DWORD dwIndex;
        for (dwIndex = 0; dwIndex < pInfo->dwNumOffsetTableEntries; dwIndex++)
        {
            if (pOffsetTable->ulOffsetTable[dwIndex] >= pInfo->cbSize)
            {
                delete[] ppvOffsetTable;
                Trace(1,"Error: Download failed because of corrupt download tables.\n");
                return DMUS_E_BADOFFSETTABLE;   // Bad!
            }
            ppvOffsetTable[dwIndex] = (void *) &pcData[pOffsetTable->ulOffsetTable[dwIndex]];
        }
        if (pInfo->dwDLType == DMUS_DOWNLOADINFO_INSTRUMENT) // Instrument.
        {
            *pbFree = TRUE;
            hr = DownloadInstrument(phDownload, pInfo, ppvOffsetTable, ppvOffsetTable[0],FALSE);
        }
        else if (pInfo->dwDLType == DMUS_DOWNLOADINFO_INSTRUMENT2) // New instrument format.
        {
            *pbFree = TRUE;
            hr = DownloadInstrument(phDownload, pInfo, ppvOffsetTable, ppvOffsetTable[0],TRUE);
        }
        else if (pInfo->dwDLType == DMUS_DOWNLOADINFO_WAVE) // Wave.
        {
            *pbFree = FALSE;
            hr = DownloadWave(phDownload, pInfo, ppvOffsetTable, ppvOffsetTable[0]);
        }
        else if (pInfo->dwDLType == DMUS_DOWNLOADINFO_WAVEARTICULATION) // Wave onshot & streaming
        {
            *pbFree = TRUE;
            hr = DownloadWaveArticulation(phDownload, pInfo, ppvOffsetTable, ppvOffsetTable[0]);
        }
        else if (pInfo->dwDLType == DMUS_DOWNLOADINFO_STREAMINGWAVE) // Streaming
        {
            *pbFree = FALSE;
            hr = DownloadWaveRaw(phDownload, pInfo, ppvOffsetTable, ppvOffsetTable[0]);
        }
        else if (pInfo->dwDLType == DMUS_DOWNLOADINFO_ONESHOTWAVE) // Wave onshot
        {
            *pbFree = FALSE;
            hr = DownloadWaveRaw(phDownload, pInfo, ppvOffsetTable, ppvOffsetTable[0]);
        }

        delete[] ppvOffsetTable;
    }
    else
    {
        hr = E_OUTOFMEMORY;
    }
    return hr;
}

HRESULT CInstManager::Unload(HANDLE hDownload,
                             HRESULT ( CALLBACK *lpFreeHandle)(HANDLE,HANDLE),
                             HANDLE hUserData)

{
    DWORD dwIndex;
    EnterCriticalSection(&m_CriticalSection);
    for (dwIndex = 0; dwIndex < INSTRUMENT_HASH_SIZE; dwIndex++)
    {
        CInstrument *pInstrument = m_InstrumentList[dwIndex].GetHead();
        for (;pInstrument != NULL; pInstrument = pInstrument->GetNext())
        {
            if (pInstrument == (CInstrument *) hDownload)
            {
                Trace(3,"Unloading instrument %lx\n",pInstrument->m_dwProgram);
                m_InstrumentList[dwIndex].Remove(pInstrument);
                delete pInstrument;
                LeaveCriticalSection(&m_CriticalSection);
                return S_OK;
            }
        }
    }
    for (dwIndex = 0; dwIndex < WAVE_HASH_SIZE; dwIndex++)
    {
        CWave *pWave = m_WavePool[dwIndex].GetHead();
        for (;pWave != NULL;pWave = pWave->GetNext())
        {
            if (pWave == (CWave *) hDownload)
            {
                // Track memory usage
                m_dwSynthMemUse -= (pWave->m_bSampleType == SFORMAT_16)?pWave->m_dwSampleLength << 1: pWave->m_dwSampleLength;

                Trace(3,"Unloading wave %ld memory usage %ld\n",pWave->m_dwID,m_dwSynthMemUse);
                m_WavePool[dwIndex].Remove(pWave);

                pWave->m_hUserData = hUserData;
                pWave->m_lpFreeHandle = lpFreeHandle;
                pWave->Release();
                LeaveCriticalSection(&m_CriticalSection);
                return S_OK;
            }
        }
    }
    for (dwIndex = 0; dwIndex < WAVE_HASH_SIZE; dwIndex++)
    {
        CWaveArt* pWaveArt = m_WaveArtList[dwIndex].GetHead();
        for (;pWaveArt != NULL;pWaveArt = pWaveArt->GetNext())
        {
            if (pWaveArt == (CWaveArt *) hDownload)
            {
                Trace(3,"Unloading wave articulation %ld\n",pWaveArt->m_dwID,m_dwSynthMemUse);
                m_WaveArtList[dwIndex].Remove(pWaveArt);

                pWaveArt->Release();
                LeaveCriticalSection(&m_CriticalSection);
                return S_OK;
            }
        }
    }
    LeaveCriticalSection(&m_CriticalSection);
    Trace(1,"Error: Unload failed - downloaded object not found.\n");
    return E_FAIL;
}

//////////////////////////////////////////////////////////
// Directx8 Methods

CWave * CInstManager::GetWave(DWORD dwDLId)
{
    EnterCriticalSection(&m_CriticalSection);
    CWave *pWave = m_WavePool[dwDLId % WAVE_HASH_SIZE].GetHead();
    for (;pWave;pWave = pWave->GetNext())
    {
        if (dwDLId == pWave->m_dwID)
        {
            break;
        }
    }
    LeaveCriticalSection(&m_CriticalSection);

    return pWave;
}

CWaveArt *  CInstManager::GetWaveArt(DWORD dwDLId)
{
    EnterCriticalSection(&m_CriticalSection);
    CWaveArt *pWaveArt = m_WaveArtList[dwDLId % WAVEART_HASH_SIZE].GetHead();
    for (;pWaveArt;pWaveArt = pWaveArt->GetNext())
    {
        if (dwDLId == pWaveArt->m_dwID)
        {
            break;
        }
    }
    LeaveCriticalSection(&m_CriticalSection);

    return pWaveArt;
}

HRESULT CInstManager::DownloadWaveArticulation(LPHANDLE phDownload,
                                   DMUS_DOWNLOADINFO *pInfo,
                                   void *pvOffsetTable[],
                                   void *pvData)
{
    DMUS_WAVEARTDL* pWaveArtDl  = (DMUS_WAVEARTDL*)pvData;
    WAVEFORMATEX *pWaveformatEx = (WAVEFORMATEX *) pvOffsetTable[1];
    DWORD *dwDlId = (DWORD*)pvOffsetTable[2];
    DWORD i;

    CWaveArt* pWaveArt = new CWaveArt();
    if ( pWaveArt )
    {
        pWaveArt->m_dwID = pInfo->dwDLId;
        pWaveArt->m_WaveArtDl = *pWaveArtDl;;
        pWaveArt->m_WaveformatEx = *pWaveformatEx;
        if (pWaveformatEx->wBitsPerSample == 8)
        {
            pWaveArt->m_bSampleType = SFORMAT_8;
        }
        else if (pWaveformatEx->wBitsPerSample == 16)
        {
            pWaveArt->m_bSampleType = SFORMAT_16;
        }
        else
        {
            Trace(1,"Error: Download failed because wave data is %ld bits instead of 8 or 16.\n",(long) pWaveformatEx->wBitsPerSample);
            delete pWaveArt;
            return DMUS_E_BADWAVE;
        }

        for ( i = 0; i < pWaveArtDl->ulBuffers; i++ )
        {
            // Get wave buffer and fill header with waveformat data
            CWave *pWave = GetWave(dwDlId[i]);
            assert(pWave);

            if (!pWave)
            {
                delete pWaveArt;
                return E_POINTER;
            }

            pWave->m_dwSampleRate = pWaveformatEx->nSamplesPerSec;

            if (pWaveformatEx->wBitsPerSample == 8)
            {
                DWORD dwX;
                char *pData = (char *) pWave->m_pnWave;
                for (dwX = 0; dwX < pWave->m_dwSampleLength; dwX++)
                {
                    pData[dwX] -= (char) 128;
                }
                pWave->m_bSampleType = SFORMAT_8;
            }
            else if (pWaveformatEx->wBitsPerSample == 16)
            {
                pWave->m_dwSampleLength >>= 1;
                pWave->m_bSampleType = SFORMAT_16;
            }
            else
            {
                Trace(1,"Error: Download failed because wave data is %ld bits instead of 8 or 16.\n",(long) pWaveformatEx->wBitsPerSample);
                delete pWaveArt;
                return DMUS_E_BADWAVE;
            }
            pWave->m_dwSampleLength++;  // We always add one sample to the end for interpolation.

            // Default is to duplicate last sample. This will be overrwritten for
            // streaming waves.
            //
            if (pWave->m_dwSampleLength > 1)
            {
                if (pWave->m_bSampleType == SFORMAT_8)
                {
                    char* pb = (char*)pWave->m_pnWave;
                    pb[pWave->m_dwSampleLength - 1] = pb[pWave->m_dwSampleLength - 2];
                }
                else
                {
                    short *pn = pWave->m_pnWave;
                    pn[pWave->m_dwSampleLength - 1] = pn[pWave->m_dwSampleLength - 2];
                }
            }

            // Create a WaveBuffer listitem and save the wave in and add it to the circular buffer list
            CWaveBuffer* pWavBuf = new CWaveBuffer();
            if ( pWavBuf == NULL )
            {
                delete pWaveArt;
                return E_OUTOFMEMORY;
            }
            pWavBuf->m_pWave = pWave;

            // This Articulation will be handling streaming data
            if ( pWave->m_bStream )
                pWaveArt->m_bStream = TRUE;

            pWaveArt->m_pWaves.AddTail(pWavBuf);
        }

        EnterCriticalSection(&m_CriticalSection);
        if (pWaveArt)
        {
            CWaveBuffer* pCurrentBuffer = pWaveArt->m_pWaves.GetHead();
            for (; pCurrentBuffer; pCurrentBuffer = pCurrentBuffer->GetNext() )
            {
                if (pCurrentBuffer->m_pWave)
                {
                    pCurrentBuffer->m_pWave->AddRef();
                }
            }
        }
        m_WaveArtList[pWaveArt->m_dwID % WAVEART_HASH_SIZE].AddHead(pWaveArt);
        LeaveCriticalSection(&m_CriticalSection);

        *phDownload = (HANDLE) pWaveArt;

        return S_OK;
    }
    return E_OUTOFMEMORY;
}

HRESULT CInstManager::DownloadWaveRaw(LPHANDLE phDownload,
                                   DMUS_DOWNLOADINFO *pInfo,
                                   void *pvOffsetTable[],
                                   void *pvData)
{
    CWave *pWave = new CWave;
    if (pWave)
    {
        DMUS_WAVEDATA *pdmWaveData= (DMUS_WAVEDATA *)pvData;
        Trace(3,"Downloading raw wave data%ld\n",pInfo->dwDLId);

        pWave->m_dwID = pInfo->dwDLId;
        pWave->m_hUserData = NULL;
        pWave->m_lpFreeHandle = NULL;
        pWave->m_dwSampleLength = pdmWaveData->cbSize;
        pWave->m_pnWave = (short *) &pdmWaveData->byData[0];

        if ( pInfo->dwDLType == DMUS_DOWNLOADINFO_STREAMINGWAVE )
        {
            pWave->m_bStream = TRUE;
            pWave->m_bValid = TRUE;
        }

        EnterCriticalSection(&m_CriticalSection);
        m_WavePool[pWave->m_dwID % WAVE_HASH_SIZE].AddHead(pWave);
        LeaveCriticalSection(&m_CriticalSection);

        *phDownload = (HANDLE) pWave;
        pWave->AddRef();

        m_dwSynthMemUse += pWave->m_dwSampleLength;

        return S_OK;
    }
    return E_OUTOFMEMORY;
}

