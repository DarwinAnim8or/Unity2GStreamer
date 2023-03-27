# UnityLiveStreamingPlugin
The goal of this plugin is to create a RenderTexture for Unity to use, then render camera(s) to it. 
Once the frame is completed, we will take the generated texture and pass it as a frame to an encoding client, which will then in turn encode it and generate an RTSP stream from it.

The plugin and encoding client are seperated by a network connection, provided by [RakNet](https://github.com/LBBStudios/RakNet).
This is to *allow* for the computer rendering your game to be unburdened by encoding your video stream. There is of course, nothing stopping you from using it on localhost.

## Cloning
If you clone this repository and git didn't clone the submodules, you need to run the following command:

```git submodule update --init --recursive```

## Requirements
This project has a few dependencies which need to be installed manually. In order to build this project, you will need:
* [Unity](https://store.unity.com/#plans-individual)
* [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/)
* [CMake](https://cmake.org/download/)
* [libjpeg-turbo](https://sourceforge.net/projects/libjpeg-turbo/files/2.1.5.1/)

Note that this project is currently only known to work on Windows, due to it's use of WinSock.
While it would be possible to translate this into code for Linux, it is currently not a goal of this project to do so.

### Software Versions
* Unity: 2021.3.15 LTS
* Visual Studio: 2022 Community
* libjpeg-turbo: 2.1.5.1

## Prerequiste steps
### Step 1: Configure RakNet
* Open cmake-gui
* Set "Where to build the source" to the 3rdparty/RakNet folder
* Set the "Where to build the binaries" to the 3rdparty/RakNet/build folder (created if needed)
* Press Configure. Set the compiler settings to match yours, should be defaults.
* Press Generate
* Press Open Project

RakNet will need some settings modified in order to compile correctly inside our Plugin's DLL. To do this, right-click your "RakNetLibStatic" project and go to "Properties". 

Go to "C/C++" -> "Code Generation" -> "Runtime Library". Set it to "Multi-threaded (/MT)" for all options except Debug. Debug sohuld be "Multi-threaded debug (/MTd)" 

For the final step, simply open "NativeFeatureIncludesOverrides.h", and add "#define LIBCAT_SECURITY 0" in the middle of the file, before the #endif statement.
Per info: libcat is the library RakNet uses for providing TLS-like encryption to the protocol. For this usecase, it is not relevant. 

### Step 2: Build RakNet
* Right-click on the Solution in the Solution Explorer.
* "Batch Build..."
* Click "Select All"
* Click "Build"

## Building The Client (Encoding) side
Make sure you've completed the Prereq steps.
Use the supplied solution file in the EncodingClient\ folder to compile the project.

## Building the server (Unity Plugin)
Make sure you've completed the Prereq steps.
Use the supplied solution file in PluginSource\projects\VisualStudio2015 to compile the project.
The resulting .DLL file should be in the UnityProject for testing. The Unity project also serves as a test / minimal setup and contains the needed C# script to make the plugin work. 

## Attribution
This is based off the [template](https://github.com/Unity-Technologies/NativeRenderingPlugin) provided by Unity Technologies themselves.

## License
This takes the same license as the Unity template, MIT/X11. 
