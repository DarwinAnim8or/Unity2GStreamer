// Example low level rendering Unity plugin

#include "PlatformBase.h"
#include "RenderAPI.h"

#include <assert.h>
#include <math.h>
#include <vector>
#include <iostream>

//#include <gl/GL.h>
#include "gl3w/glcorearb.h"
#include "gl3w/gl3w.h"

#include "gstRtsp.h"
#include "CCTVServer.h"
#include <d3d11.h>
#include <d3d.h>
#include <thread>
#include <future>

// --------------------------------------------------------------------------
// SetTimeFromUnity, an example function we export which is called by one of the scripts.

static float g_Time;

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetTimeFromUnity (float t) { 
	g_Time = t; 
}


// --------------------------------------------------------------------------
// SetTextureFromUnity, an example function we export which is called by one of the scripts.
// We will be using this to handle the frame we just got rendered by the unity side.

static void* g_TextureHandle = NULL;
static int   g_TextureWidth  = 0;
static int   g_TextureHeight = 0;

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetTextureFromUnity(void* textureHandle, int w, int h)
{
	// A script calls this at initialization time; just remember the texture pointer here.
	// Will update texture pixels each frame from the plugin rendering event (texture update
	// needs to happen on the rendering thread).
	g_TextureHandle = textureHandle;
	g_TextureWidth = w;
	g_TextureHeight = h;
}

// --------------------------------------------------------------------------
// UnitySetInterfaces

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);

static IUnityInterfaces* s_UnityInterfaces = NULL;
static IUnityGraphics* s_Graphics = NULL;

static CCTVServer* g_Server = nullptr;

extern "C" void	UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
	s_UnityInterfaces = unityInterfaces;
	s_Graphics = s_UnityInterfaces->Get<IUnityGraphics>();
	s_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);
	
#if SUPPORT_VULKAN
	if (s_Graphics->GetRenderer() == kUnityGfxRendererNull)
	{
		extern void RenderAPI_Vulkan_OnPluginLoad(IUnityInterfaces*);
		RenderAPI_Vulkan_OnPluginLoad(unityInterfaces);
	}
#endif // SUPPORT_VULKAN

	// Run OnGraphicsDeviceEvent(initialize) manually on plugin load
	OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);

	//Start our server
	g_Server = new CCTVServer(3001);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
	s_Graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
	delete g_Server;
}

#if UNITY_WEBGL
typedef void	(UNITY_INTERFACE_API * PluginLoadFunc)(IUnityInterfaces* unityInterfaces);
typedef void	(UNITY_INTERFACE_API * PluginUnloadFunc)();

extern "C" void	UnityRegisterRenderingPlugin(PluginLoadFunc loadPlugin, PluginUnloadFunc unloadPlugin);

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API RegisterPlugin()
{
	UnityRegisterRenderingPlugin(UnityPluginLoad, UnityPluginUnload);
}
#endif

// --------------------------------------------------------------------------
// GraphicsDeviceEvent


static RenderAPI* s_CurrentAPI = NULL;
static UnityGfxRenderer s_DeviceType = kUnityGfxRendererNull;


static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
	// Create graphics API implementation upon initialization
	if (eventType == kUnityGfxDeviceEventInitialize)
	{
		assert(s_CurrentAPI == NULL);
		s_DeviceType = s_Graphics->GetRenderer();
		s_CurrentAPI = CreateRenderAPI(s_DeviceType);
	}

	// Let the implementation process the device related events
	if (s_CurrentAPI)
	{
		s_CurrentAPI->ProcessDeviceEvent(eventType, s_UnityInterfaces);
	}

	// Cleanup graphics API implementation upon shutdown
	if (eventType == kUnityGfxDeviceEventShutdown)
	{
		delete s_CurrentAPI;
		s_CurrentAPI = NULL;
		s_DeviceType = kUnityGfxRendererNull;
	}
}



// --------------------------------------------------------------------------
// OnRenderEvent
// This will be called for GL.IssuePluginEvent script calls; eventID will
// be the integer passed to IssuePluginEvent. In this example, we just ignore
// that value.


