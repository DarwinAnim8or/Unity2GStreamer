using UnityEngine;
using System;
using System.Collections;
using System.Runtime.InteropServices;
using UnityEngine.Rendering;

public class UseRenderingPlugin : MonoBehaviour
{
	//TODO: define variables here, rather than in CreateTextureAndPassToPlugin()
	int frameCount = 0;

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
	[DllImport ("Unity2GStreamerPlugin")]
#endif
	private static extern void SetTimeFromUnity(float t); //example from Unity, keeps the plugin loaded

	//import our dll's functions:
	[DllImport("Unity2GStreamerPlugin")] 
	private static extern void SetTextureFromUnity(System.IntPtr texture, int w, int h); //tells the plugin where our rendertexture is

	[DllImport("Unity2GStreamerPlugin")] 
	private static extern IntPtr GetRenderEventFunc(); //handles the received frame

	[DllImport("Unity2GStreamerPlugin")]
	private static extern void UpdateRakNet(); //handles our networking

	[DllImport("Unity2GStreamerPlugin")]
	private static extern void UploadFrame(byte[] data, int length);

	[DllImport("Unity2GStreamerPlugin")]
	private static extern void SetReadFromGPU(bool value);

	IEnumerator Start() {
		CreateTextureAndPassToPlugin(); //create our output render texture

		//Tell the plugin whether or not we're going to be using the GPU:
		SetReadFromGPU(readFromGPU);

		yield return StartCoroutine("CallPluginAtEndOfFrames"); //setup a function to call when we're done rendering a frame
	}

	private void CreateTextureAndPassToPlugin() {
		Camera m_MainCamera = Camera.main;
		//m_MainCamera.pixelRect = new Rect(0, 0, m_MainCamera.pixelWidth, m_MainCamera.pixelHeight);

		//create a new render texture based on the size of the main camera:
		RenderTexture m_RenderTexture = new RenderTexture(m_MainCamera.pixelWidth, m_MainCamera.pixelHeight, 24, RenderTextureFormat.ARGB32);
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
		if (frameCount % 4 != 0) return;
		GetComponent<Camera>().Render(); //render the camera
	}

	Texture2D toTexture2D(RenderTexture rTex)
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
        //tempTexture.Apply(); //this would upload our image to the GPU again, but it isn't relevant for us.
        return tempTexture;
    }

	private IEnumerator CallPluginAtEndOfFrames() {
		while(true) 
		{
			yield return new WaitForEndOfFrame(); //wait until the frame is done
			UpdateRakNet();

			//frameCount++;
			if (frameCount % 4 != 0) continue;
			frameCount = 0;

			if (!readFromGPU) {
				//Get the render texture as a Texture2D so we can access the pixels
				var texture = toTexture2D(GetComponent<Camera>().targetTexture);

				//convert the byte array to a float array:
				byte[] data = texture.GetRawTextureData();

				//Save the image for debugging:
				//Debug.Log("data length: " + data.Length);
				//byte[] bytes = texture.EncodeToPNG();
				//System.IO.File.WriteAllBytes(Application.dataPath + "/../SavedScreen.png", bytes);

				//pass to plugin:
				UploadFrame(data, data.Length);
			}

			GL.IssuePluginEvent(GetRenderEventFunc(), 1); //tell our plugin it can begin processing our frame, the 1 is the event
			//it may be possible to just use the eventID as the channelID later on.
		}
	}
}
