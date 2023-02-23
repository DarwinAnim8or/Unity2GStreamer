# Unity2GStreamer
The goal of this plugin is to create a RenderTexture for Unity to use, then render camera(s) to it. 
Once the frame is completed, we will take the generated texture and pass it as a frame to gstreamer, which will then in turn encode it and generate an RTSP stream from it.

## Possible Networked Expansion
The plan is to expand this plugin with the capability to send it's generated images to a small server that then passes the texture to gstreamer instead. 
The reason for this is that it'd be great to seperate out your Unity instance, and your encoding hardware, so that the Unity instance can use all it's power to render out frames and not encode video signals. 

## Software Versions
* Unity: 2021.3.15 LTS
* Visual Studio: 2022 Community
* Gstreamer: 1.22.0 

## Attribution
This is based off the [template](https://github.com/Unity-Technologies/NativeRenderingPlugin) provided by Unity Technologies themselves.

## License
This takes the same license as the Unity template, MIT/X11. 
