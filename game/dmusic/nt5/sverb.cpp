/***********************************************************
Copyrights : ksWaves Ltd. 1998.

Provided to Microsoft under contract between ksWaves and Microsoft.

************************************************************/

/***********************************************************
General description :

The functions in this file provides for any apoplication to process audio data
with the SVerb algorithm.

In order to do so the application should :

1. Allocate two chunks of memory for 'Coefs' and 'States' with sizes as returned 
   by the functions 'GetCoefsSize' and 'GetStatesSize' accordingly.
   
2. Initialize these memory chunks using the functions : 'InitSVerb' and 'InitSVerbStates' 
   accordingly.

3. Change the settings of the SVerb sound using the function 'SetSVerb'.

4. Call one of the process functions according to the input/output data format:

   SVerbMonoToMonoShort 
   SVerbMonoToStereoShort
   SVerbStereoToStereoShort 
   SVerbMonoToMonoFloat
   SVerbMonoToStereoFloat
   SVerbStereoToStereoFloat

   The input/output are always the same data type (i.e. both input and output are short integer
   or both are 32bits floats).

   Stereo data format is always'interlaced' left,right samples.

   The 'coefs' and 'states' memory should be passed to the process functions.
   
5. Many coefs structures can be initialized each for different SVerb settings. Passing a different
   coefs structure will cause a real time change of sound quality.

   As long as sound continuity should be maintained the states structure should not be changes or
   re-initialized. Only when a completly new audio sequence is desired should the states be re-initialized.

6. Note that the coefs are valid per sampling rate.

7. Althaugh provisions for coefs compatibility for future versions are provided, it should be avoided to save coefs
   structures to files as-is and re-use them later. Rather the application should save the 'real-world'
   settings of the reverb - namely the parameters passed to 'SetSVerb'. These 'real-world' settings 
   will always be valid for future versions, as well as if other sampling rates are used. The coefs 
   structur(es) should be re-initialized in run time using the real-world settings and call to 
   'SetSverb'.


************************************************************/

// NT5 dmsynth sverb — ported to cross-platform C++ by replacing the
// Windows-specific <windows.h>/<String.h> includes with nt5compat.h (which
// provides BOOL/DWORD/LONG aliases etc.) plus <cstring> for memset. The
// MSVC-only "#pragma optimize" is gated so GCC/Clang don't warn on it.
// USE_ALL_VERBS is defined so SVerbStereoToStereoFloat (our only path)
// is visible — in the original it sits inside that #ifdef block.
#define USE_ALL_VERBS 1
#include "nt5compat.h"
#include <cstring>
#include <cmath>
#include "sverb.h"

#ifdef _MSC_VER
#pragma optimize( "ty", on )
#endif

/****************************************************************************

Function Name	: GetCoefsSize

Input Arguments : None

Return Value    : The size of memory in bytes, to be allocated in order to hold coefficients.

Description		:  

This function must be called before any calls to other functions that uses the coefs structure.
The calling app must than allocate the returned size of memory and initialize it using 'InitSVerb()'
and 'SetSVerb()'.

The caller should not be botherred by the internals of the coefs structure, rather only konw it's size 
and than allocate enough memory to hold it.

The structure allocated can be used in a very flexible way in order to allow for real-time, pre-computed
changes in Reverb Sound. 

*****************************************************************************/

int32_t GetCoefsSize(void) 
{
	return sizeof(sCoefsStruct); 
};

/****************************************************************************

Function Name	: GetStatesSize

Input Arguments : None

Return Value    : The size of memory in bytes, to be allocated in order to hold states.

Description		:  

This function must be called before any calls to other functions that uses the states structure.
The calling app must than allocate the returned size of memory and initialize it using 'InitSVerbStates()'.

The states allocated are valid in run-time only, and sould be re-initialized only when a complete
new input is to be processed by the SVerb. 

When changing the settings of revevreb in real time while audio is playing, the states should not 
be re-initialized, rather the same passed states must be passed to the process functions in order 
to maintain sound continuity.

*****************************************************************************/

int32_t GetStatesSize(void) 
{
	return sizeof(int32_t)*(BASE_REV_DELAY+2*BASE_DSPS_DELAY+2); 
};

/****************************************************************************

Function Name	: GetSVerbVersion

Input Arguments : None

Return Value    : Version of SVerb implementation - for future compatibility.

Description		:  

Since the caller do not know about the internals of the coefs structure, this function,
together with 'VerifyVersion' function provides a way to verify if a coefs structure
match the version of the reverb used.

This should be needed only if one is using a coefs structure that was saved to file, and 
being used later.

NOTE : In normal operation, this way of usage should be avoided... and only real-world reverb
settings should be saved to files, and re-initialize the coefs in run time.

*****************************************************************************/

int32_t GetSVerbVersion(void) 
{
	return 0x1; 
};

/****************************************************************************

Function Name	: VerifySampleRate

Input Arguments : 

 void *pC		: The pointer to the coefs memory.

Return Value    : The sample rate for which this coefs are valid.

Description		:  

When an application uses different sampling rates, and re-uses same coefs structures, 
it should verify that the coefs match the audio sampling rate.

*****************************************************************************/

float VerifySampleRate(void *pC) {
	return ((sCoefsStruct *)pC)->SampleRate; 
};

/****************************************************************************

Function Name	: VerifyVersion

Input Arguments : 

 void *pC		: The pointer to the coefs memory.

Return Value    : The version of this coefs structure.

Description		:  

When initialized, each coefs structure is 'stamped' with it's version.
The location of this variable in the structure is fixed, and thus all future versions of
SVerb will know to read it.

Note : as explained above, in normal uses coefs should not be saved to files, rather the 
'real-world' settings should be saved and coefs re-initialized in run-time.
*****************************************************************************/

int32_t VerifyVersion(void *pC) {
	return ((sCoefsStruct *)pC)->myVersion; 
};

