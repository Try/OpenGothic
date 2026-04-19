//
// Voice.cpp
// Copyright (c) 1996-2001 Microsoft Corporation
//

#ifdef DMSYNTH_MINIPORT
#include "common.h"
#include <math.h>
#include "muldiv32.h"
#else
#include "debug.h"
#include "simple.h"
#include <mmsystem.h>
#include <dmusicc.h>
#include <dmusics.h>
#include "synth.h"
#include <math.h>
#include <stdio.h>
#include "csynth.h"
#endif

#include "fparms.h" // Generated filter parameter arrays

#ifdef _X86_
#define MMX_ENABLED 1
#endif

#ifdef DBG
extern DWORD sdwDebugLevel;
#endif

CVoiceLFO::CVoiceLFO()
{
    m_pModWheelIn = NULL;
    m_pPressureIn = NULL;
    m_bEnable = TRUE;
}

short CVoiceLFO::m_snSineTable[256];

void CVoiceLFO::Init()
{
    double flTemp;
    long lIndex;

    for (lIndex = 0;lIndex < 256;lIndex++)
    {
        flTemp = lIndex;
        flTemp *= 6.283185307;
        flTemp /= 256.0;
        flTemp = sin(flTemp);
        flTemp *= 100.0;
        m_snSineTable[lIndex] = (short) flTemp;
    }
}


STIME CVoiceLFO::StartVoice(CSourceLFO *pSource,
                    STIME stStartTime, CModWheelIn * pModWheelIn, CPressureIn * pPressureIn)
{
    m_bEnable = TRUE;

    m_pModWheelIn = pModWheelIn;
    m_pPressureIn = pPressureIn;
    m_Source = *pSource;
    m_stStartTime = stStartTime;
    if ((m_Source.m_prMWPitchScale == 0) && (m_Source.m_vrMWVolumeScale == 0) &&
        (m_Source.m_prPitchScale == 0) && (m_Source.m_vrVolumeScale == 0))
    {
        m_stRepeatTime = 44100;
    }
    else
    {
        m_stRepeatTime = 2097152 / m_Source.m_pfFrequency; // (1/8 * 256 * 4096 * 16)
    }
    return (m_stRepeatTime);
}

long CVoiceLFO::GetLevel(STIME stTime, STIME *pstNextTime)
{
    if ( !m_bEnable )
        return 0;

    stTime -= (m_stStartTime + m_Source.m_stDelay);
    if (stTime < 0)
    {
        *pstNextTime = -stTime;
        return (0);
    }
    *pstNextTime = m_stRepeatTime;
    stTime *= m_Source.m_pfFrequency;
    stTime = stTime >> (12 + 4); // We've added 4 extra bits of resolution...
    return (m_snSineTable[stTime & 0xFF]);
}

VREL CVoiceLFO::GetVolume(STIME stTime, STIME *pstNextTime)
{
    VREL vrVolume;
    VREL vrCPVolume;
    VREL vrMWVolume;

    if ( !m_bEnable )
        return 0;

    vrCPVolume = m_pPressureIn->GetPressure(stTime);
    vrCPVolume *= m_Source.m_vrCPVolumeScale;
    vrCPVolume /= 127;

    vrMWVolume = m_pModWheelIn->GetModulation(stTime);
    vrMWVolume *= m_Source.m_vrMWVolumeScale;
    vrMWVolume /= 127;

    vrVolume  = vrMWVolume;
    vrVolume += vrCPVolume;
    vrVolume += m_Source.m_vrVolumeScale;
    vrVolume *= GetLevel(stTime, pstNextTime);
    vrVolume /= 100;
    return (vrVolume);
}

PREL CVoiceLFO::GetPitch(STIME stTime, STIME *pstNextTime)
{
    PREL prPitch;
    PREL prCPPitch;
    PREL prMWPitch;

    if ( !m_bEnable )
        return 0;

    prCPPitch = m_pPressureIn->GetPressure(stTime);
    prCPPitch *= m_Source.m_prCPPitchScale;
    prCPPitch /= 127;

    prMWPitch = m_pModWheelIn->GetModulation(stTime);
    prMWPitch *= m_Source.m_prMWPitchScale;
    prMWPitch /= 127;

    prPitch  = prMWPitch;
    prPitch += prCPPitch;
    prPitch += m_Source.m_prPitchScale;
    prPitch *= GetLevel(stTime, pstNextTime);
    prPitch /= 100;

    return (prPitch);
}

PREL CVoiceLFO::GetCutoff(STIME stTime)
{
    PREL prPitch;
    PREL prCPPitch;
    PREL prMWPitch;
    STIME stNextTime;

    if ( !m_bEnable )
        return 0;

    prCPPitch = m_pPressureIn->GetPressure(stTime);
    prCPPitch *= m_Source.m_prCPCutoffScale;
    prCPPitch /= 127;

    prMWPitch = m_pModWheelIn->GetModulation(stTime);
    prMWPitch *= m_Source.m_prMWCutoffScale;
    prMWPitch /= 127;

    prPitch  = prMWPitch;
    prPitch += prCPPitch;
    prPitch += m_Source.m_prCutoffScale;
    prPitch *= GetLevel(stTime, &stNextTime);
    prPitch /= 100;

    return (prPitch);
}

CVoiceEG::CVoiceEG()
{
    m_stStopTime = 0;
    m_bEnable = TRUE;
}

short CVoiceEG::m_snAttackTable[201];

void CVoiceEG::Init()
{
    double flTemp;
    long lV;

    m_snAttackTable[0] = 0;
    for (lV = 1;lV <= 200; lV++)
    {
        flTemp = lV;
        flTemp /= 200.0;
        flTemp *= flTemp;
        flTemp = log10(flTemp);
        flTemp *= 10000.0;
        flTemp /= 96.0;
        flTemp += 1000.0;
        m_snAttackTable[lV] = (short) flTemp;
    }
}

void CVoiceEG::StopVoice(STIME stTime)
{
    if ( m_bEnable )
    {
        m_Source.m_stRelease *= GetLevel(stTime, &m_stStopTime, TRUE);    // Adjust for current sustain level.
        m_Source.m_stRelease /= 1000;
    }
    m_stStopTime = stTime;
}

void CVoiceEG::QuickStopVoice(STIME stTime, DWORD dwSampleRate)

{
    if ( m_bEnable )
    {
        m_Source.m_stRelease *= GetLevel(stTime, &m_stStopTime, TRUE);    // Adjust for current sustain level.
        m_Source.m_stRelease /= 1000;
        dwSampleRate /= 70;
        if (m_Source.m_stRelease > (long) dwSampleRate)
        {
            m_Source.m_stRelease = dwSampleRate;
        }
    }
    m_stStopTime = stTime;
}

STIME CVoiceEG::StartVoice(CSourceEG *pSource, STIME stStartTime,
                    WORD nKey, WORD nVelocity, STIME stMinAttack)
{
    m_bEnable = TRUE;

    m_stStartTime = stStartTime;
    m_stStopTime = 0x7fffffffffffffff;      // set to indefinite future
    m_Source = *pSource;
    if (m_Source.m_stAttack < stMinAttack)
    {
        m_Source.m_stAttack = stMinAttack;
    }
    if (m_Source.m_stRelease < stMinAttack)
    {
        m_Source.m_stRelease = stMinAttack;
    }

    // apply velocity to attack length scaling here
    m_Source.m_stAttack *= CDigitalAudio::PRELToPFRACT(nVelocity * m_Source.m_trVelAttackScale / 127);
    m_Source.m_stAttack /= 4096;

    m_Source.m_stHold  *= CDigitalAudio::PRELToPFRACT(nKey * m_Source.m_trKeyDecayScale / 127);
    m_Source.m_stHold  /= 4096;

    m_Source.m_stDecay *= CDigitalAudio::PRELToPFRACT(nKey * m_Source.m_trKeyDecayScale / 127);
    m_Source.m_stDecay /= 4096;

    m_Source.m_stDecay *= (1000 - m_Source.m_pcSustain);
    m_Source.m_stDecay /= 1000;

    if ( m_Source.m_stDelay )
        return ((STIME)m_Source.m_stDelay);
    else
        return ((STIME)m_Source.m_stAttack);
}

//note: appears to not be in use
BOOL CVoiceEG::InAttack(STIME st)
{
    if ( !m_bEnable )
        return FALSE;

    // has note been released?
    if (st >= m_stStopTime)
        return FALSE;

    // past length of attack?
    if (st >= m_stStartTime + m_Source.m_stDelay + m_Source.m_stAttack)
        return FALSE;

    return TRUE;
}

BOOL CVoiceEG::InRelease(STIME st)
{
    if ( !m_bEnable )
        return FALSE;

    // has note been released?
    if (st > m_stStopTime)
        return TRUE;

    return FALSE;
}

long CVoiceEG::GetLevel(STIME stEnd, STIME *pstNext, BOOL fVolume)
{
    long lLevel = 0;

    if (stEnd <= m_stStopTime)
    {
        stEnd -= m_stStartTime;
        if (stEnd < m_Source.m_stDelay )
        {
            lLevel = 0;
            *pstNext = m_Source.m_stDelay - stEnd;
        }
        else
        {
            stEnd -= m_Source.m_stDelay;
            if (stEnd < m_Source.m_stAttack )
            {
                // still in attack
                lLevel = 1000 * (long) stEnd;
                if (m_Source.m_stAttack)
                {
                    lLevel /= (long) m_Source.m_stAttack;
                }
                else // This should never happen, but it does...
                {
                    lLevel = 0;
                }
                *pstNext = m_Source.m_stAttack - stEnd;
                if (lLevel < 0) lLevel = 0;
                if (lLevel > 1000) lLevel = 1000;
                if (fVolume)
                {
                    lLevel = m_snAttackTable[lLevel / 5];
                }
            }
            else
            {
                stEnd -= m_Source.m_stAttack;
                if ( stEnd < m_Source.m_stHold )
                {
                    lLevel = 1000;
                    *pstNext = m_Source.m_stHold - stEnd;
                    if (fVolume)
                    {
                        lLevel = m_snAttackTable[lLevel / 5];
                    }
                }
                else
                {
                    stEnd -= m_Source.m_stHold;
                    if (stEnd < m_Source.m_stDecay)
                    {
                        // still in decay
                        lLevel = (1000 - m_Source.m_pcSustain) * (long) stEnd;
                        lLevel /= (long) m_Source.m_stDecay;
                        lLevel = 1000 - lLevel;
                        // To improve the decay curve, set the next point to be 1/4, 1/2, or end of slope.
                        // To avoid close duplicates, fudge an extra 100 samples.
                        if (stEnd < ((m_Source.m_stDecay >> 2) - 100))
                        {
                            *pstNext = (m_Source.m_stDecay >> 2) - stEnd;
                        }
                        else if (stEnd < ((m_Source.m_stDecay >> 1) - 100))
                        {
                            *pstNext = (m_Source.m_stDecay >> 1) - stEnd;
                        }
                        else
                        {
                            *pstNext = m_Source.m_stDecay - stEnd;  // Next is end of decay.
                        }
                    }
                    else
                    {
                        // in sustain
                        lLevel = m_Source.m_pcSustain;
                        *pstNext = 44100;
                    }
                }
            }
        }
    }
    else
    {
        STIME stBogus;
        // in release
        stEnd -= m_stStopTime;

        if (stEnd < m_Source.m_stRelease)
        {
            lLevel = GetLevel(m_stStopTime, &stBogus, fVolume) * (long) (m_Source.m_stRelease - stEnd);
            lLevel /= (long) m_Source.m_stRelease;
            if (stEnd < ((m_Source.m_stRelease >> 2) - 100))
            {
                *pstNext = (m_Source.m_stRelease >> 2) - stEnd;
            }
            else if (stEnd < ((m_Source.m_stRelease >> 1) - 100))
            {
                *pstNext = (m_Source.m_stRelease >> 1) - stEnd;
            }
            else
            {
                *pstNext = m_Source.m_stRelease - stEnd;  // Next is end of decay.
            }
        }
        else
        {
            lLevel = 0;   // !!! off
            *pstNext = 0x7FFFFFFFFFFFFFFF;
        }
    }

    return lLevel;
}

