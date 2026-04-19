//      Copyright (c) 1996-1999 Microsoft Corporation
//      Mix.cpp
//      Mix engines for MSSynth

#ifdef DMSYNTH_MINIPORT
#include "common.h"
#define STR_MODULENAME "DMusicMix:"
#else
#include "simple.h"
#include <mmsystem.h>
#include "synth.h"
#endif

#pragma warning(disable : 4101 4102 4146)  

#ifdef _ALPHA_

extern "C" {
	int __ADAWI(short, short *);
};
#pragma intrinsic(__ADAWI)

#define ALPHA_OVERFLOW 2 
#define ALPHA_NEGATIVE 8

#else // !_ALPHA_
//  TODO -- overflow detection for ia64 (+ axp64?)
#endif // !_ALPHA_
#ifdef DMSYNTH_MINIPORT
#pragma code_seg("PAGE")
#endif // DMSYNTH_MINIPORT

DWORD CDigitalAudio::Mix8(short * pBuffer, 
						  DWORD dwLength, 
						  DWORD dwDeltaPeriod,
						  VFRACT vfDeltaLVolume, 
						  VFRACT vfDeltaRVolume,
					      VFRACT vfLastVolume[],
						  PFRACT pfDeltaPitch, 
						  PFRACT pfSampleLength, 
						  PFRACT pfLoopLength)
{
    DWORD dwI;
    DWORD dwPosition;
    long lM, lLM;
    DWORD dwIncDelta = dwDeltaPeriod;
    VFRACT dwFract;
    char * pcWave = (char *) m_pnWave;
    PFRACT pfSamplePos = m_pfLastSample;
    VFRACT vfLVolume = vfLastVolume[0];
    VFRACT vfRVolume = vfLastVolume[1];
    PFRACT pfPitch = m_pfLastPitch;
    PFRACT pfPFract = pfPitch << 8;
    VFRACT vfLVFract = vfLVolume << 8;  // Keep high res version around.
    VFRACT vfRVFract = vfRVolume << 8;  
	dwLength <<= 1;

#ifndef _X86_
    for (dwI = 0; dwI < dwLength; )
    {
        if (pfSamplePos >= pfSampleLength)
	    {	
	        if (pfLoopLength)
		    pfSamplePos -= pfLoopLength;
	        else
		    break;
	    }
        dwIncDelta--;
        if (!dwIncDelta)    
        {
            dwIncDelta = dwDeltaPeriod;
            pfPFract += pfDeltaPitch;
            pfPitch = pfPFract >> 8;
            vfLVFract += vfDeltaLVolume;
            vfLVolume = vfLVFract >> 8;
            vfRVFract += vfDeltaRVolume;
            vfRVolume = vfRVFract >> 8;
        }

        dwPosition = pfSamplePos >> 12;
        dwFract = pfSamplePos & 0xFFF;
        pfSamplePos += pfPitch;

        lLM = pcWave[dwPosition];
        lM = ((pcWave[dwPosition + 1] - lLM) * dwFract) >> 12;
        lM += lLM;
        lLM = lM;

        lLM *= vfLVolume;
        lLM >>= 5;         // Signal bumps up to 15 bits.
        lM *= vfRVolume;
        lM >>= 5;

#ifndef _X86_

#ifdef _ALPHA_
		int nBitmask;
		if( ALPHA_OVERFLOW & (nBitmask = __ADAWI( (short) lLM, &pBuffer[dwI] )) )  {
			if( ALPHA_NEGATIVE & nBitmask )  {
				pBuffer[dwI] = 0x7FFF;
			}
			else  pBuffer[dwI] = (short) 0x8000;
		}
		if( ALPHA_OVERFLOW & (nBitmask = __ADAWI( (short) lM, &pBuffer[dwI+1] )) )  {
			if( ALPHA_NEGATIVE & nBitmask )  {
				pBuffer[dwI+1] = 0x7FFF;
			}
			else  pBuffer[dwI+1] = (short) 0x8000;
		}
#else // !_ALPHA_
    // TODO -- overflow detection on ia64 (+ axp64?)
#endif // !_ALPHA_

#else // _X86_  (dead code)
      //  Keep this around so we can use it to generate new assembly code (see below...)
		pBuffer[dwI] += (short) lLM;

        _asm{jno no_oflowl}
        pBuffer[dwI] = 0x7fff;
        _asm{js  no_oflowl}
        pBuffer[dwI] = (short) 0x8000;
no_oflowl:	

		pBuffer[dwI+1] += (short) lM;

        _asm{jno no_oflowr}
        pBuffer[dwI+1] = 0x7fff;
        _asm{js  no_oflowr}
        pBuffer[dwI+1] = (short) 0x8000;
no_oflowr:
#endif // _X86_  (dead code)

		dwI += 2;
    }
#else // _X86_
	int i, a, b, c, total;
	short * pBuf = pBuffer + dwLength, *pBufX;
	dwI = - dwLength;

	_asm {

; 979  :     for (dwI = 0; dwI < dwLength; )

//	Induction variables.
	mov	edi, dwI
	mov	ebx, DWORD PTR pfSamplePos

// Previously set up.
	cmp	DWORD PTR dwLength, 0
	mov	edx, pfPFract

	mov	ecx, DWORD PTR pfPitch
	je	$L30539

$L30536:
	cmp	ebx, DWORD PTR pfSampleLength

; 981  :         if (pfSamplePos >= pfSampleLength)

	mov	esi, DWORD PTR dwIncDelta
	jge	SHORT $L30540_

$L30540:
; 987  : 	        else
; 988  : 		    break;
; 990  :         dwIncDelta--;

	dec	esi
	mov	DWORD PTR dwIncDelta, esi

; 991  :         if (!dwIncDelta)    

	je	SHORT $L30541_

$L30541:
// esi, edx, edi		esi == dwIncDelta

	mov	DWORD PTR i, 0

; 1010 : 	b = dwIncDelta;

// esi = b == dwIncDelta

; 1011 : 	c = (pfSampleLength - pfSamplePos) / pfPitch;
; 1009 : 	a = (dwLength - dwI) / 2;	// Remaining span.

	mov	edx, edi
	neg	edx
	shr	edx, 1		// edx = a

; 1017 : 	if (b < a && b < c)

	cmp	esi, edx
	jge	try_ax

	mov	eax, ecx
	imul	eax, esi
	add	eax, ebx

	cmp eax, DWORD PTR pfSampleLength 
	jge	try_c

; 1019 : 		i = b;

	cmp	esi, 3
	jl	got_it

	mov	DWORD PTR i, esi
	jmp	SHORT got_it

; 1013 : 	if (a < b && a < c)

try_a:

	cmp	edx, esi
	jge	try_c
try_ax:
	mov	eax, edx
	imul	eax, ecx
	add	eax, ebx

	cmp eax, DWORD PTR pfSampleLength
	jge	try_c

; 1015 : 		i = a;

	cmp	edx, 3
	jl	got_it

	mov	DWORD PTR i, edx
	jmp	SHORT got_it

; 1021 : 	else if (c < a && c < b)
try_c:

    push	edx
	mov	eax, DWORD PTR pfSampleLength
	sub	eax, ebx
	cdq
	idiv	ecx		// eax == c
	pop	edx

    cmp	eax, edx
	jge	got_it
try_cx:
	cmp	eax, esi
	jge	got_it

; 1023 : 		i = c;

	cmp	eax, 3
	jl	$L30543	

	mov DWORD PTR i, eax

got_it:
	mov	edx, DWORD PTR i
	mov	eax, DWORD PTR pBuf

	dec	edx
	jl	$L30543

	sub	DWORD PTR dwIncDelta, edx

; 1093 :     return (dwI >> 1);
; 1094 : }

	lea	edx, [edx*2+2]			// Current span.
	lea	eax, [eax+edi*2]		// Starting position.

	add	edi, edx				// Remaining span.
	lea	eax, [eax+edx*2]		// New ending position.

	push	edi
	mov	edi, edx				// Current span.

	mov	DWORD PTR pBufX, eax
	neg	edi

$L30797:
; 1005 : 			do
; 1010 : 				dwPosition = pfSamplePos >> 12;
; 1011 : 				dwFract = pfSamplePos & 0xFFF;

	mov	edx, ebx
	mov	esi, ebx

	add	ebx, ecx
	mov	ecx, DWORD PTR pcWave

; 1012 : 				pfSamplePos += pfPitch;

	sar	edx, 12					; 0000000cH
	and	esi, 4095				; 00000fffH

; 1013 : 
; 1014 : 				lLM = (long) pcWave[dwPosition];

	movsx	eax, BYTE PTR [ecx+edx]

; 1015 : 				lM = ((pcWave[dwPosition+1] - lLM) * dwFract);
; 1016 : 				lM >>= 12;
; 1017 : 				lM += lLM;

	movsx	edx, BYTE PTR [ecx+edx+1]

; 1018 : 				lLM = lM;
; 1019 : 				lLM *= vfLVolume;
; 1020 : 				lLM >>= 5;         // Signal bumps up to 15 bits.
; 1022 : 				pBuffer[dwI] += (short) lLM;
; 1028 : 						lM *= vfRVolume;
; 1029 : 						lM >>= 5;
; 1030 : 				pBuffer[dwI+1] += (short) lM;
; 1036 : 
; 1037 : 				dwI += 2;
	sub	edx, eax

	imul	edx, esi

	sar	edx, 12					; 0000000cH
	mov	esi, DWORD PTR vfLVolume

	add	edx, eax

	imul	esi, edx

	sar	esi, 5					; 00000005H
	mov	eax, DWORD PTR pBufX

	add	WORD PTR [eax+edi*2], si
	mov	esi, DWORD PTR vfRVolume

	jo	overflow_lx
no_oflowlx:

	imul	esi, edx

; 1038 : 			} while (--dwIncDelta);

	sar	esi, 5					; 00000005H
	mov	ecx, DWORD PTR pfPitch

	add	WORD PTR [eax+edi*2+2], si
	jo	overflow_rx