/****************************************************************************

Function Name	: VerifySize

Input Arguments : 

 void *pC		: The pointer to the coefs memory.

Return Value    : The size of this coefs structure.

Description		:  

When initialized, each coefs structure is 'stamped' with it's size.
The location of this variable in the structure is fixed, and thus all future versions of
SVerb will know to read it.

Note : as explained above, in normal uses coefs should not be saved to files, rather the 
'real-world' settings should be saved and coefs re-initialized in run-time.
*****************************************************************************/

int32_t VerifySize(void *pC) {
	return ((sCoefsStruct *)pC)->mySize; 
};


/****************************************************************************

Function Name	: InitSVerbStates

Input Arguments : 

 float *pStates	: The pointer to the states memory.

Return Value    : none.

Description		:  

After allocating memory for the states, according to thge size returned by 'GetStatesSize'
The application MUST initialize the states using this function. 
Note : in future versions this may be more complex than simply memset to 0...
*****************************************************************************/

void InitSVerbStates( int32_t *pStates )
{
    memset( pStates, 0, GetStatesSize() ) ;
}

/****************************************************************************

Function Name	: DToF16

Input Arguments : 

 float SampleRate	: The sampling rate.
 void *pC			: The pointer to the coefs memory.

Return Value    : none.

Description		:  

Converts a float number between -1.0 .. 1.0 to a 16bits integer 
fixed point representation.
This allows for fix point arithmetics, where two 16bits integers are multiplied to 
a 32bits integer, and we than take the upper 16 bits of the result.

*****************************************************************************/

int32_t DToF16( float dbl  )
{
	dbl *= MAX_16;
	// max/min are <windows.h> macros on MSVC; use nt5compat's __max/__min
	// so the file builds on GCC/Clang too (and on MSVC without windows.h).
	dbl = __max(-MAX_16,__min(MAX_16-(float)1.0,dbl+(float)0.5));
	return (int32_t)(dbl);
}

/****************************************************************************

Function Name	: ConvertCoefsToFix

Input Arguments : 

 void *pC			: The pointer to the coefs memory.

Return Value    : none.

Description		:  converts coefficients to longs, as fixed point numbers

*****************************************************************************/


void ConvertCoefsToFix( void *pC )
{

	sCoefsStruct *pCoefs = ((sCoefsStruct *)pC);

//		float directGain; 

	pCoefs->l_directGain =  DToF16(pCoefs->directGain);

//		float revGain; 
	pCoefs->l_revGain =  DToF16(pCoefs->revGain);
//		float dDsps;
	pCoefs->l_dDsps =  DToF16(pCoefs->dDsps);
//		float dDG1;
	pCoefs->l_dDG1 =  DToF16(pCoefs->dDG1);
//		float dDG2; 
	pCoefs->l_dDG2 =  DToF16(pCoefs->dDG2);
//	float dFB11;
	pCoefs->l_dFB11 =  DToF16(pCoefs->dFB11);
//		float dFB12;
	pCoefs->l_dFB12 =  DToF16(pCoefs->dFB12);
//		float dFB21;
	pCoefs->l_dFB21 =  DToF16(pCoefs->dFB21);
//		float dFB22;
	pCoefs->l_dFB22 =  DToF16(pCoefs->dFB22);
//		float dFB31;
	pCoefs->l_dFB31 =  DToF16(pCoefs->dFB31);
//		float dFB32;
	pCoefs->l_dFB32 =  DToF16(pCoefs->dFB32);
//		float dFB41;
	pCoefs->l_dFB41 =  DToF16(pCoefs->dFB41);
//		float dFB42;
	pCoefs->l_dFB42 =  DToF16(pCoefs->dFB42);
//		float dDamp;
	pCoefs->l_dDamp =  DToF16(pCoefs->dDamp);



}

/****************************************************************************

Function Name	: InitSVerb

Input Arguments : 

 float SampleRate	: The sampling rate.
 void *pC			: The pointer to the coefs memory.

Return Value    : none.

Description		:  

After allocating memory for the coefs, according to thge size returned by 'GetCoefsSize'
The application MUST initialize the coefs using this function. 
The initialization takes the sampling rate as an argument, ans thus is valid per this
sampling rate only.

It is possible to find out what is the sampling rate a coefs structure is valid for by calling 
the function 'VerifySampleRate'.

This function initialises the SVerb to so reasonable default setting by calling 'SetSVerb' with
the following real-world settings :

InGain				= -3.0dB   (to avoid output overflows)
dRevMix				= -6.0dB   (a reasonable reverb mix)
dRevTime			= 1000.0ms (one second global reverb time)
dHighFreqRTRatio	= 0.001    (the ratio of the high frequencies to the global reverb time) 

*****************************************************************************/

void InitSVerb( float SampleRate, void *pC)
{

	sCoefsStruct *pCoefs = ((sCoefsStruct *)pC);
 	//Magic numbers ...
    int32_t lRefD;
	
	float dRatio =  (float)1.189207115003;
	
	float dD2MRatio = (float)0.2309333333;

	pCoefs->mySize = sizeof(sCoefsStruct);
	pCoefs->myVersion = 0x1;

	pCoefs->dDsps =  (float)0.6180339887499;

	pCoefs->SampleRate = SampleRate;

    lRefD = (int32_t)( 0.5 + 0.045 * pCoefs->SampleRate ) ;

	pCoefs->lDelay1 = lRefD;
	pCoefs->lDelay3 = (int32_t)(0.5 + dRatio * (float)pCoefs->lDelay1);
	pCoefs->lDelay2 = (int32_t)(0.5 + dRatio * (float)pCoefs->lDelay3);
	pCoefs->lDelay4 = (int32_t)(0.5 + dRatio * (float)pCoefs->lDelay2);
  
    pCoefs->lDDly1 = (int32_t)(0.5 + 0.5 * dD2MRatio * (float)(pCoefs->lDelay1+pCoefs->lDelay2));
	pCoefs->lDDly2 = (int32_t)(0.5 + 0.5 * dD2MRatio * (float)(pCoefs->lDelay3+pCoefs->lDelay4));

    pCoefs->lDelay1 -= pCoefs->lDDly1 ;    
    pCoefs->lDelay2 -= pCoefs->lDDly1 ;    
    pCoefs->lDelay3 -= pCoefs->lDDly2 ;    
    pCoefs->lDelay4 -= pCoefs->lDDly2 ;        

    pCoefs->lDelay1 <<= 2;    
    pCoefs->lDelay2 <<= 2;    
    pCoefs->lDelay3 <<= 2;    
    pCoefs->lDelay4 <<= 2;        

	pCoefs->lDDly1 <<= 1;
	pCoefs->lDDly2 <<= 1;

	SetSVerb( (float)0.0, (float)-10.0, (float)1000.0, (float)0.001, pC );

}