VREL CVoiceEG::GetVolume(STIME stTime, STIME *pstNextTime)
{
    if ( !m_bEnable )
        return 0;

    VREL vrLevel = GetLevel(stTime, pstNextTime, TRUE) * 96;
    vrLevel /= 10;
    vrLevel = vrLevel - 9600;
    return vrLevel;
}

PREL CVoiceEG::GetPitch(STIME stTime, STIME *pstNextTime)
{
    if ( !m_bEnable )
        return 0;

    PREL prLevel;
    if (m_Source.m_sScale != 0)
    {
        prLevel = GetLevel(stTime, pstNextTime, FALSE);
        prLevel *= m_Source.m_sScale;
        prLevel /= 1000;
    }
    else
    {
        *pstNextTime = 44100;
        prLevel = 0;
    }
    return prLevel;
}

PREL CVoiceEG::GetCutoff(STIME stTime)
{
    if ( !m_bEnable )
        return 0;

    PREL prLevel;
    STIME pstNextTime;  // not used
    if (m_Source.m_prCutoffScale != 0)
    {
        prLevel = GetLevel(stTime, &pstNextTime, FALSE);
        prLevel *= m_Source.m_prCutoffScale;
        prLevel /= 1000;
    }
    else
    {
        prLevel = 0;
    }
    return prLevel;
}

void CVoiceFilter::StartVoice(CSourceFilter *pSource, CVoiceLFO *pLFO, CVoiceEG *pEG, WORD nKey, WORD nVelocity)
{
    m_Source = *pSource;
    m_pLFO = pLFO;
    m_pEG  = pEG;
    m_prVelScale = (nVelocity * m_Source.m_prVelScale) / 127;
    m_prKeyScale = (nKey * m_Source.m_prKeyScale) / 127;
}


/////////////////////////////////////////////////////////////////////////////////
// DLS2 Lowpass Filter Filter
/*

>>>>> finish low pass filter comment

        b1 = -2.0 * r * cos(theta);
        b2 = r * r;
        K  = (1.0 + b1 + b2) * pow(10.0, -qIndex * 0.0375);

  The Filter :

        z  = (K * sample[i]) - (b1 * z1) - (b2 * z2)
        z2 = z1
        z1 = z

>>> B1 negation turned to positive then used as an add instead of subtraction.

  Resonance : Q
    GainUnits

        -qIndex * 0.0375

        0.0375 = 1.5/40 in db's

    Values
        Q min/max Values are 0db to 22.5db
        Q min/max Values are 0   to 225 in 1/10th db's


  Cutoff Fequency : Fc
    Pitch absolute values

        Absolute Pitch = ((1200 * log2(F/440)) + 6900)

    Values
        Initial Fc min/max Values are 200Hz to 7998Hz
        Initial Fc min/max Values are 5535  to 11921 in abosolute pitch cents


  Table Indexs

        65 - entries in the table

                                Hertz   Pitch
        --------------------------------------
        Max Sample Rate     -> 48000Hz (15023) ---|
                               44100Hz (14877)    |
                               22050Hz (13676)    |
                               .......          9488
        Max Cutoff Freq     -> 7999Hz  (11921)    |
                               .......            |
        Min Cutoff Freq     ->  200Hz  (5535)  ---|


        More Acurately .....

        48KHz       15023.26448623030034519401770744100
        200Hz     -  5534.99577150007811000514765931632
                  =====================================
        Feq Range    9488.26871473022223518887004812496

        Feq Range/1200 =  7.906890596 is the Feq Range in octaves
        Feq Range/100  = 94.882687147 is the Feq Range in setimtones


      Behavoir of Fc to indexes according to ouput Sample Rate

        SampleRate of 48k (15023)
            Fc < 5535  (200Hz)   -> fIndex = 0
            Fc = 11921 (7999Hz)  -> fIndex = 63.86
            Fc > 11935 (8064Hz)  -> fIndex = 64

        SampleRate of 41k (14877)
            Fc = 5535  (200Hz)   -> fIndex = 0
            Fc < 5389  (200Hz)   -> fIndex = 0
            Fc > 11789 (7411Hz)  -> fIndex = 64
            Fc = 11921 (7999Hz)  -> fIndex = 65.32

        SampleRate of 22k (13676)
            Fc < 4188  (92Hz)    -> fIndex = 0
            Fc = 5535  (200Hz)   -> fIndex = 13.44
                 10574 (3675Hz)  -> spec min of 1/6th the sample rate
            Fc > 10588 (3704Hz)  -> fIndex = 64
                 11276 (5510Hz)  -> filter fails one octave bellow Nyquist
            Fc = 11921 (7999Hz)  -> fIndex = 77.33
                 12476 (11025Hz) -> nyquist

  Precision

        0.01 - minimal acuracy for interpolation

        9488.2687
        0.00025 +/- error

        m_aB1[0][63] =    0x33ea24fb  = 0.811166044148771133412393865559
        m_aB1[0][64] =  - 0x2fa8ebf5  = 0.744685163483661751713288716704
                        ============
                          0x04413906  = 0.066480880665109381699105148854

        fIndex's fractional constant  = 0.002687147302222351888700481249

        interpolation of
        m_aB1[0][63] + constant       = 0.810987400229642518622447868604

        difference                    = 0.000178643919128614789945996955

        One 2.30 fixpoint bit         = 0.000000000931322575482840254421

        9488.2687147
        7.906890596 * 1200 = 9488.2687152 <-- precision error

        1-bit lossed when going to intger math
*/
//
void CVoiceFilter::GetCoeff(STIME stTime, PREL prFreqIn, COEFF& cfK, COEFF& cfB1, COEFF& cfB2)
{
    PREL prCutoff;
    DWORD dwFract;
    int iQIndex;
    int iIndex;

    //
    // Check if filter is disabled
    //
    if (m_Source.m_prCutoff == 0x7FFF)
    {
        cfK  = 0x40000000;  // is unity in 2.30 fixpoint
        cfB1 = 0;
        cfB2 = 0;
        return;
    }

    //
    // Accumulate the current Cutoff Frequency
    //
    prCutoff  = m_Source.m_prCutoffSRAdjust;
    prCutoff += m_pLFO->GetCutoff(stTime);
    prCutoff += m_pEG->GetCutoff(stTime);
    prCutoff += m_prVelScale;
    prCutoff += prFreqIn;

    //
    // Set the Resonance Q index
    //
    iQIndex = m_Source.m_iQIndex;

    //
    // Set the cutoff frequency index, and retrive
    // the fractional part for interpolation
    //
    iIndex  = prCutoff;
    if ( iIndex >= 0 )
    {
        dwFract = iIndex % 100;
        iIndex /= 100;
    }
    else
    {
        dwFract = 0;
        iIndex  = -1;
    }

    if (iIndex < 0) // Cutoff fequency is less than 100Hz (at 48k Fs)
    {
        cfK  =  m_aK[iQIndex][0];
        cfB1 = m_aB1[iQIndex][0];
        cfB2 = m_aB2[iQIndex][0];
    }
    else if (iIndex >= FILTER_PARMS_DIM_FC - 1)
    {
        cfK  =  m_aK[iQIndex][FILTER_PARMS_DIM_FC - 1];
        cfB1 = m_aB1[iQIndex][FILTER_PARMS_DIM_FC - 1];
        cfB2 = m_aB2[iQIndex][FILTER_PARMS_DIM_FC - 1];
    }
    else if (iIndex >= FILTER_PARMS_DIM_FC - 5)
    {
        //
        // Not enough headroom to handle the calculation,
        // shift the range douwn by half
        //
        cfK  =  m_aK[iQIndex][iIndex] + (((( m_aK[iQIndex][iIndex+1] -  m_aK[iQIndex][iIndex])   >> 1) * dwFract)/50);
        cfB1 = m_aB1[iQIndex][iIndex] - ((((m_aB1[iQIndex][iIndex]   - m_aB1[iQIndex][iIndex+1]) >> 1) * dwFract)/50);
        cfB2 = m_aB2[iQIndex][iIndex] - ((((m_aB2[iQIndex][iIndex]   - m_aB2[iQIndex][iIndex+1]) >> 1) * dwFract)/50);
    }
    else
    {
        cfK  =  m_aK[iQIndex][iIndex] + (((( m_aK[iQIndex][iIndex+1] -  m_aK[iQIndex][iIndex]))   * dwFract)/100);
        cfB1 = m_aB1[iQIndex][iIndex] - ((((m_aB1[iQIndex][iIndex]   - m_aB1[iQIndex][iIndex+1])) * dwFract)/100);
        cfB2 = m_aB2[iQIndex][iIndex] - ((((m_aB2[iQIndex][iIndex]   - m_aB2[iQIndex][iIndex+1])) * dwFract)/100);
    }
}