no_oflowrx:

	add	edi, 2
	jne	SHORT $L30797

	pop	edi

; 1039 : 			++dwIncDelta;
; 1040 : 			continue;

	mov	edx, DWORD PTR pfPFract
	cmp	edi, 0

	jl	SHORT $L30536
	jmp	SHORT $L30539

$L30540_:

; 982  : 	    {	
; 983  : 	        if (pfLoopLength)

	cmp	DWORD PTR pfLoopLength, 0
	je	$L30539

; 984  : 			{
; 985  : 				pfSamplePos -= pfLoopLength;

	sub	ebx, DWORD PTR pfLoopLength
	jmp	$L30540

$L30541_:
; 994  :             pfPFract += pfDeltaPitch;

	mov	ecx, DWORD PTR pfDeltaPitch
	mov	esi, DWORD PTR vfDeltaLVolume

	add	ecx, edx
	mov	edx, DWORD PTR vfLVFract

; 995  :             pfPitch = pfPFract >> 8;
; 996  :             vfLVFract += vfDeltaLVolume;

	mov	DWORD PTR pfPFract, ecx
	add	edx, esi

; 997  :             vfLVolume = vfLVFract >> 8;
; 998  :             vfRVFract += vfDeltaRVolume;

	sar	ecx, 8
	mov	DWORD PTR vfLVFract, edx
	
	sar	edx, 8
	mov	esi, DWORD PTR vfDeltaRVolume

	mov	DWORD PTR vfLVolume, edx
	mov	edx, DWORD PTR vfRVFract

	add	edx, esi
	mov	DWORD PTR pfPitch, ecx

	mov	DWORD PTR vfRVFract, edx
	mov	esi, DWORD PTR dwDeltaPeriod

; 999  :             vfRVolume = vfRVFract >> 8;

	sar	edx, 8
	mov	DWORD PTR dwIncDelta, esi

; 993  :             dwIncDelta = dwDeltaPeriod;

	mov	DWORD PTR vfRVolume, edx
	jmp	$L30541

// Handle truncation.

overflow_l:
	mov	WORD PTR [eax+edi*2], 0x7fff
	js	no_oflowl
	mov	WORD PTR [eax+edi*2], 0x8000
	jmp no_oflowl

overflow_r:
	mov	WORD PTR [eax+edi*2+2], 0x7fff
	js	no_oflowr
	mov	WORD PTR [eax+edi*2+2], 0x8000
	jmp	no_oflowr

overflow_lx:
	mov	WORD PTR [eax+edi*2], 0x7fff
	js	no_oflowlx
	mov	WORD PTR [eax+edi*2], 0x8000
	jmp	no_oflowlx

overflow_rx:
	mov	WORD PTR [eax+edi*2+2], 0x7fff
	js	no_oflowrx
	mov	WORD PTR [eax+edi*2+2], 0x8000
	jmp	no_oflowrx

$L30543:
; 1041 : 		}
; 1044 :         dwPosition = pfSamplePos >> 12;

	mov	edx, ebx
	mov	ecx, DWORD PTR pfPitch

; 1045 :         dwFract = pfSamplePos & 0xFFF;

	sar	edx, 12					; 0000000cH
	mov	esi, ebx

	add	ebx, ecx
	and	esi, 4095				; 00000fffH

; 1046 :         pfSamplePos += pfPitch;

	mov	ecx, DWORD PTR pcWave

; 1047 : 
; 1048 :         lLM = (long) pcWave[dwPosition];

	movsx	eax, BYTE PTR [ecx+edx]

; 1049 :         lM = ((pcWave[dwPosition+1] - lLM) * dwFract);
; 1050 :         lM >>= 12;
; 1051 :         lM += lLM;

	movsx	edx, BYTE PTR [ecx+edx+1]

    sub	edx, eax
	
    imul	edx, esi

; 1052 :         lLM = lM;
; 1053 :         lLM *= vfLVolume;
; 1054 :         lLM >>= 5;         // Signal bumps up to 15 bits.

	sar	edx, 12					; 0000000cH
	mov	esi, DWORD PTR vfLVolume

	add	edx, eax

; 1072 : 		pBuffer[dwI] += (short) lLM;

	imul	esi, edx

	sar	esi, 5					; 00000005H
	mov	eax, DWORD PTR pBuf

	add	WORD PTR [eax+edi*2], si
	mov	esi, DWORD PTR vfRVolume

	jo	overflow_l
no_oflowl:

; 1078 :         lM *= vfRVolume;
; 1079 : 		lM >>= 5;

	imul	esi, edx

; 1080 : 		pBuffer[dwI+1] += (short) lM;
; 1085 : no_oflowr:
; 1087 : 		dwI += 2;

	sar	esi, 5					; 00000005H
	mov	ecx, DWORD PTR pfPitch

	add	WORD PTR [eax+edi*2+2], si
	mov	edx, DWORD PTR pfPFract

	jo	overflow_r

no_oflowr:
; 978  : 
; 979  :     for (dwI = 0; dwI < dwLength; )

	add	edi, 2
	jl $L30536

$L30539:
	mov DWORD PTR dwI, edi
	mov DWORD PTR pfSamplePos, ebx
}

	dwI += dwLength;

#endif // _X86_

    vfLastVolume[0] = vfLVolume;
    vfLastVolume[1] = vfRVolume;

    m_pfLastPitch = pfPitch;
    m_pfLastSample = pfSamplePos;
    return (dwI >> 1);
}

