//      Copyright (c) 1996-1999 Microsoft Corporation
//      MIDI.cpp
//

#ifdef DMSYNTH_MINIPORT
#include "common.h"
#else
#include "simple.h"
#include <mmsystem.h>
#include "synth.h"
#include "math.h"
#include "debug.h"
#endif
 
CMIDIDataList    CMIDIRecorder::m_sFreeList;
DWORD            CMIDIRecorder::m_sUsageCount = 0;

CMIDIData::CMIDIData() 
{
    m_stTime = 0;
    m_lData = 0;            
}


CMIDIRecorder::CMIDIRecorder()
{
    m_sUsageCount++;
    m_lCurrentData = 0;
    m_stCurrentTime = 0;
}

CMIDIRecorder::~CMIDIRecorder()

{
    ClearMIDI(0x7FFFFFFF);
     m_sUsageCount--;
    // If there are no instances of CMIDIRecorder left, get rid of the free pool.
    if (!m_sUsageCount)
    {
        CMIDIData *pMD;
        while (pMD = m_sFreeList.RemoveHead())
        {
            delete pMD;
        }
    }
}

VREL CMIDIRecorder::m_vrMIDIToVREL[128] = 
{
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

VREL CMIDIRecorder::m_vrMIDIPercentToVREL[128] = 
{
    -9600, -4207, -3605, -3253, -3003, -2809, -2651, -2517, 
    -2401, -2299, -2207, -2124, -2049, -1979, -1915, -1855, 
    -1799, -1746, -1697, -1650, -1605, -1563, -1522, -1484, 
    -1447, -1411, -1377, -1344, -1313, -1282, -1253, -1224, 
    -1197, -1170, -1144, -1119, -1095, -1071, -1048, -1025, 
    -1003, -982, -961, -940, -920, -901, -882, -863, 
    -845, -827, -809, -792, -775, -759, -742, -726, 
    -711, -695, -680, -665, -651, -636, -622, -608, 
    -595, -581, -568, -555, -542, -529, -517, -505, 
    -492, -480, -469, -457, -445, -434, -423, -412, 
    -401, -390, -379, -369, -359, -348, -338, -328, 
    -318, -308, -299, -289, -280, -270, -261, -252, 
    -243, -234, -225, -216, -207, -198, -190, -181, 
    -173, -165, -156, -148, -140, -132, -124, -116, 
    -109, -101, -93, -86, -78, -71, -63, -56, 
    -49, -42, -34, -27, -20, -13, -6, 0 
};

/*void CMIDIRecorder::Init()
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
        fAlreadyDone = TRUE;*/
/*		for (nIndex = 1; nIndex < 128; nIndex++)
		{
			double   flTemp;
			flTemp = nIndex;
			flTemp /= 127.0;
			flTemp = pow(flTemp,4.0);
			flTemp = log10(flTemp);
			flTemp *= 1000.0;
            Trace(0,"%ld, ",(long)flTemp);
            if ((nIndex % 8) == 7)
                Trace(0,"\n");
			m_vrMIDIToVREL[nIndex] = (VREL) flTemp;
		}
        Trace(0,"\n");
		m_vrMIDIToVREL[0] = -9600;
        for (nIndex = 1; nIndex < 128; nIndex++)
        {
            double flTemp;
            flTemp = nIndex;
            flTemp /= 127;
            flTemp *= flTemp;
            flTemp = log10(flTemp);
            flTemp *= 1000.0;
            m_vrMIDIPercentToVREL[nIndex] = (VREL) flTemp;
            Trace(0,"%ld, ",(long)flTemp);
            if ((nIndex % 8) == 7)
                Trace(0,"\n");
        }
        m_vrMIDIPercentToVREL[0] = -9600;*/
    /*}
}*/

BOOL CMIDIRecorder::FlushMIDI(STIME stTime)
{
    CMIDIData *pMD;
    CMIDIData *pLast = NULL;
    for (pMD = m_EventList.GetHead();pMD != NULL;pMD = pMD->GetNext())
    {
        if (pMD->m_stTime >= stTime)
        {
            if (pLast == NULL)
            {
                m_EventList.RemoveAll();
            }
            else
            {
                pLast->SetNext(NULL);
            }
            m_sFreeList.Cat(pMD);
            break;
        }
        pLast = pMD;
    }
    return m_EventList.IsEmpty();
}

BOOL CMIDIRecorder::ClearMIDI(STIME stTime)

{
    CMIDIData *pMD;
    for (;pMD = m_EventList.GetHead();)
    {
        if (pMD->m_stTime < stTime)
        {
            m_EventList.RemoveHead();
            m_stCurrentTime = pMD->m_stTime;
            m_lCurrentData = pMD->m_lData;
            m_sFreeList.AddHead(pMD);
            
        }
        else break;
    }
    return m_EventList.IsEmpty();
}

VREL CMIDIRecorder::VelocityToVolume(WORD nVelocity)

{
    return (m_vrMIDIToVREL[nVelocity]);
}

BOOL CMIDIRecorder::RecordMIDINote(STIME stTime, long lData)

{
    CMIDIData *pMD = m_sFreeList.RemoveHead();
    if (!pMD)
    {
        pMD = new CMIDIData;
    }

	CMIDIData *pScan = m_EventList.GetHead();
	CMIDIData *pNext;
    if (pMD)
    {
        pMD->m_stTime = stTime;
        pMD->m_lData = lData;
		if (pScan == NULL)
		{
			m_EventList.AddHead(pMD);
		}
		else
		{
			if (pScan->m_stTime > stTime)
			{
				m_EventList.AddHead(pMD);
			}
			else
			{
				for (;pScan != NULL; pScan = pNext)
				{
					pNext = pScan->GetNext();
					if (pNext == NULL)
					{
						pScan->SetNext(pMD);
					}
					else
					{
						if (pNext->m_stTime > stTime)
						{
							pMD->SetNext(pNext);
							pScan->SetNext(pMD);
							break;
						}
					}
				}
			}
		}
        return (TRUE);
    }
    return (FALSE);
}

BOOL CMIDIRecorder::RecordMIDI(STIME stTime, long lData)

{
    CMIDIData *pMD = m_sFreeList.RemoveHead();
    if (!pMD)
    {
        pMD = new CMIDIData;
    }

	CMIDIData *pScan = m_EventList.GetHead();
	CMIDIData *pNext;
    if (pMD)
    {
        pMD->m_stTime = stTime;
        pMD->m_lData = lData;
		if (pScan == NULL)
		{
			m_EventList.AddHead(pMD);
		}
		else
		{
			if (pScan->m_stTime > stTime)
			{
				m_EventList.AddHead(pMD);
			}
			else
			{
				for (;pScan != NULL; pScan = pNext)
				{
                    if ((pScan->m_stTime == stTime) && 
                        (pScan->m_lData == lData))
                    {
                        m_sFreeList.AddHead(pMD);
                        break;
                    }
					pNext = pScan->GetNext();
					if (pNext == NULL)
					{
						pScan->SetNext(pMD);
					}
					else
					{
						if (pNext->m_stTime > stTime)
						{
							pMD->SetNext(pNext);
							pScan->SetNext(pMD);
							break;
						}
					}
				}
			}
		}
        return (TRUE);
    }
/*#ifdef DBG
    static gWarnCount = 0;
    if (!gWarnCount)
    {
        Trace(1,"Warning: MIDI Free event pool empty. This can be caused by time stamping problems, too much MIDI data, or too many PChannels.\n");
        gWarnCount = 100;
    }
    gWarnCount--;
#endif*/
    return (FALSE);
}

long CMIDIRecorder::GetData(STIME stTime)

{
    CMIDIData *pMD = m_EventList.GetHead();
    long lData = m_lCurrentData;
    for (;pMD;pMD = pMD->GetNext())
    {
        if (pMD->m_stTime > stTime)
        {
            break;
        }
        lData = pMD->m_lData;
    }
    return (lData);
}

BOOL CNoteIn::RecordNote(STIME stTime, CNote * pNote)

{
	long lData = pNote->m_bPart << 16;
	lData |= pNote->m_bKey << 8;
	lData |= pNote->m_bVelocity;
	return (RecordMIDINote(stTime,lData));
}

BOOL CNoteIn::RecordEvent(STIME stTime, DWORD dwPart, DWORD dwCommand, BYTE bData)

{
	long lData = dwPart;
	lData <<= 8;
	lData |= dwCommand;
	lData <<= 8;
	lData |= bData;
	return (RecordMIDINote(stTime,lData));	
}

BOOL CNoteIn::GetNote(STIME stTime, CNote * pNote)

{
    CMIDIData *pMD = m_EventList.GetHead();
	if (pMD != NULL)
	{
		if (pMD->m_stTime <= stTime)
		{
			pNote->m_stTime = pMD->m_stTime;
			pNote->m_bPart = (BYTE) (pMD->m_lData >> 16);
			pNote->m_bKey = (BYTE) (pMD->m_lData >> 8) & 0xFF;
			pNote->m_bVelocity = (BYTE) pMD->m_lData & 0xFF;
            m_EventList.RemoveHead();
            m_sFreeList.AddHead(pMD);
			return (TRUE);
		}
	}
	return (FALSE);
}

void CNoteIn::FlushMIDI(STIME stTime)

{
    CMIDIData *pMD;
    for (pMD = m_EventList.GetHead();pMD != NULL;pMD = pMD->GetNext())
    {
        if (pMD->m_stTime >= stTime)
        {
            pMD->m_stTime = stTime;     // Play now.
            pMD->m_lData &= 0xFFFFFF00; // Clear velocity to make note off.
        }
    }
}


void CNoteIn::FlushPart(STIME stTime, BYTE bChannel)

{
    CMIDIData *pMD;
    for (pMD = m_EventList.GetHead();pMD != NULL;pMD = pMD->GetNext())
    {
        if (pMD->m_stTime >= stTime)
        {
			if (bChannel == (BYTE) (pMD->m_lData >> 16))
			{
				pMD->m_stTime = stTime;     // Play now.
				pMD->m_lData &= 0xFFFFFF00; // Clear velocity to make note off.
			}
		}
    }
}

DWORD CModWheelIn::GetModulation(STIME stTime)

{
    DWORD nResult = CMIDIRecorder::GetData(stTime);
    return (nResult);
}

CPitchBendIn::CPitchBendIn()

{
    m_lCurrentData = 0x2000;	// initially at midpoint, no bend
    m_prRange = 200;           // whole tone range by default.
}

// note (davidmay 8/14/96): we don't keep a time-stamped range.
// if people are changing the pitch bend range often, this won't work right,
// but that didn't seem likely enough to warrant a new list.
PREL CPitchBendIn::GetPitch(STIME stTime)

{
    PREL prResult = (PREL) CMIDIRecorder::GetData(stTime);
    prResult -= 0x2000;         // Subtract MIDI Midpoint.
    prResult *= m_prRange;	// adjust by current range
    prResult >>= 13;
    return (prResult);
}

CVolumeIn::CVolumeIn()

{
    m_lCurrentData = 100;
}

VREL CVolumeIn::GetVolume(STIME stTime)

{
    long lResult = CMIDIRecorder::GetData(stTime);
    return (m_vrMIDIToVREL[lResult]);
}

CExpressionIn::CExpressionIn()

{
    m_lCurrentData = 127;
}

VREL CExpressionIn::GetVolume(STIME stTime)

{
    long lResult = CMIDIRecorder::GetData(stTime);
    return (m_vrMIDIToVREL[lResult]);
}

CPanIn::CPanIn()

{
    m_lCurrentData = 64;
}

long CPanIn::GetPan(STIME stTime)

{
    long lResult = (long) CMIDIRecorder::GetData(stTime);
    return (lResult);
}

//////////////////////////////////////////////////////////
// Directx8 Methods 

DWORD CPressureIn::GetPressure(STIME stTime)

{
    DWORD nResult = CMIDIRecorder::GetData(stTime);
    return (nResult);
}

CReverbIn::CReverbIn()

{
    m_lCurrentData = 40;
}

DWORD CReverbIn::GetVolume(STIME stTime)

{
    return (m_vrMIDIPercentToVREL[CMIDIRecorder::GetData(stTime)]);
}

DWORD CChorusIn::GetVolume(STIME stTime)

{
    return (m_vrMIDIPercentToVREL[CMIDIRecorder::GetData(stTime)]);
}

CCutOffFreqIn::CCutOffFreqIn()
{
	m_lCurrentData = 64;
}

DWORD CCutOffFreqIn::GetFrequency(STIME stTime)
{
    DWORD nResult = CMIDIRecorder::GetData(stTime);
    return (nResult);
}