//------------------------------------------------------------------------------------
// Reference Filter
// Note: This code is used only for testing or to understance the derivation
// of the above filter code. It was the original source for the current implementation
// aboce was optimized
//------------------------------------------------------------------------------------
/*void CVoiceFilter::GetCoeffRef(STIME stTime, COEFF &cfK, COEFF &cfB1, COEFF &cfB2)
{
    PREL prCutoff;
    int iQIndex;
    int iIndex;
    double fIndex;
    double fIntrp;

    //
    // Check if filter is disabled
    //
    if (m_Source.m_prCutoff == 0x7FFF)
    {
        cfK  = 0x40000000;  // unity in 2.30 fixpoint
        cfB1 = 0;
        cfB2 = 0;
        return;
    }

    //
    // Accumulate the current Cutoff Frequency
    //
    prCutoff  = m_Source.m_prCutoff;
    prCutoff += m_pLFO->GetCutoff(stTime);
    prCutoff += m_pEG->GetCutoff(stTime);
    prCutoff += m_prVelScale;

    //
    // There are 16 resonance values spaced 1.5db arpart
    // DLS2's has a minimum 1.5db error tolerance
    // Range of values it  0db to 22.5db
    // m_Source.m_vrQ are in 1/10 db's
    // The 15.0 represents the 1.5db'in 1/10 db's
    // with the 0.5 for rounding to the nearest index
    //
    iQIndex = (int)((m_Source.m_vrQ / 15.0f) + 0.5f);
    if (iQIndex < 0)
        iQIndex = 0;
    if (iQIndex > FILTER_PARMS_DIM_Q-1) // FILTER_PARMS_DIM_Q = 16
        iQIndex = FILTER_PARMS_DIM_Q-1;

    // >>>>> docdoc
    //
    //
    fIndex = 12.0 * (((prCutoff - m_Source.m_prSampleRate) / 1200.0 ) + 7.906890596);
    iIndex = (int)fIndex;
    fIntrp = fIndex - iIndex;

    if (iIndex < 0)
    {
        cfK  = m_aK [iQIndex][0];
        cfB1 = m_aB1[iQIndex][0];
        cfB2 = m_aB2[iQIndex][0];
    }
    else if (iIndex >= FILTER_PARMS_DIM_FC - 1)
    {
        cfK  = m_aK [iQIndex][FILTER_PARMS_DIM_FC - 1];
        cfB1 = m_aB1[iQIndex][FILTER_PARMS_DIM_FC - 1];
        cfB2 = m_aB2[iQIndex][FILTER_PARMS_DIM_FC - 1];
    }
    else
    {
        //
        // Linearly interpolate the fractional part of the index
        // accross two values of the coeficient table
        //
        cfK  = (COEFF)(m_aK[iQIndex][iIndex] * (1.0 - fIntrp) +
                         m_aK[iQIndex][iIndex+1] * fIntrp);

        cfB1 = (COEFF)(m_aB1[iQIndex][iIndex] * (1.0 - fIntrp) +
                         m_aB1[iQIndex][iIndex+1] * fIntrp);

        cfB2 = (COEFF)(m_aB2[iQIndex][iIndex] * (1.0 - fIntrp) +
                         m_aB2[iQIndex][iIndex+1] * fIntrp);
    }
}*/

BOOL CVoiceFilter::IsFiltered()
{
    return (m_Source.m_prCutoff != 0x7FFF);
}

CDigitalAudio::CDigitalAudio()
{
    m_pfBasePitch = 0;
    m_pfLastPitch = 0;
    m_pfLastSample = 0;
    m_pfLoopEnd = 0;
    m_pfLoopStart = 0;
    m_pfSampleLength = 0;
    m_prLastPitch = 0;
    m_ullLastSample = 0;
    m_ullLoopStart = 0;
    m_ullLoopEnd = 0;
    m_ullSampleLength = 0;
    m_fElGrande = FALSE;
    m_pCurrentBuffer = NULL;
    m_pWaveArt = NULL;
    m_ullSamplesSoFar = 0;
    m_lPrevSample = 0;
    m_lPrevPrevSample = 0;
};

CDigitalAudio::~CDigitalAudio()
{
    if (m_pWaveArt)
    {
        m_pWaveArt->Release();
    }
}

PFRACT CDigitalAudio::m_spfCents[201];
PFRACT CDigitalAudio::m_spfSemiTones[97];
VFRACT CDigitalAudio::m_svfDbToVolume[(MAXDB - MINDB) * 10 + 1];
BOOL CDigitalAudio::m_sfMMXEnabled = FALSE;

#ifdef MMX_ENABLED
BOOL MultiMediaInstructionsSupported();
#endif
#pragma optimize("", off) // Optimize causes crash! Argh!

void CDigitalAudio::Init()
{
    double flTemp;
    VREL    vrdB;

#ifdef MMX_ENABLED
    m_sfMMXEnabled = MultiMediaInstructionsSupported();
#endif // MMX_ENABLED
    for (vrdB = MINDB * 10;vrdB <= MAXDB * 10;vrdB++)
    {
        flTemp = vrdB;
        flTemp /= 100.0;
        flTemp = pow(10.0, flTemp);
        flTemp = pow(flTemp, 0.5);   // square root.
        flTemp *= 4095.0; // 2^12th, but avoid overflow...
        m_svfDbToVolume[vrdB - (MINDB * 10)] = (long) flTemp;
    }

    PREL prRatio;

    for (prRatio = -100;prRatio <= 100;prRatio++)
    {
        flTemp = prRatio;
        flTemp /= 1200.0;
        flTemp = pow(2.0, flTemp);
        flTemp *= 4096.0;
        m_spfCents[prRatio + 100] = (long) flTemp;
    }

    for (prRatio = -48;prRatio <= 48;prRatio++)
    {
        flTemp = prRatio;
        flTemp /= 12.0;
        flTemp = pow(2.0, flTemp);
        flTemp *= 4096.0;
        m_spfSemiTones[prRatio + 48] = (long) flTemp;
    }
}
#pragma optimize("", on)

VFRACT CDigitalAudio::VRELToVFRACT(VREL vrVolume)
{
    vrVolume /= 10;

    if (vrVolume < MINDB * 10)
        vrVolume = MINDB * 10;
    else if (vrVolume >= MAXDB * 10)
        vrVolume = MAXDB * 10;

    return (m_svfDbToVolume[vrVolume - MINDB * 10]);
}

PFRACT CDigitalAudio::PRELToPFRACT(PREL prPitch)
{
    PFRACT pfPitch = 0;
    PREL prOctave;
    if (prPitch > 100)
    {
        if (prPitch > 4800)
        {
            prPitch = 4800;
        }
        prOctave = prPitch / 100;
        prPitch = prPitch % 100;
        pfPitch = m_spfCents[prPitch + 100];
        pfPitch <<= prOctave / 12;
        prOctave = prOctave % 12;
        pfPitch *= m_spfSemiTones[prOctave + 48];
        pfPitch >>= 12;
    }
    else if (prPitch < -100)
    {
        if (prPitch < -4800)
        {
            prPitch = -4800;
        }
        prOctave = prPitch / 100;
        prPitch = (-prPitch) % 100;
        pfPitch = m_spfCents[100 - prPitch];
        pfPitch >>= ((-prOctave) / 12);
        prOctave = (-prOctave) % 12;
        pfPitch *= m_spfSemiTones[48 - prOctave];
        pfPitch >>= 12;
    }
    else
    {
        pfPitch = m_spfCents[prPitch + 100];
    }
    return (pfPitch);
}

void CDigitalAudio::ClearVoice()

{
    if (m_Source.m_pWave != NULL)
    {
        m_Source.m_pWave->PlayOff();
        m_Source.m_pWave->Release();    // Releases wave structure.
        m_Source.m_pWave = NULL;
    }
    if (m_pWaveArt)
    {
        m_pWaveArt->Release();
        m_pWaveArt = NULL;
    }
}

STIME CDigitalAudio::StartVoice(CSynth *pSynth,
                               CSourceSample *pSample,
                               PREL prBasePitch,
                               long lKey)
{
    m_prLastPitch = 0;
    m_lPrevSample = 0;
    m_lPrevPrevSample = 0;
    m_cfLastK  = 0;
    m_cfLastB1 = 0;
    m_cfLastB2 = 0;

    m_Source = *pSample;
    m_pnWave = pSample->m_pWave->m_pnWave;
    m_pSynth = pSynth;

    m_bOneShot = m_Source.m_bOneShot;

    pSample->m_pWave->AddRef(); // Keeps track of Wave usage.
    pSample->m_pWave->PlayOn();

    // Set initial pitch
    prBasePitch += pSample->m_prFineTune;
    prBasePitch += ((lKey - pSample->m_bMIDIRootKey) * 100);
    m_pfBasePitch = PRELToPFRACT(prBasePitch);
    m_pfBasePitch *= pSample->m_dwSampleRate;
    m_pfBasePitch /= pSynth->m_dwSampleRate;
    m_pfLastPitch = m_pfBasePitch;

    m_fElGrande = pSample->m_dwSampleLength >= 0x80000;     // Greater than 512k.
    if ((pSample->m_dwLoopEnd - pSample->m_dwLoopStart) >= 0x80000)
    {   // We can't handle loops greater than 1 meg!
        m_bOneShot = TRUE;
    }

    m_ullLastSample = 0;
    m_ullLoopStart = pSample->m_dwLoopStart;
    m_ullLoopStart = m_ullLoopStart << 12;
    m_ullLoopEnd = pSample->m_dwLoopEnd;
    m_ullLoopEnd = m_ullLoopEnd << 12;
    m_ullSampleLength = pSample->m_dwSampleLength;
    m_ullSampleLength = m_ullSampleLength << 12;
    m_pfLastSample = 0;
    m_pfLoopStart = (long) m_ullLoopStart;
    m_pfLoopEnd = (long) m_ullLoopEnd;

    if (m_ullLoopEnd <= m_ullLoopStart) // Should never happen, but death if it does!
    {
        m_bOneShot = TRUE;
    }
    if (m_fElGrande)
    {
        m_pfSampleLength = 0x7FFFFFFF;
    }
    else
    {
        m_pfSampleLength = (long) m_ullSampleLength;
    }

    m_pCurrentBuffer = NULL;    // Used by wave playing must be null for standard sample
    m_pWaveArt = NULL;
    m_ullSamplesSoFar = 0;

    return (0); // !!! what is this return value?
}


