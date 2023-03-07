# Unity2GStreamer
The goal of this plugin is to create a RenderTexture for Unity to use, then render camera(s) to it. 
Once the frame is completed, we will take the generated texture and pass it as a frame to gstreamer, which will then in turn encode it and generate an RTSP stream from it.

## Cloning
If you clone this repository and git didn't clone the submodules, you need to run the following command:
```git submodule update --init --recursive```

## Building The Client (Encoding) side
The client is only confirmed to work on Linux. A Visual Studio project is included, but your milage may vary.
For Linux, the recommended setup, run the following scripts in order:
* linux_installdeps.sh
* build.sh

## Building the server (Unity Plugin)
The server is only confirmed to work on Windows. Use the supplied solution file in PluginSource\projects\VisualStudio2015 to compile the project.
The resulting .DLL file should be in the UnityProject for testing. The Unity project also serves as a test / minimal setup and contains the needed C# script to make the plugin work. 

## Software Versions
* Unity: 2021.3.15 LTS
* Visual Studio: 2022 Community
* Gstreamer: 1.22.0 

## Attribution
This is based off the [template](https://github.com/Unity-Technologies/NativeRenderingPlugin) provided by Unity Technologies themselves.

## License
This takes the same license as the Unity template, MIT/X11. 
