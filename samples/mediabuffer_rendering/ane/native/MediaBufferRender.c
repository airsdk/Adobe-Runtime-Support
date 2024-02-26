#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "FlashRuntimeExtensions.h"
#include <malloc.h>
#include <memory.h>

#ifdef WIN32
# define DLLEXPORT __declspec( dllexport )
# ifndef NULL
#  define NULL	0
# endif // NULL
#else // linux: need --fvisibility=hidden on the compiler command-line
# define DLLEXPORT  __attribute__((visibility("default")))
#endif

#define FALSE	0
#define TRUE	1


/**************************************************************
/* forward declarations of our functions
/**************************************************************/
FREObject setDisplayObjectSource(FREContext ctx, void* functionData, uint32_t argc, FREObject argv[]);
FREObject startChangingMediaBuffer(FREContext ctx, void* functionData, uint32_t argc, FREObject argv[]);
void ContextInitializer(void* extData, const uint8_t* ctxType, FREContext ctx, uint32_t* numFunctionsToSet, const FRENamedFunction** functionsToSet);
void ContextFinalizer(FREContext ctx);


/**************************************************************
/* main entry points
/**************************************************************/

DLLEXPORT void InitExtension(
        void**                 extDataToSet       ,
        FREContextInitializer* ctxInitializerToSet,
        FREContextFinalizer*   ctxFinalizerToSet)
{
	*extDataToSet = NULL;
	*ctxInitializerToSet = ContextInitializer;
	*ctxFinalizerToSet   = ContextFinalizer;
}

DLLEXPORT void DestroyExtension(void* extData)
{
	// not actually guaranteed to be called at all..
}


/**************************************************************
/* context creation and destruction
/**************************************************************/

void ContextInitializer(
        void*                    extData          ,
        const uint8_t*           ctxType          ,
        FREContext               ctx              ,
        uint32_t*                numFunctionsToSet,
        const FRENamedFunction** functionsToSet)
{
	static FRENamedFunction arrFunctions[] = {
		{ (uint8_t*)"setDisplayObjectSource", NULL, setDisplayObjectSource },
		{ (uint8_t*)"startChangingMediaBuffer", NULL, startChangingMediaBuffer }
	};
	*functionsToSet = arrFunctions;
	*numFunctionsToSet = 2;
}

void ContextFinalizer(FREContext ctx)
{
	// per-object tidy-up
}

/**************************************************************
/* setDisplayObjectSource
/* Sets up the display object (arg 2) with a MediaBuffer (arg 1)
/**************************************************************/

FREObject setDisplayObjectSource(FREContext ctx, void* functionData, uint32_t argc, FREObject argv[])
{
	FREObject retVal = (FREObject)FRE_INVALID_OBJECT;
	uint32_t bSuccess = FALSE;
	char strTrace[1024];
	sprintf_s(strTrace, 1024, "FRESetRenderSource called with %d arguments", argc);
	FRETrace(ctx, strTrace);

	if (2 == argc)
	{
		sprintf_s(strTrace, 1024, "FRESetRenderSource media buffer %p, display object %p", argv[0], argv[1]);
		FRETrace(ctx, strTrace);
		FREResult result = FRESetRenderSource(ctx, argv[0], argv[1]);
		sprintf_s(strTrace, 1024, "FRESetRenderSource return value was %d", result);
		FRETrace(ctx, strTrace);
		if (FRE_OK == result) bSuccess = TRUE;
	}
	FRENewObjectFromBool(bSuccess, &retVal);
	return retVal;
}


/**************************************************************
/* startChangingMediaBuffer
/* Sets up a thread to update the media buffer (arg 1)
/**************************************************************/

FREObject startChangingMediaBuffer(FREContext ctx, void* functionData, uint32_t argc, FREObject argv[])
{
	FREObject retVal = (FREObject)FRE_INVALID_OBJECT;
	uint32_t bSuccess = FALSE;
	
	FRETrace(ctx, "Locking media buffer...");
	uint8_t* pData = NULL;
	uint32_t nWidth, nHeight, nStride, nFormat;
	FREResult result = FREMediaBufferLock(ctx, argv[0], &pData, &nWidth, &nHeight, &nStride, &nFormat);
	char strTrace[1024];
	sprintf_s(strTrace, 1024, "FREMediaBufferLock returned %p, size %d x %d, stride %d, format %d", result, nWidth, nHeight, nStride, nFormat);
	FRETrace(ctx, strTrace);
	if (FRE_OK == result)
	{
		sprintf_s(strTrace, 1024, "FREMediaBufferLock memset %p with size %d", pData, nStride * nHeight);
		FRETrace(ctx, strTrace);
		memset(pData, 0x80, nStride * nHeight / 2); // semi-transparent grey..
		FREMediaBufferUnlock(ctx, argv[0], TRUE);
		bSuccess = TRUE;
	}
	
	FRENewObjectFromBool(bSuccess, &retVal);
	return retVal;
}
