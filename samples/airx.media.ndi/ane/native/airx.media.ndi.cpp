
// fix for Windows SDK compatibility
#if  defined(WIN32) && !(defined(__GNUC__) && __GNUC__ >= 9)
#include <emmintrin.h>
__m128i _mm_loadu_si64(const void*mem_addr)
{
        return _mm_loadl_epi64((const __m128i*)mem_addr);
}
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "FlashRuntimeExtensions.h"
#include <cstdio>
#include <chrono>
#include <Processing.NDI.Lib.h>

#ifdef WIN32
# define DLLEXPORT __declspec( dllexport )
# ifndef NULL
#  define NULL	0
# endif // NULL

// include the library too
  #ifdef _WIN64
  #pragma comment(lib, "Processing.NDI.Lib.x64.lib")
  #else // _WIN64
  #pragma comment(lib, "Processing.NDI.Lib.x86.lib")
  #endif // _WIN64

#else // linux: need --fvisibility=hidden on the compiler command-line
# define DLLEXPORT  __attribute__((visibility("default")))
#endif
typedef char* char_pointer;


#define USE_BACKGROUND_THREAD true


/**************************************************************
/* Class to provide the necessary context and functionality
/**************************************************************/
class NDIVideoRenderer
{
	FREContext m_ctx;
	NDIlib_source_t m_NDI_source;
	NDIlib_recv_instance_t m_pNDI_recv;
public:
	NDIVideoRenderer(FREContext ctx);
	~NDIVideoRenderer();
	
	bool initialize();
	bool terminate();
	
	bool listSources(FREObject list);
	bool requestSources(FREObject list);
	
	bool startCapture(const char* source);
	bool stopCapture();
	
	bool render(FREObject mediaBuffer);
	
private:
	FREMutex m_mutex;
	void blitFrame(NDIlib_video_frame_v2_t* pFrame, FREObject mediaBuffer);

	// requesting sources thread
	static unsigned long RequestSourcesThreadFnc(void* param);
	int m_requestSourcesThreadTimeout;
	void requestSourcesThread();
	int m_numSources;
	char_pointer* m_arrSources;
	void cleanUpSourceList();
	// the capture thread
	char* m_strSourceName;
	FREThread m_videoThread;
	static unsigned long CaptureVideoThreadFnc(void* param);
	void captureVideoThread();
	bool m_bCapturingVideo;
	int m_numFrames;
	// frame that's transferred between threads
	NDIlib_video_frame_v2_t m_frameInfo;
};

extern "C" {

/**************************************************************
/* forward declarations of our functions
/**************************************************************/
FREObject initializeNDI (FREContext ctx, void* functionData, uint32_t argc, FREObject argv[]);
FREObject listSources   (FREContext ctx, void* functionData, uint32_t argc, FREObject argv[]);
FREObject requestSources(FREContext ctx, void* functionData, uint32_t argc, FREObject argv[]);
FREObject startCapture  (FREContext ctx, void* functionData, uint32_t argc, FREObject argv[]);
FREObject stopCapture   (FREContext ctx, void* functionData, uint32_t argc, FREObject argv[]);
FREObject closeNDI      (FREContext ctx, void* functionData, uint32_t argc, FREObject argv[]);
FREObject render        (FREContext ctx, void* functionData, uint32_t argc, FREObject argv[]);
FREObject setTarget     (FREContext ctx, void* functionData, uint32_t argc, FREObject argv[]);
FREObject startRendering(FREContext ctx, void* functionData, uint32_t argc, FREObject argv[]);
FREObject stopRendering (FREContext ctx, void* functionData, uint32_t argc, FREObject argv[]);


void ContextInitializer(void* extData, const uint8_t* ctxType, FREContext ctx, uint32_t* numFunctionsToSet, const FRENamedFunction** functionsToSet);
void ContextFinalizer  (FREContext ctx);


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
		{ (uint8_t*)"initializeNDI",  NULL, initializeNDI  },
		{ (uint8_t*)"listSources",    NULL, listSources    },
		{ (uint8_t*)"requestSources", NULL, requestSources },
		{ (uint8_t*)"startCapture",   NULL, startCapture   },
		{ (uint8_t*)"stopCapture",    NULL, stopCapture    },
		{ (uint8_t*)"closeNDI",       NULL, closeNDI       },
		{ (uint8_t*)"render",         NULL, render         },
		{ (uint8_t*)"setTarget",      NULL, setTarget      },
		{ (uint8_t*)"startRendering", NULL, startRendering },
		{ (uint8_t*)"stopRendering",  NULL, stopRendering  }
	};
	*functionsToSet = arrFunctions;
	*numFunctionsToSet = sizeof(arrFunctions) / sizeof(FRENamedFunction);
}

