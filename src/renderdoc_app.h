#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum RENDERDOC_Version {
	eRENDERDOC_API_Version_1_0_0 = 10000,
	eRENDERDOC_API_Version_1_0_1 = 10001,
	eRENDERDOC_API_Version_1_0_2 = 10002,
	eRENDERDOC_API_Version_1_1_0 = 10100,
	eRENDERDOC_API_Version_1_1_1 = 10101,
	eRENDERDOC_API_Version_1_1_2 = 10102,
	eRENDERDOC_API_Version_1_2_0 = 10200,
	eRENDERDOC_API_Version_1_3_0 = 10300,
	eRENDERDOC_API_Version_1_4_0 = 10400,
	eRENDERDOC_API_Version_1_4_1 = 10401,
	eRENDERDOC_API_Version_1_4_2 = 10402,
	eRENDERDOC_API_Version_1_5_0 = 10500,
	eRENDERDOC_API_Version_1_6_0 = 10600,
} RENDERDOC_Version;

typedef void* RENDERDOC_DevicePointer;
typedef void* RENDERDOC_WindowHandle;

typedef enum RENDERDOC_CaptureOption {
	eRENDERDOC_Option_AllowVSync = 0,
	eRENDERDOC_Option_AllowFullscreen = 1,
	eRENDERDOC_Option_APIValidation = 2,
	eRENDERDOC_Option_CaptureCallstacks = 3,
	eRENDERDOC_Option_CaptureCallstacksOnlyDraws = 4,
	eRENDERDOC_Option_DelayForDebugger = 5,
	eRENDERDOC_Option_VerifyBufferAccess = 6,
	eRENDERDOC_Option_HookIntoChildren = 7,
	eRENDERDOC_Option_RefAllResources = 8,
	eRENDERDOC_Option_SaveAllInitials = 9,
	eRENDERDOC_Option_CaptureAllCmdLists = 10,
	eRENDERDOC_Option_MuteDebugOutput = 11,
} RENDERDOC_CaptureOption;

typedef struct RENDERDOC_API_1_4_0 {
	void (*GetAPIVersion)(int* major, int* minor, int* patch);
	void (*SetCaptureOptionU32)(RENDERDOC_CaptureOption opt, uint32_t val);
	void (*SetCaptureOptionF32)(RENDERDOC_CaptureOption opt, float val);
	uint32_t (*GetCaptureOptionU32)(RENDERDOC_CaptureOption opt);
	float (*GetCaptureOptionF32)(RENDERDOC_CaptureOption opt);
	void (*SetFocusToggleKeys)(void* keys, int num);
	void (*SetCaptureKeys)(void* keys, int num);
	uint32_t (*GetOverlayBits)();
	void (*MaskOverlayBits)(uint32_t And, uint32_t Or);
	void (*RemoveHooks)();
	void (*UnloadCrashHandler)();
	void (*SetCaptureFilePathTemplate)(const char* pathtemplate);
	const char* (*GetCaptureFilePathTemplate)();
	uint32_t (*GetNumCaptures)();
	uint32_t (*GetCapture)(uint32_t idx, char* filename, uint32_t* pathlength, uint64_t* timestamp);
	void (*TriggerCapture)();
	uint32_t (*IsRemoteAccessConnected)();
	void (*LaunchReplayUI)(uint32_t connectTargetControl, const char* cmdline);
	void (*RenderOverlayUserText)(uint32_t x, uint32_t y, const char* text);
	void (*UnloadRemoteAccess)();
	void (*StartFrameCapture)(RENDERDOC_DevicePointer device, RENDERDOC_WindowHandle wndHandle);
	uint32_t (*IsFrameCapturing)();
	void (*EndFrameCapture)(RENDERDOC_DevicePointer device, RENDERDOC_WindowHandle wndHandle);
	void (*TriggerMultiFrameCapture)(uint32_t numFrames);
	void (*SetCaptureFileComments)(const char* filePath, const char* comments);
	void (*DiscardFrameCapture)(RENDERDOC_DevicePointer device, RENDERDOC_WindowHandle wndHandle);
} RENDERDOC_API_1_4_0;

typedef int (*pRENDERDOC_GetAPI)(RENDERDOC_Version version, void** outAPIPointers);

#ifdef __cplusplus
}
#endif