/****************************************************************************

Function Name	: SetSVerb

Input Arguments : 

InGain				: input gain in dB (to avoid output overflows)

dRevMix				: Reverb mix in dB. 0dB means 100% wet reverb (no direct signal)
                      Negative values gives less wet signal.
					  The coeficients are calculated so that the overall output level stays 
					  (approximately) constant regardless of the ammount of reverb mix.
dRevTime			: The global reverb time (decay time) in milliseconds.

dHighFreqRTRatio	: The ratio of the high frequencies to the global reverb time. 
					  Unless very 'splashy-bright' reverbs are wanted, this should be set to 
					  a value < 1.0.
					  For example if dRevTime==1000ms and dHighFreqRTRatio=0.1 than the 
					  decay time for high frequencies will be 100ms.

void *pC			: The pointer to the coefs memory.

Return Value    : none.

Description		:  

This function accepts the 'real world' settings or SVerb and computes the corresponding 
coefs structure.

The coefs pointer passed to it MUST have been initialized first by InitSVerb.

In normal uses one coefs structure is allocated, initialized, and than as the user changes 
SVerb settings this function should be called repeatedly with the same coefs pointer and the 
new 'real world' settings. 

And the coefs structure passed to the process function in the next buffer to process.

Also few coefs structures can be pre allocated, and initialized, and than different 'presets' 
can be pre-computed into each of them, and switched in real time. 

The coefs structures should not be saved to files by the application for future uses, rather 
the 'real world' settings them selvs. This way future compatibility is guaranteed.

*****************************************************************************/

void SetSVerb( float InGain, float dRevMix, 
			   float dRevTime, float dHighFreqRTRatio, void *pC )
{


	sCoefsStruct *pCoefs = ((sCoefsStruct *)pC);

    float dD,dTmp,dInGain,dRevGain;

	float dHfR;
    float dAPS;

    if (dHighFreqRTRatio > (float) 0.999)
    {
        dHighFreqRTRatio = (float) 0.999;
    }
    if (dHighFreqRTRatio <= (float) 0.0)
    {
        dHighFreqRTRatio = (float) 0.001;
    }
    dHfR = ( (float)1.0/dHighFreqRTRatio - (float)1.0);

    if (dRevTime < (float) 0.001) 
    {
        dRevTime = (float) 0.001;
    }

    if (InGain > (float) 0.0)
    {
        InGain = (float) 0.0;
    }

    if (dRevMix > (float) 0.0)
    {
        dRevMix = (float) 0.0;
    }

    if (pCoefs->SampleRate < (float) 1.0) 
    {
        pCoefs->SampleRate = (float) 22050.0;
    }

    dAPS = (float)(-3000.0) / (pCoefs->SampleRate * dRevTime);


    pCoefs->dDamp = 0.0;

 	pCoefs->dDG1 = (float)pow((float)10.0,(float)(pCoefs->lDDly1>>1)*dAPS);
 	pCoefs->dDG2 = (float)pow((float)10.0,(float)(pCoefs->lDDly2>>1)*dAPS);

	//////////////////////////////

		pCoefs->dFB11 = (float)pow((float)10.0,(float)(pCoefs->lDelay1>>2)*dAPS);
        
		dD = pCoefs->dFB11 * pCoefs->dDG1;
        dD = (float)1.0+dD*((float)1.0+dD*((float)1.0+dD*((float)1.0 + dD)));
        pCoefs->dDamp += dD *dD;

		dTmp = (float)pow((float)10.0,(float)((pCoefs->lDDly1>>1)+(pCoefs->lDelay1>>2))*dAPS*dHfR);
		dTmp = ((float)1.0 - dTmp)*(float)0.5;

		pCoefs->dFB12 = pCoefs->dFB11 * dTmp;
		pCoefs->dFB11 *= ((float)1.0-dTmp);

	///////////////////////////////

		pCoefs->dFB21 = (float)pow((float)10.0,(float)(pCoefs->lDelay2>>2)*dAPS);
        
		dD = pCoefs->dFB21 * pCoefs->dDG1;
        dD = (float)1.0+dD*((float)1.0+dD*((float)1.0+dD*((float)1.0 + dD)));
        pCoefs->dDamp += dD *dD;

		dTmp = (float)pow((float)10.0,(float)((pCoefs->lDDly1>>1)+(pCoefs->lDelay2>>2))*dAPS*dHfR);
		dTmp = ((float)1.0 - dTmp)*(float)0.5;

		pCoefs->dFB22 = pCoefs->dFB21 * dTmp;
		pCoefs->dFB21 *= ((float)1.0-dTmp);

	////////////////////////////////

		pCoefs->dFB31 = (float)pow((float)10.0,(float)(pCoefs->lDelay3>>2)*dAPS);
        
		dD = pCoefs->dFB31 * pCoefs->dDG2;
        dD = (float)1.0+dD*((float)1.0+dD*((float)1.0+dD*((float)1.0 + dD)));
		    pCoefs->dDamp += dD *dD;

		dTmp = (float)pow((float)10.0,(float)((pCoefs->lDDly2>>1)+(pCoefs->lDelay3>>2))*dAPS*dHfR);
		dTmp = ((float)1.0 - dTmp)*(float)0.5;

		pCoefs->dFB32 = pCoefs->dFB31 * dTmp;
		pCoefs->dFB31 *= ((float)1.0-dTmp);


	//////////////////////////////

		pCoefs->dFB41 = (float)pow((float)10.0,(float)(pCoefs->lDelay4>>2)*dAPS);

        dD = pCoefs->dFB41 * pCoefs->dDG2;
        dD = (float)1.0+dD*((float)1.0+dD*((float)1.0+dD*((float)1.0 + dD)));
        pCoefs->dDamp += dD *dD;

		dTmp = (float)pow((float)10.0,(float)((pCoefs->lDDly2>>1)+(pCoefs->lDelay4>>2))*dAPS*dHfR);
		dTmp = ((float)1.0 - dTmp)*(float)0.5;

		pCoefs->dFB42 = pCoefs->dFB41 * dTmp;
		pCoefs->dFB41 *= ((float)1.0-dTmp);


    pCoefs->dDamp = (float)sqrt(pCoefs->dDamp);

 	dInGain = (float)pow((float)10.0, (float)0.05*InGain ) ;
	dRevMix = (float)pow((float)10.0,(float)0.1*dRevMix);

	dRevGain = (float)4.0 / pCoefs->dDamp * dInGain;

	//in the DSP we used -  	 
	

	pCoefs->directGain = dInGain * (float)sqrt((float)1.0-dRevMix);
	pCoefs->revGain = dRevGain * (float)sqrt(dRevMix);

	ConvertCoefsToFix( pC );

}