void ContextFinalizer(FREContext ctx)
{
	// per-object tidy-up
}

} // Extern C

/**************************************************************
/* initializeNDI
/* Initialize the NDI library
/**************************************************************/

FREObject initializeNDI(FREContext ctx, void* functionData, uint32_t argc, FREObject argv[])
{
	FREObject retVal = (FREObject)FRE_INVALID_OBJECT;
	uint32_t bSuccess = FALSE;
	char strTrace[1024];
	sprintf_s(strTrace, 1024, "initializeNDI called with %d arguments", argc);
	FRETrace(ctx, strTrace);

	// we should not be called twice...
	void* pData = NULL;
	FREGetContextNativeData(ctx, &pData);
	if (pData != NULL)
	{
		FRETrace(ctx, "initializeNDI is being called multiple times. Ignoring this call");
	}
	else
	{
		// create a new handler object and set it up
		NDIVideoRenderer* pRenderer = new NDIVideoRenderer(ctx);
		bSuccess = pRenderer->initialize();
		FRESetContextNativeData(ctx, (void*)pRenderer);
	}

	FRENewObjectFromBool(bSuccess, &retVal);
	return retVal;
}

/**************************************************************
/* closeNDI
/* Terminate the NDI library
/**************************************************************/

FREObject closeNDI(FREContext ctx, void* functionData, uint32_t argc, FREObject argv[])
{
	FREObject retVal = (FREObject)FRE_INVALID_OBJECT;
	uint32_t bSuccess = FALSE;
	char strTrace[1024];
	sprintf_s(strTrace, 1024, "closeNDI called with %d arguments", argc);
	FRETrace(ctx, strTrace);
	
	// get the existing handler
	NDIVideoRenderer* pRenderer = NULL;
	FREGetContextNativeData(ctx, (void**)&pRenderer);
	if (pRenderer == NULL)
	{
		FRETrace(ctx, "closeNDI is being called without a successful initialization. Ignoring this call");
	}
	else
	{
		bSuccess = pRenderer->terminate();
		FRESetContextNativeData(ctx, NULL);
		FRESetContextActionScriptData(ctx, NULL);
		delete pRenderer;
	}

	FRENewObjectFromBool(bSuccess, &retVal);
	return retVal;
}

/**************************************************************
/* listSources
/* Provide a list of all current sources in the input vector
/**************************************************************/

FREObject listSources(FREContext ctx, void* functionData, uint32_t argc, FREObject argv[])
{
	FREObject retVal = (FREObject)FRE_INVALID_OBJECT;
	uint32_t bSuccess = FALSE;
	char strTrace[1024];
	sprintf_s(strTrace, 1024, "listSources called with %d arguments", argc);
	FRETrace(ctx, strTrace);

	// get the existing handler
	NDIVideoRenderer* pRenderer = NULL;
	FREGetContextNativeData(ctx, (void**)&pRenderer);
	if (pRenderer == NULL)
	{
		FRETrace(ctx, "listSources is being called without a successful initialization. Ignoring this call");
	}
	// should be called with 1 argument which is the vector that we add things to
	else if (1 == argc)
	{
		bSuccess = pRenderer->listSources(argv[0]);
	}
	else
	{
		FRETrace(ctx, "listSources needs to be passed a vector object");
	}

	FRENewObjectFromBool(bSuccess, &retVal);
	return retVal;
}

/**************************************************************
/* requestSources
/* Kick off a thread to retrieve the sources, and send them
/* via the event dispatch mechanism
/**************************************************************/