#ifdef ORG_MONO_MIXER
DWORD CDigitalAudio::MixMono8(short * pBuffer, 
							  DWORD dwLength,
							  DWORD dwDeltaPeriod,
							  VFRACT vfDeltaVolume,
							  VFRACT vfLastVolume[],
							  PFRACT pfDeltaPitch, 
							  PFRACT pfSampleLength, 
							  PFRACT pfLoopLength)
{
    DWORD dwI;
    DWORD dwPosition;
    long lM;
    DWORD dwIncDelta = dwDeltaPeriod;
    VFRACT dwFract;
    char * pcWave = (char *) m_pnWave;
    PFRACT pfSamplePos = m_pfLastSample;
    VFRACT vfVolume = vfLastVolume[0];
    PFRACT pfPitch = m_pfLastPitch;
    PFRACT pfPFract = pfPitch << 8;
    VFRACT vfVFract = vfVolume << 8;  // Keep high res version around. 

#ifndef _X86_
    for (dwI = 0; dwI < dwLength; )
    {
        if (pfSamplePos >= pfSampleLength)
	    {	
	        if (pfLoopLength)
		    pfSamplePos -= pfLoopLength;
	        else
		    break;
	    }
        dwIncDelta--;
        if (!dwIncDelta) 
        {
            dwIncDelta = dwDeltaPeriod;
            pfPFract += pfDeltaPitch;
            pfPitch = pfPFract >> 8;
            vfVFract += vfDeltaVolume;
            vfVolume = vfVFract >> 8;
        }

	    dwPosition = pfSamplePos >> 12;
	    dwFract = pfSamplePos & 0xFFF;
		pfSamplePos += pfPitch;

	    lM = pcWave[dwPosition];
	    lM += ((pcWave[dwPosition + 1] - lM) * dwFract) >> 12;
		lM *= vfVolume;
		lM >>= 5;

#ifndef _X86_
#ifdef _ALPHA_
		int nBitmask;
		if( ALPHA_OVERFLOW & (nBitmask = __ADAWI( (short) lM, &pBuffer[dwI] )) )  {
			if( ALPHA_NEGATIVE & nBitmask )  {
				pBuffer[dwI] = 0x7FFF;
			}
			else  pBuffer[dwI] = (short) 0x8000;
		}
#else // !_ALPHA_
    //  TODO -- overflow code on ia64 (+ axp64?)
#endif // !_ALPHA_

#else // _X86_  (dead code)
      // Keep this around so we can use it to generate new assembly code (see below...)
		pBuffer[dwI] += (short) lM;
        _asm{jno no_oflow}
        pBuffer[dwI] = 0x7fff;
        _asm{js  no_oflow}
        pBuffer[dwI] = (short) 0x8000;
no_oflow:
#endif  // _X86_  (dead code)
		dwI++;
    }
#else // _X86_
	int i, a, b, c, total;
	short * pBuf = pBuffer + dwLength, *pBufX;
	dwI = - dwLength;

	_asm {

; 979  :     for (dwI = 0; dwI < dwLength; )

//	Induction variables.
	mov	edi, dwI
	mov	ebx, DWORD PTR pfSamplePos

// Previously set up.
	cmp	DWORD PTR dwLength, 0
	mov	edx, pfPFract

	mov	ecx, DWORD PTR pfPitch
	je	$L30539

$L30536:
	cmp	ebx, DWORD PTR pfSampleLength

; 981  :         if (pfSamplePos >= pfSampleLength)

	mov	esi, DWORD PTR dwIncDelta
	jge	SHORT $L30540_

$L30540:

; 987  : 	        else
; 988  : 		    break;
; 990  :         dwIncDelta--;

	dec	esi
	mov	DWORD PTR dwIncDelta, esi

; 991  :         if (!dwIncDelta)    

	je	SHORT $L30541_

$L30541:
// esi, edx, edi		esi == dwIncDelta

	mov	DWORD PTR i, 0

; 1010 : 	b = dwIncDelta;
// esi = b == dwIncDelta

; 1011 : 	c = (pfSampleLength - pfSamplePos) / pfPitch;

; 1009 : 	a = dwLength - dwI;	// Remaining span.

	mov	edx, edi
	neg	edx

; 1017 : 	if (b < a && b < c)

	cmp	esi, edx
	jge	try_ax

	mov	eax, ecx
	imul	eax, esi
	add	eax, ebx

	cmp eax, DWORD PTR pfSampleLength 
	jge	try_c

; 1019 : 		i = b;

	cmp	esi, 3
	jl	got_it

	mov	DWORD PTR i, esi
	jmp	SHORT got_it

; 1013 : 	if (a < b && a < c)

try_a:

	cmp	edx, esi
	jge	try_c
try_ax:
	mov	eax, edx
	imul	eax, ecx
	add	eax, ebx

	cmp eax, DWORD PTR pfSampleLength
	jge	try_c

; 1015 : 		i = a;

	cmp	edx, 3
	jl	got_it

	mov	DWORD PTR i, edx
	jmp	SHORT got_it

; 1021 : 	else if (c < a && c < b)
try_c:

    push	edx
	mov	eax, DWORD PTR pfSampleLength
	sub	eax, ebx
	cdq
	idiv	ecx		// eax == c
	pop	edx

    cmp	eax, edx
	jge	got_it
try_cx:
	cmp	eax, esi
	jge	got_it

; 1023 : 		i = c;

	cmp	eax, 3
	jl	$L30543

	mov DWORD PTR i, eax

got_it:
	mov	edx, DWORD PTR i
	mov	eax, DWORD PTR pBuf

	dec	edx
	jl	$L30543

	sub	DWORD PTR dwIncDelta, edx

; 1093 :     return (dwI);
; 1094 : }

	lea	edx, [edx+1]			// Current span.
	lea	eax, [eax+edi*2]		// Starting position.

	add	edi, edx				// Remaining span.
	lea	eax, [eax+edx*2]		// New ending position.

	push	edi
	mov	edi, edx				// Current span.

	mov	DWORD PTR pBufX, eax
	neg	edi

$L30797:
		
; 1005 : 			do
; 1010 : 				dwPosition = pfSamplePos >> 12;
; 1011 : 				dwFract = pfSamplePos & 0xFFF;

	mov	edx, ebx
	mov	esi, ebx

	add	ebx, ecx
	mov	ecx, DWORD PTR pcWave

; 1012 : 				pfSamplePos += pfPitch;

	sar	edx, 12					; 0000000cH
	and	esi, 4095				; 00000fffH

; 1013 : 
; 1014 : 				lLM = (long) pcWave[dwPosition];

	movsx	eax, BYTE PTR [ecx+edx]

; 1015 : 				lM = ((pcWave[dwPosition+1] - lLM) * dwFract);
; 1016 : 				lM >>= 12;
; 1017 : 				lM += lLM;

	movsx	edx, BYTE PTR [ecx+edx+1]

	sub	edx, eax

; 1018 : 				lLM = lM;
; 1019 : 				lLM *= vfLVolume;
; 1020 : 				lLM >>= 5;         // Signal bumps up to 15 bits.
; 1022 : 				pBuffer[dwI] += (short) lLM;
; 1027 : no_oflowx:	
; 1037 : 				++dwI;

	imul	edx, esi

	sar	edx, 12					; 0000000cH
	mov	esi, DWORD PTR vfVolume

	mov	ecx, DWORD PTR pfPitch
	add	edx, eax

	imul	esi, edx

	sar	esi, 5					; 00000005H
	mov	eax, DWORD PTR pBufX

	add	WORD PTR [eax+edi*2], si
	jo	overflow_x

no_oflowx:

	inc	edi
	jne	SHORT $L30797

	pop	edi

; 1039 : 			++dwIncDelta;
; 1040 : 			continue;

	mov	edx, DWORD PTR pfPFract
	cmp	edi, 0

	jl	SHORT $L30536
	jmp	SHORT $L30539

$L30540_:
; 982  : 	    {	
; 983  : 	        if (pfLoopLength)

	cmp	DWORD PTR pfLoopLength, 0
	je	$L30539

; 984  : 			{
; 985  : 				pfSamplePos -= pfLoopLength;

	sub	ebx, DWORD PTR pfLoopLength
	jmp	$L30540

$L30541_:
; 994  :             pfPFract += pfDeltaPitch;

	mov	ecx, DWORD PTR pfDeltaPitch
	mov	esi, DWORD PTR vfDeltaVolume

	add	ecx, edx
	mov	edx, DWORD PTR vfVFract

; 995  :             pfPitch = pfPFract >> 8;
; 996  :             vfVFract += vfDeltaVolume;

	mov	DWORD PTR pfPFract, ecx
	add	edx, esi

; 997  :             vfLVolume = vfLVFract >> 8;

	sar	ecx, 8
	mov	DWORD PTR vfVFract, edx
	
	sar	edx, 8
	mov	esi, DWORD PTR dwDeltaPeriod


	mov	DWORD PTR vfVolume, edx
	mov	DWORD PTR pfPitch, ecx


	mov	DWORD PTR dwIncDelta, esi

; 993  :             dwIncDelta = dwDeltaPeriod;

	jmp	$L30541

// Handle truncation.

overflow_:
	mov	WORD PTR [eax+edi*2], 0x7fff
	js	no_oflow
	mov	WORD PTR [eax+edi*2], 0x8000
	jmp no_oflow

overflow_x:
	mov	WORD PTR [eax+edi*2], 0x7fff
	js	no_oflowx
	mov	WORD PTR [eax+edi*2], 0x8000
	jmp	no_oflowx

$L30543:
; 1044 :         dwPosition = pfSamplePos >> 12;

	mov	edx, ebx
	mov	ecx, DWORD PTR pfPitch

; 1045 :         dwFract = pfSamplePos & 0xFFF;

	sar	edx, 12					; 0000000cH
	mov	esi, ebx

	add	ebx, ecx
	and	esi, 4095				; 00000fffH

; 1046 :         pfSamplePos += pfPitch;

	mov	ecx, DWORD PTR pcWave

; 1047 : 
; 1048 :         lLM = (long) pcWave[dwPosition];

	movsx	eax, BYTE PTR [ecx+edx]

; 1049 :         lM = ((pcWave[dwPosition+1] - lLM) * dwFract);
; 1050 :         lM >>= 12;
; 1051 :         lM += lLM;

	movsx	edx, BYTE PTR [ecx+edx+1]

	sub	edx, eax

	imul	edx, esi

; 1052 :         lLM = lM;
; 1053 :         lLM *= vfLVolume;
; 1054 :         lLM >>= 5;         // Signal bumps up to 15 bits.

	sar	edx, 12					; 0000000cH
	mov	esi, DWORD PTR vfVolume

	add	edx, eax

; 1072 : 		pBuffer[dwI] += (short) lLM;

	imul	esi, edx

	sar	esi, 5					; 00000005H
	mov	eax, DWORD PTR pBuf

	add	WORD PTR [eax+edi*2], si
	jo	overflow_
no_oflow:
	inc	edi
	mov	edx, DWORD PTR pfPFract

; 979  :     for (dwI = 0; dwI < dwLength; )

	mov	ecx, DWORD PTR pfPitch
	jl $L30536

$L30539:
	mov DWORD PTR dwI, edi
	mov DWORD PTR pfSamplePos, ebx
}

	dwI += dwLength;

#endif // _X86_

    vfLastVolume[0] = vfVolume;
    vfLastVolume[1] = vfVolume; // !!! is this right?
    m_pfLastPitch = pfPitch;
    m_pfLastSample = pfSamplePos;
    return (dwI);
}
#endif

DWORD CDigitalAudio::Mix8Filter(short * pBuffer, 
                            DWORD dwLength, 
                            DWORD dwDeltaPeriod,
                            VFRACT vfDeltaLVolume, 
                            VFRACT vfDeltaRVolume,
                            VFRACT vfLastVolume[],
                            PFRACT pfDeltaPitch, 
                            PFRACT pfSampleLength, 
                            PFRACT pfLoopLength,
                            COEFF cfdK,
                            COEFF cfdB1,
                            COEFF cfdB2)
{
    DWORD dwI;
    DWORD dwPosition;
    long lA;
    long lM;
    DWORD dwIncDelta = dwDeltaPeriod;
    VFRACT dwFract;
    char * pcWave = (char *) m_pnWave;
    PFRACT pfSamplePos = m_pfLastSample;
    VFRACT vfLVolume = vfLastVolume[0];
    VFRACT vfRVolume = vfLastVolume[1];
    PFRACT pfPitch = m_pfLastPitch;
    PFRACT pfPFract = pfPitch << 8;
    VFRACT vfLVFract = vfLVolume << 8;  // Keep high res version around.
    VFRACT vfRVFract = vfRVolume << 8; 
    COEFF cfK  = m_cfLastK;
    COEFF cfB1 = m_cfLastB1;
    COEFF cfB2 = m_cfLastB2;
	dwLength <<= 1;

    for (dwI = 0; dwI < dwLength; )
    {
        if (pfSamplePos >= pfSampleLength)
	    {	
	        if (pfLoopLength)
			{
				pfSamplePos -= pfLoopLength;
			}
	        else
		    break;
	    }
        dwIncDelta--;
        if (!dwIncDelta)    
        {
            dwIncDelta = dwDeltaPeriod;
            pfPFract += pfDeltaPitch;
            pfPitch = pfPFract >> 8;
            vfLVFract += vfDeltaLVolume;
            vfLVolume = vfLVFract >> 8;
            vfRVFract += vfDeltaRVolume;
            vfRVolume = vfRVFract >> 8;
            cfK += cfdK;
            cfB1 += cfdB1;
            cfB2 += cfdB2;
        }
        dwPosition = pfSamplePos >> 12;
        dwFract = pfSamplePos & 0xFFF;
        pfSamplePos += pfPitch;

        lA = (long) pcWave[dwPosition];
        lM = ((pcWave[dwPosition+1] - lA) * dwFract);
        lM >>= 12;
        lM += lA;
        lM <<= 8; // Source was 8 bits, so convert to 16.

        // Filter
        //
		// z = k*s - b1*z1 - b2*b2
		// We store the negative of b1 in the table, so we flip the sign again by
		// adding here
		//
        lM =
              MulDiv(lM, cfK, (1 << 30))
            + MulDiv(m_lPrevSample, cfB1, (1 << 30))
            - MulDiv(m_lPrevPrevSample, cfB2, (1 << 30));

        m_lPrevPrevSample = m_lPrevSample;
        m_lPrevSample = lM;

        lA = lM;

        lA *= vfLVolume;
        lA >>= 15;         // Signal bumps up to 15 bits.
		lM *= vfRVolume;
		lM >>= 15;
        //  TODO -- overflow detection on ia64 (+ axp64?)
#ifdef _X86_
        //  Keep this around so we can use it to generate new assembly code (see below...)
		pBuffer[dwI] += (short) lA;

        _asm{jno no_oflowl}
        pBuffer[dwI] = 0x7fff;
        _asm{js  no_oflowl}
        pBuffer[dwI] = (short) 0x8000;
no_oflowl:	

		pBuffer[dwI+1] += (short) lM;

        _asm{jno no_oflowr}
        pBuffer[dwI+1] = 0x7fff;
        _asm{js  no_oflowr}
        pBuffer[dwI+1] = (short) 0x8000;
no_oflowr:
#endif // _X86_
		dwI += 2;
    }

    vfLastVolume[0] = vfLVolume;
    vfLastVolume[1] = vfRVolume;
    m_pfLastPitch = pfPitch;
    m_pfLastSample = pfSamplePos;
    m_cfLastK = cfK;
	m_cfLastB1 = cfB1;
	m_cfLastB2 = cfB2;
    return (dwI >> 1);
}

DWORD CDigitalAudio::Mix16Filter(short * pBuffer, 
                            DWORD dwLength, 
                            DWORD dwDeltaPeriod,
                            VFRACT vfDeltaLVolume, 
                            VFRACT vfDeltaRVolume,
                            VFRACT vfLastVolume[],
                            PFRACT pfDeltaPitch, 
                            PFRACT pfSampleLength, 
                            PFRACT pfLoopLength,
                            COEFF cfdK,
                            COEFF cfdB1,
                            COEFF cfdB2)
{
    DWORD dwI;
    DWORD dwPosition;
    long lA;
    long lM;
    DWORD dwIncDelta = dwDeltaPeriod;
    VFRACT dwFract;
    short * pcWave = m_pnWave;
    PFRACT pfSamplePos = m_pfLastSample;
    VFRACT vfLVolume = vfLastVolume[0];
    VFRACT vfRVolume = vfLastVolume[1];
    PFRACT pfPitch = m_pfLastPitch;
    PFRACT pfPFract = pfPitch << 8;
    VFRACT vfLVFract = vfLVolume << 8;  // Keep high res version around.
    VFRACT vfRVFract = vfRVolume << 8; 
    COEFF cfK  = m_cfLastK;
    COEFF cfB1 = m_cfLastB1;
    COEFF cfB2 = m_cfLastB2;
	dwLength <<= 1;

    for (dwI = 0; dwI < dwLength; )
    {
        if (pfSamplePos >= pfSampleLength)
	    {	
	        if (pfLoopLength)
			{
				pfSamplePos -= pfLoopLength;
			}
	        else
		    break;
	    }
        dwIncDelta--;
        if (!dwIncDelta)    
        {
            dwIncDelta = dwDeltaPeriod;
            pfPFract += pfDeltaPitch;
            pfPitch = pfPFract >> 8;
            vfLVFract += vfDeltaLVolume;
            vfLVolume = vfLVFract >> 8;
            vfRVFract += vfDeltaRVolume;
            vfRVolume = vfRVFract >> 8;
            cfK += cfdK;
            cfB1 += cfdB1;
            cfB2 += cfdB2;
        }
        dwPosition = pfSamplePos >> 12;
        dwFract = pfSamplePos & 0xFFF;
        pfSamplePos += pfPitch;

        lA = (long) pcWave[dwPosition];
        lM = ((pcWave[dwPosition+1] - lA) * dwFract);
        lM >>= 12;
        lM += lA;

        // Filter
        //
		// z = k*s - b1*z1 - b2*b2
		// We store the negative of b1 in the table, so we flip the sign again by
		// adding here
		//
        lM =
              MulDiv(lM, cfK, (1 << 30))
            + MulDiv(m_lPrevSample, cfB1, (1 << 30))
            - MulDiv(m_lPrevPrevSample, cfB2, (1 << 30));

        m_lPrevPrevSample = m_lPrevSample;
        m_lPrevSample = lM;

        lA = lM;

        lA *= vfLVolume;
        lA >>= 15;         // Signal bumps up to 15 bits.
		lM *= vfRVolume;
		lM >>= 15;
        //  TODO -- overflow detection on ia64 (+ axp64?)
#ifdef _X86_
        //  Keep this around so we can use it to generate new assembly code (see below...)
		pBuffer[dwI] += (short) lA;

        _asm{jno no_oflowl}
        pBuffer[dwI] = 0x7fff;
        _asm{js  no_oflowl}
        pBuffer[dwI] = (short) 0x8000;
no_oflowl:	

		pBuffer[dwI+1] += (short) lM;

        _asm{jno no_oflowr}
        pBuffer[dwI+1] = 0x7fff;
        _asm{js  no_oflowr}
        pBuffer[dwI+1] = (short) 0x8000;
no_oflowr:
#endif
		dwI += 2;
    }

    vfLastVolume[0] = vfLVolume;
    vfLastVolume[1] = vfRVolume;
    m_pfLastPitch = pfPitch;
    m_pfLastSample = pfSamplePos;
    m_cfLastK = cfK;
	m_cfLastB1 = cfB1;
	m_cfLastB2 = cfB2;
    return (dwI >> 1);
}



DWORD CDigitalAudio::Mix16(short * pBuffer, 
						   DWORD dwLength, 
						   DWORD dwDeltaPeriod,
						   VFRACT vfDeltaLVolume, 
						   VFRACT vfDeltaRVolume,
						   VFRACT vfLastVolume[],
						   PFRACT pfDeltaPitch, 
						   PFRACT pfSampleLength, 
						   PFRACT pfLoopLength)
{
    DWORD dwI;
    DWORD dwPosition;
    long lA;
    long lM;
    DWORD dwIncDelta = dwDeltaPeriod;
    VFRACT dwFract;
    short * pcWave = m_pnWave;
    PFRACT pfSamplePos = m_pfLastSample;
    VFRACT vfLVolume = vfLastVolume[0];
    VFRACT vfRVolume = vfLastVolume[1];
    PFRACT pfPitch = m_pfLastPitch;
    PFRACT pfPFract = pfPitch << 8;
    VFRACT vfLVFract = vfLVolume << 8;  // Keep high res version around.
    VFRACT vfRVFract = vfRVolume << 8; 
	dwLength <<= 1;

	static int _a = 0, _b = 0, _c = 0;

#ifndef _X86_
    for (dwI = 0; dwI < dwLength; )
    {
        if (pfSamplePos >= pfSampleLength)
	    {	
	        if (pfLoopLength)
			{
				pfSamplePos -= pfLoopLength;
			}
	        else
		    break;
	    }
        dwIncDelta--;
        if (!dwIncDelta)    
        {
            dwIncDelta = dwDeltaPeriod;
            pfPFract += pfDeltaPitch;
            pfPitch = pfPFract >> 8;
            vfLVFract += vfDeltaLVolume;
            vfLVolume = vfLVFract >> 8;
            vfRVFract += vfDeltaRVolume;
            vfRVolume = vfRVFract >> 8;
        }
        dwPosition = pfSamplePos >> 12;
        dwFract = pfSamplePos & 0xFFF;
        pfSamplePos += pfPitch;

        lA = (long) pcWave[dwPosition];
        lM = ((pcWave[dwPosition+1] - lA) * dwFract);
        lM >>= 12;
        lM += lA;
        lA = lM;
        lA *= vfLVolume;
        lA >>= 13;         // Signal bumps up to 15 bits.
		lM *= vfRVolume;
		lM >>= 13;
#ifndef _X86_
#ifdef _ALPHA_
		int nBitmask;
		if( ALPHA_OVERFLOW & (nBitmask = __ADAWI( (short) lA, &pBuffer[dwI] )) )  {
			if( ALPHA_NEGATIVE & nBitmask )  {
				pBuffer[dwI] = 0x7FFF;
			}
			else  pBuffer[dwI] = (short) 0x8000;
		}
		if( ALPHA_OVERFLOW & (nBitmask = __ADAWI( (short) lM, &pBuffer[dwI+1] )) )  {
			if( ALPHA_NEGATIVE & nBitmask )  {
				pBuffer[dwI+1] = 0x7FFF;
			}
			else  pBuffer[dwI+1] = (short) 0x8000;
		}
#else // !_ALPHA_
    //  TODO -- overflow detection on ia64 (+ axp64?)
#endif // !_ALPHA_
#else // _X86_  (dead code)
      //  Keep this around so we can use it to generate new assembly code (see below...)
		pBuffer[dwI] += (short) lA;

        _asm{jno no_oflowl}
        pBuffer[dwI] = 0x7fff;
        _asm{js  no_oflowl}
        pBuffer[dwI] = (short) 0x8000;
no_oflowl:	

		pBuffer[dwI+1] += (short) lM;

        _asm{jno no_oflowr}
        pBuffer[dwI+1] = 0x7fff;
        _asm{js  no_oflowr}
        pBuffer[dwI+1] = (short) 0x8000;
no_oflowr:

#endif // _X86_  (dead code)
		dwI += 2;
    }
#else // _X86_
	int i, a, b, c, total;
	short * pBuf = pBuffer + dwLength, *pBufX;
	dwI = - dwLength;

	_asm {

; 979  :     for (dwI = 0; dwI < dwLength; )

//	Induction variables.
	mov	edi, dwI
	mov	ebx, DWORD PTR pfSamplePos

// Previously set up.
	cmp	DWORD PTR dwLength, 0
	mov	edx, pfPFract

	mov	ecx, DWORD PTR pfPitch
	je	$L30539

$L30536:
	cmp	ebx, DWORD PTR pfSampleLength

; 981  :         if (pfSamplePos >= pfSampleLength)

	mov	esi, DWORD PTR dwIncDelta
	jge	SHORT $L30540_

$L30540:
; 987  : 	        else
; 988  : 		    break;
; 990  :         dwIncDelta--;

	dec	esi
	mov	DWORD PTR dwIncDelta, esi

; 991  :         if (!dwIncDelta)    

	je	SHORT $L30541_

$L30541:
// esi, edx, edi		esi == dwIncDelta

	mov	DWORD PTR i, 0

; 1010 : 	b = dwIncDelta;
// esi = b == dwIncDelta
; 1011 : 	c = (pfSampleLength - pfSamplePos) / pfPitch;
; 1009 : 	a = (dwLength - dwI) / 2;	// Remaining span.

	mov	edx, edi
	neg	edx
	shr	edx, 1		// edx = a

; 1017 : 	if (b < a && b < c)

	cmp	esi, edx
	jge	try_ax

	mov	eax, ecx
	imul	eax, esi
	add	eax, ebx

	cmp eax, DWORD PTR pfSampleLength 
	jge	try_c

; 1019 : 		i = b;

	cmp	esi, 3
	jl	got_it

	mov	DWORD PTR i, esi
	jmp	SHORT got_it

; 1013 : 	if (a < b && a < c)

try_a:

	cmp	edx, esi
	jge	try_c
try_ax:
	mov	eax, edx
	imul	eax, ecx
	add	eax, ebx

	cmp eax, DWORD PTR pfSampleLength
	jge	try_c

; 1015 : 		i = a;

	cmp	edx, 3
	jl	got_it

	mov	DWORD PTR i, edx
	jmp	SHORT got_it

; 1021 : 	else if (c < a && c < b)
try_c:

    push	edx
	mov	eax, DWORD PTR pfSampleLength
	sub	eax, ebx
	cdq
	idiv	ecx		// eax == c
	pop	edx

    cmp	eax, edx
	jge	got_it
try_cx:
	cmp	eax, esi
	jge	got_it

; 1023 : 		i = c;

	cmp	eax, 3
	jl	$L30543

	mov DWORD PTR i, eax

got_it:
	mov	edx, DWORD PTR i
	mov	eax, DWORD PTR pBuf

	dec	edx
	jl	$L30543

	sub	DWORD PTR dwIncDelta, edx

; 1093 :     return (dwI >> 1);
; 1094 : }

	lea	edx, [edx*2+2]			// Current span.
	lea	eax, [eax+edi*2]		// Starting position.

	add	edi, edx				// Remaining span.
	lea	eax, [eax+edx*2]		// New ending position.

	push	edi
	mov	edi, edx				// Current span.

	mov	DWORD PTR pBufX, eax
	neg	edi

$L30797:
		
; 1005 : 			do
; 1010 : 				dwPosition = pfSamplePos >> 12;
; 1011 : 				dwFract = pfSamplePos & 0xFFF;

	mov	edx, ebx
	mov	esi, ebx

	add	ebx, ecx
	mov	ecx, DWORD PTR pcWave

; 1012 : 				pfSamplePos += pfPitch;

	sar	edx, 12					; 0000000cH
	and	esi, 4095				; 00000fffH

; 1014 : 				lA = (long) pcWave[dwPosition];

	movsx	eax, WORD PTR [ecx+edx*2]

; 1015 : 				lM = ((pcWave[dwPosition+1] - lA) * dwFract);
; 1016 : 				lM >>= 12;
; 1017 : 				lM += lA;

	movsx	edx, WORD PTR [ecx+edx*2+2]
	sub	edx, eax

; 1018 : 				lA = lM;
; 1019 : 				lA *= vfLVolume;
; 1020 : 				lA >>= 13;         // Signal bumps up to 15 bits.
; 1022 : 				pBuffer[dwI] += (short) lA;
; 1027 : no_oflowlx:	
; 1028 : 						lM *= vfRVolume;
; 1029 : 						lM >>= 13;
; 1030 : 				pBuffer[dwI+1] += (short) lM;
; 1035 : no_oflowrx:
; 1037 : 				dwI += 2;

	imul	edx, esi

	sar	edx, 12					; 0000000cH
	mov	esi, DWORD PTR vfLVolume

	add	edx, eax
	mov	eax, DWORD PTR pBufX

	imul	esi, edx

	sar	esi, 13					; 0000000dH

	add	WORD PTR [eax+edi*2], si

	mov	esi, DWORD PTR vfRVolume
	jo	overflow_lx
no_oflowlx:

	imul	esi, edx

; 1038 : 			} while (--dwIncDelta);

	sar	esi, 13					; 0000000dH
	mov	ecx, DWORD PTR pfPitch

	add	WORD PTR [eax+edi*2+2], si
	jo	overflow_rx

no_oflowrx:

	add	edi, 2
	jne	SHORT $L30797

	pop	edi

; 1039 : 			++dwIncDelta;
; 1040 : 			continue;

	mov	edx, DWORD PTR pfPFract
	cmp	edi, 0

	jl	SHORT $L30536
	jmp	SHORT $L30539

$L30540_:
; 982  : 	    {	
; 983  : 	        if (pfLoopLength)

	cmp	DWORD PTR pfLoopLength, 0
	je	$L30539

; 985  : 				pfSamplePos -= pfLoopLength;

	sub	ebx, DWORD PTR pfLoopLength
	jmp	$L30540

$L30541_:
; 994  :             pfPFract += pfDeltaPitch;

	mov	ecx, DWORD PTR pfDeltaPitch
	mov	esi, DWORD PTR vfDeltaLVolume

	add	ecx, edx
	mov	edx, DWORD PTR vfLVFract

; 995  :             pfPitch = pfPFract >> 8;
; 996  :             vfLVFract += vfDeltaLVolume;

	mov	DWORD PTR pfPFract, ecx
	add	edx, esi

; 997  :             vfLVolume = vfLVFract >> 8;
; 998  :             vfRVFract += vfDeltaRVolume;

	sar	ecx, 8
	mov	DWORD PTR vfLVFract, edx
	
	sar	edx, 8
	mov	esi, DWORD PTR vfDeltaRVolume

	mov	DWORD PTR vfLVolume, edx
	mov	edx, DWORD PTR vfRVFract

	add	edx, esi
	mov	DWORD PTR pfPitch, ecx

	mov	DWORD PTR vfRVFract, edx
	mov	esi, DWORD PTR dwDeltaPeriod

; 999  :             vfRVolume = vfRVFract >> 8;

	sar	edx, 8
	mov	DWORD PTR dwIncDelta, esi

; 993  :             dwIncDelta = dwDeltaPeriod;

	mov	DWORD PTR vfRVolume, edx
	jmp	$L30541

// Handle truncation.
overflow_l:
	mov	WORD PTR [eax+edi*2], 0x7fff
	js	no_oflowl
	mov	WORD PTR [eax+edi*2], 0x8000
	jmp no_oflowl

overflow_r:
	mov	WORD PTR [eax+edi*2+2], 0x7fff
	js	no_oflowr
	mov	WORD PTR [eax+edi*2+2], 0x8000
	jmp	no_oflowr

overflow_lx:
	mov	WORD PTR [eax+edi*2], 0x7fff
	js	no_oflowlx
	mov	WORD PTR [eax+edi*2], 0x8000
	jmp	no_oflowlx

overflow_rx:
	mov	WORD PTR [eax+edi*2+2], 0x7fff
	js	no_oflowrx
	mov	WORD PTR [eax+edi*2+2], 0x8000
	jmp	no_oflowrx

$L30543:
; 1044 :         dwPosition = pfSamplePos >> 12;

	mov	edx, ebx
	mov	ecx, DWORD PTR pfPitch

; 1045 :         dwFract = pfSamplePos & 0xFFF;

	sar	edx, 12					; 0000000cH
	mov	esi, ebx

	and	esi, 4095				; 00000fffH
	add	ebx, ecx

; 1046 :         pfSamplePos += pfPitch;

	mov	ecx, DWORD PTR pcWave

; 1047 : 
; 1048 :         lA = (long) pcWave[dwPosition];

	movsx	eax, WORD PTR [ecx+edx*2]

; 1049 :         lM = ((pcWave[dwPosition+1] - lA) * dwFract);
; 1050 :         lM >>= 12;
; 1051 :         lM += lA;

	movsx	edx, WORD PTR [ecx+edx*2+2]

	sub	edx, eax

	imul	edx, esi

; 1052 :         lA = lM;
; 1053 :         lA *= vfLVolume;
; 1054 :         lA >>= 13;         // Signal bumps up to 15 bits.

	sar	edx, 12					; 0000000cH
	mov	esi, DWORD PTR vfLVolume

	add	edx, eax

; 1072 : 		pBuffer[dwI] += (short) lA;

	imul	esi, edx

	sar	esi, 13					; 0000000dH
	mov	eax, DWORD PTR pBuf

	add	WORD PTR [eax+edi*2], si
	mov	esi, DWORD PTR vfRVolume

	jo	overflow_l
no_oflowl:

; 1077 : no_oflowl:	
; 1078 :         lM *= vfRVolume;
; 1079 : 		lM >>= 13;

	imul	esi, edx

; 1080 : 		pBuffer[dwI+1] += (short) lM;
; 1085 : no_oflowr:
; 1086 : #endif  /* _ALPHA */
; 1087 : 		dwI += 2;

	sar	esi, 13					; 0000000dH
	mov	ecx, DWORD PTR pfPitch

	add	WORD PTR [eax+edi*2+2], si
	mov	edx, DWORD PTR pfPFract

	jo	overflow_r
no_oflowr:

	add	edi, 2

; 978  : 
; 979  :     for (dwI = 0; dwI < dwLength; )

	jl $L30536

$L30539:
	mov DWORD PTR dwI, edi
	mov DWORD PTR pfSamplePos, ebx
}

	dwI += dwLength;

#endif // _X86_

    vfLastVolume[0] = vfLVolume;
    vfLastVolume[1] = vfRVolume;
    m_pfLastPitch = pfPitch;
    m_pfLastSample = pfSamplePos;
    return (dwI >> 1);
}