///////////////////////////////////////////////////////////////////////////////////////
/**************************************************************************************/
/**************************************************************************************/
/**************************************************************************************/
/**************************************************************************************/
/* Process functions */
/**************************************************************************************/
/**************************************************************************************/
/**************************************************************************************/

/**********************************************************************************

Bellow are 6 different process functions.
The difference between the functions is only in the input/output data formats.

3 functions support short samples input/output.
3 other functions support float samples input/output.

Per each of the data types there are 3 functions :

  Mono-Mono
  Mono-Stereo
  Stereo-Stereo

The names of the functions are clear to which format they apply.

Stereo data is always interlaced left,right samples.

All process functions have basically the same format namely :

  SVerbXXXXXX(int32_t NumInFrames, short *pInShort, short *pOutShort, 
			  void *pC, float *pStates)

Input arguments :

int32_t NumInFrames	: Number of input frames
short *pInXXX		: Pointer to input buffer.
					  Each function expects the data format suggested by it's name in terms of
					  data type (short or float) and mono/stereo.
short *pOutXXX		: Pointer to output buffer.
					  Each function expects the data format suggested by it's name in terms of
					  data type (short or float) and mono/stereo.

void *pC			: The coefs structure allocated and initialized as explained above.
float *pStates		: The states structure allocated and initialized as explained above.

*******************************************************************************************/

void SVerbMonoToMonoShort(int32_t NumInFrames, short *pInShort, short *pOutShort, 
						  void *pC, int32_t *pStates)
{

	sCoefsStruct *pCoefs =  ((sCoefsStruct *)pC);
	int32_t n_sample;
	int32_t In1, In2, Out1, Out2;
	int32_t Indx1,Indx2,Indx3,Indx4;
	int32_t *pNewDll1, *pNewDll2, *pNewDll3, *pNewDll4;
	int32_t *pPrevDll1, *pPrevDll2, *pPrevDll3, *pPrevDll4, *pDelayIn;
	int32_t	*pDelay = pStates+2;
	int32_t	*pDD1	 = pDelay+0x4000;
	int32_t	*pDD2	 = pDD1+0x800;
	int32_t Indx = ((int32_t *)pStates)[0];
	int32_t DIndx = ((int32_t *)pStates)[1] ;

	Indx1 = (Indx+4+pCoefs->lDelay1) & REV_MASK;
	Indx2 = (Indx+4+pCoefs->lDelay2) & REV_MASK;
	Indx3 = (Indx+4+pCoefs->lDelay3) & REV_MASK;
	Indx4 = (Indx+4+pCoefs->lDelay4) & REV_MASK;

	pPrevDll1 = pDelay+Indx1;
	pPrevDll2 = pDelay+Indx2+1;
	pPrevDll3 = pDelay+Indx3+2;
	pPrevDll4 = pDelay+Indx4+3;

	Indx1 = (Indx1-4)&REV_MASK;
	Indx2 = (Indx2-4)&REV_MASK;
	Indx3 = (Indx3-4)&REV_MASK;
	Indx4 = (Indx4-4)&REV_MASK;

		for (n_sample = 0;n_sample < NumInFrames;n_sample++)
		{

			In1 = In2 = (int32_t)(*pInShort++)>>1;

			Out1 = (In1 * pCoefs->l_directGain)>>15;

			Out2 = (In2 * pCoefs->l_directGain)>>15;

			In1 = (In1 * pCoefs->l_revGain)>>15;

			In2 = (In2 * pCoefs->l_revGain)>>15;

			pNewDll1 = pDelay+Indx1;
			pNewDll2 = pDelay+Indx2+1;
			pNewDll3 = pDelay+Indx3+2;
			pNewDll4 = pDelay+Indx4+3;

			dspsL( pDD1, DIndx, pCoefs->lDDly1, pCoefs->l_dDG1, pCoefs->l_dDsps, pNewDll1, pNewDll2 );
			dspsL( pDD2, DIndx, pCoefs->lDDly2, pCoefs->l_dDG2, pCoefs->l_dDsps, pNewDll3, pNewDll4 );

			Out1 += *pNewDll1 + *pNewDll3;
			Out2 += *pNewDll2 + *pNewDll4;

			pDelayIn = pDelay + Indx;

			*pDelayIn++ = In1 + ((*pNewDll1*pCoefs->l_dFB11 + *pPrevDll1*pCoefs->l_dFB12)>>15);
			pPrevDll1 = pNewDll1;
			Indx1 = (Indx1 - 4) & REV_MASK;

			*pDelayIn++ = In2 + ((*pNewDll2*pCoefs->l_dFB21 + *pPrevDll2*pCoefs->l_dFB22)>>15);			
			pPrevDll2 = pNewDll2;
			Indx2 = (Indx2 - 4) & REV_MASK;

			*pDelayIn++ = -In2 + ((*pNewDll3*pCoefs->l_dFB31 + *pPrevDll3*pCoefs->l_dFB32)>>15);
			pPrevDll3 = pNewDll3;
			Indx3 = (Indx3 - 4) & REV_MASK;

			*pDelayIn++ = In1 + ((*pNewDll4*pCoefs->l_dFB41 + *pPrevDll4*pCoefs->l_dFB42)>>15);
			pPrevDll4 = pNewDll4;
			Indx4 = (Indx4 - 4) & REV_MASK;

			Indx = (Indx - 4) & REV_MASK;
			DIndx = (DIndx - 2) & DSPS_MASK;

			Out1 += Out2;
			CLIP_SHORT_TO_SHORT(Out1)

			*pOutShort++ = (short)(Out1);
			
		}

	((int32_t *)pStates)[0] = Indx ;
	((int32_t *)pStates)[1] = DIndx ;

}