FREObject requestSources(FREContext ctx, void* functionData, uint32_t argc, FREObject argv[])
{
	FREObject retVal = (FREObject)FRE_INVALID_OBJECT;
	uint32_t bSuccess = FALSE;
	char strTrace[1024];
	sprintf_s(strTrace, 1024, "requestSources called with %d arguments", argc);
	FRETrace(ctx, strTrace);

	// get the existing handler
	NDIVideoRenderer* pRenderer = NULL;
	FREGetContextNativeData(ctx, (void**)&pRenderer);
	if (pRenderer == NULL)
	{
		FRETrace(ctx, "requestSources is being called without a successful initialization. Ignoring this call");
	}
	// should be called with 1 argument which is the timeout in ms
	else if (1 == argc)
	{
		bSuccess = pRenderer->requestSources(argv[0]);
	}
	else
	{
		FRETrace(ctx, "requestSources needs to be passed a timeout value");
	}

	FRENewObjectFromBool(bSuccess, &retVal);
	return retVal;
}

/**************************************************************
/* startCapture
/* Start capturing the video feed from the given source
/**************************************************************/

FREObject startCapture(FREContext ctx, void* functionData, uint32_t argc, FREObject argv[])
{
	FREObject retVal = (FREObject)FRE_INVALID_OBJECT;
	uint32_t bSuccess = FALSE;
	char strTrace[1024];
	sprintf_s(strTrace, 1024, "startCapture called with %d arguments", argc);
	FRETrace(ctx, strTrace);
	const char* source = NULL;
	uint32_t strLen = 0;

	// get the existing handler
	NDIVideoRenderer* pRenderer = NULL;
	FREGetContextNativeData(ctx, (void**)&pRenderer);
	if (pRenderer == NULL)
	{
		FRETrace(ctx, "startCapture is being called without a successful initialization. Ignoring this call");
	}
	// should be called with 1 argument which is the source we should connect to
	else if (1 == argc && (FRE_OK == FREGetObjectAsUTF8(argv[0], &strLen, (const uint8_t**)&source)))
	{
		bSuccess = pRenderer->startCapture(source);
	}
	else
	{
		FRETrace(ctx, "startCapture needs to be passed a string being the NDI source to connect to");
	}

	FRENewObjectFromBool(bSuccess, &retVal);
	return retVal;
}

/**************************************************************
/* stopCapture
/* Stop capturing the video feed
/**************************************************************/

FREObject stopCapture(FREContext ctx, void* functionData, uint32_t argc, FREObject argv[])
{
	FREObject retVal = (FREObject)FRE_INVALID_OBJECT;
	uint32_t bSuccess = FALSE;
	char strTrace[1024];
	sprintf_s(strTrace, 1024, "stopCapture called with %d arguments", argc);
	FRETrace(ctx, strTrace);

	// get the existing handler
	NDIVideoRenderer* pRenderer = NULL;
	FREGetContextNativeData(ctx, (void**)&pRenderer);
	if (pRenderer == NULL)
	{
		FRETrace(ctx, "stopCapture is being called without a successful initialization. Ignoring this call");
	}
	else
	{
		bSuccess = pRenderer->stopCapture();
	}

	FRENewObjectFromBool(bSuccess, &retVal);
	return retVal;
}

/**************************************************************
/* render
/* Render the latest frame available from NDI
/**************************************************************/

FREObject render(FREContext ctx, void* functionData, uint32_t argc, FREObject argv[])
{
	FREObject retVal = (FREObject)FRE_INVALID_OBJECT;
	uint32_t bSuccess = FALSE;
	// get the existing handler
	NDIVideoRenderer* pRenderer = NULL;
	FREGetContextNativeData(ctx, (void**)&pRenderer);
	FREObject mediaBuffer = NULL;
	FREGetContextActionScriptData(ctx, &mediaBuffer);
	if (pRenderer && mediaBuffer)
	{
		bSuccess = pRenderer->render(mediaBuffer);
	}

	FRENewObjectFromBool(bSuccess, &retVal);
	return retVal;
}

/**************************************************************
/* setTarget
/* Set up our mediabuffer render target using the provided media buffer
/**************************************************************/

