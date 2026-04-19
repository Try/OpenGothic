/***********************************************************
Copyrights : ksWaves Ltd. 1998.

Provided to Microsoft under contract between ksWaves and Microsoft.

************************************************************/

// OpenGothic port note (2026): the original NT5 source used `long` for every
// fixed-point value and ring-buffer index. On Windows MSVC (all archs) that
// is 32-bit; on 64-bit Linux/macOS `long` is 64-bit, which breaks the
// reinterpret-casts between `float*` state pointers and `long*` index words
// at the head of the state buffer. To keep behaviour byte-identical to the
// reference across platforms we replaced `long` with `int32_t` throughout.
#ifdef __cplusplus
#  include <cstdint>
#else
#  include <stdint.h>
#endif

/****************************************************************************
Const defines :
*****************************************************************************/
#define FPU_DENORM_OFFS (float)1.0E-30

#define BASE_REV_DELAY  0x4000
#define BASE_DSPS_DELAY 0x800

#define DSPS_MASK   0x7ff
#define REV_MASK    0x3fff

/****************************************************************************
Coefs Struct :
*****************************************************************************/
typedef struct
{

	int32_t mySize;
	int32_t myVersion;
	float SampleRate;

	float directGain; 
	int32_t  l_directGain; 
	float revGain; 
	int32_t l_revGain; 

	int32_t lDelay1;
	int32_t lDelay2;
	int32_t lDelay3;
	int32_t lDelay4;

	int32_t lDDly1; 
	int32_t lDDly2; 

	float dDsps;
	int32_t l_dDsps;

	float dDG1;
	int32_t l_dDG1;

	float dDG2; 
	int32_t l_dDG2; 

	float dFB11;
	int32_t l_dFB11;
	float dFB12;
	int32_t l_dFB12;
	float dFB21;
	int32_t l_dFB21;
	float dFB22;
	int32_t l_dFB22;
	float dFB31;
	int32_t l_dFB31;
	float dFB32;
	int32_t l_dFB32;
	float dFB41;
	int32_t l_dFB41;
	float dFB42;
	int32_t l_dFB42;

	float dDamp;
	int32_t l_dDamp;


} sCoefsStruct;

/****************************************************************************
Initialization and control functions :
*****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

#define MAX_16 (float)((uint32_t)0x00008000)

void InitSVerbStates( int32_t *pStates );
int32_t DToF32( float dbl  );
void ConvertCoefsToFix( void *pC );
void InitSVerb( float SampleRate, void  *pCoefs);
void SetSVerb( float InGain, float dRevMix,  float dRevTime, 
			    float dHighFreqRTRatio, void  *pCoefs );



int32_t GetCoefsSize(void);
int32_t GetStatesSize(void);
int32_t GetSVerbVersion(void);

float VerifySampleRate(void  *pCoefs);
int32_t VerifyVersion(void  *pCoefs);
int32_t VerifySize(void  *pCoefs);


#define CLIP_SHORT_TO_SHORT(x)\
			if (x>32767)\
				x = 32767;\
			else if (x<-32768)\
				x = -32768;

/****************************************************************************
//Process Functions :
*****************************************************************************/

__inline void dsps( float *pDly, int32_t ref, int32_t delay, float dDG1, float dDsps, float *inL, float *inR );
__inline void dspsL( int32_t *pDly, int32_t ref, int32_t delay, int32_t dDG1, int32_t dDsps, int32_t *inL, int32_t *inR );

void SVerbMonoToMonoShort(int32_t NumInFrames, short *pInShort, short *pOutShort, 
						 void  *pCoefs, int32_t *pStates);

void SVerbMonoToStereoShort(int32_t NumInFrames, short *pInShort, short *pOutShort, 
						 void  *pCoefs, int32_t *pStates);

void SVerbStereoToStereoShort(int32_t NumInFrames, short *pInShort, short *pOutShort, 
						 void  *pCoefs, int32_t *pStates);

void SVerbMonoToMonoFloat(int32_t NumInFrames, float *pInFloat, float *pOutFloat, 
						 void  *pCoefs, float *pStates);

void SVerbMonoToStereoFloat(int32_t NumInFrames, float *pInFloat, float *pOutFloat, 
						 void  *pCoefs, float *pStates);

void SVerbStereoToStereoFloat(int32_t NumInFrames, float *pInFloat, float *pOutFloat, 
						 void  *pCoefs, float *pStates);


#ifdef __cplusplus
}
#endif