#ifdef ORG_MONO_MIXER
DWORD CDigitalAudio::MixMono16(short * pBuffer, 
							   DWORD dwLength,
							   DWORD dwDeltaPeriod,
							   VFRACT vfDeltaVolume,
							   VFRACT vfLastVolume[],
							   PFRACT pfDeltaPitch, 
							   PFRACT pfSampleLength, 
							   PFRACT pfLoopLength)
{
    DWORD dwI;
    DWORD dwPosition;
    long lA;//, lB;
    long lM;
    DWORD dwIncDelta = dwDeltaPeriod;
    VFRACT dwFract;
    short * pcWave = m_pnWave;
    PFRACT pfSamplePos = m_pfLastSample;
    VFRACT vfVolume = vfLastVolume[0];
    PFRACT pfPitch = m_pfLastPitch;
    PFRACT pfPFract = pfPitch << 8;
    VFRACT vfVFract = vfVolume << 8;  // Keep high res version around.

#ifndef _X86_
    for (dwI = 0; dwI < dwLength;)
    {
        if (pfSamplePos >= pfSampleLength)
	    {	
	        if (pfLoopLength)
		    pfSamplePos -= pfLoopLength;
	        else
		    break;
	    }
        dwIncDelta--;
        if (!dwIncDelta)   
        {
            dwIncDelta = dwDeltaPeriod;
            pfPFract += pfDeltaPitch;
            pfPitch = pfPFract >> 8;
            vfVFract += vfDeltaVolume;
            vfVolume = vfVFract >> 8;
        }

        dwPosition = pfSamplePos >> 12;
        dwFract = pfSamplePos & 0xFFF;
        pfSamplePos += pfPitch;

        lA = (long) pcWave[dwPosition];
        lM = (((pcWave[dwPosition+1] - lA) * dwFract) >> 12) + lA;

        lM *= vfVolume; 
        lM >>= 13;         // Signal bumps up to 12 bits.

#ifndef _X86_
#ifdef _ALPHA_
		int nBitmask;
		if( ALPHA_OVERFLOW & (nBitmask = __ADAWI( (short) lM, &pBuffer[dwI] )) )  {
			if( ALPHA_NEGATIVE & nBitmask )  {
				pBuffer[dwI] = 0x7FFF;
			}
			else  pBuffer[dwI] = (short) 0x8000;
		}
#else // !_ALPHA_
    // TODO -- overflow detection for ia64 (+ axp64?)
#endif // !_ALPHA_
#else // _X86_  (dead code)
        // Keep this around so we can use it to generate new assembly code (see below...)
        pBuffer[dwI] += (short) lM;
        _asm{jno no_oflow}
        pBuffer[dwI] = 0x7fff;
        _asm{js  no_oflow}
        pBuffer[dwI] = (short) 0x8000;
no_oflow:	
#endif // _X86  (dead code)
		dwI++;
    }
#else // _X86_
	int i, a, b, c, total;
	short * pBuf = pBuffer + dwLength, *pBufX;
	dwI = - dwLength;

	_asm {

; 979  :     for (dwI = 0; dwI < dwLength; )

//	Induction variables.
	mov	edi, dwI
	mov	ebx, DWORD PTR pfSamplePos

// Previously set up.
	cmp	DWORD PTR dwLength, 0
	mov	edx, pfPFract

	mov	ecx, DWORD PTR pfPitch
	je	$L30539

$L30536:
	cmp	ebx, DWORD PTR pfSampleLength

; 981  :         if (pfSamplePos >= pfSampleLength)

	mov	esi, DWORD PTR dwIncDelta
	jge	SHORT $L30540_

$L30540:
; 987  : 	        else
; 988  : 		    break;
; 990  :         dwIncDelta--;

	dec	esi
	mov	DWORD PTR dwIncDelta, esi

; 991  :         if (!dwIncDelta)    

	je	SHORT $L30541_

$L30541:
// esi, edx, edi		esi == dwIncDelta

	mov	DWORD PTR i, 0

; 1010 : 	b = dwIncDelta;
// esi = b == dwIncDelta
; 1011 : 	c = (pfSampleLength - pfSamplePos) / pfPitch;
; 1009 : 	a = dwLength - dwI;	// Remaining span.

	mov	edx, edi
	neg	edx

; 1017 : 	if (b < a && b < c)

	cmp	esi, edx
	jge	try_ax

	mov	eax, ecx
	imul	eax, esi
	add	eax, ebx

	cmp eax, DWORD PTR pfSampleLength 
	jge	try_c

; 1019 : 		i = b;

	cmp	esi, 3
	jl	got_it

	mov	DWORD PTR i, esi
	jmp	SHORT got_it

; 1013 : 	if (a < b && a < c)

try_a:

	cmp	edx, esi
	jge	try_c
try_ax:
	mov	eax, edx
	imul	eax, ecx
	add	eax, ebx

	cmp eax, DWORD PTR pfSampleLength
	jge	try_c

; 1015 : 		i = a;

	cmp	edx, 3
	jl	got_it

	mov	DWORD PTR i, edx
	jmp	SHORT got_it

; 1021 : 	else if (c < a && c < b)
try_c:
	push	edx
	mov	eax, DWORD PTR pfSampleLength
	sub	eax, ebx
	cdq
	idiv	ecx		// eax == c
	pop	edx

    cmp	eax, edx
	jge	got_it
try_cx:
	cmp	eax, esi
	jge	got_it

; 1023 : 		i = c;

	cmp	eax, 3
	jl	$L30543

	mov DWORD PTR i, eax

got_it:
	mov	edx, DWORD PTR i
	mov	eax, DWORD PTR pBuf

	dec	edx
	jl	$L30543

	sub	DWORD PTR dwIncDelta, edx

; 1093 :     return (dwI);
; 1094 : }

	lea	edx, [edx+1]			// Current span.
	lea	eax, [eax+edi*2]		// Starting position.

	add	edi, edx				// Remaining span.
	lea	eax, [eax+edx*2]		// New ending position.

	push	edi
	mov	edi, edx				// Current span.

	mov	DWORD PTR pBufX, eax
	neg	edi

$L30797:
; 1005 : 			do
; 1010 : 				dwPosition = pfSamplePos >> 12;
; 1011 : 				dwFract = pfSamplePos & 0xFFF;

	mov	edx, ebx
	mov	esi, ebx

	add	ebx, ecx
	mov	ecx, DWORD PTR pcWave

; 1012 : 				pfSamplePos += pfPitch;

	sar	edx, 12					; 0000000cH
	and	esi, 4095				; 00000fffH

; 1013 : 
; 1014 : 				lA = (long) pcWave[dwPosition];

	movsx	eax, WORD PTR [ecx+edx*2]

; 1015 : 				lM = ((pcWave[dwPosition+1] - lA) * dwFract);
; 1016 : 				lM >>= 12;
; 1017 : 				lM += lA;

	movsx	edx, WORD PTR [ecx+edx*2+2]

	sub	edx, eax

; 1018 : 				lA = lM;
; 1019 : 				lA *= vfLVolume;
; 1020 : 				lA >>= 13;         // Signal bumps up to 15 bits.
; 1022 : 				pBuffer[dwI] += (short) lA;
; 1027 : no_oflowx:	
; 1037 : 				++dwI;

	imul	edx, esi

	sar	edx, 12					; 0000000cH
	mov	esi, DWORD PTR vfVolume

	add	edx, eax
	mov	ecx, DWORD PTR pfPitch

	imul	esi, edx

	sar	esi, 13					; 0000000dH
	mov	eax, DWORD PTR pBufX

	add	WORD PTR [eax+edi*2], si
	jo	overflow_x
no_oflowx:

; 1038 : 			} while (--dwIncDelta);

	inc	edi
	jne	SHORT $L30797

	pop	edi