FREObject setTarget(FREContext ctx, void* functionData, uint32_t argc, FREObject argv[])
{
	FREObject retVal = (FREObject)FRE_INVALID_OBJECT;
	uint32_t bSuccess = FALSE;
	char strTrace[1024];
	sprintf_s(strTrace, 1024, "setTarget called with %d arguments", argc);
	FRETrace(ctx, strTrace);

	// arg 0 = the target sprite
	// arg 1 = the mediabuffer
	if (2 == argc)
	{
		sprintf_s(strTrace, 1024, "setTarget - new media buffer is %p", (void*)((uintptr_t)argv[1] & ~7));
		FRETrace(ctx, strTrace);
		bSuccess = (FRE_OK == FRESetRenderSource(ctx, argv[1], argv[0]));
		// we need to keep hold of the mediabuffer so that we can update it
		FRESetContextActionScriptData(ctx, argv[1]);
	}

	FRENewObjectFromBool(bSuccess, &retVal);
	return retVal;
}


/**************************************************************
/* startRendering
/* Starts the thread to populate the video buffers.
/**************************************************************/
FREObject startRendering(FREContext ctx, void* functionData, uint32_t argc, FREObject argv[])
{
	return NULL;
}

/**************************************************************
/* stopRendering
/* Stops the thread doing the rendering.
/**************************************************************/
FREObject stopRendering (FREContext ctx, void* functionData, uint32_t argc, FREObject argv[])
{
	return NULL;
}



/**************************************************************
/* IMPLEMENTATION
/**************************************************************/

NDIVideoRenderer::NDIVideoRenderer(FREContext ctx)
: m_ctx(ctx)
, m_pNDI_recv(NULL)
, m_numSources(0)
, m_arrSources(NULL)
, m_bCapturingVideo(false)
, m_numFrames(0)
, m_strSourceName(NULL)
{
	m_frameInfo.p_data = NULL;
	m_NDI_source.p_ndi_name = NULL;
	m_NDI_source.p_url_address = NULL;
	FRERecursiveMutexInit(&m_mutex);
}

NDIVideoRenderer::~NDIVideoRenderer()
{
	cleanUpSourceList();
	FRERecursiveMutexDestroy(&m_mutex);
	if (m_NDI_source.p_ndi_name)
	{
		delete[] m_NDI_source.p_ndi_name;
	}
	if (m_NDI_source.p_url_address)
	{
		delete[] m_NDI_source.p_url_address;
	}
	if (m_strSourceName)
	{
		delete[] m_strSourceName;
	}
}

void NDIVideoRenderer::cleanUpSourceList()
{
	if (m_arrSources)
	{
		FRERecursiveMutexLock(&m_mutex);
		for (int i = 0; i < m_numSources; i++) delete[] m_arrSources[i];
		m_numSources = 0;
		delete[] m_arrSources;
		m_arrSources = NULL;
		FRERecursiveMutexUnlock(&m_mutex);
	}
}

bool NDIVideoRenderer::initialize()
{
	return NDIlib_initialize();
}

bool NDIVideoRenderer::terminate()
{
	NDIlib_destroy();
	return true; // always works...
}

bool NDIVideoRenderer::listSources(FREObject list)
{
	bool bSuccess = false;
#if original_code
	// get the number of sources from NDI -> original code below (synchronous...)
	NDIlib_find_instance_t pNDI_find = NDIlib_find_create_v2();
	if (pNDI_find)
	{
		FRETrace(m_ctx, "Listing sources...");
		uint32_t no_sources = 0;
		// Bizarrely .. this can't find anything unless we give it a bit of time?
		NDIlib_find_wait_for_sources(pNDI_find, 1000);

		const NDIlib_source_t* p_sources = NDIlib_find_get_current_sources(pNDI_find, &no_sources);
		char strTrace1[1024];
		sprintf_s(strTrace1, 1024, "Found %d sources", no_sources);
		FRETrace(m_ctx, strTrace1);
		bSuccess = (FRE_OK == FRESetArrayLength(list, no_sources));
		for (uint32_t i = 0; i < no_sources; i++)
		{
			FREObject hString = NULL;
			const char* strVal = p_sources[i].p_ndi_name;
			uint32_t strLen = ::strlen(strVal);
			if (strLen < 1000)
			{
				char strTrace[1024];
				sprintf_s(strTrace, 1024, "Source %d = %s", i, strVal);
				FRETrace(m_ctx, strTrace);
			}
			if (FRE_OK != FRENewObjectFromUTF8(strLen, (const uint8_t*)strVal, &hString) ||
				FRE_OK != FRESetArrayElementAt(list, i, hString))
				bSuccess = false;
		}
		NDIlib_find_destroy(pNDI_find);
	}
#endif // original_code
	FRETrace(m_ctx, "Getting the list of sources");
	// get the list that should have been created already from our call to "requestSources"
	FRERecursiveMutexLock(&m_mutex);
	bSuccess = (FRE_OK == FRESetArrayLength(list, m_numSources));
	FRETrace(m_ctx, "Set the length");
	for (int i = 0; i < m_numSources && bSuccess; i++)
	{
		FREObject hString = NULL;
		const char* strVal = m_arrSources[i];
		uint32_t strLen = ::strlen(strVal);
		FRETrace(m_ctx, "Setting a string");
		if (FRE_OK != FRENewObjectFromUTF8(strLen, (const uint8_t*)strVal, &hString) ||
			FRE_OK != FRESetArrayElementAt(list, i, hString))
			bSuccess = false;
	}
	FRERecursiveMutexUnlock(&m_mutex);
	return bSuccess;
}