#ifdef USE_ALL_VERBS
void SVerbMonoToStereoShort(int32_t NumInFrames, short *pInShort, short *pOutShort, 
						    void *pC, int32_t *pStates)
{

	sCoefsStruct *pCoefs = ((sCoefsStruct *)pC);
	int32_t n_sample;
	int32_t In1, In2, Out1, Out2;
	int32_t Indx1,Indx2,Indx3,Indx4;
	int32_t *pNewDll1, *pNewDll2, *pNewDll3, *pNewDll4;
	int32_t *pPrevDll1, *pPrevDll2, *pPrevDll3, *pPrevDll4, *pDelayIn;
	int32_t	*pDelay = pStates+2;
	int32_t	*pDD1	 = pDelay+0x4000;
	int32_t	*pDD2	 = pDD1+0x800;
	int32_t Indx = ((int32_t *)pStates)[0];
	int32_t DIndx = ((int32_t *)pStates)[1] ;

	Indx1 = (Indx+4+pCoefs->lDelay1) & REV_MASK;
	Indx2 = (Indx+4+pCoefs->lDelay2) & REV_MASK;
	Indx3 = (Indx+4+pCoefs->lDelay3) & REV_MASK;
	Indx4 = (Indx+4+pCoefs->lDelay4) & REV_MASK;

	pPrevDll1 = pDelay+Indx1;
	pPrevDll2 = pDelay+Indx2+1;
	pPrevDll3 = pDelay+Indx3+2;
	pPrevDll4 = pDelay+Indx4+3;

	Indx1 = (Indx1-4)&REV_MASK;
	Indx2 = (Indx2-4)&REV_MASK;
	Indx3 = (Indx3-4)&REV_MASK;
	Indx4 = (Indx4-4)&REV_MASK;

		for (n_sample = 0;n_sample < NumInFrames;n_sample++)
		{

			In1 = (int32_t)(*pInShort++);
			In1 += (In1>>1) - (In1>>2);
			In2 = In1;

			Out1 = (In1 * pCoefs->l_directGain)>>15;

			Out2 = (In2 * pCoefs->l_directGain)>>15;

			In1 = (In1 * pCoefs->l_revGain)>>15;

			In2 = (In2 * pCoefs->l_revGain)>>15;

			pNewDll1 = pDelay+Indx1;
			pNewDll2 = pDelay+Indx2+1;
			pNewDll3 = pDelay+Indx3+2;
			pNewDll4 = pDelay+Indx4+3;

			dspsL( pDD1, DIndx, pCoefs->lDDly1, pCoefs->l_dDG1, pCoefs->l_dDsps, pNewDll1, pNewDll2 );
			dspsL( pDD2, DIndx, pCoefs->lDDly2, pCoefs->l_dDG2, pCoefs->l_dDsps, pNewDll3, pNewDll4 );

			Out1 += *pNewDll1 + *pNewDll3;
			Out2 += *pNewDll2 + *pNewDll4;

			pDelayIn = pDelay + Indx;

			*pDelayIn++ = In1 + ((*pNewDll1*pCoefs->l_dFB11 + *pPrevDll1*pCoefs->l_dFB12)>>15);
			pPrevDll1 = pNewDll1;
			Indx1 = (Indx1 - 4) & REV_MASK;

			*pDelayIn++ = In2 + ((*pNewDll2*pCoefs->l_dFB21 + *pPrevDll2*pCoefs->l_dFB22)>>15);			
			pPrevDll2 = pNewDll2;
			Indx2 = (Indx2 - 4) & REV_MASK;

			*pDelayIn++ = -In2 + ((*pNewDll3*pCoefs->l_dFB31 + *pPrevDll3*pCoefs->l_dFB32)>>15);
			pPrevDll3 = pNewDll3;
			Indx3 = (Indx3 - 4) & REV_MASK;

			*pDelayIn++ = In1 + ((*pNewDll4*pCoefs->l_dFB41 + *pPrevDll4*pCoefs->l_dFB42)>>15);
			pPrevDll4 = pNewDll4;
			Indx4 = (Indx4 - 4) & REV_MASK;

			Indx = (Indx - 4) & REV_MASK;
			DIndx = (DIndx - 2) & DSPS_MASK;

			CLIP_SHORT_TO_SHORT(Out1)
			CLIP_SHORT_TO_SHORT(Out2)

			*pOutShort++ = (short)(Out1);
			*pOutShort++ = (short)(Out2);
			
		}

	((int32_t *)pStates)[0] = Indx ;
	((int32_t *)pStates)[1] = DIndx ;

}
#endif

