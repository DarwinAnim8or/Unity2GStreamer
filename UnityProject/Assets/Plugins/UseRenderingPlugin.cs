using UnityEngine;
using System;
using System.Collections;
using System.Runtime.InteropServices;
using UnityEngine.Rendering;

public class UseRenderingPlugin : MonoBehaviour
{
	//TODO: define variables here, rather than in CreateTextureAndPassToPlugin()
	int frameCount = 0;
	public int fpsForCam = 15; //Indicates the framerate of the camera. Must be set BEFORE playing the scene.

	//Indicates whether or not the frame will be read from the GPU on the plugin side. Must be set BEFORE playing the scene.
	public bool readFromGPU = false;
	Texture2D tempTexture;

	//--------------------------------------------------------------------------------------------------------------------------------------------------------

	// Native plugin rendering events are only called if a plugin is used
	// by some script. This means we have to DllImport at least
	// one function in some active script.
	// For this example, we'll call into plugin's SetTimeFromUnity
	// function and pass the current time so the plugin can animate.

#if (UNITY_IOS || UNITY_TVOS || UNITY_WEBGL) && !UNITY_EDITOR
	[DllImport ("__Internal")]
#else
	[DllImport ("UnityLiveStreamingPlugin")]
#endif
	private static extern void SetTimeFromUnity(float t); //example from Unity, keeps the plugin loaded

	//import our dll's functions:
	[DllImport("UnityLiveStreamingPlugin")] 
	private static extern void SetTextureFromUnity(System.IntPtr texture, int w, int h); //tells the plugin where our rendertexture is

	[DllImport("UnityLiveStreamingPlugin")] 
	private static extern IntPtr GetRenderEventFunc(); //handles the received frame

	[DllImport("UnityLiveStreamingPlugin")]
	private static extern void UpdateRakNet(); //handles our networking

	[DllImport("UnityLiveStreamingPlugin")]
	private static extern void UploadFrame(byte[] data, int length, int channelID);

	[DllImport("UnityLiveStreamingPlugin")]
	private static extern void SetReadFromGPU(bool value);

	void Awake () {
		#if UNITY_EDITOR
			QualitySettings.vSyncCount = 0;  // VSync must be disabled
			Application.targetFrameRate = 60;
		#endif
	}

	IEnumerator Start() {
		CreateTextureAndPassToPlugin(); //create our output render texture

		//Tell the plugin whether or not we're going to be using the GPU:
		SetReadFromGPU(readFromGPU);

		yield return StartCoroutine("CallPluginAtEndOfFrames"); //setup a function to call when we're done rendering a frame
	}

	private void CreateTextureAndPassToPlugin() {
		Camera m_MainCamera = Camera.main;
		//m_MainCamera.pixelRect = new Rect(0, 0, 1280, 720);

		//create a new render texture based on the size of the main camera:
		RenderTexture m_RenderTexture = new RenderTexture(m_MainCamera.pixelWidth, m_MainCamera.pixelHeight, 24, RenderTextureFormat.ARGB32);
		/*m_RenderTexture.antiAliasing = 1;
		m_RenderTexture.wrapMode = TextureWrapMode.Clamp;
		m_RenderTexture.filterMode = FilterMode.Point;
		m_RenderTexture.anisoLevel = 0;
		m_RenderTexture.useMipMap = false;
		m_RenderTexture.autoGenerateMips = false;*/
		m_RenderTexture.Create();

		m_MainCamera.targetTexture = m_RenderTexture;
		m_MainCamera.Render(); //note that we're manually calling Render on the camera here! this renders 1 frame.

		//Create a texture2D for passing:
		tempTexture = new Texture2D(m_RenderTexture.width, m_RenderTexture.height, TextureFormat.ARGB32, false);

		//tell our plugin where the texture is, so it can load it when we're done rendering:
		SetTextureFromUnity(m_MainCamera.targetTexture.GetNativeTexturePtr(), m_MainCamera.pixelWidth, m_MainCamera.pixelHeight);
		Debug.Log("Setting texture from Unity");

		m_MainCamera.enabled = false; //disable the camera
	}

	void Update() {
		frameCount++;
		UpdateRakNet();
		
		//make our camera render at 15fps:
		if (frameCount % (60 / fpsForCam) == 0) {
			GetComponent<Camera>().Render(); //render the camera
		}
	}

	private ComputeShader asyncReadbackShader;
	private int asyncReadbackKernel;