unsigned long NDIVideoRenderer::RequestSourcesThreadFnc(void* param)
{
	NDIVideoRenderer* pThis = (NDIVideoRenderer*)param;
	pThis->requestSourcesThread();
	return 0;
}

bool NDIVideoRenderer::requestSources(FREObject timeoutMS)
{
	bool bSuccess = false;
	// get the arg
	if (FRE_OK == FREGetObjectAsInt32(timeoutMS, &m_requestSourcesThreadTimeout))
	{
		cleanUpSourceList();
		// kick off a thread using the timeout value..
		FREThread newThread = FRENullThread();
		bSuccess = FREThreadCreate(&newThread, (FREThreadFunction)&NDIVideoRenderer::RequestSourcesThreadFnc, this);
	}	
	
	return bSuccess;
}

void NDIVideoRenderer::requestSourcesThread()
{
	// get the number of sources from NDI. Could switch this to an ongoing thread that continues to wait
	// and then notifies every time the list has changed? Would need a "startFind" and "stopFind" mechanism..
	NDIlib_find_instance_t pNDI_find = NDIlib_find_create_v2();
	if (pNDI_find)
	{
		uint32_t no_sources = 0;
		NDIlib_find_wait_for_sources(pNDI_find, m_requestSourcesThreadTimeout);

		const NDIlib_source_t* p_sources = NDIlib_find_get_current_sources(pNDI_find, &no_sources);
		if (no_sources)
		{
			FRERecursiveMutexLock(&m_mutex);
			m_numSources = no_sources;
			m_arrSources = new char_pointer[m_numSources];
			for (int i = 0; i < m_numSources; i++)
			{
				const char* strVal = p_sources[i].p_ndi_name;
				uint32_t strLen = ::strlen(strVal);
				if (strLen < 1000)
				{
					char strTrace[1024];
					sprintf_s(strTrace, 1024, "Source %d = %s", i, strVal);
					FRETrace(m_ctx, strTrace);
				}
				m_arrSources[i] = new char[strLen + 1];
				strcpy( m_arrSources[i], strVal );
			}
			FRERecursiveMutexUnlock(&m_mutex);
		}
		NDIlib_find_destroy(pNDI_find);
		// say we're done
		FREDispatchStatusEventAsync(m_ctx, (const uint8_t*)"source_list", (const uint8_t*)"status");
	}
}

bool NDIVideoRenderer::startCapture(const char* source)
{
	if (m_pNDI_recv)
	{
		FRETrace(m_ctx, "A receive context already exists - ignoring this call");
		return false;
	}
	bool bSuccess = false;

	if (USE_BACKGROUND_THREAD)
	{
		FRETrace(m_ctx, "Capturing in a different thread...");
		m_strSourceName = new char[ ::strlen(source) + 1 ];
		::strcpy(m_strSourceName, source);
		m_videoThread = FRENullThread();
		bSuccess = FREThreadCreate(&m_videoThread, (FREThreadFunction)&NDIVideoRenderer::CaptureVideoThreadFnc, this);
	}
	else
	{
		// connect
		NDIlib_recv_create_v3_t create_settings;
		create_settings.color_format = NDIlib_recv_color_format_BGRX_BGRA;
		m_pNDI_recv = NDIlib_recv_create_v3(&create_settings);
		if (m_pNDI_recv)
		{
			// connect to the source
			NDIlib_source_t ndiSource;
			ndiSource.p_ndi_name = source;
			FRETrace(m_ctx, "NDI requesting to connect:");
			FRETrace(m_ctx, source);
			NDIlib_recv_connect(m_pNDI_recv, &ndiSource);

			bSuccess = true;
		}
	}
	return bSuccess;
}

