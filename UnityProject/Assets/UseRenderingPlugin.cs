using UnityEngine;
using System;
using System.Collections;
using System.Runtime.InteropServices;


public class UseRenderingPlugin : MonoBehaviour
{
	//TODO: define variables here, rather than in CreateTextureAndPassToPlugin()

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

	IEnumerator Start() {
		CreateTextureAndPassToPlugin(); //create our output render texture
		yield return StartCoroutine("CallPluginAtEndOfFrames"); //setup a function to call when we're done rendering a frame
	}

	private void CreateTextureAndPassToPlugin() {
		Camera m_MainCamera = Camera.main;
		m_MainCamera.pixelRect = new Rect(0, 0, 512, 512);

		//create a new render texture based on the size of the main camera:
		RenderTexture m_RenderTexture = new RenderTexture(m_MainCamera.pixelWidth, m_MainCamera.pixelHeight, 24, RenderTextureFormat.ARGB32);
		m_RenderTexture.Create();
		m_MainCamera.targetTexture = m_RenderTexture;
		m_MainCamera.Render(); //note that we're manually calling Render on the camera here! this renders 1 frame.

		//tell our plugin where the texture is, so it can load it when we're done rendering:
		SetTextureFromUnity(m_RenderTexture.GetNativeTexturePtr(), m_RenderTexture.width, m_RenderTexture.height);
		Debug.Log("Setting texture from Unity");
	}

	private IEnumerator CallPluginAtEndOfFrames() {
		while(true) 
		{
			yield return new WaitForEndOfFrame(); //wait until the frame is done
			GL.IssuePluginEvent(GetRenderEventFunc(), 1); //tell our plugin it can begin processing our frame, the 1 is the event
			//it may be possible to just use the eventID as the channelID later on.
		}
	}
}