void SVerbStereoToStereoShort(int32_t NumInFrames, short *pInShort, short *pOutShort, 
						      void *pC, int32_t *pStates)
{

	sCoefsStruct *pCoefs = ((sCoefsStruct *)pC);
	int32_t n_sample;
	int32_t In1, In2, Out1, Out2;
	int32_t Indx1,Indx2,Indx3,Indx4;
	int32_t *pNewDll1, *pNewDll2, *pNewDll3, *pNewDll4;
	int32_t *pPrevDll1, *pPrevDll2, *pPrevDll3, *pPrevDll4, *pDelayIn;
	int32_t	*pDelay = pStates+2;
	int32_t	*pDD1	 = pDelay+0x4000;
	int32_t	*pDD2	 = pDD1+0x800;
	int32_t Indx = ((int32_t *)pStates)[0];
	int32_t DIndx = ((int32_t *)pStates)[1] ;

	Indx1 = (Indx+4+pCoefs->lDelay1) & REV_MASK;
	Indx2 = (Indx+4+pCoefs->lDelay2) & REV_MASK;
	Indx3 = (Indx+4+pCoefs->lDelay3) & REV_MASK;
	Indx4 = (Indx+4+pCoefs->lDelay4) & REV_MASK;

	pPrevDll1 = pDelay+Indx1;
	pPrevDll2 = pDelay+Indx2+1;
	pPrevDll3 = pDelay+Indx3+2;
	pPrevDll4 = pDelay+Indx4+3;

	Indx1 = (Indx1-4)&REV_MASK;
	Indx2 = (Indx2-4)&REV_MASK;
	Indx3 = (Indx3-4)&REV_MASK;
	Indx4 = (Indx4-4)&REV_MASK;

		for (n_sample = 0;n_sample < NumInFrames;n_sample++)
		{

			In1 = (int32_t)(*pInShort++);
			In2 = (int32_t)(*pInShort++);

			Out1 = (In1 * pCoefs->l_directGain)>>15;

			Out2 = (In2 * pCoefs->l_directGain)>>15;

			In1 = (In1 * pCoefs->l_revGain)>>15;

			In2 = (In2 * pCoefs->l_revGain)>>15;

			pNewDll1 = pDelay+Indx1;
			pNewDll2 = pDelay+Indx2+1;
			pNewDll3 = pDelay+Indx3+2;
			pNewDll4 = pDelay+Indx4+3;

			dspsL( pDD1, DIndx, pCoefs->lDDly1, pCoefs->l_dDG1, pCoefs->l_dDsps, pNewDll1, pNewDll2 );
			dspsL( pDD2, DIndx, pCoefs->lDDly2, pCoefs->l_dDG2, pCoefs->l_dDsps, pNewDll3, pNewDll4 );

			Out1 += *pNewDll1 + *pNewDll3;
			Out2 += *pNewDll2 + *pNewDll4;

			pDelayIn = pDelay + Indx;

			*pDelayIn++ = In1 + ((*pNewDll1*pCoefs->l_dFB11 + *pPrevDll1*pCoefs->l_dFB12)>>15);
			pPrevDll1 = pNewDll1;
			Indx1 = (Indx1 - 4) & REV_MASK;

			*pDelayIn++ = In2 + ((*pNewDll2*pCoefs->l_dFB21 + *pPrevDll2*pCoefs->l_dFB22)>>15);			
			pPrevDll2 = pNewDll2;
			Indx2 = (Indx2 - 4) & REV_MASK;

			*pDelayIn++ = -In2 + ((*pNewDll3*pCoefs->l_dFB31 + *pPrevDll3*pCoefs->l_dFB32)>>15);
			pPrevDll3 = pNewDll3;
			Indx3 = (Indx3 - 4) & REV_MASK;

			*pDelayIn++ = In1 + ((*pNewDll4*pCoefs->l_dFB41 + *pPrevDll4*pCoefs->l_dFB42)>>15);
			pPrevDll4 = pNewDll4;
			Indx4 = (Indx4 - 4) & REV_MASK;

			Indx = (Indx - 4) & REV_MASK;
			DIndx = (DIndx - 2) & DSPS_MASK;

			CLIP_SHORT_TO_SHORT(Out1)
			CLIP_SHORT_TO_SHORT(Out2)

			*pOutShort++ = (short)(Out1);
			*pOutShort++ = (short)(Out2);
		}

	((int32_t *)pStates)[0] = Indx ;
	((int32_t *)pStates)[1] = DIndx ;

}

#ifdef USE_ALL_VERBS