	Texture2D toTexture2D(RenderTexture rTex)
	{
		// Create a new Texture2D object
		Texture2D tex = new Texture2D(rTex.width, rTex.height, TextureFormat.RGBA32, false);

		// Get the compute shader kernel
		if (asyncReadbackShader == null) asyncReadbackShader = Resources.Load<ComputeShader>("AsyncReadbackShader");
		if (asyncReadbackKernel == 0) asyncReadbackKernel = asyncReadbackShader.FindKernel("AsyncReadback");

		// Set the active RenderTexture
		RenderTexture.active = rTex;

		// Create a RenderTexture for the result
		RenderTexture rtResult = new RenderTexture(rTex.width, rTex.height, 0, RenderTextureFormat.ARGB32, RenderTextureReadWrite.Linear);
		rtResult.enableRandomWrite = true;
		rtResult.Create();

		// Set the compute shader parameters
		asyncReadbackShader.SetTexture(asyncReadbackKernel, "_MainTex", rTex);
		asyncReadbackShader.SetTexture(asyncReadbackKernel, "Result", rtResult);

		// Dispatch the compute shader
		asyncReadbackShader.Dispatch(asyncReadbackKernel, rTex.width / 32, rTex.height / 32, 1);

		// Read the data from the RenderTexture asynchronously
		AsyncGPUReadback.Request(rtResult, 0, TextureFormat.RGBA32, (AsyncGPUReadbackRequest request) => {
			if (request.hasError) {
				Debug.LogError("GPU readback error!");
			} else {
				// Copy the data to the Texture2D
				tex.LoadRawTextureData(request.GetData<byte>());
				tex.Apply();
			}
		});

		return tex;
	}

	Texture2D toTexture2DOld(RenderTexture rTex)
    {
        // ReadPixels looks at the active RenderTexture.
        RenderTexture.active = rTex;

		AsyncGPUReadback.Request(rTex, 0, TextureFormat.ARGB32, (AsyncGPUReadbackRequest request) => {
			if (request.hasError) {
				Debug.LogError("GPU readback error!");
			} else {
				tempTexture.SetPixelData(request.GetData<byte>(), 0);
				//outputTexture.Apply();
			}
		});

        //tempTexture.ReadPixels(new Rect(0, 0, rTex.width, rTex.height), 0, 0);
        return tempTexture;
    }

	private IEnumerator CallPluginAtEndOfFrames() {
		while(true) 
		{
			yield return new WaitForEndOfFrame(); //wait until the frame is done

			//make our camera render at 15fps:
			if (!(frameCount % (60 / fpsForCam) == 0)) {
				continue;
			}

			frameCount = 0;

			if (!readFromGPU) {
				//Get the render texture as a Texture2D so we can access the pixels
				var renderTexture = GetComponent<Camera>().targetTexture;

				RenderTexture.active = renderTexture;
				//byte[] pixelBytes = new byte[renderTexture.width * renderTexture.height * 4];

				AsyncGPUReadback.Request(renderTexture, 0, TextureFormat.ARGB32, (AsyncGPUReadbackRequest request) => {
					if (request.hasError) {
						Debug.LogError("GPU readback error!");
					} else {
						var bytes = request.GetData<byte>().ToArray();
						UploadFrame(bytes, bytes.Length, 1);
						//GL.IssuePluginEvent(GetRenderEventFunc(), 1);
					}
				});

				//var texture = toTexture2DOld(renderTexture);

				//Get the pixels from the texture:
				//byte[] pixelsBytes = texture.GetRawTextureData();

				//flip the image vertically:
				/*for (int i = 0; i < texture.height / 2; i++) {
					for (int j = 0; j < texture.width; j++) {
						int index1 = (i * texture.width + j) * 4;
						int index2 = ((texture.height - i - 1) * texture.width + j) * 4;
						byte temp = pixelsBytes[index1];
						pixelsBytes[index1] = pixelsBytes[index2];
						pixelsBytes[index2] = temp;
						temp = pixelsBytes[index1 + 1];
						pixelsBytes[index1 + 1] = pixelsBytes[index2 + 1];
						pixelsBytes[index2 + 1] = temp;
						temp = pixelsBytes[index1 + 2];
						pixelsBytes[index1 + 2] = pixelsBytes[index2 + 2];
						pixelsBytes[index2 + 2] = temp;
						temp = pixelsBytes[index1 + 3];
						pixelsBytes[index1 + 3] = pixelsBytes[index2 + 3];
						pixelsBytes[index2 + 3] = temp;
					}
				}*/
				
				//give frame to plugin:
				//UploadFrame(pixelBytes, pixelBytes.Length); //20FPS-ish
				continue;
			}

			//--Costs 100 FPS without GPU reading? 
			if (readFromGPU) {
				GL.IssuePluginEvent(GetRenderEventFunc(), 1); //tell our plugin it can begin processing our frame, the 1 is the event
																//it may be possible to just use the eventID as the channelID later on.
			}
		}
	}
}
