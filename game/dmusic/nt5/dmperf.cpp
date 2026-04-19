// Copyright (c) 1998-2001 Microsoft Corporation
// dmperf.cpp

#include <windows.h>
#include <mmsystem.h>
#include <time.h>       // To seed random number generator
#include <dsoundp.h>
#include "debug.h"
#define ASSERT assert
#include "dmperf.h"
#include "dmime.h"
#include "dmgraph.h"
#include "dmsegobj.h"
#include "song.h"
#include "curve.h"
#include "math.h"
#include "..\shared\Validate.h"
#include "..\dmstyle\dmstylep.h"
#include <ks.h>
#include "dmksctrl.h"
#include <dsound.h>
#include "dmscriptautguids.h"
#include "..\shared\dmusiccp.h"
#include "wavtrack.h"
#include "tempotrk.h"
#include <strsafe.h>

#pragma warning(disable:4296)

#define PORT_CHANNEL 0

// @doc EXTERNAL
#define MIDI_NOTEOFF        0x80
#define MIDI_NOTEON         0x90
#define MIDI_PTOUCH         0xA0
#define MIDI_CCHANGE        0xB0
#define MIDI_PCHANGE        0xC0
#define MIDI_MTOUCH         0xD0
#define MIDI_PBEND          0xE0
#define MIDI_SYSX           0xF0
#define MIDI_MTC            0xF1
#define MIDI_SONGPP         0xF2
#define MIDI_SONGS          0xF3
#define MIDI_EOX            0xF7
#define MIDI_CLOCK          0xF8
#define MIDI_START          0xFA
#define MIDI_CONTINUE       0xFB
#define MIDI_STOP           0xFC
#define MIDI_SENSE          0xFE
#define MIDI_CC_BS_MSB      0x00
#define MIDI_CC_BS_LSB      0x20
#define MIDI_CC_DATAENTRYMSB 0x06
#define MIDI_CC_DATAENTRYLSB 0x26
#define MIDI_CC_NRPN_LSB    0x62
#define MIDI_CC_NRPN_MSB    0x63
#define MIDI_CC_RPN_LSB     0x64
#define MIDI_CC_RPN_MSB     0x65
#define MIDI_CC_MOD_WHEEL   0x01
#define MIDI_CC_VOLUME      0x07
#define MIDI_CC_PAN         0x0A
#define MIDI_CC_EXPRESSION  0x0B
#define MIDI_CC_FILTER      0x4A
#define MIDI_CC_REVERB      0x5B
#define MIDI_CC_CHORUS      0x5D
#define MIDI_CC_RESETALL    0x79
#define MIDI_CC_ALLSOUNDSOFF 0x78

#define CLEARTOOLGRAPH(x)   { \
    if( (x)->pTool ) \
    { \
        (x)->pTool->Release(); \
        (x)->pTool = NULL; \
    } \
    if( (x)->pGraph ) \
    { \
        (x)->pGraph->Release(); \
        (x)->pGraph = NULL; }}

#define GetLatencyWithPrePlay() ( GetLatency() + m_rtBumperLength )

void CChannelBlockList::Clear()
{
    CChannelBlock* pCB;
    while( pCB = RemoveHead() )
    {
        delete pCB;
    }
}

void CChannelMap::Clear()

{
    Reset(TRUE);                // Clear all MIDI controllers
    m_TransposeMerger.Clear(0); // No transpose.
    nTranspose = 0;
    wFlags = CMAP_FREE;
}

void CChannelMap::Reset(BOOL fVolumeAndPanToo)

{
    if (fVolumeAndPanToo)
    {
        m_PanMerger.Clear(0);       // Panned to center.
        m_VolumeMerger.Clear(-415); // Equivalent to MIDI value 100.
    }
    m_PitchbendMerger.Clear(0); // No pitch bend.
    m_ExpressionMerger.Clear(0);// Full volume for expression (MIDI 127.)
    m_FilterMerger.Clear(0);    // No filter change.
    m_ReverbMerger.Clear(-87); // Start at default level (MIDI 40).
    m_ChorusMerger.Clear(-127);    // Start with no chorus.
    m_ModWheelMerger.Clear(-127);  // Start with no mod wheel.
}

void CParamMerger::Clear(long lInitValue )

{
    CMergeParam *pParam;
    while (pParam = RemoveHead())
    {
        delete pParam;
    }
    m_lZeroIndexData = lInitValue;
    m_lMergeTotal = 0;
}


long CParamMerger::m_lMIDIToDB[128] = {       // Global array used to convert MIDI to dB.
    -9600, -8415, -7211, -6506, -6006, -5619, -5302, -5034,
    -4802, -4598, -4415, -4249, -4098, -3959, -3830, -3710,
    -3598, -3493, -3394, -3300, -3211, -3126, -3045, -2968,
    -2894, -2823, -2755, -2689, -2626, -2565, -2506, -2449,
    -2394, -2341, -2289, -2238, -2190, -2142, -2096, -2050,
    -2006, -1964, -1922, -1881, -1841, -1802, -1764, -1726,
    -1690, -1654, -1619, -1584, -1551, -1518, -1485, -1453,
    -1422, -1391, -1361, -1331, -1302, -1273, -1245, -1217,
    -1190, -1163, -1137, -1110, -1085, -1059, -1034, -1010,
    -985, -961, -938, -914, -891, -869, -846, -824,
    -802, -781, -759, -738, -718, -697, -677, -657,
    -637, -617, -598, -579, -560, -541, -522, -504,
    -486, -468, -450, -432, -415, -397, -380, -363,
    -347, -330, -313, -297, -281, -265, -249, -233,
    -218, -202, -187, -172, -157, -142, -127, -113,
    -98, -84, -69, -55, -41, -27, -13, 0
};


long CParamMerger::m_lDBToMIDI[97] = {        // Global array used to convert db to MIDI.
    127, 119, 113, 106, 100, 95, 89, 84, 80, 75,
    71, 67, 63, 60, 56, 53, 50, 47, 45, 42,
    40, 37, 35, 33, 31, 30, 28, 26, 25, 23,
    22, 21, 20, 19, 17, 16, 15, 15, 14, 13,
    12, 11, 11, 10, 10, 9, 8, 8, 8, 7,
    7, 6, 6, 6, 5, 5, 5, 4, 4, 4,
    4, 3, 3, 3, 3, 3, 2, 2, 2, 2,
    2, 2, 2, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0
};


CParamMerger::CParamMerger()
{
    m_lMergeTotal = 0;
    m_lZeroIndexData = 0;
}

BYTE CParamMerger::VolumeToMidi(long lVolume)

{
    if (lVolume < -9600) lVolume = -9600;
    if (lVolume > 0) lVolume = 0;
    lVolume = -lVolume;
    long lFraction = lVolume % 100;
    lVolume = lVolume / 100;
    long lResult = m_lDBToMIDI[lVolume];
    lResult += ((m_lDBToMIDI[lVolume + 1] - lResult) * lFraction) / 100;
    return (BYTE) lResult;
}

/*  MergeMidiVolume() takes an incoming volume and updates the matching
    MergeParam structure (determined by index.) If there is no such matching
    structure, it creates one. Also, the volumes are totaled to create a new
    total volume, which is converted back to MIDI volume and returned.
    This mechanism allows us to introduce additional volume controllers
    that are summed.
*/

BYTE CParamMerger::MergeMidiVolume(DWORD dwIndex, BYTE bMIDIVolume)

{
    long lVolume = MergeData(dwIndex,m_lMIDIToDB[bMIDIVolume]);
    if (m_lMergeTotal || dwIndex) // Optimization for simplest and most frequent case - there are no additional indexes.
    {
        return (BYTE) VolumeToMidi(lVolume);
    }
    return bMIDIVolume;
}

BYTE CParamMerger::GetVolumeStart(DWORD dwIndex)

{
    if (dwIndex == 0)
    {
        return VolumeToMidi(m_lZeroIndexData);
    }
    return VolumeToMidi(GetIndexedValue(dwIndex));
}

/*  MergeValue is used for all data types that have a plus and minus range
    around a center bias. These include pitch bend, pan and filter.
    MergeValue takes an incoming data value, adds the bias (in lRange),
    calls MergeData to combine it with the other merged inputs,
    adds the bias back in and checks for over or underflow.
*/

long CParamMerger::MergeValue(DWORD dwIndex, long lData, long lCenter, long lRange)

{
    lData = MergeData(dwIndex,lData - lCenter) + lCenter;
    if (lData < 0) lData = 0;
    if (lData > lRange) lData = lRange;
    return lData;
}


short CParamMerger::MergeTranspose(DWORD dwIndex, short nTranspose)

{
    return (short) MergeData(dwIndex,nTranspose);
}

long CParamMerger::MergeData(DWORD dwIndex, long lData)

{
    if (dwIndex)
    {
        // If this has an index, scan the indexes. Look
        // for the matching index. If it is found, update it
        // with the new data. Meanwhile, add up all the data fields.
        // If it is not found, add an entry for it.
        m_lMergeTotal = 0;   // Recalculate
        BOOL fNoEntry = TRUE;
        CMergeParam *pParam = GetHead();
        for (;pParam;pParam = pParam->GetNext())
        {
            if (pParam->m_dwIndex == dwIndex)
            {
                // Found the index. Store the new value.
                pParam->m_lData = lData;
                fNoEntry = FALSE;
            }
            // Sum all values to create the merged total.
            m_lMergeTotal += pParam->m_lData;
        }
        if (fNoEntry)
        {
            // Didn't find the index. Create one and store the value.
            pParam = new CMergeParam;
            if (pParam)
            {
                pParam->m_dwIndex = dwIndex;
                pParam->m_lData = lData;
                m_lMergeTotal += lData;
                AddHead(pParam);
            }
        }
        // Add the initial value for merge index 0.
        lData = m_lMergeTotal + m_lZeroIndexData;
    }
    else
    {
        m_lZeroIndexData = lData;
        lData += m_lMergeTotal;
    }
    return lData;
}


long CParamMerger::GetIndexedValue(DWORD dwIndex)

{
    if (dwIndex)
    {
        // If this has an index, scan the indexes. Look
        // for the matching index. If it is found, return its data.
        // If not, return the default 0.
        BOOL fNoEntry = TRUE;
        CMergeParam *pParam = GetHead();
        for (;pParam;pParam = pParam->GetNext())
        {
            if (pParam->m_dwIndex == dwIndex)
            {
                return pParam->m_lData;
            }
        }
        return 0;
    }
    return m_lZeroIndexData;
}

void CChannelBlock::Init(DWORD dwPChannelStart,
                         DWORD dwPortIndex, DWORD dwGroup,
                         WORD wFlags)

{
    DWORD dwIndex;
    m_dwPortIndex = dwPortIndex;
    m_dwPChannelStart = ( dwPChannelStart / PCHANNEL_BLOCKSIZE ) * PCHANNEL_BLOCKSIZE;
    for( dwIndex = 0; dwIndex < PCHANNEL_BLOCKSIZE; dwIndex++ )
    {
        m_aChannelMap[dwIndex].Clear();
        m_aChannelMap[dwIndex].dwPortIndex = dwPortIndex;
        m_aChannelMap[dwIndex].dwGroup = dwGroup;
        m_aChannelMap[dwIndex].dwMChannel = dwIndex;
        m_aChannelMap[dwIndex].nTranspose = 0;
        m_aChannelMap[dwIndex].wFlags = wFlags;
    }
    if (wFlags == CMAP_FREE) m_dwFreeChannels = 16;
    else m_dwFreeChannels = 0;
}


/////////////////////////////////////////////////////////////////////////////
// CPerformance

// Flags for which critical sections have been initialized
//

#define PERF_ICS_SEGMENT        0x0001
#define PERF_ICS_PIPELINE       0x0002
#define PERF_ICS_PCHANNEL       0x0004
#define PERF_ICS_GLOBAL         0x0010
#define PERF_ICS_REALTIME       0x0020
#define PERF_ICS_PORTTABLE      0x0040
#define PERF_ICS_MAIN           0x0100
#define PERF_ICS_PMSGCACHE      0x0200

CPerformance::CPerformance()
{
    m_pGraph = NULL;
    m_dwPrepareTime = 1000;
    m_dwBumperLength = 50;
    m_rtBumperLength = m_dwBumperLength * REF_PER_MIL;
    m_pGlobalData = NULL;
    m_fInTrackPlay = FALSE;
    m_fPlaying = FALSE;
    m_wRollOverCount = 0;
    m_mtTransported = 0;
    m_mtTempoCursor = 0;
    m_pParamHook = NULL;
    m_hNotification = 0;
    m_rtNotificationDiscard = 20000000;
    m_rtStart = 0;
    m_rtAdjust = 0;
    m_mtPlayTo = 0;
    m_cRef = 1;
    m_pUnkDispatch = NULL;
    m_dwVersion = 6;
    m_dwNumPorts = 0;
    m_pPortTable = NULL;
    m_fKillThread = 0;
    m_fKillRealtimeThread = 0;
    m_fInTransportThread = 0;
    m_dwTransportThreadID = 0;
    m_pDirectMusic = NULL;
    m_pDirectSound = NULL;
    m_pClock = NULL;
    m_fReleasedInTransport = false;
    m_fReleasedInRealtime = false;
    InterlockedIncrement(&g_cComponent);

    TraceI(3,"CPerformance %lx\n", this);
    m_dwInitCS = 0;

    InitializeCriticalSection(&m_SegmentCrSec);         m_dwInitCS |= PERF_ICS_SEGMENT;
    InitializeCriticalSection(&m_PipelineCrSec);        m_dwInitCS |= PERF_ICS_PIPELINE;
    InitializeCriticalSection(&m_PChannelInfoCrSec);    m_dwInitCS |= PERF_ICS_PCHANNEL;
    InitializeCriticalSection(&m_GlobalDataCrSec);      m_dwInitCS |= PERF_ICS_GLOBAL;
    InitializeCriticalSection(&m_RealtimeCrSec);        m_dwInitCS |= PERF_ICS_REALTIME;
    InitializeCriticalSection(&m_PMsgCacheCrSec);       m_dwInitCS |= PERF_ICS_PMSGCACHE;
    InitializeCriticalSection(&m_MainCrSec);            m_dwInitCS |= PERF_ICS_MAIN;
    memset( m_apPMsgCache, 0, sizeof(DMUS_PMSG*) * (PERF_PMSG_CB_MAX - PERF_PMSG_CB_MIN) );
    DWORD dwCount;
    for (dwCount = 0; dwCount < SQ_COUNT; dwCount++)
    {
        m_SegStateQueues[dwCount].SetID(dwCount);
    }
    Init();
}

void CPerformance::Init()

{
    m_rtEarliestStartTime = 0;
    m_lMasterVolume = 0;
    if (m_dwVersion >= 8)
    {
        m_rtQueuePosition = 0;
        m_dwPrepareTime = 1000;
        m_dwBumperLength = 50;
        m_rtBumperLength = m_dwBumperLength * REF_PER_MIL;
        if (m_dwAudioPathMode)
        {
            CloseDown();
        }
    }
    m_pDefaultAudioPath = NULL;
    m_fltRelTempo = 1;
    m_pGetParamSegmentState = NULL;
    m_dwGetParamFlags = 0;
    m_rtHighestPackedNoteOn = 0;
    m_dwAudioPathMode = 0;
    m_hTransport = 0;
    m_hTransportThread = 0;
    m_dwRealtimeThreadID = 0;
    m_hRealtime = 0;
    m_hRealtimeThread = 0;
    m_fMusicStopped = TRUE;
    BOOL fAuto = FALSE;
    SetGlobalParam(GUID_PerfAutoDownload,&fAuto,sizeof(BOOL));
    DMUS_TIMESIG_PMSG* pTimeSig;
    if (SUCCEEDED(AllocPMsg(sizeof(DMUS_TIMESIG_PMSG),(DMUS_PMSG **) &pTimeSig)))
    {
        pTimeSig->wGridsPerBeat = 4;
        pTimeSig->bBeatsPerMeasure = 4;
        pTimeSig->bBeat = 4;
        pTimeSig->dwFlags = DMUS_PMSGF_REFTIME;
        pTimeSig->dwType = DMUS_PMSGT_TIMESIG;
        EnterCriticalSection(&m_PipelineCrSec);
        m_TimeSigQueue.Enqueue(  DMUS_TO_PRIV(pTimeSig) );
        LeaveCriticalSection(&m_PipelineCrSec);
    }
}

CPerformance::~CPerformance()
{
    TraceI(3,"~CPerformance %lx\n", this);
    if (m_pParamHook)
    {
        m_pParamHook->Release();
    }
    CloseDown(); // this should have already been called, but just in case...
    if (m_pUnkDispatch)
        m_pUnkDispatch->Release(); // free IDispatch implementation we may have borrowed

    if (m_dwInitCS & PERF_ICS_SEGMENT)  DeleteCriticalSection(&m_SegmentCrSec);
    if (m_dwInitCS & PERF_ICS_PIPELINE) DeleteCriticalSection(&m_PipelineCrSec);
    if (m_dwInitCS & PERF_ICS_PCHANNEL) DeleteCriticalSection(&m_PChannelInfoCrSec);
    if (m_dwInitCS & PERF_ICS_GLOBAL)   DeleteCriticalSection(&m_GlobalDataCrSec);
    if (m_dwInitCS & PERF_ICS_REALTIME) DeleteCriticalSection(&m_RealtimeCrSec);
    if (m_dwInitCS & PERF_ICS_PMSGCACHE)DeleteCriticalSection(&m_PMsgCacheCrSec);
    if (m_dwInitCS & PERF_ICS_MAIN)     DeleteCriticalSection(&m_MainCrSec);

    InterlockedDecrement(&g_cComponent);
}

STDMETHODIMP CPerformance::CloseDown(void)
{
    V_INAME(CPerformance::CloseDown);
    DWORD dwThreadID = GetCurrentThreadId();
    if( m_dwAudioPathMode )
    {
        // kill the transport thread
        m_fKillThread = 1;
        m_fKillRealtimeThread = 1;
        if (dwThreadID != m_dwTransportThreadID)
        {
            // signal the transport thread so we don't have to wait for it to wake up on its own
            if( m_hTransport ) SetEvent( m_hTransport );
            // wait until the transport thread quits
            WaitForSingleObject(m_hTransportThread, INFINITE);
        }
        if (dwThreadID != m_dwRealtimeThreadID)
        {
            // signal the realtime thread so we don't have to wait for it to wake up on its own
            if( m_hRealtime ) SetEvent( m_hRealtime );
            // wait until the realtime thread quits
            WaitForSingleObject(m_hRealtimeThread, INFINITE);
        }
    }

    if (m_pGraph) SetGraph(NULL); // shut down the graph and release it (needs to happen before clearing audio path)

    EnterCriticalSection(&m_SegmentCrSec);
    EnterCriticalSection(&m_RealtimeCrSec);

    m_fPlaying = FALSE; // prevents transport thread from doing anything more
    IDirectMusicPerformance* pPerf = NULL;
    if (SUCCEEDED(QueryInterface(IID_IDirectMusicPerformance, (void**)&pPerf)))
    {
        CWavTrack::UnloadAllWaves(pPerf);
        pPerf->Release();
    }
    DequeueAllSegments();
    if (m_pDefaultAudioPath)
    {
        m_pDefaultAudioPath->Release();
        m_pDefaultAudioPath = NULL;
    }
    m_dwAudioPathMode = 0;
    m_AudioPathList.Clear();
    CNotificationItem* pItem = m_NotificationList.GetHead();
    while( pItem )
    {
        CNotificationItem* pNext = pItem->GetNext();
        m_NotificationList.Remove( pItem );
        delete pItem;
        pItem = pNext;
    }
    LeaveCriticalSection(&m_RealtimeCrSec);
    LeaveCriticalSection(&m_SegmentCrSec);

    EnterCriticalSection(&m_PipelineCrSec);
    PRIV_PMSG* pPMsg;
    while( pPMsg = m_EarlyQueue.Dequeue() )
    {
        FreePMsg(pPMsg);
    }
    while( pPMsg = m_NearTimeQueue.Dequeue() )
    {
        FreePMsg(pPMsg);
    }
    while( pPMsg = m_OnTimeQueue.Dequeue() )
    {
        FreePMsg(pPMsg);
    }
    while( pPMsg = m_TempoMap.Dequeue() )
    {
        FreePMsg(pPMsg);
    }
    while( pPMsg = m_OldTempoMap.Dequeue() )
    {
        FreePMsg(pPMsg);
    }
    while( pPMsg = m_NotificationQueue.Dequeue() )
    {
        FreePMsg(pPMsg);
    }
    while( pPMsg = m_TimeSigQueue.Dequeue() )
    {
        FreePMsg(pPMsg);
    }

    LeaveCriticalSection(&m_PipelineCrSec);

    EnterCriticalSection(&m_GlobalDataCrSec);
    GlobalData* pGD = m_pGlobalData;
    while( pGD )
    {
        m_pGlobalData = pGD->pNext;
        delete pGD;
        pGD = m_pGlobalData;
    }
    LeaveCriticalSection(&m_GlobalDataCrSec);

    EnterCriticalSection(&m_PChannelInfoCrSec);
    // clear out ports, buffers, and pchannel maps
    if( m_pPortTable )
    {
        DWORD dwIndex;
        for( dwIndex = 0; dwIndex < m_dwNumPorts; dwIndex++ )
        {
            if( m_pPortTable[dwIndex].pPort )
            {
                m_pPortTable[dwIndex].pPort->Release();
            }
            if( m_pPortTable[dwIndex].pBuffer )
            {
                m_pPortTable[dwIndex].pBuffer->Release();
            }
            if( m_pPortTable[dwIndex].pLatencyClock )
            {
                m_pPortTable[dwIndex].pLatencyClock->Release();
            }
        }
        delete [] m_pPortTable;
        m_pPortTable = NULL;
        m_dwNumPorts = 0;
    }
    m_ChannelBlockList.Clear();
    LeaveCriticalSection(&m_PChannelInfoCrSec);
    EnterCriticalSection(&m_MainCrSec);
    if( m_pClock )
    {
        m_pClock->Release();
        m_pClock = NULL;
    }
    m_BufferManager.Clear();
    if( m_pDirectMusic )
    {
        m_pDirectMusic->Release();
        m_pDirectMusic = NULL;
    }
    if (m_pDirectSound)
    {
        m_pDirectSound->Release();
        m_pDirectSound = NULL;
    }
    m_hNotification = NULL;
    LeaveCriticalSection(&m_MainCrSec);

    EnterCriticalSection(&m_PMsgCacheCrSec);
    for( int i = 0; i < (PERF_PMSG_CB_MAX - PERF_PMSG_CB_MIN); i++ )
    {
        while( m_apPMsgCache[i] )
        {
            PRIV_PMSG* pPriv = m_apPMsgCache[i];
            m_apPMsgCache[i] = pPriv->pNext;
            delete [] pPriv;
        }
    }
    LeaveCriticalSection(&m_PMsgCacheCrSec);

    DWORD dwExitCode = 0;
    if (m_hTransportThread)
    {
        CloseHandle( m_hTransportThread );
        m_hTransportThread = 0;
    }
    if( m_hTransport )
    {
        CloseHandle( m_hTransport );
        m_hTransport = 0;
    }
    if (m_hRealtimeThread)
    {
        CloseHandle( m_hRealtimeThread );
        m_hRealtimeThread = 0;
    }
    if( m_hRealtime )
    {
        CloseHandle( m_hRealtime );
        m_hRealtime = 0;
    }
    m_mtPlayTo = 0;
    return S_OK;
}

// @method:(INTERNAL) HRESULT | IDirectMusicPerformance | QueryInterface | Standard QueryInterface implementation for <i IDirectMusicPerformance>
//
// @rdesc Returns one of the following:
//
// @flag S_OK | If the interface is supported and was returned
// @flag E_NOINTERFACE | If the object does not support the given interface.
// @flag E_POINTER | <p ppv> is NULL or invalid.
//
STDMETHODIMP CPerformance::QueryInterface(
    const IID &iid,   // @parm Interface to query for
    void **ppv)       // @parm The requested interface will be returned here
{
    V_INAME(CPerformance::QueryInterface);
    V_PTRPTR_WRITE(ppv);
    V_REFGUID(iid);

    *ppv = NULL;
    if (iid == IID_IUnknown || iid == IID_IDirectMusicPerformance)
    {
        *ppv = static_cast<IDirectMusicPerformance*>(this);
    } else
    if (iid == IID_IDirectMusicPerformance8)
    {
        m_dwVersion = 8;
        *ppv = static_cast<IDirectMusicPerformance8*>(this);
    } else
    if (iid == IID_IDirectMusicPerformance2)
    {
        m_dwVersion = 7;
        *ppv = static_cast<IDirectMusicPerformance*>(this);
    } else
    if( iid == IID_IDirectMusicPerformanceStats )
    {
        *ppv = static_cast<IDirectMusicPerformanceStats*>(this);
    } else
    if( iid == IID_IDirectMusicSetParamHook )
    {
        *ppv = static_cast<IDirectMusicSetParamHook*>(this);
    } else
    if (iid == IID_IDirectMusicTool)
    {
        *ppv = static_cast<IDirectMusicTool*>(this);
    } else
    if (iid == IID_CPerformance)
    {
        *ppv = static_cast<CPerformance*>(this);
    }
    if (iid == IID_IDirectMusicGraph)
    {
        *ppv = static_cast<IDirectMusicGraph*>(this);
    }
    if (iid == IID_IDirectMusicPerformanceP)
    {
        *ppv = static_cast<IDirectMusicPerformanceP*>(this);
    } else
    if (iid == IID_IDispatch)
    {
        // A helper scripting object implements IDispatch, which we expose from the
        // Performance object via COM aggregation.
        if (!m_pUnkDispatch)
        {
            // Create the helper object
            ::CoCreateInstance(
                CLSID_AutDirectMusicPerformance,
                static_cast<IDirectMusicPerformance*>(this),
                CLSCTX_INPROC_SERVER,
                IID_IUnknown,
                reinterpret_cast<void**>(&m_pUnkDispatch));
        }
        if (m_pUnkDispatch)
        {
            return m_pUnkDispatch->QueryInterface(IID_IDispatch, ppv);
        }
    }
    if (*ppv == NULL)
    {
        Trace(4,"Warning: Request to query unknown interface on Performance object\n");
        return E_NOINTERFACE;
    }
    reinterpret_cast<IUnknown*>(this)->AddRef();
    return S_OK;
}


// @method:(INTERNAL) HRESULT | IDirectMusicPerformance | AddRef | Standard AddRef implementation for <i IDirectMusicPerformance>
//
// @rdesc Returns the new reference count for this object.
//
STDMETHODIMP_(ULONG) CPerformance::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}


// @method:(INTERNAL) HRESULT | IDirectMusicPerformance | Release | Standard Release implementation for <i IDirectMusicPerformance>
//
// @rdesc Returns the new reference count for this object.
//
STDMETHODIMP_(ULONG) CPerformance::Release()
{
    if (!InterlockedDecrement(&m_cRef))
    {
        DWORD dwThreadID = GetCurrentThreadId();
        m_cRef = 100; // artificial reference count to prevent reentrency due to COM aggregation
        if (dwThreadID == m_dwTransportThreadID)
        {
            m_fReleasedInTransport = true;
            m_fKillThread = TRUE;
        }
        else if (dwThreadID == m_dwRealtimeThreadID)
        {
            m_fReleasedInRealtime = true;
            m_fKillRealtimeThread = TRUE;
        }
        else
        {
            delete this;
        }
        return 0;
    }

    return m_cRef;
}

// call this only from within a m_SegmentCrSec critical section
// if fSendNotify, then send segment end notifications for segments that were
// playing
void CPerformance::DequeueAllSegments()
{
    CSegState *pNode;
    DWORD dwCount;

    for( dwCount = 0; dwCount < SQ_COUNT; dwCount++ )
    {
        while( pNode = m_SegStateQueues[dwCount].RemoveHead())
        {
            pNode->ShutDown();
        }
    }
    while( pNode = m_ShutDownQueue.RemoveHead())
    {
        pNode->ShutDown();
    }
}

// IDirectMusicPerformanceStats

STDMETHODIMP CPerformance::TraceAllSegments()
{
    CSegState *pNode;
    DWORD dwCount;
    for( dwCount = 0; dwCount < SQ_COUNT; dwCount++ )
    {
        EnterCriticalSection(&m_SegmentCrSec);
        for ( pNode = m_SegStateQueues[dwCount].GetHead();pNode;pNode=pNode->GetNext())
        {
            TraceI(0,"%x %ld: Playing: %ld, Start: %ld, Seek: %ld, LastPlayed: %ld\n",
                pNode,dwCount,pNode->m_fStartedPlay, pNode->m_mtResolvedStart,
                pNode->m_mtSeek, pNode->m_mtLastPlayed);
        }
        LeaveCriticalSection(&m_SegmentCrSec);
    }
    return S_OK;
}

STDMETHODIMP CPerformance::CreateSegstateList(DMUS_SEGSTATEDATA ** ppList)

{
    if (!ppList) return E_POINTER;
    CSegState *pNode;
    DWORD dwCount;
    for( dwCount = 0; dwCount < SQ_COUNT; dwCount++ )
    {
        EnterCriticalSection(&m_SegmentCrSec);
        for ( pNode = m_SegStateQueues[dwCount].GetHead();pNode;pNode=pNode->GetNext())
        {
            DMUS_SEGSTATEDATA *pData = new DMUS_SEGSTATEDATA;
            if (pData)
            {
                CSegment *pSegment = pNode->m_pSegment;
                if (pSegment && (pSegment->m_dwValidData & DMUS_OBJ_NAME))
                {
                    StringCchCopyW(pData->wszName, DMUS_MAX_NAME, pSegment->m_wszName);
                }
                else
                {
                    pData->wszName[0] = 0;
                }
                pData->dwQueue = dwCount;
                pData->pSegState = (IDirectMusicSegmentState *) pNode;
                pNode->AddRef();
                pData->pNext = *ppList;
                pData->mtLoopEnd = pNode->m_mtLoopEnd;
                pData->mtLoopStart = pNode->m_mtLoopStart;
                pData->dwRepeats = pNode->m_dwRepeats;
                pData->dwPlayFlags = pNode->m_dwPlaySegFlags;
                pData->mtLength = pNode->m_mtLength;
                pData->rtGivenStart = pNode->m_rtGivenStart;
                pData->mtResolvedStart = pNode->m_mtResolvedStart;
                pData->mtOffset = pNode->m_mtOffset;
                pData->mtLastPlayed = pNode->m_mtLastPlayed;
                pData->mtPlayTo = pNode->m_mtStopTime;
                pData->mtSeek = pNode->m_mtSeek;
                pData->mtStartPoint = pNode->m_mtStartPoint;
                pData->dwRepeatsLeft = pNode->m_dwRepeatsLeft;
                pData->fStartedPlay = pNode->m_fStartedPlay;
                *ppList = pData;
            }
        }
        LeaveCriticalSection(&m_SegmentCrSec);
    }
    return S_OK;
}

STDMETHODIMP CPerformance::FreeSegstateList(DMUS_SEGSTATEDATA * pList)

{
    DMUS_SEGSTATEDATA *pState;
    while (pList)
    {
        pState = pList;
        pList = pList->pNext;
        pState->pSegState->Release();
        delete pState;
    }
    return S_OK;
}

void CPerformance::SendBuffers()
{
    DWORD dwIndex;
    PortTable* pPortTable;

#ifdef DBG_PROFILE
    DWORD dwDebugTime;
    dwDebugTime = timeGetTime();
#endif
    EnterCriticalSection(&m_PChannelInfoCrSec);
    for( dwIndex = 0; dwIndex < m_dwNumPorts; dwIndex++ )
    {
        if( m_pPortTable[dwIndex].fBufferFilled && m_pPortTable[dwIndex].pBuffer )
        {
            pPortTable = &m_pPortTable[dwIndex];
            pPortTable->fBufferFilled = FALSE;
            ASSERT( pPortTable->pBuffer );
            if( pPortTable->pPort )
            {
                pPortTable->pPort->PlayBuffer( pPortTable->pBuffer );
//  TraceI(5, "SENT BUFFERS time=%ld latency=%ld\n", (long)(GetTime() / 10000),(long)(GetLatency()/10000));
            }
            pPortTable->pBuffer->Flush();
        }
    }
    LeaveCriticalSection(&m_PChannelInfoCrSec);
#ifdef DBG_PROFILE
    dwDebugTime = timeGetTime() - dwDebugTime;
    if( dwDebugTime > 1 )
    {
        TraceI(5, "Hall, debugtime SendBuffers %u\n", dwDebugTime);
    }
#endif
}

static DWORD WINAPI _Realtime(LPVOID lpParam)
{
    if (SUCCEEDED(::CoInitialize(NULL)))
    {
        ((CPerformance *)lpParam)->Realtime();
        ::CoUninitialize();
    }
    return 0;
}

void CPerformance::Realtime()
{
    while (!m_fKillRealtimeThread)
    {
        EnterCriticalSection(&m_RealtimeCrSec);
        PRIV_PMSG *pEvent;
        HRESULT hr;
        REFERENCE_TIME  rtFirst = 0;
        REFERENCE_TIME  rtEnter = GetLatencyWithPrePlay();
        DWORD   dwTestTime;
        DWORD   dwBeginTime = timeGetTime();
        DWORD   dwLimitLoop = 0;

        if( rtEnter > m_rtQueuePosition )
        {
            m_rtQueuePosition = rtEnter;
        }

        while (1)
        {
            // rtFirst equals the time that the first event was packed into a buffer.
            // Once this time is greater than the latency clock (minus a delay) we need
            // to queue the buffers so the events get down in time to be rendered.
            // If rtFirst is 0 it means it hasn't been initialized yet.
            dwTestTime = timeGetTime();
            if( dwTestTime - dwBeginTime > REALTIME_RES )
            {
                if( ++dwLimitLoop > 10 )
                {
                    TraceI(1,"Error! We've been in the realtime thread too long!!! Breaking out without completing.\n");
                    break;
                }
                SendBuffers();
                dwBeginTime = dwTestTime;
            }
            pEvent = GetNextPMsg();
            if( NULL == pEvent )
            {
                break;
            }
            ASSERT( pEvent->pNext == NULL );
            if( !pEvent->pTool )
            {
                // this event doesn't have a Tool pointer, so stamp it with the
                // final output Tool.
                pEvent->pTool = (IDirectMusicTool*)this;
                AddRef();
            }

            // before processing the event, set rtLast to the event's current time
            pEvent->rtLast = pEvent->rtTime;

            hr = pEvent->pTool->ProcessPMsg( this, PRIV_TO_DMUS(pEvent) );
            if( hr != S_OK ) // S_OK means do nothing
            {
                if( hr == DMUS_S_REQUEUE )
                {
                    if(FAILED(SendPMsg( PRIV_TO_DMUS(pEvent) )))
                    {
                        FreePMsg(pEvent);
                    }
                }
                else // e.g. DMUS_S_FREE or error code
                {
                    FreePMsg( pEvent );
                }
            }
        }
        SendBuffers();
        LeaveCriticalSection(&m_RealtimeCrSec);
        if( m_hRealtime )
        {
            WaitForSingleObject( m_hRealtime, REALTIME_RES );
        }
        else
        {
            Sleep(REALTIME_RES);
        }
    }
    m_fKillRealtimeThread = FALSE;
    TraceI(2, "dmperf: LEAVE realtime\n");
    if (m_fReleasedInRealtime)
    {
        delete this;
    }
}

void CPerformance::GenerateNotification( DWORD dwNotification, MUSIC_TIME mtTime,
                                          IDirectMusicSegmentState* pSegSt)
{
    GUID guid;
    guid = GUID_NOTIFICATION_PERFORMANCE;
    if( FindNotification( guid ))
    {
        DMUS_NOTIFICATION_PMSG* pEvent = NULL;
        if( SUCCEEDED( AllocPMsg( sizeof(DMUS_NOTIFICATION_PMSG),
            (DMUS_PMSG**)&pEvent )))
        {
            pEvent->dwField1 = 0;
            pEvent->dwField2 = 0;
            pEvent->guidNotificationType = GUID_NOTIFICATION_PERFORMANCE;
            pEvent->dwType = DMUS_PMSGT_NOTIFICATION;
            pEvent->mtTime = mtTime;
            pEvent->dwFlags = DMUS_PMSGF_MUSICTIME | DMUS_PMSGF_TOOL_ATTIME;
            pEvent->dwGroupID = 0xffffffff;
            pEvent->dwPChannel = 0;
            pEvent->dwNotificationOption = dwNotification;
            if( pSegSt )
            {
                pSegSt->QueryInterface(IID_IUnknown, (void**)&pEvent->punkUser);
            }
            StampPMsg((DMUS_PMSG*)pEvent);
            if(FAILED(SendPMsg( (DMUS_PMSG*)pEvent )))
            {
                FreePMsg((DMUS_PMSG*)pEvent);
            }
        }
    }
}

void CPerformance::PrepSegToPlay(CSegState *pSegState, bool fQueue)

/*  Called when a segment is first queued, once the start time of the segment is known.
    This calculates various fields that need to be initialized and also regenerates the
    tempo map if the new segment has an active tempo map in it.
*/

{
    if (!pSegState->m_fPrepped)
    {
        pSegState->m_fPrepped = TRUE;
        pSegState->m_mtLastPlayed = pSegState->m_mtResolvedStart;
        // if this is queued to play after the current segment ends, no need to recalc the tempo map;
        // it will be updated as necessary by the transport thread.
        if (!fQueue)
        {
            RecalcTempoMap(pSegState, pSegState->m_mtResolvedStart);
        }
        MusicToReferenceTime(pSegState->m_mtLastPlayed,&pSegState->m_rtLastPlayed);
        // Calculate the total duration of the segment and store in m_mtEndTime.
        pSegState->m_mtEndTime = pSegState->GetEndTime(pSegState->m_mtResolvedStart);
    }
}

/*

  void | CPerformance | PerformSegStNode |
  Perform a Segment State contained in the CSegState.

  Note that this ppSegStNode may be dequeued, so don't depend on it
  staying around!

*/
void CPerformance::PerformSegStNode(
    DWORD dwList,   // The list the segmentstate comes from.
    CSegState* pSegStNode)  // The segmentstate node.
{
    MUSIC_TIME mtMargin; // tracks how much of a segment to play
    HRESULT hr;
    CSegStateList *pList = &m_SegStateQueues[dwList];
    CSegState *pNext;

    if( !m_fPlaying || m_fInTrackPlay )
    {
        return;
    }
    if( pSegStNode )
    {
        m_fInTransportThread = TRUE;    // Disable realtime processing of early queue messages.
        hr = S_OK;
//Trace(0,"%ld: Performing %lx, Active: %ld, Start Time: %ld, End Time: %ld\n",m_mtPlayTo,
//      pSegStNode->m_pSegment,pSegStNode->m_fStartedPlay,pSegStNode->m_mtResolvedStart,pSegStNode->m_mtEndTime);
        if( !pSegStNode->m_fStartedPlay )
        {
            // check to see if this SegState should start playing.
            ASSERT( !(pSegStNode->m_dwPlaySegFlags & DMUS_SEGF_REFTIME ));
            if( pSegStNode->m_mtResolvedStart < m_mtPlayTo )
            {
                pSegStNode->m_fStartedPlay = TRUE;
                PrepSegToPlay(pSegStNode);
                // send a MUSICSTARTED notification if needed
                if(m_fMusicStopped)
                {
                    m_fMusicStopped = FALSE;
                    GenerateNotification( DMUS_NOTIFICATION_MUSICSTARTED, pSegStNode->m_mtResolvedStart, NULL );
                }
                // We don't want the music to start with a big BLURP in track
                // order, so we send a little dribble out on each track.
                mtMargin = m_mtPlayTo - pSegStNode->m_mtLastPlayed;
                if( mtMargin >= 50 )
                {
                    hr = pSegStNode->Play( 50 );
                    ProcessEarlyPMsgs();
                    // Once done processing all the early messages, make sure that the realtime
                    // thread wakes up and does whatever it needs to do. This ensures that the starting
                    // notes in a sequence get to the output port immediately.
                    if( m_hRealtime ) SetEvent( m_hRealtime );
                    mtMargin = m_mtPlayTo - pSegStNode->m_mtLastPlayed;
                    // Then, we send a larger chunk out on each track to catch up a little more...
                    if ((hr == S_OK) && ( mtMargin >= 200 ))
                    {
                        hr = pSegStNode->Play( 200 );
                        ProcessEarlyPMsgs();
                    }
                }
            }
            else
            {
                MusicToReferenceTime(pSegStNode->m_mtLastPlayed,&pSegStNode->m_rtLastPlayed);
            }
        }
        if( pSegStNode->m_fStartedPlay )
        {
            if( pSegStNode->m_mtStopTime && ( pSegStNode->m_mtStopTime < m_mtPlayTo ) )
            {
                mtMargin = pSegStNode->m_mtStopTime - pSegStNode->m_mtLastPlayed;
            }
            else
            {
                mtMargin = m_mtPlayTo - pSegStNode->m_mtLastPlayed;
            }
            while ((hr == S_OK) && (mtMargin > 0))
            {
                // Do not allow more than a quarter note's worth to be done at once.
                MUSIC_TIME mtRange = mtMargin;
                if (mtRange > DMUS_PPQ)
                {
                    mtRange = DMUS_PPQ;
                    mtMargin -= mtRange;
                }
                else
                {
                    mtMargin = 0;
                }
                hr = pSegStNode->Play( mtRange );
                ProcessEarlyPMsgs();
            }
        }
        if( (hr == DMUS_S_END) || ( pSegStNode->m_mtStopTime &&
                                  ( pSegStNode->m_mtStopTime <= pSegStNode->m_mtLastPlayed ) ) )
        {

            if( pSegStNode->m_mtStopTime && (pSegStNode->m_mtStopTime == pSegStNode->m_mtLastPlayed) )
            {
                pSegStNode->AbortPlay(pSegStNode->m_mtStopTime - 1, FALSE);
            }
            MUSIC_TIME mtEnd = pSegStNode->m_mtLastPlayed;
            if( pList == &m_SegStateQueues[SQ_PRI_PLAY] )
            {
                // move primary segments to PriPastList
                pList->Remove(pSegStNode);
                m_SegStateQueues[SQ_PRI_DONE].Insert(pSegStNode);
                pNext = pList->GetHead();
                if( pNext )
                {
                    if (!( pNext->m_dwPlaySegFlags & DMUS_SEGF_NOINVALIDATE ))
                    {
                        if (IsConQueue(dwList))
                        {
                            Invalidate( pNext->m_mtResolvedStart, 0 );
                        }
                    }
                }
                else    // No more primary segments, send DMUS_NOTIFICATION_MUSICALMOSTEND
                {
                    if (m_dwVersion >= 8)
                    {
                        MUSIC_TIME mtNow;
                        GetTime( NULL, &mtNow );
                        GenerateNotification( DMUS_NOTIFICATION_MUSICALMOSTEND, mtNow, pSegStNode );
                    }
                }
                ManageControllingTracks();
            }
            else if ( pList == &m_SegStateQueues[SQ_CON_PLAY] )
            {
                pList->Remove(pSegStNode );
                if (pSegStNode->m_mtStopTime == pSegStNode->m_mtLastPlayed)
                {
                    m_ShutDownQueue.Insert(pSegStNode);
                }
                else
                {
                    m_SegStateQueues[SQ_CON_DONE].Insert(pSegStNode);
                }
            }
            else
            {
                // move 2ndary segments to SecPastList
                pList->Remove(pSegStNode);
                m_SegStateQueues[SQ_SEC_DONE].Insert(pSegStNode);
            }
            // if there aren't any more segments to play, send a Music Stopped
            // notification
            if( (m_SegStateQueues[SQ_PRI_PLAY].IsEmpty() && m_SegStateQueues[SQ_SEC_PLAY].IsEmpty() &&
                m_SegStateQueues[SQ_PRI_WAIT].IsEmpty() && m_SegStateQueues[SQ_SEC_WAIT].IsEmpty() &&
                m_SegStateQueues[SQ_CON_PLAY].IsEmpty() && m_SegStateQueues[SQ_CON_WAIT].IsEmpty()))
            {
                m_fMusicStopped = TRUE;
                GenerateNotification( DMUS_NOTIFICATION_MUSICSTOPPED, mtEnd, NULL );
            }
        }
        m_fInTransportThread = FALSE;
    }
}

static DWORD WINAPI _Transport(LPVOID lpParam)
{
    if (SUCCEEDED(::CoInitialize(NULL)))
    {
        ((CPerformance *)lpParam)->Transport();
        ::CoUninitialize();
    }
    return 0;
}

// call Segment's play code on a periodic basis. This routine is in its
// own thread.
void CPerformance::Transport()
{
    srand((unsigned int)time(NULL));
    while (!m_fKillThread)
    {
        DWORD dwCount;
        CSegState*  pNode;
        CSegState*  pNext;
        CSegState*  pTempQueue = NULL;
        REFERENCE_TIME rtNow = GetTime();

        EnterCriticalSection(&m_SegmentCrSec);
        // Compute the time we should play all the segments to.
        REFERENCE_TIME rtPlayTo = rtNow + PREPARE_TIME;
        MUSIC_TIME mtAmount, mtResult, mtPlayTo;
        mtPlayTo = 0;
        ReferenceToMusicTime( rtPlayTo, &mtPlayTo );
        if (m_fTempoChanged)
        {
            // If there has been a tempo change to slower, any clock time tracks could
            // be delayed to long as the transport holds off sending out events. That's
            // okay for music time tracks, but bad news for clock time tracks. This
            // makes sure that the clock time tracks get a chance to spew.
            if (m_mtPlayTo >= mtPlayTo)
            {
                mtPlayTo = m_mtPlayTo + 10;
            }
            m_fTempoChanged = FALSE;
        }
        IncrementTempoMap();
        while (m_mtPlayTo < mtPlayTo)
        {
            BOOL fDirty = FALSE; // see below
            m_mtPlayTo = mtPlayTo; // Start out optimistic
            // We need to set play boundaries at the end of control segments.
            // The beginnings of control segments are handled inside the segment state code.
            pNode = m_SegStateQueues[SQ_PRI_PLAY].GetHead();
            if( pNode && pNode->m_fStartedPlay )
            {
                mtAmount = m_mtPlayTo - pNode->m_mtLastPlayed;
                pNode->CheckPlay( mtAmount, &mtResult );
                if( mtResult < mtAmount )
                {
                    m_mtPlayTo -= ( mtAmount - mtResult );
                    // don't need dirty flag when primary segment loops or ends normally (bug 30829)
                    // fDirty = TRUE; // see below
                }
            }
            // if a control segment ended prematurely, mtPlayTo will have a value besides 0
            // check for upcoming endings to control segments
            for( pNode = m_SegStateQueues[SQ_CON_PLAY].GetHead(); pNode; pNode = pNode->GetNext() )
            {
                if( pNode->m_fStartedPlay )
                {
                    if( pNode->m_mtStopTime && (m_mtPlayTo > pNode->m_mtStopTime) )
                    {
                        m_mtPlayTo = pNode->m_mtStopTime;
                        fDirty = TRUE; // see below
                    }
                    else
                    {
                        mtAmount = m_mtPlayTo - pNode->m_mtLastPlayed;
                        pNode->CheckPlay( mtAmount, &mtResult );
                        if( mtResult < mtAmount )
                        {
                            m_mtPlayTo -= ( mtAmount - mtResult );
                            fDirty = TRUE; // see below
                        }
                    }
                }
            }
            // play the primary segment
            PerformSegStNode( SQ_PRI_PLAY,m_SegStateQueues[SQ_PRI_PLAY].GetHead() );
            // check to see if the next primary segment in the queue is ready to play
            while( (pNode = m_SegStateQueues[SQ_PRI_PLAY].GetHead()) &&
                (pNext = pNode->GetNext()) &&
                ( pNext->m_mtResolvedStart <= pNode->m_mtLastPlayed ) )
            {
                // the next primary segment is indeed ready to begin playing.
                // save the old one in the primary past list so Tools can reference
                // it if they're looking for chord progressions and such.
                pNode->AbortPlay(pNext->m_mtResolvedStart-1,TRUE && (pNext->m_dwPlaySegFlags & DMUS_SEGF_NOINVALIDATE));
                m_SegStateQueues[SQ_PRI_DONE].Insert(m_SegStateQueues[SQ_PRI_PLAY].RemoveHead());
                ManageControllingTracks();
                // we need to flush primary events after the new start time
                if(!( m_SegStateQueues[SQ_PRI_PLAY].GetHead()->m_dwPlaySegFlags & (DMUS_SEGF_NOINVALIDATE | DMUS_SEGF_INVALIDATE_PRI) ))
                {
                    Invalidate( m_SegStateQueues[SQ_PRI_PLAY].GetHead()->m_mtResolvedStart, 0 );
                }
                // and play the new segment
                PerformSegStNode( SQ_PRI_PLAY,m_SegStateQueues[SQ_PRI_PLAY].GetHead());
            }
            // play the controlling segments
            pNode = m_SegStateQueues[SQ_CON_PLAY].GetHead();
            pNext = NULL;
            for(; pNode != NULL; pNode = pNext)
            {
                pNext = pNode->GetNext();
                PerformSegStNode(SQ_CON_PLAY,pNode );
            }
            // play the secondary segments
            pNode = m_SegStateQueues[SQ_SEC_PLAY].GetHead();
            pNext = NULL;
            for(; pNode != NULL; pNode = pNext)
            {
                pNext = pNode->GetNext();
                PerformSegStNode( SQ_SEC_PLAY,pNode );
            }

            // if we set fDirty above, it means that we truncated the playback of a control
            // segment because of a loop or end condition. Therefore, we want all segments
            // to set the DMUS_TRACKF_DIRTY flag on the next play cycle.
            if( fDirty )
            {
                for (dwCount = SQ_PRI_PLAY; dwCount <= SQ_SEC_PLAY; dwCount++)
                {
                    for( pNode = m_SegStateQueues[dwCount].GetHead(); pNode; pNode = pNode->GetNext() )
                    {
                        if( pNode->m_fStartedPlay )
                        {
                            pNode->m_dwPlayTrackFlags |= DMUS_TRACKF_DIRTY;
                        }
                    }
                }
                ManageControllingTracks();
            }
            m_mtTransported = m_mtPlayTo;

        }

        // check segments queued in ref-time to see if it's time for them to
        // play. Add some extra time just in case. We'll bet that a tempo pmsg won't come
        // in in the intervening 200 ms.
        REFERENCE_TIME rtLatency = GetLatencyWithPrePlay();
        for (dwCount = SQ_PRI_WAIT;dwCount <= SQ_SEC_WAIT; dwCount++)
        {
            while( m_SegStateQueues[dwCount].GetHead() )
            {
                if( m_SegStateQueues[dwCount].GetHead()->m_rtGivenStart > rtLatency + PREPARE_TIME + (200 * REF_PER_MIL) )
                {
                    // it's not yet time to handle this one
                    break;
                }
                if (dwCount == SQ_PRI_WAIT)
                {
                    QueuePrimarySegment( m_SegStateQueues[SQ_PRI_WAIT].RemoveHead());
                }
                else
                {
                    QueueSecondarySegment( m_SegStateQueues[dwCount].RemoveHead());
                }
            }
        }

        // Check to see if Segments in the done queues
        // can be released. They can be released if their
        // final play times are older than the current time.
        for (dwCount = SQ_PRI_DONE;dwCount <= SQ_SEC_DONE; dwCount++)
        {
            for (pNode = m_SegStateQueues[dwCount].GetHead();pNode;pNode = pNext)
            {
                pNext = pNode->GetNext();
                if( pNode->m_rtLastPlayed < rtNow - 1000 * REF_PER_MIL ) // Let it last an additional second
                {
                    m_SegStateQueues[dwCount].Remove(pNode);
                    pNode->ShutDown();
                }
            }
        }
        for (pNode = m_ShutDownQueue.GetHead();pNode;pNode = pNext)
        {
            pNext = pNode->GetNext();
            if( pNode->m_rtLastPlayed < rtNow - 1000 * REF_PER_MIL ) // Let it last an additional second
            {
                m_ShutDownQueue.Remove(pNode);
                pNode->ShutDown();
            }
        }
        LeaveCriticalSection(&m_SegmentCrSec);

        // check to see if there are old notifications that haven't been
        // retrieved by the application and need to be removed.
        EnterCriticalSection(&m_PipelineCrSec);
        while( m_NotificationQueue.GetHead() )
        {
            if( m_NotificationQueue.GetHead()->rtTime <
                (rtNow - m_rtNotificationDiscard) )
            {
                FreePMsg(m_NotificationQueue.Dequeue());
            }
            else
            {
                break;
            }
        }
        LeaveCriticalSection(&m_PipelineCrSec);
        if( m_hTransport )
        {
            WaitForSingleObject( m_hTransport, TRANSPORT_RES );
        }
        else
        {
            Sleep(TRANSPORT_RES);
        }
    }
    m_fKillThread = FALSE;
    if (m_fReleasedInTransport)
    {
        delete this;
    }
}

//////////////////////////////////////////////////////////////////////
// CPerformance::GetNextPMsg
/*
HRESULT | CPerformance | GetNextPMsg |
Returns messages from the queues in priority order.  Any message in the
OnTime queue that is scheduled to be played at the current time is
returned above any other.  Secondly, any message in the NearTime queue
that is scheduled to be played within the next NEARTIME ms is returned.
Lastly, any message in the Early queue is returned.

rvalue PRIV_PMSG* | The message, or NULL if there are no messages.
*/
inline PRIV_PMSG *CPerformance::GetNextPMsg()
{
#ifdef DBG_PROFILE
    DWORD dwDebugTime;
    dwDebugTime = timeGetTime();
#endif
    PRIV_PMSG* pEvent = NULL;

    EnterCriticalSection(&m_PipelineCrSec);
    if (m_OnTimeQueue.GetHead())
    {
        ASSERT( m_OnTimeQueue.GetHead()->dwFlags & DMUS_PMSGF_REFTIME );
        if ( m_OnTimeQueue.GetHead()->rtTime - GetTime() <= 0 )
        {
            pEvent = m_OnTimeQueue.Dequeue();
        }
    }
    if( !pEvent )
    {
        if (m_NearTimeQueue.GetHead())
        {
            ASSERT( m_NearTimeQueue.GetHead()->dwFlags & DMUS_PMSGF_REFTIME );
            if ( m_NearTimeQueue.GetHead()->rtTime < (m_rtQueuePosition + (m_rtBumperLength >> 1)))
            {
                pEvent = m_NearTimeQueue.Dequeue();
            }
        }
        if( !pEvent && !m_fInTransportThread)
        {
            if (m_EarlyQueue.GetHead())
            {
                pEvent = m_EarlyQueue.Dequeue();
            }
        }
    }
    LeaveCriticalSection(&m_PipelineCrSec);
#ifdef DBG_PROFILE
    dwDebugTime = timeGetTime() - dwDebugTime;
    if( dwDebugTime > 1 )
    {
        TraceI(5, "Hall, debugtime GetNextPMsg %u\n", dwDebugTime);
    }
#endif

    return pEvent;
}

/*  This next function is used just by the transport thread
    which can process messages in the early queue, but not
    the other types. This allows all the tools that process
    events right after they are generated by tracks to process
    the events right after they were generated, and in sequential
    order. This allows them to take a little longer, since it's
    not as time critical, and it's much more likely to ensure
    that they are in sequential order. If the realtime thread were
    allowed to process these, it would preempt and process them
    as soon as generated, so they would be processed in the order
    of the tracks. The m_fInTransportThread is set by the
    transport thread when it is generating and processing events
    and this disallows the realtime thread from processing
    early events (but not others.) At other times, the realtime
    thread is welcome to process early events.
*/

void CPerformance::ProcessEarlyPMsgs()
{
    PRIV_PMSG* pEvent;

    //  Exit if the thread is exiting.  If we don't test here
    //  we can actually loop forever because tools and queue more
    //  early PMSGs (the Echo tool does this)
    while (!m_fKillThread)
    {
        EnterCriticalSection(&m_PipelineCrSec);
        pEvent = m_EarlyQueue.Dequeue();
        LeaveCriticalSection(&m_PipelineCrSec);
        if (!pEvent) break; // Done?
        ASSERT( pEvent->pNext == NULL );
        if( !pEvent->pTool )
        {
            // this event doesn't have a Tool pointer, so stamp it with the
            // final output Tool.
            pEvent->pTool = (IDirectMusicTool*)this;
            AddRef();
            // Don't process it. Instead, send to neartime queue so
            // realtime thread will deal with it.
            pEvent->dwFlags &= ~(DMUS_PMSGF_TOOL_IMMEDIATE | DMUS_PMSGF_TOOL_QUEUE | DMUS_PMSGF_TOOL_ATTIME);
            pEvent->dwFlags |= DMUS_PMSGF_TOOL_QUEUE;
            SendPMsg( PRIV_TO_DMUS(pEvent) );
        }
        else
        {
            // before processing the event, set rtLast to the event's current time
            pEvent->rtLast = pEvent->rtTime;

            HRESULT hr = pEvent->pTool->ProcessPMsg( this, PRIV_TO_DMUS(pEvent) );
            if( hr != S_OK ) // S_OK means do nothing
            {
                if( hr == DMUS_S_REQUEUE )
                {
                    if(FAILED(SendPMsg( PRIV_TO_DMUS(pEvent) )))
                    {
                        FreePMsg(pEvent);
                    }
                }
                else // e.g. DMUS_S_FREE or error code
                {
                    FreePMsg( pEvent );
                }
            }
        }
    }
}

REFERENCE_TIME CPerformance::GetTime()
{
    REFERENCE_TIME rtTime;
    REFERENCE_TIME rtCurrent = 0;
    WORD    w;
    HRESULT hr = S_OK;

    EnterCriticalSection(&m_MainCrSec);
    if (m_pClock) hr = m_pClock->GetTime( &rtCurrent );
    if( !m_pClock || FAILED( hr ) || rtCurrent == 0 )
    {
        // this only gets called with machines that don't support m_pClock
        rtTime = timeGetTime();
        rtCurrent = rtTime * REF_PER_MIL; // 100 ns increments
        // take care of timeGetTime rolling over every 49 days
        if( rtCurrent < 0 )
        {
            m_wRollOverCount++;
        }
        for( w = 0; w < m_wRollOverCount; w++ )
        {
            rtCurrent += 4294967296;
        }
        // if rtCurrent is negative, it means we've rolled over rtCurrent. Ignore
        // this case for now, as it will be quite uncommon.
    }
    LeaveCriticalSection(&m_MainCrSec);

    return rtCurrent;
}

REFERENCE_TIME CPerformance::GetLatency(void)
{
    DWORD dwIndex;
    REFERENCE_TIME rtLatency = 0;
    REFERENCE_TIME rtTemp;

#ifdef DBG_PROFILE
    DWORD dwDebugTime;
    dwDebugTime = timeGetTime();
#endif
    EnterCriticalSection(&m_PChannelInfoCrSec);
    if( m_pPortTable )
    {
        for( dwIndex = 0; dwIndex < m_dwNumPorts; dwIndex++ )
        {
            if( m_pPortTable[dwIndex].pLatencyClock )
            {
                if( SUCCEEDED( m_pPortTable[dwIndex].pLatencyClock->GetTime( &rtTemp )))
                {
                    if( rtTemp > rtLatency )
                        rtLatency = rtTemp;
                }
            }
            else if( m_pPortTable[dwIndex].pPort )
            {
                if( SUCCEEDED( m_pPortTable[dwIndex].pPort->GetLatencyClock( &m_pPortTable[dwIndex].pLatencyClock )))
                {
                    if( SUCCEEDED( m_pPortTable[dwIndex].pLatencyClock->GetTime( &rtTemp )))
                    {
                        if( rtTemp > rtLatency )
                            rtLatency = rtTemp;
                    }
                }
            }
        }
    }
    LeaveCriticalSection(&m_PChannelInfoCrSec);
    if( 0 == rtLatency )
    {
        rtLatency = GetTime();
    }
#ifdef DBG_PROFILE
    dwDebugTime = timeGetTime() - dwDebugTime;
    if( dwDebugTime > 1 )
    {
        TraceI(5, "Hall, debugtime GetLatency %u\n", dwDebugTime);
    }
#endif
    if (m_rtEarliestStartTime > rtLatency)
    {
        rtLatency = m_rtEarliestStartTime;
    }
    return rtLatency;
}

// return the most desireable Segment latency, based on which ports this
// segment plays on.
REFERENCE_TIME CPerformance::GetBestSegLatency( CSegState* pSeg )
{
    // If we're using audiopaths, the code below doesn't work because it doesn't
    // take converting pchannels into account. So, just use the worse case
    // latency. 99% of the time, there is only one port, so this results
    // in just a performance enhancement.
    if (m_dwAudioPathMode == 2)
    {
        return GetLatency();
    }
    DWORD dwIndex;
    REFERENCE_TIME rtLatency = 0;
    REFERENCE_TIME rtTemp;
    BOOL* pafIndexUsed = NULL;
    DWORD dwCount;

    if( m_dwNumPorts == 1 )
    {
        return GetLatency();
    }
    pafIndexUsed = new BOOL[m_dwNumPorts];
    if( NULL == pafIndexUsed )
    {
        return GetLatency();
    }
    for( dwCount = 0; dwCount < m_dwNumPorts; dwCount++ )
    {
        pafIndexUsed[dwCount] = FALSE;
    }
    DWORD dwNumPChannels, dwGroup, dwMChannel;
    DWORD* paPChannels;
    pSeg->m_pSegment->GetPChannels( &dwNumPChannels, &paPChannels );
    for( dwCount = 0; dwCount < dwNumPChannels; dwCount++ )
    {
        if( SUCCEEDED( PChannelIndex( paPChannels[dwCount],
            &dwIndex, &dwGroup, &dwMChannel )))
        {
            pafIndexUsed[dwIndex] = TRUE;
        }
    }
    for( dwCount = 0; dwCount < m_dwNumPorts; dwCount++ )
    {
        if( pafIndexUsed[dwCount] )
            break;
    }
    if( dwCount >= m_dwNumPorts )
    {
        delete [] pafIndexUsed;
        return GetLatency();
    }
    EnterCriticalSection(&m_PChannelInfoCrSec);
    for( dwIndex = 0; dwIndex < m_dwNumPorts; dwIndex++ )
    {
        if( pafIndexUsed[dwIndex] )
        {
            if( m_pPortTable[dwIndex].pLatencyClock )
            {
                if( SUCCEEDED( m_pPortTable[dwIndex].pLatencyClock->GetTime( &rtTemp )))
                {
                    if( rtTemp > rtLatency )
                        rtLatency = rtTemp;
                }
            }
            else if( m_pPortTable[dwIndex].pPort )
            {
                if( SUCCEEDED( m_pPortTable[dwIndex].pPort->GetLatencyClock( &m_pPortTable[dwIndex].pLatencyClock )))
                {
                    if( SUCCEEDED( m_pPortTable[dwIndex].pLatencyClock->GetTime( &rtTemp )))
                    {
                        if( rtTemp > rtLatency )
                            rtLatency = rtTemp;
                    }
                }
            }
        }
    }
    LeaveCriticalSection(&m_PChannelInfoCrSec);
    if( 0 == rtLatency )
    {
        rtLatency = GetLatency();
    }
    delete [] pafIndexUsed;
    return rtLatency;
}

/*  Called from either QueuePrimarySegment or QueueSecondarySegment,
    this calculates the appropriate boundary time to start the segment
    playback. Most of the logic takes care of the new DMUS_SEGF_ALIGN
    capabilities.
*/


void CPerformance::CalculateSegmentStartTime( CSegState* pSeg )
{
    BOOL fNoValidStart = TRUE;
    if (pSeg->m_dwPlaySegFlags & DMUS_SEGF_ALIGN)
    {
        // If the ALIGN flag is set, see if we can align with the requested resolution,
        // but switch to the new segment at an earlier point, as defined by
        // a "valid start" point in the new segment.
        DMUS_VALID_START_PARAM ValidStart;    // Used to read start parameter from segment.
        MUSIC_TIME mtIntervalSize = 0;      // Quantization value.
        MUSIC_TIME mtTimeNow = (MUSIC_TIME)pSeg->m_rtGivenStart; // The earliest time this can start.
        // Call resolve time to get the last quantized interval that precedes mtTimeNow.
        MUSIC_TIME mtStartTime = ResolveTime( mtTimeNow, pSeg->m_dwPlaySegFlags, &mtIntervalSize );
        // StartTime actually shows the next time after now, so subtract the interval time to get the previous position.
        mtStartTime -= mtIntervalSize;
        // If the segment was supposed to start after the very beginning, quantize it.
        if (mtIntervalSize && pSeg->m_mtStartPoint)
        {
            pSeg->m_mtStartPoint = ((pSeg->m_mtStartPoint + (mtIntervalSize >> 1))
                / mtIntervalSize) * mtIntervalSize;
            // If this ends up being longer than the segment, do we need to drop back?
        }
        // Now, get the next start point after the point in the segment that
        // corresponds with mtTimeNow, adjusted for the startpoint.
        if (SUCCEEDED(pSeg->m_pSegment->GetParam( GUID_Valid_Start_Time,-1,0,
            pSeg->m_mtStartPoint + mtTimeNow - mtStartTime,NULL,(void *) &ValidStart)))
        {
            // If the valid start point is within the range, we can cut in at the start point.
            if ((mtTimeNow - mtStartTime + ValidStart.mtTime) < (mtIntervalSize + pSeg->m_mtStartPoint))
            {
                pSeg->m_mtResolvedStart = mtTimeNow + ValidStart.mtTime;
                pSeg->m_mtStartPoint += mtTimeNow - mtStartTime + ValidStart.mtTime;
                fNoValidStart = FALSE;
            }
        }
        if (fNoValidStart)
        {
            // Couldn't find a valid start point. Was DMUS_SEGF_VALID_START_XXX set so we can override?
            if (pSeg->m_dwPlaySegFlags &
                (DMUS_SEGF_VALID_START_MEASURE | DMUS_SEGF_VALID_START_BEAT | DMUS_SEGF_VALID_START_GRID | DMUS_SEGF_VALID_START_TICK))
            {
                MUSIC_TIME mtOverrideTime;
                // Depending on the flag, we need to get the appropriate interval resolution.
                if (pSeg->m_dwPlaySegFlags & DMUS_SEGF_VALID_START_MEASURE)
                {
                    mtOverrideTime = ResolveTime( mtTimeNow, DMUS_SEGF_MEASURE, 0 );
                }
                else if (pSeg->m_dwPlaySegFlags & DMUS_SEGF_VALID_START_BEAT)
                {
                    mtOverrideTime = ResolveTime( mtTimeNow, DMUS_SEGF_BEAT, 0 );
                }
                else if (pSeg->m_dwPlaySegFlags & DMUS_SEGF_VALID_START_GRID)
                {
                    mtOverrideTime = ResolveTime( mtTimeNow, DMUS_SEGF_GRID, 0 );
                }
                else
                {
                    mtOverrideTime = mtTimeNow;
                }
                // If the valid start point is within the range, we can cut in at the start point.
                if ((mtOverrideTime - mtTimeNow) < (mtIntervalSize + pSeg->m_mtStartPoint))
                {
                    pSeg->m_mtResolvedStart = mtOverrideTime;
                    if ((mtOverrideTime - mtStartTime) >= mtIntervalSize)
                    {
                        mtOverrideTime -= mtIntervalSize;
                    }
/*Trace(0,"Startpoint %ld plus OverrideTime %ld - StartTime %ld = %ld\n",
      pSeg->m_mtStartPoint, mtOverrideTime - mtSegmentTime, mtStartTime - mtSegmentTime,
        pSeg->m_mtStartPoint + mtOverrideTime - mtStartTime);*/
                    pSeg->m_mtStartPoint += mtOverrideTime - mtStartTime;
                    fNoValidStart = FALSE;
                }
            }
        }
    }
    if (fNoValidStart)
    {
        pSeg->m_mtResolvedStart = ResolveTime( (MUSIC_TIME)pSeg->m_rtGivenStart,
            pSeg->m_dwPlaySegFlags, NULL );
    }
    else
    {
        // If we succeeded in finding a place to switch over, make sure it isn't deep inside
        // a loop. This is specifically a problem when syncing to segment and switching inside
        // or after a loop.
        while (pSeg->m_dwRepeats && (pSeg->m_mtStartPoint >= pSeg->m_mtLoopEnd))
        {
            pSeg->m_dwRepeats--;
            pSeg->m_mtStartPoint -= (pSeg->m_mtLoopEnd - pSeg->m_mtLoopStart);
        }
        // Since we were decrementing the repeats, we need to also decrement the repeats left.
        pSeg->m_dwRepeatsLeft = pSeg->m_dwRepeats;
        // Finally, if the startpoint is after the end of the segment, cut it back to the end of the
        // segment. This will cause it to play for time 0 and, if this is a transition segment, whatever
        // should play after will play immediately.
        if (pSeg->m_mtStartPoint > pSeg->m_mtLength)
        {
            pSeg->m_mtStartPoint = pSeg->m_mtLength;
        }
    }
    pSeg->m_mtOffset = pSeg->m_mtResolvedStart;
    pSeg->m_mtLastPlayed = pSeg->m_mtResolvedStart;
}

// this function should only be called from within a SegmentCrSec
// critical section!
void CPerformance::QueuePrimarySegment( CSegState* pSeg )
{
    CSegState* pTemp;
    BOOL fInCrSec = TRUE;
    BOOL fNotDone = TRUE;
    EnterCriticalSection(&m_PipelineCrSec);
    pSeg->m_dwPlayTrackFlags |= DMUS_TRACKF_DIRTY;
    if( pSeg->m_dwPlaySegFlags & DMUS_SEGF_QUEUE )
    {
        MUSIC_TIME mtStart = 0;

        pTemp = m_SegStateQueues[SQ_PRI_PLAY].GetTail();
        if( pTemp )
        {
            mtStart = pTemp->GetEndTime( pTemp->m_mtResolvedStart );
        }
        else
        {
            pTemp = m_SegStateQueues[SQ_PRI_DONE].GetTail();
            if( pTemp )
            {
                mtStart = pTemp->m_mtLastPlayed;
            }
        }
        pSeg->m_dwPlaySegFlags &= ~DMUS_SEGF_QUEUE;
        if( NULL == pTemp )
        {
            // if there's nothing in the queue, this means play it now
            if( pSeg->m_dwPlaySegFlags & DMUS_SEGF_AFTERPREPARETIME )
            {
                // we want to queue this at the last transported time,
                // so we don't need to do an invalidate
                if( pSeg->m_dwPlaySegFlags & DMUS_SEGF_REFTIME )
                {
                    REFERENCE_TIME rtTrans;
                    MusicToReferenceTime( m_mtTransported, &rtTrans );
                    if( pSeg->m_rtGivenStart < rtTrans )
                    {
                        pSeg->m_dwPlaySegFlags &= ~DMUS_SEGF_REFTIME;
                        pSeg->m_rtGivenStart = m_mtTransported;
                    }
                }
                else
                {
                    if( pSeg->m_rtGivenStart < m_mtTransported )
                    {
                        pSeg->m_rtGivenStart = m_mtTransported;
                    }
                }
            }
            else
            {
                // This will be changed to Queue time below
                pSeg->m_rtGivenStart = 0;
            }
        }
        else
        {
            REFERENCE_TIME rtQueue;

            // otherwise, time stamp it with the time corresponding to
            // the end time of all segments currently in the queue.
            pSeg->m_mtResolvedStart = mtStart;
            // make sure the resolved start time isn't before the latency
            GetQueueTime(&rtQueue);
            ReferenceToMusicTime( rtQueue, &mtStart );
            if( pSeg->m_mtResolvedStart < mtStart )
            {
                pSeg->m_mtResolvedStart = 0; // below code will take care of this case
            }
            else
            {
                pSeg->m_dwPlaySegFlags &= ~DMUS_SEGF_REFTIME;
                pSeg->m_mtOffset = pSeg->m_mtResolvedStart;
                m_SegStateQueues[SQ_PRI_PLAY].Insert(pSeg);
                TraceI(2, "dmperf: queueing primary seg/DMUS_SEGF_QUEUE. Prev time=%ld, this=%ld\n",
                    pTemp->m_mtResolvedStart, pSeg->m_mtResolvedStart);
                fNotDone = FALSE;
                PrepSegToPlay(pSeg, true);
            }
        }
    }
    if( fNotDone && (pSeg->m_rtGivenStart == 0) )
    {
        // if the given start time is 0, it means play now.
        MUSIC_TIME mtStart;
        REFERENCE_TIME rtStart;

        GetQueueTime( &rtStart );
        ReferenceToMusicTime( rtStart, &mtStart );
        pSeg->m_dwPlaySegFlags &= ~DMUS_SEGF_REFTIME;
        pSeg->m_rtGivenStart = mtStart;
        // we definitely want to get rid of all segments following
        // the currently playing segment
        if( m_SegStateQueues[SQ_PRI_PLAY].GetHead() )
        {
            while( pTemp = m_SegStateQueues[SQ_PRI_PLAY].GetHead()->GetNext() )
            {
                m_SegStateQueues[SQ_PRI_PLAY].Remove(pTemp);
                pTemp->AbortPlay(mtStart,FALSE);
                m_ShutDownQueue.Insert(pTemp);
            }
        }
    }
    if( fNotDone && pSeg->m_dwPlaySegFlags & DMUS_SEGF_REFTIME )
    {
        // rtStartTime is in RefTime units.
        // We can convert this to Music Time immediately if either there
        // is no currently playing Primary Segment, or the conversion
        // falls within the time that has already played. If the time
        // falls within PREPARE_TIME, we need to get this Segment
        // playing right away.
        REFERENCE_TIME rtNow = m_rtQueuePosition;
        MUSIC_TIME mtTime;
        if( m_SegStateQueues[SQ_PRI_PLAY].IsEmpty() || ( pSeg->m_rtGivenStart <= rtNow ) )
        {
            ReferenceToMusicTime( pSeg->m_rtGivenStart, &mtTime );
            pSeg->m_dwPlaySegFlags &= ~( DMUS_SEGF_REFTIME );
            pSeg->m_rtGivenStart = mtTime;
            // let the block of code below that handles music time
            // deal with it from here on
        }
        else
        {
            // Otherwise, we must wait until rtStartTime
            // has been performed in order to convert to music time, because
            // we require the tempo map at that time to do the conversion.
            // This will be handled by the Transport code.
            m_SegStateQueues[SQ_PRI_WAIT].Insert(pSeg);
            fNotDone = FALSE; // prevents the next block of code from operating on
                        // this Segment.
        }
    }
    if( fNotDone ) // music time
    {
        // if we're in music time units, we can queue this segment in the
        // main queue, in time order. If this segment's music time is less
        // than the start time of other segments in the queue, all of those
        // segments are removed and discarded. Also, segments that are in
        // the wait queue as RefTime are discarded.

        ASSERT( !(pSeg->m_dwPlaySegFlags & DMUS_SEGF_REFTIME )); // m_rtGivenStart must be in music time
        CalculateSegmentStartTime( pSeg );
        while( (pTemp = m_SegStateQueues[SQ_PRI_WAIT].RemoveHead()) )
        {
            pTemp->AbortPlay(pSeg->m_mtResolvedStart,FALSE);
            m_ShutDownQueue.Insert(pTemp);
        }
        if( pTemp = m_SegStateQueues[SQ_PRI_PLAY].GetHead() )
        {
            if( pSeg->m_mtResolvedStart > pTemp->m_mtResolvedStart )
            {
                while( pTemp->GetNext() )
                {
                    if( pTemp->GetNext()->m_mtResolvedStart >= pSeg->m_mtResolvedStart )
                    {
                        break;
                    }
                    pTemp = pTemp->GetNext();
                }
                pSeg->SetNext(pTemp->GetNext());
                pTemp->SetNext(pSeg);
                while( pTemp = pSeg->GetNext() )
                {
                    // delete the remaining pSegs after this one
                    pSeg->SetNext(pTemp->GetNext());
                    pTemp->AbortPlay(pSeg->m_mtResolvedStart,FALSE);
                    m_ShutDownQueue.Insert(pTemp);
                }
            }
            else
            {
                if( !pTemp->m_fStartedPlay )
                {
                    // blow away the entire queue
                    while( m_SegStateQueues[SQ_PRI_PLAY].GetHead() )
                    {
                        pTemp = m_SegStateQueues[SQ_PRI_PLAY].RemoveHead();
                        pTemp->AbortPlay(pSeg->m_mtResolvedStart,FALSE);
                        m_ShutDownQueue.Insert(pTemp);
                    }
                    m_SegStateQueues[SQ_PRI_PLAY].AddHead(pSeg);
                    // give this a chance to start performing if it's near
                    // enough to time
                    if( fInCrSec )
                    {
                        LeaveCriticalSection(&m_PipelineCrSec);
                        fInCrSec = FALSE;
                    }
                    SyncTimeSig( pSeg );
                    ManageControllingTracks();
                    PerformSegStNode( SQ_PRI_PLAY,pSeg);
                }
                else
                {
                    // else, place this segment after the current one
                    // and count on the routine below to take care of dequeing
                    // the current one, because in this case m_mtLastPlayed
                    // must be greater than m_mtResolvedStart.
                    if ( m_SegStateQueues[SQ_PRI_PLAY].GetHead()->m_mtLastPlayed <=
                        m_SegStateQueues[SQ_PRI_PLAY].GetHead()->m_mtResolvedStart )
                    {
                        TraceI(0,"Current Primary segment has not started playing.\n");
                    }
                    m_SegStateQueues[SQ_PRI_PLAY].AddHead(pSeg);
                    MUSIC_TIME mtTime = pSeg->m_mtResolvedStart;
                    while( pTemp = pSeg->GetNext() )
                    {
                        pTemp->AbortPlay( mtTime, TRUE && (pSeg->m_dwPlaySegFlags & DMUS_SEGF_NOINVALIDATE) );
                        // delete the remaining pSegs after this one
                        pSeg->SetNext(pTemp->GetNext());
                        m_ShutDownQueue.Insert(pTemp);
                    }
                }
            }
            // m_pPriSegQueue could have become NULL from the PerformSegStNode call above.
            if( m_SegStateQueues[SQ_PRI_PLAY].GetHead() && (pSeg != m_SegStateQueues[SQ_PRI_PLAY].GetHead()) )
            {
                CSegState *pCurrentSeg = m_SegStateQueues[SQ_PRI_PLAY].GetHead();
                if( pCurrentSeg->m_fStartedPlay &&
                    ( pSeg->m_mtResolvedStart <= pCurrentSeg->m_mtLastPlayed ))
                {
                    // If Playsegment is recursively called by the end of a previous segment in a song, don't abort.
                    if (!pCurrentSeg->m_fInPlay || !pCurrentSeg->m_fSongMode)
                    {
                        // the new segment wants to play on top of stuff that's
                        // already been transported by the current primary segment.
                        pCurrentSeg->AbortPlay(pSeg->m_mtResolvedStart-1,TRUE && (pSeg->m_dwPlaySegFlags & DMUS_SEGF_NOINVALIDATE));
                        m_SegStateQueues[SQ_PRI_DONE].Insert(m_SegStateQueues[SQ_PRI_PLAY].RemoveHead());
                        // make sure none of the last played times in the past list
                        // are past the resolved start
                        for( CSegState* pSegTemp = m_SegStateQueues[SQ_PRI_DONE].GetHead();
                            pSegTemp; pSegTemp = pSegTemp->GetNext() )
                        {
                            if( pSegTemp->m_mtLastPlayed > pSeg->m_mtResolvedStart )
                            {
                                pSegTemp->m_mtLastPlayed = pSeg->m_mtResolvedStart;
                            }
                        }
                        if( !( pSeg->m_dwPlaySegFlags & (DMUS_SEGF_NOINVALIDATE | DMUS_SEGF_INVALIDATE_PRI) ) )
                        {
                            // if we set the PREPARE flag it means we specifically
                            // don't want to invalidate
                            Invalidate( pSeg->m_mtResolvedStart, pSeg->m_dwPlaySegFlags );
                        }
                        else if ( (pSeg->m_dwPlaySegFlags & DMUS_SEGF_INVALIDATE_PRI) &&
                                 !(pSeg->m_dwPlaySegFlags & DMUS_SEGF_NOINVALIDATE) )
                        {
                            pCurrentSeg->Flush(pSeg->m_mtResolvedStart);
                        }
                        ASSERT( m_SegStateQueues[SQ_PRI_PLAY].GetHead() == pSeg ); // this should be the case
                        if( fInCrSec )
                        {
                            LeaveCriticalSection(&m_PipelineCrSec);
                            fInCrSec = FALSE;
                        }
                        SyncTimeSig( pSeg );
                        ManageControllingTracks();
                        PerformSegStNode( SQ_PRI_PLAY,m_SegStateQueues[SQ_PRI_PLAY].GetHead() );
                    }
                }
                else
                {
                    if( !( pSeg->m_dwPlaySegFlags & (DMUS_SEGF_NOINVALIDATE | DMUS_SEGF_INVALIDATE_PRI) ))
                    {
                        Invalidate( pSeg->m_mtResolvedStart, pSeg->m_dwPlaySegFlags );
                    }
                    else if ( (pSeg->m_dwPlaySegFlags & DMUS_SEGF_INVALIDATE_PRI) &&
                             !(pSeg->m_dwPlaySegFlags & DMUS_SEGF_NOINVALIDATE) )
                    {
                        pCurrentSeg->Flush(pSeg->m_mtResolvedStart);
                    }
                }
            }
        }
        else
        {
            m_SegStateQueues[SQ_PRI_PLAY].AddHead(pSeg);
            // give this a chance to start performing if it's near
            // enough to time
            if( fInCrSec )
            {
                LeaveCriticalSection(&m_PipelineCrSec);
                fInCrSec = FALSE;
            }
            //DWORD dwDebugTime = timeGetTime();
            SyncTimeSig( pSeg );
            //DWORD dwDebugTime2 = timeGetTime();
            //Trace(0, "perf, debugtime SyncTimeSig %u\n", dwDebugTime2 - dwDebugTime);

            ManageControllingTracks();
            //dwDebugTime = timeGetTime();
            //Trace(0, "perf, debugtime ManageControllingTracks %u\n", dwDebugTime - dwDebugTime2);

            PerformSegStNode( SQ_PRI_PLAY,pSeg );
            //dwDebugTime2 = timeGetTime();
            //Trace(0, "perf, debugtime PerformSegStNode %u\n", dwDebugTime2 - dwDebugTime);
        }
    }
    if( fInCrSec )
    {
        LeaveCriticalSection(&m_PipelineCrSec);
    }
}

// this function should only be called from within a SegmentCrSec
// critical section!
void CPerformance::QueueSecondarySegment( CSegState* pSeg)
{
    BOOL fInCrSec = FALSE;
    BOOL fNotDone = TRUE;

    if( pSeg->m_dwPlaySegFlags & DMUS_SEGF_CONTROL )
    {
        EnterCriticalSection(&m_PipelineCrSec);
        fInCrSec = TRUE;
    }
    pSeg->m_dwPlaySegFlags &= ~DMUS_SEGF_QUEUE; // not legal for 2ndary segs.
    if( pSeg->m_rtGivenStart == 0 )
    {
        MUSIC_TIME mtStart;

        if( pSeg->m_dwPlaySegFlags & DMUS_SEGF_CONTROL )
        {
            REFERENCE_TIME rtStart;
            GetQueueTime( &rtStart ); // need queue time because control segments cause invalidations
            ReferenceToMusicTime( rtStart, &mtStart );
        }
        else
        {
            ReferenceToMusicTime( GetBestSegLatency(pSeg), &mtStart );
        }
        pSeg->m_dwPlaySegFlags &= ~DMUS_SEGF_REFTIME;
        pSeg->m_rtGivenStart = mtStart;
    }

    if( pSeg->m_dwPlaySegFlags & DMUS_SEGF_REFTIME )
    {
        // rtStartTime is in RefTime units.
        // We can convert this to Music Time immediately if either there
        // is no currently playing Primary Segment, or the conversion
        // falls within the time that has already played. If the time
        // falls within PREPARE_TIME, we need to get this Segment
        // playing right away.
        REFERENCE_TIME rtNow;
        if( pSeg->m_dwPlaySegFlags & DMUS_SEGF_CONTROL )
        {
            GetQueueTime( &rtNow ); // need queue time because control segments cause invalidations
        }
        else
        {
            rtNow = GetBestSegLatency(pSeg);
        }
        MUSIC_TIME mtTime;
        if( pSeg->m_rtGivenStart <= rtNow )
        {
            ReferenceToMusicTime( rtNow, &mtTime );
            pSeg->m_dwPlaySegFlags &= ~( DMUS_SEGF_REFTIME );
            pSeg->m_rtGivenStart = mtTime;
            // let the block of code below that handles music time
            // deal with it from here on
        }
        else if( m_SegStateQueues[SQ_PRI_PLAY].IsEmpty() )
        {
            ReferenceToMusicTime( pSeg->m_rtGivenStart, &mtTime );
            pSeg->m_dwPlaySegFlags &= ~( DMUS_SEGF_REFTIME );
            pSeg->m_rtGivenStart = mtTime;
        }
        else
        {
            // Otherwise, we must wait until rtStartTime
            // has been performed in order to convert to music time, because
            // we require the tempo map at that time to do the conversion.
            // This will be handled by the Transport code.
            m_SegStateQueues[SQ_SEC_WAIT].Insert(pSeg);
            fNotDone = FALSE; // prevents the next block of code from operating on
                        // this Segment.
        }
    }

    if( fNotDone ) // music time
    {
        // if we're in music time units, we can queue this segment in the
        // main queue, in time order. If this segment's music time is less
        // than the start time of other segments in the queue, all of those
        // segments are removed and discarded.
        ASSERT( !(pSeg->m_dwPlaySegFlags & DMUS_SEGF_REFTIME )); // m_m_rtGivenStart must be in music time
        CalculateSegmentStartTime( pSeg );
        TraceI(2,"Queuing 2ndary seg time %ld\n",pSeg->m_mtResolvedStart);
        if( pSeg->m_dwPlaySegFlags & DMUS_SEGF_CONTROL)
        {
            m_SegStateQueues[SQ_CON_PLAY].Insert( pSeg );
            // If this is a control segment, we need to do an invalidate.
            if(!(pSeg->m_dwPlaySegFlags & DMUS_SEGF_NOINVALIDATE) )
            {
                ManageControllingTracks();
                Invalidate( pSeg->m_mtResolvedStart, 0 );
            }
        }
        else
        {
            m_SegStateQueues[SQ_SEC_PLAY].Insert( pSeg );
        }
        // give this a chance to start performing if it's near
        // enough to time
        if( fInCrSec )
        {
            LeaveCriticalSection(&m_PipelineCrSec);
            fInCrSec = FALSE;
        }
        // play the secondary segments
        CSegState *pNode = m_SegStateQueues[SQ_SEC_PLAY].GetHead();
        CSegState *pNext;
        for(; pNode != NULL; pNode = pNext)
        {
            pNext = pNode->GetNext();
            PerformSegStNode( SQ_SEC_PLAY,pNode );
        }
        // play the controlling segments
        pNode = m_SegStateQueues[SQ_CON_PLAY].GetHead();
        for(; pNode != NULL; pNode = pNext)
        {
            pNext = pNode->GetNext();
            PerformSegStNode( SQ_CON_PLAY,pNode );
        }
    }
    if( fInCrSec )
    {
        LeaveCriticalSection(&m_PipelineCrSec);
    }
}

/*  If a segment is controlling, this establishes which tracks in the currently playing
    primary segment are disabled.
    We store temporary information in each playing track's m_dwInternalFlags, which is not used
    otherwise in segmentstates.

    Four scenarios, each for play and notify:
    1) An officially enabled track is currently enabled and gets disabled.
    2) An officially enabled track is currently disabled and continues to be disabled.
    3) An officially enabled track is currently disabled and gets enabled.
    4) An officially disabled track is left disabled. If none of the CONTROL_ flags are set and the track is disabled,
       set the _WAS_DISABLED flag, which also indicates that this should be left alone.

    This should get called every time a primary or secondary segment starts or stop, so it
    can recalculate the behavior of all tracks in the primary segment.
*/

void CPerformance::ManageControllingTracks()

{
    EnterCriticalSection(&m_SegmentCrSec);
    CSegState* pSegNode;
    // First, prepare all tracks in the primary segment, putting them back to normal.
    // so they are ready to be reset by the controlling tracks.
    // To do this, check for WAS_ENABLED or WAS_DISABLED and set the appropriate flags in m_dwFlags.
    // Else, if these weren't set, then it's time to set them, since this is the first pass through this segment.
    for( pSegNode = m_SegStateQueues[SQ_PRI_PLAY].GetHead(); pSegNode; pSegNode = pSegNode->GetNext() )
    {
        EnterCriticalSection(&pSegNode->m_CriticalSection);
        CTrack *pTrack = pSegNode->m_TrackList.GetHead();
        for (;pTrack;pTrack = pTrack->GetNext())
        {
            if (pTrack->m_dwInternalFlags) // This has been touched before.
            {
                // First transfer and reset the is disabled flags.
                if (pTrack->m_dwInternalFlags & CONTROL_PLAY_IS_DISABLED)
                {
                    pTrack->m_dwInternalFlags |= CONTROL_PLAY_WAS_DISABLED;
                }
                pTrack->m_dwInternalFlags &= ~(CONTROL_PLAY_IS_DISABLED | CONTROL_NTFY_IS_DISABLED);
                // Then, set the play flags based on the original state.
                if (pTrack->m_dwInternalFlags & CONTROL_PLAY_DEFAULT_ENABLED)
                {
                    pTrack->m_dwFlags |= DMUS_TRACKCONFIG_PLAY_ENABLED;
                }
                if (pTrack->m_dwInternalFlags & CONTROL_NTFY_DEFAULT_ENABLED)
                {
                    pTrack->m_dwFlags |= DMUS_TRACKCONFIG_NOTIFICATION_ENABLED;
                }
            }
            else
            {
                // Since this has never been touched before, set the flags so we can know what to return to.
                if (pTrack->m_dwFlags & DMUS_TRACKCONFIG_PLAY_ENABLED)
                {
                    pTrack->m_dwInternalFlags = CONTROL_PLAY_DEFAULT_ENABLED;
                }
                else
                {
                    pTrack->m_dwInternalFlags = CONTROL_PLAY_DEFAULT_DISABLED;
                }
                if (pTrack->m_dwFlags & DMUS_TRACKCONFIG_NOTIFICATION_ENABLED)
                {
                    pTrack->m_dwInternalFlags |= CONTROL_NTFY_DEFAULT_ENABLED;
                }
                else
                {
                    pTrack->m_dwInternalFlags |= CONTROL_NTFY_DEFAULT_DISABLED;
                }
            }
        }
        LeaveCriticalSection(&pSegNode->m_CriticalSection);
    }
    CSegState* pControlNode;
    // Now, go through all the controlling segments and, for each controlling track that matches
    // a primary segment track, clear the enable flags on the segment track.
    for( pControlNode = m_SegStateQueues[SQ_CON_PLAY].GetHead(); pControlNode; pControlNode = pControlNode->GetNext() )
    {
        EnterCriticalSection(&pControlNode->m_CriticalSection);
        CTrack *pTrack = pControlNode->m_TrackList.GetHead();
        for (;pTrack;pTrack = pTrack->GetNext())
        {
            // If the track has never been overridden, the internal flags for IS_DISABLED should be clear.
            // If the track is currently overridden, the internal flags should be CONTROL_PLAY_IS_DISABLED and/or
            // CONTROL_NTFY_IS_DISABLED
            if (pTrack->m_dwFlags & (DMUS_TRACKCONFIG_CONTROL_PLAY | DMUS_TRACKCONFIG_CONTROL_NOTIFICATION)) // This overrides playback and/or notification.
            {
                for( pSegNode = m_SegStateQueues[SQ_PRI_PLAY].GetHead(); pSegNode; pSegNode = pSegNode->GetNext() )
                {
                    EnterCriticalSection(&pSegNode->m_CriticalSection);
                    CTrack *pPrimaryTrack = pSegNode->m_TrackList.GetHead();
                    for (;pPrimaryTrack;pPrimaryTrack = pPrimaryTrack->GetNext())
                    {
                        // A track matches if it has the same class id and overlapping group bits.
                        if ((pPrimaryTrack->m_guidClassID == pTrack->m_guidClassID) &&
                            (pPrimaryTrack->m_dwGroupBits & pTrack->m_dwGroupBits))
                        {
                            if ((pTrack->m_dwFlags & DMUS_TRACKCONFIG_CONTROL_PLAY) &&
                                (pPrimaryTrack->m_dwFlags & DMUS_TRACKCONFIG_PLAY_ENABLED))
                            {
                                pPrimaryTrack->m_dwFlags &= ~DMUS_TRACKCONFIG_PLAY_ENABLED;
                                pPrimaryTrack->m_dwInternalFlags |= CONTROL_PLAY_IS_DISABLED; // Mark so we can turn on later.
                            }
                            if ((pTrack->m_dwFlags & DMUS_TRACKCONFIG_CONTROL_NOTIFICATION) &&
                                (pPrimaryTrack->m_dwFlags & DMUS_TRACKCONFIG_NOTIFICATION_ENABLED))
                            {
                                pPrimaryTrack->m_dwFlags &= ~DMUS_TRACKCONFIG_NOTIFICATION_ENABLED;
                                pPrimaryTrack->m_dwInternalFlags |= CONTROL_NTFY_IS_DISABLED; // Mark so we can turn on later.
                            }
                        }
                    }
                    LeaveCriticalSection(&pSegNode->m_CriticalSection);
                }
            }
        }
        LeaveCriticalSection(&pControlNode->m_CriticalSection);
    }
    // Now, go back to the primary segment and find all tracks that have been reenabled
    // and tag them so they will generate refresh data on the next play (by seeking, as if they
    // were starting or looping playback.) We only do this for play, not notify, because no
    // notifications have state.
    for( pSegNode = m_SegStateQueues[SQ_PRI_PLAY].GetHead(); pSegNode; pSegNode = pSegNode->GetNext() )
    {
        EnterCriticalSection(&pSegNode->m_CriticalSection);
        CTrack *pTrack = pSegNode->m_TrackList.GetHead();
        for (;pTrack;pTrack = pTrack->GetNext())
        {
            if ((pTrack->m_dwInternalFlags & CONTROL_PLAY_DEFAULT_ENABLED) &&
                (pTrack->m_dwInternalFlags & CONTROL_PLAY_WAS_DISABLED) &&
                !(pTrack->m_dwInternalFlags & CONTROL_PLAY_IS_DISABLED))
            {
                pTrack->m_dwInternalFlags |= CONTROL_PLAY_REFRESH; // Mark so we can turn on later.
            }
        }
        LeaveCriticalSection(&pSegNode->m_CriticalSection);
    }
    LeaveCriticalSection(&m_SegmentCrSec);
}

void CPerformance::GetTimeSig( MUSIC_TIME mtTime, DMUS_TIMESIG_PMSG* pTimeSig )
{
    EnterCriticalSection(&m_PipelineCrSec);
    PRIV_PMSG* pEvent = m_TimeSigQueue.GetHead();
    for (;pEvent;pEvent = pEvent->pNext)
    {
        // If this is the last time sig, return it. Or, if the next time sig is after mtTime.
        if (!pEvent->pNext || ( pEvent->pNext->mtTime > mtTime ))
        {
            DMUS_TIMESIG_PMSG* pNewTimeSig = (DMUS_TIMESIG_PMSG*)PRIV_TO_DMUS(pEvent);
            memcpy( pTimeSig, pNewTimeSig, sizeof(DMUS_TIMESIG_PMSG) );
            LeaveCriticalSection(&m_PipelineCrSec);
            return;
        }
    }
    // This should only happen if there is no timesig at all. Should only happen before any segments play.
    memset( pTimeSig, 0, sizeof(DMUS_TIMESIG_PMSG ) );
    pTimeSig->wGridsPerBeat = 4;
    pTimeSig->bBeatsPerMeasure = 4;
    pTimeSig->bBeat = 4;
    LeaveCriticalSection(&m_PipelineCrSec);
}

void CPerformance::SyncTimeSig( CSegState *pSegState )

/*  If a primary segment is played that does not have a time signature track,
    this forces the current time signature to line up with the start of the
    primary segment.
*/

{
    // First, test to see if the segment has a time signature.
    // If it doesn't then we need to do this.
    DMUS_TIMESIGNATURE TimeSig;
    if (FAILED(pSegState->GetParam(this,GUID_TimeSignature,-1,0,0,NULL,(void *)&TimeSig)))
    {
        MUSIC_TIME mtTime = pSegState->m_mtResolvedStart;
        EnterCriticalSection(&m_PipelineCrSec);
        PRIV_PMSG* pEvent = m_TimeSigQueue.GetHead();
        // Scan through the time signatures until the most recent one is found.
        for (;pEvent;pEvent = pEvent->pNext)
        {
            // If this is the last time sig, return it. Or, if the next time sig is after mtTime.
            if (!pEvent->pNext || ( pEvent->pNext->mtTime > mtTime ))
            {
                pEvent->mtTime = mtTime;
                MusicToReferenceTime(mtTime,&pEvent->rtTime);
                break;
            }
        }
        // Should never fall through to here without finding a time signature because Init() creates a timesig.
        LeaveCriticalSection(&m_PipelineCrSec);
    }
}

// Convert mtTime into the resolved time according to the resolution in
// dwResolution.
// This should only be called from within a segment critical section.
MUSIC_TIME CPerformance::ResolveTime( MUSIC_TIME mtTime, DWORD dwResolution, MUSIC_TIME *pmtIntervalSize )
{
    if (pmtIntervalSize)
    {
        *pmtIntervalSize = 0;
    }
    if (dwResolution & DMUS_SEGF_MARKER)
    {
        DMUS_PLAY_MARKER_PARAM Marker;
        MUSIC_TIME mtNext;
        // First, get the time of the marker preceding this one.
        if (SUCCEEDED (GetParam(GUID_Play_Marker,-1,0,mtTime,&mtNext,(void *) &Marker)))
        {
            BOOL fIsMarker = FALSE;
            MUSIC_TIME mtInitialTime = mtTime;
            MUSIC_TIME mtFirst = mtTime + Marker.mtTime; // This is the time of the preceding marker.
            MUSIC_TIME mtSecond = mtTime + mtNext;  // This might be the time of the next marker.
            // Then, scan forward until a marker is found after or equal to this time.
            // If a loop point or end of segment is encountered, the value in Marker.mtTime will
            // continue to be negative. Once we hit the actual marker, it will become 0, since
            // we are asking for the marker at that specific time.
            while (mtNext)
            {
                mtTime += mtNext;
                if (SUCCEEDED(GetParam(GUID_Play_Marker,-1,0,mtTime,&mtNext,(void *) &Marker)))
                {
                    // If the marker time is 0, this means we are sitting right on the marker,
                    // so we are done.
                    if (fIsMarker = (Marker.mtTime == 0))
                    {
                        mtSecond = mtTime;
                        break;
                    }
                    // Otherwise, this was a loop boundary or segment end, so we should continue scanning forward.
                }
                else
                {
                    // GetParam failed, must be nothing more to search.
                    break;
                }
            }
            // If the caller wants the interval size, then we know they are interested in
            // aligning to a previous marker as well as a future one. In that case,
            // if we didn't find a marker in the future, it's okay because it will
            // use the previous marker (mtFirst) anyway.
            // For all other cases, we only return if the upcoming marker is legal.
            // Otherwise, we drop through and try other resolutions.
            if (pmtIntervalSize || fIsMarker)
            {
                if (pmtIntervalSize)
                {
                    *pmtIntervalSize = mtSecond - mtFirst;
                }
                return mtSecond;
            }
            mtTime = mtInitialTime;
        }
        // If marker fails, we can drop down to the other types...
    }
    if( dwResolution & DMUS_SEGF_SEGMENTEND )
    {
        // In this mode, we don't actually get the time signature. Instead, we
        // find out the time of the next segment start after the requested time.
        CSegState *pSegNode = GetPrimarySegmentAtTime( mtTime );
        if( pSegNode )
        {
            // First, calculate the end time of the segment.
            // Include any starting offset so we see the full span of the segment.
            mtTime = pSegNode->GetEndTime( pSegNode->m_mtStartPoint );
            if (pmtIntervalSize)
            {
                // Interval would be the length of the primary segment!
                *pmtIntervalSize = mtTime;
            }
            // Return the end of the segment.
            LONGLONG llEnd = mtTime + (LONGLONG)(pSegNode->m_mtResolvedStart - pSegNode->m_mtStartPoint);
            if(llEnd > 0x7fffffff) llEnd = 0x7fffffff;
            mtTime = (MUSIC_TIME) llEnd;
            return mtTime;
        }
        // If there was no segment, we should fail and try the other flags.
    }
    long        lQuantize;
    MUSIC_TIME  mtNewTime;
    MUSIC_TIME  mtStartOfTimeSig = 0;
    DMUS_TIMESIGNATURE  timeSig;
    if (!(dwResolution & DMUS_SEGF_TIMESIG_ALWAYS))
    {
        if (!GetPrimarySegmentAtTime(mtTime))
        {
            return mtTime;
        }
    }
    GetParam(GUID_TimeSignature,-1,0,mtTime,NULL,(void *) &timeSig);
    mtStartOfTimeSig = timeSig.mtTime + mtTime;
    mtNewTime = mtTime - mtStartOfTimeSig;
    if (dwResolution & DMUS_SEGF_MEASURE)
    {
        lQuantize = ( DMUS_PPQ * 4 * timeSig.bBeatsPerMeasure ) / timeSig.bBeat;
    }
    else if (dwResolution & DMUS_SEGF_BEAT)
    {
        lQuantize = ( DMUS_PPQ * 4 ) / timeSig.bBeat;
    }
    else if (dwResolution & DMUS_SEGF_GRID)
    {
        lQuantize = ( ( DMUS_PPQ * 4 ) / timeSig.bBeat ) / timeSig.wGridsPerBeat;
    }
    else
    {
        lQuantize = 1;
    }
    if (lQuantize == 0) // Avoid divide by 0 error.
    {
        lQuantize = 1;
    }
    if (pmtIntervalSize)
    {
        *pmtIntervalSize = lQuantize;
    }
    if( mtNewTime ) // if it's 0 it stays 0
    {
        // round up to next boundary
        mtNewTime = ((mtNewTime-1) / lQuantize ) * lQuantize;
        mtNewTime += lQuantize;
    }
    return (mtNewTime + mtStartOfTimeSig);
}

// returns:
// true if the note should be invalidated (any other return code will invalidate)
// false if the note should not be invalidated
inline bool GetInvalidationStatus(DMUS_PMSG* pPMsg)
{
    bool fResult = true; // default: invalidate the note

    if( pPMsg->dwType == DMUS_PMSGT_NOTE )
    {
        DMUS_NOTE_PMSG* pNote = (DMUS_NOTE_PMSG*)pPMsg;
        if (pNote->bFlags & DMUS_NOTEF_NOINVALIDATE)
        {
            fResult = false;
        }
    }
    else if( pPMsg->dwType == DMUS_PMSGT_WAVE )
    {
        DMUS_WAVE_PMSG* pWave = (DMUS_WAVE_PMSG*)pPMsg;
        if(pWave->bFlags & DMUS_WAVEF_NOINVALIDATE)
        {
            fResult = false;
        }
    }
    else if( pPMsg->dwType == DMUS_PMSGT_NOTIFICATION )
    {
        // Don't invalidate segment abort messages
        DMUS_NOTIFICATION_PMSG* pNotification = (DMUS_NOTIFICATION_PMSG*) pPMsg;
        if ((pNotification->guidNotificationType == GUID_NOTIFICATION_SEGMENT) &&
            (pNotification->dwNotificationOption == DMUS_NOTIFICATION_SEGABORT))
        {
            fResult = false;
        }
    }
    return fResult;
}

static inline long ComputeCurveTimeSlice(DMUS_CURVE_PMSG* pCurve)
{
    long lTimeIncrement;
    DWORD dwTotalDistance;
    DWORD dwResolution;
    if ((pCurve->bType == DMUS_CURVET_PBCURVE) ||
        (pCurve->bType == DMUS_CURVET_RPNCURVE) ||
        (pCurve->bType == DMUS_CURVET_NRPNCURVE))
    {
        dwResolution = 100;
    }
    else
    {
        dwResolution = 3;
    }
    if (pCurve->nEndValue > pCurve->nStartValue)
        dwTotalDistance = pCurve->nEndValue - pCurve->nStartValue;
    else
        dwTotalDistance = pCurve->nStartValue - pCurve->nEndValue;
    if (dwTotalDistance == 0) dwTotalDistance = 1;
    lTimeIncrement = (pCurve->mtDuration * dwResolution) / dwTotalDistance;
    // Force to no smaller than 192nd note (10ms at 120 bpm.)
    if( lTimeIncrement < (DMUS_PPQ/48) ) lTimeIncrement = DMUS_PPQ/48;
    return lTimeIncrement;
}

static DWORD ComputeCurve( DMUS_CURVE_PMSG* pCurve )
{
    DWORD dwRet;
    short *panTable;
    MUSIC_TIME mtCurrent;
    long lIndex;

    switch( pCurve->bCurveShape )
    {
    case DMUS_CURVES_INSTANT:
    default:
        if( pCurve->dwFlags & DMUS_PMSGF_TOOL_FLUSH )
        {
            pCurve->rtTime = 0;
            return (DWORD)pCurve->nResetValue;
        }
        if( ( pCurve->bFlags & DMUS_CURVE_RESET ) && ( pCurve->mtResetDuration > 0 ) )
        {
            pCurve->mtTime = pCurve->mtResetDuration + pCurve->mtOriginalStart;
            pCurve->mtDuration = 0;
            pCurve->dwFlags &= ~DMUS_PMSGF_REFTIME;
        }
        else
        {
            pCurve->rtTime = 0; // setting this to 0 will free the event upon return
        }
        return (DWORD)pCurve->nEndValue;
        break;
    case DMUS_CURVES_LINEAR:
        panTable = &ganCT_Linear[ 0 ];
        break;
    case DMUS_CURVES_EXP:
        panTable = &ganCT_Exp[ 0 ];
        break;
    case DMUS_CURVES_LOG:
        panTable = &ganCT_Log[ 0 ];
        break;
    case DMUS_CURVES_SINE:
        panTable = &ganCT_Sine[ 0 ];
        break;
    }

    // compute index into table
    // there are CT_MAX + 1 elements in the table.
    mtCurrent = pCurve->mtTime - pCurve->mtOriginalStart;

    // if we're flushing this event, send the reset value
    if( pCurve->dwFlags & DMUS_PMSGF_TOOL_FLUSH )
    {
        // it will only get here if pCurve->bFlags & 1, because that is checked in
        // the :Flush() routine.
        pCurve->rtTime = 0;
        return pCurve->nResetValue;
    }

    // this should now never happen, as a result of fixing 33987: Transition on a beat boundary invalidates CC's right away (doesn't wait for the beat)
    if( (pCurve->bFlags & DMUS_CURVE_RESET) &&
        (pCurve->mtResetDuration < 0 ) && // this can happen from flushing
        (pCurve->mtTime >= pCurve->mtOriginalStart + pCurve->mtDuration + pCurve->mtResetDuration ))
    {
        pCurve->rtTime = 0;
        return pCurve->nResetValue;
    }
    else if( (pCurve->mtDuration == 0) ||
        (pCurve->mtTime - pCurve->mtOriginalStart >= pCurve->mtDuration ))
    {
        // if we're supposed to send the return value (m_bFlags & 1) then
        // set it up to do so. Otherwise, free the event.
        if( pCurve->bFlags & DMUS_CURVE_RESET )
        {
            pCurve->mtTime = pCurve->mtDuration + pCurve->mtResetDuration +
                pCurve->mtOriginalStart;
            pCurve->dwFlags &= ~DMUS_PMSGF_REFTIME;
        }
        else
        {
            pCurve->rtTime = 0; // time to free the event, we're done
        }
        dwRet = pCurve->nEndValue;
    }
    else
    {
        // Calculate how far into the table we should be.
        lIndex = (mtCurrent * (CT_MAX + 1)) / pCurve->mtDuration;

        // find an amount of time to add to the curve event such that there is at
        // least a change by CT_FACTOR. This will be used as the time stamp
        // for the next iteration of the curve.

        // clamp lIndex
        if( lIndex < 0 )
        {
            lIndex = 0;
        }
        if( lIndex >= CT_MAX )
        {
            lIndex = CT_MAX;
            dwRet = pCurve->nEndValue;
        }
        else
        {
            // Okay, in the curve, so calculate the return value.
            dwRet = ((panTable[lIndex] * (pCurve->nEndValue - pCurve->nStartValue)) /
                CT_DIVFACTOR) + pCurve->nStartValue;
        }

        // this should now never happen, as a result of fixing 33987
        if( (pCurve->bFlags & DMUS_CURVE_RESET) && (pCurve->mtResetDuration < 0) )
        {
            // this can happen as a result of flushing. We want to make sure the next
            // time is the reset flush time.
            pCurve->mtTime = pCurve->mtDuration + pCurve->mtResetDuration +
                pCurve->mtOriginalStart;
        }
        else
        {
            // Within curve, so increment time.
            if (!pCurve->wMeasure) // oops --- better compute this.
            {
                TraceI(2, "Warning: Computing curve time slice...\n");
                pCurve->wMeasure = (WORD) ComputeCurveTimeSlice(pCurve);  // Use this to store the time slice interval.
            }
            pCurve->mtTime += pCurve->wMeasure; // We are storing the time increment here.
        }
        if( pCurve->mtTime > pCurve->mtDuration + pCurve->mtOriginalStart )
        {
            pCurve->mtTime = pCurve->mtDuration + pCurve->mtOriginalStart;
        }
        pCurve->dwFlags &= ~DMUS_PMSGF_REFTIME;

    }
    return dwRet;
}

static int RecomputeCurveEnd( DMUS_CURVE_PMSG* pCurve, MUSIC_TIME mtCurrent )
{
    int nRet = 0;
    short *panTable;

    switch( pCurve->bCurveShape )
    {
    case DMUS_CURVES_INSTANT:
    default:
        return pCurve->nEndValue;
        break;
    case DMUS_CURVES_LINEAR:
        panTable = &ganCT_Linear[ 0 ];
        break;
    case DMUS_CURVES_EXP:
        panTable = &ganCT_Exp[ 0 ];
        break;
    case DMUS_CURVES_LOG:
        panTable = &ganCT_Log[ 0 ];
        break;
    case DMUS_CURVES_SINE:
        panTable = &ganCT_Sine[ 0 ];
        break;
    }

    if( (pCurve->mtDuration == 0) || (mtCurrent >= pCurve->mtDuration ))
    {
        return pCurve->nEndValue;
    }
    else
    {
        // Calculate how far into the table we should be.
        long lIndex = (mtCurrent * (CT_MAX + 1)) / pCurve->mtDuration;

        // find an amount of time to add to the curve event such that there is at
        // least a change by CT_FACTOR. This will be used as the time stamp
        // for the next iteration of the curve.

        // clamp lIndex
        if( lIndex < 0 )
        {
            lIndex = 0;
        }
        if( lIndex >= CT_MAX )
        {
            lIndex = CT_MAX;
            nRet = pCurve->nEndValue;
        }
        else
        {
            // Okay, in the curve, so calculate the return value.
            nRet = ((panTable[lIndex] * (pCurve->nEndValue - pCurve->nStartValue)) /
                CT_DIVFACTOR) + pCurve->nStartValue;
        }
    }
    return nRet;
}

void CPerformance::FlushEventQueue( DWORD dwId,
    CPMsgQueue *pQueue,                 // Queue to flush events from.
    REFERENCE_TIME rtFlush,             // Time that flush occurs. This may be resolved to a timing resolution.
    REFERENCE_TIME rtFlushUnresolved,   // Queue time at time flush was requested. This is not resolved to the timing resolution.
                                        // Instead, it is the actual time at which that the flush was requested. This is used only by curves.
    BOOL fLeaveNotesOn)                 // If notes or waves are currently on, do not cut short their durations.
{
    PRIV_PMSG* pEvent;
    PRIV_PMSG* pNext;
    HRESULT hr = S_OK;

    REFERENCE_TIME rtTemp;
    GetQueueTime(&rtTemp);
    pNext = NULL;
    for(pEvent = pQueue->GetHead(); pEvent; pEvent = pNext )
    {
        pNext = pEvent->pNext;
        // Clear the remove bit. This will be set for each event that should be removed from the queue.
        pEvent->dwPrivFlags &= ~PRIV_FLAG_REMOVE;
        // Also clear the requeue bit, which will be set for each event that needs to be requeued.
        pEvent->dwPrivFlags &= ~PRIV_FLAG_REQUEUE;
        if( ( 0 == dwId ) || ( pEvent->dwVirtualTrackID == dwId ) )
        {
            // First, create the correct mtTime and rtTime for invalidation.
            REFERENCE_TIME rtTime = pEvent->rtTime;
            if( pEvent->dwType == DMUS_PMSGT_NOTE )
            {
                DMUS_NOTE_PMSG* pNote = (DMUS_NOTE_PMSG*)PRIV_TO_DMUS(pEvent);
                if( pNote->bFlags & DMUS_NOTEF_NOTEON )
                {
                    // If this is a note on, we want to take the offset into consideration for
                    // determining whether or not to invalidate.
                    MUSIC_TIME mtNote = pNote->mtTime - pNote->nOffset;
                    MusicToReferenceTime( mtNote, &rtTime );
                }
                // If note off and we want to leave notes playing, turn on the noinvalidate flag.
                else if (fLeaveNotesOn)
                {
                    pNote->bFlags |= DMUS_NOTEF_NOINVALIDATE;
                }
            }
            else if( pEvent->dwType == DMUS_PMSGT_WAVE )
            {
                DMUS_WAVE_PMSG* pWave = (DMUS_WAVE_PMSG*)PRIV_TO_DMUS(pEvent);
                if( !(pWave->bFlags & DMUS_WAVEF_OFF) )
                {
                    if (pWave->dwFlags & DMUS_PMSGF_LOCKTOREFTIME)
                    {
                        rtTime = pWave->rtTime;
                    }
                    else
                    {
                        MusicToReferenceTime(pWave->mtTime, &rtTime);
                    }
                }
                // If wave off and we want to leave waves playing, turn on the noinvalidate flag.
                else if (fLeaveNotesOn)
                {
                    pWave->bFlags |= DMUS_WAVEF_NOINVALIDATE;
                }
            }
            else if( pEvent->dwType == DMUS_PMSGT_CURVE )
            {
                if (fLeaveNotesOn)
                {
                    rtTime = 0;
                }
                else
                {
                    DMUS_CURVE_PMSG* pCurve = (DMUS_CURVE_PMSG*)PRIV_TO_DMUS(pEvent);
                    MUSIC_TIME mtCurve;
                    MUSIC_TIME mtStart;
                    mtStart = pCurve->mtOriginalStart ? pCurve->mtOriginalStart : pCurve->mtTime;

                    // if rtFlush is before the beginning of the curve minus the offset of
                    // the curve, we want to prevent the curve from playing
                    mtCurve = mtStart - pCurve->nOffset;
                    MusicToReferenceTime( mtCurve, &rtTime );
                    if( rtFlush > rtTime ) // if it isn't...
                    {
                        // if the curve has a reset value and has already begun,
                        // we may want to flush right away.
                        if( ( pCurve->bFlags & DMUS_CURVE_RESET) &&
                              pCurve->mtOriginalStart &&
                              rtFlush <= rtFlushUnresolved )
                        {
                            mtCurve = mtStart + pCurve->mtDuration;
                            MusicToReferenceTime( mtCurve, &rtTime );
                            if( rtTime >= rtFlush && !(pEvent->dwPrivFlags & PRIV_FLAG_FLUSH) )
                            {
                                MUSIC_TIME mt = 0;
                                ReferenceToMusicTime(rtFlush, &mt);
                                pCurve->mtDuration = (mt - mtStart) - 1;
                                pCurve->mtResetDuration = 1;
                            }
                            else
                            {
                                mtCurve = mtStart + pCurve->mtDuration + pCurve->mtResetDuration;
                                MusicToReferenceTime( mtCurve, &rtTime );
                                if ( rtTime >= rtFlush && !(pEvent->dwPrivFlags & PRIV_FLAG_FLUSH) )
                                {
                                    MUSIC_TIME mt = 0;
                                    ReferenceToMusicTime(rtFlush, &mt);
                                    pCurve->mtResetDuration = mt - (mtStart + pCurve->mtDuration);
                                }
                            }
                        }
                        else
                        {
                            // Otherwise, we may cut the curve short in the code below.
                            rtTime = 0;
                        }
                    }
                }
            }
            // now flush the event if needed
            if( rtTime >= rtFlush )
            {
                if (!(pEvent->dwPrivFlags & PRIV_FLAG_FLUSH))
                {
                    if( pEvent->pTool)
                    {
                        bool fFlush = false;
                        if (pEvent->dwType == DMUS_PMSGT_WAVE)
                        {
                            DMUS_WAVE_PMSG* pWave = (DMUS_WAVE_PMSG*)PRIV_TO_DMUS(pEvent);
                            if( !(pWave->bFlags & DMUS_WAVEF_OFF) )
                            {
                                // this wave on is due to start after the flush time.
                                // we never want to hear it.
                                fFlush = true;
                            }
                            else
                            {
                                // cut the duration short, but don't actually flush here,
                                // since it's possible to invalidate the same wave more
                                // than once, and the second invalidation might have a
                                // time prior to the first one (e.g., first is from a loop,
                                // second is from a transition)
                                if (GetInvalidationStatus(PRIV_TO_DMUS(pEvent)) &&
                                    rtFlush < pWave->rtTime)
                                {
                                    pEvent->dwPrivFlags |= PRIV_FLAG_REQUEUE;
                                    MUSIC_TIME mtFlush = 0;
                                    ReferenceToMusicTime(rtFlush, &mtFlush);
                                    pWave->rtTime = rtFlush;
                                    pWave->mtTime = mtFlush;
                                }
                            }
                        }
                        if (fFlush ||
                            (pEvent->dwType != DMUS_PMSGT_WAVE &&
                             GetInvalidationStatus(PRIV_TO_DMUS(pEvent))) )
                        {
                            pEvent->dwPrivFlags |= PRIV_FLAG_REMOVE;
                            pEvent->dwFlags |= DMUS_PMSGF_TOOL_FLUSH;
                            if( rtFlush <= pEvent->rtLast )
                            {
                                pEvent->pTool->Flush( this, PRIV_TO_DMUS(pEvent), pEvent->rtLast + REF_PER_MIL );
                            }
                            else
                            {
                                pEvent->pTool->Flush( this, PRIV_TO_DMUS(pEvent), rtFlush );
                            }
                        }
                    }
                    else
                    {
                        pEvent->dwPrivFlags |= PRIV_FLAG_REMOVE;
                    }
                }
            }
            else // cut notes, waves, and curves short if needed
            {
                if( pEvent->dwType == DMUS_PMSGT_NOTE && !fLeaveNotesOn )
                {
                    DMUS_NOTE_PMSG* pNote = (DMUS_NOTE_PMSG*)PRIV_TO_DMUS(pEvent);
                    if( pNote->bFlags & DMUS_NOTEF_NOTEON )
                    {
                        if (GetInvalidationStatus(PRIV_TO_DMUS(pEvent)))
                        {
                            // subtract 2 from the duration to guarantee the note cuts short
                            // 1 clock before the flush time.
                            MUSIC_TIME mtNoteOff = pNote->mtTime + pNote->mtDuration - 2;
                            REFERENCE_TIME rtNoteOff;
                            MusicToReferenceTime( mtNoteOff, &rtNoteOff );
                            if( rtNoteOff >= rtFlush )
                            {
                                ReferenceToMusicTime( rtFlush, &mtNoteOff );
                                mtNoteOff -= pNote->mtTime;
                                // Make any duration < 1 be 0; this will cause the note not to
                                // sound.  Can happen if the note's logical time is well before
                                // its physical time.
                                if( mtNoteOff < 1 ) mtNoteOff = 0;
                                pNote->mtDuration = mtNoteOff;
                            }
                        }
                    }
                }
                else if( pEvent->dwType == DMUS_PMSGT_WAVE && !fLeaveNotesOn )
                {
                    DMUS_WAVE_PMSG* pWave = (DMUS_WAVE_PMSG*)PRIV_TO_DMUS(pEvent);
                    if( !(pWave->bFlags & DMUS_WAVEF_OFF) &&
                        (GetInvalidationStatus(PRIV_TO_DMUS(pEvent))) )
                    {
                        if (pWave->dwFlags & DMUS_PMSGF_LOCKTOREFTIME)
                        {
                            // This is a clock time message.
                            // subtract 2 from the duration to guarantee the wave cuts short
                            // 1 clock before the flush time.
                            if ((rtTime + pWave->rtDuration - 2) >= rtFlush)
                            {
                                pWave->rtDuration = rtFlush - rtTime;
                            }

                        }
                        else
                        {
                            MUSIC_TIME mtTime = 0;
                            MUSIC_TIME mtFlush = 0;
                            ReferenceToMusicTime(rtTime, &mtTime);
                            ReferenceToMusicTime(rtFlush, &mtFlush);
                            // subtract 2 from the duration to guarantee the wave cuts short
                            // 1 clock before the flush time.
                            if ((mtTime + (MUSIC_TIME)pWave->rtDuration - 2) >= mtFlush)
                            {
                                pWave->rtDuration = mtFlush - mtTime;
                            }
                        }
                        if (pWave->rtDuration < 1) // disallow durations less than 1. This should never happen anyway.
                        {
                            pWave->rtDuration = 1;
                        }
                    }
                }
                else if( pEvent->dwType == DMUS_PMSGT_CURVE && !fLeaveNotesOn )
                {
                    DMUS_CURVE_PMSG* pCurve = (DMUS_CURVE_PMSG*)PRIV_TO_DMUS(pEvent);
                    MUSIC_TIME mtEnd;
                    MUSIC_TIME mtStart = pCurve->mtOriginalStart ? pCurve->mtOriginalStart : pCurve->mtTime;

                    if( pCurve->bFlags & DMUS_CURVE_RESET )
                    {
                        mtEnd = mtStart + pCurve->mtResetDuration + pCurve->mtDuration;
                    }
                    else
                    {
                        mtEnd = mtStart + pCurve->mtDuration;
                    }
                    REFERENCE_TIME rtEnd;
                    MusicToReferenceTime( mtEnd, &rtEnd );
                    // Note: as a result of fixing 33987, the curve is no longer given
                    // a negative reset duration.  Now, the curve's duration is recomputed
                    // and its time slice is recalculated.
                    if( rtEnd >= rtFlush )
                    {
                        // reset the curve's duration
                        ReferenceToMusicTime( rtFlush, &mtEnd );
                        mtEnd -= mtStart;
                        // get the curve value at the flush time, and make that the end value
                        pCurve->nEndValue = (short) RecomputeCurveEnd(pCurve, mtEnd);
                        // subtract 2 from the duration to guarantee the curve cuts short
                        // 1 clock before the flush time.
                        mtEnd -= 2;
                        if ( mtEnd < 1)
                        {
                            mtEnd = 1;
                        }
                        else if (pCurve->bFlags & DMUS_CURVE_RESET)
                        {
                            if (mtEnd > pCurve->mtDuration)
                            {
                                // curve ends in the reset duration; keep regular duration the
                                // same as it was and adjust reset duration
                                pEvent->dwPrivFlags |= PRIV_FLAG_FLUSH;
                                MUSIC_TIME mt = 0;
                                ReferenceToMusicTime(rtFlush, &mt);
                                pCurve->mtResetDuration = mt - (mtStart + pCurve->mtDuration);
                                mtEnd = pCurve->mtDuration;
                                if (pCurve->mtTime > mtEnd + pCurve->mtResetDuration + mtStart)
                                {
                                    pCurve->mtTime = mtEnd + pCurve->mtResetDuration + mtStart;
                                    MusicToReferenceTime(pCurve->mtTime, &pCurve->rtTime);
                                }
                            }
                            else
                            {
                                // curve ends in the regular duration; reduce it by 1 and
                                // give the reset duration a value of 1
                                mtEnd--;
                                pCurve->mtResetDuration = 1;
                                if (mtEnd < 1)
                                {
                                    // this is unlikely, but the curve really should have
                                    // a duration...
                                    mtEnd = 1;
                                }
                                pEvent->dwPrivFlags |= PRIV_FLAG_FLUSH;
                            }
                            // If this is an instant curve that's already started, we
                            // don't want it to play again, so reset its start time
                            if ( pCurve->bCurveShape == DMUS_CURVES_INSTANT &&
                                 pCurve->mtOriginalStart )
                            {
                                pCurve->mtTime = pCurve->mtResetDuration + pCurve->mtOriginalStart + mtEnd;
                            }
                        }
                        pCurve->mtDuration = mtEnd;
                    }
                }
            }
        }
    }
    // remove (and unmark) all marked PMsgs from the current queue
    for(pEvent = pQueue->GetHead(); pEvent; pEvent = pNext )
    {
        pNext = pEvent->pNext;
        if (pEvent->dwPrivFlags & (PRIV_FLAG_REMOVE | PRIV_FLAG_REQUEUE))
        {
            pEvent->dwPrivFlags &= ~PRIV_FLAG_REMOVE;
            if (pQueue->Dequeue(pEvent))
            {
                if (pEvent->dwPrivFlags & PRIV_FLAG_REQUEUE)
                {
                    pEvent->dwPrivFlags &= ~PRIV_FLAG_REQUEUE;
                    pQueue->Enqueue(pEvent);
                }
                else
                {
                    FreePMsg(pEvent);
                }
            }
            else
            {
                TraceI(0,"Error dequeing event for flushing\n");
            }
        }
    }
    SendBuffers();
}

/*

  Flushes all events in all queues from time <p mtFlush> on.

  comm Only call this from withing a PipelineCrSec critical section!

*/
void CPerformance::FlushMainEventQueues(
    DWORD dwId,                      // Virtual Track ID to flush, or zero for all.
    MUSIC_TIME mtFlush,              // Time to flush (resolved to timing resolution).
    MUSIC_TIME mtFlushUnresolved,    // Time to flush (unresolved).
    BOOL fLeaveNotesOn)              // If true, notes currently on are left to play through their duration.
{
    REFERENCE_TIME rt;
    if( mtFlush )
    {
        MusicToReferenceTime( mtFlush, &rt );
    }
    else
    {
        rt = 0;
    }
    REFERENCE_TIME rtUnresolved;
    if( mtFlushUnresolved && mtFlushUnresolved != mtFlush)
    {
        MusicToReferenceTime( mtFlushUnresolved, &rtUnresolved );
    }
    else
    {
        rtUnresolved = rt;
    }
    FlushEventQueue( dwId, &m_OnTimeQueue, rt, rtUnresolved, fLeaveNotesOn );
    FlushEventQueue( dwId, &m_NearTimeQueue, rt, rtUnresolved, fLeaveNotesOn );
    FlushEventQueue( dwId, &m_EarlyQueue, rt, rtUnresolved, fLeaveNotesOn );
    if (dwId == 0)
    {
        MUSIC_TIME mtTime;
        ReferenceToMusicTime(rt,&mtTime);
        FlushEventQueue( dwId, &m_TempoMap, rt, rtUnresolved, fLeaveNotesOn );
        RecalcTempoMap(NULL, mtTime );
    }
}

// the only kinds of events we care about are note events.
void CPerformance::OnChordUpdateEventQueue( DMUS_NOTIFICATION_PMSG* pNotify, CPMsgQueue *pQueue, REFERENCE_TIME rtFlush )
{
    PRIV_PMSG* pEvent;
    PRIV_PMSG* pNext;
    HRESULT hr = S_OK;
    DWORD dwId = pNotify->dwVirtualTrackID;
    DWORD dwTrackGroup = pNotify->dwGroupID;
    CPMsgQueue UpdateQueue;        // List of PMsgs to be inserted into a queue during update.

    REFERENCE_TIME rtTemp;
    GetQueueTime(&rtTemp);
    pNext = NULL;
    for(pEvent = pQueue->GetHead(); pEvent; pEvent = pNext )
    {
        pNext = pEvent->pNext;
        pEvent->dwPrivFlags &= ~PRIV_FLAG_REMOVE;
        DMUS_PMSG* pNew = NULL;
        if( ( 0 == dwId || pEvent->dwVirtualTrackID == dwId ) &&
            (pEvent->dwType == DMUS_PMSGT_NOTE) )
        {
            REFERENCE_TIME rtTime = pEvent->rtTime;
            DMUS_NOTE_PMSG* pNote = (DMUS_NOTE_PMSG*)PRIV_TO_DMUS(pEvent);
            if( pNote->bFlags & DMUS_NOTEF_NOTEON )
            {
                MUSIC_TIME mtNote = pNote->mtTime - pNote->nOffset;
                MusicToReferenceTime( mtNote, &rtTime );
            }
            // now flush the event if needed
            if( rtTime >= rtFlush )
            {
                REFERENCE_TIME rtFlushTime = (rtFlush <= pEvent->rtLast) ? pEvent->rtLast + REF_PER_MIL : rtFlush;
                if( pEvent->pTool &&
                    !(pNote->bFlags & DMUS_NOTEF_NOTEON) &&
                    S_OK == (hr = GetChordNotificationStatus(pNote, dwTrackGroup, rtFlushTime, &pNew)))
                {
                    pEvent->dwPrivFlags |= PRIV_FLAG_REMOVE;
                    pEvent->dwFlags |= DMUS_PMSGF_TOOL_FLUSH;
                    pEvent->pTool->Flush( this, PRIV_TO_DMUS(pEvent), rtFlushTime );
                }
                if (SUCCEEDED(hr) && pNew) // add to temp queue for later insertion into regular queue
                {
                    UpdateQueue.Enqueue( DMUS_TO_PRIV(pNew) );
                }
            }
            else // cut notes short if needed
            {
                if( pNote->bFlags & DMUS_NOTEF_NOTEON )
                {
                    if (S_OK == (hr = GetChordNotificationStatus(pNote, dwTrackGroup, rtFlush, &pNew)))
                    {
                        // subtract 2 from the duration to guarantee the note cuts short
                        // 1 clock before the flush time.
                        MUSIC_TIME mtNoteOff = pNote->mtTime + pNote->mtDuration - 2;
                        REFERENCE_TIME rtNoteOff;
                        MusicToReferenceTime( mtNoteOff, &rtNoteOff );
                        if( rtNoteOff >= rtFlush )
                        {
                            ReferenceToMusicTime( rtFlush, &mtNoteOff );
                            mtNoteOff -= pNote->mtTime;
                            if( mtNoteOff < 1 ) mtNoteOff = 1; // disallow durations less than 1. This should never happen anyway.
                            pNote->mtDuration = mtNoteOff;
                        }
                    }
                    if (SUCCEEDED(hr) && pNew) // add to temp queue for later insertion into regular queue
                    {
                        UpdateQueue.Enqueue( DMUS_TO_PRIV(pNew) );
                    }
                }
            }
        }
    }
    // remove (and unmark) all marked PMsgs from the current queue
    for(pEvent = pQueue->GetHead(); pEvent; pEvent = pNext )
    {
        pNext = pEvent->pNext;
        if (pEvent->dwPrivFlags & PRIV_FLAG_REMOVE)
        {
            pEvent->dwPrivFlags &= ~PRIV_FLAG_REMOVE;
            if (pQueue->Dequeue(pEvent))
            {
                FreePMsg(pEvent);
            }
            else
            {
                TraceI(0,"Error dequeing event for flushing\n");
            }
        }
    }
    // empty the Update queue into the current queue
    while( pEvent = UpdateQueue.Dequeue() )
    {
        pQueue->Enqueue(pEvent);
    }
    SendBuffers();
}

/*

  Only call this from withing a PipelineCrSec critical section!

*/
void CPerformance::OnChordUpdateEventQueues(
    DMUS_NOTIFICATION_PMSG* pNotify)    // notification PMsg that caused this to be called
{
    IDirectMusicSegmentState* pSegState = NULL;
    if (!pNotify || !pNotify->punkUser) return;
    REFERENCE_TIME rt = 0;
    if( pNotify->mtTime )
    {
        MusicToReferenceTime( pNotify->mtTime, &rt );
    }
    OnChordUpdateEventQueue( pNotify, &m_OnTimeQueue, rt );
    OnChordUpdateEventQueue( pNotify, &m_NearTimeQueue, rt );
    OnChordUpdateEventQueue( pNotify, &m_EarlyQueue, rt );
}

/////////////////////////////////////////////////////////////////////////////
// IDirectMusicPerformance

HRESULT CPerformance::CreateThreads()

{
    // initialize the realtime thread
    m_hRealtimeThread = CreateThread(NULL, 0, _Realtime, this, 0, &m_dwRealtimeThreadID);
    if( m_hRealtimeThread )
    {
        m_hRealtime = CreateEvent(NULL,FALSE,FALSE,NULL);
        SetThreadPriority( m_hRealtimeThread, THREAD_PRIORITY_TIME_CRITICAL );
    }
    else
    {
        TraceI(0, "Major error! Realtime thread not created.\n");
        return E_OUTOFMEMORY;
    }
    // initialize the transport thread
    m_hTransportThread = CreateThread(NULL, 0, _Transport, this, 0, &m_dwTransportThreadID);
    if( m_hTransportThread )
    {
        m_hTransport = CreateEvent(NULL, FALSE, FALSE, NULL);
        SetThreadPriority( m_hTransportThread, THREAD_PRIORITY_ABOVE_NORMAL );
    }
    else
    {
        TraceI(0, "Major error! Transport thread not created.\n");
        m_fKillRealtimeThread = TRUE;
        if( m_hRealtime ) SetEvent( m_hRealtime );
        return E_OUTOFMEMORY;
    }
    m_pDirectMusic->GetMasterClock( NULL, &m_pClock );
    m_rtStart = GetTime();
    m_rtQueuePosition = m_rtStart;
    return S_OK;
}


STDMETHODIMP CPerformance::InitAudio(IDirectMusic** ppDirectMusic,
                           IDirectSound** ppDirectSound,
                           HWND hWnd,
                           DWORD dwDefaultPathType,
                           DWORD dwPChannelCount,
                           DWORD dwFlags,
                           DMUS_AUDIOPARAMS *pParams)

{
    V_INAME(IDirectMusicPerformance::InitAudio);
    V_PTRPTR_WRITE_OPT(ppDirectMusic);
    V_PTRPTR_WRITE_OPT(ppDirectSound);
    V_HWND_OPT(hWnd);
    HRESULT hr = S_OK;

    // Further validate, checking for a pointer to a bad interface pointer...
    if (ppDirectMusic)
    {
        V_INTERFACE_OPT(*ppDirectMusic);
    }
    if (ppDirectSound)
    {
        V_INTERFACE_OPT(*ppDirectSound);
    }
    if( m_dwAudioPathMode )
    {
        Trace(1,"Error: InitAudio called on an already initialized Performance.\n");
        return DMUS_E_ALREADY_INITED;
    }
    if (dwFlags == 0)
    {
        dwFlags = DMUS_AUDIOF_ALL;
    }
    Init();
    m_AudioParams.dwFeatures = dwFlags;
    m_AudioParams.dwSampleRate = 22050;
    m_AudioParams.dwSize = sizeof (m_AudioParams);
    m_AudioParams.dwValidData = DMUS_AUDIOPARAMS_FEATURES | DMUS_AUDIOPARAMS_VOICES | DMUS_AUDIOPARAMS_SAMPLERATE | DMUS_AUDIOPARAMS_DEFAULTSYNTH;
    m_AudioParams.dwVoices = 64;
    m_AudioParams.fInitNow = TRUE;
    m_AudioParams.clsidDefaultSynth = CLSID_DirectMusicSynth;
    if (pParams)
    {
        if (pParams->dwValidData & DMUS_AUDIOPARAMS_FEATURES)
        {
            m_AudioParams.dwFeatures = pParams->dwFeatures;
        }
        if (pParams->dwValidData & DMUS_AUDIOPARAMS_VOICES)
        {
            m_AudioParams.dwVoices = pParams->dwVoices;
        }
        if (pParams->dwValidData & DMUS_AUDIOPARAMS_DEFAULTSYNTH)
        {
            // If they requested the DX7 default synth and yet also asked for audiopath
            // features, force to DX8 default synth.
            if ((pParams->clsidDefaultSynth != GUID_NULL) ||
                !((m_AudioParams.dwValidData & DMUS_AUDIOPARAMS_FEATURES) &&
                (m_AudioParams.dwFeatures & DMUS_AUDIOF_ALL)))
            {
                m_AudioParams.clsidDefaultSynth = pParams->clsidDefaultSynth;
            }
        }
        if (pParams->dwValidData & DMUS_AUDIOPARAMS_SAMPLERATE)
        {
            if (pParams->dwSampleRate > 96000)
            {
                m_AudioParams.dwSampleRate = 96000;
            }
            else if (pParams->dwSampleRate < 11025)
            {
                m_AudioParams.dwSampleRate = 11025;
            }
            else
            {
                m_AudioParams.dwSampleRate = pParams->dwSampleRate;
            }
        }
    }
    m_dwAudioPathMode = 2;
    EnterCriticalSection(&m_MainCrSec);
    if (ppDirectMusic && *ppDirectMusic)
    {
        hr = (*ppDirectMusic)->QueryInterface(IID_IDirectMusic8,(void **) &m_pDirectMusic);
    }
    if (SUCCEEDED(hr))
    {
        if (ppDirectSound && *ppDirectSound)
        {
            hr = (*ppDirectSound)->QueryInterface(IID_IDirectSound8,(void **) &m_pDirectSound);
        }
        if (SUCCEEDED(hr))
        {
            if (!m_pDirectSound)
            {
                hr = DirectSoundCreate8(NULL,&m_pDirectSound,NULL);
                if (SUCCEEDED(hr))
                {
                    if (!hWnd)
                    {
                        hWnd = GetForegroundWindow();
                        if (!hWnd)
                        {
                            hWnd = GetDesktopWindow();
                        }
                    }
                    m_pDirectSound->SetCooperativeLevel(hWnd, DSSCL_PRIORITY);
                }
            }

            if (SUCCEEDED(hr))
            {
                if (!m_pDirectMusic)
                {
                    hr = CoCreateInstance(CLSID_DirectMusic,
                                          NULL,
                                          CLSCTX_INPROC,
                                          IID_IDirectMusic8,
                                          (LPVOID*)&m_pDirectMusic);
                    if (SUCCEEDED(hr))
                    {
                        hr = m_pDirectMusic->SetDirectSound(m_pDirectSound,hWnd);
                    }
                }
            }
        }
    }
    if (SUCCEEDED(hr))
    {
        hr = m_BufferManager.Init(this,&m_AudioParams);
        if (SUCCEEDED(hr))
        {
            // If we are going to be connecting the synth to Buffers,
            // force the use of the dsound clock.
            if (m_AudioParams.dwFeatures & DMUS_AUDIOF_BUFFERS)
            {
                DMUS_CLOCKINFO ClockInfo;
                ClockInfo.dwSize = sizeof(ClockInfo);
                DWORD dwIndex;
                GUID guidMasterClock = GUID_NULL;
                for (dwIndex = 0; ;dwIndex++)
                {
                    if (S_OK == m_pDirectMusic->EnumMasterClock(dwIndex, &ClockInfo))
                    {
                        if (!wcscmp(ClockInfo.wszDescription, L"DirectSound Clock"))
                        {
                            guidMasterClock = ClockInfo.guidClock;
                            break;
                        }
                    }
                    else
                    {
                        break;
                    }
                }
                m_pDirectMusic->SetMasterClock(guidMasterClock);
            }
            hr = CreateThreads();
            if (SUCCEEDED(hr))
            {
                if (dwDefaultPathType)
                {
                    IDirectMusicAudioPath *pPath;
                    hr = CreateStandardAudioPath(dwDefaultPathType,dwPChannelCount,m_AudioParams.fInitNow,&pPath);
                    if (SUCCEEDED(hr))
                    {
                        hr = SetDefaultAudioPath(pPath);
                        pPath->Release();
                    }
                }
            }
        }
    }
    if (SUCCEEDED(hr))
    {
        if (m_pDirectMusic && ppDirectMusic && !*ppDirectMusic)
        {
            *ppDirectMusic = m_pDirectMusic;
            m_pDirectMusic->AddRef();
        }
        if (m_pDirectSound && ppDirectSound && !*ppDirectSound)
        {
            *ppDirectSound = m_pDirectSound;
            m_pDirectSound->AddRef();
        }
        if (pParams && pParams->fInitNow)
        {
            if (pParams->clsidDefaultSynth != m_AudioParams.clsidDefaultSynth)
            {
                pParams->clsidDefaultSynth = m_AudioParams.clsidDefaultSynth;
                if (pParams->dwValidData & DMUS_AUDIOPARAMS_DEFAULTSYNTH)
                {
                    Trace(2,"Warning: Default synth choice has been changed.\n");
                    hr = S_FALSE;
                }
            }
            if (pParams->dwFeatures != m_AudioParams.dwFeatures)
            {
                pParams->dwFeatures = m_AudioParams.dwFeatures;
                if (pParams->dwValidData & DMUS_AUDIOPARAMS_FEATURES)
                {
                    Trace(2,"Warning: Features flags has been changed to %lx.\n",pParams->dwFeatures);
                    hr = S_FALSE;
                }
            }
            if (pParams->dwSampleRate != m_AudioParams.dwSampleRate)
            {
                pParams->dwSampleRate = m_AudioParams.dwSampleRate;
                if (pParams->dwValidData & DMUS_AUDIOPARAMS_SAMPLERATE)
                {
                    Trace(2,"Warning: Sample rate has been changed to %ld.\n",pParams->dwSampleRate);
                    hr = S_FALSE;
                }
            }
            if (pParams->dwVoices != m_AudioParams.dwVoices)
            {
                pParams->dwVoices = m_AudioParams.dwVoices;
                if (pParams->dwValidData & DMUS_AUDIOPARAMS_VOICES)
                {
                    Trace(2,"Warning: Number of requested voices has been changed to %ld.\n",pParams->dwVoices);
                    hr = S_FALSE;
                }
            }
            pParams->dwValidData = m_AudioParams.dwValidData;
        }
        LeaveCriticalSection(&m_MainCrSec);
    }
    else
    {
        LeaveCriticalSection(&m_MainCrSec);
        CloseDown();
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE CPerformance::Init(
    IDirectMusic** ppDirectMusic,   LPDIRECTSOUND pDirectSound,HWND hWnd)
{
    V_INAME(IDirectMusicPerformance::Init);
    V_PTRPTR_WRITE_OPT(ppDirectMusic);
    V_INTERFACE_OPT(pDirectSound);
    V_HWND_OPT(hWnd);
    HRESULT hr = S_OK;

    // Further validate, checking for a pointer to a bad interface pointer...
    if (ppDirectMusic)
    {
        V_INTERFACE_OPT(*ppDirectMusic);
    }
    if( m_dwAudioPathMode )
    {
        Trace(1,"Error: Init called on an already initialized Performance.\n");
        return DMUS_E_ALREADY_INITED;
    }
    Init();
    m_dwAudioPathMode = 1;
    EnterCriticalSection(&m_MainCrSec);

    if(( NULL == ppDirectMusic ) || ( NULL == *ppDirectMusic ))
    {
        // intialize DirectMusic.

        if( FAILED( CoCreateInstance(CLSID_DirectMusic,
                              NULL,
                              CLSCTX_INPROC,
                              IID_IDirectMusic,
                              (LPVOID*)&m_pDirectMusic)))
        {
            m_pDirectMusic = NULL;
            LeaveCriticalSection(&m_MainCrSec);
            return E_OUTOFMEMORY;
        }

        // If version2 was requested by the app (in the process of requesting the
        // IDirectMusicPerformance2 interface), do the same for IDirectMusic.
        if (m_dwVersion > 6)
        {
            IDirectMusic *pTemp = NULL;
            if (SUCCEEDED(m_pDirectMusic->QueryInterface(
                IID_IDirectMusic2,
                (LPVOID*)&pTemp)))
            {
                // Succeeded in requesting DX7 and up behavior...
                pTemp->Release();
            }
        }

        hr = m_pDirectMusic->SetDirectSound(pDirectSound, hWnd);
        if( FAILED( hr ) )
        {
            m_pDirectMusic->Release();
            m_pDirectMusic = NULL;
            LeaveCriticalSection(&m_MainCrSec);
            return hr;
        }

        if( ppDirectMusic )
        {
            *ppDirectMusic = m_pDirectMusic;
            m_pDirectMusic->AddRef();
        }
    }
    else
    {
        m_pDirectMusic = (IDirectMusic8 *) *ppDirectMusic;
        m_pDirectMusic->AddRef();
    }
    if (FAILED(hr = CreateThreads()))
    {
        if( m_pDirectMusic )
        {
            m_pDirectMusic->Release();
            m_pDirectMusic = NULL;
        }
    }
    LeaveCriticalSection(&m_MainCrSec);
    return hr;
}

CSegState *CPerformance::GetSegmentForTransition(DWORD dwFlags,MUSIC_TIME mtTime, IUnknown *pFrom)

{
    CSegState *pSegState = NULL;

    // If the source segment was provided, use it.
    if (pFrom)
    {
        if (SUCCEEDED(pFrom->QueryInterface(IID_CSegState,(void **) &pSegState)))
        {
            pSegState->Release();
        }
    }
    // Else, if this is a primary segment, get the current primary segment.
    if (!pSegState && !(dwFlags & DMUS_SEGF_SECONDARY))
    {
        pSegState = GetPrimarySegmentAtTime(mtTime);
    }
    return pSegState;
}

void CPerformance::ClearMusicStoppedNotification()

{
    EnterCriticalSection(&m_PipelineCrSec);
    PRIV_PMSG* pPMsg;
    PRIV_PMSG* pNext;
    DMUS_NOTIFICATION_PMSG* pNotification;

    pPMsg = m_OnTimeQueue.GetHead(); // where notifications live normally
    for (; pPMsg ; pPMsg = pNext)
    {
        pNext = pPMsg->pNext;
        pNotification = (DMUS_NOTIFICATION_PMSG*)PRIV_TO_DMUS(pPMsg);
        if( ( pPMsg->dwType == DMUS_PMSGT_NOTIFICATION ) &&
            ( pNotification->guidNotificationType == GUID_NOTIFICATION_PERFORMANCE ) &&
            ( pNotification->dwNotificationOption == DMUS_NOTIFICATION_MUSICSTOPPED ) )
        {
            pPMsg = m_OnTimeQueue.Dequeue(pPMsg);
            if( pPMsg ) // Should always succeeed
            {
                FreePMsg(pPMsg);
            }
            m_fMusicStopped = FALSE;
        }
    }
    LeaveCriticalSection(&m_PipelineCrSec);
}

HRESULT CPerformance::PlayOneSegment(
    CSegment* pSegment,
    DWORD dwFlags,
    __int64 i64StartTime,
    CSegState **ppSegState,
    CAudioPath *pAudioPath)
{
    HRESULT hr;
#ifdef DBG_PROFILE
    DWORD dwDebugTime;
    dwDebugTime = timeGetTime();
#endif

    TraceI(0,"Play Segment %lx (%ls) at time %ld with flags %lx\n",pSegment,pSegment->m_wszName,(long)i64StartTime,dwFlags);
    if( dwFlags & DMUS_SEGF_CONTROL )
    {
        dwFlags |= DMUS_SEGF_SECONDARY;
    }
    if( i64StartTime )
    {
        if(dwFlags & DMUS_SEGF_REFTIME)
        {
            // Give a grace period of 100ms.
            if( i64StartTime < (GetLatency() - (100 * REF_PER_MIL)))
            {
                Trace(1,"Error: Unable to play segment, requested clock time %ld is past current time %ld\n",
                    (long)i64StartTime,(long)(GetLatency() - (100 * REF_PER_MIL)));
                return DMUS_E_TIME_PAST;
            }
        }
        else
        {
            MUSIC_TIME mtPrePlay;
            // Give a grace period of 100ms.
            ReferenceToMusicTime( (GetLatency() - (100 * REF_PER_MIL)), &mtPrePlay );
            if( (MUSIC_TIME)i64StartTime < mtPrePlay )
            {
                Trace(1,"Error: Unable to play segment, requested music time %ld is past current time %ld\n",
                    (long)i64StartTime,(long)mtPrePlay);
                return DMUS_E_TIME_PAST;
            }
        }
    }

    CSegState *pSegState = NULL;
    hr = pSegment->CreateSegmentState( &pSegState, this, pAudioPath, dwFlags);
    *ppSegState = pSegState;
    if (FAILED(hr))
    {
        Trace(1,"Error: Unable to play segment because of failure creating segment state.\n");
        return DMUS_E_SEGMENT_INIT_FAILED;
    }
    pSegState->m_rtGivenStart = i64StartTime;

    pSegState->m_dwPlaySegFlags = dwFlags;

    // add the pSegState to the appropriate queue
    EnterCriticalSection(&m_SegmentCrSec);
    m_fPlaying = 1; // turn on the transport
    // add all notifications to the segment. First, clear it, in case old notifications
    // are in effect.
    pSegment->RemoveNotificationType(GUID_NULL,TRUE);
    CNotificationItem* pItem;
    pItem = m_NotificationList.GetHead();
    while( pItem )
    {
        pSegment->AddNotificationType( pItem->guidNotificationType, TRUE );
        pItem = pItem->GetNext();
    }

    if( pSegState->m_dwPlaySegFlags & DMUS_SEGF_AFTERPREPARETIME )
    {
        // we want to queue this at the last transported time,
        // so we don't need to do an invalidate
        if( pSegState->m_dwPlaySegFlags & DMUS_SEGF_REFTIME )
        {
            REFERENCE_TIME rtTrans;
            MusicToReferenceTime( m_mtTransported, &rtTrans );
            if( pSegState->m_rtGivenStart < rtTrans )
            {
                pSegState->m_dwPlaySegFlags &= ~DMUS_SEGF_REFTIME;
                pSegState->m_rtGivenStart = m_mtTransported;
            }
        }
        else
        {
            if( pSegState->m_rtGivenStart < m_mtTransported )
            {
                pSegState->m_rtGivenStart = m_mtTransported;
            }
        }
    }
    else if( pSegState->m_dwPlaySegFlags & DMUS_SEGF_AFTERQUEUETIME )
    {
        // we want to queue this at the queue time, as opposed to latency time,
        // which is an option for secondary segments.
        REFERENCE_TIME rtStart;
        GetQueueTime( &rtStart ); // need queue time because control segments cause invalidations
        if( pSegState->m_dwPlaySegFlags & DMUS_SEGF_REFTIME )
        {
            if( pSegState->m_rtGivenStart < rtStart )
            {
                pSegState->m_rtGivenStart = rtStart;
            }
        }
        else
        {
            MUSIC_TIME mtStart;
            ReferenceToMusicTime( rtStart, &mtStart );
            if( pSegState->m_rtGivenStart < mtStart )
            {
                pSegState->m_rtGivenStart = mtStart;
            }
        }
    }
    // need to get rid of any pending musicstopped notifications
    ClearMusicStoppedNotification();

    pSegState->AddRef();

    if( dwFlags & DMUS_SEGF_SECONDARY ) // queue a secondary segment
    {
        QueueSecondarySegment( pSegState );
    }
    else // queue a primary segment
    {
        QueuePrimarySegment( pSegState );
    }

    LeaveCriticalSection(&m_SegmentCrSec);

#ifdef DBG_PROFILE
    dwDebugTime = timeGetTime() - dwDebugTime;
    TraceI(5, "perf, debugtime PlaySegment %u\n", dwDebugTime);
#endif

    // signal the transport thread so we don't have to wait for it to wake up on its own
    if( m_hTransport ) SetEvent( m_hTransport );

    return S_OK;
}


HRESULT CPerformance::PlaySegmentInternal(
    CSegment* pSegment,
    CSong * pSong,
    WCHAR *pwzSegmentName,
    CSegment* pTransition,
    DWORD dwFlags,
    __int64 i64StartTime,
    IDirectMusicSegmentState** ppSegmentState,
    IUnknown *pFrom,
    CAudioPath *pAudioPath)
{
    HRESULT hr;
    CAudioPath *pInternalPath = NULL;
    if( m_pClock == NULL )
    {
        Trace(1,"Error: Can not play segment because master clock has not been initialized.\n");
        return DMUS_E_NO_MASTER_CLOCK;
    }
    if (pAudioPath && (pAudioPath->NoPorts()))
    {
        // This audiopath can't be used for playback since it doesn't have any ports.
        Trace(1,"Error: Audiopath can't be used for playback because it doesn't have any ports.\n");
        return DMUS_E_AUDIOPATH_NOPORT;
    }

    // Pointer to segment or song provided audio path config.
    IUnknown *pConfig = NULL;

    /*  If this is a song, use the segment name to get the segment.
        Then, it looks like a normal segment except the
        existence of the pSong will let the segstate know
        that it is a member of a song, so it should chain segments.
    */
    if (pSong)
    {
        IDirectMusicSegment *pISegment = NULL;
        hr = pSong->GetSegment(pwzSegmentName,&pISegment);
        if (hr != S_OK)
        {
            return DMUS_E_NOT_FOUND;
        }
        pSegment = (CSegment *) pISegment;
        // If the app wants an audiopath created dynamically from the song, find it and use it.
        if (dwFlags & DMUS_SEGF_USE_AUDIOPATH)
        {
            pSong->GetAudioPathConfig(&pConfig);
        }
    }
    else if (pSegment)
    {
        // Addref so we can release later.
        pSegment->AddRef();
    }
    else
    {
        // No Segment!
        Trace(1,"Error: No segment - nothing to play!\n");
        return E_FAIL;
    }
    if (dwFlags & DMUS_SEGF_DEFAULT )
    {
        DWORD   dwResTemp;
        pSegment->GetDefaultResolution( &dwResTemp );
        dwFlags &= ~DMUS_SEGF_DEFAULT;
        dwFlags |= dwResTemp;
    }
    // If the app wants an audiopath created dynamically from the segment, find it and use it.
    // Note that this overrides an audiopath created from the song.
    if (dwFlags & DMUS_SEGF_USE_AUDIOPATH)
    {
        IUnknown *pSegConfig;
        if (SUCCEEDED(pSegment->GetAudioPathConfig(&pSegConfig)))
        {
            if (pConfig)
            {
                pConfig->Release();
            }
            pConfig = pSegConfig;
        }
    }

    // If we got an audiopath config from the segment or song, use it.
    if (pConfig)
    {
        IDirectMusicAudioPath *pNewPath;
        if (SUCCEEDED(CreateAudioPath(pConfig,TRUE,&pNewPath)))
        {
            // Now, get the CAudioPath structure.
            if (SUCCEEDED(pNewPath->QueryInterface(IID_CAudioPath,(void **) &pInternalPath)))
            {
                pAudioPath = pInternalPath;
            }
            pNewPath->Release();
        }
        else
        {
            pConfig->Release();
            Trace(1,"Error: Embedded audiopath failed to create, segment will not play.\n");
            return DMUS_E_NO_AUDIOPATH;
        }
        pConfig->Release();
    }

    if (pTransition)
    {
        pTransition->AddRef();
    }

    if ((dwFlags & DMUS_SEGF_SECONDARY) && (dwFlags & DMUS_SEGF_QUEUE))
    {
        // Can only queue if there's a segment to queue after.
        if (pFrom)
        {
            CSegState *pSegFrom = NULL;
            if (SUCCEEDED(pFrom->QueryInterface(IID_CSegState,(void **) &pSegFrom)))
            {
                // Calculate the time at which the preceding segment will stop.
                MUSIC_TIME mtStartTime = pSegFrom->GetEndTime( pSegFrom->m_mtResolvedStart );
                i64StartTime = mtStartTime;
                dwFlags &= ~DMUS_SEGF_REFTIME;
                pSegFrom->Release();
            }
        }
    }

    // If auto-transition is requested,
    // get the transition template, if it exists,
    // and compose a segment with it.
    CSegment *pPlayAfter = NULL;    // This will hold the second segment, if we end up with a transition.
    DWORD dwFlagsAfter = dwFlags & (DMUS_SEGF_SECONDARY | DMUS_SEGF_CONTROL);
    if ( dwFlags & DMUS_SEGF_AUTOTRANSITION )
    {
        // First, calculate the time to start the transition.
        // Note: this will be done again later. We really need to fold this all together.
        REFERENCE_TIME rtTime;
        if (i64StartTime == 0)
        {
            GetQueueTime( &rtTime );
        }
        else if (dwFlags & DMUS_SEGF_REFTIME)
        {
            rtTime = i64StartTime;
        }
        else
        {
            MusicToReferenceTime((MUSIC_TIME) i64StartTime,&rtTime);
        }
        REFERENCE_TIME rtResolved;
        GetResolvedTime(rtTime, &rtResolved,dwFlags);
        MUSIC_TIME mtTime;  // Actual time to start transition.
        ReferenceToMusicTime(rtResolved,&mtTime);

        CSegment *pPriorSeg = NULL;
        // Find the segment that is active at transition time.
        CSegState *pPriorState = GetSegmentForTransition(dwFlags,mtTime,pFrom);
        if (pPriorState)
        {
            pPriorSeg = pPriorState->m_pSegment;
        }
        // If this is a song, use the id to get the transition.
        if (pSong && !pTransition)
        {
            DMUS_IO_TRANSITION_DEF Transition;
            // Now, find out what sort of transition is expected.
            if (SUCCEEDED(pSong->GetTransitionSegment(pPriorSeg,pSegment,&Transition)))
            {
                if (Transition.dwTransitionID != DMUS_SONG_NOSEG)
                {
                    if (S_OK == pSong->GetPlaySegment(Transition.dwTransitionID,&pTransition))
                    {
                        dwFlags = Transition.dwPlayFlags;
                    }
                }
                else
                {
                    dwFlags = Transition.dwPlayFlags;
                }
            }
        }
        if (pTransition)
        {
            IDirectMusicSegment *pITransSegment = NULL;
            if (pPriorState)
            {
                pTransition->Compose(mtTime - pPriorState->m_mtOffset, pPriorSeg, pSegment, &pITransSegment);
            }
            else
            {
                pTransition->Compose(0,pPriorSeg,pSegment,&pITransSegment);
            }
            // Now, if we successfully composed a transition segment, set it up to be the one we
            // will play first. Later, we fill call PlaySegment() with pPlayAfter, to queue it
            // to play after the transition.
            if (pITransSegment)
            {
                pPlayAfter = pSegment;
                pSegment = (CSegment *) pITransSegment;
            }
        }
    }
    if (pSegment)
    {
        CSegState *pSegState;
        if (!pAudioPath)
        {
            pAudioPath = m_pDefaultAudioPath;
        }
        if (pAudioPath && !pAudioPath->IsActive())
        {
            Trace(1,"Error: Can not play segment on inactive audiopath\n");
            hr = DMUS_E_AUDIOPATH_INACTIVE;
        }
        else if ((m_dwAudioPathMode != 1) && !pAudioPath)
        {
            Trace(1,"Error: No audiopath to play segment on.\n");
            hr = DMUS_E_NO_AUDIOPATH;
        }
        else
        {
            if (ppSegmentState)
            {
                *ppSegmentState = NULL;
            }
            hr = PlayOneSegment(
                pSegment,
                dwFlags,
                i64StartTime,
                &pSegState,
                pAudioPath);
            if (SUCCEEDED(hr))
            {
                if (pFrom)
                {
                    pSegState->m_fCanStop = FALSE;
                    StopEx(pFrom, pSegState->m_mtResolvedStart, 0);
                    pSegState->m_fCanStop = TRUE;
                }
                // If this was actually a transition segment, now we need to play the original segment!
                if (pPlayAfter)
                {
                    MUSIC_TIME mtStartTime = pSegState->GetEndTime(pSegState->m_mtResolvedStart );
                    pSegState->Release();
                    hr = PlayOneSegment(pPlayAfter,dwFlagsAfter,mtStartTime,&pSegState,pAudioPath);
                }
                if (SUCCEEDED(hr))
                {
                    if (pSong)
                    {
                        pSegState->m_fSongMode = TRUE;
                    }
                    if (ppSegmentState)
                    {
                        *ppSegmentState = pSegState;
                    }
                    else
                    {
                        pSegState->Release();
                    }
                }
            }
        }
    }
    else
    {
        // There never was a segment to play, not even a transition.
        Trace(1,"Error: No segment to play.\n");
        hr = E_INVALIDARG;
    }
    // Before leaving, reduce the reference counts on variables that have been addref'd.
    if (pSegment)
    {
        pSegment->Release();
    }
    if (pTransition)
    {
        pTransition->Release();
    }
    if (pPlayAfter)
    {
        pPlayAfter->Release();
    }
    if (pInternalPath)
    {
        pInternalPath->Release();
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE CPerformance::PlaySegment(
    IDirectMusicSegment *pSegment,
    DWORD dwFlags,
    __int64 i64StartTime,
    IDirectMusicSegmentState **ppSegmentState)
{
    V_INAME(IDirectMusicPerformance::PlaySegment);
    V_INTERFACE(pSegment);
    V_PTRPTR_WRITE_OPT(ppSegmentState);
    CSegment *pCSourceSegment = NULL;
    if (SUCCEEDED(pSegment->QueryInterface(IID_CSegment,(void **) &pCSourceSegment)))
    {
        pCSourceSegment->Release();
    }
    else
    {
        Trace(1,"Error: Invalid segment object passed to PlaySegment(). Segment must be created using CLSID_DirectMusicSegment object.\n");
        return E_POINTER;
    }
    return PlaySegmentInternal(pCSourceSegment,NULL,0,NULL,dwFlags,i64StartTime,ppSegmentState,NULL,NULL);
}

HRESULT STDMETHODCALLTYPE CPerformance::PlaySegmentEx(
    IUnknown* pSource,
    WCHAR *pwzSegmentName,
    IUnknown* pTransition,
    DWORD dwFlags,
    __int64 i64StartTime,
    IDirectMusicSegmentState** ppSegmentState,
    IUnknown *pFrom,
    IUnknown *pAudioPath)
{
    V_INAME(IDirectMusicPerformance::PlaySegmentEx);
    V_INTERFACE_OPT(pSource);
    V_INTERFACE_OPT(pTransition);
    V_PTRPTR_WRITE_OPT(ppSegmentState);
    V_INTERFACE_OPT(pFrom);
    V_INTERFACE_OPT(pAudioPath);

    CSegment *pCSourceSegment = NULL;
    CSong *pCSourceSong = NULL;
    CSegment *pCTransition = NULL;
    CAudioPath *pCAudioPath = NULL;
//    TraceI(0,"Playing %lx at time %ld, flags %lx, Transition %lx\n",pSource,(long)i64StartTime,dwFlags,pTransition);

    // We may not have a source segment in the special case of transitioning from NULL.
    if (!pSource && !pTransition)
    {
        Trace(1,"Error: Must pass either a segment or transition segment to PlaySegmentEx()\n");
        return E_POINTER;
    }
    if (pSource)
    {
        // Figure out if we have a source song or segment and get the internal representations.
        if (SUCCEEDED(pSource->QueryInterface(IID_CSegment,(void **) &pCSourceSegment)))
        {
            pCSourceSegment->Release();
        }
        else if (SUCCEEDED(pSource->QueryInterface(IID_CSong,(void **) &pCSourceSong)))
        {
            pCSourceSong->Release();
        }
        else
        {
            Trace(1,"Error: Invalid segment or song passed to PlaySegmentEx().\n");
            return E_POINTER;
        }
    }
    // If we have a transition segment, get the CSegment representation.
    if (pTransition)
    {
        if (SUCCEEDED(pTransition->QueryInterface(IID_CSegment,(void **) &pCTransition)))
        {
            pCTransition->Release();
        }
        else
        {
            Trace(1,"Error: Invalid transition passed to PlaySegmentEx().\n");
            return E_POINTER;
        }
    }
    if (pAudioPath)
    {
        if (SUCCEEDED(pAudioPath->QueryInterface(IID_CAudioPath,(void **) &pCAudioPath)))
        {
            pCAudioPath->Release();
        }
        else
        {
            Trace(1,"Error: Invalid audiopath passed to PlaySegmentEx().\n");
            return E_POINTER;
        }
    }
    return PlaySegmentInternal(pCSourceSegment,pCSourceSong,pwzSegmentName,
        pCTransition,dwFlags,i64StartTime,
        ppSegmentState,pFrom,
        pCAudioPath);
}

STDMETHODIMP CPerformance::SetDefaultAudioPath(IDirectMusicAudioPath *pAudioPath)
{
    V_INAME(IDirectMusicPerformance::SetDefaultAudioPath);
    V_INTERFACE_OPT(pAudioPath);
    if (m_dwAudioPathMode == 0)
    {
        Trace(1,"Error: Performance not initialized.\n");
        return DMUS_E_NOT_INIT;
    }
    if (m_dwAudioPathMode == 1)
    {
        Trace(1,"Error: Performance initialized not to support Audiopaths.\n");
        return DMUS_E_AUDIOPATHS_NOT_VALID;
    }
    CAudioPath *pCPath = NULL;
    if (pAudioPath)
    {
        if (SUCCEEDED(pAudioPath->QueryInterface(IID_CAudioPath,(void **) &pCPath)))
        {
            pCPath->Release();
            if (!m_AudioPathList.IsMember(pCPath))
            {
                // This is not a legal audiopath, since it wasn't created by this performance.
                Trace(1,"Error: Invalid audiopath - not created by this Performance.\n");
                return E_INVALIDARG;
            }
            if (pCPath->NoPorts())
            {
                // This is an audiopath that doesn't have any port configurations.
                // For example, it might be environmental reverb.
                Trace(1,"Error: Failure setting default audiopath - does not have any ports, so can not be played on.\n");
                return DMUS_E_AUDIOPATH_NOPORT;
            }
        }
        else
        {
            // This is not a legal audiopath object at all.
            Trace(1,"Error: Invalid audiopath - not created by call to Performance->CreateAudioPath().\n");
            return E_INVALIDARG;
        }
    }
    if (m_pDefaultAudioPath)
    {
        m_pDefaultAudioPath->Release();
        m_pDefaultAudioPath = NULL;
    }
    m_pDefaultAudioPath = pCPath;
    if (pCPath)
    {
        pCPath->AddRef();
        pCPath->Activate(TRUE);
    }
    return S_OK;
}

STDMETHODIMP CPerformance::GetDefaultAudioPath(IDirectMusicAudioPath **ppAudioPath)
{
    V_INAME(IDirectMusicPerformance::GetDefaultAudioPath);
    V_PTRPTR_WRITE(ppAudioPath);
    if (m_dwAudioPathMode == 0)
    {
        Trace(1,"Error: Performance not initialized.\n");
        return DMUS_E_NOT_INIT;
    }
    if (m_dwAudioPathMode == 1)
    {
        Trace(1,"Error: Performance was initialized not to support audiopaths.\n");
        return DMUS_E_AUDIOPATHS_NOT_VALID;
    }
    if (m_pDefaultAudioPath)
    {
        *ppAudioPath = (IDirectMusicAudioPath *) m_pDefaultAudioPath;
        m_pDefaultAudioPath->AddRef();
        return S_OK;
    }
    Trace(3,"Warning: No default audiopath\n");
    return DMUS_E_NOT_FOUND;
}

HRESULT STDMETHODCALLTYPE CPerformance::CreateAudioPath( IUnknown *pSourceConfig,
                                                        BOOL fActivate,
                                                        IDirectMusicAudioPath **ppNewPath)

{
    V_INAME(IDirectMusicPerformance::CreateAudioPath);
    V_INTERFACE(pSourceConfig);
    V_PTRPTR_WRITE_OPT(ppNewPath);

    if (m_dwAudioPathMode == 0)
    {
        Trace(1,"Error: Performance not initialized.\n");
        return DMUS_E_NOT_INIT;
    }
    if (m_dwAudioPathMode == 1)
    {
        Trace(1,"Error: Performance not initialized to support audiopaths (must use InitAudio.)\n");
        return DMUS_E_AUDIOPATHS_NOT_VALID;
    }
    HRESULT hr = E_OUTOFMEMORY;
    CAudioPath *pPath = new CAudioPath;
    if (pPath)
    {
        hr = pPath->Init(pSourceConfig,this);
        if (SUCCEEDED(hr) && fActivate)
        {
            hr = pPath->Activate(TRUE);
#ifdef DBG
            if (FAILED(hr))
            {
                Trace(1,"Error: Audiopath creation failed because one or more buffers could not be activated.\n");
            }
#endif
        }
        if (SUCCEEDED(hr))
        {
            hr = pPath->QueryInterface(IID_IDirectMusicAudioPath,(void **) ppNewPath);
        }
        else
        {
            delete pPath;
        }
    }
    return hr;
}

STDMETHODIMP CPerformance::CreateStandardAudioPath(DWORD dwType,
                                                   DWORD dwPChannelCount,
                                                   BOOL fActivate,
                                                   IDirectMusicAudioPath **ppNewPath)
{
    V_INAME(IDirectMusicPerformance::CreateStandardAudioPath);
    V_PTRPTR_WRITE_OPT(ppNewPath);
    HRESULT hr = S_OK;
    if (m_dwAudioPathMode == 2)
    {
        if ((dwType <= DMUS_APATH_DYNAMIC_STEREO) && (dwType >= DMUS_APATH_DYNAMIC_3D)
            || (dwType == DMUS_APATH_SHARED_STEREOPLUSREVERB))
        {
            if (!(m_AudioParams.dwFeatures & DMUS_AUDIOF_BUFFERS))
            {
                Trace(4,"Warning: Creating a standard audiopath without buffers - InitAudio specified no buffer support.\n");
                // If the default synth doesn't support buffers, then create a simple port with no buffers.
                dwType = 0;
            }
            CAudioPathConfig *pConfig = CAudioPathConfig::CreateStandardConfig(dwType,dwPChannelCount,m_AudioParams.dwSampleRate);
            if (pConfig)
            {
                hr = CreateAudioPath((IPersistStream *) pConfig,fActivate,ppNewPath);
                pConfig->Release();
            }
            else
            {
                // CreateStandardConfig only returns NULL if we've run out of memory.
                hr = E_OUTOFMEMORY;
            }
        }
        else
        {
            Trace(1,"Error: %ld is not a valid predefined audiopath.\n",dwType);
            hr  = E_INVALIDARG;
        }
    }
    else
    {
        Trace(1,"Error: Performance not initialized to support audiopaths.\n");
        hr = DMUS_E_AUDIOPATHS_NOT_VALID;
    }
    return hr;
}

// Stop the segment state at mtTime. If NULL, stop all.
void CPerformance::DoStop( CSegState* pSegState, MUSIC_TIME mtTime,
                                     BOOL fInvalidate)
{
    HRESULT hrAbort = S_OK;
    DWORD dwCount;
    if( NULL == pSegState ) return;
    EnterCriticalSection(&m_SegmentCrSec);
    CSegStateList *pSourceList = NULL;
    CSegStateList *pDestList = NULL;
    CSegState *pNode = NULL;
    // Mark the length of the segstate to be only as far as it played
    // to keep GetParam() from accessing the unplayed portion.
    if (pSegState)
    {
        if (mtTime < pSegState->m_mtEndTime)
        {
            pSegState->m_mtLength = mtTime - pSegState->m_mtResolvedStart +
                pSegState->m_mtStartPoint;
            if (pSegState->m_mtLength < 0)
            {
                pSegState->m_mtLength = 0;
            }
            // Make endtime one greater than mtTime so Abort notification will still happen.
            pSegState->m_mtEndTime = mtTime + 1;
        }
    }
    RecalcTempoMap(pSegState,mtTime);
    // check each play queue
    for (dwCount = SQ_PRI_PLAY; dwCount <= SQ_SEC_PLAY; dwCount++)
    {
        for( pNode = m_SegStateQueues[dwCount].GetHead(); pNode; pNode = pNode->GetNext())
        {
            if( pNode == pSegState )
            {
                // we want to move this to the approprate done queue
                pDestList = &m_SegStateQueues[SQ_PRI_DONE - SQ_PRI_PLAY + dwCount];
                pSourceList = &m_SegStateQueues[dwCount];
                if ((dwCount == SQ_PRI_PLAY) && (m_SegStateQueues[SQ_PRI_PLAY].GetCount() == 1))
                {
                    if (m_dwVersion >= 8)
                    {
                        MUSIC_TIME mtNow;
                        GetTime( NULL, &mtNow );
                        GenerateNotification( DMUS_NOTIFICATION_MUSICALMOSTEND, mtNow, pSegState );
                    }
                }
                dwCount = SQ_SEC_PLAY;  // Force out of outer loop.
                break;
            }
        }
    }
    if (!pNode)
    {
        // check each done queue
        for (dwCount = SQ_PRI_DONE; dwCount <= SQ_SEC_DONE; dwCount++)
        {
            for( pNode = m_SegStateQueues[dwCount].GetHead(); pNode; pNode = pNode->GetNext())
            {
                if( pNode == pSegState )
                {
                    pSourceList = &m_SegStateQueues[dwCount];
                    dwCount = SQ_SEC_DONE;  // Force out of outer loop.
                    break;
                }
            }
        }
    }
    if( pNode && pSourceList)
    {
        REFERENCE_TIME rtTime;
        MusicToReferenceTime(mtTime,&rtTime);
        if( pNode->m_mtLastPlayed >= mtTime )
        {
            pNode->Flush( mtTime );
            pNode->m_mtLastPlayed = mtTime; // must set this to indicate it only played until then
            pNode->m_rtLastPlayed = rtTime;
        }
        if( fInvalidate )
        {
            if( pNode->m_dwPlaySegFlags & DMUS_SEGF_CONTROL )
            {
                Invalidate( mtTime, 0 ); // must call Invalidate before AbortPlay so we don't
                // invalidate the abort notification
            }
            else if ( !(pNode->m_dwPlaySegFlags & DMUS_SEGF_SECONDARY ))
            {
                // If this is a primary segment, kill the tempo map.
                FlushEventQueue( 0, &m_TempoMap, rtTime, rtTime, FALSE );
            }
        }
        hrAbort = pNode->AbortPlay( mtTime, FALSE );
        if( pNode->m_dwPlaySegFlags & DMUS_SEGF_CONTROL )
        {
            pSourceList->Remove(pNode);
            m_ShutDownQueue.Insert(pNode); // we're guaranteed to never need this again

            // set dirty flags on all other segments

            for (dwCount = SQ_PRI_PLAY; dwCount <= SQ_SEC_PLAY; dwCount++)
            {
                for( pNode = m_SegStateQueues[dwCount].GetHead(); pNode; pNode = pNode->GetNext() )
                {
                    if( pNode->m_fStartedPlay )
                    {
                        pNode->m_dwPlayTrackFlags |= DMUS_TRACKF_DIRTY;
                    }
                }
            }
        }
        else if( pDestList )
        {
            pSourceList->Remove(pNode);
            pDestList->Insert(pNode);
        }
    }
    else
    {
        // check the wait lists.
        for (dwCount = SQ_PRI_WAIT; dwCount <= SQ_SEC_WAIT; dwCount++)
        {
            for( pNode = m_SegStateQueues[dwCount].GetHead(); pNode; pNode = pNode->GetNext() )
            {
                if( pNode == pSegState )
                {
                    hrAbort = pNode->AbortPlay( mtTime, FALSE );
                    m_SegStateQueues[dwCount].Remove(pNode);
                    RecalcTempoMap(pNode, mtTime);
                    m_ShutDownQueue.Insert(pNode);
                    break;
                }
            }
        }
    }
    // if there aren't any more segments to play, send a Music Stopped
    // notification
    if( m_SegStateQueues[SQ_PRI_PLAY].IsEmpty() && m_SegStateQueues[SQ_SEC_PLAY].IsEmpty() &&
        m_SegStateQueues[SQ_PRI_WAIT].IsEmpty() && m_SegStateQueues[SQ_SEC_WAIT].IsEmpty() &&
        m_SegStateQueues[SQ_CON_PLAY].IsEmpty() && m_SegStateQueues[SQ_CON_WAIT].IsEmpty())
    {
        m_fMusicStopped = TRUE;
        // S_FALSE means we tried to abort this segstate, but it's already been aborted
        if (hrAbort != S_FALSE)
        {
            GenerateNotification( DMUS_NOTIFICATION_MUSICSTOPPED, mtTime, NULL );
        }
    }
    LeaveCriticalSection(&m_SegmentCrSec);
}

// Stop all segment states based off of the segment.
void CPerformance::DoStop( CSegment* pSeg, MUSIC_TIME mtTime, BOOL fInvalidate )
{
    DWORD dwCount;
    CSegState* pNode;
    CSegState* pNext;
    EnterCriticalSection(&m_SegmentCrSec);
    // find all seg pSegStates based off this segment that have played through time mtTime
    // if pSeg is NULL, go through all of the segment lists. Flush any
    // segment that played through time mtTime. Move any active segments
    // into past lists.
    if( pSeg )
    {
        for (dwCount = 0; dwCount < SQ_COUNT; dwCount++)
        {
            pNode = m_SegStateQueues[dwCount].GetHead();
            while( pNode )
            {
                pNext = pNode->GetNext();
                if( pNode->m_pSegment == pSeg )
                {
                    if (IsDoneQueue(dwCount))
                    {
                        if (pNode->m_mtLastPlayed >= mtTime)
                        {
                             DoStop( pNode, mtTime, fInvalidate );
                        }
                    }
                    else
                    {
                        DoStop( pNode, mtTime, fInvalidate );
                    }
                }
                pNode = pNext;
            }
        }
    }
    else // pSeg is NULL, stop everything.
    {
        // go ahead and flush the event queues
        EnterCriticalSection(&m_PipelineCrSec);
        FlushMainEventQueues( 0, mtTime, mtTime, FALSE );
        LeaveCriticalSection(&m_PipelineCrSec);
        // clear out the wait lists
        for (dwCount = SQ_PRI_WAIT; dwCount <= SQ_SEC_WAIT; dwCount++)
        {
            while (pNode = m_SegStateQueues[dwCount].GetHead())
            {
                pNode->AbortPlay( mtTime, FALSE );
                m_SegStateQueues[dwCount].RemoveHead();
                m_ShutDownQueue.Insert(pNode);
            }
        }
        // stop any segment that is currently playing.
        for (dwCount = SQ_PRI_DONE; dwCount <= SQ_SEC_DONE; dwCount++)
        {
            for( pNode = m_SegStateQueues[dwCount].GetHead(); pNode; pNode = pNode->GetNext() )
            {
                if( pNode->m_mtLastPlayed >= mtTime )
                {
                    DoStop( pNode, mtTime, fInvalidate );
                }
            }
        }
        for (dwCount = SQ_PRI_PLAY; dwCount <= SQ_SEC_PLAY; dwCount++)
        {
            while( m_SegStateQueues[dwCount].GetHead() )
            {
                DoStop( m_SegStateQueues[dwCount].GetHead(), mtTime, fInvalidate );
            }
        }
        // reset controllers and force all notes off.
        ResetAllControllers( GetLatency() );
    }
    LeaveCriticalSection(&m_SegmentCrSec);
}


STDMETHODIMP CPerformance::StopEx(IUnknown *pObjectToStop,__int64 i64StopTime,DWORD dwFlags)
{
    V_INAME(IDirectMusicPerformance::StopEx);
    V_INTERFACE_OPT(pObjectToStop);
    HRESULT hr = E_INVALIDARG;
    IDirectMusicSegmentState *pState;
    IDirectMusicSegment *pSegment;
    CSong *pSong;
    CAudioPath *pAudioPath;
    if (m_dwAudioPathMode == 0)
    {
        Trace(1,"Error: Performance not initialized.\n");
        return DMUS_E_NOT_INIT;
    }
TraceI(0,"StopExing %lx at time %ld, flags %lx\n",pObjectToStop,(long)i64StopTime,dwFlags);
    if (pObjectToStop == NULL)
    {
        return Stop(NULL,NULL,(MUSIC_TIME)i64StopTime,dwFlags);
    }
    if (dwFlags & DMUS_SEGF_AUTOTRANSITION)
    {
        // I this is an autotransition, it will only work if the currently playing segment in question
        // is a member of a song. So, check the segstate, segment, song, and audiopath
        // to find the segstate. And, if found, see if it is part of a song. If so,
        // then go ahead and do the transition.
        EnterCriticalSection(&m_SegmentCrSec);
        BOOL fTransition = FALSE;
        dwFlags &= ~DMUS_SEGF_AUTOTRANSITION;
        CSegState *pCState = NULL;
        // First, see if this is a segstate.
        HRESULT hrTemp = pObjectToStop->QueryInterface(IID_CSegState,(void **)&pCState);
        if (FAILED(hrTemp))
        {
            // Segstate failed. Is this a Song? If so, find the first correlating segstate.
            CSong *pCSong = NULL;
            CAudioPath *pCAudioPath = NULL;
            CSegment *pCSegment = NULL;
            hrTemp = pObjectToStop->QueryInterface(IID_CSong,(void **)&pCSong);
            if (FAILED(hrTemp))
            {
                hrTemp = pObjectToStop->QueryInterface(IID_CSegment,(void **)&pCSegment);
            }
            if (FAILED(hrTemp))
            {
                hrTemp = pObjectToStop->QueryInterface(IID_CAudioPath,(void **)&pCAudioPath);
            }
            if (SUCCEEDED(hrTemp))
            {
                CSegState *pNode;
                DWORD dwCount;
                for (dwCount = SQ_PRI_WAIT; dwCount <= SQ_SEC_DONE; dwCount++)
                {
                    for( pNode = m_SegStateQueues[dwCount].GetHead(); pNode; pNode = pNode->GetNext() )
                    {
                        if (pNode->m_fCanStop)
                        {
                            // Can only do this if the segstate ultimately points to a song.
                            if (pNode->m_pSegment && pNode->m_pSegment->m_pSong)
                            {
                                if ((pNode->m_pSegment == pCSegment) ||
                                    (pNode->m_pSegment->m_pSong == pCSong) ||
                                    (pCAudioPath && (pNode->m_pAudioPath == pCAudioPath)))
                                {
                                    pCState = pNode;
                                    pCState->AddRef();
                                    break;
                                }
                            }
                        }
                    }
                    if (pCState) break;
                }
            }
            if (pCSong) pCSong->Release();
            else if (pCAudioPath) pCAudioPath->Release();
            else if (pCSegment) pCSegment->Release();
        }
        if (pCState)
        {
            CSegment *pPriorSeg = pCState->m_pSegment;
            if (pPriorSeg)
            {
                pSong = pPriorSeg->m_pSong;
                if (pSong)
                {
                    // If this is an autotransition, compose a transition segment from the
                    // current position in the song and play it.
                    // This will, in turn, call stop on the song, so we don't need to do it here.
                    // First, calculate the time to start the transition.
                    REFERENCE_TIME rtTime;
                    if (i64StopTime == 0)
                    {
                        GetQueueTime( &rtTime );
                    }
                    else if (dwFlags & DMUS_SEGF_REFTIME)
                    {
                        rtTime = i64StopTime;
                    }
                    else
                    {
                        MusicToReferenceTime((MUSIC_TIME) i64StopTime,&rtTime);
                    }
                    REFERENCE_TIME rtResolved;
                    GetResolvedTime(rtTime, &rtResolved,dwFlags);
                    MUSIC_TIME mtTime;  // Actual time to start transition.
                    ReferenceToMusicTime(rtResolved,&mtTime);

                    CSegment *pTransition = NULL;
                    // Now, get the transition.
                    DMUS_IO_TRANSITION_DEF Transition;
                    if (SUCCEEDED(pSong->GetTransitionSegment(pPriorSeg,NULL,&Transition)))
                    {
                        if (Transition.dwTransitionID != DMUS_SONG_NOSEG)
                        {
                            if (S_OK == pSong->GetPlaySegment(Transition.dwTransitionID,&pTransition))
                            {
                                dwFlags = Transition.dwPlayFlags;
                            }
                        }
                    }
                    if (pTransition)
                    {
                        IDirectMusicSegment *pITransSegment = NULL;
                        pTransition->Compose(mtTime - pCState->m_mtOffset, pPriorSeg, NULL, &pITransSegment);
                        // Now, if we successfully composed a transition segment, set it up to be the one we
                        // will play first. Later, we fill call PlaySegment() with pPlayAfter, to queue it
                        // to play after the transition.
                        if (pITransSegment)
                        {
                            hr = PlaySegmentEx(pITransSegment,NULL,NULL,dwFlags,i64StopTime,NULL,(IDirectMusicSegmentState *)pCState,NULL);
                            pITransSegment->Release();
                            fTransition = TRUE;
                        }
                        pTransition->Release();
                    }
                }
            }
            pCState->Release();
        }
        LeaveCriticalSection(&m_SegmentCrSec);
        if (fTransition)
        {
            return hr;
        }
    }
    if (SUCCEEDED(pObjectToStop->QueryInterface(IID_IDirectMusicSegmentState,(void **) &pState)))
    {
        hr = Stop(NULL,pState,(MUSIC_TIME)i64StopTime,dwFlags);
        pState->Release();
    }
    else if (SUCCEEDED(pObjectToStop->QueryInterface(IID_IDirectMusicSegment,(void **) &pSegment)))
    {
        hr = Stop(pSegment,NULL,(MUSIC_TIME)i64StopTime,dwFlags);
        pSegment->Release();
    }
    else if (SUCCEEDED(pObjectToStop->QueryInterface(IID_CAudioPath,(void **) &pAudioPath)))
    {
        pAudioPath->Release();
        EnterCriticalSection(&m_SegmentCrSec);
        CSegState *pNode;
        DWORD dwCount;
        for (dwCount = SQ_PRI_WAIT; dwCount <= SQ_SEC_DONE; dwCount++)
        {
            CSegState *pNext;
            for( pNode = m_SegStateQueues[dwCount].GetHead(); pNode; pNode = pNext )
            {
                pNext = pNode->GetNext();
                if (pNode->m_fCanStop && (pNode->m_pAudioPath == pAudioPath))
                {
                    hr = Stop(NULL,(IDirectMusicSegmentState *)pNode,(MUSIC_TIME)i64StopTime,dwFlags);
                }
            }
        }
        LeaveCriticalSection(&m_SegmentCrSec);
    }
    else if (SUCCEEDED(pObjectToStop->QueryInterface(IID_CSong,(void **) &pSong)))
    {
        pSong->Release();
        EnterCriticalSection(&m_SegmentCrSec);
        CSegState *pNode;
        DWORD dwCount;
        for (dwCount = SQ_PRI_WAIT; dwCount <= SQ_SEC_DONE; dwCount++)
        {
            for( pNode = m_SegStateQueues[dwCount].GetHead(); pNode; pNode = pNode->GetNext() )
            {
                if (pNode->m_fCanStop && pNode->m_pSegment && (pNode->m_pSegment->m_pSong == pSong))
                {
                    hr = Stop(NULL,(IDirectMusicSegmentState *)pNode,(MUSIC_TIME)i64StopTime,dwFlags);
                }
            }
        }
        LeaveCriticalSection(&m_SegmentCrSec);
     }

    return hr;
}


HRESULT STDMETHODCALLTYPE CPerformance::Stop(
    IDirectMusicSegment *pISegment, // @parm The Segment to stop playing. All SegmentState's based upon this Segment are
                                    // stopped playing at time <p mtTime>.
    IDirectMusicSegmentState *pISegmentState, // @parm The SegmentState to stop playing.
    MUSIC_TIME mtTime,  // @parm The time at which to stop the Segments, Segment State, or everything. If
                                    // this time is in the past, stop everything right away. Therefore, a value of
                                    // 0 indicates stop everything NOW.
    DWORD dwFlags)      // @parm Flag that indicates whether we should stop immediately at time <p mtTime>,
                                    // or on the grid, measure, or beat following <p mtTime>. This is only valid in
                                    // relation to the currently playing primary segment. (For flag descriptions,
                                    // see <t DMPLAYSEGFLAGS>.)
{
    V_INAME(IDirectMusicPerformance::Stop);
    V_INTERFACE_OPT(pISegment);
    V_INTERFACE_OPT(pISegmentState);

    EnterCriticalSection(&m_SegmentCrSec);

    CSegment *pSegment = NULL;
    CSegState *pSegmentState = NULL;
TraceI(0,"Stopping Segment %lx, SegState %lx at time %ld, flags %lx\n",pISegment,pISegmentState,mtTime,dwFlags);
    if (pISegmentState)
    {
        if (SUCCEEDED(pISegmentState->QueryInterface(IID_CSegState,(void **)&pSegmentState)))
        {
            pISegmentState->Release();
        }
        else
        {
            Trace(1,"Error: Pointer in SegState parameter to Stop() is invalid.\n");
            return E_INVALIDARG;
        }
    }
    if (pISegment)
    {
        if (SUCCEEDED(pISegment->QueryInterface(IID_CSegment,(void **)&pSegment)))
        {
            pISegment->Release();
        }
        else
        {
            Trace(1,"Error: Pointer in Segment parameter to Stop() is invalid.\n");
            return E_INVALIDARG;
        }
    }
    if (pSegmentState)
    {
        // If this is the starting segstate from a playing song, find the
        // current active segstate within that song.
        // The current active segstate keeps a pointer to
        // this segstate.
        if (pSegmentState->m_fSongMode)
        {
            CSegState* pNode;
            DWORD dwCount;
            for (dwCount = 0; dwCount < SQ_COUNT; dwCount++)
            {
                for( pNode = m_SegStateQueues[dwCount].GetHead(); pNode; pNode = pNode->GetNext() )
                {
                    if (pNode->m_pSongSegState == pSegmentState)
                    {
                        pSegmentState = pNode;
                        dwCount = SQ_COUNT;
                        break;
                    }
                }
            }
        }
    }
    if( dwFlags & DMUS_SEGF_DEFAULT )
    {
        DWORD   dwNewRes = 0;
        if( pSegment )
        {
            pSegment->GetDefaultResolution( &dwNewRes );
        }
        else if( pSegmentState )
        {
            IDirectMusicSegment*    pSegTemp;
            if( SUCCEEDED( pSegmentState->GetSegment( &pSegTemp ) ) )
            {
                pSegTemp->GetDefaultResolution( &dwNewRes );
                pSegTemp->Release();
            }
            else
            {
                dwNewRes = 0;
            }
        }
        else
        {
            dwNewRes = 0;
        }
        dwFlags |= dwNewRes;
        dwFlags &= ~DMUS_SEGF_DEFAULT;
    }
    // Make sure mtTime is greater or equal to QueueTime, which is the last time notes were
    // queued down (or latency time, whichever is later) so we can stop everything after it.
    MUSIC_TIME mtLatency;
    REFERENCE_TIME rtQueueTime;
    GetQueueTime( &rtQueueTime );
    ReferenceToMusicTime( rtQueueTime, &mtLatency );
    if( mtTime < mtLatency ) mtTime = mtLatency;
    // Resolve the time according to the resolution
    mtTime = ResolveTime( mtTime, dwFlags, NULL );
    // if mtTime is less than the current transported time, we can take
    // care of the Stop now. Otherwise, we need to cue a Stop PMsg and
    // take care of it at QUEUE time.
    if( mtTime <= m_mtTransported )
    {
        if( pSegmentState )
        {
            DoStop( pSegmentState, mtTime, TRUE );
            if( pSegment )
            {
                DoStop( pSegment, mtTime, TRUE );
            }
        }
        else
        {
            DoStop( pSegment, mtTime, TRUE );
        }
    }
    else
    {
        // find and mark the segment and/or segment state to not play beyond
        // the stop point.
        CSegState* pNode;
        DWORD dwCount;
        for (dwCount = SQ_PRI_PLAY; dwCount <= SQ_SEC_PLAY; dwCount++)
        {
            for( pNode = m_SegStateQueues[dwCount].GetHead(); pNode; pNode = pNode->GetNext() )
            {
                if( (pNode->m_pSegment == pSegment) ||
                    (pNode == pSegmentState) )
                {
                    if (pNode->m_fCanStop)
                    {
                        pNode->m_mtStopTime = mtTime;
                        // Make sure GetParams ignore the rest of the segment from now on.
                        if (mtTime < pNode->m_mtEndTime)
                        {
                            pNode->m_mtLength = mtTime - pNode->m_mtResolvedStart +
                                pNode->m_mtStartPoint;
                            if (pNode->m_mtLength < 0)
                            {
                                pNode->m_mtLength = 0;
                            }
                            // Make endtime one greater than mtTime so Abort notification will still happen.
                            pNode->m_mtEndTime = mtTime + 1;
                        }
                        // Force the tempo map to be recalculated IF this has a tempo track.
                        RecalcTempoMap(pNode,mtTime);
                    }
                }
            }
        }

        // create a Stop PMsg and cue it for QUEUE time
        // I've removed this to fix bugs. A stop message at queue time, 
        // if in a controlling or primary segment, results in invalidation.
        // This is particularily bad for controlling segments.
        // Can't figure out why we even need the stop message...
/*      DMUS_PMSG* pPMsg;

        if( SUCCEEDED( AllocPMsg( sizeof(DMUS_PMSG), &pPMsg )))
        {
            pPMsg->dwType = DMUS_PMSGT_STOP;
            pPMsg->mtTime = mtTime;
            pPMsg->dwFlags = DMUS_PMSGF_MUSICTIME | DMUS_PMSGF_TOOL_QUEUE;
            if( pSegment )
            {
                pSegment->QueryInterface( IID_IUnknown, (void**)&pPMsg->punkUser );
                if( pSegmentState )
                {
                    // if there is also a segment state pointer, we need to create two
                    // pmsg's
                    DMUS_PMSG* pPMsg2;

                    if( SUCCEEDED( AllocPMsg( sizeof(DMUS_PMSG), &pPMsg2 )))
                    {
                        pPMsg2->dwType = DMUS_PMSGT_STOP;
                        pPMsg2->mtTime = mtTime;
                        pPMsg2->dwFlags = DMUS_PMSGF_MUSICTIME | DMUS_PMSGF_TOOL_QUEUE;
                        pSegmentState->QueryInterface( IID_IUnknown, (void**)&pPMsg2->punkUser );
                        pPMsg2->pTool = this;
                        AddRef();
                        if(FAILED(SendPMsg( pPMsg2 )))
                        {
                            FreePMsg(pPMsg2);
                        }
                    }
                }
            }
            else if( pSegmentState )
            {
                pSegmentState->QueryInterface( IID_IUnknown, (void**)&pPMsg->punkUser );
            }
            pPMsg->pTool = this;
            AddRef();
            if(FAILED(SendPMsg( pPMsg )))
            {
                FreePMsg(pPMsg);
            }
        }*/
    }
    LeaveCriticalSection(&m_SegmentCrSec);
    return S_OK;
}

void CPerformance::ResetAllControllers(CChannelMap* pChannelMap, REFERENCE_TIME rtTime, bool fGMReset)

{
    DWORD dwIndex = pChannelMap->dwPortIndex;
    DWORD dwGroup = pChannelMap->dwGroup;
    DWORD dwMChannel = pChannelMap->dwMChannel;

    EnterCriticalSection(&m_PChannelInfoCrSec);
    IDirectMusicPort* pPort = m_pPortTable[dwIndex].pPort;
    IDirectMusicBuffer* pBuffer = m_pPortTable[dwIndex].pBuffer;
    if( pPort && pBuffer )
    {
        m_pPortTable[dwIndex].fBufferFilled = TRUE;
        if (!rtTime)
        {
            rtTime = m_pPortTable[dwIndex].rtLast + 1;
        }
        else
        {
            m_pPortTable[dwIndex].rtLast = rtTime;
        }
        pChannelMap->Reset(true);
        DWORD dwMsg = dwMChannel | MIDI_CCHANGE | (MIDI_CC_ALLSOUNDSOFF << 8); // 0x78 is all sounds off.
        if( FAILED( pBuffer->PackStructured( rtTime, dwGroup, dwMsg ) ) )
        {
            pPort->PlayBuffer( pBuffer );
            pBuffer->Flush();
            // try one more time
            pBuffer->PackStructured( rtTime, dwGroup, dwMsg );
        }
        dwMsg = dwMChannel | MIDI_CCHANGE | (MIDI_CC_RESETALL << 8) | (1 << 16) ; // 0x79 is reset all controllers. Data byte set to indicate volume and pan too.
        if( FAILED( pBuffer->PackStructured( rtTime + 30 * REF_PER_MIL, dwGroup, dwMsg ) ) )
        {
            pPort->PlayBuffer( pBuffer );
            pBuffer->Flush();
            // try one more time
            pBuffer->PackStructured( rtTime + (30 * REF_PER_MIL), dwGroup, dwMsg );
        }
        // Send one GM Reset per channel group, but only under DX8 (and only if we need to).
        if ((dwMChannel == 0) && (m_dwVersion >= 8) && fGMReset)
        {
            // create a buffer of the right size
            DMUS_BUFFERDESC dmbd;
            IDirectMusicBuffer *pLocalBuffer;
            static BYTE abGMReset[6] = { (BYTE)MIDI_SYSX,0x7E,0x7F,9,1,(BYTE)MIDI_EOX };
            memset( &dmbd, 0, sizeof(DMUS_BUFFERDESC) );
            dmbd.dwSize = sizeof(DMUS_BUFFERDESC);
            dmbd.cbBuffer = 50;

            EnterCriticalSection(&m_MainCrSec);
            if( SUCCEEDED( m_pDirectMusic->CreateMusicBuffer(&dmbd, &pLocalBuffer, NULL)))
            {
                if( SUCCEEDED( pLocalBuffer->PackUnstructured( rtTime + (30 * REF_PER_MIL), dwGroup,
                    6, abGMReset ) ) )
                {
                    pPort->PlayBuffer(pLocalBuffer);
                }
                pLocalBuffer->Release();
            }
            LeaveCriticalSection(&m_MainCrSec);
        }
        m_rtEarliestStartTime = rtTime + (60 * REF_PER_MIL); // Give synth chance to stabilize
                                                             // before next start.
    }
    LeaveCriticalSection(&m_PChannelInfoCrSec);
}


void CPerformance::ResetAllControllers( REFERENCE_TIME rtTime )
{
    EnterCriticalSection(&m_PChannelInfoCrSec);

    CChannelBlock* pChannelBlock;
    SendBuffers();
    for( pChannelBlock = m_ChannelBlockList.GetHead(); pChannelBlock; pChannelBlock = pChannelBlock->GetNext() )
    {
        CChannelMap* pChannelMap;
        for( DWORD dwPChannel = pChannelBlock->m_dwPChannelStart;
            dwPChannel < pChannelBlock->m_dwPChannelStart + PCHANNEL_BLOCKSIZE;
            dwPChannel++ )
        {
            pChannelMap = &pChannelBlock->m_aChannelMap[dwPChannel - pChannelBlock->m_dwPChannelStart];
            if( pChannelMap->dwGroup ) // Valid group?
            {
                // Reset controllers and send a GM reset.
                ResetAllControllers(pChannelMap, rtTime, true);
            }
        }
    }
    SendBuffers();

    LeaveCriticalSection(&m_PChannelInfoCrSec);
}

// internal: return CSegState* at time mtTime
// only call this from within a segment critical section
CSegState* CPerformance::GetPrimarySegmentAtTime( MUSIC_TIME mtTime )
{
    CSegState* pSegNode;
    CSegState* pSegReturn = NULL;
    BOOL fCheckedPri = FALSE;
    for( pSegNode = m_SegStateQueues[SQ_PRI_DONE].GetHead(); pSegNode; pSegNode = pSegNode->GetNext() )
    {
        // if we're checking the past list, only check up until the last time played.
        if( (mtTime >= pSegNode->m_mtResolvedStart) && (mtTime <= pSegNode->m_mtLastPlayed) )
        {
            pSegReturn = pSegNode;
            break;
        }
    }
    for( pSegNode = m_SegStateQueues[SQ_PRI_PLAY].GetHead(); pSegNode; pSegNode = pSegNode->GetNext() )
    {
        MUSIC_TIME mtTest = mtTime;
        MUSIC_TIME mtOffset;
        DWORD dwRepeat;
        // if we're checking the current list, check the full segment time
        if( S_OK == pSegNode->ConvertToSegTime( &mtTest, &mtOffset, &dwRepeat ))
        {
            pSegReturn = pSegNode;
            break;
        }
    }
    if (!pSegReturn)
    {
        for( pSegNode = m_SegStateQueues[SQ_PRI_WAIT].GetHead(); pSegNode; pSegNode = pSegNode->GetNext() )
        {
            MUSIC_TIME mtTest = mtTime;
            MUSIC_TIME mtOffset;
            DWORD dwRepeat;
            // if we're checking the current list, check the full segment time
            if( S_OK == pSegNode->ConvertToSegTime( &mtTest, &mtOffset, &dwRepeat ))
            {
                pSegReturn = pSegNode;
                break;
            }
        }
    }
    return pSegReturn;
}

/*

  @method HRESULT | IDirectMusicPerformance | GetSegmentState |
  Returns the Primary SegmentState at time <p mtTime>.

  @rvalue S_OK | Success.
  @rvalue E_POINTER | ppSegmentState is NULL or invalid.
  @rvalue DMUS_E_NOT_FOUND | There is no currently playing SegmentState or one at <p mtTime>.

  @comm This function is intended for routines that need to access the currently
  playing SegmentState, e.g. to obtain the chord or command track. "Currently
  Playing" in this context means that it is being called into to perform messages.
  I.e., this includes all latencies and doesn't imply that this
  SegmentState is currenty being "heard" through the speakers.

*/
HRESULT STDMETHODCALLTYPE CPerformance::GetSegmentState(
    IDirectMusicSegmentState **ppSegmentState,  // @parm Returns the SegmentState pointer to the one currently playing.
                                                // The caller is responsible for calling Release on this pointer.
    MUSIC_TIME mtTime ) // @parm Return the SegmentState which played, is playing, or will
                        // be playing at mtTime. To get the currently playing segment, pass the
                        // mtTime retrieved from <om .GetTime>.
{
    V_INAME(IDirectMusicPerformance::GetSegmentState);
    V_PTRPTR_WRITE(ppSegmentState);

    CSegState* pSegNode;
    HRESULT hr;
    EnterCriticalSection(&m_SegmentCrSec);
    if( pSegNode = GetPrimarySegmentAtTime( mtTime ))
    {
        *ppSegmentState = pSegNode;
        pSegNode->AddRef();
        hr = S_OK;
    }
    else
    {
        Trace(3,"Unable to find a segment state at time %ld\n",mtTime);
        hr  = DMUS_E_NOT_FOUND;
    }
    LeaveCriticalSection(&m_SegmentCrSec);
    return hr;
}

/*
  @method HRESULT | IDirectMusicPerformance | SetPrepareTime |
  Sets the prepare time. The prepare time is the amount of time ahead that
  <om IDirectMusicTrack.Play> is called before the messages should actually
  be heard through the loudspeaker. The midi messages from the tracks are placed in
  the early queue, are processed by Tools, and then placed in the near-time
  queue to await being sent to the midi ports.

  @rvalue S_OK | Success.
  @comm The default value is 1000 milliseconds.
*/
HRESULT STDMETHODCALLTYPE CPerformance::SetPrepareTime(
    DWORD dwMilliSeconds) // @parm The amount of time.
{
    m_dwPrepareTime = dwMilliSeconds;
    return S_OK;
}

/*
  @method HRESULT | IDirectMusicPerformance | GetPrepareTime |
  Gets the prepare time. The prepare time is the amount of time ahead that
  <om IDirectMusicTrack.Play> is called before the messages should actually
  be heard through the loudspeaker. The midi messages from the tracks are placed in
  the early queue, are processed by Tools, and then placed in the near-time
  queue to await being sent to the midi ports.

  @rvalue S_OK | Success.
  @rvalue E_POINTER | pdwMilliSeconds is NULL or invalid.
  @comm The default value is 1000 milliseconds.
*/
HRESULT STDMETHODCALLTYPE CPerformance::GetPrepareTime(
    DWORD* pdwMilliSeconds) // @parm The amount of time.
{
    V_INAME(IDirectMusicPerformance::GetPrepareTime);
    V_PTR_WRITE(pdwMilliSeconds,DWORD);

    *pdwMilliSeconds = m_dwPrepareTime;
    return S_OK;
}

/*
  @method HRESULT | IDirectMusicPerformance | SetBumperLength |
  Sets the bumper length. The bumper length is the amount of time to buffer ahead
  of the Port's latency for midi messages to be sent to the Port for rendering.

  @rvalue S_OK | Success.
  @comm The default value is 50 milliseconds.
*/
HRESULT STDMETHODCALLTYPE CPerformance::SetBumperLength(
    DWORD dwMilliSeconds)   // @parm The amount of time.
{
    m_dwBumperLength = dwMilliSeconds;
    m_rtBumperLength = m_dwBumperLength * REF_PER_MIL;
    return S_OK;
}

/*
  @method HRESULT | IDirectMusicPerformance | GetBumperLength |
  Gets the bumper length. The bumper length is the amount of time to buffer ahead
  of the Port's latency for midi messages to be sent to the Port for rendering.

  @rvalue S_OK | Success.
  @rvalue E_POINTER | pdwMilliSeconds is NULL or invalid.
  @comm The default value is 50 milliseconds.
*/
HRESULT STDMETHODCALLTYPE CPerformance::GetBumperLength(
    DWORD* pdwMilliSeconds) // @parm The amount of time.
{
    V_INAME(IDirectMusicPerformance::GetBumperLength);
    V_PTR_WRITE(pdwMilliSeconds,DWORD);

    *pdwMilliSeconds = m_dwBumperLength;
    return S_OK;
}

#define RESOLVE_FLAGS (DMUS_TIME_RESOLVE_AFTERPREPARETIME | \
                       DMUS_TIME_RESOLVE_AFTERLATENCYTIME | \
                       DMUS_TIME_RESOLVE_AFTERQUEUETIME | \
                       DMUS_TIME_RESOLVE_BEAT | \
                       DMUS_TIME_RESOLVE_MEASURE | \
                       DMUS_TIME_RESOLVE_GRID | \
                       DMUS_TIME_RESOLVE_MARKER | \
                       DMUS_TIME_RESOLVE_SEGMENTEND)


HRESULT STDMETHODCALLTYPE CPerformance::SendPMsg(
    DMUS_PMSG *pDMUS_PMSG)

{
    V_INAME(IDirectMusicPerformance::SendPMsg);
    if( m_dwVersion < 8)
    {
        V_BUFPTR_WRITE(pDMUS_PMSG,sizeof(DMUS_PMSG));
    }
    else
    {
#ifdef DBG
        V_BUFPTR_WRITE(pDMUS_PMSG,sizeof(DMUS_PMSG));
#else
        if (!pDMUS_PMSG)
        {
            return E_POINTER;
        }
#endif
    }
    EnterCriticalSection(&m_MainCrSec);
    if( m_pClock == NULL )
    {
        LeaveCriticalSection(&m_MainCrSec);
        Trace(1,"Error: Unable to Send PMsg because performance not initialized.\n");
        return DMUS_E_NO_MASTER_CLOCK;
    }
    LeaveCriticalSection(&m_MainCrSec);

    if (pDMUS_PMSG->dwSize < sizeof(DMUS_PMSG))
    {
        TraceI(1,"Warning: PMsg size field has been cleared.\n");
    }

    // If this is a PMsg that was marked by STampPMsg as one that should be removed,
    // do so now.
    if (pDMUS_PMSG->dwPChannel == DMUS_PCHANNEL_KILL_ME)
    {
        FreePMsg(pDMUS_PMSG);
        return S_OK;
    }

    EnterCriticalSection(&m_PipelineCrSec);
    PRIV_PMSG* pPrivPMsg = DMUS_TO_PRIV(pDMUS_PMSG);
    if( ( pPrivPMsg->dwPrivFlags & PRIV_FLAG_QUEUED ) ||
        ( ( pPrivPMsg->dwPrivFlags & PRIV_FLAG_ALLOC_MASK ) != PRIV_FLAG_ALLOC ) )
    {
        Trace(1, "Error: Attempt to send an improperly allocated PMsg, or trying to send it after it is already sent.\n" );
        LeaveCriticalSection(&m_PipelineCrSec);
        return DMUS_E_ALREADY_SENT;
    }

    if (m_dwVersion >= 8)
    {
        // If the music and ref times are both 0, set to latency time.
        if ((pDMUS_PMSG->mtTime == 0) && ( pDMUS_PMSG->rtTime == 0 ))
        {
            // If this needs to resolve, use the worse case latency
            // because this needs to sync with other pmsgs.
            if (pDMUS_PMSG->dwFlags & RESOLVE_FLAGS)
            {
                GetLatencyTime(&pDMUS_PMSG->rtTime);
            }
            else
            {
                // Otherwise, we want to play as soon as possible.
                pDMUS_PMSG->rtTime = GetTime();
            }
            pDMUS_PMSG->dwFlags |= DMUS_PMSGF_REFTIME;
            pDMUS_PMSG->dwFlags &= ~DMUS_PMSGF_MUSICTIME;
        }
    }

    // fill in missing time value
    if (!(pDMUS_PMSG->dwFlags & DMUS_PMSGF_MUSICTIME))
    {
        if( !(pDMUS_PMSG->dwFlags & DMUS_PMSGF_REFTIME ) )
        {
            LeaveCriticalSection(&m_PipelineCrSec);
            Trace(1,"Error: Unable to send PMsg because neither clock time (DMUS_PMSGF_REFTIME) nor music time (DMUS_PMSGF_MUSICTIME) has been set.\n");
            return E_INVALIDARG; // one or the other MUST be set
        }
        // quantize to resolution boundaries
        GetResolvedTime( pDMUS_PMSG->rtTime, &pDMUS_PMSG->rtTime, pDMUS_PMSG->dwFlags );
        pDMUS_PMSG->dwFlags &= ~RESOLVE_FLAGS;
        // if time is zero, set it to time now plus latency
        if( pDMUS_PMSG->rtTime == 0 )
        {
            pDMUS_PMSG->rtTime = GetLatency();
        }
        ReferenceToMusicTime(pDMUS_PMSG->rtTime,
            &pDMUS_PMSG->mtTime);
        pDMUS_PMSG->dwFlags |= DMUS_PMSGF_MUSICTIME;
    }
    else if (!(pDMUS_PMSG->dwFlags & DMUS_PMSGF_REFTIME))
    {
        MusicToReferenceTime(pDMUS_PMSG->mtTime,
            &pDMUS_PMSG->rtTime);
        pDMUS_PMSG->dwFlags |= DMUS_PMSGF_REFTIME;
        // quantize to resolution boundaries
        REFERENCE_TIME rtNew;
        GetResolvedTime( pDMUS_PMSG->rtTime, &rtNew, pDMUS_PMSG->dwFlags );
        pDMUS_PMSG->dwFlags &= ~RESOLVE_FLAGS;
        if( rtNew != pDMUS_PMSG->rtTime )
        {
            pDMUS_PMSG->rtTime = rtNew;
            ReferenceToMusicTime( pDMUS_PMSG->rtTime, &pDMUS_PMSG->mtTime );
        }
    }

    // insert into the proper queue by music value
    if (pDMUS_PMSG->dwFlags & DMUS_PMSGF_TOOL_QUEUE)
    {
        m_NearTimeQueue.Enqueue(pPrivPMsg);
    }
    else if (pDMUS_PMSG->dwFlags & DMUS_PMSGF_TOOL_ATTIME)
    {
        m_OnTimeQueue.Enqueue(pPrivPMsg);
    }
    else // (pDMUS_PMSG->dwFlags & DMUS_PMSGF_TOOL_IMMEDIATE)
    {
        pDMUS_PMSG->dwFlags |= DMUS_PMSGF_TOOL_IMMEDIATE;
        m_EarlyQueue.Enqueue(pPrivPMsg);
    }
    LeaveCriticalSection(&m_PipelineCrSec);
    return S_OK;
}

/*

  Call this only from within a PipelineCrSec.
*/
void CPerformance::RevalidateRefTimes( CPMsgQueue * pList, MUSIC_TIME mtTime )
{
    PRIV_PMSG* pCheck;
    BOOL fError = FALSE;
    for( pCheck = pList->GetHead(); pCheck; pCheck = pCheck->pNext )
    {
        if (pCheck->mtTime > mtTime)
        {
            if (pCheck->dwFlags & DMUS_PMSGF_LOCKTOREFTIME)
            {
                ReferenceToMusicTime(pCheck->rtTime,&pCheck->mtTime);
            }
            else // if(pCheck->dwFlags & DMUS_PMSGF_MUSICTIME)
            {
                MusicToReferenceTime(pCheck->mtTime,&pCheck->rtTime);
            }
        }
    }
    // Make sure that we do not end up with out of order RTimes. This can happen with
    // DMUS_PMSGF_LOCKTOREFTIME messages or very abrupt changes in tempo.
    for( pCheck = pList->GetHead(); pCheck; pCheck = pCheck->pNext )
    {
        if (pCheck->pNext && ( pCheck->rtTime > pCheck->pNext->rtTime ))
        {
            fError = TRUE;  // Need to sort the list.
        }
    }
    if (fError)
    {
        TraceI(2,"Rearrangement of times in message list due to tempo change, resorting\n");
        pList->Sort();
    }
}

void CPerformance::AddToTempoMap( double dblTempo, MUSIC_TIME mtTime, REFERENCE_TIME rtTime )
{
    DMInternalTempo* pITempo = NULL;

    if( FAILED( AllocPMsg( sizeof(DMInternalTempo), (PRIV_PMSG**)&pITempo )))
    {
        return; // out of memory!
    }
    if( dblTempo > DMUS_TEMPO_MAX ) dblTempo = DMUS_TEMPO_MAX;
    else if( dblTempo < DMUS_TEMPO_MIN ) dblTempo = DMUS_TEMPO_MIN;
    pITempo->tempoPMsg.dblTempo = dblTempo;
    pITempo->tempoPMsg.rtTime = rtTime;
    pITempo->tempoPMsg.mtTime = mtTime;
    pITempo->tempoPMsg.dwFlags = DMUS_PMSGF_MUSICTIME | DMUS_PMSGF_REFTIME;
    pITempo->pNext = NULL;
    // set the relative tempo field
    EnterCriticalSection(&m_GlobalDataCrSec);
    pITempo->fltRelTempo = m_fltRelTempo;
    // add the tempo event to the tempo map and clear the tool and graph pointers
    pITempo->tempoPMsg.pTool = NULL;
    EnterCriticalSection(&m_PipelineCrSec);
    // remove stale tempo events from the tempo map.
    // as long as there is another tempo with a time stamp before the current
    // time, get rid of the first in the list.
    REFERENCE_TIME rtNow = GetTime() - (10000 * 1000); // keep around for a second.
    PRIV_PMSG* pCheck;
    while (pCheck = m_TempoMap.FlushOldest(rtNow))
    {
        m_OldTempoMap.Enqueue(pCheck);
    }
    // add the new tempo event to the queue
    m_TempoMap.Enqueue( (PRIV_PMSG*) pITempo );
    // now that it's been added, scan forward from it and change the relative tempo
    // times of everyone after it
    DMInternalTempo* pChange;
    for( pChange = (DMInternalTempo*)pITempo->pNext; pChange;
        pChange = (DMInternalTempo*)pChange->pNext )
    {
        pChange->fltRelTempo = pITempo->fltRelTempo;
    }
    // remove stale tempo events from the old tempo map.
    // as long as there is another tempo with a time stamp before the current
    // time, get rid of the first in the list.
    rtNow = GetTime() - ((REFERENCE_TIME)10000 * 300000); // keep around for five minutes.
    while (pCheck = m_OldTempoMap.FlushOldest(rtNow))
    {
        FreePMsg(pCheck);
    }
    m_fTempoChanged = TRUE;
    LeaveCriticalSection(&m_PipelineCrSec);
    LeaveCriticalSection(&m_GlobalDataCrSec);
}

void CPerformance::AddEventToTempoMap( PRIV_PMSG* pEvent )
{
    PRIV_TEMPO_PMSG* pTempo = (PRIV_TEMPO_PMSG*)pEvent;
    MUSIC_TIME mtTime = pTempo->tempoPMsg.mtTime;
    AddToTempoMap( pTempo->tempoPMsg.dblTempo, mtTime, pTempo->tempoPMsg.rtTime );
    pEvent->dwPrivFlags = PRIV_FLAG_ALLOC;
    EnterCriticalSection(&m_GlobalDataCrSec);
    EnterCriticalSection(&m_PipelineCrSec);
    // revalidate the ref times of the events in the queues
    RevalidateRefTimes( &m_TempoMap, mtTime );
    RevalidateRefTimes( &m_OnTimeQueue, mtTime );
    RevalidateRefTimes( &m_NearTimeQueue, mtTime );
    RevalidateRefTimes( &m_EarlyQueue, mtTime );
    m_fTempoChanged = TRUE;
    LeaveCriticalSection(&m_PipelineCrSec);
    LeaveCriticalSection(&m_GlobalDataCrSec);
    RecalcTempoMap(NULL, mtTime+1, false);
}

#define TEMPO_AHEAD 768 * 4 * 10    // 10 measures ahead is plenty!

void CPerformance::IncrementTempoMap()

{
    if (m_mtTempoCursor <= (m_mtTransported + TEMPO_AHEAD))
    {
        UpdateTempoMap(m_mtTempoCursor, false, NULL);
    }
}

void CPerformance::RecalcTempoMap(CSegState *pSegState, MUSIC_TIME mtStart, bool fAllDeltas)

/*  Called whenever a primary or controlling segment that has a tempo
    track is played or stopped.
    1) Convert the music time at transport time to ref time using the old
    map.
    2) Build a replacement tempo map starting at mtStart, by
    calling GetParam() until there is no next time.
    3) Install the new map.
    4) Convert with the new map.
    5) If the two numbers are not identical, recalculate all message times.
*/

{
    if( mtStart > 0) // Don't do this for invalid values.
    {
        if (!pSegState || (pSegState->m_pSegment && pSegState->m_pSegment->IsTempoSource()))
        {
            REFERENCE_TIME rtCompareTime;
            REFERENCE_TIME rtAfterTime;
            MUSIC_TIME mtCompareTime = m_mtTransported;
            MusicToReferenceTime(mtCompareTime,&rtCompareTime);
            EnterCriticalSection(&m_PipelineCrSec);
            FlushEventQueue( 0, &m_TempoMap, rtCompareTime, rtCompareTime, FALSE );
            LeaveCriticalSection(&m_PipelineCrSec);
            UpdateTempoMap(mtStart, true, pSegState, fAllDeltas);
            MusicToReferenceTime(mtCompareTime,&rtAfterTime);
            if (rtAfterTime != rtCompareTime)
            {
                EnterCriticalSection(&m_GlobalDataCrSec);
                EnterCriticalSection(&m_PipelineCrSec);
                // revalidate the ref times of the events in the queues
                RevalidateRefTimes( &m_TempoMap, mtStart );
                RevalidateRefTimes( &m_OnTimeQueue, mtStart );
                RevalidateRefTimes( &m_NearTimeQueue, mtStart );
                RevalidateRefTimes( &m_EarlyQueue, mtStart );
                m_fTempoChanged = TRUE;
                LeaveCriticalSection(&m_PipelineCrSec);
                LeaveCriticalSection(&m_GlobalDataCrSec);
            }
        }
    }
}


void CPerformance::UpdateTempoMap(MUSIC_TIME mtStart, bool fFirst, CSegState *pSegState, bool fAllDeltas)

{
    HRESULT hr = S_OK;
    DWORD dwIndex = 0;
    PrivateTempo Tempo;
    TList<PrivateTempo> TempoList;
    TListItem<PrivateTempo>* pScan = NULL;
    MUSIC_TIME mtNext = 0;
    MUSIC_TIME mtTime = mtStart;
    MUSIC_TIME mtCursor = mtStart;
    REFERENCE_TIME rtTime;
    do
    {
        hr = GetParam(GUID_PrivateTempoParam,-1,dwIndex,mtTime,&mtNext,(void *)&Tempo );
        Tempo.mtTime = mtTime;
        if (hr == S_OK && Tempo.mtDelta > 0)
        {
            mtTime += Tempo.mtDelta;
            hr = GetParam(GUID_PrivateTempoParam,-1,dwIndex,mtTime,&mtNext,(void *)&Tempo );
            Tempo.mtTime = mtTime;
        }
        if (hr == S_FALSE && fFirst && !pSegState)
        {
            // If this was the very first try, there might not be any tempo track, and
            // so global tempo is called. If so, S_FALSE is returned. This is okay
            // for the NULL segstate case where we are recomputing the tempo map in response
            // to a change in global tempo, or stop of all segments.
            if (fAllDeltas) // Never do this in response to adding a new event to the tempo map
            {
                MusicToReferenceTime(mtTime,&rtTime);
                // the rtTime in the tempo map needs to be the non-adjusted value (305694)
                AddToTempoMap( Tempo.dblTempo, mtTime, rtTime + m_rtAdjust );
            }
            break;
        }
        if (hr == S_OK)
        {
            TListItem<PrivateTempo>* pNew = new TListItem<PrivateTempo>(Tempo);
            if (pNew)
            {
                // add to TempoList, replacing duplicate times with the most recent mtDelta
                TListItem<PrivateTempo>* pNext = TempoList.GetHead();
                if (!pNext || Tempo.mtTime < pNext->GetItemValue().mtTime)
                {
                    TempoList.AddHead(pNew);
                }
                else for (pScan = TempoList.GetHead(); pScan; pScan = pNext)
                {
                    pNext = pScan->GetNext();
                    if (Tempo.mtTime == pScan->GetItemValue().mtTime)
                    {
                        if (Tempo.mtDelta > pScan->GetItemValue().mtDelta)
                        {
                            pScan->GetItemValue() = Tempo;
                        }
                        delete pNew;
                        break;
                    }
                    else if (!pNext || Tempo.mtTime < pNext->GetItemValue().mtTime)
                    {
                        pScan->SetNext(pNew);
                        pNew->SetNext(pNext);
                        break;
                    }
                }
            }
            mtTime += mtNext;
            fFirst = false;
            // If this was the last tempo in the track (that we care about),
            // reset the time and bump the track index
            if (Tempo.fLast || mtTime > (m_mtTransported + TEMPO_AHEAD))
            {
                dwIndex++;
                mtCursor = mtTime;
                mtTime = mtStart;
            }
            else if (!mtNext) break; // should never happen but if it does, infinite loop
        }
        else if (Tempo.fLast) // There was an empty tempo track
        {
            dwIndex++;
            hr = S_OK;
        }
        Tempo.fLast = false;
    } while (hr == S_OK);
    if (TempoList.GetHead() && TempoList.GetHead()->GetItemValue().mtTime > mtStart)
    {
        // add a tempo of 120 at time mtStart
        TListItem<PrivateTempo>* pNew = new TListItem<PrivateTempo>();
        if (pNew)
        {
            PrivateTempo& rNew = pNew->GetItemValue();
            rNew.dblTempo = 120.0;
            rNew.mtTime = mtStart;
            TempoList.AddHead(pNew);
        }
        else
        {
#ifdef DBG
            Trace(1, "Error: Out of memory; Tempo map is incomplete.\n");
#endif
            TempoList.GetHead()->GetItemValue().mtTime = mtStart;
        }
    }
    for (pScan = TempoList.GetHead(); pScan; pScan = pScan->GetNext())
    {
        PrivateTempo& rTempo = pScan->GetItemValue();
        if (fAllDeltas || rTempo.mtTime + rTempo.mtDelta >= mtStart)
        {
            MusicToReferenceTime(rTempo.mtTime,&rtTime);
            // the rtTime in the tempo map needs to be the non-adjusted value (305694)
            AddToTempoMap( rTempo.dblTempo, rTempo.mtTime, rtTime + m_rtAdjust );
        }
    }
    m_mtTempoCursor = mtCursor;
}

HRESULT STDMETHODCALLTYPE CPerformance::MusicToReferenceTime(
    MUSIC_TIME mtTime,          // @parm The time in MUSIC_TIME format to convert.
    REFERENCE_TIME *prtTime)    // @parm Returns the converted time in REFERENCE_TIME format.
{
    V_INAME(IDirectMusicPerformance::MusicToReferenceTime);
    V_PTR_WRITE(prtTime,REFERENCE_TIME);

    EnterCriticalSection(&m_MainCrSec);
    if( m_pClock == NULL )
    {
        LeaveCriticalSection(&m_MainCrSec);
        Trace(1,"Error: Unable to convert music to reference time because the performance has not been initialized.\n");
        return DMUS_E_NO_MASTER_CLOCK;
    }
    LeaveCriticalSection(&m_MainCrSec);

    PRIV_PMSG*  pEvent;
    double dbl = 120;
    MUSIC_TIME mtTempo = 0;
    REFERENCE_TIME rtTempo = m_rtStart;
    REFERENCE_TIME rtTemp;

    EnterCriticalSection( &m_PipelineCrSec );
    pEvent = m_TempoMap.GetHead();
    if( pEvent )
    {
        if( mtTime >= pEvent->mtTime )
        {
            while( pEvent->pNext )
            {
                if( pEvent->pNext->mtTime > mtTime )
                {
                    break;
                }
                pEvent = pEvent->pNext;
            }
            DMInternalTempo* pTempo = (DMInternalTempo*)pEvent;
            dbl = pTempo->tempoPMsg.dblTempo * pTempo->fltRelTempo;
            mtTempo = pTempo->tempoPMsg.mtTime;
            rtTempo = pTempo->tempoPMsg.rtTime;
        }
        else
        {
            // If mtTime is less than everything in the tempo map, look in the old tempo map
            // (which goes five minutes into the past).  This keeps the regular tempo map
            // small, but allows us to get a valid tempo in the cases where the regular tempo
            // map no longer contains the tempo we need.
            pEvent = m_OldTempoMap.GetHead();
            if( pEvent )
            {
                if( mtTime >= pEvent->mtTime )
                {
                    while( pEvent->pNext )
                    {
                        if( pEvent->pNext->mtTime > mtTime )
                        {
                            break;
                        }
                        pEvent = pEvent->pNext;
                    }
                    DMInternalTempo* pTempo = (DMInternalTempo*)pEvent;
                    dbl = pTempo->tempoPMsg.dblTempo * pTempo->fltRelTempo;
                    mtTempo = pTempo->tempoPMsg.mtTime;
                    rtTempo = pTempo->tempoPMsg.rtTime;
                }
            }
        }
    }
    LeaveCriticalSection( &m_PipelineCrSec );
    rtTempo -= m_rtAdjust;

    rtTemp = ( mtTime - mtTempo );
    rtTemp *= 600000000;
    rtTemp += (DMUS_PPQ / 2);
    rtTemp /= DMUS_PPQ;
    rtTemp = (REFERENCE_TIME)(rtTemp / dbl);
    *prtTime = rtTempo + rtTemp;
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CPerformance::ReferenceToMusicTime(
    REFERENCE_TIME rtTime,  // @parm The time in REFERENCE_TIME format to convert.
    MUSIC_TIME *pmtTime)    // @parm Returns the converted time in MUSIC_TIME format.
{
    V_INAME(IDirectMusicPerformance::ReferenceToMusicTime);
    V_PTR_WRITE(pmtTime,MUSIC_TIME);

    EnterCriticalSection(&m_MainCrSec);
    if( m_pClock == NULL )
    {
        LeaveCriticalSection(&m_MainCrSec);
        Trace(1,"Error: Unable to convert reference to music time because the performance has not been initialized.\n");
        return DMUS_E_NO_MASTER_CLOCK;
    }
    LeaveCriticalSection(&m_MainCrSec);

    PRIV_PMSG*  pEvent;
    double dbl = 120;
    MUSIC_TIME mtTempo = 0;
    REFERENCE_TIME rtTempo = m_rtStart;

    EnterCriticalSection( &m_PipelineCrSec );
    pEvent = m_TempoMap.GetHead();
    if( pEvent )
    {
        if( rtTime >= pEvent->rtTime )
        {
            while( pEvent->pNext )
            {
                if( pEvent->pNext->rtTime > rtTime )
                {
                    break;
                }
                pEvent = pEvent->pNext;
            }
            DMInternalTempo* pTempo = (DMInternalTempo*)pEvent;
            dbl = pTempo->tempoPMsg.dblTempo * pTempo->fltRelTempo;
            mtTempo = pTempo->tempoPMsg.mtTime;
            rtTempo = pTempo->tempoPMsg.rtTime;
        }
        else
        {
            // If mtTime is less than everything in the tempo map, look in the old tempo map
            // (which goes five minutes into the past).  This keeps the regular tempo map
            // small, but allows us to get a valid tempo in the cases where the regular tempo
            // map no longer contains the tempo we need.
            pEvent = m_OldTempoMap.GetHead();
            if( pEvent )
            {
                if( rtTime >= pEvent->rtTime )
                {
                    while( pEvent->pNext )
                    {
                        if( pEvent->pNext->rtTime > rtTime )
                        {
                            break;
                        }
                        pEvent = pEvent->pNext;
                    }
                    DMInternalTempo* pTempo = (DMInternalTempo*)pEvent;
                    dbl = pTempo->tempoPMsg.dblTempo * pTempo->fltRelTempo;
                    mtTempo = pTempo->tempoPMsg.mtTime;
                    rtTempo = pTempo->tempoPMsg.rtTime;
                }
            }
        }
    }
    LeaveCriticalSection( &m_PipelineCrSec );
    rtTempo -= m_rtAdjust;
    if( rtTime < rtTempo )
    {
        rtTime = rtTempo;
    }
    rtTime -= rtTempo;
    rtTime *= DMUS_PPQ;
    rtTime = (REFERENCE_TIME)(rtTime * dbl);
    rtTime += 300000000;
    rtTime /= 600000000;
#ifdef DBG
    if ( rtTime & 0xFFFFFFFF00000000 )
    {
        Trace(1,"Error: Invalid Reference to Music time conversion resulted in overflow.\n");
    }
#endif
    *pmtTime = (long) (rtTime & 0xFFFFFFFF);
    *pmtTime += mtTempo;
    return S_OK;
}

/*
  @method HRESULT | IDirectMusicPerformance | AdjustTime |
  Adjust the internal Performance time forward or backward. This is mostly used to
  compensate for drift when synchronizing to another source, such as SMPTE.

  @rvalue S_OK | Success.
  @rvalue E_INVALIDARG | rtAmount is too large or too small.
*/
HRESULT STDMETHODCALLTYPE CPerformance::AdjustTime(
    REFERENCE_TIME rtAmount)    // @parm The amount of time to adjust. This may be a
                                // number from -10000000 to 10000000 (-1 second to +1 second.)
{
    if( ( rtAmount < -10000000 ) || ( rtAmount > 10000000 ) )
    {
        Trace(1,"Error: Time parameter passed to AdjustTime() is out of range.\n");
        return E_INVALIDARG;
    }
    m_rtAdjust += rtAmount;
    return S_OK;
}

/*
  @method HRESULT | IDirectMusicPerformance | GetResolvedTime |
  Quantize a time to a resolution boundary. Given a time, in REFERENCE_TIME,
  return the next time on a given boundary after the time given.

  @rvalue S_OK | Success.
  @rvalue E_POINTER <prtResolved> is not valid.
*/
HRESULT STDMETHODCALLTYPE CPerformance::GetResolvedTime(
    REFERENCE_TIME rtTime,
    REFERENCE_TIME* prtResolved,
    DWORD dwResolvedTimeFlags)
{
    V_INAME(IDirectMusicPerformance::GetResolvedTime);
    V_PTR_WRITE(prtResolved,REFERENCE_TIME);

    if (rtTime == 0)
    {
        dwResolvedTimeFlags |= DMUS_TIME_RESOLVE_AFTERQUEUETIME ;
    }
    if( dwResolvedTimeFlags & DMUS_TIME_RESOLVE_AFTERPREPARETIME )
    {
        REFERENCE_TIME rtTrans;
        MusicToReferenceTime( m_mtTransported, &rtTrans );
        if( rtTime < rtTrans ) rtTime = rtTrans;
    }
    else if (dwResolvedTimeFlags & DMUS_TIME_RESOLVE_AFTERLATENCYTIME )
    {
        REFERENCE_TIME rtStart;
        rtStart = GetLatency();
        if( rtTime < rtStart ) rtTime = rtStart;
    }
    else if( dwResolvedTimeFlags & DMUS_TIME_RESOLVE_AFTERQUEUETIME )
    {
        REFERENCE_TIME rtStart;
        GetQueueTime( &rtStart ); // need queue time because control segments cause invalidations
        if( rtTime < rtStart ) rtTime = rtStart;
    }


    if( dwResolvedTimeFlags & ( DMUS_TIME_RESOLVE_BEAT | DMUS_TIME_RESOLVE_MEASURE |
        DMUS_TIME_RESOLVE_GRID | DMUS_TIME_RESOLVE_MARKER | DMUS_TIME_RESOLVE_SEGMENTEND))
    {
        MUSIC_TIME mtTime; //, mtResolved;

        ReferenceToMusicTime( rtTime, &mtTime );
        EnterCriticalSection(&m_SegmentCrSec);
        mtTime = ResolveTime( mtTime, dwResolvedTimeFlags, NULL);
        LeaveCriticalSection(&m_SegmentCrSec);
        MusicToReferenceTime( mtTime, prtResolved );
    }
    else
    {
        *prtResolved = rtTime;
    }
    return S_OK;
}


/*
  @method HRESULT | IDirectMusicPerformance | IsPlaying |
  Find out if a particular Segment or SegmentState is currently playing.

  @rvalue E_POINTER | Both pSegment and pSegState are null, or one or both are invalid.
  @rvalue DMUS_E_NO_MASTER_CLOCK | There is no master clock in the performance.
  Make sure to call <om .Init> before calling this method.
  @rvalue S_OK | Yes, it is playing.
  @rvalue S_FALSE | No, it is not playing.
*/
HRESULT STDMETHODCALLTYPE CPerformance::IsPlaying(
    IDirectMusicSegment *pSegment,          // @parm The Segment to check. If NULL, check
                                            // <p pSegState>.
    IDirectMusicSegmentState *pSegState)    // @parm The SegmentState to check. If NULL,
                                            // check <p pSegment>.
{
    CSegState* pNode;
    DWORD dwCount;

    V_INAME(IDirectMusicPerformance::IsPlaying);
    V_INTERFACE_OPT(pSegment);
    V_INTERFACE_OPT(pSegState);

    EnterCriticalSection(&m_MainCrSec);
    if( m_pClock == NULL )
    {
        LeaveCriticalSection(&m_MainCrSec);
        Trace(1,"Error: IsPlaying() failed because the performance has not been initialized.\n");
        return DMUS_E_NO_MASTER_CLOCK;
    }
    LeaveCriticalSection(&m_MainCrSec);

    if( !pSegment && !pSegState )
    {
        Trace(1,"Error: IsPlaying() failed because segment and segment state are both NULL pointers.\n");
        return E_POINTER;
    }

    MUSIC_TIME mtNow;
    GetTime(NULL, &mtNow);
    EnterCriticalSection(&m_SegmentCrSec);

    for( dwCount = 0; dwCount < SQ_COUNT; dwCount++ )
    {
        for( pNode = m_SegStateQueues[dwCount].GetHead(); pNode; pNode = pNode->GetNext() )
        {
            if( !pNode->m_fStartedPlay )
            {
                continue;
            }
            if( mtNow >= pNode->m_mtResolvedStart )
            {
                if( mtNow < pNode->m_mtLastPlayed )
                {
                    if(( pNode == (CSegState*) pSegState ) ||
                        ( pNode->m_pSegment == (CSegment *) pSegment ))
                    {
                        LeaveCriticalSection(&m_SegmentCrSec);
                        return S_OK;
                    }
                }
            }
            else
            {
                // if mtNow is before this pSegState's resolved start, it is before every
                // pSegState after this too, so break now.
                break;
            }
        }
    }
    LeaveCriticalSection(&m_SegmentCrSec);
    return S_FALSE;
}


HRESULT STDMETHODCALLTYPE CPerformance::GetTime(
        REFERENCE_TIME *prtNow, // @parm Returns the current time in REFERENCE_TIME
                                            // format. May be NULL.
        MUSIC_TIME  *pmtNow)    // @parm Returns the current time in MUSIC_TIME
                                            // format. May be NULL.
{
    V_INAME(IDirectMusicPerformance::GetTime);
    V_PTR_WRITE_OPT(prtNow,REFERENCE_TIME);
    V_PTR_WRITE_OPT(pmtNow,MUSIC_TIME);

    EnterCriticalSection(&m_MainCrSec);
    if( m_pClock == NULL )
    {
        LeaveCriticalSection(&m_MainCrSec);
        Trace(1,"Error: GetTime() failed because the performance has not been initialized.\n");
        return DMUS_E_NO_MASTER_CLOCK;
    }
    LeaveCriticalSection(&m_MainCrSec);

    REFERENCE_TIME rtTime = GetTime();
    if( prtNow )
    {
        *prtNow = rtTime;
    }
    if( pmtNow )
    {
        MUSIC_TIME mtTime;
        ReferenceToMusicTime( rtTime, &mtTime );
        *pmtNow = mtTime;
    }
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CPerformance::GetLatencyTime(
        REFERENCE_TIME *prtTime)    // @parm Returns the current latency time.
{
    V_INAME(IDirectMusicPerformance::GetLatencyTime);
    V_PTR_WRITE(prtTime,REFERENCE_TIME);

    EnterCriticalSection(&m_MainCrSec);
    if( m_pClock == NULL )
    {
        LeaveCriticalSection(&m_MainCrSec);
        Trace(1,"Error: GetLatencyTime() failed because the performance has not been initialized.\n");
        return DMUS_E_NO_MASTER_CLOCK;
    }
    LeaveCriticalSection(&m_MainCrSec);

    *prtTime = GetLatency();
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CPerformance::GetQueueTime(
        REFERENCE_TIME *prtTime)    // @parm Returns the current queue time.
{
    V_INAME(IDirectMusicPerformance::GetQueueTime);
    V_PTR_WRITE(prtTime,REFERENCE_TIME);

    EnterCriticalSection(&m_MainCrSec);
    if( m_pClock == NULL )
    {
        LeaveCriticalSection(&m_MainCrSec);
        Trace(1,"Error: GetQueueTime() failed because the performance has not been initialized.\n");
        return DMUS_E_NO_MASTER_CLOCK;
    }
    LeaveCriticalSection(&m_MainCrSec);

    DWORD dw;
    REFERENCE_TIME rtLatency;

    *prtTime = 0;
    EnterCriticalSection(&m_PChannelInfoCrSec);
    for( dw = 0; dw < m_dwNumPorts; dw++ )
    {
        if( m_pPortTable[dw].rtLast > *prtTime )
            *prtTime = m_pPortTable[dw].rtLast;
    }
    LeaveCriticalSection(&m_PChannelInfoCrSec);
    rtLatency = GetLatency();
    if( *prtTime < rtLatency )
    {
        *prtTime = rtLatency;
    }
    if (m_rtEarliestStartTime > rtLatency)
    {
        rtLatency = m_rtEarliestStartTime;
    }
    return S_OK;
}

// private version of AllocPMsg
HRESULT CPerformance::AllocPMsg(
    ULONG cb,
    PRIV_PMSG** ppPMSG)
{
    ASSERT( cb >= sizeof(PRIV_PMSG) );
    DMUS_PMSG* pDMUS_PMSG;
    HRESULT hr;

    hr = AllocPMsg( cb - PRIV_PART_SIZE, &pDMUS_PMSG );
    if( SUCCEEDED(hr) )
    {
        *ppPMSG = DMUS_TO_PRIV(pDMUS_PMSG);
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE CPerformance::ClonePMsg(DMUS_PMSG* pSourcePMSG,DMUS_PMSG** ppCopyPMSG)
{
    V_INAME(IDirectMusicPerformance::ClonePMsg);
#ifdef DBG
    V_PTRPTR_WRITE(ppCopyPMSG);
    V_BUFPTR_READ(pSourcePMSG,sizeof(DMUS_PMSG));
#else
    if (!ppCopyPMSG || !pSourcePMSG)
    {
        return E_POINTER;
    }
#endif
    HRESULT hr = AllocPMsg(pSourcePMSG->dwSize,ppCopyPMSG);
    if (SUCCEEDED(hr))
    {
        memcpy(*ppCopyPMSG,pSourcePMSG,pSourcePMSG->dwSize);
        if (pSourcePMSG->punkUser)
        {
            pSourcePMSG->punkUser->AddRef();
        }
        if (pSourcePMSG->pTool)
        {
            pSourcePMSG->pTool->AddRef();
        }
        if (pSourcePMSG->pGraph)
        {
            pSourcePMSG->pGraph->AddRef();
        }
    }
    return hr;
}


//////////////////////////////////////////////////////////////////////
// CPerformance::AllocPMsg
/*
  @method HRESULT | IDirectMusicPerformance | AllocPMsg |
  Allocate a DMUS_PMSG.

  @rvalue E_OUTOFMEMORY | Out of memory.
  @rvalue S_OK | Success.
  @rvalue E_INVALIDARG | <p cb> is smaller than sizeof(DMUS_PMSG)
  @rvalue E_POINTER | <p ppPMSG> is NULL or invalid.
*/
HRESULT STDMETHODCALLTYPE CPerformance::AllocPMsg(
    ULONG cb,               // @parm Size of the <p ppPMSG>. Must be equal to or greater
                            // than sizeof(DMUS_PMSG).
    DMUS_PMSG** ppPMSG  // @parm Returns the pointer to the allocated message, which will
                            // be of size <p cb>. All fields are initialized to zero,
                            // except dwSize which is initialized to <p cb>.
    )
{
    V_INAME(IDirectMusicPerformance::AllocPMsg);
    if( m_dwVersion < 8)
    {
        V_PTRPTR_WRITE(ppPMSG);
    }
    else
    {
#ifdef DBG
        V_PTRPTR_WRITE(ppPMSG);
#else
        if (!ppPMSG)
        {
            return E_POINTER;
        }
#endif
    }
    PRIV_PMSG* pPrivPMsg;

    if( cb < sizeof(DMUS_PMSG) )
        return E_INVALIDARG;

    EnterCriticalSection(&m_PMsgCacheCrSec);
    // cached pmsg's are stored in an array based on their public size.
    // If a cached pmsg exists, return it. Otherwise, make a new one.
    if( (cb >= PERF_PMSG_CB_MIN) && (cb < PERF_PMSG_CB_MAX) )
    {
        ULONG cbIndex = cb - PERF_PMSG_CB_MIN;
        if( m_apPMsgCache[ cbIndex ] )
        {
            pPrivPMsg = m_apPMsgCache[ cbIndex ];
            m_apPMsgCache[ cbIndex ] = pPrivPMsg->pNext;
            pPrivPMsg->pNext = NULL;
            if (pPrivPMsg->dwPrivFlags != PRIV_FLAG_FREE)
            {
                Trace(0,"Error - previously freed PMsg has been mangled.\n");
                LeaveCriticalSection(&m_PMsgCacheCrSec);
                return E_FAIL;
            }
            pPrivPMsg->dwPrivFlags = PRIV_FLAG_ALLOC;
            if (m_fInTrackPlay) pPrivPMsg->dwPrivFlags |= PRIV_FLAG_TRACK;
            *ppPMSG = PRIV_TO_DMUS(pPrivPMsg);
            LeaveCriticalSection(&m_PMsgCacheCrSec);
            return S_OK;
        }
    }

    HRESULT hr = S_OK;
    // no cached pmsg exists. Return a new one.
    ULONG cbPriv = cb + PRIV_PART_SIZE;
    pPrivPMsg = (PRIV_PMSG*)(new char[cbPriv]);
    if( pPrivPMsg )
    {
        memset( pPrivPMsg, 0, cbPriv );
        pPrivPMsg->dwSize = pPrivPMsg->dwPrivPubSize = cb; // size of public part only
        pPrivPMsg->dwPrivFlags = PRIV_FLAG_ALLOC;
        if (m_fInTrackPlay) pPrivPMsg->dwPrivFlags |= PRIV_FLAG_TRACK;
        *ppPMSG = PRIV_TO_DMUS(pPrivPMsg);
        hr = S_OK;
    }
    else
    {
        hr = E_OUTOFMEMORY;
    }
    LeaveCriticalSection(&m_PMsgCacheCrSec);
    return hr;
}

// private version of FreePMsg
HRESULT CPerformance::FreePMsg(
    PRIV_PMSG* pPMSG)
{
    return FreePMsg( PRIV_TO_DMUS(pPMSG) );
}


HRESULT STDMETHODCALLTYPE CPerformance::FreePMsg(
    DMUS_PMSG*  pPMSG   // @parm The message to free. This message must have been allocated
                            // using <om .AllocPMsg>.
    )
{
    V_INAME(IDirectMusicPerformance::FreePMsg);
    if( m_dwVersion < 8)
    {
        V_BUFPTR_WRITE(pPMSG,sizeof(DMUS_PMSG));
    }
    else
    {
#ifdef DBG
        V_BUFPTR_WRITE(pPMSG,sizeof(DMUS_PMSG));
#else
        if (!pPMSG)
        {
            return E_POINTER;
        }
#endif
    }

    PRIV_PMSG* pPrivPMsg = DMUS_TO_PRIV(pPMSG);

    if( (pPrivPMsg->dwPrivFlags & PRIV_FLAG_ALLOC_MASK) != PRIV_FLAG_ALLOC )
    {
        Trace(0, "Error --- Attempt to free a PMsg that is not allocated memory.\n");
        // this isn't a msg allocated by AllocPMsg.
        return DMUS_E_CANNOT_FREE;
    }
    if( pPrivPMsg->dwPrivFlags & PRIV_FLAG_QUEUED )
    {
        TraceI(1, "Attempt to free a PMsg that is currently in the Performance queue.\n");
        return DMUS_E_CANNOT_FREE;
    }

    EnterCriticalSection(&m_PMsgCacheCrSec);
    if( pPMSG->pTool )
    {
        pPMSG->pTool->Release();
    }
    if( pPMSG->pGraph )
    {
        pPMSG->pGraph->Release();
    }
    if( pPMSG->punkUser )
    {
        pPMSG->punkUser->Release();
    }

    ULONG cbSize = pPrivPMsg->dwPrivPubSize;
    if( (cbSize >= PERF_PMSG_CB_MIN) && (cbSize < PERF_PMSG_CB_MAX) )
    {
        memset( pPrivPMsg, 0, cbSize + PRIV_PART_SIZE );
        pPrivPMsg->dwPrivFlags = PRIV_FLAG_FREE; // Mark this as in the free queue.
        pPrivPMsg->dwSize = pPrivPMsg->dwPrivPubSize = cbSize;
        pPrivPMsg->pNext = m_apPMsgCache[ cbSize - PERF_PMSG_CB_MIN ];
        m_apPMsgCache[ cbSize - PERF_PMSG_CB_MIN ] = pPrivPMsg;
    }
    else
    {
        delete [] pPrivPMsg;
    }
    LeaveCriticalSection(&m_PMsgCacheCrSec);
    return S_OK;
}

HRESULT CPerformance::FlushVirtualTrack(
    DWORD       dwId,
    MUSIC_TIME  mtTime,
    BOOL fLeaveNotesOn)
{
    EnterCriticalSection(&m_PipelineCrSec);
    FlushMainEventQueues( dwId, mtTime, mtTime, fLeaveNotesOn );
    LeaveCriticalSection(&m_PipelineCrSec);
    return S_OK;
}

/*
  Given a time, mtTime, returns the time of the next control segment in pmtNextSeg.
  Returns S_FALSE if none found, and sets pmtNextSeg to zero.
*/

HRESULT CPerformance::GetControlSegTime(
    MUSIC_TIME mtTime,
    MUSIC_TIME* pmtNextSeg)
{
    HRESULT hr = S_FALSE;
    *pmtNextSeg = 0;
    EnterCriticalSection( &m_SegmentCrSec );
    // search the secondary lists for a control segment
    CSegState* pTemp;
    for( pTemp = m_SegStateQueues[SQ_CON_DONE].GetHead(); pTemp; pTemp = pTemp->GetNext() )
    {
        if( pTemp->m_mtResolvedStart >= mtTime )
        {
            *pmtNextSeg = pTemp->m_mtResolvedStart;
            hr = S_OK;
            break;
        }
    }
    if( S_FALSE == hr ) // if this is still zero, check the current queue
    {
        for( pTemp = m_SegStateQueues[SQ_CON_PLAY].GetHead(); pTemp; pTemp = pTemp->GetNext() )
        {
            if( pTemp->m_mtResolvedStart >= mtTime )
            {
                *pmtNextSeg = pTemp->m_mtResolvedStart;
                hr = S_OK;
                break;
            }
        }
    }
    LeaveCriticalSection( &m_SegmentCrSec );
    return hr;
}

/*
  Given a time, mtTime, returns the time of the next primary segment in pmtNextSeg.
  Returns S_FALSE if none found, and sets pmtNextSeg to zero.
*/
HRESULT CPerformance::GetPriSegTime(
    MUSIC_TIME mtTime,
    MUSIC_TIME* pmtNextSeg)
{
    HRESULT hr = S_FALSE;
    *pmtNextSeg = 0;
    EnterCriticalSection( &m_SegmentCrSec );
    CSegState* pTemp;
    for( pTemp = m_SegStateQueues[SQ_PRI_PLAY].GetHead(); pTemp; pTemp = pTemp->GetNext() )
    {
        if( pTemp->m_mtResolvedStart > mtTime )
        {
            *pmtNextSeg = pTemp->m_mtResolvedStart;
            hr = S_OK;
            break;
        }
    }
    LeaveCriticalSection( &m_SegmentCrSec );
    return hr;
}

/*
  @method HRESULT | IDirectMusicPerformance | GetGraph |
  Returns the performance's Tool Graph, AddRef'd.

  @rvalue S_OK | Success.
  @rvalue DMUS_E_NOT_FOUND | There is no graph in the performance, and therefore
  one couldn't be returned.
  @rvalue E_POINTER | <p ppGraph> is NULL or invalid.
*/
HRESULT STDMETHODCALLTYPE CPerformance::GetGraph(
         IDirectMusicGraph** ppGraph // @parm Returns the tool graph pointer.
        )
{
    V_INAME(IDirectMusicPerformance::GetGraph);
    V_PTRPTR_WRITE(ppGraph);

    HRESULT hr;
    if ((m_dwVersion >= 8) && (m_dwAudioPathMode == 0))
    {
        Trace(1,"Error: Performance not initialized.\n");
        return DMUS_E_NOT_INIT;
    }
    EnterCriticalSection(&m_MainCrSec);
    if( m_pGraph )
    {
        *ppGraph = m_pGraph;
        m_pGraph->AddRef();
        hr = S_OK;
    }
    else
    {
        Trace(1,"Error: Performance does not currently have a tool graph installed.\n");
        hr = DMUS_E_NOT_FOUND;
    }
    LeaveCriticalSection(&m_MainCrSec);
    return hr;
}


HRESULT CPerformance::GetGraphInternal(
         IDirectMusicGraph** ppGraph )
{
    EnterCriticalSection(&m_MainCrSec);
    if( !m_pGraph )
    {
        m_pGraph = new CGraph;
    }
    LeaveCriticalSection(&m_MainCrSec);
    return GetGraph(ppGraph);
}

/*
  @method HRESULT | IDirectMusicPerformance | SetGraph |
  Replaces the performance's Tool Graph. <p pGraph> is AddRef'd inside this
  method. Any messages flowing through Tools in the current Tool Graph are deleted.

  @rvalue S_OK | Success.
  @rvalue E_POINTER | <p pGraph> is invalid.
*/
HRESULT STDMETHODCALLTYPE CPerformance::SetGraph(
         IDirectMusicGraph* pGraph  // @parm The tool graph pointer. May be NULL to clear
                                    // the current graph out of the performance.
        )
{
    V_INAME(IDirectMusicPerformance::SetGraph);
    V_INTERFACE_OPT(pGraph);

    if ((m_dwVersion >= 8) && (m_dwAudioPathMode == 0))
    {
        Trace(1,"Error: Performance not initialized.\n");
        return DMUS_E_NOT_INIT;
    }

    EnterCriticalSection(&m_MainCrSec);
    if( m_pGraph )
    {
        m_pGraph->Release();
    }
    m_pGraph = pGraph;
    if( pGraph )
    {
        pGraph->AddRef();
    }
    LeaveCriticalSection(&m_MainCrSec);
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CPerformance::SetNotificationHandle(
     HANDLE hNotification,      // @parm The event handle created by CreateEvent, or
                                // 0 to clear out an existing handle.
     REFERENCE_TIME rtMinimum ) // @parm The minimum amount of time that the
                                // performance should hold notify messages before discarding them.
                                // 0 means to use the default minimum time of 20000000 reference time units,
                                // which is 2 seconds, or the previous value if this API has been called previously.
                                // If the application hasn't called <om .GetNotificationPMsg> by this time, the message is
                                // discarded to free the memory.
{
    EnterCriticalSection(&m_MainCrSec);
    m_hNotification = hNotification;
    if( rtMinimum )
    {
        m_rtNotificationDiscard = rtMinimum;
    }
    LeaveCriticalSection(&m_MainCrSec);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CPerformance::GetNotificationPMsg(
     DMUS_NOTIFICATION_PMSG** ppNotificationPMsg )

{
    V_INAME(IDirectMusicPerformance::GetNotificationPMsg);
    V_PTRPTR_WRITE(ppNotificationPMsg);

    HRESULT hr;
    EnterCriticalSection(&m_PipelineCrSec);
    if( m_NotificationQueue.GetHead() )
    {
        PRIV_PMSG* pPriv = m_NotificationQueue.Dequeue();
        ASSERT(pPriv);
        *ppNotificationPMsg = (DMUS_NOTIFICATION_PMSG*)PRIV_TO_DMUS(pPriv);
        hr = S_OK;
    }
    else
    {
        *ppNotificationPMsg = NULL;
        hr = S_FALSE;
    }
    LeaveCriticalSection(&m_PipelineCrSec);
    return hr;
}

void CPerformance::AddNotificationTypeToAllSegments( REFGUID rguidNotification )
{
    CSegState* pSegSt;
    DWORD dwCount;
    // Note: might be nice to optimize this so the same segment
    // doesn't get called multiple times
    EnterCriticalSection(&m_SegmentCrSec);
    for (dwCount = 0; dwCount < SQ_COUNT; dwCount++)
    {
        for( pSegSt = m_SegStateQueues[dwCount].GetHead(); pSegSt; pSegSt = pSegSt->GetNext() )
        {
            pSegSt->m_pSegment->AddNotificationType( rguidNotification, TRUE );
        }
    }
    LeaveCriticalSection(&m_SegmentCrSec);
}

void CPerformance::RemoveNotificationTypeFromAllSegments( REFGUID rguidNotification )
{
    CSegState* pSegSt;
    DWORD dwCount;
    // Note: might be nice to optimize this so the same segment
    // doesn't get called multiple times
    EnterCriticalSection(&m_SegmentCrSec);
    for (dwCount = 0; dwCount < SQ_COUNT; dwCount++)
    {
        for( pSegSt = m_SegStateQueues[dwCount].GetHead(); pSegSt; pSegSt = pSegSt->GetNext() )
        {
            pSegSt->m_pSegment->RemoveNotificationType( rguidNotification, TRUE );
        }
    }
    LeaveCriticalSection(&m_SegmentCrSec);
}

/*
  Check to see if this notification is already being tracked.
*/
CNotificationItem* CPerformance::FindNotification( REFGUID rguidNotification )
{
    CNotificationItem* pItem;

    pItem = m_NotificationList.GetHead();
    while(pItem)
    {
        if( rguidNotification == pItem->guidNotificationType )
        {
            break;
        }
        pItem = pItem->GetNext();
    }
    return pItem;
}

/*
  @method HRESULT | IDirectMusicPerformance | AddNotificationType |
  Adds a notification type to the performance. Notifications are identified
  by a guid. When a notification is added to the performance, notify messages
  are sent to the application, which provides a message handle on which to
  block through <om IDirectMusicPerformance.SetNotificationHandle>. All segments
  and tracks are automatically updated with the new notification by calling
  their AddNotificationType methods.

  @rvalue S_OK | Success.
  @rvalue S_FALSE | The requested notification is already on the performance.
  @rvalue E_OUTOFMEMORY | Out of memory.

  @xref <om .SetNotificationHandle>, <om .GetNotificationPMsg>, <om .RemoveNotificationType>
*/
HRESULT STDMETHODCALLTYPE CPerformance::AddNotificationType(
     REFGUID rguidNotification) // @parm The guid of the notification message to add.
{
    V_INAME(IDirectMusicPerformance::AddNotificationType);
    V_REFGUID(rguidNotification);

    CNotificationItem*  pItem;
    HRESULT hr = S_OK;

    EnterCriticalSection(&m_SegmentCrSec);
    if( NULL == FindNotification( rguidNotification ) )
    {
        pItem = new CNotificationItem;
        if( NULL == pItem )
        {
            hr = E_OUTOFMEMORY;
        }
        else
        {
            pItem->guidNotificationType = rguidNotification;
            m_NotificationList.Cat( pItem );
            AddNotificationTypeToAllSegments( rguidNotification );
        }
    }
    else
    {
        hr = S_FALSE;
    }
    LeaveCriticalSection(&m_SegmentCrSec);
    return hr;
}

/*
  @method HRESULT | IDirectMusicPerformance | RemoveNotificationType |
  Removes a previously added notification type from the performance. All
  segments and tracks are updated with the removed notification by calling
  their RemoveNotificationType methods.

  @rvalue S_OK | Success.
  @rvalue S_FALSE | The requested notification isn't currently active.

  @xref <om .SetNotificationHandle>, <om .GetNotificationPMsg>, <om .AddNotificationType>
*/
HRESULT STDMETHODCALLTYPE CPerformance::RemoveNotificationType(
     REFGUID rguidNotification) // @parm The guid of the notification message to remove.
                        // If GUID_NULL, remove all notifications.
{
    V_INAME(IDirectMusicPerformance::RemoveNotificationType);
    V_REFGUID(rguidNotification);

    HRESULT hr = S_OK;
    CNotificationItem* pItem;

    if( GUID_NULL == rguidNotification )
    {
        while (pItem = m_NotificationList.RemoveHead())
        {
            RemoveNotificationTypeFromAllSegments( pItem->guidNotificationType );
            delete pItem;
        }
    }
    else
    {
        if( pItem = FindNotification( rguidNotification ))
        {
            RemoveNotificationTypeFromAllSegments( pItem->guidNotificationType );
            m_NotificationList.Remove( pItem );
            delete pItem;
        }
        else
        {
            Trace(2,"Warning: Unable to remove requested notification because it is not currently installed.\n");
            hr = S_FALSE;
        }
    }
    return hr;
}

void CPerformance::RemoveUnusedPorts()


{
    DWORD dwIndex;
    EnterCriticalSection(&m_PChannelInfoCrSec);
    for( dwIndex = 0; dwIndex < m_dwNumPorts; dwIndex++ )
    {
        if( m_pPortTable[dwIndex].pPort && !m_AudioPathList.UsesPort(m_pPortTable[dwIndex].pPort))
        {
            // release the port and buffer. NULL them in the table. PChannels
            // that map will return an error code.
            ASSERT( m_pPortTable[dwIndex].pBuffer );
            m_pPortTable[dwIndex].pPort->Release();
            m_pPortTable[dwIndex].pBuffer->Release();
            if( m_pPortTable[dwIndex].pLatencyClock )
            {
                m_pPortTable[dwIndex].pLatencyClock->Release();
            }
            memset( &m_pPortTable[dwIndex], 0, sizeof( PortTable ));
            CChannelBlock *pBlock = m_ChannelBlockList.GetHead();
            CChannelBlock *pNext;
            for(;pBlock;pBlock = pNext)
            {
                pNext = pBlock->GetNext();
                if (pBlock->m_dwPortIndex == dwIndex)
                {
                    m_ChannelBlockList.Remove(pBlock);
                    delete pBlock;
                }
            }
        }
    }
    LeaveCriticalSection(&m_PChannelInfoCrSec);
}

HRESULT CPerformance::GetPathPort(CPortConfig *pConfig)

{
    HRESULT hr = S_OK;
    DWORD dwPort;
    EnterCriticalSection(&m_PChannelInfoCrSec);
    GUID &guidScan = pConfig->m_PortHeader.guidPort;
    // If we are looking for the default synth, get the class id for the default synth.
    BOOL fDefault = (pConfig->m_PortHeader.guidPort == GUID_Synth_Default);
    if (fDefault)
    {
        guidScan = m_AudioParams.clsidDefaultSynth;
    }
    for (dwPort = 0;dwPort < m_dwNumPorts;dwPort++)
    {
        if ((m_pPortTable[dwPort].guidPortID == guidScan) && m_pPortTable[dwPort].pPort)
        {
            pConfig->m_dwPortID = dwPort;
            pConfig->m_pPort = m_pPortTable[dwPort].pPort;
            pConfig->m_PortParams = m_pPortTable[dwPort].PortParams;
            ASSERT(pConfig->m_pPort);
            pConfig->m_pPort->AddRef();
            break;
        }
    }
    LeaveCriticalSection(&m_PChannelInfoCrSec);
    // Failed finding the port, so create it.
    if (dwPort >= m_dwNumPorts)
    {
        BOOL fUseBuffers = FALSE;
        pConfig->m_PortParams.dwSampleRate = m_AudioParams.dwSampleRate;
        if (m_AudioParams.dwFeatures & DMUS_AUDIOF_STREAMING)
        {
            pConfig->m_PortParams.dwFeatures |= DMUS_PORT_FEATURE_STREAMING;
        }
        if (m_AudioParams.dwFeatures & DMUS_AUDIOF_BUFFERS)
        {
            fUseBuffers = TRUE;
            pConfig->m_PortParams.dwFeatures |= DMUS_PORT_FEATURE_AUDIOPATH;
        }
        pConfig->m_PortParams.dwValidParams |= DMUS_PORTPARAMS_SAMPLERATE  | DMUS_PORTPARAMS_FEATURES;
        // If this wants a default synth, consult m_AudioParams and create that synth.
        if (fDefault)
        {
            pConfig->m_PortParams.dwAudioChannels = 1;
            pConfig->m_PortParams.dwVoices = m_AudioParams.dwVoices;
            pConfig->m_PortParams.dwValidParams |= DMUS_PORTPARAMS_AUDIOCHANNELS | DMUS_PORTPARAMS_VOICES;
        }
        hr = m_pDirectMusic->CreatePort(guidScan,&pConfig->m_PortParams,&pConfig->m_pPort, NULL);
        if (SUCCEEDED(hr))
        {
            if ((pConfig->m_PortParams.dwValidParams & DMUS_PORTPARAMS_FEATURES) && (pConfig->m_PortParams.dwFeatures & DMUS_PORT_FEATURE_AUDIOPATH))
            {
                IDirectMusicPortP* pPortP = NULL;
                // QI for the private interface.
                if (SUCCEEDED(pConfig->m_pPort->QueryInterface(IID_IDirectMusicPortP,(void **) &pPortP)))
                {
                    // Connect the port to the sink.
                    hr = pPortP->SetSink(m_BufferManager.m_pSinkConnect);
                    pPortP->Release();
                }
                else
                {
                    Trace(1,"Error: Attempt to create a port with audiopath buffer support failed because synth does not support buffers.\n");
                    hr = E_INVALIDARG;
                }
            }
            else if (fUseBuffers && fDefault)
            {
                Trace(1,"Error: Attempt to create a port with audiopath buffer support failed because default synth does not support buffers.\n");
                hr = E_INVALIDARG;
            }
        }
        if (SUCCEEDED(hr))
        {
            // Now add the port to the performance.
            hr = AddPort(pConfig->m_pPort,&pConfig->m_PortHeader.guidPort,
                &pConfig->m_PortParams,&pConfig->m_dwPortID);
        }
        if (SUCCEEDED(hr))
        {
            // Activate the port.
            hr = pConfig->m_pPort->Activate(TRUE);
            // It's okay if the synth is already active.
            if (hr == DMUS_E_SYNTHACTIVE)
            {
                hr = S_OK;
            }
        }
        if (SUCCEEDED(hr))
        {
            DWORD dwPortID = GetPortID(pConfig->m_pPort);
            // Then create matching channel blocks for all of the channel groups in the port.
            for (DWORD dwGroup = 0;dwGroup < pConfig->m_PortParams.dwChannelGroups; dwGroup++)
            {
                AllocVChannelBlock(dwPortID,dwGroup+1);
            }
        }
    }
    return (hr);
}

HRESULT STDMETHODCALLTYPE CPerformance::AddPort(
            IDirectMusicPort* pPort)
{
    V_INAME(IDirectMusicPerformance::AddPort);
    V_INTERFACE_OPT(pPort);
    if (m_dwAudioPathMode == 2)
    {
        Trace(1,"Error: Can not call AddPort() when using AudioPaths.\n");
        return DMUS_E_AUDIOPATHS_IN_USE;
    }
    m_dwAudioPathMode = 1;
    return AddPort(pPort,NULL,NULL,NULL);
}

HRESULT CPerformance::AddPort(
            IDirectMusicPort* pPort,
            GUID *pguidPortID,
            DMUS_PORTPARAMS8 *pParams,
            DWORD *pdwPortID)
{
    PortTable* pPortTable;
    IDirectMusicBuffer* pBuffer;
    BOOL    fSetUpBlock = FALSE;
    BOOL    fBuiltNewTable = FALSE;
    HRESULT hr = S_OK;
    GUID guidPortID;             // Class ID of port.
    DWORD dwChannelGroups;       // Number of channel groups at initialization.
    DWORD dwNewPortIndex = 0;    // Index into port array for new port.

    EnterCriticalSection(&m_MainCrSec);
    EnterCriticalSection(&m_PChannelInfoCrSec);

    if( NULL == m_pDirectMusic )
    {
        Trace(1,"Error: Performance is not initialized, ports can not be added.\n");
        hr = DMUS_E_NOT_INIT;
        goto END;
    }

    for (;dwNewPortIndex < m_dwNumPorts; dwNewPortIndex++)
    {
        if (!m_pPortTable[dwNewPortIndex].pPort)
        {
            break;
        }
    }

    if (dwNewPortIndex == m_dwNumPorts)
    {
        pPortTable = new PortTable[m_dwNumPorts + 1];
        if( !pPortTable )
        {
            hr = E_OUTOFMEMORY;
            goto END;
        }
        fBuiltNewTable = TRUE;
    }

    // if pPort is NULL, create a software synth port
    DMUS_PORTPARAMS dmpp;
    if( NULL == pPort )
    {
        pParams = &dmpp;
        memset(&dmpp, 0, sizeof(DMUS_PORTPARAMS) );
        dmpp.dwSize = sizeof(DMUS_PORTPARAMS);
        dmpp.dwChannelGroups = dwChannelGroups = 1;
        dmpp.dwValidParams = DMUS_PORTPARAMS_CHANNELGROUPS |
            DMUS_PORTPARAMS_AUDIOCHANNELS;
        dmpp.dwAudioChannels = 2;
        guidPortID = GUID_NULL;
        hr = m_pDirectMusic->CreatePort(GUID_NULL, &dmpp, &pPort, NULL);

        if ( SUCCEEDED( hr ) )
        {
            hr = pPort->Activate(TRUE);
        }


        fSetUpBlock = TRUE;
    }
    else
    {
        if (pguidPortID)
        {
            guidPortID = *pguidPortID;
        }
        else
        {
            DMUS_PORTCAPS PortCaps;
            PortCaps.dwSize = sizeof (PortCaps);
            pPort->GetCaps(&PortCaps);
            guidPortID = PortCaps.guidPort;
        }
        pPort->GetNumChannelGroups(&dwChannelGroups);
        pPort->AddRef();
    }
    if( FAILED(hr) || ( pPort == NULL ) )
    {
        if (fBuiltNewTable) delete [] pPortTable;
        Trace(1,"Error: Unable to open requested port.\n");
        hr = DMUS_E_CANNOT_OPEN_PORT;
        goto END;
    }

    // Create a buffer
    DMUS_BUFFERDESC dmbd;
    memset( &dmbd, 0, sizeof(DMUS_BUFFERDESC) );
    dmbd.dwSize = sizeof(DMUS_BUFFERDESC);
    dmbd.cbBuffer = DEFAULT_BUFFER_SIZE;
    if( FAILED( m_pDirectMusic->CreateMusicBuffer(&dmbd, &pBuffer, NULL)))
    {
        if (fBuiltNewTable) delete [] pPortTable;
        pPort->Release();
        Trace(1,"Error: Unable to create MIDI buffer for port.\n");
        hr = DMUS_E_CANNOT_OPEN_PORT;
        goto END;
    }

    if (fBuiltNewTable)
    {
        // if there is an existing port table, copy its contents to the new, bigger, port table
        if( m_pPortTable )
        {
            if( m_dwNumPorts > 0 )
            {
                memcpy( pPortTable, m_pPortTable, sizeof(PortTable) * ( m_dwNumPorts ) );
            }
            delete [] m_pPortTable;
        }
        m_pPortTable = pPortTable;
    }
    if (pdwPortID)
    {
        *pdwPortID = dwNewPortIndex;
    }
    pPortTable = &m_pPortTable[dwNewPortIndex];
    pPortTable->pPort = pPort;
    // If we have a passed params structure, copy it. This will be used for identifying the
    // params as initialized by the synth.
    if (pParams)
    {
        pPortTable->PortParams = *pParams;
    }
    pPortTable->dwGMFlags = 0;
    //set master volume
    IKsControl *pControl;
    if(SUCCEEDED(pPort->QueryInterface(IID_IKsControl, (void**)&pControl)))
    {
        KSPROPERTY ksp;
        ULONG cb;

        memset(&ksp, 0, sizeof(ksp));
        ksp.Set   = GUID_DMUS_PROP_Volume;
        ksp.Id    = 0;
        ksp.Flags = KSPROPERTY_TYPE_SET;

        pControl->KsProperty(&ksp,
                            sizeof(ksp),
                            (LPVOID)&m_lMasterVolume,
                            sizeof(m_lMasterVolume),
                            &cb);
        // Now, find out if it has a gm, gs, or xg sets in rom...
        BOOL bIsSupported = FALSE;
        ksp.Set     = GUID_DMUS_PROP_GM_Hardware;
        ksp.Flags   = KSPROPERTY_TYPE_GET;

        hr = pControl->KsProperty(&ksp,
                            sizeof(ksp),
                            (LPVOID)&bIsSupported,
                            sizeof(bIsSupported),
                            &cb);
        if (SUCCEEDED(hr) && (bIsSupported))
        {
            pPortTable->dwGMFlags |= DM_PORTFLAGS_GM;
        }
        bIsSupported = FALSE;
        ksp.Set     = GUID_DMUS_PROP_GS_Hardware;
        ksp.Flags   = KSPROPERTY_TYPE_GET;

        hr = pControl->KsProperty(&ksp,
                            sizeof(ksp),
                            (LPVOID)&bIsSupported,
                            sizeof(bIsSupported),
                            &cb);
        if (SUCCEEDED(hr) && (bIsSupported))
        {
            pPortTable->dwGMFlags |= DM_PORTFLAGS_GS;
        }
        bIsSupported = FALSE;
        ksp.Set     = GUID_DMUS_PROP_XG_Hardware;
        ksp.Flags   = KSPROPERTY_TYPE_GET;

        hr = pControl->KsProperty(&ksp,
                            sizeof(ksp),
                            (LPVOID)&bIsSupported,
                            sizeof(bIsSupported),
                            &cb);
        if (SUCCEEDED(hr) && (bIsSupported))
        {
            pPortTable->dwGMFlags |= DM_PORTFLAGS_XG;
        }
        pControl->Release();
    }

    if( FAILED( pPort->GetLatencyClock( &pPortTable->pLatencyClock )))
    {
        pPortTable->pLatencyClock = NULL;
    }
    pPortTable->dwChannelGroups = dwChannelGroups;
    pPortTable->guidPortID = guidPortID;
    pPortTable->pBuffer = pBuffer;
    pPortTable->fBufferFilled = FALSE;
    pPortTable->rtLast = 0;
    if (fBuiltNewTable) m_dwNumPorts++; // must do this before calling AssignPChannelBlock
    if( fSetUpBlock && m_ChannelBlockList.IsEmpty() ) // set up default PChannel map if none already set
    {
        AssignPChannelBlock( 0, pPort, 1);
    }
    hr = S_OK;
END:
    LeaveCriticalSection(&m_PChannelInfoCrSec);
    LeaveCriticalSection(&m_MainCrSec);

    return hr;
}

HRESULT STDMETHODCALLTYPE CPerformance::RemovePort(
            IDirectMusicPort* pPort     // @parm The port to remove.
        )
{
    V_INAME(IDirectMusicPerformance::RemovePort);
    V_INTERFACE(pPort);

    DWORD dwIndex;
    HRESULT hr = E_INVALIDARG;

    EnterCriticalSection(&m_PChannelInfoCrSec);
    for( dwIndex = 0; dwIndex < m_dwNumPorts; dwIndex++ )
    {
        if( m_pPortTable[dwIndex].pPort == pPort )
        {
            // release the port and buffer. NULL them in the table. PChannels
            // that map will return an error code.
            ASSERT( m_pPortTable[dwIndex].pBuffer );
            m_pPortTable[dwIndex].pPort->Release();
            m_pPortTable[dwIndex].pBuffer->Release();
            if( m_pPortTable[dwIndex].pLatencyClock )
            {
                m_pPortTable[dwIndex].pLatencyClock->Release();
            }
            memset( &m_pPortTable[dwIndex], 0, sizeof( PortTable ));
            hr = S_OK;
            break;
        }
    }
#ifdef DBG
    if (hr == E_INVALIDARG)
    {
        Trace(1,"Error: Invalid port passed to RemovePort().\n");
    }
#endif
    LeaveCriticalSection(&m_PChannelInfoCrSec);
    return hr;
}

// this must be called from within a PChannelCrSec critical section.
HRESULT CPerformance::AssignPChannelBlock(
            DWORD dwBlockNum,
            DWORD dwPortIndex,
            DWORD dwGroup,
            WORD wFlags)
{
    // see if we've already allocated this block before
    // blocknum is PChannel / 16, so search on that.
    DWORD dwPChannel = dwBlockNum * 16;
    CChannelBlock* pChannelBlock = m_ChannelBlockList.GetHead();

    for( ; pChannelBlock; pChannelBlock = pChannelBlock->GetNext() )
    {
        if( pChannelBlock->m_dwPChannelStart == dwPChannel )
        {
            pChannelBlock->Init(dwPChannel,dwPortIndex,dwGroup,wFlags);
            break;
        }
    }
    if( !pChannelBlock )
    {
        pChannelBlock = new CChannelBlock;
        if( !pChannelBlock )
        {
            return E_OUTOFMEMORY;
        }
        pChannelBlock->Init(dwPChannel,dwPortIndex,dwGroup,wFlags);
        m_ChannelBlockList.AddHead(pChannelBlock);
        pChannelBlock->m_dwPChannelStart = dwPChannel;
    }
    return S_OK;
}

// this must be called from within a PChannelCrSec critical section.
HRESULT CPerformance::AssignPChannel(
            DWORD dwPChannel,
            DWORD dwPortIndex,
            DWORD dwGroup,
            DWORD dwMChannel,
            WORD wFlags)
{
    DWORD dwIndex;
    CChannelBlock* pChannelBlock = m_ChannelBlockList.GetHead();

    for( ; pChannelBlock; pChannelBlock = pChannelBlock->GetNext() )
    {
        if( pChannelBlock->m_dwPChannelStart <= dwPChannel )
        {
            if( pChannelBlock->m_dwPChannelStart + PCHANNEL_BLOCKSIZE > dwPChannel )
            {
                break;
            }
        }
    }
    if( !pChannelBlock )
    {
        // there is no currently existing block that encompases dwPChannel.
        // Create one.
        pChannelBlock = new CChannelBlock;

        if( !pChannelBlock )
        {
            return E_OUTOFMEMORY;
        }
        pChannelBlock->Init(dwPChannel,0,0,CMAP_FREE);
        m_ChannelBlockList.AddHead(pChannelBlock);
    }

    dwIndex = dwPChannel - pChannelBlock->m_dwPChannelStart;

    ASSERT( dwIndex < PCHANNEL_BLOCKSIZE );
    CChannelMap *pMap = &pChannelBlock->m_aChannelMap[dwIndex];
    pMap->dwPortIndex = dwPortIndex;
    pMap->dwGroup = dwGroup;
    pMap->dwMChannel = dwMChannel;
    pMap->nTranspose = 0;
    if ((pMap->wFlags & CMAP_FREE) && !(wFlags & CMAP_FREE))
        pChannelBlock->m_dwFreeChannels--;
    else if (!(pMap->wFlags & CMAP_FREE) && (wFlags & CMAP_FREE))
        pChannelBlock->m_dwFreeChannels++;
    pMap->wFlags = wFlags;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CPerformance::AssignPChannelBlock(
            DWORD dwBlockNum,           // @parm The block number. Should be 0 or greater.
            IDirectMusicPort* pPort,    // @parm The port.
            DWORD dwGroup               // @parm The group on the port. Should be 1 or greater.
        )
{
    V_INAME(IDirectMusicPerformance::AssignPChannelBlock);
    V_INTERFACE(pPort);


    if (m_dwAudioPathMode == 2)
    {
        Trace(1,"Error: Can not call AssignPChannelBlock() when using AudioPaths.\n");
        return DMUS_E_AUDIOPATHS_IN_USE;
    }
    m_dwAudioPathMode = 1;
    DWORD dwIndex;
    HRESULT hr = E_INVALIDARG;
    EnterCriticalSection(&m_PChannelInfoCrSec);
    for( dwIndex = 0; dwIndex < m_dwNumPorts; dwIndex++ )
    {
        if( m_pPortTable[dwIndex].pPort == pPort )
        {
            if( SUCCEEDED( hr = AssignPChannelBlock( dwBlockNum, dwIndex, dwGroup, CMAP_STATIC )))
            {
                if (m_pPortTable[dwIndex].dwChannelGroups < dwGroup)
                {
                    hr = S_FALSE;
                }
            }
            break;
        }
    }
#ifdef DBG
    if (hr == E_INVALIDARG)
    {
        Trace(1,"Error: AssignPChannelBlock() called with invalid port.\n");
    }
#endif
    LeaveCriticalSection(&m_PChannelInfoCrSec);
    return hr;
}

HRESULT STDMETHODCALLTYPE CPerformance::AssignPChannel(
            DWORD dwPChannel,           // @parm The PChannel.
            IDirectMusicPort* pPort,    // @parm The port.
            DWORD dwGroup,              // @parm The group on the port.
            DWORD dwMChannel            // @parm The channel on the group.
        )
{
    V_INAME(IDirectMusicPerformance::AssignPChannel);
    V_INTERFACE(pPort);


    if (m_dwAudioPathMode == 2)
    {
        Trace(1,"Error: Can not call AssignPChannel() when using AudioPaths.\n");
        return DMUS_E_AUDIOPATHS_IN_USE;
    }
    m_dwAudioPathMode = 1;
    DWORD dwIndex;
    HRESULT hr = E_INVALIDARG;
    if( (dwMChannel < 0) || (dwMChannel > 15))
    {
        Trace(1,"Error: AssignPChannel() called with invalid MIDI Channel %ld.\n",dwMChannel);
        return E_INVALIDARG;
    }
    EnterCriticalSection(&m_PChannelInfoCrSec);
    for( dwIndex = 0; dwIndex < m_dwNumPorts; dwIndex++ )
    {
        if( m_pPortTable[dwIndex].pPort == pPort )
        {
            if( SUCCEEDED( hr = AssignPChannel( dwPChannel, dwIndex, dwGroup, dwMChannel, CMAP_STATIC )))
            {
                if (m_pPortTable[dwIndex].dwChannelGroups < dwGroup)
                {
                    hr = S_FALSE;
                }
            }
            break;
        }
    }
    LeaveCriticalSection(&m_PChannelInfoCrSec);
    return hr;
}

/*  ReleasePChannel finds the requested PChannel and makes it available
    for reuse.
    It also calls ResetAllControllers(), which sends MIDI CC 121 and 123,
    reset all controllers and all notes off.
*/

HRESULT CPerformance::ReleasePChannel(DWORD dwPChannel)
{
    HRESULT hr = E_INVALIDARG;
    EnterCriticalSection(&m_PChannelInfoCrSec);
    CChannelBlock* pChannelBlock = m_ChannelBlockList.GetHead();
    for( ; pChannelBlock; pChannelBlock = pChannelBlock->GetNext() )
    {
        if( pChannelBlock->m_dwPChannelStart <= dwPChannel )
        {
            if( pChannelBlock->m_dwPChannelStart + PCHANNEL_BLOCKSIZE > dwPChannel )
            {
                break;
            }
        }
    }
    if( pChannelBlock )
    {
        // Only release if this is genuinely a virtual pchannel. Otherwise, leave alone.
        CChannelMap *pMap = &pChannelBlock->m_aChannelMap[dwPChannel - pChannelBlock->m_dwPChannelStart];
        if (pMap->wFlags & CMAP_VIRTUAL)
        {
            pChannelBlock->m_dwFreeChannels++;
            // Clear out all the merge lists, etc.
            pMap->Clear();
            // Reset controllers, but don't send a GM reset.
            ResetAllControllers(pMap,0, false);
        }
        hr = S_OK;
    }
    LeaveCriticalSection(&m_PChannelInfoCrSec);
    return hr;
}

HRESULT CPerformance::GetPort(DWORD dwPortID, IDirectMusicPort **ppPort)

{
    HRESULT hr;
    EnterCriticalSection(&m_PChannelInfoCrSec);
    if (dwPortID < m_dwNumPorts)
    {
        *ppPort = m_pPortTable[dwPortID].pPort;
        (*ppPort)->AddRef();
        hr = S_OK;
    }
    else
    {
        Trace(1,"Error: Unable to find requested port.\n");
        hr = E_FAIL;
    }
    LeaveCriticalSection(&m_PChannelInfoCrSec);
    return hr;
}


HRESULT CPerformance::AllocVChannel(DWORD dwPortID, DWORD dwDrumFlags, DWORD *pdwPChannel, DWORD *pdwGroup,DWORD *pdwMChannel)
{
    // dwDrumsFlags:
    // bit 0 determines whether this port separates out drums on channel 10.
    // bit 1 determines whether this request is for a drum.
    // First, figure out if we are scanning for drums on channel 10, melodic instruments
    // on the other channels, or any on all channels.
    static DWORD sdwSearchForDrums[1] = { 9 };
    static DWORD sdwSearchForAll[16] = { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 };
    static DWORD sdwSearchForMelodic[15] = { 0,1,2,3,4,5,6,7,8,10,11,12,13,14,15 };
    DWORD *pSearchArray = sdwSearchForAll;
    DWORD dwSearchSize = 16;
    if (dwDrumFlags & 1) // Do we handle drums as a special case for channel 10?
    {
        if (dwDrumFlags & 2) // And are we looking for drums on channel 10?
        {
            pSearchArray = sdwSearchForDrums;
            dwSearchSize = 1;
        }
        else
        {
            pSearchArray = sdwSearchForMelodic;
            dwSearchSize = 15;
        }
    }
    HRESULT hr = E_INVALIDARG; // Return this if the vChannel is out of range.
    EnterCriticalSection(&m_PChannelInfoCrSec);

    CChannelBlock* pChannelBlock = m_ChannelBlockList.GetHead();
    BOOL fNotFound = TRUE;              // Use to indicate when we finally find a match.
    DWORD dwHighestPChannel = 0;        // Keep track of the highest PCHannel in use, this will be
                                        // used to create a new PChannel block, if needed.
    DWORD dwChannel;
    for (;fNotFound && pChannelBlock;pChannelBlock = pChannelBlock->GetNext() )
    {
        if (dwHighestPChannel < pChannelBlock->m_dwPChannelStart)
        {
            dwHighestPChannel = pChannelBlock->m_dwPChannelStart;
        }
        if ((pChannelBlock->m_dwPortIndex == dwPortID) && (pChannelBlock->m_dwFreeChannels))
        {
            DWORD dwIndex;
            for (dwIndex = 0; dwIndex < dwSearchSize; dwIndex++)
            {
                dwChannel = pSearchArray[dwIndex];
                if (pChannelBlock->m_aChannelMap[dwChannel].wFlags & CMAP_FREE)
                {
                    *pdwPChannel = pChannelBlock->m_dwPChannelStart + dwChannel;
                    pChannelBlock->m_dwFreeChannels--;
                    pChannelBlock->m_aChannelMap[dwChannel].wFlags = CMAP_VIRTUAL;
                    *pdwGroup = pChannelBlock->m_aChannelMap[dwChannel].dwGroup;
                    *pdwMChannel = pChannelBlock->m_aChannelMap[dwChannel].dwMChannel;
                    fNotFound = FALSE;
                    hr = S_OK;
                    break;
                }
            }
        }
    }
    if( fNotFound )
    {
        // there is no currently existing block that has a free channel.
        // Create one.
        IDirectMusicPort *pPort = m_pPortTable[dwPortID].pPort;
        DWORD dwChannelGroupCount;
        pPort->GetNumChannelGroups(&dwChannelGroupCount);
        dwChannelGroupCount++;
        hr = pPort->SetNumChannelGroups(dwChannelGroupCount);
        if (SUCCEEDED(hr))
        {
            m_pPortTable[dwPortID].dwChannelGroups = dwChannelGroupCount;
            hr = E_OUTOFMEMORY;
            dwHighestPChannel += PCHANNEL_BLOCKSIZE;
            pChannelBlock = new CChannelBlock;
            if (pChannelBlock)
            {
                pChannelBlock->Init(dwHighestPChannel,dwPortID,dwChannelGroupCount,CMAP_FREE);
                m_ChannelBlockList.AddTail(pChannelBlock);
                dwChannel = pSearchArray[0];  // Which channel should we use?
                CChannelMap *pMap = &pChannelBlock->m_aChannelMap[dwChannel];
                pMap->dwMChannel = dwChannel;
                pMap->wFlags = CMAP_VIRTUAL;
                pChannelBlock->m_dwFreeChannels--;
                *pdwPChannel = dwChannel + dwHighestPChannel;
                *pdwGroup = pMap->dwGroup;
                *pdwMChannel = dwChannel;
                hr = S_OK;
            }
        }
    }
#ifdef DBG
    if (hr == E_INVALIDARG)
    {
        Trace(1,"Error: Unable to allocated dynamic PChannel.\n");
    }
#endif
    LeaveCriticalSection(&m_PChannelInfoCrSec);
    return hr;
}

HRESULT CPerformance::AllocVChannelBlock(DWORD dwPortID,DWORD dwGroup)
{
    EnterCriticalSection(&m_PChannelInfoCrSec);

    CChannelBlock* pChannelBlock = m_ChannelBlockList.GetHead();
    long lHighestPChannel = -PCHANNEL_BLOCKSIZE;
    for (;pChannelBlock;pChannelBlock = pChannelBlock->GetNext() )
    {
        if (lHighestPChannel < (long) pChannelBlock->m_dwPChannelStart)
        {
            lHighestPChannel = pChannelBlock->m_dwPChannelStart;
        }
    }
    HRESULT hr = E_OUTOFMEMORY;
    lHighestPChannel += PCHANNEL_BLOCKSIZE;
    pChannelBlock = new CChannelBlock;
    if (pChannelBlock)
    {
        pChannelBlock->Init((DWORD) lHighestPChannel,dwPortID,dwGroup,CMAP_FREE);
        m_ChannelBlockList.AddTail(pChannelBlock);
        hr = S_OK;
    }
    LeaveCriticalSection(&m_PChannelInfoCrSec);
    return hr;
}


#ifdef DBG
void CPerformance::TraceAllChannelMaps()

{
    EnterCriticalSection(&m_PChannelInfoCrSec);
    CChannelBlock* pChannelBlock = m_ChannelBlockList.GetHead();
    for( ; pChannelBlock; pChannelBlock = pChannelBlock->GetNext() )
    {
        TraceI(0,"ChannelBlock %lx, Free %ld\n",pChannelBlock->m_dwPChannelStart,pChannelBlock->m_dwFreeChannels);
        DWORD dwIndex;
        for (dwIndex = 0; dwIndex < PCHANNEL_BLOCKSIZE; dwIndex++)
        {
            CChannelMap *pMap = &pChannelBlock->m_aChannelMap[dwIndex];
            TraceI(0,"\tPort %ld, Group: %ld, MIDI: %ld, Transpose: %ld, Flags: %ld\n",
                pMap->dwPortIndex, pMap->dwGroup, pMap->dwMChannel, (long) pMap->nTranspose, (long) pMap->wFlags);
        }
    }
    LeaveCriticalSection(&m_PChannelInfoCrSec);
}

#endif


/* Note that the following must be called from within a m_PChannelInfoCrSec
   critical section and stay within that critical section for the duration
   of using the returned CChannelMap.
*/


CChannelMap * CPerformance::GetPChannelMap( DWORD dwPChannel )
{
    CChannelBlock*  pChannelBlock = m_ChannelBlockList.GetHead();

    for( ; pChannelBlock; pChannelBlock = pChannelBlock->GetNext() )
    {
        if( ( dwPChannel >= pChannelBlock->m_dwPChannelStart ) &&
            ( dwPChannel < pChannelBlock->m_dwPChannelStart + PCHANNEL_BLOCKSIZE ) )
        {
            CChannelMap* pChannelMap;

            pChannelMap = &pChannelBlock->m_aChannelMap[ dwPChannel - pChannelBlock->m_dwPChannelStart ];
            if( pChannelMap->dwGroup == 0 )
            {
                // this PChannel isn't on a valid group, therefore it hasn't
                // been set.
//              return NULL;
            }
            return pChannelMap;
        }
    }
    return NULL;
}

/*
  internal version
*/

HRESULT CPerformance::PChannelIndex( DWORD dwPChannel, DWORD* pdwIndex,
            DWORD* pdwGroup, DWORD* pdwMChannel, short* pnTranspose )
{
    if (m_dwAudioPathMode == 0)
    {
        Trace(1,"Error: Performance not initialized.\n");
        return DMUS_E_NOT_INIT;
    }
    HRESULT hr;
    EnterCriticalSection(&m_PChannelInfoCrSec);
    CChannelMap *pChannelMap = GetPChannelMap(dwPChannel);
    if (pChannelMap)
    {
        ASSERT( pdwIndex && pdwGroup && pdwMChannel );

        *pdwIndex = pChannelMap->dwPortIndex;
        *pdwGroup = pChannelMap->dwGroup;
        *pdwMChannel = pChannelMap->dwMChannel;
        if( pnTranspose )
        {
            *pnTranspose = pChannelMap->nTranspose;
        }
        hr = S_OK;
    }
    else
    {
        Trace(1,"Error: PChannel %ld has not been assigned to a port.\n",dwPChannel);
        if (m_dwVersion < 8)
        {
            hr = E_INVALIDARG;
        }
        else
        {
            hr = DMUS_E_AUDIOPATH_NOPORT;
        }
    }
    LeaveCriticalSection(&m_PChannelInfoCrSec);
    return hr;
}

DWORD CPerformance::GetPortID(IDirectMusicPort * pPort)

{
    EnterCriticalSection(&m_PChannelInfoCrSec);
    DWORD dwID = 0;
    for (;dwID < m_dwNumPorts; dwID++)
    {
        if (pPort == m_pPortTable[dwID].pPort)
        {
            break;
        }
    }
    LeaveCriticalSection(&m_PChannelInfoCrSec);
    if (dwID == m_dwNumPorts) dwID = 0;
    return dwID;
}

STDMETHODIMP CPerformance::GetPortAndFlags(DWORD dwPChannel,IDirectMusicPort **ppPort,DWORD * pdwFlags)

{

    EnterCriticalSection(&m_PChannelInfoCrSec);
    DWORD dwIndex;
    DWORD dwGroup;
    DWORD dwMChannel;
    HRESULT hr = PChannelIndex( dwPChannel, &dwIndex, &dwGroup, &dwMChannel, NULL );
    if (SUCCEEDED(hr))
    {
        *ppPort = m_pPortTable[dwIndex].pPort;
        if( *ppPort )
        {
            m_pPortTable[dwIndex].pPort->AddRef();
        }
        else
        {
            Trace(1,"Error: Performance does not have a port assigned to PChannel %ld.\n",dwPChannel);
            hr = DMUS_E_NOT_INIT;
        }
        *pdwFlags = m_pPortTable[dwIndex].dwGMFlags;
    }
    LeaveCriticalSection(&m_PChannelInfoCrSec);
    return hr;
}

STDMETHODIMP CPerformance::PChannelInfo(
            DWORD dwPChannel,           // @parm The PChannel to convert.
            IDirectMusicPort** ppPort,  // @parm Returns the port. May be NULL.
            DWORD* pdwGroup,            // @parm Returns the group on the port. May be NULL.
            DWORD* pdwMChannel          // @parm Returns the channel on the group. May be NULL.
        )
{
    V_INAME(IDirectMusicPerformance::PChannelInfo);
    V_PTRPTR_WRITE_OPT(ppPort);
    V_PTR_WRITE_OPT(pdwGroup,DWORD);
    V_PTR_WRITE_OPT(pdwMChannel,DWORD);

    DWORD dwIndex, dwGroup, dwMChannel;
    HRESULT hr;

    if (m_dwAudioPathMode == 0)
    {
        Trace(1,"Error: Performance not initialized.\n");
        return DMUS_E_NOT_INIT;
    }
    EnterCriticalSection(&m_PChannelInfoCrSec);
    if( SUCCEEDED( PChannelIndex( dwPChannel, &dwIndex, &dwGroup, &dwMChannel )))
    {
        if( ppPort )
        {
            *ppPort = m_pPortTable[dwIndex].pPort;
            if( *ppPort )
            {
                m_pPortTable[dwIndex].pPort->AddRef();
            }
        }
        if( pdwGroup )
        {
            *pdwGroup = dwGroup;
        }
        if( pdwMChannel )
        {
            *pdwMChannel = dwMChannel;
        }
        hr = S_OK;
    }
    else
    {
        // No need to print an error message because PChannelIndex() does it.
        hr = E_INVALIDARG;
    }
    LeaveCriticalSection(&m_PChannelInfoCrSec);
    return hr;
}

/*
  @method HRESULT | IDirectMusicPerformance | DownloadInstrument |
  Downloads an IDirectMusicInstrument to the IDirectMusicPort specified by
  the selected PChannel.

  @rvalue E_INVALIDARG | The PChannel isn't assigned to a Port, or the Port failed
  to download the instrument. No return parameter is valid.
  @rvalue S_OK | Success.
  @rvalue E_POINTER | One of the pointers is invalid.
*/
HRESULT STDMETHODCALLTYPE CPerformance::DownloadInstrument(
    IDirectMusicInstrument* pInst,  // @parm The instrument to download.
    DWORD dwPChannel,               // @parm The PChannel to assign the instrument.
    IDirectMusicDownloadedInstrument** ppDownInst,  // @parm Returns the downloaded instrument.
    DMUS_NOTERANGE* pNoteRanges,    // @parm A pointer to an array of DMUS_NOTERANGE structures
    DWORD dwNumNoteRanges,          // @parm Number of DMUS_NOTERANGE structures in array pointed to by pNoteRanges
    IDirectMusicPort** ppPort,      // @parm Returns the port to which the instrument was downloaded.
    DWORD* pdwGroup,                // @parm Returns the group to which the instrument was assigned.
    DWORD* pdwMChannel              // @parm Returns the MChannel to which the instrument was assigned.
        )
{
    V_INAME(IDirectMusicPerformance::DownloadInstrument);
    V_INTERFACE(pInst);
    V_PTRPTR_WRITE(ppDownInst);
    V_BUFPTR_READ_OPT(pNoteRanges, (sizeof(DMUS_NOTERANGE) * dwNumNoteRanges));
    V_PTRPTR_WRITE(ppPort);
    V_PTR_WRITE(pdwGroup,DWORD);
    V_PTR_WRITE(pdwMChannel,DWORD);


    DWORD dwIndex, dwGroup, dwMChannel;
    IDirectMusicPort* pPort = NULL;
    HRESULT hr = E_INVALIDARG;

    if (m_dwAudioPathMode == 0)
    {
        Trace(1,"Error: Performance not initialized.\n");
        return DMUS_E_NOT_INIT;
    }
    EnterCriticalSection(&m_PChannelInfoCrSec);
    if( SUCCEEDED( PChannelIndex( dwPChannel, &dwIndex, &dwGroup, &dwMChannel )))
    {
        pPort = m_pPortTable[dwIndex].pPort;
        if( pPort )
        {
            hr = pPort->DownloadInstrument( pInst, ppDownInst, pNoteRanges, dwNumNoteRanges );
            pPort->AddRef();
        }
    }
    else
    {
        Trace(1,"Error: Download attempted on unassigned PChannel %ld\n",dwPChannel);
    }
    LeaveCriticalSection(&m_PChannelInfoCrSec);
    if( SUCCEEDED(hr) )
    {
        *ppPort = pPort;
        pPort->AddRef();
        *pdwGroup = dwGroup;
        *pdwMChannel = dwMChannel;
    }
    if( pPort )
    {
        pPort->Release();
    }
    return hr;
}

/*
  @method HRESULT | IDirectMusicPerformance | Invalidate |
  Flushes all methods from <p mtTime> forward, and seeks all Segments back
  to <p mtTime>, thereby calling all Tracks to resend their data.

  @rvalue S_OK | Success.
  @rvalue DMUS_E_NO_MASTER_CLOCK | There is no master clock in the performance.
  Make sure to call <om .Init> before calling this method.

  @comm If <p mtTime> is so long ago that it is impossible to invalidate that time,
  the earliest possible time will be used.
*/
HRESULT STDMETHODCALLTYPE CPerformance::Invalidate(
    MUSIC_TIME mtTime,  // @parm The time to invalidate, adjusted by <p dwFlags>. 0 means now.
    DWORD dwFlags)      // @parm Adjusts <p mtTime> to align to measures, beats, etc. See
                        // <t DMPLAYSEGFLAGS>.
{
    EnterCriticalSection(&m_MainCrSec);
    if( m_pClock == NULL )
    {
        LeaveCriticalSection(&m_MainCrSec);
         Trace(1,"Error: Invalidate() failed because the performance has not been initialized.\n");
        return DMUS_E_NO_MASTER_CLOCK;
    }
    LeaveCriticalSection(&m_MainCrSec);

    EnterCriticalSection( &m_SegmentCrSec );
    EnterCriticalSection( &m_PipelineCrSec );

    SendBuffers();

    // make sure mtTime is greater than the current queue time
    REFERENCE_TIME rtQueue;
    MUSIC_TIME mtQueue;
    MUSIC_TIME mtBumperLength;

    GetQueueTime( &rtQueue );
    ReferenceToMusicTime( rtQueue, &mtQueue );
    ReferenceToMusicTime( m_rtBumperLength, &mtBumperLength );
    if( mtTime < mtQueue + mtBumperLength )
    {
        mtTime = mtQueue + mtBumperLength;
    }
    // resolve mtTime to the boundary of dwFlags
    mtTime = ResolveTime( mtTime, dwFlags, NULL );
    // flush messages
    FlushMainEventQueues( 0, mtTime, mtQueue, FALSE );
    // move any segments in the past list that are affected into the current list
    CSegState *pSegSt;
    CSegState *pNext;
    for (pSegSt = m_SegStateQueues[SQ_SEC_DONE].GetHead();pSegSt;pSegSt = pNext)
    {
        pNext = pSegSt->GetNext();
        if( pSegSt->m_mtLastPlayed > mtTime )
        {
            m_SegStateQueues[SQ_SEC_DONE].Remove(pSegSt);
            m_SegStateQueues[SQ_SEC_PLAY].Insert( pSegSt );
        }
    }
    for (pSegSt = m_SegStateQueues[SQ_CON_DONE].GetHead();pSegSt;pSegSt = pNext)
    {
        pNext = pSegSt->GetNext();
        if( pSegSt->m_mtLastPlayed > mtTime )
        {
            m_SegStateQueues[SQ_CON_DONE].Remove(pSegSt);
            m_SegStateQueues[SQ_CON_PLAY].Insert( pSegSt );
        }
    }
    pSegSt = m_SegStateQueues[SQ_PRI_DONE].GetTail();
    if(pSegSt)
    {
        // only check the last one in this list
        if( pSegSt->m_mtLastPlayed > mtTime )
        {
            m_SegStateQueues[SQ_PRI_DONE].Remove(pSegSt);
            m_SegStateQueues[SQ_PRI_PLAY].Insert( pSegSt );
        }
    }
    // seek back any affected segmentstates that were playing
    DWORD dwCount;
    for( dwCount = SQ_PRI_PLAY; dwCount <= SQ_SEC_PLAY; dwCount++ )
    {
        for( pSegSt = m_SegStateQueues[dwCount].GetHead(); pSegSt; pSegSt = pSegSt->GetNext() )
        {
            if( pSegSt->m_fStartedPlay )
            {
                if (SQ_PRI_PLAY == dwCount && pSegSt->m_mtResolvedStart >= mtTime)
                {
                    // resend the segment start notification
                    pSegSt->GenerateNotification( DMUS_NOTIFICATION_SEGSTART, pSegSt->m_mtResolvedStart );
                    // if this is a primary or controlling segment, resend a DMUS_PMSGT_DIRTY message
                    if( !(pSegSt->m_dwPlaySegFlags & DMUS_SEGF_SECONDARY) || (pSegSt->m_dwPlaySegFlags & DMUS_SEGF_CONTROL) )
                    {
                        TraceI(4, "ReSend Dirty PMsg [3] %d (%d)\n", pSegSt->m_mtSeek, pSegSt->m_mtOffset + pSegSt->m_mtSeek);
                        pSegSt->SendDirtyPMsg( pSegSt->m_mtOffset + pSegSt->m_mtSeek );
                    }
                }
                if( pSegSt->m_mtLastPlayed > mtTime )
                {
                    // if mtTime is after the actual start time of the segment,
                    // set it so the segment has never been played before and
                    // seek the segment to the beginning
                    if( pSegSt->m_mtResolvedStart > mtTime )
                    {
                        pSegSt->m_mtLastPlayed = pSegSt->m_mtResolvedStart;
                        pSegSt->m_fStartedPlay = FALSE;
                    }
                    else
                    {
                        pSegSt->m_mtLastPlayed = mtTime;
                    }
                    pSegSt->SetInvalidate( pSegSt->m_mtLastPlayed );
                }
            }
        }
    }

    LeaveCriticalSection( &m_PipelineCrSec );
    LeaveCriticalSection( &m_SegmentCrSec );
    // signal the transport thread so we don't have to wait for it to wake up on its own
    if( m_hTransport ) SetEvent( m_hTransport );
    return S_OK;
}

STDMETHODIMP CPerformance::SetParamHook(IDirectMusicParamHook *pIHook)

{   V_INAME(IDirectMusicPerformance::SetParamHook);
    V_INTERFACE_OPT(pIHook);

    EnterCriticalSection(&m_MainCrSec);
    if (m_pParamHook)
    {
        m_pParamHook->Release();
    }
    m_pParamHook = pIHook;
    if (pIHook)
    {
        pIHook->AddRef();
    }
    LeaveCriticalSection(&m_MainCrSec);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CPerformance::GetParamEx(
    REFGUID rguidType,
    DWORD dwTrackID,
    DWORD dwGroupBits,
    DWORD dwIndex,
    MUSIC_TIME mtTime,
    MUSIC_TIME* pmtNext,
    void* pData)

{
    V_INAME(IDirectMusicPerformance::GetParamEx);
    V_PTR_WRITE_OPT(pmtNext,MUSIC_TIME);
    V_PTR_WRITE_OPT(pData,1);
    V_REFGUID(rguidType);

    static DWORD dwSearchOrder[SQ_COUNT] = { SQ_PRI_PLAY, SQ_SEC_PLAY,
                                      SQ_PRI_DONE, SQ_SEC_DONE,
                                      SQ_PRI_WAIT, SQ_SEC_WAIT,
                                      SQ_CON_PLAY, SQ_CON_DONE,
                                      SQ_CON_WAIT };

    DWORD dwIX;
    HRESULT hr;
    CSegState *pSegNode;
    if (dwTrackID)
    {
        EnterCriticalSection(&m_SegmentCrSec);
        for (dwIX = 0; dwIX < SQ_COUNT; dwIX++)
        {
            pSegNode = m_SegStateQueues[dwSearchOrder[dwIX]].GetHead();
            for (;pSegNode;pSegNode = pSegNode->GetNext())
            {
                if ((pSegNode->m_dwFirstTrackID <= dwTrackID) &&
                    (pSegNode->m_dwLastTrackID >= dwTrackID))
                {
                    CTrack* pCTrack;
                    for (pCTrack = pSegNode->m_TrackList.GetHead();pCTrack;pCTrack = pCTrack->GetNext())
                    {
                        if (pCTrack->m_dwVirtualID == dwTrackID)
                        {
                            m_dwGetParamFlags = pCTrack->m_dwFlags;
                            m_pGetParamSegmentState = pSegNode;
                            break;
                        }
                    }
                    break;
                }
            }
        }
        LeaveCriticalSection(&m_SegmentCrSec);
    }
    else
    {
        m_pGetParamSegmentState = NULL;
        m_dwGetParamFlags = 0;
    }
    hr = GetParam(rguidType,dwGroupBits,dwIndex,mtTime,pmtNext,pData);
    m_pGetParamSegmentState = NULL;
    m_dwGetParamFlags = 0;
    return hr;
}

HRESULT STDMETHODCALLTYPE CPerformance::GetParam(
    REFGUID rguidType,
    DWORD dwGroupBits,
    DWORD dwIndex,
    MUSIC_TIME mtTime,
    MUSIC_TIME* pmtNext,
    void* pData)

{
    V_INAME(IDirectMusicPerformance::GetParam);
    V_PTR_WRITE_OPT(pmtNext,MUSIC_TIME);
    V_PTR_WRITE_OPT(pData,1);
    V_REFGUID(rguidType);

    EnterCriticalSection(&m_MainCrSec);
    if( m_pClock == NULL )
    {
        LeaveCriticalSection(&m_MainCrSec);
        Trace(1,"Error: GetParam() failed because the performance has not been initialized.\n");
        return DMUS_E_NO_MASTER_CLOCK;
    }
    LeaveCriticalSection(&m_MainCrSec);

    if( pmtNext )
    {
        *pmtNext = 0; // this will be replaced by calls to IDMSegment::GetParam
    }
    CSegState* pSegNode;
    CSegState* pSegSource = (CSegState *) m_pGetParamSegmentState;
    DWORD dwOverrideFlags;
    HRESULT hr = DMUS_E_NOT_FOUND;
    BOOL fCheckedPast = FALSE;
    MUSIC_TIME mtOffset;
    DWORD dwRepeat = 0;
    MUSIC_TIME mtSegTime = 0;
    MUSIC_TIME mtSegEnd = 0;
    MUSIC_TIME mtLoopEnd = 0;
    DWORD dwRepeatsLeft = 0;
    if (pSegSource)
    {
        dwOverrideFlags = m_dwGetParamFlags & (DMUS_TRACKCONFIG_OVERRIDE_ALL | DMUS_TRACKCONFIG_OVERRIDE_PRIMARY | DMUS_TRACKCONFIG_FALLBACK);
    }
    else
    {
        dwOverrideFlags = 0;
    }

    if (dwOverrideFlags & DMUS_TRACKCONFIG_OVERRIDE_ALL)
    {
        // The calling track wants the controlling param to come from the segment itself
        mtSegTime = mtTime;
        if( S_OK == pSegSource->ConvertToSegTime( &mtSegTime, &mtOffset, &dwRepeat ) )
        {
            hr = pSegSource->GetParam( this, rguidType, dwGroupBits, dwIndex,
                    mtSegTime, pmtNext, pData );
            if( SUCCEEDED(hr) )
            {
                dwRepeatsLeft = pSegSource->m_dwRepeats;
                mtLoopEnd = pSegSource->m_mtLoopEnd;
                mtSegEnd = pSegSource->m_mtLength;
                dwRepeatsLeft -= dwRepeat;
            }
        }
    }
    if (FAILED(hr))
    {
        EnterCriticalSection(&m_SegmentCrSec);
        // we only care about control segments
        if( m_SegStateQueues[SQ_CON_DONE].GetHead() )
        {
            pSegNode = m_SegStateQueues[SQ_CON_DONE].GetHead();
        }
        else
        {
            pSegNode = m_SegStateQueues[SQ_CON_PLAY].GetHead();
            fCheckedPast = TRUE;
        }
        while( pSegNode )
        {
            mtSegTime = mtTime;
            if( S_OK == pSegNode->ConvertToSegTime( &mtSegTime, &mtOffset, &dwRepeat ) )
            {
                hr = pSegNode->GetParam( this, rguidType, dwGroupBits, dwIndex,
                        mtSegTime, pmtNext, pData );
                if( SUCCEEDED(hr) )
                {
                    dwRepeatsLeft = pSegNode->m_dwRepeats;
                    mtLoopEnd = pSegNode->m_mtLoopEnd;
                    mtSegEnd = pSegNode->m_mtLength;
                    dwRepeatsLeft -= dwRepeat;

                    break; // got the param we want. We're outta this loop with a success.
                }
            }
            // we didn't find the param, so try the next segment.
            pSegNode = pSegNode->GetNext();

            // if we're the last segnode in the done queue, we need to
            // check against the time of the first segnode in the control play queue
            if (!pSegNode && !fCheckedPast )
            {
                pSegNode = m_SegStateQueues[SQ_CON_PLAY].GetHead();
                fCheckedPast = TRUE;
            }
        }
        LeaveCriticalSection(&m_SegmentCrSec);
    }

    if( FAILED(hr) && (dwOverrideFlags & DMUS_TRACKCONFIG_OVERRIDE_PRIMARY))
    {
        // The calling track wants the controlling param to come from the segment
        // itself if there was no controlling segment.
        mtSegTime = mtTime;
        if( S_OK == pSegSource->ConvertToSegTime( &mtSegTime, &mtOffset, &dwRepeat ) )
        {
            hr = pSegSource->GetParam( this, rguidType, dwGroupBits, dwIndex,
                    mtSegTime, pmtNext, pData );
            if( SUCCEEDED(hr) )
            {
                dwRepeatsLeft = pSegSource->m_dwRepeats;
                mtLoopEnd = pSegSource->m_mtLoopEnd;
                mtSegEnd = pSegSource->m_mtLength;
                dwRepeatsLeft -= dwRepeat;
            }
        }
    }

    if( FAILED(hr) ) // didn't find one in the previous, so check for a primary segment
    {
        IDirectMusicSegment* pSegment = NULL;
        mtSegTime = mtTime;
        EnterCriticalSection(&m_SegmentCrSec);
        pSegNode = GetPrimarySegmentAtTime( mtTime );
        if( pSegNode )
        {
            pSegment = pSegNode->m_pSegment;
            pSegment->AddRef();
            pSegNode->ConvertToSegTime( &mtSegTime, &mtOffset, &dwRepeat );
            dwRepeatsLeft = pSegNode->m_dwRepeats;
            mtLoopEnd = pSegNode->m_mtLoopEnd;
            mtSegEnd = pSegNode->m_mtLength;
            dwRepeatsLeft -= dwRepeat;
        }
        else
        {
            Trace(4, "Couldn't find SegState in GetParam call.\n");
        }
        LeaveCriticalSection(&m_SegmentCrSec);
        if( pSegment )
        {
            hr = pSegNode->GetParam( this, rguidType, dwGroupBits, dwIndex,
                    mtSegTime, pmtNext, pData );
            pSegment->Release();
        }
    }

    if( FAILED(hr) && (dwOverrideFlags & DMUS_TRACKCONFIG_FALLBACK))
    {
        // The calling track wants the controlling param to come from the segment itself
        mtSegTime = mtTime;
        if( S_OK == pSegSource->ConvertToSegTime( &mtSegTime, &mtOffset, &dwRepeat ) )
        {
            hr = pSegSource->GetParam( this, rguidType, dwGroupBits, dwIndex,
                    mtSegTime, pmtNext, pData );
            if( SUCCEEDED(hr) )
            {
                dwRepeatsLeft = pSegSource->m_dwRepeats;
                mtLoopEnd = pSegSource->m_mtLoopEnd;
                mtSegEnd = pSegSource->m_mtLength;
                dwRepeatsLeft -= dwRepeat;
            }
        }
    }

    if( FAILED(hr) )
    {   // If we failed, fill in the end time of loop or segment anyway.
        if (pmtNext)
        {   // Check to see if the loop end is earlier than end of segment.
            if (dwRepeatsLeft && (mtLoopEnd > mtSegTime))
            {
                *pmtNext = mtLoopEnd - mtSegTime;
            }
            else // Or, mark end of segment.
            {
                *pmtNext = mtSegEnd - mtSegTime;
            }
        }
        // if we're looking for timesig, and didn't find it anywhere,
        // return the Performance timesig
        if( rguidType == GUID_TimeSignature )
        {
            if( NULL == pData )
            {
                Trace(1,"Error: Null pointer for time signature passed to GetParam().\n");
                hr = E_POINTER;
            }
            else
            {
                DMUS_TIMESIGNATURE* pTSigData = (DMUS_TIMESIGNATURE*)pData;
                DMUS_TIMESIG_PMSG timeSig;

                GetTimeSig( mtTime, &timeSig );
                pTSigData->bBeatsPerMeasure = timeSig.bBeatsPerMeasure;
                pTSigData->bBeat = timeSig.bBeat;
                pTSigData->wGridsPerBeat = timeSig.wGridsPerBeat;
                pTSigData->mtTime = timeSig.mtTime - mtTime;
                hr = S_OK;
            }
        }
        // Likewise, if there was no tempo in a segment, we need to read directly from the tempo list.
        else if  ( rguidType == GUID_TempoParam || rguidType == GUID_PrivateTempoParam)
        {
            if( NULL == pData )
            {
                Trace(1,"Error: Null pointer for tempo passed to GetParam().\n");
                hr = E_POINTER;
            }
            else
            {
                DMInternalTempo* pInternalTempo;
                EnterCriticalSection( &m_PipelineCrSec );
                pInternalTempo = (DMInternalTempo*)m_TempoMap.GetHead();
                DMInternalTempo* pNextTempo = NULL;
                for ( ;pInternalTempo;pInternalTempo = pNextTempo )
                {
                    pNextTempo = (DMInternalTempo *) pInternalTempo->pNext;
                    if (pNextTempo && (pNextTempo->tempoPMsg.mtTime <= mtTime))
                    {
                        continue;
                    }
                    if (rguidType == GUID_TempoParam)
                    {
                        DMUS_TEMPO_PARAM* pTempoData = (DMUS_TEMPO_PARAM*)pData;
                        pTempoData->mtTime = pInternalTempo->tempoPMsg.mtTime - mtTime;
                        pTempoData->dblTempo = pInternalTempo->tempoPMsg.dblTempo;
                    }
                    else // rguidType == GUID_PrivateTempoParam
                    {
                        PrivateTempo* pTempoData = (PrivateTempo*)pData;
                        pTempoData->mtTime = pInternalTempo->tempoPMsg.mtTime - mtTime;
                        pTempoData->dblTempo = pInternalTempo->tempoPMsg.dblTempo;
                    }
                    if( pmtNext )
                    {
                        *pmtNext = 0;
                    }
                    break;
                }
                LeaveCriticalSection( &m_PipelineCrSec );
                if (pInternalTempo)
                {
                    hr = S_FALSE;
                }
            }
        }
    }
    else // GetParam from a segment succeeded, so we need to clean up the next time parameter to account
         // for loops and end of segment.
    {
        if (pmtNext) // Check to see if the loop end is earlier than *pmtNext.
        {
            if (dwRepeatsLeft && (*pmtNext > (mtLoopEnd - mtSegTime)))
            {
                if (mtLoopEnd >= mtSegTime) // This should always be true, but test anyway.
                {
                    *pmtNext = mtLoopEnd - mtSegTime;
                }
            }
        }
    }
    EnterCriticalSection(&m_MainCrSec);
    if (m_pParamHook && SUCCEEDED(hr))
    {
        hr = m_pParamHook->GetParam(rguidType,dwGroupBits,dwIndex,mtTime,pmtNext,pData,
            pSegSource,m_dwGetParamFlags,hr);

    }
    LeaveCriticalSection(&m_MainCrSec);
    return hr;
}



/*
  @method HRESULT | IDirectMusicPerformance | SetParam |
  Sets data on a Track inside a Primary Segment in this Performance.

  @rvalue S_OK | Success.
  @rvalue DMUS_E_NO_MASTER_CLOCK | There is no master clock in the performance.
  Make sure to call <om .Init> before calling this method.
*/
HRESULT STDMETHODCALLTYPE CPerformance::SetParam(
    REFGUID rguidType,      // @parm The type of data to set.
    DWORD dwGroupBits,      // @parm The group the desired track is in. Use 0xffffffff
                            // for all groups.
    DWORD dwIndex,          // @parm Identifies which track, by index, in the group
                            // identified by <p dwGroupBits> to set the data.
    MUSIC_TIME mtTime,      // @parm The time at which to set the data. Unlike
                            // <om IDirectMusicSegment.SetParam>, this time is in
                            // performance time. The start time of the segment is
                            // subtracted from this time, and <om IDirectMusicSegment.SetParam>
                            // is called.
    void* pData)            // @parm The struture containing the data to set. Each
                            // <p pGuidType> identifies a particular structure of a
                            // particular size. It is important that this field contain
                            // the correct structure of the correct size. Otherwise,
                            // fatal results can occur.
{
    V_INAME(IDirectMusicPerformance::SetParam);
    V_PTR_WRITE_OPT(pData,1);
    V_REFGUID(rguidType);

    EnterCriticalSection(&m_MainCrSec);
    if( m_pClock == NULL )
    {
        LeaveCriticalSection(&m_MainCrSec);
        Trace(1,"Error: SetParam() failed because the performance has not been initialized.\n");
        return DMUS_E_NO_MASTER_CLOCK;
    }
    LeaveCriticalSection(&m_MainCrSec);

    CSegState* pSegNode;
    IDirectMusicSegment* pSegment = NULL;
    HRESULT hr;

    EnterCriticalSection(&m_SegmentCrSec);
    pSegNode = GetPrimarySegmentAtTime( mtTime );

    MUSIC_TIME mtOffset;
    DWORD dwRepeat;
    if( pSegNode )
    {
        pSegment = pSegNode->m_pSegment;
        pSegment->AddRef();
        pSegNode->ConvertToSegTime( &mtTime, &mtOffset, &dwRepeat );
    }
    LeaveCriticalSection(&m_SegmentCrSec);
    if( pSegment )
    {
        hr = pSegment->SetParam( rguidType, dwGroupBits, dwIndex,
                mtTime, pData );
        pSegment->Release();
    }
    else
    {
        Trace(1,"Error: SetParam failed because there is no segment at requested time.\n");
        hr = DMUS_E_NOT_FOUND;
    }
    return hr;
}

/*
  @method HRESULT | IDirectMusicPerformance | GetGlobalParam |
  Gets global values from the Performance.

  @rvalue S_OK | Success.
  @rvalue E_INVALIDARG | <p pGuidType> isn't in the list of global data being handled by this
  Performance. Make sure to call <om IDirectMusicPerformance.SetGlobalParam> first.  Or,
  the value of <p pData> doesn't point to valid memory. Or, <p dwSize> isn't the size
  originally given in <om .SetGlobalParam>
  @rvalue E_POINTER | <p pData> is NULL or invalid.

  @xref <om .SetGlobalParam>
*/
HRESULT STDMETHODCALLTYPE CPerformance::GetGlobalParam(
    REFGUID rguidType,  // @parm Identifies the type of data.
    void* pData,        // @parm Allocated memory to receive a copy of the data. This must be
                        // the correct size, which is constant for each <p pGuidType> type of
                        // data, and was also passed in to <om .SetGlobalParam>.
    DWORD dwSize        // @parm The size of the data in <p pData>. This should be constant for each
                        // <p pGuidType>. This parameter is needed because the Performance doesn't
                        // know about all types of data, allowing new ones to be created as needed.
    )
{
    V_INAME(IDirectMusicPerformance::GetGlobalParam);
    V_REFGUID(rguidType);

    if( dwSize )
    {
        V_BUFPTR_WRITE( pData, dwSize );
    }

    GlobalData* pGD;
    HRESULT hr = S_OK;
    EnterCriticalSection(&m_GlobalDataCrSec);
    for( pGD = m_pGlobalData; pGD; pGD = pGD->pNext )
    {
        if( pGD->guidType == rguidType )
        {
            break;
        }
    }
    if( pGD && ( dwSize == pGD->dwSize ) )
    {
        memcpy( pData, pGD->pData, pGD->dwSize );
    }
    else
    {
#ifdef DBG
        if (pGD && ( dwSize != pGD->dwSize ))
        {
            Trace(1,"Error: GetGlobalParam() failed because the passed data size %ld was inconsistent with %ld, set previously.\n",
                dwSize, pGD->dwSize);
        }
        else
        {
            Trace(4,"Warning: GetGlobalParam() failed because the parameter had never been set.\n");
        }
#endif
        hr = E_INVALIDARG;
    }
    LeaveCriticalSection(&m_GlobalDataCrSec);
    return hr;
}

/*
  @method HRESULT | IDirectMusicPerformance | SetGlobalParam |
  Set global values on the Performance.

  @rvalue S_OK | Success.
  @rvalue E_POINTER | <p pData> is NULL or invalid.
  @rvalue E_OUTOFMEMORY | Ran out of memory.
  @rvalue E_INVALIDARG | Other failure. pData or dwSize not correct?

  @xref <om .GetGlobalParam>
*/
HRESULT STDMETHODCALLTYPE CPerformance::SetGlobalParam(
    REFGUID rguidType,  // @parm Identifies the type of data.
    void* pData,        // @parm The data itself, which will be copied and stored by the Performance.
    DWORD dwSize        // @parm The size of the data in <p pData>. This should be constant for each
                        // <p pGuidType>. This parameter is needed because the Performance doesn't
                        // know about all types of data, allowing new ones to be created as needed.
    )
{
    V_INAME(IDirectMusicPerformance::SetGlobalParam);
    V_REFGUID(rguidType);

    if( dwSize )
    {
        V_BUFPTR_READ( pData, dwSize );
    }

    GlobalData* pGD;
    // see if this is one of our special Performance globals
    if( rguidType == GUID_PerfMasterTempo )
    {
        if( dwSize == sizeof(float) )
        {
            float flt;
            memcpy( &flt, pData, sizeof(float) );
            if( (flt >= DMUS_MASTERTEMPO_MIN) && (flt <= DMUS_MASTERTEMPO_MAX) )
            {
                if( m_fltRelTempo != flt )
                {
                    m_fltRelTempo = flt;
                    // It's only necessary to recalc the tempo map if something is playing
                    EnterCriticalSection(&m_SegmentCrSec);
                    if (GetPrimarySegmentAtTime(m_mtTransported))
                    {
                        RecalcTempoMap(NULL,m_mtTransported);
                    }
                    LeaveCriticalSection(&m_SegmentCrSec);
                }
            }
        }
        else
        {
            Trace(1,"Error: Attempt to set global tempo failed because dwSize is not size of float.\n");
            return E_INVALIDARG;
        }
    }
    else if( rguidType == GUID_PerfMasterVolume )
    {
        // master volume
        if( dwSize == sizeof(long) )
        {
            memcpy( &m_lMasterVolume, pData, sizeof(long) );
        }
        else
        {
            Trace(1,"Error: Attempt to set global volume failed because dwSize is not size of long.\n");
            return E_INVALIDARG;
        }
        // Go through all Ports and set the master volume.
        // This is also done upon adding a Port.
        IDirectMusicPort* pPort;
        DWORD dw;

        EnterCriticalSection(&m_PChannelInfoCrSec);
        for( dw = 0; dw < m_dwNumPorts; dw++ )
        {
            pPort = m_pPortTable[dw].pPort;
            if( pPort )
            {
                IKsControl *pControl;
                if(SUCCEEDED(pPort->QueryInterface(IID_IKsControl, (void**)&pControl)))
                {
                    KSPROPERTY ksp;
                    ULONG cb;

                    memset(&ksp, 0, sizeof(ksp));
                    ksp.Set   = GUID_DMUS_PROP_Volume;
                    ksp.Id    = 0;
                    ksp.Flags = KSPROPERTY_TYPE_SET;

                    pControl->KsProperty(&ksp,
                                         sizeof(ksp),
                                         (LPVOID)&m_lMasterVolume,
                                         sizeof(m_lMasterVolume),
                                         &cb);
                    pControl->Release();
                }
            }
        }
        LeaveCriticalSection(&m_PChannelInfoCrSec);
    }

    // see if this type is already there. If so, use it.
    EnterCriticalSection(&m_GlobalDataCrSec);
    for( pGD = m_pGlobalData; pGD; pGD = pGD->pNext )
    {
        if( pGD->guidType == rguidType )
        {
            break;
        }
    }
    LeaveCriticalSection(&m_GlobalDataCrSec);
    // if it already exists, just copy the new data into the
    // existing memory block and return
    if( pGD )
    {
        if( pGD->dwSize != dwSize )
        {
            Trace(1,"Error: Attempt to set global parameter failed because dwSize is not consistent with previous SetGlobalParam() call.\n");
            return E_INVALIDARG;
        }
        if( dwSize )
        {
            memcpy( pGD->pData, pData, dwSize );
        }
        return S_OK;
    }

    // otherwise, create new memory
    pGD = new GlobalData;
    if( NULL == pGD )
    {
        return E_OUTOFMEMORY;
    }
    pGD->dwSize = dwSize;
    if( dwSize )
    {
        pGD->pData = (void*)(new char[dwSize]);
        if( NULL == pGD->pData )
        {
            delete pGD;
            return E_OUTOFMEMORY;
        }
        memcpy( pGD->pData, pData, dwSize );
    }
    else
    {
        pGD->pData = NULL;
    }
    pGD->guidType = rguidType;
    EnterCriticalSection(&m_GlobalDataCrSec); // just using this one since it's available and not used much
    pGD->pNext = m_pGlobalData;
    m_pGlobalData = pGD;
    LeaveCriticalSection(&m_GlobalDataCrSec);
    return S_OK;
}

// IDirectMusicTool
/*
  @method HRESULT | IDirectMusicTool | Init |
  Called when the Tool is inserted into the Graph, providing the Tool the opportunity
  to initialize itself.

  @rvalue S_OK | Success.
  @rvalue E_NOTIMPL | Not implemented is a valid return for the method.
*/
HRESULT STDMETHODCALLTYPE CPerformance::Init(
         IDirectMusicGraph* pGraph  // @parm The calling graph.
    )
{
    return E_NOTIMPL;
}

inline bool CPerformance::SendShortMsg( IDirectMusicBuffer* pBuffer,
                                   IDirectMusicPort* pPort,DWORD dwMsg,
                                   REFERENCE_TIME rt, DWORD dwGroup)

{
    if( FAILED( pBuffer->PackStructured( rt, dwGroup, dwMsg ) ) )
    {
        // ran out of room in the buffer
        TraceI(2, "RAN OUT OF ROOM IN THE BUFFER!\n");
        pPort->PlayBuffer( pBuffer );
        pBuffer->Flush();
        // try one more time
        if( FAILED( pBuffer->PackStructured( rt, dwGroup, dwMsg ) ) )
        {
            TraceI(1, "MAJOR BUFFER PACKING FAILURE!\n");
            // if it didn't work this time, free the event because something
            // bad has happened.
            return false;
        }
    }
    return true;
}

//////////////////////////////////////////////////////////////////////
// CPerformance::PackNote
/*
  HRESULT | CPerformance | PackNote |
  Converts the message into a midiShortMsg, midiLongMsg, or user message
  and packs it into the appropriate IDirectMusicBuffer in the PortTable,
  setting the m_fBufferFilled flag.

  DMUS_PMSG* | pPMsg |
  [in] The message to pack into the buffer.

  REFERENCE_TIME | mt |
  [in] The time (in the Buffer's clock coordinates) at which to queue the message.

  E_INVALIDARG | Either pPMsg or pBuffer is NULL.
  E_OUTOFMEMORY | Failed to pack the buffer.
  DMUS_S_REQUEUE | Tells the Pipeline to requeue this message.
  DMUS_S_FREE | Tells the Pipeline to free this message.
*/
HRESULT CPerformance::PackNote(
            DMUS_PMSG* pEvent,
            REFERENCE_TIME rt )
{
    DMUS_NOTE_PMSG* pNote = (DMUS_NOTE_PMSG*)pEvent;
    PRIV_PMSG* pPriv = DMUS_TO_PRIV(pEvent);
    REFERENCE_TIME rtLogical; // the time the note occurs in logical music time (subtract offset)
    IDirectMusicBuffer* pBuffer = NULL;
    IDirectMusicPort* pPort = NULL;
    DWORD dwMsg;
    DWORD dwGroup, dwMChannel, dwPortTableIndex;
    short nTranspose = 0;
    short nValue;
    HRESULT hr = DMUS_S_FREE;

    if( NULL == pEvent )
        return E_INVALIDARG;

    if( FAILED( PChannelIndex( pNote->dwPChannel, &dwPortTableIndex, &dwGroup, &dwMChannel,
        &nTranspose )))
    {
        Trace(1,"Play note failed on unassigned PChannel %ld\n",pNote->dwPChannel);
        return DMUS_S_FREE; // the PChannel map wasn't found. Just free the event.
    }
    EnterCriticalSection(&m_PChannelInfoCrSec);
    if( dwPortTableIndex > m_dwNumPorts )
    {
        pPort = NULL; // the PChannel map is out of range of the number of ports
                    // so return outta here! (see after the LeaveCriticalSection)
    }
    else
    {
        pPort = m_pPortTable[dwPortTableIndex].pPort;
        if( pPort ) pPort->AddRef();
        pBuffer = m_pPortTable[dwPortTableIndex].pBuffer;
        if( pBuffer ) pBuffer->AddRef();
    }
    LeaveCriticalSection(&m_PChannelInfoCrSec);
    if(pPort && pBuffer )
    {
        dwMsg = 0;
        if( pNote->bFlags & DMUS_NOTEF_NOTEON )
        {
            // transpose the note's bMidiValue, and store it in the note so the note off
            // plays the correct pitch.
            nValue = pNote->bMidiValue + nTranspose;
            if( ( nValue > 127 ) || ( nValue < 0 )
                || pNote->mtDuration <= 0 )
            {
                // don't play this out-of-range or 0-duration note
                pPort->Release();
                pBuffer->Release();
                return DMUS_S_FREE;
            }
            pNote->bMidiValue = (BYTE)nValue;
            dwMsg |= pNote->bVelocity << 16;
        }
        else if( rt < pPriv->rtLast )
        {
            // the note off will play before the note on. Bad.
            rt = pPriv->rtLast + REF_PER_MIL;
        }
        dwMsg |= pNote->bMidiValue << 8; // set note value
        dwMsg |= dwMChannel; // MIDI Channel
        if( pNote->bFlags & DMUS_NOTEF_NOTEON )
        {
            dwMsg |= MIDI_NOTEON;
        }
        else
        {
            dwMsg |= MIDI_NOTEOFF;
        }

        if (SendShortMsg(pBuffer,pPort,dwMsg,rt-2,dwGroup))
        {
            EnterCriticalSection(&m_PipelineCrSec); // to prevent deadlock in MusicToReferenceTime
            EnterCriticalSection(&m_PChannelInfoCrSec);
            m_pPortTable[dwPortTableIndex].fBufferFilled = TRUE; // so we send this in SendBuffers
            rtLogical = rt;
            // subtract the offset if needed, but only for a note on.
            if( pNote->nOffset && (pNote->bFlags & DMUS_NOTEF_NOTEON))
            {
                MUSIC_TIME mtTemp = pNote->mtTime - pNote->nOffset + 1;
                REFERENCE_TIME rtTemp;
                MusicToReferenceTime( mtTemp, &rtTemp );
                if( rtTemp > rtLogical )
                {
                    rtLogical = rtTemp;
                }
            }
            if( m_pPortTable[dwPortTableIndex].rtLast < rtLogical )
            {
                m_pPortTable[dwPortTableIndex].rtLast = rtLogical;
            }
            LeaveCriticalSection(&m_PChannelInfoCrSec);
            LeaveCriticalSection(&m_PipelineCrSec);

            if( pNote->bFlags & DMUS_NOTEF_NOTEON )
            {
                pPriv->rtLast = rt;
                m_rtHighestPackedNoteOn = rt;
                if (pNote->dwFlags & DMUS_PMSGF_LOCKTOREFTIME)
                {
                    // This is a clock time message.
                    rt = pNote->rtTime;
                    pNote->rtTime += (pNote->mtDuration * REF_PER_MIL);
                    if (pNote->mtDuration > 1)
                    {
                        pNote->rtTime -= REF_PER_MIL;
                    }
                    // subtract 1 to guarantee that a note off at the same time as a note on doesn't
                    // stop the note on short. It's possible that rt == pNote->rtTime, if the duration
                    // was zero, so be sure to check that.
                    if( pNote->rtTime < rt + 1 )
                    {
                        pNote->rtTime = rt + 1;
                    }
                    pNote->bFlags &= ~DMUS_NOTEF_NOTEON; // make this a note off now
                    pNote->dwFlags &= ~DMUS_PMSGF_MUSICTIME;
                    hr = DMUS_S_REQUEUE;
                }
                else
                {
                    pNote->mtTime += pNote->mtDuration;
                    if (pNote->mtDuration > 1)
                    {
                        pNote->mtTime--;
                    }
                    MusicToReferenceTime( pNote->mtTime, &rt );
                    // subtract 1 to guarantee that a note off at the same time as a note on doesn't
                    // stop the note on short. It's possible that rt == pNote->rtTime, if the duration
                    // was zero, so be sure to check that.
                    if( rt < pNote->rtTime + 2 )
                    {
                        rt = pNote->rtTime + 2;
                    }
                    pNote->rtTime = rt - 1;
                }
                pNote->bFlags &= ~DMUS_NOTEF_NOTEON; // make this a note off now
                hr = DMUS_S_REQUEUE;
            }
        }
    }
    if( pPort ) pPort->Release();
    if( pBuffer ) pBuffer->Release();
    return hr;
}

//////////////////////////////////////////////////////////////////////
// CPerformance::PackCurve
HRESULT CPerformance::PackCurve(
            DMUS_PMSG* pEvent,
            REFERENCE_TIME rt )
{
    DMUS_CURVE_PMSG* pCurve = (DMUS_CURVE_PMSG*)pEvent;
    IDirectMusicBuffer* pBuffer = NULL;
    IDirectMusicPort* pPort = NULL;
    DWORD dwMsg;
    HRESULT hr = DMUS_S_FREE;
    BOOL fCalcStartValue = FALSE;
    CChannelMap *pChannelMap = NULL;

    if( NULL == pEvent )
        return E_INVALIDARG;

    // store the original start time so we know how far into the curve we are
    if( pCurve->mtOriginalStart == 0 )
    {
        // if we're flushing and have never played this curve at all, just free
        // it.
        if( pCurve->dwFlags & DMUS_PMSGF_TOOL_FLUSH )
        {
            return DMUS_S_FREE;
        }
        if (pCurve->dwFlags & DMUS_PMSGF_LOCKTOREFTIME)
        {
            // This is a clock time message. Convert the duration into music time. It will act as
            // a music time message from now on. This does have the downside that if a dramatic tempo
            // change occurs in the middle of a lengthy curve, the end time can be distorted.
            // But, given the purpose of curves, this is really an unlikely issue.
            MUSIC_TIME mtTemp;
            ReferenceToMusicTime(pCurve->rtTime + (pCurve->mtDuration * REF_PER_MIL),&mtTemp);
            mtTemp -= pCurve->mtTime;
            pCurve->mtDuration = mtTemp;
            ReferenceToMusicTime(pCurve->rtTime + (pCurve->mtResetDuration * REF_PER_MIL),&mtTemp);
            mtTemp -= pCurve->mtTime;
            pCurve->mtResetDuration = mtTemp;
            pCurve->dwFlags &= ~DMUS_PMSGF_LOCKTOREFTIME;
        }
        pCurve->mtOriginalStart = pCurve->mtTime;
        // check the latency clock. Adjust pCurve->mtTime if needed. This can happen
        // if the curve is time-stamped for the past. We only need do this for non-instant
        // curve types.
        if( pCurve->bCurveShape != DMUS_CURVES_INSTANT )
        {
            REFERENCE_TIME rtLatency = GetLatency();
            MUSIC_TIME mtLatency;
            ReferenceToMusicTime( rtLatency, &mtLatency );
            if( pCurve->mtTime < mtLatency )
            {
                if( pCurve->mtTime + pCurve->mtDuration < mtLatency )
                {
                    // If it is far enough in the past,
                    // we only need to send out the final value.
                    pCurve->mtTime += pCurve->mtDuration;
                }
                else
                {
                    pCurve->mtTime = mtLatency;
                }
            }
            // If this is the start of a curve and we are supposed to start with the current playing value...
            if (pCurve->bFlags & DMUS_CURVE_START_FROM_CURRENT)
            {
                fCalcStartValue = TRUE;
            }
            else
            {
                pCurve->wMeasure = (WORD) ComputeCurveTimeSlice(pCurve);    // Use this to store the time slice interval.
            }
        }
    }
    // it is necessary to check reset duration >= 0 because it could have been set
    // to be negative by the flushing, and we don't want to toss it in that case.
    // (should no longer be necessary to check, as a result of fixing 33987)
    if( ( pCurve->bFlags & DMUS_CURVE_RESET ) && (pCurve->mtResetDuration >= 0) && ( pCurve->mtTime ==
        pCurve->mtDuration + pCurve->mtResetDuration + pCurve->mtOriginalStart ))
    {
        if( !( pCurve->dwFlags & DMUS_PMSGF_TOOL_FLUSH ) )
        {
            PRIV_PMSG* pPrivPMsg = DMUS_TO_PRIV(pEvent);
            if ( (pPrivPMsg->dwPrivFlags & PRIV_FLAG_FLUSH) )
            {
                pPrivPMsg->dwPrivFlags &= ~PRIV_FLAG_FLUSH;
                pCurve->dwFlags |= DMUS_PMSGF_TOOL_FLUSH;
                MUSIC_TIME mt = 0;
                if( rt <= pPrivPMsg->rtLast )
                {
                    return PackCurve( pEvent, pPrivPMsg->rtLast + REF_PER_MIL );
                }
                else
                {
                    return PackCurve( pEvent, rt );
                }
            }
            else
            {
                // the reset duration has expired, and we're not flushing, so expire the event.
                return DMUS_S_FREE;
            }
        }
    }
    EnterCriticalSection(&m_PChannelInfoCrSec);
    pChannelMap = GetPChannelMap(pCurve->dwPChannel);
    if (!pChannelMap)
    {
        Trace(1,"Play curve failed on unassigned PChannel %ld\n",pCurve->dwPChannel);
        LeaveCriticalSection(&m_PChannelInfoCrSec);
        return DMUS_S_FREE; // the PChannel map wasn't found. Just free the event.
    }
    if( pChannelMap->dwPortIndex > m_dwNumPorts )
    {
        pPort = NULL; // the PChannel map is out of range of the number of ports
                    // so return outta here! (see after the LeaveCriticalSection)
    }
    else
    {
        pPort = m_pPortTable[pChannelMap->dwPortIndex].pPort;
        if( pPort ) pPort->AddRef();
        pBuffer = m_pPortTable[pChannelMap->dwPortIndex].pBuffer;
        if( pBuffer ) pBuffer->AddRef();
    }
    if( pPort && pBuffer)
    {
        DWORD dwCurve;
        DWORD dwMergeIndex = 0;
        dwMsg = 0;
        if (pCurve->dwFlags & DMUS_PMSGF_DX8)
        {
            dwMergeIndex = pCurve->wMergeIndex;
        }
        switch( pCurve->bType )
        {
        case DMUS_CURVET_PBCURVE:
            if (fCalcStartValue)
            {
                pCurve->nStartValue =
                    (short) pChannelMap->m_PitchbendMerger.GetIndexedValue(dwMergeIndex) + 0x2000;
            }
            dwCurve = ComputeCurve( pCurve );
            dwCurve = pChannelMap->m_PitchbendMerger.MergeValue(dwMergeIndex,(long)dwCurve,0x2000,0x3FFF);
            dwMsg = MIDI_PBEND;
            dwMsg |= ( (dwCurve & 0x7F) << 8);
            dwCurve = dwCurve >> 7;
            dwMsg |= ( (dwCurve & 0x7F) << 16);
            break;
        case DMUS_CURVET_CCCURVE:
            switch (pCurve->bCCData)
            {
            case MIDI_CC_MOD_WHEEL:
                if (fCalcStartValue)
                {
                    pCurve->nStartValue =
                        (short) pChannelMap->m_ModWheelMerger.GetIndexedValue(dwMergeIndex) + 0x7F;
                }
                dwCurve = ComputeCurve( pCurve );
                dwCurve = pChannelMap->m_ModWheelMerger.MergeValue(dwMergeIndex,(long)dwCurve,0x7F,0x7F);
                break;
            case MIDI_CC_VOLUME:
                if (fCalcStartValue)
                {
                    pCurve->nStartValue =
                        (short) pChannelMap->m_VolumeMerger.GetVolumeStart(dwMergeIndex);
                }
                dwCurve = ComputeCurve( pCurve );
                dwCurve = pChannelMap->m_VolumeMerger.MergeMidiVolume(dwMergeIndex,(BYTE) dwCurve);
                break;
            case MIDI_CC_PAN:
                if (fCalcStartValue)
                {
                    pCurve->nStartValue =
                        (short) pChannelMap->m_PanMerger.GetIndexedValue(dwMergeIndex) + 0x40;
                }
                dwCurve = ComputeCurve( pCurve );
                dwCurve = pChannelMap->m_PanMerger.MergeValue(dwMergeIndex,(long)dwCurve,0x40,0x7F);
                break;
            case MIDI_CC_EXPRESSION:
                if (fCalcStartValue)
                {
                    pCurve->nStartValue =
                        (short) pChannelMap->m_ExpressionMerger.GetVolumeStart(dwMergeIndex);
                }
                dwCurve = ComputeCurve( pCurve );
                dwCurve = pChannelMap->m_ExpressionMerger.MergeMidiVolume(dwMergeIndex,(BYTE) dwCurve);
                break;
            case MIDI_CC_FILTER:
                if (fCalcStartValue)
                {
                    pCurve->nStartValue =
                        (short) pChannelMap->m_FilterMerger.GetIndexedValue(dwMergeIndex) + 0x40;
                }
                dwCurve = ComputeCurve( pCurve );
                dwCurve = pChannelMap->m_FilterMerger.MergeValue(dwMergeIndex,(long)dwCurve,0x40,0x7F);
                break;
            case MIDI_CC_REVERB:
                if (fCalcStartValue)
                {
                    pCurve->nStartValue =
                        (short) pChannelMap->m_ReverbMerger.GetIndexedValue(dwMergeIndex) + 0x7F;
                }
                dwCurve = ComputeCurve( pCurve );
                dwCurve = pChannelMap->m_ReverbMerger.MergeValue(dwMergeIndex,(long)dwCurve,0x7F,0x7F);
                break;
            case MIDI_CC_CHORUS:
                if (fCalcStartValue)
                {
                    pCurve->nStartValue =
                        (short) pChannelMap->m_ChorusMerger.GetIndexedValue(dwMergeIndex) + 0x7F;
                }
                dwCurve = ComputeCurve( pCurve );
                dwCurve = pChannelMap->m_ChorusMerger.MergeValue(dwMergeIndex,(long)dwCurve,0x7F,0x7F);
                break;
            case MIDI_CC_RESETALL:
                dwCurve = ComputeCurve( pCurve );
                pChannelMap->Reset(pCurve->nEndValue);
                break;
            default:
                dwCurve = ComputeCurve( pCurve );
                break;
            }
            dwMsg = MIDI_CCHANGE;
            dwMsg |= (pCurve->bCCData << 8);
            dwMsg |= (dwCurve << 16);
            break;
        case DMUS_CURVET_MATCURVE:
            dwCurve = ComputeCurve( pCurve );
            dwMsg = MIDI_MTOUCH;
            dwMsg |= (dwCurve << 8);
            break;
        case DMUS_CURVET_PATCURVE:
            dwCurve = ComputeCurve( pCurve );
            dwMsg = MIDI_PTOUCH;
            dwMsg |= (pCurve->bCCData << 8);
            dwMsg |= (dwCurve << 16);
            break;
        case DMUS_CURVET_RPNCURVE:
        case DMUS_CURVET_NRPNCURVE:
            if (pCurve->dwFlags & DMUS_PMSGF_DX8)
            {
                dwCurve = ComputeCurve( pCurve );
                DWORD dwMsg2 = MIDI_CCHANGE;
                dwMsg = MIDI_CCHANGE;
                // First, send the two CC commands to select which RPN or NRPN event.
                if (pCurve->bType == DMUS_CURVET_RPNCURVE)
                {
                    dwMsg |= (MIDI_CC_RPN_MSB << 8);
                    dwMsg2 |= (MIDI_CC_RPN_LSB << 8);
                }
                else
                {
                    dwMsg |= (MIDI_CC_NRPN_MSB << 8);
                    dwMsg2 |= (MIDI_CC_NRPN_LSB << 8);
                }
                dwMsg |= (pCurve->wParamType  & 0x3F80) << 9;  // Upper 8 bits of command #
                dwMsg2 |= (pCurve->wParamType & 0x7F) << 16;   // Lower 8 bits.
                dwMsg |= pChannelMap->dwMChannel; // MIDI Channel
                dwMsg2 |= pChannelMap->dwMChannel; // MIDI Channel
                SendShortMsg(pBuffer,pPort,dwMsg,rt-3,pChannelMap->dwGroup); // Too bad if it fails!
                SendShortMsg(pBuffer,pPort,dwMsg2,rt-2,pChannelMap->dwGroup);
                // Then, send the two data CC commands.
                dwMsg = MIDI_CCHANGE | (MIDI_CC_DATAENTRYMSB << 8);
                dwMsg |= (dwCurve & 0x3F80) << 9;  // Upper 8 bits of data
                dwMsg |= pChannelMap->dwMChannel; // MIDI Channel
                SendShortMsg(pBuffer,pPort,dwMsg,rt-1,pChannelMap->dwGroup);
                dwMsg = MIDI_CCHANGE | (MIDI_CC_DATAENTRYLSB << 8);
                dwMsg |= (dwCurve & 0x7F) << 16;  // Lower 8 bits of data
            }
        }
        if (dwMsg) // Make sure we successfully created a message.
        {
            dwMsg |= pChannelMap->dwMChannel; // MIDI Channel
            if (SendShortMsg(pBuffer,pPort,dwMsg,rt,pChannelMap->dwGroup))
            {
                m_pPortTable[pChannelMap->dwPortIndex].fBufferFilled = TRUE; // so we send this in SendBuffers
                m_pPortTable[pChannelMap->dwPortIndex].rtLast = rt;

                // ComputeCurve() will set this to 0 if it's time to free the event. Otherwise, it
                // will set it to the next time this event should be performed.
                if( pCurve->rtTime )
                {
                    // If we didn't calculate the time slice because we didn't know
                    // what the start value was, do it now.
                    if (fCalcStartValue)
                    {
                        pCurve->wMeasure = (WORD) ComputeCurveTimeSlice(pCurve);    // Use this to store the time slice interval.
                    }
                    hr = DMUS_S_REQUEUE;
                }
            }
        }
    }
    LeaveCriticalSection(&m_PChannelInfoCrSec);
    if( pPort ) pPort->Release();
    if( pBuffer ) pBuffer->Release();
    return hr;
}

//////////////////////////////////////////////////////////////////////
// CPerformance::PackMidi
HRESULT CPerformance::PackMidi(
            DMUS_PMSG* pEvent,
            REFERENCE_TIME rt )
{
    DMUS_MIDI_PMSG* pMidi = (DMUS_MIDI_PMSG*)pEvent;
    IDirectMusicBuffer* pBuffer = NULL;
    IDirectMusicPort* pPort = NULL;
    DWORD dwMsg;
//  DWORD dwGroup, dwMChannel, dwPortTableIndex;
    HRESULT hr = DMUS_S_FREE;
    CChannelMap *pChannelMap = NULL;

    if( NULL == pMidi )
        return E_INVALIDARG;

    EnterCriticalSection(&m_PChannelInfoCrSec);
    pChannelMap = GetPChannelMap(pMidi->dwPChannel);
    if (!pChannelMap)
    {
        Trace(1,"Play MIDI failed on unassigned PChannel %ld\n",pMidi->dwPChannel);
        LeaveCriticalSection(&m_PChannelInfoCrSec);
        return DMUS_S_FREE; // the PChannel map wasn't found. Just free the event.
    }

    if( pChannelMap->dwPortIndex > m_dwNumPorts )
    {
        pPort = NULL; // the PChannel map is out of range of the number of ports
                    // so return outta here! (see after the LeaveCriticalSection)
    }
    else
    {
        pPort = m_pPortTable[pChannelMap->dwPortIndex].pPort;
        if( pPort ) pPort->AddRef();
        pBuffer = m_pPortTable[pChannelMap->dwPortIndex].pBuffer;
        if( pBuffer ) pBuffer->AddRef();
    }
    if(pPort && pBuffer )
    {
        pMidi->bStatus &= 0xF0;
        if (pMidi->bStatus == MIDI_CCHANGE)
        {
            switch (pMidi->bByte1)
            {
            case MIDI_CC_MOD_WHEEL:
                pMidi->bByte2 = (BYTE) pChannelMap->m_ModWheelMerger.MergeValue(0,pMidi->bByte2,0x7F,0x7F);
                break;
            case MIDI_CC_VOLUME:
                pMidi->bByte2 = pChannelMap->m_VolumeMerger.MergeMidiVolume(0,pMidi->bByte2);
                break;
            case MIDI_CC_PAN:
                pMidi->bByte2 = (BYTE) pChannelMap->m_PanMerger.MergeValue(0,pMidi->bByte2,0x40,0x7F);
                break;
            case MIDI_CC_EXPRESSION:
                pMidi->bByte2 = pChannelMap->m_ExpressionMerger.MergeMidiVolume(0,pMidi->bByte2);
                break;
            case MIDI_CC_FILTER:
                pMidi->bByte2 = (BYTE) pChannelMap->m_FilterMerger.MergeValue(0,pMidi->bByte2,0x40,0x7F);
                break;
            case MIDI_CC_REVERB:
                pMidi->bByte2 = (BYTE) pChannelMap->m_ReverbMerger.MergeValue(0,pMidi->bByte2,0x7F,0x7F);
                break;
            case MIDI_CC_CHORUS:
                pMidi->bByte2 = (BYTE) pChannelMap->m_ChorusMerger.MergeValue(0,pMidi->bByte2,0x7F,0x7F);
                break;
            case MIDI_CC_RESETALL:
                pChannelMap->Reset(pMidi->bByte2);
                break;
            }

        }
        else if (pMidi->bStatus == MIDI_PBEND)
        {
            WORD wBend = pMidi->bByte1 | (pMidi->bByte2 << 7);
            wBend = (WORD) pChannelMap->m_PitchbendMerger.MergeValue(0,wBend,0x2000,0x3FFF);
            pMidi->bByte1 = wBend & 0x7F;
            pMidi->bByte2 = (wBend >> 7) & 0x7F;
        }
        dwMsg = pMidi->bByte1 << 8;
        dwMsg |= pMidi->bByte2 << 16;
        dwMsg |= pMidi->bStatus;
        dwMsg |= pChannelMap->dwMChannel;
        if (SendShortMsg(pBuffer,pPort,dwMsg,rt,pChannelMap->dwGroup))
        {
            m_pPortTable[pChannelMap->dwPortIndex].fBufferFilled = TRUE; // so we send this in SendBuffers
            m_pPortTable[pChannelMap->dwPortIndex].rtLast = rt;
        }
    }
    LeaveCriticalSection(&m_PChannelInfoCrSec);
    if( pPort ) pPort->Release();
    if( pBuffer ) pBuffer->Release();
    return hr;
}

//////////////////////////////////////////////////////////////////////
// CPerformance::PackSysEx
HRESULT CPerformance::PackSysEx(
            DMUS_PMSG* pEvent,
            REFERENCE_TIME rt )
{
    DMUS_SYSEX_PMSG* pSysEx = (DMUS_SYSEX_PMSG*)pEvent;
    IDirectMusicBuffer* pBuffer = NULL;
    IDirectMusicPort* pPort = NULL;
    DWORD dwGroup, dwMChannel, dwPortTableIndex;
    HRESULT hr = DMUS_S_FREE;

    if( NULL == pEvent )
        return E_INVALIDARG;

    if( NULL == m_pDirectMusic )
        return DMUS_E_NOT_INIT;

    if( FAILED( PChannelIndex( pSysEx->dwPChannel, &dwPortTableIndex, &dwGroup, &dwMChannel)))
    {
        Trace(1,"Play SysEx failed on unassigned PChannel %ld\n",pSysEx->dwPChannel);
        return DMUS_S_FREE; // the PChannel map wasn't found. Just free the event.
    }
    EnterCriticalSection(&m_PChannelInfoCrSec);
    if( dwPortTableIndex > m_dwNumPorts )
    {
        pPort = NULL; // the PChannel map is out of range of the number of ports
                    // so return outta here! (see after the LeaveCriticalSection)
    }
    else
    {
        pPort = m_pPortTable[dwPortTableIndex].pPort;
        if( pPort ) pPort->AddRef();
    }
    LeaveCriticalSection(&m_PChannelInfoCrSec);
    if( pPort )
    {
        // create a buffer of the right size
        DMUS_BUFFERDESC dmbd;
        memset( &dmbd, 0, sizeof(DMUS_BUFFERDESC) );
        dmbd.dwSize = sizeof(DMUS_BUFFERDESC);
        dmbd.cbBuffer = pSysEx->dwLen + 48;

        EnterCriticalSection(&m_MainCrSec);
        if( SUCCEEDED( m_pDirectMusic->CreateMusicBuffer(&dmbd, &pBuffer, NULL)))
        {
            if( SUCCEEDED( pBuffer->PackUnstructured( rt - 4, dwGroup, pSysEx->dwLen, pSysEx->abData ) ) )
            {
                pPort->PlayBuffer(pBuffer);
            }
            pBuffer->Release();
        }
        LeaveCriticalSection(&m_MainCrSec);
    }
    if( pPort ) pPort->Release();
    return hr;
}

//////////////////////////////////////////////////////////////////////
// CPerformance::PackPatch
HRESULT CPerformance::PackPatch(
            DMUS_PMSG* pEvent,
            REFERENCE_TIME rt )
{
    DMUS_PATCH_PMSG* pPatch = (DMUS_PATCH_PMSG*)pEvent;
    IDirectMusicBuffer* pBuffer = NULL;
    IDirectMusicPort* pPort = NULL;
    DWORD dwGroup, dwMChannel, dwPortTableIndex;
    DWORD dwMsg;
    HRESULT hr = DMUS_S_FREE;

    if( NULL == pEvent )
        return E_INVALIDARG;

    if( FAILED( PChannelIndex( pPatch->dwPChannel, &dwPortTableIndex, &dwGroup, &dwMChannel)))
    {
        Trace(1,"Play Patch failed on unassigned PChannel %ld\n",pPatch->dwPChannel);
        return DMUS_S_FREE; // the PChannel map wasn't found. Just free the event.
    }
    EnterCriticalSection(&m_PChannelInfoCrSec);
    if( dwPortTableIndex > m_dwNumPorts )
    {
        pPort = NULL; // the PChannel map is out of range of the number of ports
                    // so return outta here! (see after the LeaveCriticalSection)
    }
    else
    {
        pPort = m_pPortTable[dwPortTableIndex].pPort;
        if( pPort ) pPort->AddRef();
        pBuffer = m_pPortTable[dwPortTableIndex].pBuffer;
        if( pBuffer ) pBuffer->AddRef();
    }
    if( pPort && pBuffer)
    {
        // subtract 10 from rt to guarantee that patch events always go out earlier than
        // notes with the same time stamp.
        rt -= 10;
        // send the bank select lsb
        dwMsg = MIDI_CCHANGE;
        dwMsg |= ( MIDI_CC_BS_LSB << 8 );
        dwMsg |= (pPatch->byLSB << 16);
        ASSERT( dwMChannel < 16 );
        dwMsg |= dwMChannel;
        SendShortMsg(pBuffer,pPort,dwMsg,rt-2,dwGroup);
        // send the bank select msb
        dwMsg = MIDI_CCHANGE;
        dwMsg |= ( MIDI_CC_BS_MSB << 8 );
        dwMsg |= (pPatch->byMSB << 16);
        dwMsg |= dwMChannel;
        SendShortMsg(pBuffer,pPort,dwMsg,rt-1,dwGroup);
        // send the program change
        dwMsg = MIDI_PCHANGE;
        dwMsg |= (pPatch->byInstrument << 8);
        dwMsg |= dwMChannel;
        if (SendShortMsg(pBuffer,pPort,dwMsg,rt,dwGroup))
        {
            m_pPortTable[dwPortTableIndex].fBufferFilled = TRUE; // so we send this in SendBuffers
            m_pPortTable[dwPortTableIndex].rtLast = rt;
        }
    }
    LeaveCriticalSection(&m_PChannelInfoCrSec);
    if( pPort ) pPort->Release();
    if( pBuffer ) pBuffer->Release();
    return hr;
}

HRESULT CPerformance::PackWave(DMUS_PMSG* pPMsg, REFERENCE_TIME rtTime)
{
    DMUS_WAVE_PMSG* pWave = (DMUS_WAVE_PMSG*)pPMsg;
    HRESULT hr = DMUS_S_FREE;

    IDirectMusicVoiceP *pVoice = (IDirectMusicVoiceP *) pWave->punkUser;
    if (pVoice)
    {
        if (pWave->bFlags & DMUS_WAVEF_OFF)
        {
            pVoice->Stop(rtTime);
            EnterCriticalSection(&m_SegmentCrSec);
            for (DWORD dwCount = 0; dwCount < SQ_COUNT; dwCount++)
            {
                for( CSegState* pSegSt = m_SegStateQueues[dwCount].GetHead(); pSegSt; pSegSt = pSegSt->GetNext() )
                {
                    CTrack* pTrack = pSegSt->m_TrackList.GetHead();
                    while( pTrack )
                    {
                        if (pTrack->m_guidClassID == CLSID_DirectMusicWaveTrack)
                        {
                            IPrivateWaveTrack* pWaveTrack = NULL;
                            if (pTrack->m_pTrack &&
                                SUCCEEDED(pTrack->m_pTrack->QueryInterface(IID_IPrivateWaveTrack, (void**)&pWaveTrack)))
                            {
                                pWaveTrack->OnVoiceEnd(pVoice, pTrack->m_pTrackState);
                                pWaveTrack->Release();
                            }
                        }
                        pTrack = pTrack->GetNext();
                    }
                }
            }
            LeaveCriticalSection(&m_SegmentCrSec);
        }
        else
        {
            if (SUCCEEDED(pVoice->Play(rtTime, pWave->lPitch, pWave->lVolume)))
            {
                if (pWave->dwFlags & DMUS_PMSGF_LOCKTOREFTIME)
                {
                    // This is a clock time message.
                    pWave->rtTime += pWave->rtDuration ;
                    pWave->dwFlags &= ~DMUS_PMSGF_MUSICTIME;

                }
                else
                {
                    pWave->mtTime += (MUSIC_TIME) pWave->rtDuration;
                    pWave->dwFlags &= ~DMUS_PMSGF_REFTIME;
                }
                pWave->bFlags |= DMUS_WAVEF_OFF;   // Queue this back up as a wave off.
                hr = DMUS_S_REQUEUE;
            }
        }
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE CPerformance::ProcessPMsg(
    IDirectMusicPerformance* pPerf, // @parm The performance pointer.
    DMUS_PMSG* pPMsg            // @parm The message to process.
    )
{
    V_INAME(IDirectMusicTool::ProcessPMsg);
    V_INTERFACE(pPerf);
    V_BUFPTR_WRITE(pPMsg,sizeof(DMUS_PMSG));

    if (m_rtQueuePosition > pPMsg->rtTime + 50000000)
    {
        // pMSg is more than 5 seconds in the past; get rid of it unless it's signalling the
        // end of something that's already been started.
        if (pPMsg->dwType == DMUS_PMSGT_NOTIFICATION)
        {
            DMUS_NOTIFICATION_PMSG* pNotify = (DMUS_NOTIFICATION_PMSG*)pPMsg;
            if ( (pNotify->guidNotificationType == GUID_NOTIFICATION_PERFORMANCE &&
                  pNotify->dwNotificationOption != DMUS_NOTIFICATION_MUSICSTOPPED) ||
                 (pNotify->guidNotificationType == GUID_NOTIFICATION_SEGMENT &&
                  pNotify->dwNotificationOption != DMUS_NOTIFICATION_SEGEND) )
            {
                return DMUS_S_FREE;
            }
        }
        else if (pPMsg->dwType == DMUS_PMSGT_NOTE)
        {
            DMUS_NOTE_PMSG* pNote = (DMUS_NOTE_PMSG*)pPMsg;
            if (pNote->bFlags & DMUS_NOTEF_NOTEON)
            {
                return DMUS_S_FREE;
            }
        }
        else if (pPMsg->dwType == DMUS_PMSGT_WAVE)
        {
            DMUS_WAVE_PMSG* pWave = (DMUS_WAVE_PMSG*)pPMsg;
            if (!(pWave->bFlags & DMUS_WAVEF_OFF))
            {
                return DMUS_S_FREE;
            }
        }
        else
        {
            return DMUS_S_FREE;
        }
    }

    HRESULT hr = DMUS_S_FREE;

    ASSERT( pPerf == this );
    if( pPMsg->dwType == DMUS_PMSGT_TEMPO )
    {
        PRIV_PMSG* pPrivPMsg = DMUS_TO_PRIV(pPMsg);
        // If the pmsg was generated by a track, discard it
        // because it was already placed in the tempo map.
        if( pPrivPMsg->dwPrivFlags & PRIV_FLAG_TRACK )
        {
            return DMUS_S_FREE;
        }
        // Otherwise, this was generated by the application, so it's not already
        // in the tempo map and we need to add it.
        AddEventToTempoMap( DMUS_TO_PRIV(pPMsg));
        return DMUS_S_FREE; // OK to free this event; not requeued
    }

    if ((pPMsg->dwPChannel == DMUS_PCHANNEL_BROADCAST_GROUPS) ||
        (pPMsg->dwPChannel == DMUS_PCHANNEL_BROADCAST_PERFORMANCE))
    {
        // Scan through all the pchannels and make copies of the message for each pchannel.
        // Then, release this one.
        DWORD dwMax = PCHANNEL_BLOCKSIZE;
        // If one per channel group (for sysex, for example,) do only one per block.
        if (pPMsg->dwPChannel == DMUS_PCHANNEL_BROADCAST_GROUPS) dwMax = 1;
        EnterCriticalSection(&m_PipelineCrSec); // Make sure we are in this so we don't deadlock in SendPMsg().
        EnterCriticalSection(&m_PChannelInfoCrSec);
        CChannelBlock*  pChannelBlock = m_ChannelBlockList.GetHead();
        for( ; pChannelBlock; pChannelBlock = pChannelBlock->GetNext() )
        {
            DWORD dwIndex;
            for (dwIndex = 0; dwIndex < dwMax; dwIndex++)
            {
                CChannelMap* pChannelMap = &pChannelBlock->m_aChannelMap[ dwIndex ];
                if( pChannelMap->dwGroup &&
                    (pChannelMap->wFlags & (CMAP_STATIC | CMAP_VIRTUAL)))
                {
                    DWORD dwPChannel = dwIndex + pChannelBlock->m_dwPChannelStart;
                    // If this is a transpose on the drum channel, don't send it.
                    if ((pPMsg->dwType != DMUS_PMSGT_TRANSPOSE) || ((dwPChannel & 0xF) != 9))
                    {
                        DMUS_PMSG *pNewMsg;
                        if (SUCCEEDED(ClonePMsg(pPMsg,&pNewMsg)))
                        {
                            pNewMsg->dwPChannel = dwIndex + pChannelBlock->m_dwPChannelStart;
                            SendPMsg(pNewMsg);
                        }
                    }
                }
            }
        }
        LeaveCriticalSection(&m_PChannelInfoCrSec);
        LeaveCriticalSection(&m_PipelineCrSec);
        return DMUS_S_FREE;
    }

    if(pPMsg->dwType == DMUS_PMSGT_TRANSPOSE)
    {
        if( !( pPMsg->dwFlags & DMUS_PMSGF_TOOL_QUEUE ))
        {
            // requeue any tranpose event to be queue time
            pPMsg->dwFlags |= DMUS_PMSGF_TOOL_QUEUE;
            pPMsg->dwFlags &= ~( DMUS_PMSGF_TOOL_ATTIME | DMUS_PMSGF_TOOL_IMMEDIATE );
            return DMUS_S_REQUEUE;
        }
        else
        {
            DMUS_TRANSPOSE_PMSG* pTrans = (DMUS_TRANSPOSE_PMSG*)pPMsg;
            // set the PChannel for this transpose message
            EnterCriticalSection(&m_PChannelInfoCrSec);
            CChannelMap * pChannelMap = GetPChannelMap(pPMsg->dwPChannel);
            if (pChannelMap)
            {
                WORD wMergeIndex = 0;
                if (pPMsg->dwFlags & DMUS_PMSGF_DX8)
                {
                    wMergeIndex = pTrans->wMergeIndex;
                }
                pChannelMap->nTranspose = pChannelMap->m_TransposeMerger.MergeTranspose(
                    wMergeIndex,pTrans->nTranspose);
            }
            LeaveCriticalSection(&m_PChannelInfoCrSec);
            return DMUS_S_FREE;
        }
    }

    if(pPMsg->dwType == DMUS_PMSGT_NOTIFICATION )
    {
        DMUS_NOTIFICATION_PMSG* pNotify = (DMUS_NOTIFICATION_PMSG*)pPMsg;
        if (pNotify->guidNotificationType == GUID_NOTIFICATION_PRIVATE_CHORD)
        {
            // if we've got a GUID_NOTIFICATION_PRIVATE_CHORD,
            // invalidate/regenerate queued note events as necessary
            EnterCriticalSection(&m_PipelineCrSec);
            OnChordUpdateEventQueues(pNotify);
            LeaveCriticalSection(&m_PipelineCrSec);
            return DMUS_S_FREE;
        }
        else if( !( pPMsg->dwFlags & DMUS_PMSGF_TOOL_ATTIME ))
        {
            // requeue any notification event to be ontime
            pPMsg->dwFlags |= DMUS_PMSGF_TOOL_ATTIME;
            pPMsg->dwFlags &= ~( DMUS_PMSGF_TOOL_QUEUE | DMUS_PMSGF_TOOL_IMMEDIATE );
            return DMUS_S_REQUEUE;
        }
        else
        {
            // otherwise, fire the notification
            // first, move the event into the notification queue.
            // The app then calls GetNotificationPMsg to get the event.
            CLEARTOOLGRAPH(pPMsg);
            EnterCriticalSection(&m_PipelineCrSec);
            m_NotificationQueue.Enqueue( DMUS_TO_PRIV(pPMsg) );
            LeaveCriticalSection(&m_PipelineCrSec);
            EnterCriticalSection(&m_MainCrSec);
            if( m_hNotification )
            {
                SetEvent(m_hNotification);
            }
            LeaveCriticalSection(&m_MainCrSec);
            return S_OK; // don't free since we've placed the event into the
            // notification queue
        }
    }

    // add time signature changes to the time sig queue
    if(pPMsg->dwType == DMUS_PMSGT_TIMESIG )
    {
        CLEARTOOLGRAPH(pPMsg);
        DMUS_TIMESIG_PMSG* pTimeSig = (DMUS_TIMESIG_PMSG*)pPMsg;

        // check for a legal time signature, which may not have any
        // members equal to 0, and bBeat must be evenly divisible by 2.
        if( pTimeSig->wGridsPerBeat &&
            pTimeSig->bBeatsPerMeasure &&
            pTimeSig->bBeat &&
            ( 0 == ( pTimeSig->bBeat % 2 )))
        {
            EnterCriticalSection(&m_PipelineCrSec);
            REFERENCE_TIME rtNow = GetTime() - (10000 * 1000); // keep around for a second.
            PRIV_PMSG* pCheck;
            while (pCheck = m_TimeSigQueue.FlushOldest(rtNow))
            {
                FreePMsg(pCheck);
            }
            m_TimeSigQueue.Enqueue(  DMUS_TO_PRIV(pPMsg) );
            LeaveCriticalSection(&m_PipelineCrSec);
            return S_OK;
        }
        else
        {
            return DMUS_S_FREE;
        }
    }

    // requeue anything else that's early to be neartime
    if (pPMsg->dwFlags & DMUS_PMSGF_TOOL_IMMEDIATE)
    {
        // if this is a stop command, make sure the segment state doesn't keep going
        if( pPMsg->dwType == DMUS_PMSGT_STOP )
        {
            IDirectMusicSegment* pSeg = NULL;
            IDirectMusicSegmentState* pSegState = NULL;
            if( pPMsg->punkUser )
            {
                if( FAILED( pPMsg->punkUser->QueryInterface( IID_IDirectMusicSegment,
                    (void**)&pSeg )))
                {
                    pSeg = NULL;
                }
                else if( FAILED( pPMsg->punkUser->QueryInterface( IID_IDirectMusicSegmentState,
                    (void**)&pSegState )))
                {
                    pSegState = NULL;
                }
            }
            if( pSeg || pSegState )
            {
                EnterCriticalSection(&m_SegmentCrSec);
                if( pPMsg->mtTime > m_mtTransported )
                {
                    // find and mark the segment and/or segment state to not play beyond
                    // the stop point.
                    CSegState* pNode;
                    DWORD dwCount;
                    for (dwCount = 0; dwCount < SQ_COUNT; dwCount++)
                    {
                        for( pNode = m_SegStateQueues[dwCount].GetHead(); pNode; pNode = pNode->GetNext() )
                        {
                            if( (pNode->m_pSegment == pSeg) ||
                                (pNode == pSegState) )
                            {
                                pNode->m_mtStopTime = pPMsg->mtTime;
                            }
                        }
                    }
                }
                LeaveCriticalSection(&m_SegmentCrSec);
                if( pSeg )
                {
                    pSeg->Release();
                }
                if( pSegState )
                {
                    pSegState->Release();
                }
            }
        }
        pPMsg->dwFlags |= DMUS_PMSGF_TOOL_QUEUE;
        pPMsg->dwFlags &= ~( DMUS_PMSGF_TOOL_ATTIME | DMUS_PMSGF_TOOL_IMMEDIATE );
        return DMUS_S_REQUEUE;
    }

    switch( pPMsg->dwType )
    {
    case DMUS_PMSGT_NOTE:
        {
            hr = PackNote(  pPMsg, pPMsg->rtTime );
        }
        break;
    case DMUS_PMSGT_CURVE:
        {
            hr = PackCurve( pPMsg, pPMsg->rtTime );
        }
        break;
    case DMUS_PMSGT_SYSEX:
        {
            hr = PackSysEx( pPMsg, pPMsg->rtTime );
        }
        break;
    case DMUS_PMSGT_MIDI:
        {
            hr = PackMidi( pPMsg, pPMsg->rtTime );
        }
        break;
    case DMUS_PMSGT_PATCH:
        {
            hr = PackPatch( pPMsg, pPMsg->rtTime );
        }
        break;
    case DMUS_PMSGT_CHANNEL_PRIORITY:
        {
            DMUS_CHANNEL_PRIORITY_PMSG* pPriPMsg = (DMUS_CHANNEL_PRIORITY_PMSG*)pPMsg;
            DWORD dwPortTableIndex, dwGroup, dwMChannel;
            IDirectMusicPort* pPort;

            hr = DMUS_S_FREE;
            if( SUCCEEDED( PChannelIndex( pPriPMsg->dwPChannel, &dwPortTableIndex, &dwGroup,
                &dwMChannel )))
            {
                EnterCriticalSection(&m_PChannelInfoCrSec);
                if( dwPortTableIndex <= m_dwNumPorts )
                {
                    pPort = m_pPortTable[dwPortTableIndex].pPort;
                    if( pPort )
                    {
                        pPort->SetChannelPriority( dwGroup, dwMChannel,
                            pPriPMsg->dwChannelPriority );
                    }
                }
                LeaveCriticalSection(&m_PChannelInfoCrSec);
            }
        }
        break;
    case DMUS_PMSGT_WAVE:
        {
            hr = PackWave( pPMsg, pPMsg->rtTime );
        }
    default:
        break;
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE CPerformance::Flush(
    IDirectMusicPerformance* pPerf, // @parm The Performance pointer.
     DMUS_PMSG* pPMsg,          // @parm The event to flush.
     REFERENCE_TIME rtTime          // @parm The time at which to flush.
    )
{
    V_INAME(IDirectMusicTool::Flush);
    V_INTERFACE(pPerf);
    V_BUFPTR_WRITE(pPMsg,sizeof(DMUS_PMSG));

    HRESULT hr = S_OK;

    ASSERT( pPerf == this );
    switch( pPMsg->dwType )
    {
    case DMUS_PMSGT_NOTE:
        {
            DMUS_NOTE_PMSG* pNote = (DMUS_NOTE_PMSG*)pPMsg;
            if( !(pNote->bFlags & DMUS_NOTEF_NOTEON) )
            {
                PackNote( pPMsg, rtTime );
            }
        }
        break;
    case DMUS_PMSGT_CURVE:
        {
            DMUS_CURVE_PMSG* pCurve = (DMUS_CURVE_PMSG*)pPMsg;
            if( pCurve->bFlags & DMUS_CURVE_RESET )
            {
                PackCurve( pPMsg, rtTime );
            }
        }
        break;
    case DMUS_PMSGT_WAVE:
        {
            DMUS_WAVE_PMSG* pWave = (DMUS_WAVE_PMSG*)pPMsg;
            if (pWave->bFlags & DMUS_WAVEF_OFF)
            {
                PackWave( pPMsg, rtTime );
            }
        }
    default:
        break;
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE CPerformance::GetMsgDeliveryType(
    DWORD* pdwDeliveryType) // @parm Should return either DMUS_PMSGF_TOOL_IMMEDIATE, DMUS_PMSGF_TOOL_QUEUE, or DMUS_PMSGF_TOOL_ATTIME.
                    // An illegal return value will be treated as DMUS_PMSGF_TOOL_IMMEDIATE by the <i IDirectMusicGraph>.
{
    V_INAME(IDirectMusicTool::GetMsgDeliveryType);
    V_PTR_WRITE(pdwDeliveryType,DWORD);

    *pdwDeliveryType = DMUS_PMSGF_TOOL_IMMEDIATE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CPerformance::GetMediaTypeArraySize(
    DWORD* pdwNumElements) // @parm Returns the number of media types, with 0 meaning all.
{
    V_INAME(IDirectMusicTool::GetMediaTypeArraySize);
    V_PTR_WRITE(pdwNumElements,DWORD);

    *pdwNumElements = 0;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CPerformance::GetMediaTypes(
    DWORD** padwMediaTypes, // @parm This should be a DWORD array of size <p dwNumElements>.
                            // Upon return, the elements will be filled with the media types
                            // this Tool supports.
    DWORD dwNumElements)    // @parm Contains the number of elements, i.e. the size, of the
                            // array <p padwMediaTypes>. <p dwNumElements> should be equal
                            // to the number returned in
                            // <om IDirectMusicTool.GetMediaTypeArraySize>. If dwNumElements
                            // is less than this number, this method can't return all of the
                            // message types that are supported. If it is greater than this
                            // number, the element fields in the array will be set to zero.
{
    return E_NOTIMPL;
}

// IDirectMusicGraph
HRESULT STDMETHODCALLTYPE CPerformance::Shutdown()
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CPerformance::InsertTool(
    IDirectMusicTool *pTool,
    DWORD *pdwPChannels,
    DWORD cPChannels,
    LONG lIndex)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CPerformance::GetTool(
    DWORD dwIndex,
    IDirectMusicTool** ppTool)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CPerformance::RemoveTool(
    IDirectMusicTool* pTool)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CPerformance::StampPMsg( DMUS_PMSG* pPMsg )
{
    V_INAME(IDirectMusicGraph::StampPMsg);
    if( m_dwVersion < 8)
    {
        V_PTR_WRITE(pPMsg,sizeof(DMUS_PMSG));
    }
    else
    {
#ifdef DBG
        V_PTR_WRITE(pPMsg,sizeof(DMUS_PMSG));
#else
        if (!pPMsg)
        {
            return E_POINTER;
        }
#endif
    }

    EnterCriticalSection(&m_MainCrSec);
    if( m_pGraph && ( S_OK == m_pGraph->StampPMsg( pPMsg )))
    {
        if (pPMsg->pGraph != this)
        {
            if( pPMsg->pGraph )
            {
                pPMsg->pGraph->Release();
                pPMsg->pGraph = NULL;
            }
            pPMsg->pGraph = this;
            pPMsg->pGraph->AddRef();
        }
        LeaveCriticalSection(&m_MainCrSec);
        return S_OK;
    }
    LeaveCriticalSection(&m_MainCrSec);
    if( pPMsg->pGraph )
    {
        pPMsg->pGraph->Release();
        pPMsg->pGraph = NULL;
    }
    if( pPMsg->pTool )
    {
        pPMsg->pTool->Release();
        pPMsg->pTool = NULL;
    }

    //otherwise there is no graph: set it to the internal Performance Tool
    pPMsg->dwFlags &= ~(DMUS_PMSGF_TOOL_IMMEDIATE | DMUS_PMSGF_TOOL_QUEUE | DMUS_PMSGF_TOOL_ATTIME);
    pPMsg->dwFlags |= DMUS_PMSGF_TOOL_QUEUE;
    pPMsg->pTool = this;
    pPMsg->pTool->AddRef();
    return S_OK;
}

// default scale is C Major
const DWORD DEFAULT_SCALE_PATTERN = 0xab5ab5;

inline DWORD BitCount(DWORD dwPattern)

{
    DWORD dwCount = 0;

    while (dwPattern)
    {
        dwPattern &= (dwPattern - 1);
        dwCount++;
    }

    return dwCount;
}

inline bool InScale(BYTE bMIDI, BYTE bRoot, DWORD dwScale)
{
    TraceI(3, "note: %d root: %d scale: %x\n", bMIDI, bRoot, dwScale);
    // shift the note by the scale root, and put it in a one-octave range
    bMIDI = ((bMIDI + 12) - (bRoot % 12)) % 12;
     // merge two octaves of scale into one
    dwScale = (dwScale & 0x0fff) | ((dwScale >> 12) & 0x0fff);
    // note n is in scale if there's a bit in position n
    TraceI(3, "shifted note: %d shifted scale: %x\n", bMIDI, dwScale);
    return ((1 << bMIDI) & dwScale) ? true : false;
}

inline DWORD CleanupScale(DWORD dwPattern)

//  Force scale to be exactly two octaves

{
    dwPattern &= 0x0FFF;            // Clear upper octave.
    dwPattern |= (dwPattern << 12); // Copy lower octave to top.
    return dwPattern;
}

inline DWORD PatternMatch(DWORD dwA, DWORD dwB)

{
    DWORD dwHit = 0;
    DWORD dwIndex = 0;
    for (;dwIndex < 24; dwIndex++)
    {
        if ((dwA & (1 << dwIndex)) == (dwB & (1 << dwIndex)))
        {
            dwHit++;
        }
    }
    return dwHit;
}

static DWORD dwFallbackScales[12] =
{
    0xab5ab5,0x6ad6ad,
    0x5ab5ab,0xad5ad5,
    0x6b56b5,0x5ad5ad,
    0x56b56b,0xd5ad5a,
    0xb56b56,0xd6ad6a,
    0xb5ab5a,0xad6ad6,
};

inline DWORD FixScale(DWORD dwScale)

{
    if (BitCount(dwScale & 0xFFF) > 4)
    {
        return dwScale;
    }
    DWORD dwBest = 0;
    DWORD dwBestPattern = DEFAULT_SCALE_PATTERN;
    DWORD dwX;
    for (dwX = 0;dwX < 12; dwX++)
    {
        DWORD dwTest = PatternMatch(dwScale,dwFallbackScales[dwX]);
        if (dwTest > dwBest)
        {
            dwBestPattern = dwFallbackScales[dwX];
            dwBest = dwTest;
        }
    }
    return dwBestPattern;
}

inline DWORD ThreeOctave(DWORD dwScale)
{
    DWORD dwResult = dwScale;
     // don't change third octave if there's something there
    if ( !(0xFFF000000 & dwScale) )
    {
        // copy second octave to third octave
        dwResult |= (dwScale & 0xFFF000) << 12;
    }
    return dwResult;
}

inline DWORD AddRootToScale(BYTE bScaleRoot, DWORD dwScalePattern)

{
    dwScalePattern = CleanupScale(dwScalePattern);
    dwScalePattern >>= (12 - (bScaleRoot % 12));
    dwScalePattern = CleanupScale(dwScalePattern);
    return dwScalePattern;
}

inline DWORD SubtractRootFromScale(BYTE bScaleRoot, DWORD dwScalePattern)

{
    dwScalePattern = CleanupScale(dwScalePattern);
    dwScalePattern >>= (bScaleRoot % 12);
    dwScalePattern = CleanupScale(dwScalePattern);
    return dwScalePattern;
}

static DWORD ChordFromScale(BYTE bRoot, DWORD dwScalePattern)

{
    DWORD dwChordPattern = CleanupScale(dwScalePattern >> (bRoot % 12));
    DWORD dwX;
    DWORD dwBitCount = 0;
    for (dwX = 0; dwX < 24; dwX++)
    {
        DWORD dwBit = 1 << dwX;
        if (dwChordPattern & dwBit)
        {
            if ((dwBitCount & 1) || (dwBitCount > 7))
            {
                dwChordPattern &= ~dwBit;
            }
            dwBitCount++;
        }
    }
    return dwChordPattern;
}

static DWORD InvertChord(BYTE bKey, BYTE bChordRoot, DWORD dwChordPattern, bool& rfBelowRoot)

{
    // rotate the chord by the difference between the key and chord root
    rfBelowRoot = false;
    bKey %= 12;
    bChordRoot %= 12;
    if (bKey < bChordRoot) bKey += 12;
    BYTE bRotate = bKey - bChordRoot;
    // first check if the whole chord fits into one octave
    if ( !(dwChordPattern & 0xFFF000) )
    {
        dwChordPattern = ThreeOctave(CleanupScale(dwChordPattern));
        dwChordPattern >>= bRotate;
        dwChordPattern &= 0xFFF;
        if (!(dwChordPattern & 0x7) && ((dwChordPattern & 0xc00)) ||
            !(dwChordPattern & 0x3) && ((dwChordPattern & 0x800)))
        {
            dwChordPattern |= (dwChordPattern << 12);
            dwChordPattern &= 0x3FFC00;
            rfBelowRoot = true;
        }
    }
    else
    {
        dwChordPattern &= 0xFFFFFF; // make sure there are only notes in the two-octave range
        // do a circular shift in the closest direction
        BYTE bRotate2 = (bChordRoot + 12) - bKey;
        if (bRotate <= bRotate2)
        {
            dwChordPattern = (dwChordPattern << (24 - bRotate)) | (dwChordPattern >> bRotate);
        }
        else
        {
            dwChordPattern = (dwChordPattern >> (24 - bRotate2)) | (dwChordPattern << bRotate2);
        }
        dwChordPattern &= 0xFFFFFF;
        if (!(dwChordPattern & 0x7) &&
            (!(dwChordPattern & 0x7000) && ((dwChordPattern & 0xc00000)) ||
             !(dwChordPattern & 0x3000) && ((dwChordPattern & 0x800000)) ||
             !(dwChordPattern & 0x1000) && ((dwChordPattern & 0x1000000)) ||
             !(dwChordPattern & 0x7) && (dwChordPattern & 0x7000) && ((dwChordPattern & 0xc00000))) )
        {
            dwChordPattern = (dwChordPattern << 12) | (dwChordPattern >> 12);
            dwChordPattern &= 0xFFFFFF;
        }
        if (!(dwChordPattern & 0x7) && ((dwChordPattern & 0xc00)) ||
            !(dwChordPattern & 0x3) && ((dwChordPattern & 0x800)) ||
            !(dwChordPattern & 0x1) && ((dwChordPattern & 0x1000)) )
        {
            // put everything up to the G in the first octave two octaves up;
            // put G# and A one octave up
            dwChordPattern |= (((dwChordPattern & 0xFF) << 24) | ((dwChordPattern & 0x300) << 12));
            // get rid of everything below A# in the first octave
            dwChordPattern &= 0xFFFFFC00;
            // If there are no notes lower than C2, shift everything back down an octave
            if (!(dwChordPattern & 0xFFF))
            {
                dwChordPattern >>= 12;
            }
            else
            {
                rfBelowRoot = true;
            }
        }
    }
    return dwChordPattern;

}

/*  This is SuperJAM! code */

static unsigned char OldMusicValueToNote(

unsigned short value,   // Music value to convert.
char scalevalue,        // Scale value if chord failes.
long keypattern,        // Description of key as interval pattern.
char keyroot,           // Root note of key.
long chordpattern,      // Description of chord as interval pattern.
char chordroot)         // Root note of chord.

{
    unsigned char   result ;
    char            octpart   = (char)(value >> 12) ;
    char            chordpart = (char)((value >> 8) & 0xF) ;
    char            keypart   = (char)((value >> 4) & 0xF) ;
    char            accpart   = (char)(value & 0xF) ;

    result  = unsigned char(12 * octpart) ;
    result += chordroot ;

    if( accpart > 8 )
        accpart -= 16 ;

    for( ;  chordpattern ;  result++ ) {
        if( chordpattern & 1L ) {
            if( !chordpart )
                break ;
            chordpart-- ;
        }
        chordpattern = chordpattern >> 1L ;
        if( !chordpattern ) {
            if( !scalevalue )
                return( 0 ) ;
            result  = unsigned char(12 * octpart) ;
            result += chordroot ;
            keypart = char(scalevalue >> 4) ;
            accpart = char(scalevalue & 0x0F) ;
            break ;
        }
    }

    if( keypart ) {
        keypattern = CleanupScale(keypattern) ;
        keypattern  = keypattern >> (LONG)((result - keyroot) % 12) ;
        for( ;  keypattern ;  result++ ) {
            if( keypattern & 1L ) {
                if( !keypart )
                    break ;
                keypart-- ;
            }
            keypattern = keypattern >> 1L ;
        }
    }

    result += unsigned char(accpart) ;
    return( result ) ;

}


/*  This is SuperJAM! code */

static unsigned short OldNoteToMusicValue(

unsigned char note,     // MIDI note to convert.
long keypattern,        // Description of key as interval pattern.
char keyroot,           // Root note of key.
long chordpattern,      // Description of chord as interval pattern.
char chordroot)         // Root note of chord.

{
    unsigned char   octpart = 0 ;
    unsigned char   chordpart = 0;
    unsigned char   keypart = (BYTE)-1 ;
    unsigned char   accpart = 0 ;
    unsigned char   scan, test, base, last ;    // was char
    long            pattern ;
    short           testa, testb ;


    scan = chordroot ;

    // If we're trying to play a note below the bottom of our chord, forget it
    if( note < scan)
    {
        return 0;
    }

    while( scan < (note - 24) )
    {
        scan += 12 ;
        octpart++ ;
    }

    base = last = scan ;

    for( ;  base<=note ;  base+=12 )
    {
        chordpart = (unsigned char)-1 ;
        pattern   = chordpattern ;
        scan      = last = base ;
        if( scan == note )
        {
            accpart = 0;
            while (!(pattern & 1) && pattern)
            {
                accpart--;
                pattern >>= 1;
            }
            return( (unsigned short) (octpart << 12) + (accpart & 0xF)) ;           // if octave, return.
        }
        for( ;  pattern ;  pattern=pattern >> 1 )
        {
            if( pattern & 1 )                   // chord interval?
            {
                if( scan == note )              // note in chord?
                {
                    chordpart++ ;
                    return((unsigned short) ((octpart << 12) | (chordpart << 8))) ; // yes, return.
                }
                else if (scan > note)           // above note?
                {
                    test = scan ;
                    break ;                     // go on to key.
                }
                chordpart++ ;
                last = scan ;
            }
            scan++ ;
        }
        if( !pattern )                          // end of chord.
        {
            test = unsigned char(base + 12) ;                  // set to next note.
        }
        octpart++ ;
        if( test > note )
        {
            break ;                             // above our note?
        }
    }

    octpart-- ;

//  To get here, the note is not in the chord.  Scan should show the last
//  note in the chord.  octpart and chordpart have their final values.
//  Now, increment up the key to find the match.

    scan        = last ;
    pattern     = CleanupScale(keypattern);
    pattern     = pattern >> ((scan - keyroot) % 12) ;

    for( ;  pattern ;  pattern=pattern >> 1 )
    {
        if( 1 & pattern )
        {
            keypart++ ;
            accpart = 0 ;
        }
        else
        {
            accpart++ ;
        }
        if( scan == note )
            break ;
        scan++;
    }

    if( accpart && keypart )
    {
        testa = short((octpart << 12) + (chordpart << 8) + (keypart << 4) + accpart + 1);
        testb = short((octpart << 12) + ((chordpart + 1) << 8) + 0);
        testa = OldMusicValueToNote( testa, 0, keypattern, keyroot,
                                     chordpattern, chordroot );
        testb = OldMusicValueToNote( testb, 0, keypattern, keyroot,
                                     chordpattern, chordroot );
        if( testa == testb )
        {
            chordpart++ ;
            keypart = 0 ;
            accpart = -1 ;
        }
    }

    // If the conversion didn't find an exact match, fudge accpart to make it work
    testa = short((octpart << 12) + (chordpart << 8) + (keypart << 4) + (accpart & 0xF));
    testa = OldMusicValueToNote( testa, 0, keypattern, keyroot,
                                 chordpattern, chordroot );

    if( testa != note )
    {
        accpart += note - testa;
    }

    return unsigned short((octpart << 12) + (chordpart << 8) + (keypart << 4) + (accpart & 0xF));

}

inline short MusicValueOctave(WORD wMusicValue)
{ return short((wMusicValue >> 12) & 0xf) * 12; }

inline short MusicValueAccidentals(WORD wMusicValue)
{
    short acc = short(wMusicValue & 0xf);
    return (acc > 8) ? acc - 16 : acc;
}

inline short BitsInChord(DWORD dwChordPattern)
{

    for (short nResult = 0; dwChordPattern != 0; dwChordPattern >>= 1)
        if (dwChordPattern & 1) nResult++;
    return nResult;
}

#define S_OVER_CHORD    0x1000      // Success code to indicate the musicval could not be
                                    // converted because the note is above the top of the chord.

short MusicValueIntervals(WORD wMusicValue, BYTE bPlayModes, DMUS_SUBCHORD *pSubChord, BYTE bRoot)
{
    if ((bPlayModes & DMUS_PLAYMODE_CHORD_INTERVALS) || (bPlayModes & DMUS_PLAYMODE_SCALE_INTERVALS))
    {
        DWORD dwDefaultScale =
            (pSubChord->dwScalePattern) ? (pSubChord->dwScalePattern) : DEFAULT_SCALE_PATTERN;
        dwDefaultScale = AddRootToScale(pSubChord->bScaleRoot, dwDefaultScale);
        dwDefaultScale = ThreeOctave(FixScale(dwDefaultScale));
        DWORD dwChordPattern = pSubChord->dwChordPattern;
        if (!dwChordPattern) dwChordPattern = 1;
        bool fBelowRoot = false;
        if ((bPlayModes & DMUS_PLAYMODE_KEY_ROOT) && bPlayModes != DMUS_PLAYMODE_PEDALPOINT)
        {
            dwChordPattern = InvertChord(bRoot, pSubChord->bChordRoot, dwChordPattern, fBelowRoot);
        }
        const short nChordPosition = (wMusicValue >> 8) & 0xf;
//      const short nScalePosition = (wMusicValue >> 4) & 0xf;
        // ensure that scale position is < 8
        const short nScalePosition = (wMusicValue >> 4) & 0x7;
        const short nChordBits = BitsInChord(dwChordPattern);
        short nSemitones = 0;
        // If the chord doesn't have a root or second, but does have a seventh, it's been inverted and
        // we need to start below the root
        short nTransposetones;
        DWORD dwPattern;
        short nPosition;
        BYTE bOctRoot = bRoot % 12; // root in one octave
        // if using chord intervals and the note is in the chord
        if ((bPlayModes & DMUS_PLAYMODE_CHORD_INTERVALS) &&
            !nScalePosition &&
            (nChordPosition < nChordBits) )
        {
            nTransposetones = bRoot + MusicValueAccidentals(wMusicValue);
            dwPattern = dwChordPattern;
            nPosition = nChordPosition;
        }
        // if using chord intervals and note is inside the chord (including 6ths)
        else if ((bPlayModes & DMUS_PLAYMODE_CHORD_INTERVALS) &&
                 (nChordPosition < nChordBits) )
        {
            dwPattern = dwChordPattern;
            nPosition = nChordPosition;
            if (dwPattern)
            {
                // skip to the first note in the chord
                while (!(dwPattern & 1))
                {
                    dwPattern >>= 1;
                    nSemitones++;
                }
            }
            if (nPosition > 0)
            {
                do
                {
                    dwPattern >>= 1; // this will ignore the first note in the chord
                    nSemitones++;
                    if (dwPattern & 1)
                    {
                        nPosition--;
                    }
                    if (!dwPattern)
                    {
                        nSemitones += nPosition;
//                      assert (0); // This shouldn't happen...
                        break;
                    }
                } while (nPosition > 0);
            }

            nSemitones += bOctRoot;
            nTransposetones = MusicValueAccidentals(wMusicValue) + bRoot - bOctRoot;
            dwPattern = dwDefaultScale >> (nSemitones % 12);  // start comparing partway through the pattern
            nPosition = nScalePosition;
        }
        // if using scale intervals
        else if (bPlayModes & DMUS_PLAYMODE_SCALE_INTERVALS)
        {
            fBelowRoot = false; // forget about chord inversions
            nSemitones = bOctRoot;
            nTransposetones = MusicValueAccidentals(wMusicValue) + bRoot - bOctRoot;
            dwPattern = dwDefaultScale >> bOctRoot;  // start comparing partway through the pattern
            nPosition = nChordPosition * 2 + nScalePosition;
        }
        else
        {
            return S_OVER_CHORD;  //
        }
        nPosition++; // Now nPosition corresponds to actual scale positions
        for (; nPosition > 0; dwPattern >>= 1)
        {
            nSemitones++;
            if (dwPattern & 1)
            {
                nPosition--;
            }
            if (!dwPattern)
            {
                nSemitones += nPosition;
//              assert (0); // This shouldn't happen...
                break;
            }
        }
        nSemitones--; // the loop counts one too many semitones...
        if (fBelowRoot)
        {
            nSemitones -=12;
        }
        return nSemitones + nTransposetones;
    }
    else
    {
        // should be impossible for 2.5 format
        return bRoot + wMusicValue;
    }
}

inline short MusicValueChord(WORD wMusicValue, BYTE bPlayModes, DMUS_SUBCHORD *pSubChord, BYTE bKey)
{
    // first, get the root for transposition.
    BYTE bRoot = 0;
    if (bPlayModes & DMUS_PLAYMODE_CHORD_ROOT)
    {
        bRoot = pSubChord->bChordRoot;
    }
    else if (bPlayModes & DMUS_PLAYMODE_KEY_ROOT)
        bRoot = bKey;
    // Next, get an interval and combine it with the root.
    return MusicValueIntervals(wMusicValue, bPlayModes, pSubChord, bRoot);
}

inline short MusicValueConvert(WORD wMV, BYTE bPlayModes, DMUS_SUBCHORD *pSubChord, BYTE bKey)
{
    short nResult = 0;
    // First, make sure the octave is not negative.
    short nOffset = 0;
    while (wMV >= 0xE000)
    {
        wMV += 0x1000;
        nOffset -= 12;
    }

    // If the music value has a negative scale offset, convert to an equivalent
    // music value with a positive offset (up an octave) and shift the whole thing
    // down an octave
    WORD wTemp = (wMV & 0x00f0) + 0x0070;
    if (wTemp & 0x0f00)
    {
        wMV = (wMV & 0xff0f) | (wTemp & 0x00f0);
        nOffset = -12;
    }

    short nChordValue = MusicValueChord(wMV, bPlayModes, pSubChord, bKey);
    if (nChordValue != S_OVER_CHORD)
    {
        nChordValue += nOffset;
        // If the chord root is < 12, take the result down an octave.
        if ((bPlayModes & DMUS_PLAYMODE_CHORD_ROOT))
            nResult = MusicValueOctave(wMV) + nChordValue - 12;
        else
            nResult = MusicValueOctave(wMV) + nChordValue;
    }
    else
        nResult = S_OVER_CHORD;
    return nResult;
}

HRESULT STDMETHODCALLTYPE CPerformance::MIDIToMusic(
                BYTE bMIDIValue,
                DMUS_CHORD_KEY* pChord,
                BYTE bPlayMode,
                BYTE bChordLevel,
                WORD *pwMusicValue
            )

{
    V_INAME(IDirectMusicPerformance::MIDIToMusic);
    V_BUFPTR_READ( pChord, sizeof(DMUS_CHORD_KEY) );
    V_PTR_WRITE(pwMusicValue,WORD);

    long lMusicValue;
    HRESULT hr = S_OK;
#ifdef DBG
    long lMIDIInTraceValue = bMIDIValue;
#endif

    if ((bPlayMode & DMUS_PLAYMODE_NONE ) || (bMIDIValue & 0x80))
    {
        Trace(1,"Error: MIDIToMusic conversion failed either because there is no playmode or MIDI value %ld is out of range.\n",(long)bMIDIValue);
        return E_INVALIDARG;
    }
    else if( bPlayMode == DMUS_PLAYMODE_FIXED )
    {
        *pwMusicValue = bMIDIValue & 0x7F;
        return S_OK;
    }
    else if (bPlayMode == DMUS_PLAYMODE_FIXEDTOKEY) // fixed to key
    {
        lMusicValue = bMIDIValue - pChord->bKey;
        while (lMusicValue < 0)
        {
            lMusicValue += 12;
            Trace(2,"Warning: MIDIToMusic had to bump the music value up an octave for DMUS_PLAYMODE_FIXEDTOKEY note.\n");
            hr = DMUS_S_UP_OCTAVE;
        }
        while (lMusicValue > 127)
        {
            lMusicValue -= 12;
            Trace(2,"Warning: MIDIToMusic had to bump the music value up an octave for DMUS_PLAYMODE_FIXEDTOKEY note.\n");
            hr = DMUS_S_DOWN_OCTAVE;
        }
        *pwMusicValue = (WORD) lMusicValue;
        return hr;
    }
    else
    {
        DMUS_SUBCHORD *pSubChord;
        DWORD dwLevel = 1 << bChordLevel;
        bool fFoundLevel = false;
        for (int i = 0; i < pChord->bSubChordCount; i++)
        {
            if (dwLevel & pChord->SubChordList[i].dwLevels)
            {
                pSubChord = &pChord->SubChordList[i];
                fFoundLevel = true;
                break;
            }
        }
        if (!fFoundLevel) // No luck? Use first chord.
        {
            pSubChord = &pChord->SubChordList[0];
        }
        if (bPlayMode == DMUS_PLAYMODE_FIXEDTOCHORD) // fixed to chord
        {
            lMusicValue = bMIDIValue - (pSubChord->bChordRoot % 24);
            while (lMusicValue < 0)
            {
                lMusicValue += 12;
                Trace(2,"Warning: MIDIToMusic had to bump the music value up an octave for DMUS_PLAYMODE_FIXEDTOCHORD note.\n");
                hr = DMUS_S_UP_OCTAVE;
            }
            while (lMusicValue > 127)
            {
                lMusicValue -= 12;
                Trace(2,"Warning: MIDIToMusic had to bump the music value down an octave for DMUS_PLAYMODE_FIXEDTOCHORD note.\n");
                hr = DMUS_S_DOWN_OCTAVE;
            }
            *pwMusicValue = (WORD) lMusicValue;
            return hr;
        }
        bool fBelowRoot = false;
        DWORD dwScalePattern = AddRootToScale(pSubChord->bScaleRoot, pSubChord->dwScalePattern);
        DWORD dwChordPattern = pSubChord->dwChordPattern;
        BYTE bKeyRoot = pChord->bKey;
        BYTE bChordRoot = pSubChord->bChordRoot;
        dwScalePattern = FixScale(dwScalePattern);
        bPlayMode &= 0xF;   // We only know about the bottom four flags, at this point.
//        if (bPlayMode == DMUS_PLAYMODE_PEDALPOINT)
        // Do this for any non-fixed key root mode (Pedalpoint, PedalpointChord, PedalpointAlways)
        if (bPlayMode & DMUS_PLAYMODE_KEY_ROOT)
        {
            while (bKeyRoot > bMIDIValue)
            {
                hr = DMUS_S_UP_OCTAVE;
                Trace(2,"Warning: MIDIToMusic had to bump the music value up an octave for DMUS_PLAYMODE_KEY_ROOT note.\n");
                bMIDIValue += 12;
            }
            dwScalePattern = SubtractRootFromScale(bKeyRoot,dwScalePattern);
            if (bPlayMode == DMUS_PLAYMODE_PEDALPOINT || !dwChordPattern)
            {
                bChordRoot = bKeyRoot;
                dwChordPattern = ChordFromScale(0,dwScalePattern);
            }
            else
            {
                dwChordPattern = InvertChord(bKeyRoot, bChordRoot, dwChordPattern, fBelowRoot);
                BYTE bNewChordRoot = 0;
                if (dwChordPattern)
                {
                    for (; !(dwChordPattern & (1 << bNewChordRoot)); bNewChordRoot++);
                }
                bChordRoot = bNewChordRoot + bKeyRoot;
                dwChordPattern >>= bNewChordRoot;
            }
        }
        else if (bPlayMode == DMUS_PLAYMODE_MELODIC)
        {
            bKeyRoot = 0;
            dwChordPattern = ChordFromScale(bChordRoot,dwScalePattern);
        }
        else
        {
            bKeyRoot = 0;
            if (!dwChordPattern)
            {
                dwChordPattern = ChordFromScale(bChordRoot,dwScalePattern);
            }
        }
        BOOL fDropOctave = FALSE;
        if (bMIDIValue < 24)
        {
            fDropOctave = TRUE;
            bMIDIValue += 24;
        }
        WORD wNewMusicValue = OldNoteToMusicValue( bMIDIValue,
            dwScalePattern,
            bKeyRoot,
            dwChordPattern,
            bChordRoot );
        if (fDropOctave)
        {
            wNewMusicValue -= 0x2000;
            bMIDIValue -= 24;
        }

        // If DMUS_PLAYMODE_CHORD_ROOT is set, take the result up an octave.
        // // also take the result up for the new pedalpoint chord modes.
        if( (bPlayMode & DMUS_PLAYMODE_CHORD_ROOT)  ||
            fBelowRoot)
            //((bPlayMode & DMUS_PLAYMODE_KEY_ROOT) && bPlayMode != DMUS_PLAYMODE_PEDALPOINT) )
        {
            wNewMusicValue += 0x1000;
        }
        short nTest =
            MusicValueConvert(wNewMusicValue, bPlayMode,
                pSubChord, pChord->bKey);

        if (nTest == (short) bMIDIValue)
        {
            *pwMusicValue = wNewMusicValue;
        }
        else
        {
            if (nTest == S_OVER_CHORD)
            {
                if (BitCount(pSubChord->dwChordPattern) < 4)
                {
                    DWORD dwOldChordPattern = pSubChord->dwChordPattern;
                    pSubChord->dwChordPattern = ChordFromScale(bChordRoot,dwScalePattern);
                    nTest =
                        MusicValueConvert(wNewMusicValue, bPlayMode,
                            pSubChord, pChord->bKey);
                    pSubChord->dwChordPattern = dwOldChordPattern;
                    if (nTest == (short) bMIDIValue)
                    {
                        *pwMusicValue = wNewMusicValue;
                        return hr;
                    }
                }
            }
            *pwMusicValue = wNewMusicValue;
#ifdef DBG // Put in brackets just in case the compiler is using something different than DBG for turning on Trace.
            Trace(1,"Error: Unable to convert MIDI value %ld to Music value. This usually means the DMUS_CHORD_KEY structure has an invalid chord or scale pattern.\n",
                lMIDIInTraceValue);
#endif
            return DMUS_E_CANNOT_CONVERT;
        }
    }
    return hr;
}


HRESULT STDMETHODCALLTYPE CPerformance::MusicToMIDI(
                WORD wMusicValue,
                DMUS_CHORD_KEY* pChord,
                BYTE bPlayMode,
                BYTE bChordLevel,
                BYTE *pbMIDIValue
            )

{
    V_INAME(IDirectMusicPerformance::MusicToMIDI);
    V_BUFPTR_READ( pChord, sizeof(DMUS_CHORD_KEY) );
    V_PTR_WRITE(pbMIDIValue,BYTE);

    long lReturnVal = wMusicValue;
    HRESULT hr = S_OK;

    if (bPlayMode != DMUS_PLAYMODE_FIXED)
    {
        DMUS_SUBCHORD *pSubChord;
        DWORD dwLevel = 1 << bChordLevel;
        bool fFoundLevel = false;
        for (int i = 0; i < pChord->bSubChordCount; i++)
        {
            if (dwLevel & pChord->SubChordList[i].dwLevels)
            {
                pSubChord = &pChord->SubChordList[i];
                fFoundLevel = true;
                break;
            }
        }
        if (!fFoundLevel) // No luck? Use first chord.
        {
            pSubChord = &pChord->SubChordList[0];
        }
        if (bPlayMode & DMUS_PLAYMODE_NONE )
        {
            *pbMIDIValue = 0;
            Trace(1,"Error: Unable to convert Music value to MIDI because the playmode is DMUS_PLAYMODE_NONE.\n");
            return E_INVALIDARG;
        }
        if (bPlayMode == DMUS_PLAYMODE_FIXEDTOCHORD) // fixed to chord
        {
            lReturnVal += (pSubChord->bChordRoot % 24);
        }
        else if (bPlayMode == DMUS_PLAYMODE_FIXEDTOKEY) // fixed to scale
        {
            lReturnVal += pChord->bKey;
        }
        else
        {
            lReturnVal =
                MusicValueConvert((WORD)lReturnVal, bPlayMode, pSubChord, pChord->bKey);
        }
    }
    if (lReturnVal == S_OVER_CHORD)
    {
        Trace(5,"Warning: MIDIToMusic unable to convert because note out of chord range.\n");
        return DMUS_S_OVER_CHORD;
    }
    while (lReturnVal < 0)
    {
        lReturnVal += 12;
        Trace(2,"Warning: MusicToMIDI had to bump the music value up an octave to stay in MIDI range.\n");
        hr = DMUS_S_UP_OCTAVE;
    }
    while (lReturnVal > 127)
    {
        lReturnVal -= 12;
        Trace(2,"Warning: MusicToMIDI had to bump the music value down an octave to stay in MIDI range.\n");
        hr = DMUS_S_DOWN_OCTAVE;
    }
    *pbMIDIValue = (BYTE) lReturnVal;
    return hr;
}

// returns:
// S_OK if the note should be invalidated (any other return code will not invalidate)
// S_FALSE if processing otherwise succeeded, but the note should not be invalidated
// E_OUTOFMEMORY if allocation of a new note failed
HRESULT CPerformance::GetChordNotificationStatus(DMUS_NOTE_PMSG* pNote,
                                                 //IDirectMusicSegment* pSegment,
                                                 DWORD dwTrackGroup,
                                                 REFERENCE_TIME rtTime,
                                                 DMUS_PMSG** ppNew)
{
    HRESULT hr = S_FALSE; // default: succeed, but don't invalidate the note

    DMUS_CHORD_PARAM CurrentChord;
    MUSIC_TIME mtTime;
    ReferenceToMusicTime(rtTime, &mtTime);

    if (pNote->bFlags & (DMUS_NOTEF_NOINVALIDATE_INSCALE | DMUS_NOTEF_NOINVALIDATE_INCHORD))
    {
        // If the note is inconsistent with the current scale/chord, invalidate it
        if (SUCCEEDED(GetParam(GUID_ChordParam, dwTrackGroup, DMUS_SEG_ANYTRACK,
                                mtTime, NULL, (void*) &CurrentChord)))
        {
            if (CurrentChord.bSubChordCount > 0)
            {
                BYTE bRoot = CurrentChord.SubChordList[0].bChordRoot;
                DWORD dwScale = CurrentChord.SubChordList[0].dwScalePattern;
                if (pNote->bFlags & DMUS_NOTEF_NOINVALIDATE_INCHORD)
                {
                    dwScale = CurrentChord.SubChordList[0].dwChordPattern;
                }
                else
                {
                    dwScale = FixScale(SubtractRootFromScale(bRoot, dwScale));
                }
                if (!InScale(pNote->bMidiValue, bRoot, dwScale))
                {
                    hr = S_OK;
                }
            }
        }
    }
    else if (pNote->bFlags & DMUS_NOTEF_REGENERATE)
    {
        // this always causes an invalidation, and in addition generates a new note event,
        // based on the Music Value of the current one, that starts at rtTime
        // and continues until pNote->mtTime + pNote->Duration
        // EXCEPTION: the newly generated note is the same as the currently playing one
        if (SUCCEEDED(GetParam(GUID_ChordParam, dwTrackGroup, DMUS_SEG_ANYTRACK,
                                mtTime, NULL, (void*) &CurrentChord)))
        {
            BYTE bNewMidiValue = 0;
            if (SUCCEEDED(MusicToMIDI(pNote->wMusicValue, &CurrentChord, pNote->bPlayModeFlags,
                                        pNote->bSubChordLevel, &bNewMidiValue)) &&
                bNewMidiValue != pNote->bMidiValue)
            {
                MUSIC_TIME mtDuration = (pNote->bFlags & DMUS_NOTEF_NOTEON) ? pNote->mtDuration - (mtTime - pNote->mtTime) : pNote->mtTime - mtTime;
                // Make any duration < 1 be 0; this will cause the note not to
                // sound.  Can happen if the note's logical time is well before
                // its physical time.
                if( mtDuration < 1 ) mtDuration = 0;
                DMUS_PMSG* pNewPMsg = NULL;
                if( SUCCEEDED( AllocPMsg( sizeof(DMUS_NOTE_PMSG), &pNewPMsg )))
                {
                    DMUS_NOTE_PMSG* pNewNote = (DMUS_NOTE_PMSG*)pNewPMsg;
                    // start by copying the current note into the new one
                    pNewNote->dwFlags = pNote->dwFlags;
                    pNewNote->dwPChannel = pNote->dwPChannel;
                    pNewNote->dwVirtualTrackID = pNote->dwVirtualTrackID;
                    pNewNote->pTool = pNote->pTool;
                    if (pNewNote->pTool) pNewNote->pTool->AddRef();
                    pNewNote->pGraph = pNote->pGraph;
                    if (pNewNote->pGraph) pNewNote->pGraph->AddRef();
                    pNewNote->dwType = pNote->dwType;
                    pNewNote->dwVoiceID = pNote->dwVoiceID;
                    pNewNote->dwGroupID = pNote->dwGroupID;
                    pNewNote->punkUser = pNote->punkUser;
                    if (pNewNote->punkUser) pNewNote->punkUser->AddRef();
                    pNewNote->wMusicValue = pNote->wMusicValue;
                    pNewNote->wMeasure = pNote->wMeasure;
                    pNewNote->nOffset = pNote->nOffset;
                    pNewNote->bBeat = pNote->bBeat;
                    pNewNote->bGrid = pNote->bGrid;
                    pNewNote->bVelocity = pNote->bVelocity;
                    pNewNote->bTimeRange = pNote->bTimeRange;
                    pNewNote->bDurRange = pNote->bDurRange;
                    pNewNote->bVelRange = pNote->bVelRange;
                    pNewNote->bPlayModeFlags = pNote->bPlayModeFlags;
                    pNewNote->bSubChordLevel = pNote->bSubChordLevel;
                    pNewNote->cTranspose = pNote->cTranspose;
                    // only things that need to change are flags, MIDI value, start time, and duration
                    pNewNote->mtTime = mtTime;
                    MusicToReferenceTime(pNewNote->mtTime, &pNewNote->rtTime);
                    pNewNote->mtDuration = mtDuration;
                    pNewNote->bMidiValue = bNewMidiValue;
                    pNewNote->bFlags = DMUS_NOTEF_NOTEON | DMUS_NOTEF_REGENERATE;
                    PackNote(pNewPMsg, rtTime + 1); // play the note on
                    *ppNew = pNewPMsg;  // PackNote modifies event to be note-off; queue this
                    // invalidate the current note
                    hr = S_OK;
                }
                else hr = E_OUTOFMEMORY;
            }
        }
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE CPerformance::TimeToRhythm(
                MUSIC_TIME mtTime,
                DMUS_TIMESIGNATURE *pTimeSig,
                WORD *pwMeasure,
                BYTE *pbBeat,
                BYTE *pbGrid,
                short *pnOffset
            )

{
    V_INAME(IDirectMusicPerformance::TimeToRhythm);
    V_BUFPTR_READ( pTimeSig, sizeof(DMUS_TIMESIGNATURE) );
    V_PTR_WRITE(pwMeasure,WORD);
    V_PTR_WRITE(pbBeat,BYTE);
    V_PTR_WRITE(pbGrid,BYTE);
    V_PTR_WRITE(pnOffset,short);

    long lMeasureLength;
    long lBeatLength = DMUS_PPQ;
    long lGridLength;

    if( pTimeSig->bBeat )
    {
        lBeatLength = DMUS_PPQ * 4 / pTimeSig->bBeat;
    }
    lMeasureLength = lBeatLength * pTimeSig->bBeatsPerMeasure;
    if( pTimeSig->wGridsPerBeat )
    {
        lGridLength = lBeatLength / pTimeSig->wGridsPerBeat;
    }
    else
    {
        lGridLength = lBeatLength / 256;
    }
    long lTemp = mtTime - pTimeSig->mtTime;
    *pwMeasure = (WORD)((lTemp / lMeasureLength));
    lTemp = lTemp % lMeasureLength;
    *pbBeat = (BYTE)(lTemp / lBeatLength);
    lTemp = lTemp % lBeatLength;
    *pbGrid = (BYTE)(lTemp / lGridLength);
    *pnOffset = (short)(lTemp % lGridLength);
    if (*pnOffset > (lGridLength >> 1))
    {
        *pnOffset -= (short) lGridLength;
        (*pbGrid)++;
        if (*pbGrid == pTimeSig->wGridsPerBeat)
        {
            *pbGrid = 0;
            (*pbBeat)++;
            if (*pbBeat == pTimeSig->bBeatsPerMeasure)
            {
                *pbBeat = 0;
                (*pwMeasure)++;
            }
        }
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CPerformance::RhythmToTime(
                WORD wMeasure,
                BYTE bBeat,
                BYTE bGrid,
                short nOffset,
                DMUS_TIMESIGNATURE *pTimeSig,
                MUSIC_TIME *pmtTime
            )

{
    V_INAME(IDirectMusicPerformance::RhythmToTime);
    V_BUFPTR_READ( pTimeSig, sizeof(DMUS_TIMESIGNATURE) );
    V_PTR_WRITE(pmtTime,MUSIC_TIME);

    long lMeasureLength;
    long lBeatLength = DMUS_PPQ;
    long lGridLength;

    if( pTimeSig->bBeat )
    {
        lBeatLength = DMUS_PPQ * 4 / pTimeSig->bBeat;
    }
    lMeasureLength = lBeatLength * pTimeSig->bBeatsPerMeasure;
    if( pTimeSig->wGridsPerBeat )
    {
        lGridLength = lBeatLength / pTimeSig->wGridsPerBeat;
    }
    else
    {
        lGridLength = lBeatLength / 256;
    }
    long lTemp = nOffset + pTimeSig->mtTime;
    lTemp += wMeasure * lMeasureLength;
    lTemp += bBeat * lBeatLength;
    lTemp += bGrid * lGridLength;
    *pmtTime = lTemp;
    return S_OK;
}