bool NDIVideoRenderer::stopCapture()
{
	bool bSuccess = false;
	if (m_bCapturingVideo)
	{
		FRETrace(m_ctx, "Stopping the video thread...");
		m_bCapturingVideo = false;
		FREThreadJoin(m_videoThread);
		FRETrace(m_ctx, "The video thread has stopped");
	}
	if (m_pNDI_recv)
	{
		NDIlib_recv_connect(m_pNDI_recv, NULL);
		NDIlib_recv_destroy(m_pNDI_recv);
		m_pNDI_recv = NULL;
		bSuccess = true;
	}
	return bSuccess;
}

void NDIVideoRenderer::blitFrame(NDIlib_video_frame_v2_t* pFrame, FREObject mediaBuffer)
{
	char strType[5];
	memcpy(strType, &(pFrame->FourCC), 4);
	strType[4] = 0;
	char strTrace[1024];
	sprintf_s(strTrace, 1024, "Video data received (%dx%d). Bytes %d, type %s (0x%08X), timestamp %lld", pFrame->xres, pFrame->yres, pFrame->line_stride_in_bytes, strType, pFrame->FourCC, pFrame->timestamp);
	FRETrace(m_ctx, strTrace);

	// copy this into the buffer
	uint8_t* pData = NULL;
	uint32_t nWidth = 0;
	uint32_t nHeight = 0;
	uint32_t nStride = 0;
	uint32_t nFormat = 0;
	FREMediaBufferLock(m_ctx, mediaBuffer, &pData, &nWidth, &nHeight, &nStride, &nFormat);
	sprintf_s(strTrace, 1024, "Media buffer %p data %p, size %d x %d, stride %d, format %d", (void*)((uintptr_t)mediaBuffer & ~7), pData, nWidth, nHeight, nStride, nFormat);
	FRETrace(m_ctx, strTrace);

	if (nFormat == 5 /* 32-bit */ && pFrame->FourCC == NDIlib_FourCC_type_BGRA)
	{
		bool bReverse = false;
		if ((int32_t)nStride < 0)
		{
			bReverse = true;
			nStride = -(int32_t)nStride;
		}

		// sizes might be different: how much to copy?  just crop for now.
		// TODO:
		// (a) scaling
		// (b) YUV formats
		// These kinds of capabilities should be exposed from the AIR runtime to take advantage
		// of the existing assembler/SIMD-accelerated rendering functions...

		uint32_t nStrideToCopy = min(nStride, (uint32_t)pFrame->line_stride_in_bytes);
		uint32_t nLinesToCopy = min(nHeight, (uint32_t)pFrame->yres);
		uint8_t* pSource = pFrame->p_data;
		for (uint32_t y = 0; y < nLinesToCopy; y++)
		{
			::memcpy(pData, pSource, nStrideToCopy);
			if (bReverse) pData -= nStride;
			else pData += nStride;
			pSource += pFrame->line_stride_in_bytes;
		}
	}

	FREMediaBufferUnlock(m_ctx, mediaBuffer, TRUE);
	NDIlib_recv_free_video_v2(m_pNDI_recv, pFrame);
}