void SVerbMonoToMonoFloat(int32_t NumInFrames, float *pInFloat, float *pOutFloat, 
						  void *pC, float *pStates)
{

	sCoefsStruct *pCoefs = ((sCoefsStruct *)pC);
	int32_t n_sample;
	float In1, In2, Out1, Out2;
	int32_t Indx1,Indx2,Indx3,Indx4;
	float *pNewDll1, *pNewDll2, *pNewDll3, *pNewDll4;
	float *pPrevDll1, *pPrevDll2, *pPrevDll3, *pPrevDll4, *pDelayIn;
	float	*pDelay = pStates+2;
	float	*pDD1	 = pDelay+0x4000;
	float	*pDD2	 = pDD1+0x800;
	int32_t Indx = ((int32_t *)pStates)[0];
	int32_t DIndx = ((int32_t *)pStates)[1] ;

	Indx1 = (Indx+4+pCoefs->lDelay1) & REV_MASK;
	Indx2 = (Indx+4+pCoefs->lDelay2) & REV_MASK;
	Indx3 = (Indx+4+pCoefs->lDelay3) & REV_MASK;
	Indx4 = (Indx+4+pCoefs->lDelay4) & REV_MASK;

	pPrevDll1 = pDelay+Indx1;
	pPrevDll2 = pDelay+Indx2+1;
	pPrevDll3 = pDelay+Indx3+2;
	pPrevDll4 = pDelay+Indx4+3;

	Indx1 = (Indx1-4)&REV_MASK;
	Indx2 = (Indx2-4)&REV_MASK;
	Indx3 = (Indx3-4)&REV_MASK;
	Indx4 = (Indx4-4)&REV_MASK;

		for (n_sample = 0;n_sample < NumInFrames;n_sample++)
		{

			In1 = In2 = (float)0.5 * (*pInFloat++) + FPU_DENORM_OFFS;

			Out1 = In1 * pCoefs->directGain;
			Out2 = In2 * pCoefs->directGain;

			In1 *= pCoefs->revGain;
			In2 *= pCoefs->revGain;

			pNewDll1 = pDelay+Indx1;
			pNewDll2 = pDelay+Indx2+1;
			pNewDll3 = pDelay+Indx3+2;
			pNewDll4 = pDelay+Indx4+3;

			dsps( pDD1, DIndx, pCoefs->lDDly1, pCoefs->dDG1, pCoefs->dDsps, pNewDll1, pNewDll2 );
			dsps( pDD2, DIndx, pCoefs->lDDly2, pCoefs->dDG2, pCoefs->dDsps, pNewDll3, pNewDll4 );

			Out1 += *pNewDll1 + *pNewDll3;
			Out2 += *pNewDll2 + *pNewDll4;

			pDelayIn = pDelay + Indx;

			*pDelayIn++ = In1 + *pNewDll1*pCoefs->dFB11 + *pPrevDll1*pCoefs->dFB12;
			pPrevDll1 = pNewDll1;
			Indx1 = (Indx1 - 4) & REV_MASK;

			*pDelayIn++ = In2 + *pNewDll2*pCoefs->dFB21 + *pPrevDll2*pCoefs->dFB22;
			pPrevDll2 = pNewDll2;
			Indx2 = (Indx2 - 4) & REV_MASK;

			*pDelayIn++ = -In2 + *pNewDll3*pCoefs->dFB31 + *pPrevDll3*pCoefs->dFB32;
			pPrevDll3 = pNewDll3;
			Indx3 = (Indx3 - 4) & REV_MASK;

			*pDelayIn++ = In1 + *pNewDll4*pCoefs->dFB41 + *pPrevDll4*pCoefs->dFB42;
			pPrevDll4 = pNewDll4;
			Indx4 = (Indx4 - 4) & REV_MASK;

			Indx = (Indx - 4) & REV_MASK;
			DIndx = (DIndx - 2) & DSPS_MASK;

			*pOutFloat++ = Out1+Out2;
			
		}

	((int32_t *)pStates)[0] = Indx ;
	((int32_t *)pStates)[1] = DIndx ;

}

void SVerbMonoToStereoFloat(int32_t NumInFrames, float *pInFloat, float *pOutFloat, 
						    void *pC, float *pStates)
{

	sCoefsStruct *pCoefs = ((sCoefsStruct *)pC);
	int32_t n_sample;
	float In1, In2, Out1, Out2;
	int32_t Indx1,Indx2,Indx3,Indx4;
	float *pNewDll1, *pNewDll2, *pNewDll3, *pNewDll4;
	float *pPrevDll1, *pPrevDll2, *pPrevDll3, *pPrevDll4, *pDelayIn;
	float	*pDelay = pStates+2;
	float	*pDD1	 = pDelay+0x4000;
	float	*pDD2	 = pDD1+0x800;
	int32_t Indx = ((int32_t *)pStates)[0];
	int32_t DIndx = ((int32_t *)pStates)[1] ;

	Indx1 = (Indx+4+pCoefs->lDelay1) & REV_MASK;
	Indx2 = (Indx+4+pCoefs->lDelay2) & REV_MASK;
	Indx3 = (Indx+4+pCoefs->lDelay3) & REV_MASK;
	Indx4 = (Indx+4+pCoefs->lDelay4) & REV_MASK;

	pPrevDll1 = pDelay+Indx1;
	pPrevDll2 = pDelay+Indx2+1;
	pPrevDll3 = pDelay+Indx3+2;
	pPrevDll4 = pDelay+Indx4+3;

	Indx1 = (Indx1-4)&REV_MASK;
	Indx2 = (Indx2-4)&REV_MASK;
	Indx3 = (Indx3-4)&REV_MASK;
	Indx4 = (Indx4-4)&REV_MASK;

		for (n_sample = 0;n_sample < NumInFrames;n_sample++)
		{

			In1 = In2 = (float)0.7071 * (*pInFloat++) + FPU_DENORM_OFFS;

			Out1 = In1 * pCoefs->directGain;
			Out2 = In2 * pCoefs->directGain;

			In1 *= pCoefs->revGain;
			In2 *= pCoefs->revGain;

			pNewDll1 = pDelay+Indx1;
			pNewDll2 = pDelay+Indx2+1;
			pNewDll3 = pDelay+Indx3+2;
			pNewDll4 = pDelay+Indx4+3;

			dsps( pDD1, DIndx, pCoefs->lDDly1, pCoefs->dDG1, pCoefs->dDsps, pNewDll1, pNewDll2 );
			dsps( pDD2, DIndx, pCoefs->lDDly2, pCoefs->dDG2, pCoefs->dDsps, pNewDll3, pNewDll4 );

			Out1 += *pNewDll1 + *pNewDll3;
			Out2 += *pNewDll2 + *pNewDll4;

			pDelayIn = pDelay + Indx;

			*pDelayIn++ = In1 + *pNewDll1*pCoefs->dFB11 + *pPrevDll1*pCoefs->dFB12;
			pPrevDll1 = pNewDll1;
			Indx1 = (Indx1 - 4) & REV_MASK;

			*pDelayIn++ = In2 + *pNewDll2*pCoefs->dFB21 + *pPrevDll2*pCoefs->dFB22;
			pPrevDll2 = pNewDll2;
			Indx2 = (Indx2 - 4) & REV_MASK;

			*pDelayIn++ = -In2 + *pNewDll3*pCoefs->dFB31 + *pPrevDll3*pCoefs->dFB32;
			pPrevDll3 = pNewDll3;
			Indx3 = (Indx3 - 4) & REV_MASK;

			*pDelayIn++ = In1 + *pNewDll4*pCoefs->dFB41 + *pPrevDll4*pCoefs->dFB42;
			pPrevDll4 = pNewDll4;
			Indx4 = (Indx4 - 4) & REV_MASK;

			Indx = (Indx - 4) & REV_MASK;
			DIndx = (DIndx - 2) & DSPS_MASK;

			*pOutFloat++ = Out1;
			*pOutFloat++ = Out2;
			
		}

	((int32_t *)pStates)[0] = Indx ;
	((int32_t *)pStates)[1] = DIndx ;

}