STIME CDigitalAudio::StartWave(CSynth *pSynth,
                               CWaveArt *pWaveArt,
                               PREL prBasePitch,
                               SAMPLE_TIME stVoiceStart,
                               SAMPLE_TIME stLoopStart,
                               SAMPLE_TIME stLoopEnd)
{
    m_pSynth   = pSynth;    // Save Synth

    if (pWaveArt)
    {
        pWaveArt->AddRef();
    }
    if (m_pWaveArt)
    {
        m_pWaveArt->Release();
    }
    m_pWaveArt = pWaveArt;  // Save Wave articulation

    // Reset all wave buffer flags
    CWaveBuffer* pWavBuf = pWaveArt->m_pWaves.GetHead();
    while ( pWavBuf )
    {
        pWavBuf->m_pWave->m_bActive = FALSE;
        pWavBuf = pWavBuf->GetNext();
    }

    // Initialize the current play buffer
    m_pCurrentBuffer = pWaveArt->m_pWaves.GetHead();;

    //if m_pCurrentBuffer is NULL the articulation contains
    //no samples... this shouldn't be possible.
    assert(m_pCurrentBuffer);

    m_pCurrentBuffer->m_pWave->m_bActive = TRUE;
    m_pCurrentBuffer->m_pWave->AddRef(); // Keeps track of Wave usage.
    m_pCurrentBuffer->m_pWave->PlayOn();

    // Fill CSourceSample class with CWave Defaults
    m_Source.m_pWave          = m_pCurrentBuffer->m_pWave;
    m_Source.m_dwSampleLength = m_pCurrentBuffer->m_pWave->m_dwSampleLength;
    m_Source.m_dwSampleRate   = m_pCurrentBuffer->m_pWave->m_dwSampleRate;
    m_Source.m_bSampleType    = m_pCurrentBuffer->m_pWave->m_bSampleType;
    m_Source.m_dwID           = m_pCurrentBuffer->m_pWave->m_dwID;
    m_Source.m_dwLoopStart    = 0;
    m_Source.m_dwLoopEnd      = m_pCurrentBuffer->m_pWave->m_dwSampleLength;
    m_Source.m_bMIDIRootKey   = 0;
    m_Source.m_prFineTune     = 0;

    m_bOneShot                = TRUE;

    // The the current sample pointer
    m_pnWave = m_pCurrentBuffer->m_pWave->m_pnWave;

    // Set initial pitch
    m_pfBasePitch = PRELToPFRACT(prBasePitch);
    m_pfBasePitch *= m_Source.m_dwSampleRate;
    m_pfBasePitch /= pSynth->m_dwSampleRate;
    m_pfLastPitch = m_pfBasePitch;
    m_prLastPitch = 0;

    m_fElGrande = m_Source.m_dwSampleLength >= 0x80000;     // Greater than 512k.

    m_ullLastSample = stVoiceStart;
    m_ullLastSample = m_ullLastSample << 12;
    m_ullSamplesSoFar = 0;
    m_ullLoopStart = m_Source.m_dwLoopStart;
    m_ullLoopStart = m_ullLoopStart << 12;
    m_ullLoopEnd = m_Source.m_dwLoopEnd;
    m_ullLoopEnd = m_ullLoopEnd << 12;
    m_ullSampleLength = m_Source.m_dwSampleLength;
    m_ullSampleLength = m_ullSampleLength << 12;
    m_pfLastSample = (long) m_ullLastSample;
    m_pfLoopStart = (long) m_ullLoopStart;
    m_pfLoopEnd = (long) m_ullLoopEnd;

    if (stLoopStart || stLoopEnd)
    {
        m_bOneShot = FALSE;

        m_ullLoopStart = stLoopStart;
        m_ullLoopStart = m_ullLoopStart << 12;
        m_ullLoopEnd = stLoopEnd;
        m_ullLoopEnd = m_ullLoopEnd << 12;
        m_pfLoopStart = (long) m_ullLoopStart;
        m_pfLoopEnd = (long) m_ullLoopEnd;
    }

    if ((stLoopEnd - stLoopStart) >= 0x80000)
    {
        m_bOneShot = TRUE;
    }

    // This could be WAY beyond the actual wave data range
    // So find out the sample we want to start at
    if(stVoiceStart > stLoopStart)
    {
        SAMPLE_TIME stLoopLen = stLoopEnd - stLoopStart;
        if(m_bOneShot == FALSE && stLoopLen != 0)
        {
            m_ullLastSample = stVoiceStart - stLoopStart;
            m_ullLastSample = m_ullLastSample - (stLoopLen * (m_ullLastSample / stLoopLen));
            m_ullLastSample = stLoopStart + m_ullLastSample;
            m_ullLastSample = m_ullLastSample << 12;
            m_pfLastSample = (long) (m_ullLastSample);
        }

        // Must be a wave with an start offset?
        // In any case we need to correct this or else we crash
        if(m_bOneShot && stVoiceStart > m_Source.m_dwSampleLength)
        {
            m_ullLastSample = 0;
            m_pfLastSample = 0;
        }
    }


    if(m_fElGrande)
    {
        m_pfSampleLength = 0x7FFFFFFF;
    }
    else
    {
        m_pfSampleLength = (long) m_ullSampleLength;
    }

    return (0);
}

/*  If the wave is bigger than one meg, the index can overflow.
    Solve this by assuming no mix session will ever be as great
    as one meg AND loops are never that long. We keep all our
    fractional indexes in two variables. In one case, m_pfLastSample,
    is the normal mode where the lower 12 bits are the fraction and
    the upper 20 bits are the index. And, m_ullLastSample
    is a LONGLONG with an extra 32 bits of index. The mix engine
    does not want the LONGLONGs, so we need to track the variables
    in the LONGLONGs and prepare them for the mixer as follows:
    Prior to mixing,
    if the sample is large (m_fElGrande is set), BeforeSampleMix()
    is called. This finds the starting point for the mix, which
    is either the current position or the start of the loop,
    whichever is earlier. It subtracts this starting point from
    the LONGLONG variables and stores an offset in m_dwAddressUpper.
    It also adjusts the pointer to the wave data appropriately.
    AfterSampleMix() does the inverse, reconstructing the the LONGLONG
    indeces and returning everthing back to normal.
*/

void CDigitalAudio::BeforeBigSampleMix()
{
    if (m_fElGrande)
    {
        ULONGLONG ullBase = 0;
        DWORD dwBase;
        if (m_bOneShot)
        {
            ullBase = m_ullLastSample;
        }
        else
        {
            if (m_ullLastSample < m_ullLoopStart)
            {
                ullBase = m_ullLastSample;
            }
            else
            {
                ullBase = m_ullLoopStart;
            }
        }

        // Keep the value as we want to offset into the wave buffer
        ULONGLONG ullWaveOffset = ullBase;

        ullBase >>= 12;
        dwBase = (DWORD) ullBase & 0xFFFFFFFE;      // Clear bottom bit so 8 bit pointer aligns with short.
        ullBase = dwBase;
        ullBase <<= 12;
        m_dwAddressUpper = dwBase;

        m_pfLastSample = (long) (m_ullLastSample - ullBase);

        if ((m_ullLoopEnd - ullBase) < 0x7FFFFFFF)
        {
            m_pfLoopStart = (long) (m_ullLoopStart - ullBase);
            m_pfLoopEnd = (long) (m_ullLoopEnd - ullBase);
        }
        else
        {
            m_pfLoopStart = 0;
            m_pfLoopEnd = 0x7FFFFFFF;
        }

        ullBase = m_ullSampleLength - ullBase;
        dwBase = (DWORD)(ullWaveOffset >> 12);

        if (ullBase > 0x7FFFFFFF)
        {
            m_pfSampleLength = 0x7FFFFFFF;
        }
        else
        {
            m_pfSampleLength = (long) ullBase;
        }
        if (m_Source.m_bSampleType & SFORMAT_8)
        {
            dwBase >>= 1;
        }
        m_pnWave = &m_Source.m_pWave->m_pnWave[dwBase];
    }
}

void CDigitalAudio::AfterBigSampleMix()
{
    m_pnWave = m_Source.m_pWave->m_pnWave;
    if (m_fElGrande)
    {
        ULONGLONG ullBase = m_dwAddressUpper;
        m_ullLastSample = m_pfLastSample;
        m_ullLastSample += (ullBase << 12);
        m_dwAddressUpper = 0;
    }
}