; 1039 : 			++dwIncDelta;
; 1040 : 			continue;

	mov	edx, DWORD PTR pfPFract
	cmp	edi, 0

	jl	SHORT $L30536
	jmp	SHORT $L30539

$L30540_:
; 983  : 	        if (pfLoopLength)

	cmp	DWORD PTR pfLoopLength, 0
	je	$L30539

; 985  : 				pfSamplePos -= pfLoopLength;

	sub	ebx, DWORD PTR pfLoopLength
	jmp	$L30540

$L30541_:
; 994  :             pfPFract += pfDeltaPitch;

	mov	ecx, DWORD PTR pfDeltaPitch
	mov	esi, DWORD PTR vfDeltaVolume

	add	ecx, edx
	mov	edx, DWORD PTR vfVFract

; 995  :             pfPitch = pfPFract >> 8;
; 996  :             vfVFract += vfDeltaVolume;

	mov	DWORD PTR pfPFract, ecx
	add	edx, esi

; 997  :             vfVolume = vfVFract >> 8;

	sar	ecx, 8
	mov	DWORD PTR vfVFract, edx
	
	sar	edx, 8
	mov	esi, DWORD PTR dwDeltaPeriod

	mov	DWORD PTR vfVolume, edx
	mov	DWORD PTR pfPitch, ecx


	mov	DWORD PTR dwIncDelta, esi