void SVerbStereoToStereoFloat(int32_t NumInFrames, float *pInFloat, float *pOutFloat, 
						      void *pC, float *pStates)
{

	sCoefsStruct *pCoefs = ((sCoefsStruct *)pC);
	int32_t n_sample;
	float In1, In2, Out1, Out2;
	int32_t Indx1,Indx2,Indx3,Indx4;
	float *pNewDll1, *pNewDll2, *pNewDll3, *pNewDll4;
	float *pPrevDll1, *pPrevDll2, *pPrevDll3, *pPrevDll4, *pDelayIn;
	float	*pDelay = pStates+2;
	float	*pDD1	 = pDelay+0x4000;
	float	*pDD2	 = pDD1+0x800;
	int32_t Indx = ((int32_t *)pStates)[0];
	int32_t DIndx = ((int32_t *)pStates)[1] ;

	Indx1 = (Indx+4+pCoefs->lDelay1) & REV_MASK;
	Indx2 = (Indx+4+pCoefs->lDelay2) & REV_MASK;
	Indx3 = (Indx+4+pCoefs->lDelay3) & REV_MASK;
	Indx4 = (Indx+4+pCoefs->lDelay4) & REV_MASK;

	pPrevDll1 = pDelay+Indx1;
	pPrevDll2 = pDelay+Indx2+1;
	pPrevDll3 = pDelay+Indx3+2;
	pPrevDll4 = pDelay+Indx4+3;

	Indx1 = (Indx1-4)&REV_MASK;
	Indx2 = (Indx2-4)&REV_MASK;
	Indx3 = (Indx3-4)&REV_MASK;
	Indx4 = (Indx4-4)&REV_MASK;

		for (n_sample = 0;n_sample < NumInFrames;n_sample++)
		{

			In1 = (*pInFloat++) + FPU_DENORM_OFFS;
			In2 = (*pInFloat++) + FPU_DENORM_OFFS;

			Out1 = In1 * pCoefs->directGain;
			Out2 = In2 * pCoefs->directGain;

			In1 *= pCoefs->revGain;
			In2 *= pCoefs->revGain;

			pNewDll1 = pDelay+Indx1;
			pNewDll2 = pDelay+Indx2+1;
			pNewDll3 = pDelay+Indx3+2;
			pNewDll4 = pDelay+Indx4+3;

			dsps( pDD1, DIndx, pCoefs->lDDly1, pCoefs->dDG1, pCoefs->dDsps, pNewDll1, pNewDll2 );
			dsps( pDD2, DIndx, pCoefs->lDDly2, pCoefs->dDG2, pCoefs->dDsps, pNewDll3, pNewDll4 );

			Out1 += *pNewDll1 + *pNewDll3;
			Out2 += *pNewDll2 + *pNewDll4;

			pDelayIn = pDelay + Indx;

			*pDelayIn++ = In1 + *pNewDll1*pCoefs->dFB11 + *pPrevDll1*pCoefs->dFB12;
			pPrevDll1 = pNewDll1;
			Indx1 = (Indx1 - 4) & REV_MASK;

			*pDelayIn++ = In2 + *pNewDll2*pCoefs->dFB21 + *pPrevDll2*pCoefs->dFB22;
			pPrevDll2 = pNewDll2;
			Indx2 = (Indx2 - 4) & REV_MASK;

			*pDelayIn++ = -In2 + *pNewDll3*pCoefs->dFB31 + *pPrevDll3*pCoefs->dFB32;
			pPrevDll3 = pNewDll3;
			Indx3 = (Indx3 - 4) & REV_MASK;

			*pDelayIn++ = In1 + *pNewDll4*pCoefs->dFB41 + *pPrevDll4*pCoefs->dFB42;
			pPrevDll4 = pNewDll4;
			Indx4 = (Indx4 - 4) & REV_MASK;

			Indx = (Indx - 4) & REV_MASK;
			DIndx = (DIndx - 2) & DSPS_MASK;

			*pOutFloat++ = Out1;
			*pOutFloat++ = Out2;
			
		}

	((int32_t *)pStates)[0] = Indx ;
	((int32_t *)pStates)[1] = DIndx ;

}

__inline void dsps( float *pDly, int32_t ref, int32_t delay, float dDG1, float dDsps, float *inL, float *inR )
{
	float outL, outR;
	float *pDlyOut; 

    pDlyOut = pDly + ((ref+delay) & DSPS_MASK);
    pDly += (ref & DSPS_MASK);

    outL = dDG1 * (*pDlyOut++) + *inR * dDsps;
	outR = dDG1 * (*pDlyOut) - *inL * dDsps ;

    // here we feed back the output.
	*pDly++ = *inL + dDsps * outR ;
	*pDly = *inR - dDsps * outL ;

	*inL = outL;
	*inR = outR;

}
#endif

__inline void dspsL( int32_t *pDly, int32_t ref, int32_t delay, int32_t dDG1, int32_t dDsps, int32_t *inL, int32_t *inR )
{
	int32_t outL, outR;
	int32_t *pDlyOut; 

    pDlyOut = pDly + ((ref+delay) & DSPS_MASK);
    pDly += (ref & DSPS_MASK);

    outL = (dDG1 * (*pDlyOut++) + *inR * dDsps)>>15;

	outR = (dDG1 * (*pDlyOut) - *inL * dDsps)>>15;

    // here we feed back the output.
	*pDly++ = *inL + ((dDsps * outR)>>15) ;

	*pDly = *inR - ((dDsps * outL)>>15) ;

	*inL = outL;
	*inR = outR;

}
#ifdef _MSC_VER
#pragma optimize( "ty", off )
#endif