BOOL CDigitalAudio::Mix(short **ppBuffers,      // Array of mix buffers
                        DWORD dwBufferCount,    // Number of mix buffers
                        DWORD dwInterleaved,    // Are the buffers interleaved data?
                        DWORD dwLength,         // Length to mix, in samples
                        VREL  vrMaxVolumeDelta, // Maximum volume accross all buses
                        VFRACT vfNewVolume[],
                        VFRACT vfLastVolume[],
                        PREL  prPitch,          // Pitch to play the sample too
                        DWORD dwIsFiltered,     // Is the mix filtered
                        COEFF cfK,              // filter coeficients
                        COEFF cfB1,
                        COEFF cfB2)
{
    DWORD i;
    PFRACT pfDeltaPitch;
    PFRACT pfEnd;
    PFRACT pfLoopLen;
    PFRACT pfNewPitch;
    VFRACT vfDeltaVolume[MAX_DAUD_CHAN];
    DWORD dwPeriod = 64;
    DWORD dwSoFar;
    DWORD dwStart; // position in WORDs
    DWORD dwMixChoice = 0;
    DWORD dwBuffers;
    PFRACT pfPreMix;
    COEFFDELTA  cfdK  = 0;
    COEFFDELTA  cfdB1 = 0;
    COEFFDELTA  cfdB2 = 0;

    if (dwLength == 0)      // Attack was instant.
    {
        m_pfLastPitch = (m_pfBasePitch * PRELToPFRACT(prPitch)) >> 12;
        m_prLastPitch = prPitch;
        m_cfLastK  = cfK;
        m_cfLastB1 = cfB1;
        m_cfLastB2 = cfB2;

        return TRUE;
    }

    if ( m_pWaveArt ) // Playing a wave or Streaming
    {
        if ( m_pWaveArt->m_bStream )
        {
            // Check if the buffer is valid yet
            if ( !m_pCurrentBuffer->m_pWave->m_bValid )
            {
                Trace(3, "Warning: Synth starting mix with invalid streaming wave buffer\n\r");
                return TRUE; // not valid yet, get out of here
            }
            m_pCurrentBuffer->m_pWave->m_bActive = TRUE;

            if ( m_pCurrentBuffer->m_pWave->m_bLastSampleInit == FALSE )
            {
                CWaveBuffer* pnextbuffer = m_pCurrentBuffer->GetNextLoop();

                if ( pnextbuffer->m_pWave->m_bValid )
                {
                    DWORD dwSampleLength = m_pCurrentBuffer->m_pWave->m_dwSampleLength;   // Length of sample.

                    if ( m_Source.m_bSampleType == SFORMAT_8 )
                    {
                        ((BYTE*)m_pCurrentBuffer->m_pWave->m_pnWave)[dwSampleLength-1] = ((BYTE*)pnextbuffer->m_pWave->m_pnWave)[0];
                    }
                    else
                    {
                        m_pCurrentBuffer->m_pWave->m_pnWave[dwSampleLength-1] = pnextbuffer->m_pWave->m_pnWave[0];
                    }

                    m_pCurrentBuffer->m_pWave->m_bLastSampleInit = TRUE;
                }
            }
        }
    }

    if ((m_Source.m_pWave == NULL) || (m_Source.m_pWave->m_pnWave == NULL))
    {
        return FALSE;
    }

    DWORD dwMax = max(vrMaxVolumeDelta, abs(prPitch - m_prLastPitch) << 1);
    dwMax >>= 1;
    m_prLastPitch = prPitch;

    if (dwMax > 0)
    {
        dwPeriod = (dwLength << 3) / dwMax;
        if (dwPeriod > 512)
        {
            dwPeriod = 512;
        }
        else if (dwPeriod < 1)
        {
            dwPeriod = 1;
        }
    }
    else
    {
        dwPeriod = 512;     // Make it happen anyway.
    }

    // This makes MMX sound a little better (MMX bug will be fixed)
    dwPeriod += 3;
    dwPeriod &= 0xFFFFFFFC;

    pfNewPitch = m_pfBasePitch * PRELToPFRACT(prPitch);
    pfNewPitch >>= 12;

    pfDeltaPitch = MulDiv(pfNewPitch - m_pfLastPitch, dwPeriod << 8, dwLength);


    if ( dwInterleaved )
    {
        vfDeltaVolume[0] = MulDiv(vfNewVolume[0] - vfLastVolume[0], dwPeriod << 8, dwLength);
        vfDeltaVolume[1] = MulDiv(vfNewVolume[1] - vfLastVolume[1], dwPeriod << 8, dwLength);
    }
    else
    {
        for (dwBuffers = 0; dwBuffers < dwBufferCount; dwBuffers++)
        {
            vfDeltaVolume[dwBuffers] = MulDiv(vfNewVolume[dwBuffers] - vfLastVolume[dwBuffers], dwPeriod << 8, dwLength);
        }
    }

    if ( dwInterleaved )
    {
        dwMixChoice |= SPLAY_INTERLEAVED;
    }

    if (m_sfMMXEnabled && (dwLength > 8))
    {
        dwMixChoice |= SPLAY_MMX;
    }

    dwMixChoice |= m_Source.m_bSampleType;
    dwStart = 0;

    if (dwIsFiltered)
    {
        dwMixChoice |= SPLAY_FILTERED;

        //
        // The coeficients have been stored as DWORD's to gain an additional
        // bit of presision when calculating the interpolation between
        // coefiecients in the table.  Since these calcutlations always
        // result in positive coefiecients no greater the 1.9999,
        // we can safely cast to a signed int, from which negative deltas
        // can be correctly determined.
        //
        cfdK =  MulDiv((LONG)cfK  - (LONG)m_cfLastK,  dwPeriod, dwLength);
        cfdB1 = MulDiv((LONG)cfB1 - (LONG)m_cfLastB1, dwPeriod, dwLength);
        cfdB2 = MulDiv((LONG)cfB2 - (LONG)m_cfLastB2, dwPeriod, dwLength);
    }

    for (;;)
    {
        if (dwLength <= 8)
        {
            dwMixChoice &= ~SPLAY_MMX;
        }

        if (m_fElGrande)
        {
            BeforeBigSampleMix();
        }

        if (m_bOneShot)
        {
            pfEnd = m_pfSampleLength;
            if(m_pCurrentBuffer && m_pCurrentBuffer->m_pWave)
            {
                // We grow the buffers by one sample for interpolation so we can transition smoothly
                // between the multiple streaming buffers. This will cause a click at the end of the 
                // buffer if the wave is ending as there's no valid nex tbuffer. So we check for that
                // and adjust the length of the buffer so that the mix engine doesn't try to interpolate
                // the additional (last) sample. If it's NOT the last buffer then we proceed as planned.
                if((pfEnd >> 12) >= (long)(m_pCurrentBuffer->m_pWave->m_dwSampleLength - 1))
                {
                    CWaveBuffer* pnextbuffer = m_pCurrentBuffer->GetNextLoop();
                    if(pnextbuffer == NULL || pnextbuffer->m_pWave->m_bValid == FALSE)
                    {
                        pfEnd = (m_pCurrentBuffer->m_pWave->m_dwSampleLength - 2) << 12;    
                    }
                    else
                    {
                        pfEnd = (m_pCurrentBuffer->m_pWave->m_dwSampleLength - 1) << 12;
                    }
                }
            }

            pfLoopLen = 0;
            pfPreMix = m_pfLastSample;      // save off last sample pos
        }
        else
        {
            pfEnd = m_pfLoopEnd;
            pfLoopLen = m_pfLoopEnd - m_pfLoopStart;
            pfPreMix = 0;
            if (pfLoopLen <= pfNewPitch)
            {
                return FALSE;
            }

            if(pfLoopLen > m_pfSampleLength)
            {
                return FALSE;
            }
        }

        switch (dwMixChoice)
        {
        case SFORMAT_8 | SPLAY_INTERLEAVED :
            dwSoFar = Mix8(ppBuffers[0], dwLength, dwPeriod,
                vfDeltaVolume[0], vfDeltaVolume[1],
                vfLastVolume,
                pfDeltaPitch,
                pfEnd, pfLoopLen);
            break;
        case SFORMAT_16 | SPLAY_INTERLEAVED :
            dwSoFar = Mix16(ppBuffers[0], dwLength, dwPeriod,
                vfDeltaVolume[0], vfDeltaVolume[1],
                vfLastVolume,
                pfDeltaPitch,
                pfEnd, pfLoopLen);
            break;
        case SFORMAT_8 | SPLAY_INTERLEAVED | SPLAY_FILTERED | SPLAY_MMX : 
        case SFORMAT_8 | SPLAY_INTERLEAVED | SPLAY_FILTERED  : 
            dwSoFar = Mix8Filter(ppBuffers[0],dwLength,dwPeriod,
                vfDeltaVolume[0], vfDeltaVolume[1],
                vfLastVolume,
                pfDeltaPitch, 
                pfEnd, pfLoopLen,
                cfdK, cfdB1, cfdB2);
            break;
        case SFORMAT_16 | SPLAY_INTERLEAVED | SPLAY_FILTERED | SPLAY_MMX : 
        case SFORMAT_16 | SPLAY_INTERLEAVED | SPLAY_FILTERED  : 
            dwSoFar = Mix16Filter(ppBuffers[0],dwLength,dwPeriod,
                vfDeltaVolume[0], vfDeltaVolume[1],
                vfLastVolume,
                pfDeltaPitch, 
                pfEnd, pfLoopLen,
                cfdK, cfdB1, cfdB2);
            break;
#ifdef MMX_ENABLED
        case SFORMAT_8 | SPLAY_MMX | SPLAY_INTERLEAVED :
            dwSoFar = Mix8X(ppBuffers[0], dwLength, dwPeriod,
                vfDeltaVolume[0], vfDeltaVolume[1],
                vfLastVolume,
                pfDeltaPitch,
                pfEnd, pfLoopLen);

            break;
        case SFORMAT_16 | SPLAY_MMX | SPLAY_INTERLEAVED :
            dwSoFar = Mix16X(ppBuffers[0], dwLength, dwPeriod,
                vfDeltaVolume[0], vfDeltaVolume[1],
                vfLastVolume,
                pfDeltaPitch,
                pfEnd, pfLoopLen);
            break;
#endif
        case SFORMAT_8 :
        case SFORMAT_8 | SPLAY_MMX :
            dwSoFar = MixMulti8(ppBuffers, dwBufferCount,
                dwLength, dwPeriod,
                vfDeltaVolume,
                vfLastVolume,
                pfDeltaPitch,
                pfEnd, pfLoopLen);
            break;
        case SFORMAT_8 | SPLAY_FILTERED :
        case SFORMAT_8 | SPLAY_FILTERED | SPLAY_MMX :
            dwSoFar = MixMulti8Filter(ppBuffers, dwBufferCount,
                dwLength, dwPeriod,
                vfDeltaVolume,
                vfLastVolume,
                pfDeltaPitch,
                pfEnd, pfLoopLen,
                cfdK, cfdB1, cfdB2);
            break;
        case SFORMAT_16 :
        case SFORMAT_16 | SPLAY_MMX :
            dwSoFar = MixMulti16(ppBuffers, dwBufferCount,
                dwLength, dwPeriod,
                vfDeltaVolume,
                vfLastVolume,
                pfDeltaPitch,
                pfEnd, pfLoopLen);
            break;
        case SFORMAT_16 | SPLAY_FILTERED :
        case SFORMAT_16 | SPLAY_FILTERED | SPLAY_MMX :
            dwSoFar = MixMulti16Filter(ppBuffers, dwBufferCount,
                dwLength, dwPeriod,
                vfDeltaVolume,
                vfLastVolume,
                pfDeltaPitch,
                pfEnd, pfLoopLen,
                cfdK, cfdB1, cfdB2);
            break;
        default :
            return (FALSE);
        }

        if (m_fElGrande)
        {
            AfterBigSampleMix();
        }

        if (m_bOneShot)
        {
            // have mixed all we needed at this time to break
            if (dwSoFar >= dwLength)
            {
                m_ullSamplesSoFar += (m_pfLastSample - pfPreMix)>>12;
                break;
            }

            // the mix engine reached the end of the source data
            m_ullSamplesSoFar += ((m_pfLastSample - pfPreMix)>>12)-1;

            if ( m_pWaveArt ) // Playing or Streaming a Wave
            {
                if ( !m_pWaveArt->m_bStream )   // we must be at the end of the buffer
                    return FALSE;

                // Set completion flags
                m_pCurrentBuffer->m_pWave->m_bActive = FALSE;
                m_pCurrentBuffer->m_pWave->m_bValid  = FALSE;
                m_pCurrentBuffer->m_pWave->m_bLastSampleInit = FALSE;

                // Get next buffer
                m_pCurrentBuffer = m_pCurrentBuffer->GetNextLoop();

                // Set new wave pointer to play out of
                m_pnWave = m_pCurrentBuffer->m_pWave->m_pnWave;

                // Check if the buffer is valid yet
                if ( !m_pCurrentBuffer->m_pWave->m_bValid )
                {
                    Trace(2, "Warning: Synth attempting to start invalid streaming wave buffer\n\r");
                    break;  // nothing to play yet, get out of here
                }
                m_pCurrentBuffer->m_pWave->m_bActive = TRUE;

                CWaveBuffer* pnextbuffer = m_pCurrentBuffer->GetNextLoop();
                if ( pnextbuffer->m_pWave->m_bValid )
                {
                    DWORD dwSampleLength = m_pCurrentBuffer->m_pWave->m_dwSampleLength;   // Length of sample.

                    if ( m_Source.m_bSampleType == SFORMAT_8 )
                    {
                        ((BYTE*)m_pCurrentBuffer->m_pWave->m_pnWave)[dwSampleLength-1] = ((BYTE*)pnextbuffer->m_pWave->m_pnWave)[0];
                    }
                    else
                    {
                        m_pCurrentBuffer->m_pWave->m_pnWave[dwSampleLength-1] = pnextbuffer->m_pWave->m_pnWave[0];
                    }

                    m_pCurrentBuffer->m_pWave->m_bLastSampleInit = TRUE;
                }

//>>>>>>>>>> CHECK FOR LOOP POINT, IF SO NOT TRY AGAIN HERE

                dwStart  += dwSoFar << dwInterleaved;
                dwLength -= dwSoFar;
                m_pfLastSample = 0;

//>>>>>>>>>> CHECK INTERLEAVED FLAG FOR CORRECT DISTANCE ????????
                // Move buffer pointers since we are mixing more samples
                for ( i = 0; i < dwBufferCount; i++ )
                    ppBuffers[i] += dwStart;

                continue;   // keep playing
            }
            else
                return FALSE;   // Playing a standard one shot, we hit the end of the buffer
        }
        else
        {
            if (dwSoFar >= dwLength)
                break;

            // Loops are handled in the mix engine, however
            // when you reach the end of source data you will
            // reach this code.

            dwStart  += dwSoFar << dwInterleaved;
            dwLength -= dwSoFar;
            m_pfLastSample -= (m_pfLoopEnd - m_pfLoopStart);

            // Move buffer pointers since we are mixing more samples
            for ( i = 0; i < dwBufferCount; i++ )
                ppBuffers[i] += dwStart;
        }
    }

    m_pfLastPitch = pfNewPitch;
    m_cfLastK  = cfK;
    m_cfLastB1 = cfB1;
    m_cfLastB2 = cfB2;

    return (TRUE);
}