; 993  :             dwIncDelta = dwDeltaPeriod;

	jmp	$L30541

// Handle truncation.
overflow_:
	mov	WORD PTR [eax+edi*2], 0x7fff
	js	no_oflow
	mov	WORD PTR [eax+edi*2], 0x8000
	jmp no_oflow

overflow_x:
	mov	WORD PTR [eax+edi*2], 0x7fff
	js	no_oflowx
	mov	WORD PTR [eax+edi*2], 0x8000
	jmp	no_oflowx

$L30543:
; 1044 :         dwPosition = pfSamplePos >> 12;

	mov	edx, ebx
	mov	ecx, DWORD PTR pfPitch

; 1045 :         dwFract = pfSamplePos & 0xFFF;

	sar	edx, 12					; 0000000cH
	mov	esi, ebx

	and	esi, 4095				; 00000fffH
	add	ebx, ecx

; 1046 :         pfSamplePos += pfPitch;

	mov	ecx, DWORD PTR pcWave

; 1047 : 
; 1048 :         lA = (long) pcWave[dwPosition];

	movsx	eax, WORD PTR [ecx+edx*2]

; 1049 :         lM = ((pcWave[dwPosition+1] - lA) * dwFract);
; 1050 :         lM >>= 12;
; 1051 :         lM += lA;

	movsx	edx, WORD PTR [ecx+edx*2+2]

	sub	edx, eax

	imul	edx, esi

; 1052 :         lA = lM;
; 1053 :         lA *= vfVolume;
; 1054 :         lA >>= 13;         // Signal bumps up to 15 bits.

	sar	edx, 12					; 0000000cH
	mov	esi, DWORD PTR vfVolume

	add	edx, eax

; 1072 : 		pBuffer[dwI] += (short) lA;

	imul	esi, edx

	sar	esi, 13					; 0000000dH
	mov	eax, DWORD PTR pBuf

	add	WORD PTR [eax+edi*2], si
	jo	overflow_
no_oflow:
; 1077 : no_oflowl:	
; 1087 : 		++dwI;

	inc	edi
	mov	edx, DWORD PTR pfPFract

; 979  :     for (dwI = 0; dwI < dwLength; )

	mov	ecx, DWORD PTR pfPitch
	jl $L30536

$L30539:
	mov DWORD PTR dwI, edi
	mov DWORD PTR pfSamplePos, ebx
}
	dwI += dwLength;

#endif // _X86_
    vfLastVolume[0] = vfVolume;
    vfLastVolume[1] = vfVolume; // !!! is this right?
    m_pfLastPitch = pfPitch;
    m_pfLastSample = pfSamplePos;
    return (dwI);
}
#endif