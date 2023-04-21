# UnityLiveStreamingPlugin
The goal of this plugin is to create a RenderTexture for Unity to use, then render camera(s) to it. 
Once the frame is completed, we will take the generated texture and pass it as a frame to an encoding client, which will then in turn encode it and generate an RTSP stream from it.

The plugin and encoding client are seperated by a network connection, provided by [RakNet](https://github.com/LBBStudios/RakNet).
This is to *allow* for the computer rendering your game to be unburdened by encoding your video stream. There is of course, nothing stopping you from using it on localhost.

## What is each project? What do I need? 
So, when you open the solution file for this project, you will see a number of projects loaded in. 
They are the following:

***3rdparty:***
* RaknetLibStatic
  * RakNet is the library that is used in this project for communication between the server (plugin) and client. For more information, please see the [RakNet repo](https://github.com/LBBStudios/RakNet)

***Debugging:***
* DummyServer
  * This is a simple application that uses the same CCTVServer class as the UnityLiveStreamingPlugin. This is because this program is meant to easily debug the network connection between the EncodingClient and the plugin.

***Main:***
* EncodingClient
  * This is the client that connects with the Unity plugin when it starts. This is the thing that actually does the heavy lifting of encoding when a client connects to it over RTSP and has selected a channel.
* UnityLiveStreamingPlugin
  * This is the main plugin. You'll want to include this into your Unity plugins folder. This project will also build staright to the plugins folder of the included sample Unity Project.

***Testing:***
* OnvifTest 
  * This program is close to an analog of DummyServer, where it's intended purpose is for debugging and testing the connection between Onvif and a VMS.

## Cloning
If you clone this repository and git didn't clone the submodules, you need to run the following command:

```git submodule update --init --recursive```

## Requirements
This project has a few dependencies which need to be installed manually. In order to build this project, you will need:
* [Unity](https://store.unity.com/#plans-individual)
* [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/)
* [CMake](https://cmake.org/download/)
* [libjpeg-turbo](https://sourceforge.net/projects/libjpeg-turbo/files/2.1.5.1/)
* [boost](https://www.boost.org/)

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

Go to "C/C++" -> "Code Generation" -> "Runtime Library". Set it to "Multi-threaded (/MT)" for all options except Debug. Debug should be "Multi-threaded debug (/MTd)" 

RakNet by default, at the time of writing, has security measures disabled by default. They can be enabled by going into the "NativeFeatureIncludesOverrides.h" file and defining LIBCAT_SECURITY 1; that said, for our usecase it is not needed to implement TLS-like security since it is meant to only go across LAN.

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