bool NDIVideoRenderer::render(FREObject mediaBuffer)
{
	if (USE_BACKGROUND_THREAD)
	{
		bool hadFrame = false;
		FRERecursiveMutexLock(&m_mutex);
		// do we have frame data?
		if ( m_frameInfo.p_data )
		{
			hadFrame = true;
			FRETrace(m_ctx, "Video frame is waiting");
			blitFrame(&m_frameInfo, mediaBuffer);
			m_frameInfo.p_data = NULL;
		}
		FRERecursiveMutexUnlock(&m_mutex);
		return hadFrame;
	}

	// capture within the render function itself -> do we have a frame?
	NDIlib_video_frame_v2_t video_frame;
	NDIlib_frame_type_e result = NDIlib_recv_capture_v2(m_pNDI_recv, &video_frame, nullptr, nullptr, 5);
	switch(result)
	{
		case NDIlib_frame_type_video:
		{
			blitFrame(&video_frame, mediaBuffer);
			return true;
		}
		break;
		case NDIlib_frame_type_error:
			FRETrace(m_ctx, "Error receiving frame - connection closed");
			break;
		case NDIlib_frame_type_status_change:
			FRETrace(m_ctx, "Frame status has changed");
			break;
		case NDIlib_frame_type_none:
			FRETrace(m_ctx, "No frames available");
			break;
	}

	return false;
}


unsigned long NDIVideoRenderer::CaptureVideoThreadFnc(void* param)
{
	NDIVideoRenderer* pThis = (NDIVideoRenderer*)param;
	pThis->m_bCapturingVideo = true;
	pThis->captureVideoThread();
	return 0;
}

void NDIVideoRenderer::captureVideoThread()
{
	FRETrace(m_ctx, "Starting video capture thread");
	bool bConnected = false;
	
	// setup
	NDIlib_recv_create_v3_t create_settings;
	create_settings.color_format = NDIlib_recv_color_format_BGRX_BGRA;
	m_pNDI_recv = NDIlib_recv_create_v3(&create_settings);
	if (m_pNDI_recv)
	{
		// connect to the source
		NDIlib_source_t ndiSource;
		ndiSource.p_ndi_name = m_strSourceName;
		FRETrace(m_ctx, "NDI requesting to connect:");
		FRETrace(m_ctx, m_strSourceName);
		NDIlib_recv_connect(m_pNDI_recv, &ndiSource);

		NDIlib_video_frame_v2_t video_frame;
		const int kTimeoutMS = 20;
		while (m_bCapturingVideo)
		{
			NDIlib_frame_type_e result = NDIlib_recv_capture_v2(m_pNDI_recv, &video_frame, nullptr, nullptr, kTimeoutMS);
			switch(result)
			{
				case NDIlib_frame_type_video:
				{
					if (!m_numFrames)
					{
						// dispatch the "video.started" event so that we can have the video size queried
						FREDispatchStatusEventAsync(m_ctx, (const uint8_t*)"video.started", (const uint8_t*)"status");
					}

					char strType[5];
					memcpy(strType, &(video_frame.FourCC), 4);
					strType[4] = 0;
					char strTrace[1024];
					sprintf_s(strTrace, 1024, "Video data received (%dx%d). Bytes %d, type %s (0x%08X), timestamp %lld", video_frame.xres, video_frame.yres, video_frame.line_stride_in_bytes, strType, video_frame.FourCC, video_frame.timestamp);
					FRETrace(m_ctx, strTrace);

					// copy the frame information
					FRERecursiveMutexLock(&m_mutex);
					// was there still data in there?
					if ( m_frameInfo.p_data )
					{
						NDIlib_recv_free_video_v2(m_pNDI_recv, &m_frameInfo);
					}
					memcpy( &m_frameInfo, &video_frame, sizeof(NDIlib_video_frame_v2_t) );
					FRERecursiveMutexUnlock(&m_mutex);
					
					m_numFrames++;
				}
				break;
				case NDIlib_frame_type_error:
					FRETrace(m_ctx, "Error receiving frame - connection closed");
					break;
				case NDIlib_frame_type_status_change:
					FRETrace(m_ctx, "Frame status has changed");
					break;
				case NDIlib_frame_type_none:
					FRETrace(m_ctx, "No frames available"); // no need to sleep as the main call waits for kTimeoutMS
					break;
			}
		}
		// teardown
		FRERecursiveMutexLock(&m_mutex);
		if ( m_frameInfo.p_data )
		{
			NDIlib_recv_free_video_v2(m_pNDI_recv, &m_frameInfo);
			m_frameInfo.p_data = NULL;
		}
		FRERecursiveMutexUnlock(&m_mutex);
	}
	FRETrace(m_ctx, "Stopping video capture thread");
}