CVoice::CVoice()
{
    m_pControl = NULL;
    m_pPitchBendIn = NULL;
    m_pExpressionIn = NULL;
    m_dwPriority = 0;
    m_nPart = 0;
    m_nKey = 0;
    m_fInUse = FALSE;
    m_fSustainOn = FALSE;
    m_fNoteOn = FALSE;
    m_fTag = FALSE;
    m_stStartTime = 0;
    m_stStopTime = 0x7fffffffffffffff;
    m_stWaveStopTime = 0;
    m_vrVolume = 0;
    m_fAllowOverlap = FALSE;
    m_pRegion = NULL;
    m_pReverbSend = NULL;
    m_pChorusSend = NULL;
    m_dwLoopType = 0;

    for ( int i = 0; i < MAX_DAUD_CHAN; i++ )
    {
        m_vfLastVolume[i] = 0;
        m_vrLastVolume[i] = 0;
    }
}

VREL CVoice::m_svrPanToVREL[128];

void CVoice::Init()
{
    static BOOL fBeenHereBefore = FALSE;
    if (fBeenHereBefore) return;
    fBeenHereBefore = TRUE;
    CVoiceLFO::Init();
    CVoiceEG::Init();
    CDigitalAudio::Init();

    WORD nI;
    for (nI = 1; nI < 128; nI++)
    {
        double flTemp;
        flTemp = nI;
        flTemp /= 127.0;
        flTemp = log10(flTemp);
        flTemp *= 1000.0;
        m_svrPanToVREL[nI] = (long) flTemp;
    }
    m_svrPanToVREL[0] = -2500;
}

void CVoice::StopVoice(STIME stTime)
{
    if (m_fNoteOn)
    {
        if (stTime <= m_stStartTime) stTime = m_stStartTime + 1;
        m_PitchEG.StopVoice(stTime);
        m_VolumeEG.StopVoice(stTime);
        m_fNoteOn = FALSE;
        m_fSustainOn = FALSE;
        m_stStopTime = stTime;
        m_stWaveStopTime = 0;

        if (m_dwLoopType == WLOOP_TYPE_RELEASE)
        {
            m_DigitalAudio.BreakLoop();
        }
    }
}

void CVoice::QuickStopVoice(STIME stTime)
{
    m_fTag = TRUE;
    if (m_fNoteOn || m_fSustainOn)
    {
        if (stTime <= m_stStartTime) stTime = m_stStartTime + 1;
        m_PitchEG.StopVoice(stTime);
        m_VolumeEG.QuickStopVoice(stTime, m_pSynth->m_dwSampleRate);
        m_fNoteOn = FALSE;
        m_fSustainOn = FALSE;
        m_stStopTime = stTime;
    }
    else
    {
        m_VolumeEG.QuickStopVoice(m_stStopTime, m_pSynth->m_dwSampleRate);
    }
}

BOOL CVoice::StartVoice(CSynth *pSynth,
                           CSourceRegion *pRegion,
                           STIME stStartTime,
                           CModWheelIn * pModWheelIn,
                           CPitchBendIn * pPitchBendIn,
                           CExpressionIn * pExpressionIn,
                           CVolumeIn * pVolumeIn,
                           CPanIn * pPanIn,
                           CPressureIn * pPressureIn,
                           CReverbIn * pReverbSend,
                           CChorusIn * pChorusSend,
                           CCutOffFreqIn * pCCutOffFreqIn,
                           CBusIds * pBusIds,
                           WORD nKey,
                           WORD nVelocity,
                           VREL vrVolume,
                           PREL prPitch)
{
    m_pSynth = pSynth;

    CSourceArticulation * pArticulation = pRegion->m_pArticulation;
    if (pArticulation == NULL)
    {
        return FALSE;
    }

    m_dwLoopType = pRegion->m_Sample.m_dwLoopType;

    // if we're going to handle volume later, don't read it now.
    if (!pSynth->m_fAllowVolumeChangeWhilePlayingNote)
        vrVolume += pVolumeIn->GetVolume(stStartTime);

    prPitch += pRegion->m_prTuning;
    m_dwGroup = pRegion->m_bGroup;
    m_fAllowOverlap = pRegion->m_bAllowOverlap;

    vrVolume += CMIDIRecorder::VelocityToVolume(nVelocity);

    vrVolume += pRegion->m_vrAttenuation;

    m_lDefaultPan = pRegion->m_pArticulation->m_sDefaultPan;

    // ignore pan here if allowing pan to vary after note starts
    // or if the source is multichannel or the dest is mono
    //

    m_fIgnorePan = pRegion->IsMultiChannel();
    if (pBusIds->m_dwBusCount == 1)
    {
        DWORD dwFunctionID;
        if (m_pSynth->BusIDToFunctionID(pBusIds->m_dwBusIds[0], &dwFunctionID, NULL, NULL))
        {
            if (dwFunctionID == DSBUSID_LEFT)
            {
                m_fIgnorePan = TRUE;
            }
        }
    }

    VREL vrVolumeL;
    VREL vrVolumeR;
    if ( pSynth->m_dwStereo &&
        !pSynth->m_fAllowPanWhilePlayingNote &&
        !m_fIgnorePan)
    {
        long lPan = pPanIn->GetPan(stStartTime) + m_lDefaultPan;

        if (lPan < 0)
            lPan = 0;

        if (lPan > 127)
            lPan = 127;

        vrVolumeL = m_svrPanToVREL[127 - lPan] + vrVolume;
        vrVolumeR = m_svrPanToVREL[lPan] + vrVolume;
    }
    else
    {
        vrVolumeL = vrVolume;
        vrVolumeR = vrVolume;
    }

    VREL vrVolumeReverb = vrVolume;
    VREL vrVolumeChorus = vrVolume;

    PREL prBusPitchBend = 0;  // This gets a pitch offset that is set by DSound in response to SetFrequency and Doppler commands.
                             // When this is applied to multiple buses, only one of the values can be used, so we always give
                             // preference to the buffer that has DSBUSID_DYNAMIC_0 for the functional id, since that
                             // would most likely be a 3D sound effect.
    BOOL fDynamic = false;

    for( DWORD i = 0; i < pBusIds->m_dwBusCount; i++ )
    {
        DWORD dwFunctionID;
        PREL prGetPitch = 0;
        if (m_pSynth->BusIDToFunctionID(pBusIds->m_dwBusIds[i], &dwFunctionID, &prGetPitch, NULL))
        {
            if (!fDynamic)
            {
                // If no previous bus was dynamic, get this value.
                prBusPitchBend = prGetPitch;
            }
            m_vrBaseVolume[i] = MIN_VOLUME;

            if (DSBUSID_IS_SPKR_LOC(dwFunctionID))
            {
                if (pRegion->IsMultiChannel())
                {
                    // Explicit channel assignment with no pan. For every bus
                    // that matches a bit in the channel mask, turn it on.
                    //
                    if (pRegion->m_dwChannel & (1 << dwFunctionID))
                    {
                        m_vrBaseVolume[i] = vrVolume;
                    }
                }
                else
                {
                    switch(dwFunctionID)
                    {
                    case DSBUSID_LEFT:
                        m_vrBaseVolume[i] = vrVolumeL;
                        break;

                    case DSBUSID_RIGHT:
                        m_vrBaseVolume[i] = vrVolumeR;
                        break;
                    }
                }
            }
            else
            {
                // Not a speaker location, a send or a 3D buffer.
                //
                switch(dwFunctionID)
                {
                case DSBUSID_REVERB_SEND:
                    m_vrBaseVolume[i] = vrVolumeReverb;
                    break;

                case DSBUSID_CHORUS_SEND:
                    m_vrBaseVolume[i] = vrVolumeChorus;
                    break;

                case DSBUSID_NULL:
                    m_vrBaseVolume[i] = MIN_VOLUME;
                    break;

                case DSBUSID_DYNAMIC_0:
                    fDynamic = true;
                default:
                    m_vrBaseVolume[i] = vrVolume;
                }
            }

            m_vrLastVolume[i] = MIN_VOLUME;
            m_vfLastVolume[i] = m_DigitalAudio.VRELToVFRACT(MIN_VOLUME);
        }
    }

    m_stMixTime = m_LFO.StartVoice(&pArticulation->m_LFO,
        stStartTime, pModWheelIn, pPressureIn);

    STIME stMixTime = m_LFO2.StartVoice(&pArticulation->m_LFO2,
        stStartTime, pModWheelIn, pPressureIn);
    if (stMixTime < m_stMixTime)
    {
        m_stMixTime = stMixTime;
    }

    stMixTime = m_PitchEG.StartVoice(&pArticulation->m_PitchEG,
        stStartTime, nKey, nVelocity, 0);
    if (stMixTime < m_stMixTime)
    {
        m_stMixTime = stMixTime;
    }

    // Force attack to never be shorter than a millisecond.
    stMixTime = m_VolumeEG.StartVoice(&pArticulation->m_VolumeEG,
        stStartTime, nKey, nVelocity, pSynth->m_dwSampleRate/1000);
    if (stMixTime < m_stMixTime)
    {
        m_stMixTime = stMixTime;
    }

    if (m_stMixTime > pSynth->m_stMaxSpan)
    {
        m_stMixTime = pSynth->m_stMaxSpan;
    }

    m_Filter.StartVoice(&pArticulation->m_Filter,
        &m_LFO, &m_PitchEG, nKey, nVelocity);

    // Make sure we have a pointer to the wave ready:
    if ((pRegion->m_Sample.m_pWave == NULL) || (pRegion->m_Sample.m_pWave->m_pnWave == NULL))
    {
        return (FALSE);     // Do nothing if no sample.
    }

    m_DigitalAudio.StartVoice(pSynth,
                              &pRegion->m_Sample,
                              prPitch,
                              (long)nKey);

    m_pPitchBendIn = pPitchBendIn;
    m_pExpressionIn = pExpressionIn;
    m_pPanIn = pPanIn;
    m_pReverbSend = pReverbSend;
    m_pChorusSend = pChorusSend;
    m_CCutOffFreqIn = pCCutOffFreqIn;
    m_pVolumeIn = pVolumeIn;
    m_BusIds = *pBusIds;
    m_fNoteOn = TRUE;
    m_fTag = FALSE;
    m_fSustainOn = FALSE;
    m_stStartTime = stStartTime;
    m_stLastMix = stStartTime - 1;
    m_stStopTime = 0x7fffffffffffffff;
    m_stWaveStopTime = 0;

    //
    // Zero length attack,
    // be sure initial settings aren't missed....
    //
    if (m_stMixTime == 0)
    {
        PREL  prNewPitch;
        COEFF cfK, cfB1, cfB2;

        GetNewPitch(stStartTime, prNewPitch);
        GetNewCoeff(stStartTime, m_prLastCutOff, cfK, cfB1, cfB2);

        m_DigitalAudio.Mix(NULL,
                           0,
                           0,
                           0,
                           0,
                           NULL,
                           NULL,
                           prNewPitch + prBusPitchBend,
                           m_Filter.IsFiltered(),
                           cfK, cfB1, cfB2);
    }

    m_vrVolume = MAX_VOLUME;

    return (TRUE);
}