static void ModifyTexturePixels()
{
	void* textureHandle = g_TextureHandle;
	int width = g_TextureWidth;
	int height = g_TextureHeight;
	if (!textureHandle)
		return;

	int textureRowPitch;
	void* textureDataPtr = s_CurrentAPI->BeginModifyTexture(textureHandle, width, height, &textureRowPitch);
	if (!textureDataPtr)
		return;

	const float t = g_Time * 4.0f;

	unsigned char* dst = (unsigned char*)textureDataPtr;
	for (int y = 0; y < height; ++y)
	{
		unsigned char* ptr = dst;
		for (int x = 0; x < width; ++x)
		{
			// Simple "plasma effect": several combined sine waves
			int vv = int(
				(127.0f + (127.0f * sinf(x / 7.0f + t))) +
				(127.0f + (127.0f * sinf(y / 5.0f - t))) +
				(127.0f + (127.0f * sinf((x + y) / 6.0f - t))) +
				(127.0f + (127.0f * sinf(sqrtf(float(x*x + y*y)) / 4.0f - t)))
				) / 4;

			// Write the texture pixel
			ptr[0] = vv;
			ptr[1] = vv;
			ptr[2] = vv;
			ptr[3] = vv;

			// To next pixel (our pixels are 4 bpp)
			ptr += 4;
		}

		// To next image row
		dst += textureRowPitch;
	}

	s_CurrentAPI->EndModifyTexture(textureHandle, width, height, textureRowPitch, textureDataPtr);
}

unsigned int cur_RT = 0;
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SubmitRT(unsigned int rt) {
	cur_RT = rt;
}

unsigned char* frameData = nullptr;
unsigned char* flippedData = nullptr;
int frameLength = 0;

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UploadFrame(unsigned char* data, int length) {
	//if (frameData) delete[] frameData; //already done by EndModifyTexture

	//Allocate the buffer:
	if (length != frameLength) {
		if (frameData) delete[] frameData;
		if (flippedData) delete[] flippedData;
		frameLength = length;

		frameData = new unsigned char[length];
		flippedData = new unsigned char[length];
	}

	//Copy our memory:
	/*for (int i = 0; i < length; i++) {
		frameData[i] = data[i];
	}*/

	memcpy(frameData, data, length);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UpdateRakNet() {
	if (g_Server) g_Server->Update();
	else std::cout << "No g_Server active!" << std::endl;
}

bool g_ReadFromGPU = false;

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetReadFromGPU(bool value) {
	g_ReadFromGPU = value;
}

static void OnRenderEvent(int eventID)
{
	void* textureObject = (void*)g_TextureHandle; // Cast to void* or appropriate texture object type for your plugin

	int width = g_TextureWidth;
	int height = g_TextureHeight;
	int bufferSize = width * height * 4; // RGBA format

	if (g_ReadFromGPU) {
		if (frameLength < bufferSize)
			frameData = new unsigned char[bufferSize];

		//Read texture from GPU memory:
		s_CurrentAPI->ReadTextureData(textureObject, frameData, bufferSize);
	}
	//else, the texture is already supplied to use by the "UploadFrame" function, which is called by the C# side in Unity itself.

	std::async(std::launch::async, [&]() {
		//If we're in Direct3D mode, we need to flip the image vertically:
		if (s_DeviceType == kUnityGfxRendererD3D11 || s_DeviceType == kUnityGfxRendererD3D12) {
			//Flip the image vertically:
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width * 4; x++) {
					flippedData[(height - y - 1) * width * 4 + x] = frameData[y * width * 4 + x];
				}
			}

			//Copy the flipped image to our frameData:
			//memcpy(frameData, flippedData, bufferSize);

			g_Server->SendNewFrameToEveryone(flippedData, bufferSize, width, height);

			//delete[] flippedData;
			return;
		}

		//Broadcast our new image to everyone:
		g_Server->SendNewFrameToEveryone((unsigned char*)frameData, bufferSize, width, height);

		//Delete the texture:
		//delete[] frameData;
	});
}

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc()
{
	return OnRenderEvent;
}