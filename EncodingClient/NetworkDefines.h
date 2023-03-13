#pragma once
#include "RakNetTypes.h"
#include "MessageIdentifiers.h"
#include "RakString.h"

#define NET_VERSION 2

enum class Messages : unsigned int {
	ID_STREAM_SETTINGS = ID_USER_PACKET_ENUM + 1, //this tells us what port to initialize our RTSP stream on. 
	ID_HANDSHAKE,
	ID_HANDSHAKE_RESPONSE,
	ID_IMAGE_DATA, //new frame data for us to encode,
	ID_READY_FOR_DATA, //empty packet outside of the ID, just to show that we're ready for incoming image data.
};

struct StreamSettings {
	int port = 2002; //port the RTSP stream will run on
	RakNet::RakString codec; //the video codec we're supposed to use
	bool useHardwareEncoder; //try to use the hardware (NVENC) encoder if available; if not we'll fall back to the software implementation. 
};