BOOL CVoice::StartWave(CSynth *pSynth,
                       CWaveArt *pWaveArt,
                       DWORD dwVoiceId,
                       STIME stStartTime,
                       CPitchBendIn * pPitchBendIn,
                       CExpressionIn * pExpressionIn,
                       CVolumeIn * pVolumeIn,
                       CPanIn * pPanIn,
                       CReverbIn * pReverbSend,
                       CChorusIn * pChorusSend,
                       CCutOffFreqIn * pCCutOffFreqIn,
                       CBusIds * pBusIds,
                       VREL vrVolume,
                       PREL prPitch,
                       SAMPLE_TIME stVoiceStart,
                       SAMPLE_TIME stLoopStart,
                       SAMPLE_TIME stLoopEnd
                       )
{
    m_pSynth = pSynth;

    DWORD dwFuncId = pWaveArt->m_WaveArtDl.ulBus;

    VREL vrVolumeReverb = vrVolume;
    VREL vrVolumeChorus = vrVolume;

    m_fIgnorePan = (BOOL)(DSBUSID_IS_SPKR_LOC(dwFuncId) && (pWaveArt->m_WaveArtDl.usOptions & F_WAVELINK_MULTICHANNEL));
    if (pBusIds->m_dwBusCount == 1)
    {
        DWORD dwFunctionID;
        if (m_pSynth->BusIDToFunctionID(pBusIds->m_dwBusIds[0], &dwFunctionID, NULL, NULL))
        {
            if (dwFunctionID == DSBUSID_LEFT)
            {
                m_fIgnorePan = TRUE;
            }
        }
    }

    for( DWORD i = 0; i < pBusIds->m_dwBusCount; i++ )
    {
        m_vrBaseVolume[i] = MIN_VOLUME;

        DWORD dwFunctionID;
        if (m_pSynth->BusIDToFunctionID(pBusIds->m_dwBusIds[i], &dwFunctionID, NULL, NULL))
        {
            // If this bus is a speaker location
            //
            if (DSBUSID_IS_SPKR_LOC(dwFunctionID))
            {
                if (pWaveArt->m_WaveArtDl.usOptions & F_WAVELINK_MULTICHANNEL)
                {
                    if (dwFuncId == dwFunctionID)
                    {
                        m_vrBaseVolume[i] = vrVolume;
                    }
                }
                else
                {
                    if (dwFunctionID == DSBUSID_LEFT || dwFunctionID == DSBUSID_RIGHT)
                    {
                        m_vrBaseVolume[i] = vrVolume;
                    }
                }
            }
            else switch (dwFunctionID)
            {
            case DSBUSID_REVERB_SEND:
                m_vrBaseVolume[i] = vrVolumeReverb;
                break;

            case DSBUSID_CHORUS_SEND:
                m_vrBaseVolume[i] = vrVolumeChorus;
                break;

            case DSBUSID_NULL:
                m_vrBaseVolume[i] = MIN_VOLUME;
                break;

            default:
                m_vrBaseVolume[i] = vrVolume;
            }

            m_vrLastVolume[i] = MIN_VOLUME;
            m_vfLastVolume[i] = m_DigitalAudio.VRELToVFRACT(MIN_VOLUME);
        }
    }

    // Initialize an envelope for wave playing
    //
    CSourceEG WaveVolumeEG;
    WaveVolumeEG.Init();
    WaveVolumeEG.m_pcSustain = 1000;
    // Force the envelope attack and release to be no smaller than 4ms. This ensures we won't get
    // clicks if we start and stop at non-zero crossings.
    m_stMixTime = m_VolumeEG.StartVoice(&WaveVolumeEG, stStartTime, 0, 0, pSynth->m_dwSampleRate/250);
    if (m_stMixTime > pSynth->m_stMaxSpan)
    {
        m_stMixTime = pSynth->m_stMaxSpan;
    }

    m_pPitchBendIn = pPitchBendIn;
    m_pExpressionIn = pExpressionIn;
    m_pPanIn = pPanIn;
    m_pReverbSend = pReverbSend;
    m_pChorusSend = pChorusSend;
    m_CCutOffFreqIn = pCCutOffFreqIn;
    m_pVolumeIn = pVolumeIn;
    m_BusIds = *pBusIds;
    m_fNoteOn = TRUE;
    m_fTag = FALSE;
    m_stStartTime = stStartTime;
    m_stLastMix = stStartTime - 1;
    m_stStopTime = 0x7fffffffffffffff;
    m_stWaveStopTime = 0;
    m_dwGroup = 0;
    m_lDefaultPan = 0;
    m_vrVolume = 0;
    m_fAllowOverlap = FALSE;
    m_fSustainOn = FALSE;
    m_dwVoiceId = dwVoiceId;

    m_LFO.Enable(FALSE);             // Disable LFO.
    m_LFO2.Enable(FALSE);            // Disable LFO2.
    m_PitchEG.Enable(FALSE);         // Disable Pitch Envelope.
    m_Filter.m_Source.m_prCutoff = 0x7FFF;

    m_DigitalAudio.StartWave(pSynth,
                             pWaveArt,
                             prPitch,
                             stVoiceStart,
                             stLoopStart,
                             stLoopEnd);

    return (TRUE);
}

SAMPLE_POSITION CVoice::GetCurrentPos()
{
    return m_DigitalAudio.GetCurrentPos();
}

void CVoice::ClearVoice()
{
    m_fInUse = FALSE;
    m_DigitalAudio.ClearVoice();
}

// return the volume delta at time <stTime>.
// volume is sum of volume envelope, LFO, expression, optionally the
// channel volume if we're allowing it to change, and optionally the current
// pan if we're allowing that to change.
// This will be added to the base volume calculated in CVoice::StartVoice().
inline void CVoice::GetNewVolume(STIME stTime, VREL& vrVolume, VREL& vrVolumeL, VREL& vrVolumeR, VREL& vrVolumeReverb, VREL& vrVolumeChorus)
{
    STIME stMixTime = m_stMixTime;

    //
    // the evelope volume is used by code that detects whether this note is off
    // and for voice stealing
    //
    m_vrVolume = m_VolumeEG.GetVolume(stTime, &stMixTime);
    if (stMixTime < m_stMixTime)
        m_stMixTime = stMixTime;

    vrVolume = m_vrVolume;
    vrVolume += m_LFO.GetVolume(stTime, &stMixTime);
    if (stMixTime < m_stMixTime)
        m_stMixTime = stMixTime;

    vrVolume += m_pExpressionIn->GetVolume(stTime);

    if (m_pSynth->m_fAllowVolumeChangeWhilePlayingNote)
        vrVolume += m_pVolumeIn->GetVolume(stTime);

    vrVolume += m_pSynth->m_vrGainAdjust;

    // handle pan here if allowing pan to vary after note starts
    vrVolumeL = vrVolume;
    vrVolumeR = vrVolume;
    if (m_pSynth->m_dwStereo && m_pSynth->m_fAllowPanWhilePlayingNote && !m_fIgnorePan)
    {
        // add current pan & instrument default pan
        LONG lPan;

        if (m_pPanIn)
        {
            lPan = m_pPanIn->GetPan(stTime) + m_lDefaultPan;
        }
        else
        {
            lPan = 63;
        }

        // don't go off either end....
        if (lPan < 0) lPan = 0;
        if (lPan > 127) lPan = 127;
        vrVolumeL += m_svrPanToVREL[127 - lPan];
        vrVolumeR += m_svrPanToVREL[lPan];
    }
    // Get Reverb Send volume
    vrVolumeReverb  = vrVolume + m_pReverbSend->GetVolume(stTime);
    // Get Chorus Send volume
    vrVolumeChorus  = vrVolume + m_pChorusSend->GetVolume(stTime);
}

// Returns the current pitch for time <stTime>.
// Pitch is the sum of the pitch LFO, the pitch envelope, and the current
// pitch bend.
inline void CVoice::GetNewPitch(STIME stTime, PREL& prPitch)
{
    STIME stMixTime = m_stMixTime;

    prPitch = m_LFO.GetPitch(stTime, &stMixTime);
    if (m_stMixTime > stMixTime) m_stMixTime = stMixTime;

    prPitch += m_LFO2.GetPitch(stTime, &stMixTime);
    if (m_stMixTime > stMixTime) m_stMixTime = stMixTime;

    prPitch += m_PitchEG.GetPitch(stTime, &stMixTime);
    if (m_stMixTime > stMixTime) m_stMixTime = stMixTime;

    prPitch += m_pPitchBendIn->GetPitch(stTime);
}

// Returns the current cutoff frequency for time <stTime>.
// cutoff frequency is the sum of the pitch LFO, the pitch envelope, and the current
// MIDI filter CC control.
inline void CVoice::GetNewCoeff(STIME stTime, PREL& prCutOff, COEFF& cfK, COEFF& cfB1, COEFF& cfB2)
{

    DWORD dwfreq;

    // returned frequency is in semitones, where 64 is the mid range
    dwfreq = m_CCutOffFreqIn->GetFrequency(stTime);
    prCutOff = (dwfreq - 64)*100;   // convert to PREL's

    m_Filter.GetCoeff(stTime, prCutOff, cfK, cfB1, cfB2);
}

DWORD CVoice::Mix(short **ppvBuffer,
                  DWORD dwBufferFlags,
                  DWORD dwLength,
                  STIME stStart,
                  STIME stEnd)

{
    BOOL   fInUse    = TRUE;
    BOOL   fFullMix   = TRUE;
    STIME  stEndMix   = stStart;
    STIME  stStartMix = m_stStartTime;
    COEFF  cfK, cfB1, cfB2;
    PREL   prPitch;
    PREL   prCutOff;
    VREL   vrVolume, vrVolumeL, vrVolumeR;
    VREL   vrVolumeReverb, vrVolumeChorus;
    VREL   vrMaxVolumeDelta;
    VFRACT vfNewVolume[MAX_DAUD_CHAN];
    VFRACT vfLastVolume[MAX_DAUD_CHAN];
    short  *ppsMixBuffers[MAX_DAUD_CHAN];

    if (stStartMix < stStart)
    {
        stStartMix = stStart;
    }

    if (m_stLastMix >= stEnd)
    {
        return (0);
    }

    if (m_stLastMix >= stStartMix)
    {
        stStartMix = m_stLastMix;
    }

    while (stStartMix < stEnd && fInUse)
    {
        stEndMix = stStartMix + m_stMixTime;
        if (stEndMix > stEnd)
        {
            stEndMix = stEnd;
        }

        m_stMixTime = m_pSynth->m_stMaxSpan;
        if ((m_stLastMix < m_stStopTime) && (m_stStopTime < stEnd))
        {
            if (m_stMixTime > (m_stStopTime - m_stLastMix))
            {
                m_stMixTime = m_stStopTime - m_stLastMix;
            }
        }

        //
        // Get the new pitch
        //
        GetNewPitch(stEndMix, prPitch);

        //
        // Get the new volume
        //
        GetNewVolume(stEndMix, vrVolume, vrVolumeL, vrVolumeR, vrVolumeReverb, vrVolumeChorus);

        //
        // Get the new filter coeficients
        //
        GetNewCoeff(stEndMix, prCutOff, cfK, cfB1, cfB2);

        //
        // Check to see if the volume is precievable, if not kill voice
        //
        if (m_VolumeEG.InRelease(stEndMix))
        {
            if (m_vrVolume < PERCEIVED_MIN_VOLUME) // End of release slope
            {
                // Breaking the loop ensures that the mixmulti functions don't mix any more samples
                // for looped wave Without this the mix engine will mix a few more samples for
                // looped waves resulting in a pop at the end of the wave.
                m_DigitalAudio.BreakLoop();
                fInUse = FALSE;
            }
        }

        vrMaxVolumeDelta = 0;
        vfNewVolume[0]   = 0;
        ppsMixBuffers[0] = NULL;
        DWORD dwMixBufferCount = 0;
        PREL prBusPitchBend = 0;  // This gets a pitch offset that is set by DSound in response to SetFrequency and Doppler commands.
                                 // When this is applied to multiple buses, only one of the values can be used, so we always give
                                 // preference to the buffer that has DSBUSID_DYNAMIC_0 for the functional id, since that
                                 // would most likely be a 3D sound effect.
        BOOL fDynamic = false;

        if (dwBufferFlags & BUFFERFLAG_MULTIBUFFER)
        {
            // Iterate through each bus id in the voice, assigning a sink bus to each one.
            for ( DWORD nBusID = 0; nBusID < m_BusIds.m_dwBusCount; nBusID++ )
            {
                DWORD dwFunctionalID;
                DWORD dwBusIndex;
                PREL prGetPitch;

                if (m_pSynth->BusIDToFunctionID(m_BusIds.m_dwBusIds[nBusID], &dwFunctionalID, &prGetPitch, &dwBusIndex))
                {
                    if (!fDynamic)
                    {
                        // If no previous bus was dynamic, get this value.
                        prBusPitchBend = prGetPitch;
                    }
                    // Default to original volume (before pan, reverb or chorus modifiers.)
                    VREL vrTemp = vrVolume;
                    // Replace for any of the other cases (left, right, reverb, chorus.)
                    if ( dwFunctionalID == DSBUSID_NULL )
                    {
                        continue;
                    }
                    if ( dwFunctionalID == DSBUSID_LEFT )
                    {
                        vrTemp = vrVolumeL;
                    }
                    if ( dwFunctionalID == DSBUSID_RIGHT )
                    {
                        vrTemp = vrVolumeR;
                    }
                    else if ( dwFunctionalID == DSBUSID_REVERB_SEND )
                    {
                        vrTemp = vrVolumeReverb;
                    }
                    else if ( dwFunctionalID == DSBUSID_CHORUS_SEND )
                    {
                        vrTemp = vrVolumeChorus;
                    }
                    else if ( dwFunctionalID == DSBUSID_DYNAMIC_0 )
                    {
                        fDynamic = true;
                    }

                    vrMaxVolumeDelta = max((long)vrMaxVolumeDelta, abs(vrTemp - m_vrLastVolume[nBusID]));
                    m_vrLastVolume[nBusID] = vrTemp;

                    vrTemp += m_vrBaseVolume[nBusID];
                    vfNewVolume[dwMixBufferCount]  = m_DigitalAudio.VRELToVFRACT(vrTemp);
                    vfLastVolume[dwMixBufferCount] = m_vfLastVolume[nBusID];
                    m_vfLastVolume[nBusID] = vfNewVolume[dwMixBufferCount];
                    ppsMixBuffers[dwMixBufferCount] = &ppvBuffer[dwBusIndex][(stStartMix - stStart)];
                    dwMixBufferCount++;
                }
            }
        }
        else
        {
            // This is the DX7 compatibility case.
            vrMaxVolumeDelta = max((long)vrMaxVolumeDelta, abs(vrVolumeL - m_vrLastVolume[0]));
            m_vrLastVolume[0] = vrVolumeL;
            vfNewVolume[0]  = m_DigitalAudio.VRELToVFRACT(m_vrBaseVolume[0] + vrVolumeL);
            vfLastVolume[0] = m_vfLastVolume[0];
            m_vfLastVolume[0] = vfNewVolume[0];
            dwMixBufferCount = 1;
            if ( dwBufferFlags & BUFFERFLAG_INTERLEAVED )   // Is this a stereo buffer?
            {
                vrMaxVolumeDelta = max((long)vrMaxVolumeDelta, abs(vrVolumeR - m_vrLastVolume[1]));
                m_vrLastVolume[1] = vrVolumeR;
                vfNewVolume[1]  = m_DigitalAudio.VRELToVFRACT(m_vrBaseVolume[1] + vrVolumeR);
                vfLastVolume[1] = m_vfLastVolume[1];
                m_vfLastVolume[1] = vfNewVolume[1];
                ppsMixBuffers[0] = &ppvBuffer[0][(stStartMix - stStart) << 1];
            }
            else    // Or mono?
            {
                ppsMixBuffers[0] = &ppvBuffer[0][(stStartMix - stStart)];
            }
        }
        // If dwMixBufferCount is 0, this indicates there is no buffer available to play into.
        // This is caused by a buffer being deactivated. Under such circumstances, the
        // voice should not continue playing, or it will hold until the buffer reactivates, which
        // doesn't make sense. So, set fInUse to FALSE.
        if (dwMixBufferCount)
        {
            DWORD dwIsFiltered = m_Filter.IsFiltered();
            if (dwIsFiltered)
            {
                vrMaxVolumeDelta = max((long)vrMaxVolumeDelta, abs(prCutOff - m_prLastCutOff));
                m_prLastCutOff = prCutOff;
            }


            //
            // note: mix will in some cases modify the pointers found ppsMixBuffers array
            //
            fFullMix = m_DigitalAudio.Mix(ppsMixBuffers,                    // Array of mix buffers
                                          dwMixBufferCount,                 // Number of mix buffers
                                          (dwBufferFlags & BUFFERFLAG_INTERLEAVED), // Are the buffers interleaved data?
                                          (DWORD) (stEndMix - stStartMix),  // Length to mix in Samples
                                          vrMaxVolumeDelta,                 //
                                          vfNewVolume,
                                          vfLastVolume,
                                          prPitch + prBusPitchBend,         // Pitch to play the sample too
                                          dwIsFiltered,         // Is the mix filtered
                                          cfK, cfB1, cfB2);
            stStartMix = stEndMix;
        }
        else
        {
            fInUse = FALSE;
        }
    }

    m_fInUse = fInUse && fFullMix;
    if (!m_fInUse)
    {
        ClearVoice();
        m_stStopTime = stEndMix;    // For measurement purposes.
    }

    m_stLastMix = stEndMix;

    return (dwLength);
}

